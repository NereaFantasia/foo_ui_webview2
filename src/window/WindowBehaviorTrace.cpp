#include "pch.h"
#include "window/WindowBehaviorTrace.h"

#include <CommCtrl.h>
#include <cstdio>
#include <unordered_map>
#include <unordered_set>

// ============================================
// 实现要点
//
// - 系统调用：在 before_ui_init 阶段改写本 DLL 自己的导入表，让它对 user32、dwmapi
//   的写操作先经过这里的包装。只改本模块，foobar2000 与 WebView2 运行时的调用不受影响。
//   uxtheme 的三个暗色模式函数经 GetProcAddress 按序号取，所以也包装 GetProcAddress。
// - 消息：UI 线程上挂一个线程级 WH_CBT 钩子，本模块注册的窗口类一创建就挂子类，
//   记录每条消息与返回值。嵌套关系（处理消息时调的 API、API 同步引起的消息）用缩进深度表示。
// - 高频且与布局无关的消息（鼠标移动、命中测试、绘制、定时器等）先不写帧头，
//   处理中产生了副作用才补写；没有副作用的，同一窗口、消息与返回值的组合只记第一次。
// - 输出按行，制表符分隔：序号、线程（ui / bg）、深度、种类、内容。
//   句柄换成符号名（类名#序号，外部窗口记 ext:类名），同一组操作两次运行的轨迹可以直接比。
// - 记录代码持锁拼行，但从不在持锁时调用被包装的真实函数：真实函数可能同步发消息，
//   嵌套的记录会再次取锁。
// - 包装函数在记录前后保存并恢复 GetLastError，调用方常在调用后读它。
// ============================================

