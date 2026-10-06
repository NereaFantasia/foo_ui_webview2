#include "stdafx.h"
#include "DarkMode.h"
#include "DarkModeEx.h"
#include "win32_utility.h"
#include "win32_op.h"
#include "PaintUtils.h"
#include "ImplementOnFinalMessage.h"
#include <vsstyle.h>
#include "GDIUtils.h"
#include "CListControl.h"
#include "CListControl-Subst.h"
#include "ReStyleWnd.h"
#include <map>
#include <shared_mutex>
#include "DarkMode-Config.h"


#if DARKMODE_DEBUG
#define DARKMODE_DEBUG_PRINT(...) PFC_DEBUG_PRINT("DarkMode: ", __VA_ARGS__)
#else
#define DARKMODE_DEBUG_PRINT(...) PFC_NO_OP
#endif


#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

/*

==== DARK MODE KNOWLEDGE BASE ====

== Generic window ==
UpdateTitleBar() to set title bar to black

== Dialog box ==
Use WM_CTLCOLORDLG, background of 0x383838
UpdateTitleBar() to set title bar to black

== Edit box ==
Use WM_CTLCOLOREDIT, background of 0x383838
ApplyDarkThemeCtrl() with "Explorer"

== Drop list combo ==
Method #1: ::SetWindowTheme(wnd, L"DarkMode_CFD", nullptr);
Method #2: ::SetWindowTheme(wnd, L"", L""); to obey WM_CTLCOLOR* but leaves oldstyle classic-colors button and breaks comboboxex
Must explicitly darken listbox to get dark scrollbars in it, see list box

== Button ==
Use WM_CTLCOLORBTN, background of 0x383838
ApplyDarkThemeCtrl() with "Explorer"

== Scroll bar ==
ApplyDarkThemeCtrl() with "Explorer"
^^ on either scrollbar or the window that implicitly creates scrollbars

== Header ==
ApplyDarkThemeCtrl() with "ItemsView"
Handle custom draw, override text color

== Tree view ==
ApplyDarkThemeCtrl() with "Explorer"
Set text/bk colors explicitly

Text color: 0xdedede
Background: 0x191919

Label-editing:
Pass WM_CTLCOLOR* to parent, shim TVM_EDITLABEL to pass theme to the editbox (not really necessary tho)


== Rebar ==
Can be beaten into working to some extent with a combination of:
* ::SetWindowTheme(toolbar, L"", L""); or else RB_SETTEXTCOLOR & REBARBANDINFO colors are disregarded
* Use RB_SETTEXTCOLOR / SetTextColor() + RB_SETBKCOLOR / SetBkColor() to override text/background colors
* Override WM_ERASEBKGND to draw band frames, RB_SETBKCOLOR doesn't seem to be thorough
* NM_CUSTOMDRAW on parent window to paint band labels & grippers without annoying glitches
NM_CUSTOMDRAW is buggy, doesn't hand you band indexes to query text, have to use hit tests to know what text to render

Solution: full custom draw


== Toolbar ==
Source: https://stackoverflow.com/questions/61271578/winapi-toolbar-text-color
Respects background color of its parent
Override text:
::SetWindowTheme(toolbar, L"", L""); or else NM_CUSTOMDRAW color is disregarded
NM_CUSTOMDRAW handler: 
switch (cd->nmcd.dwDrawStage) {
	case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW;
	case CDDS_ITEMPREPAINT: cd->clrText = DarkMode::GetSysColor(COLOR_WINDOWTEXT); return CDRF_DODEFAULT;
}

== Tab control ==
Full custom draw, see CTabsHook

== List View ==
Dark scrollbars are shown only if using "Explorer" theme, but other stuff needs "ItemsView" theme???
Other projects shim Windows functions to bypass the above.
Avoid using List View, use libPPUI CListControl instead.

== List Box ==
Use WM_CTLCOLOR* or DarkMode_Explorer

== Status Bar ==
Full custom draw

== Check box, radio button ==
SetWindowTheme(wnd, L"", L""); works but not 100% pretty, disabled text ugly in particular
Full custom draw preferred

== Group box ===
SetWindowTheme(wnd, L"", L""); works but not 100% pretty, disabled text ugly in particular
Full custom draw preferred (we don't do this).
Avoid disabling groupboxes / use something else.

==== NOTES ====
AllowDarkModeForWindow() needs SetPreferredAppMode() to take effect, hence we implicitly call it
AllowDarkModeForWindow() must be called BEFORE SetWindowTheme() to take effect

It seems it is interchangeable to do:
AllowDarkModeForWindow(wnd, true); SetWindowTheme(wnd, L"foo", nullptr);
vs
SetWindowTheme(wnd, L"DarkMode_foo", nullptr)
But the latter doesn't require undocumented function calls and doesn't infect all menus with dark mode
*/

namespace {
	enum class PreferredAppMode
	{
		Default,
		AllowDark,
		ForceDark,
		ForceLight,
		Max
	};

	enum WINDOWCOMPOSITIONATTRIB
	{
		WCA_UNDEFINED = 0,
		WCA_NCRENDERING_ENABLED = 1,
		WCA_NCRENDERING_POLICY = 2,
		WCA_TRANSITIONS_FORCEDISABLED = 3,
		WCA_ALLOW_NCPAINT = 4,
		WCA_CAPTION_BUTTON_BOUNDS = 5,
		WCA_NONCLIENT_RTL_LAYOUT = 6,
		WCA_FORCE_ICONIC_REPRESENTATION = 7,
		WCA_EXTENDED_FRAME_BOUNDS = 8,
		WCA_HAS_ICONIC_BITMAP = 9,
		WCA_THEME_ATTRIBUTES = 10,
		WCA_NCRENDERING_EXILED = 11,
		WCA_NCADORNMENTINFO = 12,
		WCA_EXCLUDED_FROM_LIVEPREVIEW = 13,
		WCA_VIDEO_OVERLAY_ACTIVE = 14,
		WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 15,
		WCA_DISALLOW_PEEK = 16,
		WCA_CLOAK = 17,
		WCA_CLOAKED = 18,
		WCA_ACCENT_POLICY = 19,
		WCA_FREEZE_REPRESENTATION = 20,
		WCA_EVER_UNCLOAKED = 21,
		WCA_VISUAL_OWNER = 22,
		WCA_HOLOGRAPHIC = 23,
		WCA_EXCLUDED_FROM_DDA = 24,
		WCA_PASSIVEUPDATEMODE = 25,
		WCA_USEDARKMODECOLORS = 26,
		WCA_LAST = 27
	};

	struct WINDOWCOMPOSITIONATTRIBDATA
	{
		WINDOWCOMPOSITIONATTRIB Attrib;
		PVOID pvData;
		SIZE_T cbData;
	};
#if DARKMODE_ALLOW_HAX
	using fnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode appMode); // ordinal 135, since 1809
	using fnFlushMenuThemes = void (WINAPI*)(); // ordinal 136
	fnSetPreferredAppMode _SetPreferredAppMode = nullptr;
	fnFlushMenuThemes _FlushMenuThemes = nullptr;

	bool ImportsInited = false;

	void InitImports() {
		if (ImportsInited) return;
		if (DarkMode::IsSupportedSystem()) {
			HMODULE hUxtheme = LoadLibraryEx(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
			if (hUxtheme) {
				_SetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135)));
				_FlushMenuThemes = reinterpret_cast<fnFlushMenuThemes>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(136)));
			}
		}
		ImportsInited = true;
	}
#endif
}



namespace DarkMode {
	UINT msgSetDarkMode() {
		// No need to threadguard this, should be main thread only, not much harm even if it's not
		static UINT val = 0;
		if (val == 0) val = RegisterWindowMessage(L"libPPUI:msgSetDarkMode");
		return val;
	}
	bool IsSupportedSystem() {
		return Win10BuildNumber() >= 17763 && !IsWine(); // require at least Win10 1809 / Server 2019
	}
	bool QueryUserOption() {
		DWORD v = 0;
		DWORD cb = sizeof(v);
		DWORD type = 0;
		if (RegGetValue(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD, &type, &v, &cb) == 0) {
			if (type == REG_DWORD) {
				return v == 0;
			}
		}
		return false;
	}
	void UpdateTitleBar(HWND hWnd, bool bDark) {
		if (!IsSupportedSystem()) return;

		CWindow wnd(hWnd);
		const DWORD style = wnd.GetStyle();
		if (style & WS_CHILD) return;

#if 0
		// Some apps do this - no idea why, doesn't work
		// Kept for future reference
		AllowDarkModeForWindow(hWnd, bDark);
		SetProp(hWnd, L"UseImmersiveDarkModeColors", (HANDLE)(INT_PTR)(bDark ? TRUE : FALSE));
#endif

		if (IsWindows11OrNewer()) {
			// DwmSetWindowAttribute()
			// Windows 11 : works
			// Windows 10 @ late 2021 : doesn't work
			// Server 2019 : as good as SetWindowCompositionAttribute(), needs ModifyStyle() hack for full effect
			BOOL arg = !!bDark;
			DwmSetWindowAttribute(hWnd, 19 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &arg, sizeof(arg));
		} else {
			// Windows 10 mode
			using fnSetWindowCompositionAttribute = BOOL(WINAPI*)(HWND hWnd, WINDOWCOMPOSITIONATTRIBDATA*);
			static fnSetWindowCompositionAttribute _SetWindowCompositionAttribute = reinterpret_cast<fnSetWindowCompositionAttribute>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
			if (_SetWindowCompositionAttribute != nullptr) {
				BOOL dark = !!bDark;
				WINDOWCOMPOSITIONATTRIBDATA data = { WCA_USEDARKMODECOLORS, &dark, sizeof(dark) };
				_SetWindowCompositionAttribute(hWnd, &data);

				// Neither of these fixes stuck titlebar (kept in here for future reference)
				// ::RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
				// ::SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_DRAWFRAME);

				// Apparently the least painful way to reliably fix stuck titlebar
				// 2x SWP_FRAMECHANGED needed with actual style changes

				if (style & WS_VISIBLE) { // Only do this if visible
					wnd.ModifyStyle(WS_BORDER, 0, SWP_FRAMECHANGED);
					wnd.ModifyStyle(0, WS_BORDER, SWP_FRAMECHANGED);
				}

			}
		}
	}

