#include "stdafx.h"
#include "win32_utility.h"
#include "win32_op.h"
#include <list>

#pragma warning(disable: 4996) // silence GetVersionEx() deprecation

SIZE QueryContextDPI(HDC dc) {
	return {GetDeviceCaps(dc,LOGPIXELSX), GetDeviceCaps(dc,LOGPIXELSY)};
}
unsigned QueryScreenDPI(HWND wnd) {
	HDC dc = GetDC(wnd);
	unsigned ret = GetDeviceCaps(dc, LOGPIXELSY);
	ReleaseDC(wnd, dc);
	return ret;
}
unsigned QueryScreenDPI_X(HWND wnd) {
	HDC dc = GetDC(wnd);
	unsigned ret = GetDeviceCaps(dc, LOGPIXELSX);
	ReleaseDC(wnd, dc);
	return ret;
}
unsigned QueryScreenDPI_Y(HWND wnd) {
	HDC dc = GetDC(wnd);
	unsigned ret = GetDeviceCaps(dc, LOGPIXELSY);
	ReleaseDC(wnd, dc);
	return ret;
}

SIZE QueryScreenDPIEx(HWND wnd) {
	HDC dc = GetDC(wnd);
	SIZE ret = { GetDeviceCaps(dc,LOGPIXELSX), GetDeviceCaps(dc,LOGPIXELSY) };
	ReleaseDC(wnd, dc);
	return ret;
}

void HeaderControl_SetSortIndicator(HWND header_, int column, bool isUp) {
	CHeaderCtrl header(header_);
	const int total = header.GetItemCount();
	for (int walk = 0; walk < total; ++walk) {
		HDITEM item = {}; item.mask = HDI_FORMAT;
		if (header.GetItem(walk, &item)) {
			auto newFormat = item.fmt;
			newFormat &= ~(HDF_SORTUP | HDF_SORTDOWN);
			if (walk == column) {
				newFormat |= isUp ? HDF_SORTUP : HDF_SORTDOWN;
			}
			if (newFormat != item.fmt) {
				item.fmt = newFormat;
				header.SetItem(walk, &item);
			}
		}
	}
}

HINSTANCE GetThisModuleHandle() {
	return (HINSTANCE)_AtlBaseModule.m_hInst;
}

WinResourceRef_t WinLoadResource(HMODULE hMod, const TCHAR * name, const TCHAR * type, WORD wLang) {
	SetLastError(0);
	HRSRC res = wLang ? FindResourceEx(hMod, type, name, wLang) : FindResource(hMod, name, type);
	if ( res == NULL ) WIN32_OP_FAIL();
	SetLastError(0);
	HGLOBAL hglob = LoadResource(hMod, res);
	if ( hglob == NULL ) WIN32_OP_FAIL();
	SetLastError(0);
	void * ptr = LockResource(hglob);
	if ( ptr == nullptr ) WIN32_OP_FAIL();
	WinResourceRef_t ref;
	ref.ptr = ptr;
	ref.bytes = SizeofResource(hMod, res);
	return ref;
}

CComPtr<IStream> WinLoadResourceAsStream(HMODULE hMod, const TCHAR * name, const TCHAR * type, WORD wLang) {
	auto res = WinLoadResource(hMod, name, type, wLang );
	auto str = SHCreateMemStream( (const BYTE*) res.ptr, (UINT) res.bytes );
	if ( str == nullptr ) throw std::bad_alloc();
	CComPtr<IStream> ret;
	ret.Attach( str );
	return ret;
}

UINT GetFontHeight(HFONT font)
{
	UINT ret;
	HDC dc = CreateCompatibleDC(0);
	SelectObject(dc, font);
	ret = GetTextHeight(dc);
	DeleteDC(dc);
	return ret;
}

UINT GetTextHeight(HDC dc)
{
	TEXTMETRIC tm;
	POINT pt[2];
	GetTextMetrics(dc, &tm);
	pt[0].x = 0;
	pt[0].y = tm.tmHeight;
	pt[1].x = 0;
	pt[1].y = 0;
	LPtoDP(dc, pt, 2);

	int ret = pt[0].y - pt[1].y;
	return ret > 1 ? (unsigned)ret : 1;
}