namespace window_behavior_trace {

namespace detail {
std::atomic<bool> g_active{false};
}  // namespace detail

namespace {

constexpr UINT_PTR kSubclassId = 0x57425452;  // 'WBTR'
constexpr size_t kFlushBytes = 16 * 1024;     // UI 线程回到顶层时，缓冲超过它就写盘
constexpr size_t kHardFlushBytes = 1024 * 1024;
constexpr size_t kMaxParamsChars = 240;

enum class FrameKind { Message, Invoke, Call };

struct Frame {
    FrameKind kind = FrameKind::Call;
    bool quiet = false;
    bool emitted = false;
    bool sawDef = false;
    bool sawDwm = false;
    // 帧头写完时缓冲的长度与写盘批次；两者都没变，说明帧内没有写过别的行，
    // 结果可以直接接在帧头那一行末尾。
    size_t lineEnd = 0;
    unsigned long long epoch = 0;
    std::string header;
    HWND hwnd = nullptr;
    UINT msg = 0;
    WPARAM wp = 0;
    LPARAM lp = 0;
    // invoke 帧的方法名与参数，指向分发函数的实参，帧在其作用域内出栈。
    // 帧头只在要写出时才拼，没有副作用的调用不做 JSON 序列化。
    const std::string* method = nullptr;
    const nlohmann::json* params = nullptr;
};

struct State {
    std::mutex mu;
    std::string buf;
    unsigned long long epoch = 0;
    unsigned long long seq = 0;
    HANDLE file = INVALID_HANDLE_VALUE;
    HMODULE self = nullptr;
    DWORD uiThread = 0;
    HHOOK cbt = nullptr;
    std::unordered_map<HWND, std::string> names;
    std::unordered_map<std::string, int> classCounts;
    // 安静帧去重分两层：先按原始值（句柄、消息、返回值）查，重复的不拼字符串；
    // 原始值是句柄一类每次不同的，再按写出的文字去重。
    std::unordered_set<unsigned long long> quietRaw;
    std::unordered_set<std::string> quietSeen;
};

// 安装后不释放：DLL 卸载期间仍可能有消息经过包装函数，届时 g_active 已清零，
// 只读指针判断，不碰已析构的对象。
State* g_state = nullptr;

std::vector<Frame>& Frames() {
    thread_local std::vector<Frame> frames;
    return frames;
}

bool OnUiThread(const State& s) {
    return GetCurrentThreadId() == s.uiThread;
}

// ---------- 格式化 ----------

std::string Hex(unsigned long long v) {
    char b[24];
    snprintf(b, sizeof(b), "0x%llX", v);
    return b;
}

std::string Dec(long long v) {
    return std::to_string(v);
}

std::string Utf8(const wchar_t* w, int len = -1) {
    if (!w) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, len, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, len, out.data(), n, nullptr, nullptr);
    if (len < 0 && !out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

// 轨迹按行、按制表符切分，外来文字里的这几个字符换成空格。
std::string Clean(std::string text, size_t maxChars = 80) {
    for (char& c : text) {
        if (c == '\t' || c == '\r' || c == '\n') c = ' ';
    }
    if (text.size() > maxChars) {
        text.resize(maxChars);
        text += "...";
    }
    return text;
}

std::string RectText(const RECT& r) {
    return "[" + Dec(r.left) + "," + Dec(r.top) + " " + Dec(r.right - r.left) + "x" + Dec(r.bottom - r.top) + "]";
}

struct FlagName {
    unsigned long long bit;
    const char* name;
};

template <size_t N>
std::string FlagsText(unsigned long long value, const FlagName (&names)[N]) {
    std::string out;
    for (const FlagName& f : names) {
        if (value & f.bit) {
            if (!out.empty()) out += '|';
            out += f.name;
            value &= ~f.bit;
        }
    }
    if (value) {
        if (!out.empty()) out += '|';
        out += Hex(value);
    }
    return out.empty() ? "0" : out;
}

constexpr FlagName kSwpFlags[] = {
    {SWP_NOSIZE, "NOSIZE"}, {SWP_NOMOVE, "NOMOVE"}, {SWP_NOZORDER, "NOZORDER"},
    {SWP_NOREDRAW, "NOREDRAW"}, {SWP_NOACTIVATE, "NOACTIVATE"}, {SWP_FRAMECHANGED, "FRAMECHANGED"},
    {SWP_SHOWWINDOW, "SHOWWINDOW"}, {SWP_HIDEWINDOW, "HIDEWINDOW"}, {SWP_NOCOPYBITS, "NOCOPYBITS"},
    {SWP_NOOWNERZORDER, "NOOWNERZORDER"}, {SWP_NOSENDCHANGING, "NOSENDCHANGING"},
    {SWP_DEFERERASE, "DEFERERASE"}, {SWP_ASYNCWINDOWPOS, "ASYNCWINDOWPOS"},
};

constexpr FlagName kRdwFlags[] = {
    {RDW_INVALIDATE, "INVALIDATE"}, {RDW_INTERNALPAINT, "INTERNALPAINT"}, {RDW_ERASE, "ERASE"},
    {RDW_VALIDATE, "VALIDATE"}, {RDW_NOINTERNALPAINT, "NOINTERNALPAINT"}, {RDW_NOERASE, "NOERASE"},
    {RDW_NOCHILDREN, "NOCHILDREN"}, {RDW_ALLCHILDREN, "ALLCHILDREN"}, {RDW_UPDATENOW, "UPDATENOW"},
    {RDW_ERASENOW, "ERASENOW"}, {RDW_FRAME, "FRAME"}, {RDW_NOFRAME, "NOFRAME"},
};

const char* ShowCmdName(int cmd) {
    switch (cmd) {
    case SW_HIDE: return "SW_HIDE";
    case SW_SHOWNORMAL: return "SW_SHOWNORMAL";
    case SW_SHOWMINIMIZED: return "SW_SHOWMINIMIZED";
    case SW_SHOWMAXIMIZED: return "SW_SHOWMAXIMIZED";
    case SW_SHOWNOACTIVATE: return "SW_SHOWNOACTIVATE";
    case SW_SHOW: return "SW_SHOW";
    case SW_MINIMIZE: return "SW_MINIMIZE";
    case SW_SHOWMINNOACTIVE: return "SW_SHOWMINNOACTIVE";
    case SW_SHOWNA: return "SW_SHOWNA";
    case SW_RESTORE: return "SW_RESTORE";
    case SW_SHOWDEFAULT: return "SW_SHOWDEFAULT";
    case SW_FORCEMINIMIZE: return "SW_FORCEMINIMIZE";
    default: return "SW_?";
    }
}

std::string SysCommandName(WPARAM wp) {
    switch (wp & 0xFFF0) {
    case SC_SIZE: return "SC_SIZE";
    case SC_MOVE: return "SC_MOVE";
    case SC_MINIMIZE: return "SC_MINIMIZE";
    case SC_MAXIMIZE: return "SC_MAXIMIZE";
    case SC_CLOSE: return "SC_CLOSE";
    case SC_MOUSEMENU: return "SC_MOUSEMENU";
    case SC_KEYMENU: return "SC_KEYMENU";
    case SC_RESTORE: return "SC_RESTORE";
    case SC_TASKLIST: return "SC_TASKLIST";
    case SC_DEFAULT: return "SC_DEFAULT";
    default: return Hex(wp & 0xFFF0);
    }
}

std::string HitTestName(LRESULT ht) {
    switch (ht) {
    case HTERROR: return "HTERROR";
    case HTTRANSPARENT: return "HTTRANSPARENT";
    case HTNOWHERE: return "HTNOWHERE";
    case HTCLIENT: return "HTCLIENT";
    case HTCAPTION: return "HTCAPTION";
    case HTSYSMENU: return "HTSYSMENU";
    case HTMINBUTTON: return "HTMINBUTTON";
    case HTMAXBUTTON: return "HTMAXBUTTON";
    case HTLEFT: return "HTLEFT";
    case HTRIGHT: return "HTRIGHT";
    case HTTOP: return "HTTOP";
    case HTTOPLEFT: return "HTTOPLEFT";
    case HTTOPRIGHT: return "HTTOPRIGHT";
    case HTBOTTOM: return "HTBOTTOM";
    case HTBOTTOMLEFT: return "HTBOTTOMLEFT";
    case HTBOTTOMRIGHT: return "HTBOTTOMRIGHT";
    case HTBORDER: return "HTBORDER";
    case HTCLOSE: return "HTCLOSE";
    default: return Dec(static_cast<long long>(ht));
    }
}

std::string DwmAttributeName(DWORD attr) {
    switch (attr) {
    case 2: return "NCRENDERING_POLICY";
    case 3: return "TRANSITIONS_FORCEDISABLED";
    case 4: return "ALLOW_NCPAINT";
    case 7: return "FORCE_ICONIC_REPRESENTATION";
    case 10: return "HAS_ICONIC_BITMAP";
    case 11: return "DISALLOW_PEEK";
    case 12: return "EXCLUDED_FROM_PEEK";
    case 13: return "CLOAK";
    case 17: return "USE_HOSTBACKDROPBRUSH";
    case 19: return "USE_IMMERSIVE_DARK_MODE_OLD";
    case 20: return "USE_IMMERSIVE_DARK_MODE";
    case 33: return "WINDOW_CORNER_PREFERENCE";
    case 34: return "BORDER_COLOR";
    case 35: return "CAPTION_COLOR";
    case 36: return "TEXT_COLOR";
    case 38: return "SYSTEMBACKDROP_TYPE";
    case 1029: return "MICA_EFFECT";
    default: return Dec(attr);
    }
}

std::string MessageName(UINT msg) {
#define WBT_MSG(x) \
    case x:        \
        return #x;
    switch (msg) {
        WBT_MSG(WM_CREATE) WBT_MSG(WM_DESTROY) WBT_MSG(WM_MOVE) WBT_MSG(WM_SIZE) WBT_MSG(WM_ACTIVATE)
        WBT_MSG(WM_SETFOCUS) WBT_MSG(WM_KILLFOCUS) WBT_MSG(WM_ENABLE) WBT_MSG(WM_SETREDRAW)
        WBT_MSG(WM_SETTEXT) WBT_MSG(WM_GETTEXT) WBT_MSG(WM_GETTEXTLENGTH) WBT_MSG(WM_PAINT)
        WBT_MSG(WM_CLOSE) WBT_MSG(WM_QUERYENDSESSION) WBT_MSG(WM_QUIT) WBT_MSG(WM_ERASEBKGND)
        WBT_MSG(WM_SYSCOLORCHANGE) WBT_MSG(WM_ENDSESSION) WBT_MSG(WM_SHOWWINDOW) WBT_MSG(WM_SETTINGCHANGE)
        WBT_MSG(WM_ACTIVATEAPP) WBT_MSG(WM_SETCURSOR) WBT_MSG(WM_MOUSEACTIVATE) WBT_MSG(WM_GETMINMAXINFO)
        WBT_MSG(WM_WINDOWPOSCHANGING) WBT_MSG(WM_WINDOWPOSCHANGED) WBT_MSG(WM_NOTIFY)
        WBT_MSG(WM_CONTEXTMENU) WBT_MSG(WM_STYLECHANGING) WBT_MSG(WM_STYLECHANGED)
        WBT_MSG(WM_DISPLAYCHANGE) WBT_MSG(WM_GETICON) WBT_MSG(WM_SETICON) WBT_MSG(WM_NCCREATE)
        WBT_MSG(WM_NCDESTROY) WBT_MSG(WM_NCCALCSIZE) WBT_MSG(WM_NCHITTEST) WBT_MSG(WM_NCPAINT)
        WBT_MSG(WM_NCACTIVATE) WBT_MSG(WM_GETDLGCODE) WBT_MSG(WM_NCMOUSEMOVE) WBT_MSG(WM_NCLBUTTONDOWN)
        WBT_MSG(WM_NCLBUTTONUP) WBT_MSG(WM_NCLBUTTONDBLCLK) WBT_MSG(WM_NCRBUTTONDOWN)
        WBT_MSG(WM_NCRBUTTONUP) WBT_MSG(WM_NCMBUTTONDOWN) WBT_MSG(WM_NCMBUTTONUP)
        WBT_MSG(WM_KEYDOWN) WBT_MSG(WM_KEYUP) WBT_MSG(WM_CHAR) WBT_MSG(WM_SYSKEYDOWN) WBT_MSG(WM_SYSKEYUP)
        WBT_MSG(WM_SYSCHAR) WBT_MSG(WM_INITDIALOG) WBT_MSG(WM_COMMAND) WBT_MSG(WM_SYSCOMMAND)
        WBT_MSG(WM_TIMER) WBT_MSG(WM_INITMENU) WBT_MSG(WM_INITMENUPOPUP) WBT_MSG(WM_MENUSELECT)
        WBT_MSG(WM_ENTERIDLE) WBT_MSG(WM_UNINITMENUPOPUP) WBT_MSG(WM_MOUSEMOVE) WBT_MSG(WM_LBUTTONDOWN)
        WBT_MSG(WM_LBUTTONUP) WBT_MSG(WM_LBUTTONDBLCLK) WBT_MSG(WM_RBUTTONDOWN) WBT_MSG(WM_RBUTTONUP)
        WBT_MSG(WM_RBUTTONDBLCLK) WBT_MSG(WM_MBUTTONDOWN) WBT_MSG(WM_MBUTTONUP) WBT_MSG(WM_MBUTTONDBLCLK)
        WBT_MSG(WM_MOUSEWHEEL) WBT_MSG(WM_XBUTTONDOWN) WBT_MSG(WM_XBUTTONUP) WBT_MSG(WM_XBUTTONDBLCLK)
        WBT_MSG(WM_MOUSEHWHEEL) WBT_MSG(WM_PARENTNOTIFY) WBT_MSG(WM_ENTERMENULOOP) WBT_MSG(WM_EXITMENULOOP)
        WBT_MSG(WM_SIZING) WBT_MSG(WM_CAPTURECHANGED) WBT_MSG(WM_MOVING) WBT_MSG(WM_POWERBROADCAST)
        WBT_MSG(WM_DEVICECHANGE) WBT_MSG(WM_ENTERSIZEMOVE) WBT_MSG(WM_EXITSIZEMOVE) WBT_MSG(WM_DROPFILES)
        WBT_MSG(WM_MOUSEHOVER) WBT_MSG(WM_MOUSELEAVE) WBT_MSG(WM_NCMOUSEHOVER) WBT_MSG(WM_NCMOUSELEAVE)
        WBT_MSG(WM_WTSSESSION_CHANGE) WBT_MSG(WM_DPICHANGED)
        WBT_MSG(WM_DWMCOMPOSITIONCHANGED) WBT_MSG(WM_DWMNCRENDERINGCHANGED)
        WBT_MSG(WM_DWMCOLORIZATIONCOLORCHANGED) WBT_MSG(WM_DWMWINDOWMAXIMIZEDCHANGE)
        WBT_MSG(WM_DWMSENDICONICTHUMBNAIL) WBT_MSG(WM_DWMSENDICONICLIVEPREVIEWBITMAP)
        WBT_MSG(WM_THEMECHANGED) WBT_MSG(WM_GETOBJECT) WBT_MSG(WM_COPYDATA) WBT_MSG(WM_INPUT)
        WBT_MSG(WM_CTLCOLORBTN) WBT_MSG(WM_CTLCOLORSTATIC) WBT_MSG(WM_CTLCOLOREDIT) WBT_MSG(WM_CTLCOLORDLG)
    default:
        break;
    }
#undef WBT_MSG
    if (msg >= 0xC000 && msg <= 0xFFFF) {
        wchar_t name[128] = {};
        const int n = GetClipboardFormatNameW(msg, name, static_cast<int>(std::size(name)));
        return n > 0 ? "reg:" + Clean(Utf8(name, n), 60) : "reg:" + Hex(msg);
    }
    if (msg >= WM_APP && msg < 0xC000) return "WM_APP+" + Dec(msg - WM_APP);
    if (msg >= WM_USER && msg < WM_APP) return "WM_USER+" + Dec(msg - WM_USER);
    return Hex(msg);
}

// 帧头不包含 WM_TIMER 的调用频度一类随时序变化的信息；这里只放各消息里稳定的参数。
// 这些消息无论有没有副作用都记成可延迟写头的「安静帧」。
bool IsQuietMessage(UINT msg) {
    switch (msg) {
    case WM_MOUSEMOVE: case WM_NCMOUSEMOVE: case WM_SETCURSOR: case WM_NCHITTEST:
    case WM_MOUSEHOVER: case WM_MOUSELEAVE: case WM_NCMOUSEHOVER: case WM_NCMOUSELEAVE:
    case WM_GETICON: case WM_GETTEXT: case WM_GETTEXTLENGTH: case WM_GETOBJECT:
    case WM_PAINT: case WM_NCPAINT: case WM_ERASEBKGND: case WM_ENTERIDLE: case WM_TIMER:
    case WM_GETDLGCODE: case WM_INPUT: case WM_CTLCOLORBTN: case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: case WM_CTLCOLORDLG: case WM_NOTIFY: case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
    case WM_DWMCOLORIZATIONCOLORCHANGED:
        return true;
    default:
        return false;
    }
}

bool IsAppMessage(UINT msg) {
    return msg >= WM_USER;
}

// ---------- 句柄命名 ----------

std::string ClassNameOf(HWND h) {
    wchar_t cls[128] = {};
    const int n = GetClassNameW(h, cls, static_cast<int>(std::size(cls)));
    return n > 0 ? Clean(Utf8(cls, n), 60) : "?";
}

std::string HwndName(State& s, HWND h) {
    if (!h) return "null";
    if (h == HWND_MESSAGE) return "HWND_MESSAGE";
    const auto it = s.names.find(h);
    if (it != s.names.end()) return it->second;
    if (!IsWindow(h)) return "dead";
    const auto classModule = reinterpret_cast<HMODULE>(GetClassLongPtrW(h, GCLP_HMODULE));
    return (classModule == s.self ? "own:" : "ext:") + ClassNameOf(h);
}

std::string InsertAfterName(State& s, HWND h) {
    if (h == HWND_TOP) return "TOP";
    if (h == HWND_BOTTOM) return "BOTTOM";
    if (h == HWND_TOPMOST) return "TOPMOST";
    if (h == HWND_NOTOPMOST) return "NOTOPMOST";
    return HwndName(s, h);
}

std::string WindowPosText(State& s, const WINDOWPOS* wp) {
    if (!wp) return "null";
    std::string out = FlagsText(wp->flags, kSwpFlags);
    if (!(wp->flags & SWP_NOZORDER)) out += " after=" + InsertAfterName(s, wp->hwndInsertAfter);
    if (!(wp->flags & SWP_NOMOVE)) out += " pos=" + Dec(wp->x) + "," + Dec(wp->y);
    if (!(wp->flags & SWP_NOSIZE)) out += " size=" + Dec(wp->cx) + "x" + Dec(wp->cy);
    return out;
}

// 消息帧头里的参数摘要。只取值稳定的字段：指针、其他进程的窗口、时间戳一律不记。
std::string MessageDetail(State& s, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE: {
        static const char* const kinds[] = {"RESTORED", "MINIMIZED", "MAXIMIZED", "MAXSHOW", "MAXHIDE"};
        const std::string kind = wp < std::size(kinds) ? kinds[wp] : Dec(static_cast<long long>(wp));
        return kind + " " + Dec(LOWORD(lp)) + "x" + Dec(HIWORD(lp));
    }
    case WM_MOVE:
        return Dec(static_cast<short>(LOWORD(lp))) + "," + Dec(static_cast<short>(HIWORD(lp)));
    case WM_ACTIVATE: {
        static const char* const states[] = {"WA_INACTIVE", "WA_ACTIVE", "WA_CLICKACTIVE"};
        const WORD st = LOWORD(wp);
        std::string out = st < std::size(states) ? states[st] : Dec(st);
        if (HIWORD(wp)) out += " minimized";
        return out;
    }
    case WM_ACTIVATEAPP:
    case WM_NCACTIVATE:
    case WM_ENABLE:
    case WM_SETREDRAW:
    case WM_DWMNCRENDERINGCHANGED:
    case WM_DWMWINDOWMAXIMIZEDCHANGE:
        return wp ? "1" : "0";
    case WM_SHOWWINDOW:
        return std::string(wp ? "1" : "0") + " status=" + Dec(lp);
    case WM_SYSCOMMAND:
        return SysCommandName(wp);
    case WM_TIMER:
        return "id=" + Dec(static_cast<long long>(wp));
    case WM_NCCALCSIZE:
        if (wp && lp) return "1 " + RectText(reinterpret_cast<const NCCALCSIZE_PARAMS*>(lp)->rgrc[0]);
        return lp ? "0 " + RectText(*reinterpret_cast<const RECT*>(lp)) : "0";
    case WM_WINDOWPOSCHANGING:
    case WM_WINDOWPOSCHANGED:
        return WindowPosText(s, reinterpret_cast<const WINDOWPOS*>(lp));
    case WM_STYLECHANGING:
    case WM_STYLECHANGED: {
        const auto* ss = reinterpret_cast<const STYLESTRUCT*>(lp);
        const std::string which = static_cast<int>(wp) == GWL_EXSTYLE ? "EXSTYLE" : "STYLE";
        return ss ? which + " " + Hex(ss->styleOld) + "->" + Hex(ss->styleNew) : which;
    }
    case WM_DPICHANGED:
        return Dec(HIWORD(wp)) + (lp ? " " + RectText(*reinterpret_cast<const RECT*>(lp)) : "");
    case WM_SETTINGCHANGE:
        return lp ? Clean(Utf8(reinterpret_cast<const wchar_t*>(lp)), 60) : Dec(static_cast<long long>(wp));
    case WM_SIZING:
        return "edge=" + Dec(static_cast<long long>(wp));
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
        return "vk=" + Hex(wp);
    case WM_COMMAND:
        return "id=" + Dec(LOWORD(wp)) + " code=" + Dec(HIWORD(wp));
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
    case WM_NCLBUTTONDBLCLK:
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
    case WM_NCMBUTTONDOWN:
    case WM_NCMBUTTONUP:
        return HitTestName(static_cast<LRESULT>(wp));
    case WM_POWERBROADCAST:
    case WM_WTSSESSION_CHANGE:
        return Dec(static_cast<long long>(wp));
    default:
        break;
    }
    if (IsAppMessage(msg)) return "wp=" + Hex(wp) + " lp=" + Hex(static_cast<unsigned long long>(lp));
    return {};
}

// 消息处理完后，处理函数改写过的输出参数。
std::string MessageOutput(UINT msg, WPARAM wp, LPARAM lp, State& s) {
    switch (msg) {
    case WM_NCCALCSIZE:
        if (wp && lp) return " out=" + RectText(reinterpret_cast<const NCCALCSIZE_PARAMS*>(lp)->rgrc[0]);
        return lp ? " out=" + RectText(*reinterpret_cast<const RECT*>(lp)) : "";
    case WM_GETMINMAXINFO: {
        const auto* mmi = reinterpret_cast<const MINMAXINFO*>(lp);
        if (!mmi) return {};
        return " minTrack=" + Dec(mmi->ptMinTrackSize.x) + "x" + Dec(mmi->ptMinTrackSize.y) +
               " maxTrack=" + Dec(mmi->ptMaxTrackSize.x) + "x" + Dec(mmi->ptMaxTrackSize.y) +
               " maxSize=" + Dec(mmi->ptMaxSize.x) + "x" + Dec(mmi->ptMaxSize.y) +
               " maxPos=" + Dec(mmi->ptMaxPosition.x) + "," + Dec(mmi->ptMaxPosition.y);
    }
    case WM_WINDOWPOSCHANGING:
        return " out=" + WindowPosText(s, reinterpret_cast<const WINDOWPOS*>(lp));
    case WM_SIZING:
    case WM_MOVING:
        return lp ? " out=" + RectText(*reinterpret_cast<const RECT*>(lp)) : "";
    default:
        return {};
    }
}

std::string MessageResult(UINT msg, LRESULT r) {
    switch (msg) {
    case WM_NCHITTEST:
        return HitTestName(r);
    // 这几条的返回值是句柄，每次运行都不同。
    case WM_GETICON:
    case WM_GETOBJECT:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORDLG:
        return r ? "handle" : "0";
    default:
        return Dec(static_cast<long long>(r));
    }
}

std::string MessageText(State& s, UINT msg, WPARAM wp, LPARAM lp) {
    std::string detail = MessageDetail(s, msg, wp, lp);
    return detail.empty() ? MessageName(msg) : MessageName(msg) + " " + detail;
}

// ---------- 缓冲与写盘 ----------

void AppendLine(State& s, bool ui, size_t depth, const char* kind, const std::string& payload) {
    char head[64];
    snprintf(head, sizeof(head), "%llu\t%s\t%zu\t", ++s.seq, ui ? "ui" : "bg", depth);
    s.buf += head;
    s.buf += kind;
    s.buf += '\t';
    s.buf += payload;
    s.buf += '\n';
}

void FlushLocked(State& s) {
    if (s.buf.empty() || s.file == INVALID_HANDLE_VALUE) return;
    const char* p = s.buf.data();
    size_t left = s.buf.size();
    while (left > 0) {
        DWORD written = 0;
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(left, 1u << 20));
        if (!WriteFile(s.file, p, chunk, &written, nullptr) || written == 0) break;
        p += written;
        left -= written;
    }
    s.buf.clear();
    ++s.epoch;
}

void MaybeFlushLocked(State& s, bool ui) {
    if (s.buf.size() >= kHardFlushBytes || (ui && Frames().empty() && s.buf.size() >= kFlushBytes)) {
        FlushLocked(s);
    }
}

std::string InvokeHeader(const std::string& method, const nlohmann::json& params) {
    std::string paramsText;
    if (params.is_object()) {
        nlohmann::json visible = nlohmann::json::object();
        for (auto it = params.begin(); it != params.end(); ++it) {
            if (!it.key().empty() && it.key()[0] != '_') visible[it.key()] = it.value();
        }
        if (!visible.empty()) paramsText = visible.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    } else if (!params.is_null()) {
        paramsText = params.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    }
    return Clean(method, 80) + (paramsText.empty() ? "" : " " + Clean(paramsText, kMaxParamsChars));
}

std::string FrameHeader(State& s, const Frame& f) {
    if (f.kind == FrameKind::Message) return HwndName(s, f.hwnd) + " " + MessageText(s, f.msg, f.wp, f.lp);
    if (f.kind == FrameKind::Invoke && f.method && f.params) return InvokeHeader(*f.method, *f.params);
    return f.header;
}

const char* FrameKindText(FrameKind kind) {
    switch (kind) {
    case FrameKind::Message: return "msg";
    case FrameKind::Invoke: return "invoke";
    default: return "call";
    }
}

// 有副作用要写时，先把还没写头的外层帧依次补上，嵌套关系才完整。
void EmitPending(State& s, std::vector<Frame>& frames) {
    for (size_t i = 0; i < frames.size(); ++i) {
        Frame& f = frames[i];
        if (f.emitted) continue;
        AppendLine(s, true, i, FrameKindText(f.kind), FrameHeader(s, f));
        f.emitted = true;
        f.lineEnd = s.buf.size();
        f.epoch = s.epoch;
    }
}

// 帧结束：帧头之后没写过别的行，结果接在帧头末尾；否则另起一行 ret。
void CloseFrameLocked(State& s, const Frame& f, size_t depth, const std::string& label, const std::string& result) {
    if (f.epoch == s.epoch && s.buf.size() == f.lineEnd && !s.buf.empty()) {
        s.buf.pop_back();
        s.buf += " -> ";
        s.buf += result;
        s.buf += '\n';
    } else {
        AppendLine(s, true, depth, "ret", label + " -> " + result);
    }
}

template <class Build>
void Effect(const char* kind, Build&& build) noexcept {
    State* s = g_state;
    if (!s || !Active()) return;
    try {
        const std::scoped_lock lock(s->mu);
        const bool ui = OnUiThread(*s);
        if (ui) {
            auto& frames = Frames();
            EmitPending(*s, frames);
            AppendLine(*s, true, frames.size(), kind, build(*s));
        } else {
            AppendLine(*s, false, 0, kind, build(*s));
        }
        MaybeFlushLocked(*s, ui);
    } catch (...) {
        // 取证代码不能影响被观察的行为。
    }
}

// 包装函数的调用帧：调用前写 call 行并入栈，真实调用期间同步发生的消息缩进在它下面。
class CallGuard {
public:
    template <class Args>
    CallGuard(const char* name, Args&& args) noexcept : name_(name) {
        State* s = g_state;
        if (!s || !Active()) return;
        try {
            const std::scoped_lock lock(s->mu);
            if (!OnUiThread(*s)) {
                background_ = true;
                args_ = args(*s);
                return;
            }
            auto& frames = Frames();
            EmitPending(*s, frames);
            AppendLine(*s, true, frames.size(), "call", std::string(name) + "(" + args(*s) + ")");
            Frame f;
            f.kind = FrameKind::Call;
            f.emitted = true;
            f.lineEnd = s->buf.size();
            f.epoch = s->epoch;
            f.header = name;
            frames.push_back(std::move(f));
            open_ = true;
        } catch (...) {
            open_ = false;
            background_ = false;
        }
    }