	void ApplyDarkThemeCtrl2(HWND ctrl, bool bDark, const wchar_t* ThemeID_light, const wchar_t * ThemeID_dark) {
		if (ctrl == NULL) return;
		if (bDark && IsSupportedSystem()) {
			::SetWindowTheme(ctrl, ThemeID_dark, NULL);
		} else {
			::SetWindowTheme(ctrl, ThemeID_light, NULL);
		}
	}

	void ApplyRetroTheme(HWND ctrl) {
		if (ctrl == NULL) return;
		SetWindowTheme(ctrl, L"", L"");
	}

	void ApplyDarkThemeCtrl(HWND ctrl, bool bDark, const wchar_t* ThemeID) {
		if ( ctrl == NULL ) return;
		if (bDark && IsSupportedSystem()) {
			std::wstring temp = L"DarkMode_"; temp += ThemeID;
			::SetWindowTheme(ctrl, temp.c_str(), NULL);
		} else {
			::SetWindowTheme(ctrl, ThemeID, NULL);
		}
	}

	void DarkenEditLite(HWND ctrl) {
		if (IsSupportedSystem()) {
			::SetWindowTheme(ctrl, L"DarkMode_Explorer", NULL);
		}
	}

	void DarkenComboLite(HWND ctrl) {
		if (IsSupportedSystem()) {
			::SetWindowTheme(ctrl, L"DarkMode_CFD", NULL);
			CComboBox combo = ctrl;
			COMBOBOXINFO info = { sizeof(info) };
			WIN32_OP_D(combo.GetComboBoxInfo(&info));
			if (info.hwndList != NULL) { // fix droplist scrollbars
				::SetWindowTheme(info.hwndList, L"DarkMode_Explorer", NULL);
			}

		}
	}

	bool IsDCDark(HDC dc_) {
		CDCHandle dc(dc_);
		return IsThemeDark(dc.GetTextColor(), dc.GetBkColor());
	}
	bool IsDialogDark(HWND dlg, UINT msgSend) {
		CWindowDC dc(dlg);
		dc.SetTextColor(0x000000);
		dc.SetBkColor(0xFFFFFF);
		::SendMessage(dlg, msgSend, (WPARAM)dc.m_hDC, (LPARAM)dlg);
		return IsDCDark(dc);
	}

	COLORREF GetSysColor(int idx, bool bDark) {
		return GetSysColor(idx, param_t{ bDark });
	}
	COLORREF GetSysColor(int idx, param_t const & p) {
		if (!p.IsDark()) {
			if (p.bRetro) {
				switch (idx) {
				case COLOR_MENU:
				case COLOR_BTNFACE:
				case COLOR_MENUBAR:
					// Win7: 0xD0D0C8
					// Win98: 0xC0C0C0
					// Win11: 0xF0F0F0
					return 0xF0F0F0;
				}
			}
			return ::GetSysColor(idx);
		}
		switch (idx) {
		case COLOR_MENU:
		case COLOR_BTNFACE:
		case COLOR_WINDOW:
		case COLOR_MENUBAR:
			// Explorer:
			// return 0x383838;
			// FIX ME apply tint here
			return 0x202020;
		case COLOR_BTNSHADOW:
			return 0;
		case COLOR_WINDOWTEXT:
		case COLOR_MENUTEXT:
		case COLOR_BTNTEXT:
		case COLOR_CAPTIONTEXT:
			// Explorer:
			// return 0xdedede;
			return 0xC0C0C0;
		case COLOR_BTNHIGHLIGHT:
		case COLOR_MENUHILIGHT:
			return 0x383838;
		case COLOR_HIGHLIGHT:
			return 0x777777;
		case COLOR_HIGHLIGHTTEXT:
			return 0x101010;
		case COLOR_GRAYTEXT:
			return 0x777777;
		case COLOR_HOTLIGHT:
			return 0xd69c56;
		default:
			return ::GetSysColor(idx);
		}
	}
#if DARKMODE_ALLOW_HAX
	void SetAppDarkMode(bool bDark) {
		InitImports();
		#
		if (_SetPreferredAppMode != nullptr) {
			static PreferredAppMode lastMode = PreferredAppMode::Default;
			PreferredAppMode wantMode = bDark ? PreferredAppMode::ForceDark : PreferredAppMode::ForceLight;
			if (lastMode != wantMode) {
				_SetPreferredAppMode(wantMode);
				lastMode = wantMode;
				if (_FlushMenuThemes) _FlushMenuThemes();
			}
		}
	}
#else
	void SetAppDarkMode(bool) {}
#endif

	bool IsThemeDark(COLORREF text, COLORREF background) {
		if (!IsSupportedSystem() || IsHighContrast()) return false;
		auto l_text = PaintUtils::Luminance(text);
		auto l_bk = PaintUtils::Luminance(background);
		if (l_text > l_bk) {
			if (l_bk <= PaintUtils::Luminance(GetSysColor(COLOR_BTNFACE, { /*.bDark = */ true}))) {
				return true;
			}
		}
		return false;
	}