LRESULT RelayEraseBkgnd(HWND p_from, HWND p_to, HDC p_dc) {

	CDCHandle dc(p_dc);
	DCStateScope scope(dc);
	CRect client;
	if (GetClientRect(p_from, client)) {
		dc.IntersectClipRect(client);
	}
	
	LRESULT status;
	POINT pt = { 0, 0 }, pt_old = { 0,0 };
	MapWindowPoints(p_from, p_to, &pt, 1);
	OffsetWindowOrgEx(p_dc, pt.x, pt.y, &pt_old);
	status = SendMessage(p_to, WM_ERASEBKGND, (WPARAM)p_dc, 0);
	SetWindowOrgEx(p_dc, pt_old.x, pt_old.y, 0);
	return status;
}

static LRESULT CALLBACK EraseHandlerProc(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData
) {
	(void)uIdSubclass;
	if (uMsg == WM_ERASEBKGND) {
		HWND wndTarget = reinterpret_cast<HWND>(dwRefData);
		PFC_ASSERT(wndTarget != NULL);
		return RelayEraseBkgnd(hWnd, wndTarget, (HDC)wParam);
	}
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK CtlColorProc(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData
) {
	(void)uIdSubclass; (void)dwRefData;
	switch (uMsg) {
	case WM_CTLCOLORMSGBOX:
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
	case WM_CTLCOLORBTN:
	case WM_CTLCOLORDLG:
	case WM_CTLCOLORSCROLLBAR:
	case WM_CTLCOLORSTATIC:
		return SendMessage(GetParent(hWnd), uMsg, wParam, lParam);
	default:
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}
}


void InjectEraseHandler(HWND wnd, HWND sendTo) {
	PFC_ASSERT(sendTo != NULL);
	WIN32_OP_D(SetWindowSubclass(wnd, EraseHandlerProc, 0, reinterpret_cast<DWORD_PTR>(sendTo)));
}
void InjectParentEraseHandler(HWND wnd) {
	InjectEraseHandler(wnd, GetParent(wnd));
}
void InjectParentCtlColorHandler(HWND wnd) {
	WIN32_OP_D(SetWindowSubclass(wnd, CtlColorProc, 0, 0));
}
static LRESULT CALLBACK BounceNextDlgCtlProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	(void)uIdSubclass;
	if (uMsg == WM_NEXTDLGCTL) {
		return ::SendMessage((HWND)dwRefData, uMsg, wParam, lParam);
	}
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void BounceNextDlgCtl(HWND wnd, HWND wndTo) {
	::SetWindowSubclass(wnd, BounceNextDlgCtlProc, 0, (DWORD_PTR)wndTo);
}


pfc::string8 EscapeTooltipText(const char * src)
{
	pfc::string8 out;
	while (*src)
	{
		if (*src == '&')
		{
			out.add_string("&&&");
			src++;
			while (*src == '&')
			{
				out.add_string("&&");
				src++;
			}
		} else out.add_byte(*(src++));
	}
	return out;
}

bool IsMenuNonEmpty(HMENU menu) {
	unsigned n, m = GetMenuItemCount(menu);
	for (n = 0; n < m; n++) {
		if (GetSubMenu(menu, n)) return true;
		if (!(GetMenuState(menu, n, MF_BYPOSITION)&MF_SEPARATOR)) return true;
	}
	return false;
}

void SetDefaultMenuItem(HMENU p_menu, unsigned p_id) {
	MENUITEMINFO info = { sizeof(info) };
	info.fMask = MIIM_STATE;
	GetMenuItemInfo(p_menu, p_id, FALSE, &info);
	info.fState |= MFS_DEFAULT;
	SetMenuItemInfo(p_menu, p_id, FALSE, &info);
}

static bool FetchWineInfoAppend(pfc::string_base & out) {
	const HMODULE ntdll = GetModuleHandle(_T("ntdll.dll"));
	if (ntdll == NULL) return false;
	const auto wine_get_build_id = (const char* (__cdecl *)(void))GetProcAddress(ntdll, "wine_get_build_id");
	const auto wine_get_host_version = (void(__cdecl *)(const char**, const char**))GetProcAddress(ntdll, "wine_get_host_version");
	if (wine_get_build_id == NULL || wine_get_host_version == NULL) {
		if (GetProcAddress(ntdll, "wine_server_call") != NULL) {
			out << "wine (unknown version)";
			return true;
		}
		return false;
	}
	const char * sysname = NULL; const char * release = NULL;
	wine_get_host_version(&sysname, &release);
	out << wine_get_build_id() << ", on: " << sysname << " / " << release;
	return true;
}
static const char* nativeMachineType(const SYSTEM_INFO& info) {
	using fnIsWow64Process2 = BOOL(WINAPI*)(HANDLE hProcess, USHORT* pProcessMachine, USHORT* pNativeMachine);
	auto proc = (fnIsWow64Process2)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "IsWow64Process2");
	if (proc) {
		USHORT processMachine = 0, nativeMachine = 0;
		if (proc(GetCurrentProcess(), &processMachine, &nativeMachine)) {
			switch (nativeMachine) {
			case IMAGE_FILE_MACHINE_IA64:
				return "IA64";
			case IMAGE_FILE_MACHINE_AMD64:
				return "x64";
			case IMAGE_FILE_MACHINE_ARM64:
				return "ARM64";
			}
		}
	}
	switch (info.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_AMD64:
		return "x64";
	case PROCESSOR_ARCHITECTURE_IA64:
		return "IA64";
	case PROCESSOR_ARCHITECTURE_INTEL:
		return "x86";
	case PROCESSOR_ARCHITECTURE_ARM64:
		return "ARM64";
	default:
		return nullptr;
	}

}
static void GetOSVersionStringAppend(pfc::string_base & out) {

	if (FetchWineInfoAppend(out)) return;

	OSVERSIONINFO ver = {}; ver.dwOSVersionInfoSize = sizeof(ver);
	WIN32_OP_D(GetVersionEx(&ver));
	SYSTEM_INFO info = {};
	GetNativeSystemInfo(&info);

	out << "Windows " << (int)ver.dwMajorVersion << "." << (int)ver.dwMinorVersion << "." << (int)ver.dwBuildNumber;
	if (ver.szCSDVersion[0] != 0) out << " " << pfc::stringcvt::string_utf8_from_os(ver.szCSDVersion, PFC_TABSIZE(ver.szCSDVersion));

	auto t = nativeMachineType(info);
	if (t) out << " " << t;
}