    template <class Result>
    void Done(Result&& result) noexcept {
        State* s = g_state;
        if (!s || done_) return;
        done_ = true;
        try {
            const std::scoped_lock lock(s->mu);
            if (background_) {
                AppendLine(*s, false, 0, "call", std::string(name_) + "(" + args_ + ") -> " + result(*s));
                MaybeFlushLocked(*s, false);
                return;
            }
            if (!open_) return;
            auto& frames = Frames();
            const Frame f = std::move(frames.back());
            frames.pop_back();
            CloseFrameLocked(*s, f, frames.size(), name_, result(*s));
            MaybeFlushLocked(*s, true);
        } catch (...) {
        }
    }

    ~CallGuard() {
        if (open_ && !done_) {
            Done([](State&) { return std::string("?"); });
        }
    }

    CallGuard(const CallGuard&) = delete;
    CallGuard& operator=(const CallGuard&) = delete;

private:
    const char* name_;  // 函数名，都是字符串字面量
    std::string args_;
    bool open_ = false;
    bool background_ = false;
    bool done_ = false;
};

std::string BoolResult(BOOL r) {
    return r ? "1" : "0";
}

std::string HresultText(HRESULT hr) {
    char b[16];
    snprintf(b, sizeof(b), "0x%08lX", static_cast<unsigned long>(hr));
    return b;
}

// ---------- 被包装的真实函数 ----------

struct RealFunctions {
    decltype(&::CreateWindowExW) CreateWindowExW = nullptr;
    decltype(&::DestroyWindow) DestroyWindow = nullptr;
    decltype(&::ShowWindow) ShowWindow = nullptr;
    decltype(&::ShowWindowAsync) ShowWindowAsync = nullptr;
    decltype(&::SetWindowPos) SetWindowPos = nullptr;
    decltype(&::MoveWindow) MoveWindow = nullptr;
    decltype(&::SetWindowLongW) SetWindowLongW = nullptr;
#ifdef _WIN64
    decltype(&::SetWindowLongPtrW) SetWindowLongPtrW = nullptr;
#endif
    decltype(&::SetWindowPlacement) SetWindowPlacement = nullptr;
    decltype(&::SetForegroundWindow) SetForegroundWindow = nullptr;
    decltype(&::SetActiveWindow) SetActiveWindow = nullptr;
    decltype(&::SetFocus) SetFocus = nullptr;
    decltype(&::BringWindowToTop) BringWindowToTop = nullptr;
    decltype(&::EnableWindow) EnableWindow = nullptr;
    decltype(&::RedrawWindow) RedrawWindow = nullptr;
    decltype(&::InvalidateRect) InvalidateRect = nullptr;
    decltype(&::UpdateWindow) UpdateWindow = nullptr;
    decltype(&::SetWindowRgn) SetWindowRgn = nullptr;
    decltype(&::SetLayeredWindowAttributes) SetLayeredWindowAttributes = nullptr;
    decltype(&::SetParent) SetParent = nullptr;
    decltype(&::SetTimer) SetTimer = nullptr;
    decltype(&::KillTimer) KillTimer = nullptr;
    decltype(&::PostMessageW) PostMessageW = nullptr;
    decltype(&::SendMessageW) SendMessageW = nullptr;
    decltype(&::SetWindowTextW) SetWindowTextW = nullptr;
    decltype(&::SetMenu) SetMenu = nullptr;
    decltype(&::DrawMenuBar) DrawMenuBar = nullptr;
    decltype(&::FlashWindowEx) FlashWindowEx = nullptr;
    decltype(&::DefWindowProcW) DefWindowProcW = nullptr;
    decltype(&::DwmSetWindowAttribute) DwmSetWindowAttribute = nullptr;
    decltype(&::DwmExtendFrameIntoClientArea) DwmExtendFrameIntoClientArea = nullptr;
    decltype(&::DwmEnableBlurBehindWindow) DwmEnableBlurBehindWindow = nullptr;
    decltype(&::DwmDefWindowProc) DwmDefWindowProc = nullptr;
    decltype(&::DwmFlush) DwmFlush = nullptr;
    decltype(&::GetProcAddress) GetProcAddress = nullptr;
};

RealFunctions g_real;

using AllowDarkModeForWindowFn = bool(WINAPI*)(HWND, bool);
using SetPreferredAppModeFn = int(WINAPI*)(int);
using FlushMenuThemesFn = void(WINAPI*)();
AllowDarkModeForWindowFn g_realAllowDarkModeForWindow = nullptr;
SetPreferredAppModeFn g_realSetPreferredAppMode = nullptr;
FlushMenuThemesFn g_realFlushMenuThemes = nullptr;

// 调用真实函数，前后记一帧，并把真实函数留下的 LastError 还给调用方。
template <class Args, class Call, class Res>
auto Traced(const char* name, Args&& args, Call&& call, Res&& res) {
    CallGuard guard(name, std::forward<Args>(args));
    auto r = call();
    const DWORD err = GetLastError();
    guard.Done([&](State& s) { return res(s, r); });
    SetLastError(err);
    return r;
}

const auto kBoolRes = [](State&, BOOL r) { return BoolResult(r); };
const auto kHresultRes = [](State&, HRESULT hr) { return HresultText(hr); };

// ---------- 包装函数 ----------

HWND WINAPI Hook_CreateWindowExW(DWORD ex, LPCWSTR cls, LPCWSTR title, DWORD style, int x, int y, int w,
                                 int h, HWND parent, HMENU menu, HINSTANCE inst, LPVOID param) {
    return Traced(
        "CreateWindowExW",
        [&](State& s) {
            const std::string clsText = IS_INTRESOURCE(cls) ? "#" + Dec(reinterpret_cast<ULONG_PTR>(cls))
                                                            : Clean(Utf8(cls), 60);
            return clsText + ", style=" + Hex(style) + ", ex=" + Hex(ex) + ", " + Dec(x) + "," + Dec(y) + " " +
                   Dec(w) + "x" + Dec(h) + ", parent=" + HwndName(s, parent);
        },
        [&] { return g_real.CreateWindowExW(ex, cls, title, style, x, y, w, h, parent, menu, inst, param); },
        [](State& s, HWND r) { return HwndName(s, r); });
}

BOOL WINAPI Hook_DestroyWindow(HWND h) {
    return Traced(
        "DestroyWindow", [&](State& s) { return HwndName(s, h); }, [&] { return g_real.DestroyWindow(h); },
        kBoolRes);
}

BOOL WINAPI Hook_ShowWindow(HWND h, int cmd) {
    return Traced(
        "ShowWindow", [&](State& s) { return HwndName(s, h) + ", " + ShowCmdName(cmd); },
        [&] { return g_real.ShowWindow(h, cmd); }, kBoolRes);
}

BOOL WINAPI Hook_ShowWindowAsync(HWND h, int cmd) {
    return Traced(
        "ShowWindowAsync", [&](State& s) { return HwndName(s, h) + ", " + ShowCmdName(cmd); },
        [&] { return g_real.ShowWindowAsync(h, cmd); }, kBoolRes);
}

BOOL WINAPI Hook_SetWindowPos(HWND h, HWND after, int x, int y, int cx, int cy, UINT flags) {
    return Traced(
        "SetWindowPos",
        [&](State& s) {
            std::string out = HwndName(s, h) + ", " + FlagsText(flags, kSwpFlags);
            if (!(flags & SWP_NOZORDER)) out += ", after=" + InsertAfterName(s, after);
            if (!(flags & SWP_NOMOVE)) out += ", pos=" + Dec(x) + "," + Dec(y);
            if (!(flags & SWP_NOSIZE)) out += ", size=" + Dec(cx) + "x" + Dec(cy);
            return out;
        },
        [&] { return g_real.SetWindowPos(h, after, x, y, cx, cy, flags); }, kBoolRes);
}

BOOL WINAPI Hook_MoveWindow(HWND h, int x, int y, int w, int hgt, BOOL repaint) {
    return Traced(
        "MoveWindow",
        [&](State& s) {
            return HwndName(s, h) + ", " + Dec(x) + "," + Dec(y) + " " + Dec(w) + "x" + Dec(hgt) + ", repaint=" +
                   BoolResult(repaint);
        },
        [&] { return g_real.MoveWindow(h, x, y, w, hgt, repaint); }, kBoolRes);
}

// GWL_STYLE / GWL_EXSTYLE 记新旧值；其余下标多是指针（用户数据、窗口过程），只记下标。
std::string LongIndexArgs(State& s, HWND h, int index, LONG_PTR value) {
    switch (index) {
    case GWL_STYLE:
        return HwndName(s, h) + ", STYLE, " + Hex(static_cast<ULONG_PTR>(GetWindowLongPtrW(h, GWL_STYLE))) + "->" +
               Hex(static_cast<ULONG_PTR>(value));
    case GWL_EXSTYLE:
        return HwndName(s, h) + ", EXSTYLE, " + Hex(static_cast<ULONG_PTR>(GetWindowLongPtrW(h, GWL_EXSTYLE))) +
               "->" + Hex(static_cast<ULONG_PTR>(value));
    case GWLP_USERDATA:
        return HwndName(s, h) + ", USERDATA";
    case GWLP_WNDPROC:
        return HwndName(s, h) + ", WNDPROC";
    default:
        return HwndName(s, h) + ", " + Dec(index);
    }
}

std::string LongResult(int index, LONG_PTR r) {
    return index == GWL_STYLE || index == GWL_EXSTYLE ? Hex(static_cast<ULONG_PTR>(r)) : (r ? "set" : "0");
}

LONG WINAPI Hook_SetWindowLongW(HWND h, int index, LONG value) {
    return Traced(
        "SetWindowLongW", [&](State& s) { return LongIndexArgs(s, h, index, value); },
        [&] { return g_real.SetWindowLongW(h, index, value); },
        [&](State&, LONG r) { return LongResult(index, r); });
}

#ifdef _WIN64
LONG_PTR WINAPI Hook_SetWindowLongPtrW(HWND h, int index, LONG_PTR value) {
    return Traced(
        "SetWindowLongPtrW", [&](State& s) { return LongIndexArgs(s, h, index, value); },
        [&] { return g_real.SetWindowLongPtrW(h, index, value); },
        [&](State&, LONG_PTR r) { return LongResult(index, r); });
}
#endif

BOOL WINAPI Hook_SetWindowPlacement(HWND h, const WINDOWPLACEMENT* wp) {
    return Traced(
        "SetWindowPlacement",
        [&](State& s) {
            if (!wp) return HwndName(s, h) + ", null";
            return HwndName(s, h) + ", " + ShowCmdName(static_cast<int>(wp->showCmd)) + ", " +
                   RectText(wp->rcNormalPosition) + ", flags=" + Hex(wp->flags);
        },
        [&] { return g_real.SetWindowPlacement(h, wp); }, kBoolRes);
}

BOOL WINAPI Hook_SetForegroundWindow(HWND h) {
    return Traced(
        "SetForegroundWindow", [&](State& s) { return HwndName(s, h); },
        [&] { return g_real.SetForegroundWindow(h); }, kBoolRes);
}

HWND WINAPI Hook_SetActiveWindow(HWND h) {
    return Traced(
        "SetActiveWindow", [&](State& s) { return HwndName(s, h); }, [&] { return g_real.SetActiveWindow(h); },
        [](State& s, HWND r) { return HwndName(s, r); });
}

HWND WINAPI Hook_SetFocus(HWND h) {
    return Traced(
        "SetFocus", [&](State& s) { return HwndName(s, h); }, [&] { return g_real.SetFocus(h); },
        [](State&, HWND) { return std::string("done"); });
}

BOOL WINAPI Hook_BringWindowToTop(HWND h) {
    return Traced(
        "BringWindowToTop", [&](State& s) { return HwndName(s, h); }, [&] { return g_real.BringWindowToTop(h); },
        kBoolRes);
}

BOOL WINAPI Hook_EnableWindow(HWND h, BOOL enable) {
    return Traced(
        "EnableWindow", [&](State& s) { return HwndName(s, h) + ", " + BoolResult(enable); },
        [&] { return g_real.EnableWindow(h, enable); }, kBoolRes);
}

BOOL WINAPI Hook_RedrawWindow(HWND h, const RECT* rc, HRGN rgn, UINT flags) {
    return Traced(
        "RedrawWindow",
        [&](State& s) {
            return HwndName(s, h) + ", " + (rc ? RectText(*rc) : "null") + ", " + (rgn ? "rgn" : "null") + ", " +
                   FlagsText(flags, kRdwFlags);
        },
        [&] { return g_real.RedrawWindow(h, rc, rgn, flags); }, kBoolRes);
}

BOOL WINAPI Hook_InvalidateRect(HWND h, const RECT* rc, BOOL erase) {
    return Traced(
        "InvalidateRect",
        [&](State& s) { return HwndName(s, h) + ", " + (rc ? RectText(*rc) : "null") + ", " + BoolResult(erase); },
        [&] { return g_real.InvalidateRect(h, rc, erase); }, kBoolRes);
}

BOOL WINAPI Hook_UpdateWindow(HWND h) {
    return Traced(
        "UpdateWindow", [&](State& s) { return HwndName(s, h); }, [&] { return g_real.UpdateWindow(h); },
        kBoolRes);
}

int WINAPI Hook_SetWindowRgn(HWND h, HRGN rgn, BOOL redraw) {
    return Traced(
        "SetWindowRgn",
        [&](State& s) { return HwndName(s, h) + ", " + (rgn ? "rgn" : "null") + ", " + BoolResult(redraw); },
        [&] { return g_real.SetWindowRgn(h, rgn, redraw); }, [](State&, int r) { return Dec(r); });
}

BOOL WINAPI Hook_SetLayeredWindowAttributes(HWND h, COLORREF key, BYTE alpha, DWORD flags) {
    return Traced(
        "SetLayeredWindowAttributes",
        [&](State& s) {
            return HwndName(s, h) + ", key=" + Hex(key) + ", alpha=" + Dec(alpha) + ", flags=" + Hex(flags);
        },
        [&] { return g_real.SetLayeredWindowAttributes(h, key, alpha, flags); }, kBoolRes);
}

HWND WINAPI Hook_SetParent(HWND child, HWND parent) {
    return Traced(
        "SetParent", [&](State& s) { return HwndName(s, child) + ", " + HwndName(s, parent); },
        [&] { return g_real.SetParent(child, parent); }, [](State& s, HWND r) { return HwndName(s, r); });
}

// 没有窗口的定时器由系统分配 id，每次运行都不同，不记 id。
UINT_PTR WINAPI Hook_SetTimer(HWND h, UINT_PTR id, UINT elapse, TIMERPROC proc) {
    return Traced(
        "SetTimer",
        [&](State& s) {
            return HwndName(s, h) + ", " + (h ? Dec(static_cast<long long>(id)) : std::string("auto")) + ", " +
                   Dec(elapse) + "ms" + (proc ? ", proc" : "");
        },
        [&] { return g_real.SetTimer(h, id, elapse, proc); },
        [&](State&, UINT_PTR r) { return h ? Dec(static_cast<long long>(r)) : std::string(r ? "auto" : "0"); });
}

BOOL WINAPI Hook_KillTimer(HWND h, UINT_PTR id) {
    return Traced(
        "KillTimer",
        [&](State& s) { return HwndName(s, h) + ", " + (h ? Dec(static_cast<long long>(id)) : std::string("auto")); },
        [&] { return g_real.KillTimer(h, id); }, kBoolRes);
}

std::string PostedMessageArgs(State& s, HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    std::string out = HwndName(s, h) + ", " + MessageName(msg);
    if (IsAppMessage(msg)) out += ", wp=" + Hex(wp) + ", lp=" + Hex(static_cast<unsigned long long>(lp));
    return out;
}

BOOL WINAPI Hook_PostMessageW(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    return Traced(
        "PostMessageW", [&](State& s) { return PostedMessageArgs(s, h, msg, wp, lp); },
        [&] { return g_real.PostMessageW(h, msg, wp, lp); }, kBoolRes);
}

LRESULT WINAPI Hook_SendMessageW(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    return Traced(
        "SendMessageW", [&](State& s) { return PostedMessageArgs(s, h, msg, wp, lp); },
        [&] { return g_real.SendMessageW(h, msg, wp, lp); },
        [&](State&, LRESULT r) { return IsAppMessage(msg) ? Dec(static_cast<long long>(r)) : MessageResult(msg, r); });
}

BOOL WINAPI Hook_SetWindowTextW(HWND h, LPCWSTR text) {
    return Traced(
        "SetWindowTextW", [&](State& s) { return HwndName(s, h) + ", \"" + Clean(Utf8(text), 60) + "\""; },
        [&] { return g_real.SetWindowTextW(h, text); }, kBoolRes);
}

BOOL WINAPI Hook_SetMenu(HWND h, HMENU menu) {
    return Traced(
        "SetMenu", [&](State& s) { return HwndName(s, h) + ", " + (menu ? "menu" : "null"); },
        [&] { return g_real.SetMenu(h, menu); }, kBoolRes);
}

BOOL WINAPI Hook_DrawMenuBar(HWND h) {
    return Traced(
        "DrawMenuBar", [&](State& s) { return HwndName(s, h); }, [&] { return g_real.DrawMenuBar(h); }, kBoolRes);
}

BOOL WINAPI Hook_FlashWindowEx(PFLASHWINFO info) {
    return Traced(
        "FlashWindowEx",
        [&](State& s) {
            if (!info) return std::string("null");
            return HwndName(s, info->hwnd) + ", flags=" + Hex(info->dwFlags) + ", count=" + Dec(info->uCount);
        },
        [&] { return g_real.FlashWindowEx(info); }, kBoolRes);
}

// 默认处理只标记在当前消息帧上：安静帧不因为走了默认处理就被写出来。
// 当前消息帧已写出时，照常记成一次调用，默认处理里同步发生的消息缩进在它下面。
bool MarkPassThrough(bool dwm) {
    State* s = g_state;
    if (!s || !Active()) return false;
    try {
        const std::scoped_lock lock(s->mu);
        if (!OnUiThread(*s)) return false;
        auto& frames = Frames();
        if (frames.empty()) return false;
        Frame& top = frames.back();
        if (top.kind != FrameKind::Message) return true;
        (dwm ? top.sawDwm : top.sawDef) = true;
        return top.emitted;
    } catch (...) {
        return false;
    }
}

LRESULT WINAPI Hook_DefWindowProcW(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (!MarkPassThrough(false)) return g_real.DefWindowProcW(h, msg, wp, lp);
    return Traced(
        "DefWindowProcW", [&](State&) { return MessageName(msg); },
        [&] { return g_real.DefWindowProcW(h, msg, wp, lp); },
        [&](State&, LRESULT r) { return MessageResult(msg, r); });
}

BOOL WINAPI Hook_DwmDefWindowProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, LRESULT* out) {
    if (!MarkPassThrough(true)) return g_real.DwmDefWindowProc(h, msg, wp, lp, out);
    return Traced(
        "DwmDefWindowProc", [&](State&) { return MessageName(msg); },
        [&] { return g_real.DwmDefWindowProc(h, msg, wp, lp, out); },
        [&](State&, BOOL r) { return r && out ? "1 " + MessageResult(msg, *out) : BoolResult(r); });
}

