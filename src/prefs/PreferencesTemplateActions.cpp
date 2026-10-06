// ============================================
// PreferencesTemplateActions.cpp - 总览页的模板命令
// 起名对话框，以及 Manage 菜单的新建、重命名、删除与打开模板文件夹。
// ============================================

#include "pch.h"
#include "prefs/PreferencesPage.h"
#include "prefs/PreferencesPageInternal.h"
#include "prefs/PreferencesFields.h"
#include "prefs/PreferencesTemplateName.h"
#include "core/WebViewContext.h"
#include "core/WebViewPanel.h"
#include "panels/PanelConfig.h"
#include <commctrl.h>
#include <filesystem>
#include <iterator>
#include <string>
#include <vector>
#include "utils/I18n.h"

namespace fs = std::filesystem;

namespace webview_prefs {

// 总览页字段表的下标与枚举上限。
namespace overview = prefs_fields::overview;

namespace {

// 模板下拉框当前选中项对应的名字（UTF-8）。列表里的“（不存在）”标注项不是真名字，
// 剥掉后缀再返回；没有选中项时返回空串。
std::string SelectedTemplateName(HWND hwnd) {
    HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_TEMPLATE);
    if (!hCombo) return {};
    const int sel = static_cast<int>(SendMessageW(hCombo, CB_GETCURSEL, 0, 0));
    if (sel < 0) return {};
    const int length = static_cast<int>(SendMessageW(hCombo, CB_GETLBTEXTLEN, sel, 0));
    if (length < 0) return {};
    std::wstring item(static_cast<size_t>(length) + 1, L'\0');
    SendMessageW(hCombo, CB_GETLBTEXT, sel, reinterpret_cast<LPARAM>(item.data()));
    item.resize(static_cast<size_t>(length));
    const std::wstring missingSuffix = TR(" (missing)", "（不存在）");
    if (item.size() > missingSuffix.size() && item.ends_with(missingSuffix)) {
        item.resize(item.size() - missingSuffix.size());
    }
    return pfc::stringcvt::string_utf8_from_wide(item.c_str()).get_ptr();
}

// 已加载面板里显式指定了该模板的个数。跟随全局模板的面板不在此列，它们由
// “活动模板不能改名 / 删除”那条保护覆盖。只看 WebViewContext 注册表里活着的实例，
// 不去翻未加载布局的存档。
size_t CountLoadedPanelsUsingTemplate(const std::string& name) {
    size_t count = 0;
    auto& context = WebViewContext::GetInstance();
    for (HWND hwnd : context.GetAllInstances()) {
        if (!IsWindow(hwnd)) continue;
        WebViewPanel* panel = context.GetPanelByHwnd(hwnd);
        if (!panel) continue;
        if (prefs_template::EqualsIgnoreCase(panel->GetPanelConfig().templateName, name)) ++count;
    }
    return count;
}

// 起名对话框的输入与结果。DialogBoxIndirectParamW 的 lParam 把它带进 WM_INITDIALOG，
// 之后挂在 DWLP_USER 上，确定时把通过校验的名字写回 accepted。
struct TemplateNamePrompt {
    const wchar_t* title = L"";
    const wchar_t* actionLabel = L"";
    std::wstring label;                 // 编辑框上方的一行说明
    std::wstring hint;                  // 编辑框下方的规则提示；校验失败时换成原因
    std::wstring value;                 // 初始文本
    std::vector<std::string> existing;  // 现有模板名，用于重名判定
    std::string accepted;               // 输出：通过校验的名字（UTF-8）
};

constexpr int IDC_PROMPT_LABEL = 1000;
constexpr int IDC_PROMPT_EDIT = 1001;
constexpr int IDC_PROMPT_HINT = 1002;

const wchar_t* TemplateNameErrorText(prefs_template::NameError error) {
    switch (error) {
    case prefs_template::NameError::Empty:
        return TR("Enter a name.", "请输入名称。");
    case prefs_template::NameError::InvalidChars:
        return TR("Only letters, numbers, hyphens and underscores are allowed.",
                  "只能使用字母、数字、连字符和下划线。");
    case prefs_template::NameError::Duplicate:
        return TR("A template with this name already exists (names are not case-sensitive).",
                  "已存在同名模板（名称不区分大小写）。");
    case prefs_template::NameError::None:
        break;
    }
    return L"";
}

void InitTemplateNameDialog(HWND hDlg, const TemplateNamePrompt& prompt) {
    SetWindowTextW(hDlg, prompt.title);
    SetDlgItemTextW(hDlg, IDC_PROMPT_LABEL, prompt.label.c_str());
    SetDlgItemTextW(hDlg, IDC_PROMPT_HINT, prompt.hint.c_str());
    // 按钮文案在此设置：DLGITEMTEMPLATE 的 titleArray 是定长内联数组，
    // 放不下更长的译文，故模板里只留 ASCII 初始文本。
    SetDlgItemTextW(hDlg, IDOK, prompt.actionLabel);
    SetDlgItemTextW(hDlg, IDCANCEL, TR("Cancel", "取消"));
    HWND hEdit = GetDlgItem(hDlg, IDC_PROMPT_EDIT);
    if (hEdit) {
        SetWindowTextW(hEdit, prompt.value.c_str());
        SendMessageW(hEdit, EM_SETSEL, 0, -1);
        SetFocus(hEdit);
    }
}

// 操作按钮：校验通过才关对话框；不通过就把原因写进提示行、焦点留在编辑框，不落盘。
void AcceptTemplateNameIfValid(HWND hDlg, TemplateNamePrompt& prompt) {
    HWND hEdit = GetDlgItem(hDlg, IDC_PROMPT_EDIT);
    const int length = hEdit ? GetWindowTextLengthW(hEdit) : 0;
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    if (hEdit) GetWindowTextW(hEdit, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    
    const std::string candidate =
        prefs_template::Trim(pfc::stringcvt::string_utf8_from_wide(text.c_str()).get_ptr());
    const prefs_template::NameError error = prefs_template::CheckName(candidate, prompt.existing);
    if (error != prefs_template::NameError::None) {
        SetDlgItemTextW(hDlg, IDC_PROMPT_HINT, TemplateNameErrorText(error));
        if (hEdit) {
            SendMessageW(hEdit, EM_SETSEL, 0, -1);
            SetFocus(hEdit);
        }
        return;
    }
    prompt.accepted = candidate;
    EndDialog(hDlg, IDOK);
}

INT_PTR CALLBACK TemplateNameDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* prompt = reinterpret_cast<TemplateNamePrompt*>(GetWindowLongPtrW(hDlg, DWLP_USER));
    switch (msg) {
    case WM_INITDIALOG:
        prompt = reinterpret_cast<TemplateNamePrompt*>(lParam);
        SetWindowLongPtrW(hDlg, DWLP_USER, static_cast<LONG_PTR>(lParam));
        InitTemplateNameDialog(hDlg, *prompt);
        return FALSE;  // 焦点已在 InitTemplateNameDialog 里交给编辑框
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK && prompt) {
            AcceptTemplateNameIfValid(hDlg, *prompt);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

// Win32 没有现成的“带输入框的消息框”：TaskDialog 不支持文本输入，
// 因此在内存中构建最小 DLGTEMPLATE。布局（对话框单位）：
//   [说明标签] / [编辑框] / [规则提示或错误原因] / [操作] [取消]
// DS_SETFONT 指定 MS Shell Dlg，系统会映射到当前 UI 字体，各控件尺寸随之按 DPI 换算。
struct alignas(DWORD) TemplateNameDialogTemplate {
    DLGTEMPLATE tmpl;
    WORD menuArray[1];      // 无菜单
    WORD classArray[1];     // 默认类
    WORD titleArray[1];     // 空标题，WM_INITDIALOG 再设
    WORD pointSize;
    WCHAR typeface[13];     // "MS Shell Dlg" + nul
    struct alignas(DWORD) {
        DLGITEMTEMPLATE item;
        WORD classArray[2]; // 0xFFFF, 0x0082 = STATIC
        WORD titleArray[1];
        WORD extraBytes;
    } label;
    struct alignas(DWORD) {
        DLGITEMTEMPLATE item;
        WORD classArray[2]; // 0xFFFF, 0x0081 = EDIT
        WORD titleArray[1];
        WORD extraBytes;
    } edit;
    struct alignas(DWORD) {
        DLGITEMTEMPLATE item;
        WORD classArray[2]; // 0xFFFF, 0x0082 = STATIC
        WORD titleArray[1];
        WORD extraBytes;
    } hint;
    struct alignas(DWORD) {
        DLGITEMTEMPLATE item;
        WORD classArray[2]; // 0xFFFF, 0x0080 = BUTTON
        WCHAR titleArray[3]; // "OK" + nul
        WORD extraBytes;
    } ok;
    struct alignas(DWORD) {
        DLGITEMTEMPLATE item;
        WORD classArray[2];
        WCHAR titleArray[7]; // "Cancel" + nul
        WORD extraBytes;
    } cancel;
};

TemplateNameDialogTemplate BuildTemplateNameDialogTemplate() {
    TemplateNameDialogTemplate dlg = {};
    dlg.tmpl.style = DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlg.tmpl.cdit = 5;
    dlg.tmpl.cx = 260; dlg.tmpl.cy = 96;
    dlg.pointSize = 8;
    wcscpy_s(dlg.typeface, L"MS Shell Dlg");
    
    dlg.label.item = {WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 7, 7, 246, 10, IDC_PROMPT_LABEL};
    dlg.label.classArray[0] = 0xFFFF; dlg.label.classArray[1] = 0x0082;
    
    dlg.edit.item = {WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 0, 7, 20, 246, 14,
                     IDC_PROMPT_EDIT};
    dlg.edit.classArray[0] = 0xFFFF; dlg.edit.classArray[1] = 0x0081;
    
    // 提示行给三行高度：错误原因与“未加载布局不迁移”的说明在英文下也放得下。
    dlg.hint.item = {WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, 7, 38, 246, 30, IDC_PROMPT_HINT};
    dlg.hint.classArray[0] = 0xFFFF; dlg.hint.classArray[1] = 0x0082;
    
    dlg.ok.item = {WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP, 0, 149, 75, 50, 14, IDOK};
    dlg.ok.classArray[0] = 0xFFFF; dlg.ok.classArray[1] = 0x0080;
    wcscpy_s(dlg.ok.titleArray, L"OK");
    
    dlg.cancel.item = {WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 0, 203, 75, 50, 14, IDCANCEL};
    dlg.cancel.classArray[0] = 0xFFFF; dlg.cancel.classArray[1] = 0x0080;
    wcscpy_s(dlg.cancel.titleArray, L"Cancel");
    return dlg;
}

// Create 与 Rename 共用的起名对话框。返回 true 且 prompt.accepted 为通过校验的名字；
// 取消或关闭返回 false。模态期间会重入消息循环，调用方要自己持有实例引用。
bool PromptTemplateName(HWND owner, TemplateNamePrompt& prompt) {
    TemplateNameDialogTemplate dlg = BuildTemplateNameDialogTemplate();
    const INT_PTR result = DialogBoxIndirectParamW(nullptr, &dlg.tmpl, owner, TemplateNameDlgProc,
                                                   reinterpret_cast<LPARAM>(&prompt));
    return result == IDOK && !prompt.accepted.empty();
}

std::wstring Utf8ToWide(const std::string& text) {
    return pfc::stringcvt::string_wide_from_utf8(text.c_str()).get_ptr();
}

}  // namespace

void WebViewPreferencesInstance::OnTemplateSelectionChanged(HWND hwnd) {
    // 选中项只进草稿；持久化与导航都等 Apply。
    const std::string newTemplate = SelectedTemplateName(hwnd);
    if (newTemplate.empty() || newTemplate == draft().GetString(overview::Template)) return;
    draft().SetString(overview::Template, newTemplate);
    RefreshTemplateFolderText();
    UpdateState();
}

void WebViewPreferencesInstance::OnCreateTemplate(HWND hwnd) {
    // 模态期间宿主可能关掉本页并释放实例：先持有自引用，返回后再看窗口是否还在。
    service_ptr_t<WebViewPreferencesInstance> keepAlive(this);
    
    TemplateNamePrompt prompt;
    prompt.title = TR("Create Template", "新建模板");
    prompt.actionLabel = TR("Create", "创建");
    prompt.label = TR("Name for the new template:", "新模板的名称：");
    prompt.hint = TR("Letters, numbers, hyphens and underscores only. A folder with an index.html is created "
                     "under webview-ui.",
                     "只能使用字母、数字、连字符和下划线。将在 webview-ui 下创建带 index.html 的文件夹。");
    prompt.existing = GetTemplateList();
    if (!PromptTemplateName(hwnd, prompt) || !get_wnd()) return;
    
    const std::string& name = prompt.accepted;
    if (CreateTemplate(name)) {
        // 新模板只选入草稿：不激活、不导航，Apply 才写配置。
        draft().SetString(overview::Template, name);
        RefreshTemplateList(hwnd);
        UpdateState();
        console::printf("[WebView2 UI] Created template: %s", name.c_str());
        return;
    }
    
    // 失败：如实说明磁盘上留下了什么，不假装回滚；列表按磁盘现状重新枚举。
    std::wstring message = TR("Failed to create the template \"", "创建模板 \"");
    message += Utf8ToWide(name);
    message += TR("\".", "\" 失败。");
    if (TemplateExists(name)) {
        message += TR("\n\nThe folder was created but index.html could not be written. "
                      "The folder is left in place:\n",
                      "\n\n文件夹已创建，但 index.html 未能写入。文件夹保留在：\n");
        message += GetWebResourcesBaseDir() + L"\\" + Utf8ToWide(name);
    } else {
        message += TR("\n\nThe folder could not be created. Check the permissions of the webview-ui folder.",
                      "\n\n无法创建文件夹。请检查 webview-ui 文件夹的权限。");
    }
    RefreshTemplateList(hwnd);
    MessageBoxW(hwnd, message.c_str(), L"WebView2 UI", MB_OK | MB_ICONERROR);
}

void WebViewPreferencesInstance::OnRenameTemplate(HWND hwnd) {
    service_ptr_t<WebViewPreferencesInstance> keepAlive(this);
    
    const std::string oldName = SelectedTemplateName(hwnd);
    if (oldName.empty()) return;
    if (!TemplateExists(oldName)) {
        // 选中的是“（不存在）”标注项，或目录在本页打开后被外部删掉了。
        RefreshTemplateList(hwnd);
        MessageBoxW(hwnd, TR("This template folder does not exist, so it cannot be renamed.",
                             "该模板文件夹不存在，无法重命名。"),
            L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // 已提交的活动模板不能改名：配置里还指着旧名字，改了就等于把当前主界面指向一个不存在的目录。
    // 同时对照当前配置，本页打开期间被别处切换的活动模板也算。
    if (oldName == initial().GetString(overview::Template) ||
        oldName == ReadSnapshotFromConfig().GetString(overview::Template)) {
        MessageBoxW(hwnd,
            TR("This template is the active one. Switch to another template and apply first, then rename it.",
               "这是当前活动模板。请先切换到其他模板并应用，再重命名。"),
            L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // 已加载面板显式引用的模板也不能改名：面板配置里存的是名字，改了它就找不到目录。
    if (const size_t panels = CountLoadedPanelsUsingTemplate(oldName); panels > 0) {
        std::wstring message = std::to_wstring(panels);
        message += TR(" loaded panel(s) use this template explicitly. Change their template in the panel "
                      "settings first, then rename it.",
                      " 个已加载的面板显式使用此模板。请先在面板设置里改用其他模板，再重命名。");
        MessageBoxW(hwnd, message.c_str(), L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    TemplateNamePrompt prompt;
    prompt.title = TR("Rename Template", "重命名模板");
    prompt.actionLabel = TR("Rename", "重命名");
    // 前后缀成对翻译：英文用 "name": 收尾，中文用括号包裹，避免任一语言出现括号不配对。
    prompt.label = TR("New name for \"", "新名称 (\"");
    prompt.label += Utf8ToWide(oldName);
    prompt.label += TR("\":", "\"):");
    prompt.hint = TR("Renames the folder immediately; Cancel in Preferences does not undo it. "
                     "Panels in layouts that are not loaded keep the old name.",
                     "文件夹会立即重命名，首选项页的取消不会撤销此操作。未加载布局中的面板仍指向旧名称，不会被更新。");
    prompt.value = Utf8ToWide(oldName);
    prompt.existing = GetTemplateList();  // 含旧名本身：改回旧名或只改大小写都按重名拒绝
    if (!PromptTemplateName(hwnd, prompt) || !get_wnd()) return;
    
    const std::string& newName = prompt.accepted;
    if (!RenameTemplate(oldName, newName)) {
        // 对话框已排除非法名与重名，走到这里多半是目录被占用或权限不足；列表按磁盘现状重枚举。
        RefreshTemplateList(hwnd);
        std::wstring message = TR("Failed to rename the template folder \"", "重命名模板文件夹 \"");
        message += Utf8ToWide(oldName);
        message += TR("\".\n\nThe folder may be in use or you may not have permission. "
                      "Close programs that might be accessing the template files and try again.",
                      "\" 失败。\n\n文件夹可能正在被使用或没有权限。请关闭可能正在访问模板文件的程序后重试。");
        MessageBoxW(hwnd, message.c_str(), TR("Rename Failed", "重命名失败"), MB_OK | MB_ICONERROR);
        return;
    }
    
    // 草稿里若选的是旧名，跟着改成新名；其余字段的未提交编辑不变。
    if (draft().GetString(overview::Template) == oldName) draft().SetString(overview::Template, newName);
    RefreshTemplateList(hwnd);
    UpdateState();
    console::printf("[WebView2 UI] Renamed template: %s -> %s", oldName.c_str(), newName.c_str());
}

void WebViewPreferencesInstance::OnDeleteTemplate(HWND hwnd) {
    service_ptr_t<WebViewPreferencesInstance> keepAlive(this);
    
    const std::string templateName = SelectedTemplateName(hwnd);
    if (templateName.empty()) return;
    if (!TemplateExists(templateName)) {
        RefreshTemplateList(hwnd);
        MessageBoxW(hwnd, TR("This template folder does not exist; there is nothing to delete.",
                             "该模板文件夹不存在，没有可删除的内容。"),
            L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // 列表可以删空：页面会显示“default（不存在）”并拒绝 Apply，不为此保留最后一个。
    // 已提交的活动模板不能删：用户须先切换并应用，不能靠“先改配置再删除”绕过保护。
    // 同时对照当前配置，本页打开期间被别处切换的活动模板也算。
    if (templateName == initial().GetString(overview::Template) ||
        templateName == ReadSnapshotFromConfig().GetString(overview::Template)) {
        MessageBoxW(hwnd,
            TR("This template is the active one. Switch to another template and apply first, then delete it.",
               "这是当前活动模板。请先切换到其他模板并应用，再删除。"),
            L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // 已加载面板显式引用的模板不能删；未加载布局里的引用查不到，只在确认文案里说明。
    if (const size_t panels = CountLoadedPanelsUsingTemplate(templateName); panels > 0) {
        std::wstring message = std::to_wstring(panels);
        message += TR(" loaded panel(s) use this template explicitly. Change their template in the panel "
                      "settings first, then delete it.",
                      " 个已加载的面板显式使用此模板。请先在面板设置里改用其他模板，再删除。");
        MessageBoxW(hwnd, message.c_str(), L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // 删除确认默认选中取消；外层首选项页的 Cancel 无法撤销已经执行的文件操作。
    std::wstring question = TR("Delete template \"", "删除模板 \"");
    question += Utf8ToWide(templateName);
    question += TR("\"?", "\"？");
    const TASKDIALOG_BUTTON buttons[] = {
        {IDYES, TR("Delete", "删除")},
        {IDCANCEL, TR("Cancel", "取消")},
    };
    TASKDIALOGCONFIG confirmation{};
    confirmation.cbSize = sizeof(confirmation);
    confirmation.hwndParent = hwnd;
    confirmation.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    confirmation.pszWindowTitle = L"WebView2 UI";
    confirmation.pszMainIcon = TD_WARNING_ICON;
    confirmation.pszMainInstruction = question.c_str();
    confirmation.pszContent = TR(
        "The template folder and its contents will be deleted immediately. Cancel in Preferences cannot undo this.\n\n"
        "Panels in layouts that are not loaded may still reference this template. When reopened, they may load "
        "another available template or show a missing-template message.",
        "模板文件夹及其内容会立即删除，首选项页的取消无法撤销此操作。\n\n"
        "未加载布局中的面板可能仍引用此模板，重开时可能显示其他可用模板或模板缺失提示。");
    confirmation.cButtons = static_cast<UINT>(std::size(buttons));
    confirmation.pButtons = buttons;
    confirmation.nDefaultButton = IDCANCEL;
    int selectedButton = IDCANCEL;
    const HRESULT result = TaskDialogIndirect(&confirmation, &selectedButton, nullptr, nullptr);
    if (FAILED(result) || selectedButton != IDYES || !get_wnd())
        return;
    
    if (DeleteTemplate(templateName)) {
        // 删的是草稿选择项时，草稿回到已提交的活动模板；其他字段的未提交编辑不变。
        if (draft().GetString(overview::Template) == templateName) {
            draft().SetString(overview::Template, initial().GetString(overview::Template));
        }
        RefreshTemplateList(hwnd);
        UpdateState();
        console::printf("[WebView2 UI] Deleted template: %s", templateName.c_str());
        return;
    }
    
    // 删除失败：remove_all 可能只删了一部分，按磁盘现状重枚举并报告实际状态。
    RefreshTemplateList(hwnd);
    if (!TemplateExists(templateName)) {
        MessageBoxW(hwnd, TR("The template folder is already gone; the list has been refreshed.",
                             "模板文件夹已不存在，列表已刷新。"),
            L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring message = TR("Failed to delete the template \"", "删除模板 \"");
    message += Utf8ToWide(templateName);
    message += TR("\".\n\nSome files may have been removed already; the folder still exists at:\n",
                  "\" 失败。\n\n部分文件可能已被删除；文件夹仍在：\n");
    message += GetWebResourcesBaseDir() + L"\\" + Utf8ToWide(templateName);
    message += TR("\n\nThe folder may be in use or you may not have permission. "
                  "Close programs that might be accessing the template files and try again.",
                  "\n\n文件夹可能正在被使用或没有权限。请关闭可能正在访问模板文件的程序后重试。");
    MessageBoxW(hwnd, message.c_str(), L"WebView2 UI", MB_OK | MB_ICONERROR);
}

void WebViewPreferencesInstance::OnOpenTemplateFolder(HWND hwnd) const {
    std::wstring path = GetWebResourcesBaseDir();
    
    const std::string& templateName = draft().GetString(overview::Template);
    if (!templateName.empty()) {
        path += L"\\";
        path += pfc::stringcvt::string_wide_from_utf8(templateName.c_str()).get_ptr();
    }
    
    // 只打开存在的目录，不替用户建；按钮在目录缺失时已禁用，这里是兜底(fallback)。
    std::error_code ec;
    if (!fs::is_directory(path, ec)) {
        MessageBoxW(hwnd, TR("The template folder does not exist.", "模板文件夹不存在。"),
            L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    static auto pShellExec = &::ShellExecuteW;
    if (pShellExec)
        pShellExec(nullptr, L"explore", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void WebViewPreferencesInstance::OnManageTemplates(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    // 选中的是“（不存在）”标注项或列表为空时，Rename / Delete 没有对象，置灰而不是点开再报错。
    const std::string selected = SelectedTemplateName(hwnd);
    const UINT targetFlags = (!selected.empty() && TemplateExists(selected)) ? MF_STRING : (MF_STRING | MF_GRAYED);
    AppendMenuW(menu, MF_STRING, IDM_TEMPLATE_CREATE, TR("Create...", "新建..."));
    AppendMenuW(menu, targetFlags, IDM_TEMPLATE_RENAME, TR("Rename...", "重命名..."));
    AppendMenuW(menu, targetFlags, IDM_TEMPLATE_DELETE, TR("Delete...", "删除..."));
    
    RECT rc{};
    HWND hButton = GetDlgItem(hwnd, IDC_BTN_MANAGE);
    if (hButton) GetWindowRect(hButton, &rc);
    
    // 弹出菜单会重入消息循环：期间宿主可能关掉本页并释放实例，
    // 先持有一份自引用，返回后再看窗口是否还在。
    service_ptr_t<WebViewPreferencesInstance> keepAlive(this);
    const UINT cmd = static_cast<UINT>(TrackPopupMenuEx(menu,
        TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
        rc.left, rc.bottom, hwnd, nullptr));
    DestroyMenu(menu);
    if (!cmd || !get_wnd()) return;
    SendMessageW(get_wnd(), WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
}

void WebViewPreferencesInstance::OnOpenAdvancedPreferences() {
    // 目标是宿主的 Advanced 页，不是本组件的 advconfig 分支 GUID；
    // 宿主不保证自动展开分支，控件旁的说明给出 Tools > WebView2 UI 位置。
    try {
        static_api_ptr_t<ui_control>()->show_preferences(preferences_page::guid_advanced);
    } catch (...) {
        console::printf("[WebView2 UI] Failed to open Advanced Preferences");
    }
}

} // namespace webview_prefs