void GetOSVersionString(pfc::string_base & out) {
	out.reset(); GetOSVersionStringAppend(out);
}
WORD GetOSVersionCode() {
	OSVERSIONINFO ver = {sizeof(ver)};
	WIN32_OP_D(GetVersionEx(&ver));
	
	DWORD ret = pfc::min_t<DWORD>(ver.dwMinorVersion, 0xFF);
	ret += pfc::min_t<DWORD>(ver.dwMajorVersion, 0xFF) << 8;

	return (WORD)ret;
}

static const char* WineVersionStr_() {
	const HMODULE ntdll = GetModuleHandle(_T("ntdll.dll"));
	if (ntdll) {
		const auto wine_get_build_id = (const char* (__cdecl*)(void))GetProcAddress(ntdll, "wine_get_version");
		if (wine_get_build_id) return wine_get_build_id();
	}
	return nullptr;
}

const char* WineVersionStr() {
	static auto ret = WineVersionStr_();
	return ret;
}

unsigned WineMajorVersion() {
	auto str = WineVersionStr();
	if (str) {
		int i = atoi(str);
		if (i > 0) return (unsigned)i;
	}
	return 0;
}

static uint32_t GetWineVersionCode_() {
	auto str = WineVersionStr();
	if (str == nullptr) return UINT32_MAX; // not Wine
	auto major = atoi(str);
	if (major > 0) {
		int ret = major * 100;
		auto dot = strchr(str, '.');
		if (dot) {
			auto minor = atoi(dot + 1);
			if (minor > 0) {
				if (minor > 99) minor = 99;
				ret += minor;
			}
		}
		return ret;
	}
	return 0; // Wine but with bad version info
}

uint32_t GetWineVersionCode() {
	static auto ret = GetWineVersionCode_();
	return ret;
}

bool IsWine() {
	static bool ret = [] {	
		HMODULE module = GetModuleHandle(_T("ntdll.dll"));
		if (!module) return false;
		return GetProcAddress(module, "wine_server_call") != NULL;
	} ();
	return ret;
}