std::string DwmValueText(LPCVOID data, DWORD size) {
    if (!data) return "null";
    if (size == sizeof(DWORD)) return Dec(*static_cast<const LONG*>(data));
    std::string out = "bytes:";
    const auto* b = static_cast<const unsigned char*>(data);
    for (DWORD i = 0; i < size && i < 16; ++i) {
        char hex[4];
        snprintf(hex, sizeof(hex), "%02X", b[i]);
        out += hex;
    }
    return out;
}

HRESULT WINAPI Hook_DwmSetWindowAttribute(HWND h, DWORD attr, LPCVOID data, DWORD size) {
    return Traced(
        "DwmSetWindowAttribute",
        [&](State& s) { return HwndName(s, h) + ", " + DwmAttributeName(attr) + ", " + DwmValueText(data, size); },
        [&] { return g_real.DwmSetWindowAttribute(h, attr, data, size); }, kHresultRes);
}

HRESULT WINAPI Hook_DwmExtendFrameIntoClientArea(HWND h, const MARGINS* m) {
    return Traced(
        "DwmExtendFrameIntoClientArea",
        [&](State& s) {
            if (!m) return HwndName(s, h) + ", null";
            return HwndName(s, h) + ", " + Dec(m->cxLeftWidth) + "," + Dec(m->cxRightWidth) + "," +
                   Dec(m->cyTopHeight) + "," + Dec(m->cyBottomHeight);
        },
        [&] { return g_real.DwmExtendFrameIntoClientArea(h, m); }, kHresultRes);
}

