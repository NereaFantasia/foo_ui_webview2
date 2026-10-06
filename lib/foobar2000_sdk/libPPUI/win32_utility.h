#pragma once
#include <functional>
#include <atlcomcli.h> // CComPtr

unsigned QueryScreenDPI(HWND wnd = NULL);
unsigned QueryScreenDPI_X(HWND wnd = NULL);
unsigned QueryScreenDPI_Y(HWND wnd = NULL);

SIZE QueryScreenDPIEx(HWND wnd = NULL);
SIZE QueryContextDPI(HDC dc);

namespace PP {
	// Returns drag threshold, in actual pixels (DPI-corrected), for the specified window
	SIZE queryDragThreshold(HWND wndFor);
	SIZE queryDragThresholdForDPI(SIZE knownDPI);
}

void HeaderControl_SetSortIndicator(HWND header, int column, bool isUp);

HINSTANCE GetThisModuleHandle();

struct WinResourceRef_t {
	const void * ptr;
	size_t bytes;
} ;

WinResourceRef_t WinLoadResource(HMODULE hMod, const TCHAR * name, const TCHAR * type, WORD wLang = 0);
CComPtr<IStream> WinLoadResourceAsStream(HMODULE hMod, const TCHAR * name, const TCHAR * type, WORD wLang = 0);

UINT GetFontHeight(HFONT font);
UINT GetTextHeight(HDC dc);

LRESULT RelayEraseBkgnd(HWND p_from, HWND p_to, HDC p_dc);

pfc::string8 EscapeTooltipText(const char * text);

class CloseHandleScope {
public:
	CloseHandleScope(HANDLE handle) throw() : m_handle(handle) {}
	~CloseHandleScope() throw() { CloseHandle(m_handle); }
	HANDLE Detach() throw() { return pfc::replace_t(m_handle, INVALID_HANDLE_VALUE); }
	HANDLE Get() const throw() { return m_handle; }
	void Close() throw() { CloseHandle(Detach()); }
	PFC_CLASS_NOT_COPYABLE_EX(CloseHandleScope)
private:
	HANDLE m_handle;
};

bool IsMenuNonEmpty(HMENU menu);
void SetDefaultMenuItem(HMENU p_menu, unsigned p_id);

void GetOSVersionString(pfc::string_base & out);
WORD GetOSVersionCode(); // per WINVER macro semantics, 0x601 for Windows 7, 0xA00 for Windows 10
DWORD Win10BuildNumber(); // See https://en.wikipedia.org/wiki/Windows_10_version_history for build number reference
bool IsWindows11OrNewer();

void EnumChildWindows(HWND, std::function<void(HWND)>); // Recursive
void EnumChildWindowsHere(HWND, std::function<void(HWND)>); // Non-recursive

// Wine detection helpers
bool IsWine();
const char* WineVersionStr();
unsigned WineMajorVersion();

// GetWineVersionCode()
// 1100 for 11.0, 1103 for 11.3, UINT32_MAX if not Wine
// meant for easy detection of old Wine builds with known bugs:
// if (GetWineVersionCode() < 1103) { workaround bugs in versions prior to 11.3 here }
uint32_t GetWineVersionCode();