static BOOL CALLBACK EnumChildWindowsProc(HWND w, LPARAM p) {
	auto f = reinterpret_cast<std::function<void(HWND)>*>(p);
	(*f)(w);
	return TRUE;
}
void EnumChildWindows(HWND w, std::function<void(HWND)> f) {
	::EnumChildWindows(w, EnumChildWindowsProc, reinterpret_cast<LPARAM>(&f));
}
void EnumChildWindowsHere(HWND parent, std::function<void(HWND)> f) {
	for (HWND walk = GetWindow(parent, GW_CHILD); walk != NULL; walk = GetWindow(walk, GW_HWNDNEXT)) {
		f(walk);
	}
}
static DWORD Win10BuildNumber_() {
	OSVERSIONINFO ver = { sizeof(ver) };
	WIN32_OP_D(GetVersionEx(&ver));
	if ( ver.dwMajorVersion < 10 ) return 0;
	else if ( ver.dwMajorVersion == 10 ) return ver.dwBuildNumber;
	else return 0xFFFFFFFF;
}
DWORD Win10BuildNumber() {
	static DWORD b = Win10BuildNumber_();
	return b;
}
bool IsWindows11OrNewer() {
	return Win10BuildNumber() >= 22000;
}


#include "hookWindowMessages.h"
#include <algorithm>

namespace {

	class CWindowHook_Map : public CWindowImpl<CWindowHook_Map, CWindow> {
	public:
		CMessageMap* m_target = nullptr;
		DWORD m_targetID = 0;

		std::vector< DWORD > m_messages;

		void setup(std::initializer_list<DWORD>&& arg) {
			m_messages = std::move(arg);
			std::sort(m_messages.begin(), m_messages.end());
		}

		bool isMsgWanted(DWORD msg) const {
			return std::binary_search(m_messages.begin(), m_messages.end(), msg);
		}
		BEGIN_MSG_MAP(CWindowHook)
			if (isMsgWanted(uMsg)) {
				CHAIN_MSG_MAP_ALT_MEMBER((*m_target), m_targetID);
			}
		END_MSG_MAP()
	};

	class CWindowHook_Proc : public CWindowImpl<CWindowHook_Proc, CWindow> {
	public:
		CWindowHook_Proc(PP::messageHook_t proc) : m_proc(proc) {}
		const PP::messageHook_t m_proc;

		BEGIN_MSG_MAP(CWindowHook)
			if (m_proc(hWnd, uMsg, wParam, lParam, lResult)) return TRUE;
		END_MSG_MAP()
	};

}

void PP::hookWindowMessages(HWND wnd, CMessageMap* target, DWORD targetID, std::initializer_list<DWORD>&& msgs) {
	auto obj = PP::subclassThisWindow< CWindowHook_Map >(wnd);
	obj->m_target = target; obj->m_targetID = targetID;
	obj->setup(std::move(msgs));
}
void PP::hookWindowMessages(HWND wnd, messageHook_t h) {
	PP::subclassThisWindow< CWindowHook_Proc >(wnd, h);
}

namespace PP {
	static LONG regReadHelper(HKEY root, const wchar_t* path, const wchar_t* value, LONG def) {
		wchar_t buf[64] = {};
		DWORD cb = (DWORD)((std::size(buf) - 1) * sizeof(buf[0]));
		if (RegGetValue(root, path, value, RRF_RT_REG_SZ, NULL, buf, &cb) == 0) {
			return _wtol(buf);
		}
		return def;
	}

	static SIZE querySystemDragThreshold() {
		constexpr DWORD def = 1;
		static constexpr wchar_t path[] = L"Control Panel\\Desktop";
		return {
			(LONG)regReadHelper(HKEY_CURRENT_USER, path, L"DragWidth", def),
			(LONG)regReadHelper(HKEY_CURRENT_USER, path, L"DragHeight", def)
		};
	}
	SIZE queryDragThresholdForDPI(SIZE dpi) {
		PFC_ASSERT(dpi.cx > 0 && dpi.cy > 0);
		static SIZE sys = {};
		if ( sys.cx == 0 || sys.cy == 0 ) sys = querySystemDragThreshold();
		return { MulDiv(sys.cx, dpi.cx, 96), MulDiv(sys.cy, dpi.cy, 96) };
	}
	SIZE queryDragThreshold(HWND wndFor) {
		return queryDragThresholdForDPI(QueryScreenDPIEx(wndFor));
	}
}