HRESULT WINAPI Hook_DwmEnableBlurBehindWindow(HWND h, const DWM_BLURBEHIND* bb) {
    return Traced(
        "DwmEnableBlurBehindWindow",
        [&](State& s) {
            if (!bb) return HwndName(s, h) + ", null";
            return HwndName(s, h) + ", flags=" + Hex(bb->dwFlags) + ", enable=" + BoolResult(bb->fEnable) +
                   (bb->hRgnBlur ? ", rgn" : "");
        },
        [&] { return g_real.DwmEnableBlurBehindWindow(h, bb); }, kHresultRes);
}

HRESULT WINAPI Hook_DwmFlush() {
    return Traced(
        "DwmFlush", [](State&) { return std::string(); }, [] { return g_real.DwmFlush(); }, kHresultRes);
}

bool WINAPI Hook_AllowDarkModeForWindow(HWND h, bool allow) {
    return Traced(
        "uxtheme.AllowDarkModeForWindow", [&](State& s) { return HwndName(s, h) + ", " + (allow ? "1" : "0"); },
        [&] { return g_realAllowDarkModeForWindow(h, allow); },
        [](State&, bool r) { return std::string(r ? "1" : "0"); });
}

int WINAPI Hook_SetPreferredAppMode(int mode) {
    return Traced(
        "uxtheme.SetPreferredAppMode", [&](State&) { return Dec(mode); },
        [&] { return g_realSetPreferredAppMode(mode); }, [](State&, int r) { return Dec(r); });
}