	bool IsHighContrast() {
		HIGHCONTRASTW highContrast = { sizeof(highContrast) };
		if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, FALSE))
			return (highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
		return false;
	}

	static void DrawTab(CTabCtrl& tabs, CDCHandle dc, int iTab, bool selected, bool focused, const RECT * rcPaint, param_t const & p) {
		(void)focused;
		PFC_ASSERT((tabs.GetStyle() & TCS_VERTICAL) == 0);

		CRect rc;
		if (!tabs.GetItemRect(iTab, rc)) return;

		if ( rcPaint != nullptr ) {
			CRect foo;
			if (!foo.IntersectRect(rc, rcPaint)) return;
		}
		const int edgeCX = MulDiv(1, QueryScreenDPI_X(tabs), 120); // note: MulDiv() rounds up from +0.5, this will
		const auto colorBackground = GetSysColor(selected ? COLOR_HIGHLIGHT : COLOR_BTNFACE, p);
		const auto colorFrame = GetSysColor(COLOR_WINDOWFRAME, p);
		dc.SetDCBrushColor(colorBackground);
		dc.FillSolidRect(rc, colorBackground);

		{
			CPen pen;
			WIN32_OP_D(pen.CreatePen(PS_SOLID, edgeCX, colorFrame));
			SelectObjectScope scope(dc, pen);
			dc.MoveTo(rc.left, rc.bottom);
			dc.LineTo(rc.left, rc.top);
			dc.LineTo(rc.right, rc.top);
			dc.LineTo(rc.right, rc.bottom);
		}

		wchar_t text[512] = {};
		TCITEM item = {};
		item.mask = TCIF_TEXT;
		item.pszText = text;
		item.cchTextMax = (int)(std::size(text) - 1);
		if (tabs.GetItem(iTab, &item)) {
			SelectObjectScope fontScope(dc, tabs.GetFont());
			dc.SetBkMode(TRANSPARENT);
			dc.SetTextColor(GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT, p));
			dc.DrawText(text, (int)wcslen(text), rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
		}
	}

	void PaintTabs(CTabCtrl tabs, CDCHandle dc, const RECT * rcPaint, param_t const & p) {
		CRect rcClient; tabs.GetClientRect(rcClient); 
		CRect rcArea = rcClient; tabs.AdjustRect(FALSE, rcArea);
		int dx = rcClient.bottom - rcArea.bottom;
		int dy = rcClient.right - rcArea.right;
		CRect rcFrame = rcArea; rcFrame.InflateRect(dx/2, dy/2);
		dc.SetDCBrushColor(GetSysColor(COLOR_WINDOWFRAME, p));
		dc.FrameRect(rcFrame, (HBRUSH)GetStockObject(DC_BRUSH));
		const int tabCount = tabs.GetItemCount();
		const int tabSelected = tabs.GetCurSel();
		const int tabFocused = tabs.GetCurFocus();
		for (int iTab = 0; iTab < tabCount; ++iTab) {
			if (iTab != tabSelected) DrawTab(tabs, dc, iTab, false, iTab == tabFocused, rcPaint, p);
		}
		if (tabSelected >= 0) DrawTab(tabs, dc, tabSelected, true, tabSelected == tabFocused, rcPaint, p);
	}

	void PaintTabsErase(CTabCtrl tabs, CDCHandle dc, param_t const & p) {
		CRect rcClient; WIN32_OP_D(tabs.GetClientRect(rcClient));
		dc.FillSolidRect(&rcClient, GetSysColor(COLOR_BTNFACE, p));
	}


	// =================================================
	// NM_CUSTOMDRAW handlers
	// =================================================

	// We keep a global list of HWNDs that require dark rendering, so dialogs can call DarkMode::OnCustomDraw() which deals with this nonsense behind the scenes
	// This way there's no need to subclass parent windows at random

	enum class whichDark_t {
		none, toolbar, header
	};

	// mutex used in case someone uses off-main-thread UI, though it should not really happen in real life
	static std::shared_mutex lstDarkGuard;
	static std::map<HWND, whichDark_t> lstDark;
	static whichDark_t lstDark_query(HWND w) {
		std::shared_lock lock(lstDarkGuard);
		auto iter = lstDark.find(w);
		if (iter == lstDark.end()) return whichDark_t::none;
		return iter->second;
	}
	static void lstDark_set(HWND w, whichDark_t which) {
		std::unique_lock lock(lstDarkGuard);
		lstDark[w] = which;
	}
	static void lstDark_clear(HWND w) {
		std::unique_lock lock(lstDarkGuard);
		lstDark.erase(w);
	}

	LRESULT CustomDrawToolbar(NMHDR* hdr, param_t const & p) {
		if (!p.IsDark()) return CDRF_DODEFAULT;
		LPNMTBCUSTOMDRAW cd = reinterpret_cast<LPNMTBCUSTOMDRAW>(hdr);
		switch (cd->nmcd.dwDrawStage) {
		case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW;
		case CDDS_ITEMPREPAINT:
			cd->clrText = p.GetSysColor(COLOR_WINDOWTEXT);
			cd->clrBtnFace = p.GetSysColor(COLOR_BTNFACE);
			cd->clrBtnHighlight = p.GetSysColor(COLOR_BTNHIGHLIGHT);
			return CDRF_DODEFAULT;
		default:
			return CDRF_DODEFAULT;
		}
	}
	LRESULT CustomDrawHeader(NMHDR* hdr, param_t const & p) {
		if (!p.IsDark()) return CDRF_DODEFAULT;

		LPNMCUSTOMDRAW nmcd = reinterpret_cast<LPNMCUSTOMDRAW>(hdr);
		switch (nmcd->dwDrawStage)
		{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
		case CDDS_ITEMPREPAINT:
		{
			// FIX ME tint
			CDCHandle dc(nmcd->hdc);
			dc.SetTextColor(0xdedede);
			dc.SetBkColor(0x191919); // disregarded anyway
		}
		return CDRF_DODEFAULT;
		default:
			return CDRF_DODEFAULT;
		}
	}

	std::optional<LRESULT> OnCustomDraw(NMHDR* hdr, param_t const & p) {
		if (!p.IsDark()) return std::nullopt;
		switch (lstDark_query(hdr->hwndFrom)) {
		case whichDark_t::toolbar:
			return CustomDrawToolbar(hdr, p);
		case whichDark_t::header:
			return CustomDrawHeader(hdr, p);
		default:
			return std::nullopt;
		}
	}

	namespace {

		class CToolbarHook {
			param_t m_param;
			const bool m_explorerTheme;
			CToolBarCtrl m_wnd;
		public:
			CToolbarHook(HWND wnd, param_t initial, bool bExplorerTheme) : m_wnd(wnd), m_explorerTheme(bExplorerTheme) {
				SetDark(initial);
				lstDark_set(m_wnd, whichDark_t::toolbar);
			}
			
			void SetDark(param_t v) {
				if (m_param == v) return;
				m_param = v;
				if (v.bDark || v.bRetro) {
					if (m_explorerTheme) ::SetWindowTheme(m_wnd, L"", L""); // if we don't do this, NM_CUSTOMDRAW color overrides get disregarded
				} else {
					if (m_explorerTheme) ::SetWindowTheme(m_wnd, L"Explorer", NULL);
				}
				m_wnd.Invalidate();
				ApplyDarkThemeCtrl(m_wnd.GetToolTips(), v.IsDark());
			}
			~CToolbarHook() {
			}
		};

		class CTabsHook : public CWindowImpl<CTabsHook, CTabCtrl> {
		public:
			CTabsHook(param_t const & p) : m_param(p) {}
			BEGIN_MSG_MAP_EX(CTabsHook)
				MSG_WM_PAINT(OnPaint)
				MSG_WM_ERASEBKGND(OnEraseBkgnd)
				MESSAGE_HANDLER_EX(msgSetDarkMode(), OnSetDarkMode)
			END_MSG_MAP()

			void SetParam(param_t const &);
			void SubclassWindow(HWND);
		private:
			void OnPaint(CDCHandle);
			BOOL OnEraseBkgnd(CDCHandle);
			void ApplyDark();

			LRESULT OnSetDarkMode(UINT, WPARAM wp, LPARAM lp) {
				auto p = param_t::importMsgParams({ wp,lp });
				if (p) SetParam(*p);
				return 1;
			}

			param_t m_param;
		};
		void CTabsHook::SubclassWindow(HWND wnd) {
			WIN32_OP_D(__super::SubclassWindow(wnd));
			this->ApplyDark();
		}

		void CTabsHook::OnPaint(CDCHandle target) {
			if (!m_param.IsDark()) { SetMsgHandled(FALSE); return; }
			if (target) {
				PaintTabs(m_hWnd, target, nullptr, m_param);
			} else {
				CPaintDC dc(*this);
				PaintTabs(m_hWnd, dc.m_hDC, &dc.m_ps.rcPaint, m_param);
			}
		}
		BOOL CTabsHook::OnEraseBkgnd(CDCHandle dc) {
			if (m_param.IsDark()) {
				PaintTabsErase(*this, dc, m_param);
				return TRUE;
			}
			SetMsgHandled(FALSE);
			return FALSE;
		}
		void CTabsHook::ApplyDark() {
			if (m_hWnd == NULL) return;
			Invalidate();
			if (m_param.IsRetroLight()) {
				SetWindowTheme(m_hWnd, L"", L"");
			} else {
				SetWindowTheme(m_hWnd, L"explorer", nullptr);
			}
			ApplyDarkThemeCtrl(GetToolTips(), m_param.IsDark());
		}

		void CTabsHook::SetParam(param_t const & v) {
			if (m_param == v) return;
			m_param = v;
			ApplyDark();
		}

		class CTreeViewHook : public CWindowImpl<CTreeViewHook, CTreeViewCtrl> {
			param_t m_param;
		public:
			CTreeViewHook(param_t const & v) : m_param(v) {}

			BEGIN_MSG_MAP_EX(CTreeViewHook)
				MESSAGE_RANGE_HANDLER_EX(WM_CTLCOLORMSGBOX, WM_CTLCOLORSTATIC, OnCtlColor)
				MESSAGE_HANDLER_EX(msgSetDarkMode(), OnSetDarkMode)
				MESSAGE_HANDLER_EX(TVM_EDITLABEL, OnEditLabel)
			END_MSG_MAP()

			LRESULT OnCtlColor(UINT uMsg, WPARAM wParam, LPARAM lParam) {
				return GetParent().SendMessage(uMsg, wParam, lParam);
			}
			LRESULT OnEditLabel(UINT, WPARAM, LPARAM) {
				LRESULT ret = DefWindowProc();
				if (ret != 0) {
					HWND edit = (HWND) ret;
					PFC_ASSERT( ::IsWindow(edit) );
					ApplyDarkThemeCtrl( edit, m_param.IsDark() );
				}
				return ret;
			}
			void SetParam(param_t const & v) { 
				if (m_param == v) return;
				m_param = v;
				ApplyDark();
			}
			void ApplyDark() {
				if (m_param.IsRetroLight()) {
					ApplyRetroTheme(m_hWnd);
				} else {
					ApplyDarkThemeCtrl(m_hWnd, m_param.IsDark());
				}
				
				COLORREF bk = m_param.IsDark() ? GetSysColor(COLOR_WINDOW, m_param) : (COLORREF)(-1);
				COLORREF tx = m_param.IsDark() ? GetSysColor(COLOR_WINDOWTEXT, m_param) : (COLORREF)(-1);
				this->SetTextColor(tx); this->SetLineColor(tx);
				this->SetBkColor(bk);

				ApplyDarkThemeCtrl(GetToolTips(), m_param.IsDark());
			}

			void SubclassWindow(HWND wnd) {
				WIN32_OP_D( __super::SubclassWindow(wnd) );
				this->ApplyDark();
			}

			LRESULT OnSetDarkMode(UINT, WPARAM wp, LPARAM lp) {
				auto p = param_t::importMsgParams({ wp,lp });
				if (p) SetParam(*p);
				return 1;
			}
		};

		class CDialogHook : public CWindowImpl<CDialogHook> {
			param_t m_param;
			COLORREF m_customDarkBackground = colorUndefined;
		public:
			CDialogHook(param_t const & v) : m_param(v) {}
			void SetDarkDialogBackground(COLORREF arg) { m_customDarkBackground = arg; }
			void SetParam(param_t const & v) { 
				if (m_param == v) return;

				// Important: PostMessage()'ing this caused bugs
				SendMessage(WM_THEMECHANGED); 
				
				m_param = v; 
				
				// Ensure menu bar redraw with RDW_FRAME
				RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME);
			}

			// Undocumented Windows menu drawing API
			// Source: https://github.com/adzm/win32-custom-menubar-aero-theme

			static constexpr unsigned WM_UAHDRAWMENU = 0x0091;
			static constexpr unsigned WM_UAHDRAWMENUITEM = 0x0092;

			typedef union tagUAHMENUITEMMETRICS
			{
				struct {
					DWORD cx;
					DWORD cy;
				} rgsizeBar[2];
				struct {
					DWORD cx;
					DWORD cy;
				} rgsizePopup[4];
			} UAHMENUITEMMETRICS;

			typedef struct tagUAHMENUPOPUPMETRICS
			{
				DWORD rgcx[4];
				DWORD fUpdateMaxWidths : 2;
			} UAHMENUPOPUPMETRICS;

			typedef struct tagUAHMENU
			{
				HMENU hmenu;
				HDC hdc;
				DWORD dwFlags;
			} UAHMENU;

			typedef struct tagUAHMENUITEM
			{
				int iPosition;
				UAHMENUITEMMETRICS umim;
				UAHMENUPOPUPMETRICS umpm;
			} UAHMENUITEM;

			typedef struct UAHDRAWMENUITEM
			{
				DRAWITEMSTRUCT dis;
				UAHMENU um;
				UAHMENUITEM umi;
			} UAHDRAWMENUITEM;

			LRESULT OnCustomDraw(NMHDR* arg) {
				auto ret = ::DarkMode::OnCustomDraw(arg, m_param);
				if (ret) return *ret;
				SetMsgHandled(FALSE); return 0;
			}

			BEGIN_MSG_MAP_EX(CDialogHook)
				MESSAGE_RANGE_HANDLER_EX(WM_CTLCOLORMSGBOX, WM_CTLCOLORSTATIC, OnCtlColor)
				NOTIFY_CODE_HANDLER_EX(NM_CUSTOMDRAW, OnCustomDraw)
				MESSAGE_HANDLER_EX(WM_UAHDRAWMENU, Handle_WM_UAHDRAWMENU)
				MESSAGE_HANDLER_EX(WM_UAHDRAWMENUITEM, Handle_WM_UAHDRAWMENUITEM)
				MESSAGE_HANDLER_EX(msgSetDarkMode(), OnSetDarkMode)
			END_MSG_MAP()

			LRESULT OnSetDarkMode(UINT, WPARAM wp, LPARAM lp) {
				auto p = param_t::importMsgParams({ wp,lp });
				if (p) SetParam(*p);
				return 1;
			}

			COLORREF GetBkColor() { return m_param.GetSysColor(COLOR_WINDOW); }
			COLORREF GetTextColor() { return m_param.GetSysColor(COLOR_WINDOWTEXT); }

			HBRUSH ctlColorDlg(CDCHandle dc, CWindow wnd) {
				if (m_param.bRetro || (m_param.IsDark() && ::IsThemeDialogTextureEnabled(*this))) {
					auto bkColor = m_param.GetSysColor(COLOR_HIGHLIGHT);
					auto txColor = GetTextColor();

					dc.SetTextColor(txColor);
					dc.SetBkColor(bkColor);
					dc.SetDCBrushColor(bkColor);
					return (HBRUSH)GetStockObject(DC_BRUSH);
				}
				return ctlColorCommon(dc, wnd);
			}
			static bool isStdTextColor(COLORREF arg) {
				for (int id : {COLOR_WINDOWTEXT, COLOR_MENUTEXT, COLOR_BTNTEXT, COLOR_CAPTIONTEXT}) {
					auto match = ::GetSysColor(id);
					if (match == arg) return true;
				}
				return false;
			}
			static int msgToSysColor(UINT msg) {
				switch (msg) {
				case WM_CTLCOLORBTN:
				case WM_CTLCOLORMSGBOX:
				case WM_CTLCOLORSCROLLBAR:
				case WM_CTLCOLORDLG:
				case WM_CTLCOLORSTATIC:
					return COLOR_BTNFACE;
				default:
					return COLOR_WINDOW;
				}
			}
			LRESULT OnCtlColor(UINT msg, WPARAM wp, LPARAM) {
				if (m_param.IsDark() || m_param.bRetro) {
					CDCHandle dc = (HDC)wp;
					COLORREF txColor, bkColor;
					txColor = this->GetTextColor();
					bkColor = (m_customDarkBackground!=colorUndefined) ? m_customDarkBackground : m_param.GetSysColor(msgToSysColor(msg));

					if (msg == WM_CTLCOLORSTATIC) { 
						// warning: below code caused bugs for checkboxes, hence only done for static controls where dialog might override colors
						DefWindowProc();
						// Does default proc appear to alter text color? If it does, keep
						auto defTxColor = dc.GetTextColor();
						if (!isStdTextColor(defTxColor)) txColor = defTxColor;
					}

					dc.SetTextColor(txColor);
					dc.SetBkColor(bkColor);
					dc.SetDCBrushColor(bkColor);
					dc.SetBkMode(OPAQUE); // checkboxes have been known to repaint just text in some scenarios, causing glitches
					return (LPARAM)GetStockObject(DC_BRUSH);
				}
				SetMsgHandled(FALSE);

				return 0;
			}
			HBRUSH ctlColorCommon(CDCHandle, CWindow wnd) {
				(void)wnd;
				return NULL;
			}
			LRESULT Handle_WM_UAHDRAWMENU(UINT, WPARAM wParam, LPARAM lParam) {
				(void)wParam;
				if (!m_param.IsDark()) {
					SetMsgHandled(FALSE);
					return 0;
				}
				UAHMENU* pUDM = (UAHMENU*)lParam;
				CRect rc;

				MENUBARINFO mbi = { sizeof(mbi) };
				WIN32_OP_D(GetMenuBarInfo(m_hWnd, OBJID_MENU, 0, &mbi));

				CRect rcWindow;
				WIN32_OP_D(GetWindowRect(rcWindow));

				rc = mbi.rcBar;
				OffsetRect(&rc, -rcWindow.left, -rcWindow.top);

				rc.top -= 1;

				CDCHandle dc(pUDM->hdc);
				dc.FillSolidRect(rc, m_param.GetSysColor(COLOR_MENUBAR));
				return 0;
			}
			LRESULT Handle_WM_UAHDRAWMENUITEM(UINT, WPARAM wParam, LPARAM lParam) {
				(void)wParam;
				if (!m_param.IsDark()) {
					SetMsgHandled(FALSE);
					return 0;
				}

				UAHDRAWMENUITEM* pUDMI = (UAHDRAWMENUITEM*)lParam;
				CMenuHandle hMenu = pUDMI->um.hmenu;

				CString menuString;
				WIN32_OP_D(hMenu.GetMenuString(pUDMI->umi.iPosition, menuString, MF_BYPOSITION) > 0);

				DWORD drawTextFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;

				int iTextStateID = MPI_NORMAL;
				int iBackgroundStateID = MPI_NORMAL;
				if ((pUDMI->dis.itemState & ODS_INACTIVE) | (pUDMI->dis.itemState & ODS_DEFAULT)) {
					iTextStateID = MPI_NORMAL;
					iBackgroundStateID = MPI_NORMAL;
				}
				if (pUDMI->dis.itemState & (ODS_HOTLIGHT|ODS_SELECTED)) {
					iTextStateID = MPI_HOT;
					iBackgroundStateID = MPI_HOT;
				}
				if (pUDMI->dis.itemState & (ODS_GRAYED|ODS_DISABLED)) {
					iTextStateID = MPI_DISABLED;
					iBackgroundStateID = MPI_DISABLED;
				}
				if (pUDMI->dis.itemState & ODS_NOACCEL) {
					drawTextFlags |= DT_HIDEPREFIX;
				}

				if (m_menuTheme == NULL) {
					m_menuTheme.OpenThemeData(m_hWnd, L"Menu");
				}

				CDCHandle dc(pUDMI->um.hdc);
				switch (iBackgroundStateID) {
				case MPI_NORMAL:
				case MPI_DISABLED:
					dc.FillSolidRect(&pUDMI->dis.rcItem, m_param.GetSysColor(COLOR_MENUBAR));
					break;
				case MPI_HOT:
				case MPI_DISABLEDHOT:
					dc.FillSolidRect(&pUDMI->dis.rcItem, m_param.GetSysColor(COLOR_MENUHILIGHT));
					break;
				default:
					DrawThemeBackground(m_menuTheme, pUDMI->um.hdc, MENU_POPUPITEM, iBackgroundStateID, &pUDMI->dis.rcItem, nullptr);
					break;
				}
				DTTOPTS dttopts = { sizeof(dttopts) };
				if (iTextStateID == MPI_NORMAL || iTextStateID == MPI_HOT)
				{
					dttopts.dwFlags |= DTT_TEXTCOLOR;
					dttopts.crText = m_param.GetSysColor(COLOR_WINDOWTEXT);
				}
				DrawThemeTextEx(m_menuTheme, dc, MENU_POPUPITEM, iTextStateID, menuString, menuString.GetLength(), drawTextFlags, &pUDMI->dis.rcItem, &dttopts);

				return 0;
			}
			CTheme m_menuTheme;
		};


		class CStatusBarHook : public CWindowImpl<CStatusBarHook, CStatusBarCtrl> {
			param_t m_param;
		public:
			CStatusBarHook(param_t const & v) : m_param(v) {}

			BEGIN_MSG_MAP_EX(CStatusBarHook)
				MSG_WM_ERASEBKGND(OnEraseBkgnd)
				MSG_WM_PAINT(OnPaint)
				MESSAGE_HANDLER_EX(SB_SETTEXT, OnSetText)
				MESSAGE_HANDLER_EX(SB_SETICON, OnSetIcon)
				MESSAGE_HANDLER_EX(msgSetDarkMode(), OnSetDarkMode)
			END_MSG_MAP()

			LRESULT OnSetDarkMode(UINT, WPARAM wp, LPARAM lp) {
				auto p = param_t::importMsgParams({ wp,lp });
				if (p) SetParam(*p);
				return 1;
			}

			void SetParam(param_t const & v) {
				if (m_param != v) {
					m_param = v;
					Invalidate();
					ApplyDarkThemeCtrl(m_hWnd, v.IsDark());
				}
			}

			void SubclassWindow(HWND wnd) {
				WIN32_OP_D(__super::SubclassWindow(wnd));
				Invalidate();
				ApplyDarkThemeCtrl(m_hWnd, m_param.IsDark());
			}
			LRESULT OnSetIcon(UINT, WPARAM wp, LPARAM lp) {
				unsigned idx = (unsigned)wp;
				if (idx < 32) {
					CSize sz;
					if (lp != 0) sz = GetIconSize((HICON)lp);
					m_iconSizeCache[idx] = sz;
				}
				SetMsgHandled(FALSE);
				return 0;
			}
			LRESULT OnSetText(UINT, WPARAM wp, LPARAM) {
				// Status bar won't tell us about ownerdraw from GetText()
				// Have to listen to relevant messages to know
				unsigned idx = (unsigned)(wp & 0xFF);
				if (idx < 32) {
					uint32_t flag = 1 << idx;
					if (wp & SBT_OWNERDRAW) {
						m_ownerDrawMask |= flag;
					} else {
						m_ownerDrawMask &= ~flag;
					}
				}

				SetMsgHandled(FALSE);
				return 0;
			}

			void Paint(CDCHandle dc) {
				CRect rcClient; WIN32_OP_D(GetClientRect(rcClient));
				dc.FillSolidRect(rcClient, m_param.GetSysColor(COLOR_BTNFACE)); // Wine seems to not call our WM_ERASEBKGND handler, fill the background here too

				dc.SelectFont(GetFont());
				dc.SetBkMode(TRANSPARENT);
				dc.SetTextColor(m_param.GetSysColor(COLOR_WINDOWTEXT));
				CPen pen; pen.CreatePen(PS_SOLID, 1, m_param.GetSysColor(COLOR_BTNHIGHLIGHT));
				dc.SelectPen(pen);
				int count = this->GetParts(0, nullptr);
				for (int iPart = 0; iPart < count; ++iPart) {
					CRect rcPart;
					this->GetRect(iPart, rcPart);
					if (rcPart.left > 0) {
						dc.MoveTo(rcPart.left, rcPart.top);
						dc.LineTo(rcPart.left, rcPart.bottom);
					}
					int type = 0;
					CString text;
					this->GetText(iPart, text, &type);

					HICON icon = this->GetIcon(iPart);
					int iconMargin = 0;
					if (icon != NULL && (unsigned)iPart < std::size(m_iconSizeCache)) {

						auto size = m_iconSizeCache[iPart];

						dc.DrawIconEx(rcPart.left + size.cx / 4, (rcPart.top + rcPart.bottom) / 2 - size.cy / 2, icon, size.cx, size.cy);
						iconMargin = MulDiv(size.cx, 3, 2);
					}

					if (m_ownerDrawMask & (1 << iPart)) { // statusbar won't tell us about ownerdraw from GetText()
						DRAWITEMSTRUCT ds = {};
						ds.CtlType = ODT_STATIC;
						ds.CtlID = this->GetDlgCtrlID();
						ds.itemID = iPart;
						ds.hwndItem = m_hWnd;
						ds.hDC = dc;
						ds.rcItem = rcPart;

						DCStateScope scope(dc);
						GetParent().SendMessage(WM_DRAWITEM, GetDlgCtrlID(), (LPARAM)&ds);
					} else {
						CRect rcText = rcPart;
						int defMargin = rcText.Height() / 4;
						int l = iconMargin > 0 ? iconMargin : defMargin;
						int r = defMargin;
						rcText.DeflateRect(l, 0, r, 0);
						dc.DrawText(text, text.GetLength(), rcText, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
					}

					if (GetStyle() & SBARS_SIZEGRIP) {
						CSize size;
						auto theme = OpenThemeData(*this, L"status");
						PFC_ASSERT(theme != NULL);
						GetThemePartSize(theme, dc, SP_GRIPPER, 0, &rcClient, TS_DRAW, &size);
						auto rc = rcClient;
						rc.left = rc.right - size.cx;
						rc.top = rc.bottom - size.cy;
						DrawThemeBackground(theme, dc, SP_GRIPPER, 0, &rc, nullptr);
						CloseThemeData(theme);
					}
				}
			}

			void OnPaint(CDCHandle target) {
				if (!m_param.IsDark() && !m_param.bRetro) { SetMsgHandled(FALSE); return; }
				if (target) {
					Paint(target);
				} else {
					CPaintDC dc(*this);
					Paint(dc.m_hDC);
				}
			}

			BOOL OnEraseBkgnd(CDCHandle dc) {
				if (m_param.IsDark()) {
					CRect rc; WIN32_OP_D(GetClientRect(rc)); dc.FillSolidRect(rc, m_param.GetSysColor(COLOR_BTNFACE)); return TRUE;
				}
				SetMsgHandled(FALSE); return FALSE;			
			}

			uint32_t m_ownerDrawMask = 0;
			CSize m_iconSizeCache[32];
		};

		class CCheckBoxHook : public CWindowImpl<CCheckBoxHook, CButton> {
			param_t m_param;
		public:
			CCheckBoxHook(const param_t & v) : m_param(v) {}

			BEGIN_MSG_MAP_EX(CCheckBoxHook)
				MSG_WM_PAINT(OnPaint)
				MSG_WM_PRINTCLIENT(OnPaint)
				MSG_WM_ERASEBKGND(OnEraseBkgnd)
				MSG_WM_UPDATEUISTATE(OnUpdateUIState)

				// Note that checkbox implementation likes to paint on its own in response to events 
				// instead of invalidating and handling WM_PAINT
				// We have to specifically trigger WM_PAINT to override their rendering with ours
				MESSAGE_HANDLER_EX(WM_SETFOCUS, OnMsgRedraw)
				MESSAGE_HANDLER_EX(WM_KILLFOCUS, OnMsgRedraw)
				MESSAGE_HANDLER_EX(WM_ENABLE, OnMsgRedraw)
				MESSAGE_HANDLER_EX(WM_SETTEXT, OnMsgRedraw)

				MESSAGE_HANDLER_EX(msgSetDarkMode(), OnSetDarkMode)
			END_MSG_MAP()

			LRESULT OnSetDarkMode(UINT, WPARAM wp, LPARAM lp) {
				auto p = param_t::importMsgParams({ wp,lp });
				if (p) SetParam(*p);
				return 1;
			}

			LRESULT OnMsgRedraw(UINT, WPARAM, LPARAM) {
				if ( m_param.IsDark() ) {
					// PROBLEM: 
					// Can't invalidate prior to their handling of the message
					// Causes bugs with specific chains of events - EnableWindow() followed immediately SetWindowText()
					LRESULT ret = DefWindowProc();
					Invalidate();
					return ret;
				}
				SetMsgHandled(FALSE); return 0;
			}

			void OnUpdateUIState(WORD nAction, WORD nState) {
				(void)nAction;
				if (m_param.IsDark() && (nState & (UISF_HIDEACCEL | UISF_HIDEFOCUS)) != 0) {
					// PROBLEM: 
					// Can't invalidate prior to their handling of the message
					// Causes bugs with specific chains of events - EnableWindow() followed immediately SetWindowText()
					DefWindowProc();
					Invalidate();
					return;
				}
				SetMsgHandled(FALSE);
			}
			void PaintHandler(CDCHandle dc) {
				CRect rcClient; WIN32_OP_D(GetClientRect(rcClient));

				const bool bDisabled = !this->IsWindowEnabled();

				dc.SetTextColor(m_param.GetSysColor(COLOR_BTNTEXT));
				dc.SetBkColor(m_param.GetSysColor(COLOR_BTNFACE));
				dc.SetBkMode(OPAQUE);
				dc.SelectFont(GetFont());
				GetParent().SendMessage(WM_CTLCOLORBTN, (WPARAM)dc.m_hDC, (LPARAM)m_hWnd);
				if (bDisabled) dc.SetTextColor(m_param.GetSysColor(COLOR_GRAYTEXT)); // override WM_CTLCOLORBTN

				const DWORD btnStyle = GetStyle();
				const DWORD btnType = btnStyle & BS_TYPEMASK;
				const bool bRadio = (btnType == BS_RADIOBUTTON || btnType == BS_AUTORADIOBUTTON);
				const int part = bRadio ? BP_RADIOBUTTON : BP_CHECKBOX;

				const auto ctrlState = GetState();
				const DWORD uiState = (DWORD)SendMessage(WM_QUERYUISTATE);

				
				const bool bChecked = (ctrlState & BST_CHECKED) != 0;
				const bool bMixed = (ctrlState & BST_INDETERMINATE) != 0;
				const bool bHot = (ctrlState & BST_HOT) != 0;
				const bool bFocus = (ctrlState & BST_FOCUS) != 0 && (uiState & UISF_HIDEFOCUS) == 0;

				HTHEME theme = OpenThemeData(m_hWnd, L"button");

				CRect rcCheckBox = rcClient;
				bool bDrawn = false;
				int margin = 0;
				if (theme != NULL && IsThemePartDefined(theme, part, 0)) {
					int state = 0;
					if (bDisabled) {
						if ( bChecked ) state = CBS_CHECKEDDISABLED;
						else if ( bMixed ) state = CBS_MIXEDDISABLED;
						else state = CBS_UNCHECKEDDISABLED;
					} else if (bHot) {
						if ( bChecked ) state = CBS_CHECKEDHOT;
						else if ( bMixed ) state = CBS_MIXEDHOT;
						else state = CBS_UNCHECKEDNORMAL;
					} else {
						if ( bChecked ) state = CBS_CHECKEDNORMAL;
						else if ( bMixed ) state = CBS_MIXEDNORMAL;
						else state = CBS_UNCHECKEDNORMAL;
					}

					CSize size;
					if (SUCCEEDED(GetThemePartSize(theme, dc, part, state, rcCheckBox, TS_TRUE, &size))) {
						if (size.cx <= rcCheckBox.Width()) {
							CRect rc = rcCheckBox;
							margin = MulDiv(size.cx, 5, 4);
							rc.right = rc.left + size.cx;
							if (size.cy < rcCheckBox.Height()) {
								rc.top += (rc.Height() - size.cy) / 2;
								rc.bottom = rc.top + size.cy;
							}
							DrawThemeBackground(theme, dc, part, state, rc, &rc);
							bDrawn = true;
						}
					}
				}
				if (theme != NULL) CloseThemeData(theme);
				if (!bDrawn) {
					int stateEx = bRadio ? DFCS_BUTTONRADIO : DFCS_BUTTONCHECK;
					if (bChecked) stateEx |= DFCS_CHECKED;
					// FIX ME bMixed ?
					if (bDisabled) stateEx |= DFCS_INACTIVE;
					else if (bHot) stateEx |= DFCS_HOT;

					const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
					int w = MulDiv(16, dpi, 96);

					CRect rc = rcCheckBox; 
					if (rc.Width() > w) rc.right = rc.left + w;;

					DrawFrameControl(dc, rc, DFC_BUTTON, stateEx);
					margin = MulDiv(20, dpi, 96);
				}

				CString text;
				if (margin < rcClient.Width()) GetWindowText(text);
				if (!text.IsEmpty()) {
					CRect rcText = rcClient;
					rcText.left += margin;
					UINT dtFlags = DT_VCENTER;
					if (btnStyle & BS_MULTILINE) {
						dtFlags |= DT_WORDBREAK;
					} else {
						dtFlags |= DT_END_ELLIPSIS | DT_SINGLELINE;
					}
					dc.DrawText(text, text.GetLength(), rcText, dtFlags);
					if (bFocus) {
						dc.DrawText(text, text.GetLength(), rcText, DT_CALCRECT | dtFlags);
						dc.DrawFocusRect(rcText);
					}
				} else if (bFocus) {
					dc.DrawFocusRect(rcClient);
				}
			}
			void OnPaint(CDCHandle userDC, UINT flags = 0) {
				(void)flags;
				if (!m_param.IsDark()) { SetMsgHandled(FALSE); return; }
				if (userDC) {
					PaintHandler(userDC);
				} else {
					CPaintDC dc(*this);
					PaintHandler(dc.m_hDC);
				}
			}
			BOOL OnEraseBkgnd(CDCHandle dc) {
				if (m_param.IsDark()) {
					CRect rc; WIN32_OP_D(GetClientRect(rc));

					dc.SetTextColor(m_param.GetSysColor(COLOR_BTNTEXT));
					dc.SetBkColor(m_param.GetSysColor(COLOR_BTNFACE));
					auto br = (HBRUSH)GetParent().SendMessage(WM_CTLCOLORSTATIC, (WPARAM)dc.m_hDC, (LPARAM)m_hWnd);
					if (br != NULL) {
						dc.FillRect(rc, br);
					} else {
						dc.FillSolidRect(rc, m_param.GetSysColor(COLOR_BTNFACE));
					}
					return TRUE;
				}
				SetMsgHandled(FALSE); return FALSE;
			}

			void SetParam(param_t const & v) {
				if (v != m_param) {
					m_param = v;
					Invalidate();
					applyDark();
				}
			}

			void SubclassWindow(HWND wnd) {
				WIN32_OP_D(__super::SubclassWindow(wnd));
				applyDark();
			}

			void applyDark() {
				if (m_param.IsRetroLight()) {
					ApplyRetroTheme(m_hWnd);
				} else {
					// 2025-02 fix: disabled "Explorer" theming for checkboxes
					// it caused bugs with specific custom themes, missing checkbox marks in light mode
					// See: https://hydrogenaud.io/index.php/topic,127426.0.html
					ApplyDarkThemeCtrl2(m_hWnd, m_param.IsDark(), NULL);
				}
			}
		};

		class CGripperHook : public CWindowImpl<CGripperHook> {
		public:
			CGripperHook(param_t const & v) : m_param(v) {}

			BEGIN_MSG_MAP_EX(CGRipperHook)
				MSG_WM_ERASEBKGND(OnEraseBkgnd)
				MSG_WM_PAINT(OnPaint)
				MESSAGE_HANDLER_EX(msgSetDarkMode(), OnSetDarkMode)
			END_MSG_MAP()

			LRESULT OnSetDarkMode(UINT, WPARAM wp, LPARAM lp) {
				auto p = param_t::importMsgParams({ wp,lp });
				if (p) SetParam(*p);
				return 1;
			}

			void SetParam(param_t const & v) {
				if (v != m_param) {
					m_param = v;
					ApplyDarkThemeCtrl(*this, v.IsDark());
				}
			}

			void SubclassWindow(HWND wnd) {
				WIN32_OP_D(__super::SubclassWindow(wnd));
				ApplyDarkThemeCtrl(m_hWnd, m_param.IsDark());
			}

			void PaintGripper(CDCHandle dc) {
				CRect rcClient; WIN32_OP_D(GetClientRect(rcClient));
				CSize size;
				auto theme = OpenThemeData(*this, L"status");
				PFC_ASSERT(theme != NULL);
				GetThemePartSize(theme, dc, SP_GRIPPER, 0, &rcClient, TS_DRAW, &size);
				auto rc = rcClient;
				rc.left = rc.right - size.cx;
				rc.top = rc.bottom - size.cy;
				DrawThemeBackground(theme, dc, SP_GRIPPER, 0, &rc, nullptr);
				CloseThemeData(theme);
			}

			void OnPaint(CDCHandle dc) {
				if (!m_param.IsDark()) { SetMsgHandled(FALSE); return; }
				if (dc) PaintGripper(dc);
				else {CPaintDC pdc(*this); PaintGripper(pdc.m_hDC);}
			}

			BOOL OnEraseBkgnd(CDCHandle dc) {
				if (m_param.IsDark()) {
					CRect rc; GetClientRect(rc);
					dc.FillSolidRect(rc, m_param.GetSysColor(COLOR_WINDOW));
					return TRUE;
				}
				SetMsgHandled(FALSE); return FALSE;
			}
			param_t m_param;
		};

		class CReBarHook : public CWindowImpl<CReBarHook, CReBarCtrl> {
			param_t m_param;
		public:
			CReBarHook(param_t v) : m_param(v) {}
			BEGIN_MSG_MAP_EX(CReBarHook)
				MSG_WM_ERASEBKGND(OnEraseBkgnd)
				MSG_WM_DESTROY(OnDestroy)
				MSG_WM_PAINT(OnPaint)
				MSG_WM_PRINTCLIENT(OnPaint)
				NOTIFY_CODE_HANDLER_EX(NM_CUSTOMDRAW, OnCustomDraw)
				MESSAGE_HANDLER_EX(msgSetDarkMode(), OnSetDarkMode)
			END_MSG_MAP()

			LRESULT OnCustomDraw(NMHDR* arg) {
				auto ret = ::DarkMode::OnCustomDraw(arg, m_param);
				if (ret) return *ret;
				SetMsgHandled(FALSE); return 0;
			}

			LRESULT OnSetDarkMode(UINT, WPARAM wp, LPARAM lp) {
				auto p = param_t::importMsgParams({ wp,lp });
				if (p) SetParam(*p);
				return 1;
			}

			void OnPaint(CDCHandle target, unsigned flags = 0) {
				if (!m_param.IsDark()) { SetMsgHandled(FALSE); return; }
				(void)flags;
				if (target) {
					HandlePaint(target);
				} else {
					CPaintDC pdc(*this);
					HandlePaint(pdc.m_hDC);
				}
			}

			void HandlePaint(CDCHandle dc) {
				const int total = this->GetBandCount();
				for (int iBand = 0; iBand < total; ++iBand) {
					CRect rc;
					WIN32_OP_D(this->GetRect(iBand, rc));

					wchar_t buffer[256] = {};
					REBARBANDINFO info = { sizeof(info) };
					info.fMask = RBBIM_TEXT | RBBIM_CHILD | RBBIM_STYLE;
					info.lpText = buffer; info.cch = (UINT)std::size(buffer);
					WIN32_OP_D(this->GetBandInfo(iBand, &info));

					HFONT useFont;
					// Sadly overriding the font breaks the layout
					// MS implementation disregards fonts too
					// useFont = this->GetFont();
					useFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
					SelectObjectScope fontScope(dc, useFont);
					dc.SetTextColor(m_param.GetSysColor(COLOR_BTNTEXT));
					dc.SetBkMode(TRANSPARENT);

					CRect rcText = rc;
					if ((info.fStyle & RBBS_NOGRIPPER) == 0) {
						auto color = PaintUtils::BlendColor(m_param.GetSysColor(COLOR_WINDOWFRAME), m_param.GetSysColor(COLOR_BTNFACE));
						dc.SetDCPenColor(color);
						SelectObjectScope penScope(dc, GetStockObject(DC_PEN));
						dc.MoveTo(rcText.TopLeft());
						dc.LineTo(CPoint(rcText.left, rcText.bottom));
						rcText.left += 6; // this should be DPI-scaled, only it's not because rebar layout isn't either
					} else {
						rcText.left += 2;
					}
					dc.DrawText(buffer, (int)wcslen(buffer), &rcText, DT_VCENTER | DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
				}
			}


			void OnDestroy() {
				SetMsgHandled(FALSE);
			}
			BOOL OnEraseBkgnd(CDCHandle dc) {
				if (m_param.bDark || m_param.bRetro) {
					CRect rc;
					WIN32_OP_D(GetClientRect(rc));
					dc.FillSolidRect(rc, m_param.GetSysColor(COLOR_BTNFACE));
					return TRUE;
				}
				SetMsgHandled(FALSE); return FALSE;
			}
			void SetParam(param_t v) {
				if (v != m_param) {
					m_param = v; Apply();
				}
			}
			void Apply() {
				if (m_param.bDark || m_param.bRetro) {
					this->SetTextColor(m_param.GetSysColor(COLOR_WINDOWTEXT));
					this->SetBkColor(m_param.GetSysColor(COLOR_BTNFACE));
				} else {
					this->SetTextColor((COLORREF)-1);
					this->SetBkColor((COLORREF)-1);
				}
				Invalidate();
			}
			void SubclassWindow(HWND wnd) {
				WIN32_OP_D(__super::SubclassWindow(wnd));
				Apply();
			}
		};

		class CStaticHook : public CWindowImpl<CStaticHook, CStatic> {
			param_t m_param;
		public:
			CStaticHook(param_t const & v) : m_param(v) {}

			BEGIN_MSG_MAP_EX(CStaticHook)
				MSG_WM_PAINT(OnPaint)
				MESSAGE_HANDLER_EX(WM_ENABLE, OnMsgRedraw)
				MESSAGE_HANDLER_EX(msgSetDarkMode(), OnSetDarkMode)
			END_MSG_MAP()

			LRESULT OnSetDarkMode(UINT, WPARAM wp, LPARAM lp) {
				auto p = param_t::importMsgParams({ wp,lp });
				if (p) SetParam(*p);
				return 1;
			}

			LRESULT OnMsgRedraw(UINT, WPARAM, LPARAM) {
				Invalidate();
				SetMsgHandled(FALSE);
				return 0;
			}

			void SetParam(param_t const & v) {
				if (m_param != v) {
					m_param = v;
					Invalidate();
				}
			}

			void OnPaint(CDCHandle dc) {
				// ONLY override the dark+disabled or dark+icon behavior
				if (!m_param.IsDark() || (this->IsWindowEnabled() && this->GetIcon() == NULL)) {
					SetMsgHandled(FALSE); return;
				}

				if (dc) HandlePaint(dc);
				else {
					CPaintDC pdc(*this); HandlePaint(pdc.m_hDC);
				}
			}
			void HandlePaint(CDCHandle dc) {
				CString str;
				CIconHandle icon = this->GetIcon();
				this->GetWindowTextW(str);

				CRect rcClient;
				WIN32_OP_D(GetClientRect(rcClient));
				
				const DWORD style = this->GetStyle();
				
				HBRUSH br = (HBRUSH) GetParent().SendMessage(WM_CTLCOLORSTATIC, (WPARAM)dc.m_hDC, (LPARAM)m_hWnd);
				if (br == NULL) {
					dc.FillSolidRect(rcClient, m_param.GetSysColor(COLOR_WINDOW));
				} else {
					WIN32_OP_D(dc.FillRect(rcClient, br));
				}

				if (icon != NULL) {
					// https://hydrogenaud.io/index.php/topic,127458.0.html
					// dc.DrawIcon(0, 0, icon); <= doesn't use actual size, doesn't match MS control behavior
					dc.DrawIconEx(0, 0, icon, 0, 0); // <= good
				} else {
					DWORD flags = 0;
					if (style & SS_SIMPLE) flags |= DT_SINGLELINE | DT_WORD_ELLIPSIS;
					else flags |= DT_WORDBREAK;
					if (style & SS_RIGHT) flags |= DT_RIGHT;
					else if (style & SS_CENTER) flags |= DT_CENTER;

					dc.SelectFont(GetFont());
					dc.SetTextColor(m_param.GetSysColor(COLOR_GRAYTEXT));
					dc.SetBkMode(TRANSPARENT);
					dc.DrawText(str, str.GetLength(), rcClient, flags);
				}
			}
		};

		class CUpDownHook : public CWindowImpl<CUpDownHook, CUpDownCtrl> {
			param_t m_param;
		public:
			CUpDownHook(param_t const & v) : m_param(v) {}

			void SetParam(param_t const & v) {
				if (v != m_param) {
					m_param = v; Invalidate();
				}
			}

			BEGIN_MSG_MAP_EX(CUpDownHook)
				MSG_WM_PAINT(OnPaint)
				MSG_WM_PRINTCLIENT(OnPaint)
				MSG_WM_MOUSEMOVE(OnMouseMove)
				MSG_WM_LBUTTONDOWN(OnMouseBtn)
				MSG_WM_LBUTTONUP(OnMouseBtn)
				MSG_WM_MOUSELEAVE(OnMouseLeave)
				MESSAGE_HANDLER_EX(msgSetDarkMode(), OnSetDarkMode)
			END_MSG_MAP()

		private:
			LRESULT OnSetDarkMode(UINT, WPARAM wp, LPARAM lp) {
				auto p = param_t::importMsgParams({ wp,lp });
				if (p) SetParam(*p);
				return 1;
			}

			struct layout_t {
				CRect whole, upper, lower;
				int yCenter;
			};
			layout_t Layout(CRect const & rcClient) {
				CRect rc = rcClient;
				rc.DeflateRect(1, 1);
				int yCenter = (rc.top + rc.bottom) / 2;
				layout_t ret;
				ret.yCenter = yCenter;
				ret.whole = rc;
				ret.upper = rc;
				ret.upper.bottom = yCenter;
				ret.lower = rc;
				ret.lower.top = yCenter;
				return ret;
			}
			layout_t Layout() {
				CRect rcClient; WIN32_OP_D(GetClientRect(rcClient)); return Layout(rcClient);
			}
			int m_hot = 0;
			bool m_btnDown = false;
			void SetHot(int iHot) {
				if (iHot != m_hot) {
					m_hot = iHot; Invalidate();
				}
			}
			void OnMouseLeave() {
				SetHot(0);
				SetMsgHandled(FALSE);
			}
			int HitTest(CPoint pt) {
				auto layout = Layout();
				if (layout.upper.PtInRect(pt)) return 1;
				else if (layout.lower.PtInRect(pt)) return 2;
				else return 0;
			}
			void OnMouseBtn(UINT flags, CPoint) {
				bool bDown = (flags & MK_LBUTTON) != 0;
				if (bDown != m_btnDown) {
					m_btnDown = bDown;
					Invalidate();
				}
				SetMsgHandled(FALSE);
			}
			void OnMouseMove(UINT, CPoint pt) {
				SetHot(HitTest(pt));
				SetMsgHandled(FALSE);
			}
			void OnPaint(CDCHandle target, unsigned flags = 0) {
				if (!m_param.IsDark()) { SetMsgHandled(FALSE); return; }
				(void)flags;
				if (target) {
					HandlePaint(target);
				} else {
					CPaintDC pdc(*this);
					HandlePaint(pdc.m_hDC);
				}
			}
			void HandlePaint(CDCHandle dc) {
				// no corresponding getsyscolor values for this, hardcoded values taken from actual button control
				// + frame trying to fit edit control frame
				const COLORREF colorText = 0xFFFFFF;
				const COLORREF colorFrame = 0xFFFFFF;
				const COLORREF colorBk = 0x333333;
				const COLORREF colorHot = 0x454545;
				const COLORREF colorPressed = 0x666666;

				CRect rcClient; WIN32_OP_D(GetClientRect(rcClient));
				auto layout = Layout(rcClient);
				dc.FillRect(rcClient, MakeTempBrush(dc, colorBk));

				if (m_hot != 0) {
					auto color = m_btnDown ? colorPressed : colorHot;
					switch (m_hot) {
					case 1:
						dc.FillSolidRect(layout.upper, color);
						break;
					case 2:
						dc.FillSolidRect(layout.lower, color);
						break;
					}
				}

				dc.FrameRect(layout.whole, MakeTempBrush(dc, colorFrame));
				dc.SetDCPenColor(colorFrame);
				dc.SelectPen((HPEN)GetStockObject(DC_PEN));
				dc.MoveTo(layout.whole.left, layout.yCenter);
				dc.LineTo(layout.whole.right, layout.yCenter);
				
				CFontHandle f = GetFont();
				if (f == NULL) {
					auto buddy = this->GetBuddy();
					if ( buddy ) f = buddy.GetFont();
				}
				if (f == NULL) f = (HFONT) GetStockObject(DEFAULT_GUI_FONT);
				PFC_ASSERT(f != NULL);
				CFont font2;
				CreateScaledFont(font2, f, 0.5);
				SelectObjectScope selectFont(dc, font2);
				dc.SetBkMode(TRANSPARENT);
				dc.SetTextColor(colorText);
				dc.DrawText(L"˄", 1, layout.upper, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
				dc.DrawText(L"˅", 1, layout.lower, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
			}
		};

		class CNCFrameHook : public CWindowImpl<CNCFrameHook, CWindow> { 
		public:
			CNCFrameHook(param_t const & p) : m_param(p) {}

			BEGIN_MSG_MAP_EX(CNCFrameHook)
				MESSAGE_HANDLER_EX(msgSetDarkMode(), OnSetDarkMode)
				MSG_WM_NCPAINT(OnNCPaint)
			END_MSG_MAP()

			void SetParam(param_t v) {
				if (v != m_param) {
					m_param = v; ApplyDark();
				}
			}
			BOOL SubclassWindow(HWND wnd) {
				auto rv = __super::SubclassWindow(wnd);
				if (rv) {
					ApplyDark();
				}
				return rv;
			}
		private:
			void OnNCPaint(HRGN rgn) {
				if (m_param.IsDark()) {
					NCPaintDarkFrame(m_hWnd, rgn, m_param);
					return;
				}
				SetMsgHandled(FALSE);
			}
			void ApplyDark() {
				ApplyDarkThemeCtrl(m_hWnd, m_param.IsDark());
				Invalidate();
			}
			LRESULT OnSetDarkMode(UINT, WPARAM wp, LPARAM lp) {
				auto p = param_t::importMsgParams({ wp,lp });
				if (p) SetParam(*p);
				return 1;
			}

			param_t m_param;
		};

		class CListViewHook : public CWindowImpl<CListViewHook, CListViewCtrl> {
			param_t m_param;
			void ApplyDark() {
				ApplyDarkThemeCtrl(m_hWnd, m_param.IsDark());
				const auto tx = m_param.GetSysColor(COLOR_WINDOWTEXT), bk = m_param.GetSysColor(COLOR_WINDOW);
				this->SetTextColor(tx);
				this->SetBkColor(bk);
				this->SetTextBkColor(bk);
			}
			LRESULT OnSetDarkMode(UINT, WPARAM wp, LPARAM lp) {
				auto p = param_t::importMsgParams({ wp,lp });
				if (p) SetParam(*p);
				return 1;
			}
			LRESULT OnCustomDraw(NMHDR* arg) {
				auto ret = DarkMode::OnCustomDraw(arg, m_param);
				if (ret) return *ret;
				SetMsgHandled(FALSE); return 0;
			}
			LRESULT OnEditLabel(UINT, WPARAM, LPARAM) {
				LRESULT ret = DefWindowProc();
				if (ret != 0) {
					HWND edit = (HWND)ret;
					PFC_ASSERT(::IsWindow(edit));
					ApplyDarkThemeCtrl(edit, m_param.IsDark());
				}
				return ret;
			}
			LRESULT OnCtlColor(UINT uMsg, WPARAM wParam, LPARAM lParam) {
				return GetParent().SendMessage(uMsg, wParam, lParam);
			}
		public:
			CListViewHook(param_t const & p) : m_param(p) {}

			BEGIN_MSG_MAP_EX(CListViewHook)
				MESSAGE_RANGE_HANDLER_EX(WM_CTLCOLORMSGBOX, WM_CTLCOLORSTATIC, OnCtlColor)
				MESSAGE_HANDLER_EX(msgSetDarkMode(), OnSetDarkMode)
				NOTIFY_CODE_HANDLER_EX(NM_CUSTOMDRAW, OnCustomDraw)
				MESSAGE_HANDLER_EX(LVM_EDITLABEL, OnEditLabel)
			END_MSG_MAP()

			BOOL SubclassWindow(HWND wnd) {
				auto rv = __super::SubclassWindow(wnd);
				if (rv) {
					ApplyDark();
				}
				return rv;
			}

			void SetParam(param_t const &v) {
				if (v != m_param) {
					m_param = v; ApplyDark();
				}
			}
		};
	}

	void CHooks::AddPopup(HWND wnd) {
		addOp( [wnd, this] {
			UpdateTitleBar(wnd, IsDark());
		} );
	}

	void CHooks::AddDialogWithControls(HWND wnd) {
		AddDialog(wnd); AddControls(wnd);
	}

	void CHooks::AddDialog(HWND wnd, COLORREF bkgnd) {

		{
			CWindow w(wnd);
			if ((w.GetStyle() & WS_CHILD) == 0) {
				AddPopup(wnd);
			}
		}

		auto hook = new ImplementOnFinalMessage< CDialogHook > (m_param);
		hook->SubclassWindow(wnd);
		if (bkgnd != colorUndefined) hook->SetDarkDialogBackground(bkgnd);
		AddCtrlMsg(wnd);
	}
	void CHooks::AddTabCtrl(HWND wnd) {
		auto hook = new ImplementOnFinalMessage<CTabsHook>(m_param);
		hook->SubclassWindow(wnd);
		AddCtrlMsg(wnd);
	}
	void CHooks::AddComboBox(HWND wnd) {
		{ // fix droplist scrollbars
			CComboBox combo = wnd;
			COMBOBOXINFO info = {sizeof(info)};
			WIN32_OP_D( combo.GetComboBoxInfo(&info) );
			if (info.hwndList != NULL) {
				AddListBox( info.hwndList );
			}
		}

		addOp([wnd, this] { 
			if (m_param.IsRetroLight()) {
				SetWindowTheme(wnd, L"", L"");
			} else {
				SetWindowTheme(wnd, IsDark() ? L"DarkMode_CFD" : L"Explorer", NULL);
			}			
		});
	}
	void CHooks::AddComboBoxEx(HWND wnd) {
		this->AddControls(wnd); // recurse to add the combo box
	}
	void CHooks::AddEditBox(HWND wnd) { 
#if 0 // Experimental
		auto hook = new ImplementOnFinalMessage<CNCFrameHook>(m_dark);
		hook->SubclassWindow( wnd );
		AddCtrlMsg( wnd );
#else
		AddGeneric(wnd); 
#endif
	}
	void CHooks::AddButton(HWND wnd) { 
		CButton btn(wnd);
		auto style = btn.GetButtonStyle();
		auto type = style & BS_TYPEMASK;
		if ((type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON || type == BS_3STATE || type == BS_AUTO3STATE) && (style & BS_PUSHLIKE) == 0) {
			// MS checkbox implementation is terminally retarded and won't draw text in correct color
			// Subclass it and draw our own content
			// Other button types seem OK
			auto hook = new ImplementOnFinalMessage<CCheckBoxHook>(m_param);
			hook->SubclassWindow(wnd);
			AddCtrlMsg(wnd);
		} else if (type == BS_GROUPBOX) {
			SetWindowTheme(wnd, L"", L"");
			// SNAFU: re-creation of other controls such as list boxes causes repaint bugs due to overlapping
			// Even though this is not a fault of the groupbox, fix it here - by defer-pushing all group boxes to the back
			// Can't move to back right away, breaks window enumeration
			m_lstMoveToBack.push_back(wnd);
		} else {
			AddGeneric(wnd);
		}
		
	}
	void CHooks::AddGeneric(HWND wnd, const wchar_t * name) {
		this->addOp([wnd, this, name] {
			if (m_param.IsRetroLight()) {
				ApplyRetroTheme(wnd);
			} else {
				ApplyDarkThemeCtrl(wnd, IsDark(), name);
			}
		});
	}
	void CHooks::AddClassic(HWND wnd, const wchar_t* normalTheme) {
		this->addOp([wnd, this, normalTheme] {
			if (m_param.bDark || m_param.bRetro) ::SetWindowTheme(wnd, L"", L"");
			else ::SetWindowTheme(wnd, normalTheme, nullptr);
		});
	}
	void CHooks::AddStatusBar(HWND wnd) {
		auto hook = new ImplementOnFinalMessage<CStatusBarHook>(m_param);
		hook->SubclassWindow(wnd);
		this->AddCtrlMsg(wnd);
	}
	void CHooks::AddScrollBar(HWND wnd) {
		CWindow w(wnd);
		if (w.GetStyle() & SBS_SIZEGRIP) {
			auto hook = new ImplementOnFinalMessage<CGripperHook>(m_param);
			hook->SubclassWindow(wnd);
			this->AddCtrlMsg(wnd);
		} else {
			AddGeneric(wnd);
		}
	}

	void CHooks::AddReBar(HWND wnd) {
		auto hook = new ImplementOnFinalMessage<CReBarHook>(m_param);
		hook->SubclassWindow(wnd);
		this->AddCtrlMsg(wnd);
	}

	void CHooks::AddToolBar(HWND wnd, bool bExplorerTheme) {
		// Not a subclass
		addObj(new CToolbarHook(wnd, m_param, bExplorerTheme));
	}

	void CHooks::AddStatic(HWND wnd) {
		auto hook = new ImplementOnFinalMessage<CStaticHook>(m_param);
		hook->SubclassWindow(wnd);
		this->AddCtrlMsg(wnd);
	}

	void CHooks::AddUpDown(HWND wnd) {
		auto hook = new ImplementOnFinalMessage<CUpDownHook>(m_param);
		hook->SubclassWindow(wnd);
		this->AddCtrlMsg(wnd);
	}

	void CHooks::AddTreeView(HWND wnd) {
		auto hook = new ImplementOnFinalMessage<CTreeViewHook>(m_param);
		hook->SubclassWindow(wnd);
		this->AddCtrlMsg(wnd);
	}
	void CHooks::AddListBox(HWND wnd) {
		this->AddGeneric( wnd );
#if 0
		auto subst = CListControl_ReplaceListBox(wnd);
		if (subst) AddPPListControl(subst);
#endif
	}

	void CHooks::AddHeader(HWND wnd) {
		lstDark_set(wnd, whichDark_t::header);
		this->AddGeneric(wnd, L"ItemsView");
	}
	void CHooks::AddListView(HWND wnd) {
#if DARKMODE_LISTVIEW_SUBST
		auto subst = CListControl_ReplaceListView(wnd);
		if (subst) {
			AddPPListControl(subst); 
			return;
		}
#endif //vDARKMODE_LISTVIEW_SUBST

		auto hook = new ImplementOnFinalMessage<CListViewHook>(m_param);
		hook->SubclassWindow(wnd);
		AddCtrlMsg(wnd);

		CListViewCtrl v = wnd;
		auto h = v.GetHeader();
		if (h) AddHeader(h);
	}

	void CHooks::AddPPListControl(HWND wnd) {
		this->AddCtrlMsg(wnd);
		// this->addOp([this, wnd] { CListControl::wndSetDarkMode(wnd, m_dark); });
	}

	bool CHooks::SetParam(param_t const & v) {
		// Important: some handlers to ugly things if told to apply when no state change occurred - UpdateTitleBar() stuff in particular
		if (m_param == v) return false;
		m_param = v;
		for (auto& f : m_apply) f();
		return true;
	}
	void CHooks::flushMoveToBack() {
		for (auto w : m_lstMoveToBack) {
			::SetWindowPos(w, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
		m_lstMoveToBack.clear();
	}
	void CHooks::AddControls(HWND wndParent) {
		for (HWND walk = GetWindow(wndParent, GW_CHILD); walk != NULL; ) {
			HWND next = GetWindow(walk, GW_HWNDNEXT);
			AddCtrlAuto(walk);
			walk = next;
		}
		this->flushMoveToBack();
		// EnumChildWindows(wndParent, [this](HWND ctrl) {this->AddCtrlAuto(ctrl);});
	}

	void CHooks::AddCtrlMsg(HWND w) {
		this->addOp([this, w] {
			auto m = m_param.msgParams();
			::SendMessage(w, msgSetDarkMode(), m.wp, m.lp);
		});
	}

	void CHooks::AddCtrlAuto(HWND wnd) {

		if (::SendMessage(wnd, msgSetDarkMode(), (WPARAM)-1, (LPARAM)-1)) {
			AddCtrlMsg(wnd); return;
		}

		wchar_t buffer[128] = {};

		::GetClassName(wnd, buffer, (int)(std::size(buffer) - 1));

		const wchar_t* cls = buffer;
		if (_wcsnicmp(cls, L"ATL:", 4) == 0) cls += 4;


		if (_wcsicmp(cls, CButton::GetWndClassName()) == 0) {
			AddButton(wnd);
		} else if (_wcsicmp(cls, CComboBox::GetWndClassName()) == 0) {
			AddComboBox(wnd);
		} else if (_wcsicmp(cls, CComboBoxEx::GetWndClassName()) == 0 ) {
			AddComboBoxEx(wnd);
		} else if (_wcsicmp(cls, CTabCtrl::GetWndClassName()) == 0) {
			AddTabCtrl(wnd);
		} else if (_wcsicmp(cls, CStatusBarCtrl::GetWndClassName()) == 0) {
			AddStatusBar(wnd);
		} else if (_wcsicmp(cls, CEdit::GetWndClassName()) == 0) {
			AddEditBox(wnd);
		} else if (_wcsicmp(cls, WC_SCROLLBAR) == 0) {
			AddScrollBar(wnd);
		} else if (_wcsicmp(cls, CToolBarCtrl::GetWndClassName()) == 0) {
			AddToolBar(wnd);
		} else if (_wcsicmp(cls, CTrackBarCtrl::GetWndClassName()) == 0) {
			AddGeneric(wnd);
		} else if (_wcsicmp(cls, CTreeViewCtrl::GetWndClassName()) == 0) {
			AddTreeView(wnd);
		} else if (_wcsicmp(cls, CStatic::GetWndClassName()) == 0) {
			AddStatic(wnd);
		} else if (_wcsicmp(cls, CUpDownCtrl::GetWndClassName()) == 0) {
			AddUpDown(wnd);
		} else if (_wcsicmp(cls, CListViewCtrl::GetWndClassName()) == 0) {
			AddListView(wnd);
		} else if (_wcsicmp(cls, CListBox::GetWndClassName()) == 0) {
			AddListBox(wnd);
		} else if (_wcsicmp(cls, CReBarCtrl::GetWndClassName()) == 0) {
			AddReBar(wnd);
		} else if (_wcsicmp(cls, CHeaderCtrl::GetWndClassName()) == 0 ) {
			AddHeader(wnd);
		} else {
			PFC_DEBUG_PRINT("unknown class - ", buffer);
		}
	}

	void CHooks::clear() {
		m_apply.clear();
		for (auto v : m_cleanup) v();
		m_cleanup.clear();
	}

	void CHooks::AddApp() {
		addOp([this] {
			SetAppDarkMode(this->IsDark());
		});
	}

	void NCPaintDarkFrame(HWND wnd, HRGN rgn) {
		NCPaintDarkFrame(wnd, rgn, param_t { true });
	}
	void NCPaintDarkFrame(HWND wnd, HRGN rgn, param_t const & p) {
		const auto colorLight = p.GetSysColor(COLOR_BTNHIGHLIGHT);
		const auto colorDark = p.GetSysColor(COLOR_BTNSHADOW);

		NCPaintFrame(wnd, rgn, colorDark, colorDark, colorLight, colorLight);
	}

	void param_t::sendMessage(HWND to) const {
		auto m = msgParams();
		::SendMessage(to, msgSetDarkMode(), m.wp, m.lp);
	}
	msgParams_t param_t::msgParams() const {
		LPARAM lp = ((LPARAM) clrTint) & clrTintMask;
		if (bRetro) lp |= flagRetro;
		return { IsDark() ? 1u : 0u, lp };
	}
	std::optional<param_t> param_t::importMsgParams(msgParams_t const & arg) {
		if (arg.wp == msgSetDarkMode_wParam_query) return std::nullopt;
		param_t ret;
		ret.bDark = (arg.wp == 1);
		ret.clrTint = (COLORREF)(arg.lp & clrTintMask);
		ret.bRetro = (arg.lp & flagRetro) != 0;
		return ret;
	}

	COLORREF param_t::GetSysColor(int idx) const {
		return ::DarkMode::GetSysColor(idx, *this);
	}
}