void WINAPI Hook_FlushMenuThemes() {
    CallGuard guard("uxtheme.FlushMenuThemes", [](State&) { return std::string(); });
    g_realFlushMenuThemes();
    const DWORD err = GetLastError();
    guard.Done([](State&) { return std::string("done"); });
    SetLastError(err);
}

// uxtheme 的暗色模式函数没有名字，只能按序号取：133 AllowDarkModeForWindow、
// 135 SetPreferredAppMode、136 FlushMenuThemes。
FARPROC WINAPI Hook_GetProcAddress(HMODULE hModule, LPCSTR name) {
    const FARPROC p = g_real.GetProcAddress(hModule, name);
    if (!p || !IS_INTRESOURCE(name) || hModule != GetModuleHandleW(L"uxtheme.dll")) return p;
    switch (LOWORD(reinterpret_cast<ULONG_PTR>(name))) {
    case 133:
        g_realAllowDarkModeForWindow = reinterpret_cast<AllowDarkModeForWindowFn>(p);
        return reinterpret_cast<FARPROC>(&Hook_AllowDarkModeForWindow);
    case 135:
        g_realSetPreferredAppMode = reinterpret_cast<SetPreferredAppModeFn>(p);
        return reinterpret_cast<FARPROC>(&Hook_SetPreferredAppMode);
    case 136:
        g_realFlushMenuThemes = reinterpret_cast<FlushMenuThemesFn>(p);
        return reinterpret_cast<FARPROC>(&Hook_FlushMenuThemes);
    default:
        return p;
    }
}

// ---------- 消息帧 ----------

void PushMessageFrame(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    State* s = g_state;
    try {
        const std::scoped_lock lock(s->mu);
        auto& frames = Frames();
        Frame f;
        f.kind = FrameKind::Message;
        f.quiet = IsQuietMessage(msg);
        f.hwnd = h;
        f.msg = msg;
        f.wp = wp;
        f.lp = lp;
        frames.push_back(std::move(f));
        if (!frames.back().quiet) EmitPending(*s, frames);
    } catch (...) {
    }
}

void PopMessageFrame(HWND h, UINT msg, WPARAM wp, LPARAM lp, LRESULT r) {
    State* s = g_state;
    try {
        const std::scoped_lock lock(s->mu);
        auto& frames = Frames();
        if (frames.empty() || frames.back().kind != FrameKind::Message) return;
        const Frame f = std::move(frames.back());
        frames.pop_back();
        if (!f.emitted) {
            // 安静帧没有副作用：同一窗口、消息与结果的组合只记第一次。
            unsigned long long raw = reinterpret_cast<ULONG_PTR>(h);
            raw = raw * 0x100000001B3ull ^ msg;
            raw = raw * 0x100000001B3ull ^ static_cast<unsigned long long>(r);
            raw = raw * 0x100000001B3ull ^ ((f.sawDef ? 1u : 0u) | (f.sawDwm ? 2u : 0u));
            if (!s->quietRaw.insert(raw).second) return;
        }
        std::string result = MessageResult(msg, r);
        if (f.sawDef) result += " def";
        if (f.sawDwm) result += " dwm";
        if (f.emitted) {
            CloseFrameLocked(*s, f, frames.size(), MessageName(msg), result + MessageOutput(msg, wp, lp, *s));
        } else {
            std::string key = HwndName(*s, h) + " " + MessageName(msg) + " -> " + result;
            if (s->quietSeen.insert(key).second) AppendLine(*s, true, 0, "quiet", key);
        }
        MaybeFlushLocked(*s, true);
    } catch (...) {
    }
}

void ForgetWindow(HWND h) {
    State* s = g_state;
    try {
        std::string name;
        {
            const std::scoped_lock lock(s->mu);
            name = HwndName(*s, h);
        }
        Effect("win-", [&](State&) { return name; });
        const std::scoped_lock lock(s->mu);
        s->names.erase(h);
    } catch (...) {
    }
}

LRESULT CALLBACK SubclassProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    State* s = g_state;
    if (!s || !Active() || !OnUiThread(*s)) {
        if (msg == WM_NCDESTROY) RemoveWindowSubclass(h, SubclassProc, kSubclassId);
        return DefSubclassProc(h, msg, wp, lp);
    }
    PushMessageFrame(h, msg, wp, lp);
    const LRESULT r = DefSubclassProc(h, msg, wp, lp);
    const DWORD err = GetLastError();
    PopMessageFrame(h, msg, wp, lp, r);
    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(h, SubclassProc, kSubclassId);
        ForgetWindow(h);
    }
    SetLastError(err);
    return r;
}

// 本模块注册的窗口类（主窗口、弹窗、面板宿主、菜单浮层等）一创建就挂子类。
// HCBT_CREATEWND 时窗口过程已就位、WM_NCCREATE 还没发，从第一条消息开始都能记到。
LRESULT CALLBACK CbtProc(int code, WPARAM wp, LPARAM lp) {
    State* s = g_state;
    if (code == HCBT_CREATEWND && s && Active()) {
        const HWND h = reinterpret_cast<HWND>(wp);
        const auto classModule = reinterpret_cast<HMODULE>(GetClassLongPtrW(h, GCLP_HMODULE));
        if (classModule == s->self) {
            try {
                std::string name;
                {
                    const std::scoped_lock lock(s->mu);
                    const std::string cls = ClassNameOf(h);
                    name = cls + "#" + Dec(++s->classCounts[cls]);
                    s->names[h] = name;
                }
                const auto* cw = reinterpret_cast<const CBT_CREATEWNDW*>(lp);
                const CREATESTRUCTW* cs = cw ? cw->lpcs : nullptr;
                Effect("win+", [&](State& st) {
                    if (!cs) return name;
                    return name + " parent=" + HwndName(st, cs->hwndParent) + " style=" + Hex(cs->style) +
                           " ex=" + Hex(cs->dwExStyle);
                });
                SetWindowSubclass(h, SubclassProc, kSubclassId, 0);
            } catch (...) {
            }
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

// ---------- 导入表 ----------

struct HookEntry {
    const wchar_t* dll;
    const char* name;
    void** real;
    void* hook;
};

template <class Fn>
HookEntry Entry(const wchar_t* dll, const char* name, Fn& real, Fn hook) {
    return HookEntry{dll, name, reinterpret_cast<void**>(&real), reinterpret_cast<void*>(hook)};
}

std::vector<HookEntry> HookTable() {
    const wchar_t* user32 = L"user32.dll";
    const wchar_t* dwmapi = L"dwmapi.dll";
    return {
        Entry(user32, "CreateWindowExW", g_real.CreateWindowExW, &Hook_CreateWindowExW),
        Entry(user32, "DestroyWindow", g_real.DestroyWindow, &Hook_DestroyWindow),
        Entry(user32, "ShowWindow", g_real.ShowWindow, &Hook_ShowWindow),
        Entry(user32, "ShowWindowAsync", g_real.ShowWindowAsync, &Hook_ShowWindowAsync),
        Entry(user32, "SetWindowPos", g_real.SetWindowPos, &Hook_SetWindowPos),
        Entry(user32, "MoveWindow", g_real.MoveWindow, &Hook_MoveWindow),
        Entry(user32, "SetWindowLongW", g_real.SetWindowLongW, &Hook_SetWindowLongW),
#ifdef _WIN64
        Entry(user32, "SetWindowLongPtrW", g_real.SetWindowLongPtrW, &Hook_SetWindowLongPtrW),
#endif
        Entry(user32, "SetWindowPlacement", g_real.SetWindowPlacement, &Hook_SetWindowPlacement),
        Entry(user32, "SetForegroundWindow", g_real.SetForegroundWindow, &Hook_SetForegroundWindow),
        Entry(user32, "SetActiveWindow", g_real.SetActiveWindow, &Hook_SetActiveWindow),
        Entry(user32, "SetFocus", g_real.SetFocus, &Hook_SetFocus),
        Entry(user32, "BringWindowToTop", g_real.BringWindowToTop, &Hook_BringWindowToTop),
        Entry(user32, "EnableWindow", g_real.EnableWindow, &Hook_EnableWindow),
        Entry(user32, "RedrawWindow", g_real.RedrawWindow, &Hook_RedrawWindow),
        Entry(user32, "InvalidateRect", g_real.InvalidateRect, &Hook_InvalidateRect),
        Entry(user32, "UpdateWindow", g_real.UpdateWindow, &Hook_UpdateWindow),
        Entry(user32, "SetWindowRgn", g_real.SetWindowRgn, &Hook_SetWindowRgn),
        Entry(user32, "SetLayeredWindowAttributes", g_real.SetLayeredWindowAttributes,
              &Hook_SetLayeredWindowAttributes),
        Entry(user32, "SetParent", g_real.SetParent, &Hook_SetParent),
        Entry(user32, "SetTimer", g_real.SetTimer, &Hook_SetTimer),
        Entry(user32, "KillTimer", g_real.KillTimer, &Hook_KillTimer),
        Entry(user32, "PostMessageW", g_real.PostMessageW, &Hook_PostMessageW),
        Entry(user32, "SendMessageW", g_real.SendMessageW, &Hook_SendMessageW),
        Entry(user32, "SetWindowTextW", g_real.SetWindowTextW, &Hook_SetWindowTextW),
        Entry(user32, "SetMenu", g_real.SetMenu, &Hook_SetMenu),
        Entry(user32, "DrawMenuBar", g_real.DrawMenuBar, &Hook_DrawMenuBar),
        Entry(user32, "FlashWindowEx", g_real.FlashWindowEx, &Hook_FlashWindowEx),
        Entry(user32, "DefWindowProcW", g_real.DefWindowProcW, &Hook_DefWindowProcW),
        Entry(dwmapi, "DwmSetWindowAttribute", g_real.DwmSetWindowAttribute, &Hook_DwmSetWindowAttribute),
        Entry(dwmapi, "DwmExtendFrameIntoClientArea", g_real.DwmExtendFrameIntoClientArea,
              &Hook_DwmExtendFrameIntoClientArea),
        Entry(dwmapi, "DwmEnableBlurBehindWindow", g_real.DwmEnableBlurBehindWindow,
              &Hook_DwmEnableBlurBehindWindow),
        Entry(dwmapi, "DwmDefWindowProc", g_real.DwmDefWindowProc, &Hook_DwmDefWindowProc),
        Entry(dwmapi, "DwmFlush", g_real.DwmFlush, &Hook_DwmFlush),
        Entry(L"kernel32.dll", "GetProcAddress", g_real.GetProcAddress, &Hook_GetProcAddress),
    };
}

// 按地址匹配导入表槽位：槽位里是加载器解析出的最终地址（含转发），
// 与 GetProcAddress 取到的相同，无论导入时按名字、按序号还是经 api-set。
int PatchImports(HMODULE self, const std::vector<HookEntry>& table) {
    auto* base = reinterpret_cast<BYTE*>(self);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return 0;

    std::vector<std::pair<ULONG_PTR, const HookEntry*>> targets;
    for (const HookEntry& e : table) {
        HMODULE dll = GetModuleHandleW(e.dll);
        FARPROC real = dll ? GetProcAddress(dll, e.name) : nullptr;
        if (!real) continue;
        *e.real = reinterpret_cast<void*>(real);
        targets.emplace_back(reinterpret_cast<ULONG_PTR>(real), &e);
    }

    int patched = 0;
    for (auto* d = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
        for (auto* t = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk); t->u1.Function; ++t) {
            for (const auto& [address, entry] : targets) {
                if (t->u1.Function != address) continue;
                DWORD old = 0;
                if (VirtualProtect(&t->u1.Function, sizeof(t->u1.Function), PAGE_READWRITE, &old)) {
                    t->u1.Function = reinterpret_cast<ULONG_PTR>(entry->hook);
                    VirtualProtect(&t->u1.Function, sizeof(t->u1.Function), old, &old);
                    ++patched;
                }
                break;
            }
        }
    }
    return patched;
}

// ---------- 安装与收尾 ----------

bool EnvFlagSet() {
    size_t len = 0;
    char buf[8] = {};
    if (getenv_s(&len, buf, sizeof(buf), "FOO_UI_WEBVIEW2_BEHAVIOR_TRACE") == 0 && len > 0) {
        const char c = buf[0];
        return c == '1' || c == 'y' || c == 'Y' || c == 't' || c == 'T';
    }
    return false;
}

std::wstring ProfileDir() {
    pfc::string8 path;
    filesystem::g_get_display_path(core_api::get_profile_path(), path);
    std::wstring dir = pfc::stringcvt::string_wide_from_utf8(path.get_ptr()).get_ptr();
    if (!dir.empty() && dir.back() != L'\\') dir += L'\\';
    return dir;
}

void Install() {
    if (g_state) return;
    try {
        const std::wstring dir = ProfileDir();
        if (dir.empty()) return;
        if (!EnvFlagSet() && GetFileAttributesW((dir + L"webview_behavior_trace.on").c_str()) == INVALID_FILE_ATTRIBUTES) {
            return;
        }
        const std::wstring logPath = dir + L"webview_behavior_trace.log";
        const HANDLE file = CreateFileW(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            console::print("[WebView2 UI] window behavior trace: cannot open the log file");
            return;
        }
        auto* s = new State();
        s->file = file;
        s->self = core_api::get_my_instance();
        s->uiThread = GetCurrentThreadId();
        g_state = s;

        const int patched = PatchImports(s->self, HookTable());
        s->cbt = SetWindowsHookExW(WH_CBT, CbtProc, nullptr, s->uiThread);
        AppendLine(*s, true, 0, "note",
                   "trace-begin v=1 patched=" + Dec(patched) + " cbt=" + (s->cbt ? "1" : "0"));
        detail::g_active.store(true, std::memory_order_release);
        console::print("[WebView2 UI] window behavior trace on: ",
                       pfc::stringcvt::string_utf8_from_wide(logPath.c_str()).get_ptr());
    } catch (...) {
    }
}

void FlushNow() {
    State* s = g_state;
    if (!s) return;
    try {
        const std::scoped_lock lock(s->mu);
        FlushLocked(*s);
    } catch (...) {
    }
}

class BehaviorTraceInitStage : public init_stage_callback {
public:
    void on_init_stage(t_uint32 stage) override {
        if (stage == init_stages::before_ui_init) Install();
    }
};

class BehaviorTraceQuit : public initquit {
public:
    void on_quit() override {
        Effect("note", [](State&) { return std::string("on_quit"); });
        FlushNow();
    }
};

// 主窗口在 on_quit 之后才销毁，这之间的轨迹在 DLL 卸载时补写。
struct FinalFlush {
    FinalFlush() = default;
    FinalFlush(const FinalFlush&) = delete;
    FinalFlush& operator=(const FinalFlush&) = delete;
    ~FinalFlush() {
        State* s = g_state;
        if (!s) return;
        detail::g_active.store(false, std::memory_order_release);
        try {
            const std::scoped_lock lock(s->mu);
            AppendLine(*s, true, 0, "note", "trace-end");
            FlushLocked(*s);
            CloseHandle(s->file);
            s->file = INVALID_HANDLE_VALUE;
        } catch (...) {
        }
    }
};

FinalFlush g_finalFlush;

service_factory_single_t<BehaviorTraceInitStage> g_behavior_trace_init_stage;
initquit_factory_t<BehaviorTraceQuit> g_behavior_trace_quit;

}  // namespace

// ---------- 对外记录点 ----------

void Com(HWND owner, const char* op) noexcept {
    if (!Active()) return;
    Effect("com", [&](State& s) { return HwndName(s, owner) + " " + op; });
}

void Com(HWND owner, const char* op, const RECT& bounds) noexcept {
    if (!Active()) return;
    Effect("com", [&](State& s) { return HwndName(s, owner) + " " + op + " " + RectText(bounds); });
}

void Com(HWND owner, const char* op, long long value) noexcept {
    if (!Active()) return;
    Effect("com", [&](State& s) { return HwndName(s, owner) + " " + op + " " + Dec(value); });
}

void Event(HWND owner, const std::string& name) noexcept {
    if (!Active()) return;
    static const char* const kPrefixes[] = {"window:", "ui:", "panel:", "menu:", "tray:", "taskbar:", "app:"};
    const bool wanted = std::any_of(std::begin(kPrefixes), std::end(kPrefixes),
                                    [&](const char* prefix) { return name.starts_with(prefix); });
    if (!wanted) return;
    Effect("event", [&](State& s) { return HwndName(s, owner) + " " + Clean(name, 60); });
}

InvokeScope::InvokeScope(const std::string& method, const nlohmann::json& params) noexcept {
    State* s = g_state;
    if (!s || !Active()) return;
    try {
        const std::scoped_lock lock(s->mu);
        if (!OnUiThread(*s)) return;
        Frame f;
        f.kind = FrameKind::Invoke;
        f.quiet = true;
        f.method = &method;
        f.params = &params;
        Frames().push_back(std::move(f));
        open_ = true;
    } catch (...) {
        open_ = false;
    }
}

InvokeScope::~InvokeScope() {
    State* s = g_state;
    if (!open_ || !s) return;
    try {
        const std::scoped_lock lock(s->mu);
        auto& frames = Frames();
        if (frames.empty() || frames.back().kind != FrameKind::Invoke) return;
        const Frame f = std::move(frames.back());
        frames.pop_back();
        if (f.emitted) {
            CloseFrameLocked(*s, f, frames.size(), "invoke", "end");
        } else if (s->quietRaw.insert(std::hash<std::string>{}(*f.method)).second) {
            std::string key = "invoke " + Clean(*f.method, 80);
            if (s->quietSeen.insert(key).second) AppendLine(*s, true, 0, "quiet", key);
        }
        MaybeFlushLocked(*s, true);
    } catch (...) {
    }
}

}  // namespace window_behavior_trace
