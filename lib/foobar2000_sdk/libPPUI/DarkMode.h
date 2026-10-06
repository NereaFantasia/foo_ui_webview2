#pragma once
#include <functional>
#include <list>
#include <optional>
#include "DarkModeParam.h"

namespace DarkMode {
	// Is dark mode supported on this system or not?
	bool IsSupportedSystem();
	// Is system in dark mode or not?
	bool QueryUserOption();
	
	// Darken menus etc app-wide
	void SetAppDarkMode(bool bDark);
	// Darken window title bar
	void UpdateTitleBar(HWND wnd, bool bDark );
	void ApplyDarkThemeCtrl(HWND ctrl, bool bDark, const wchar_t * ThemeID = L"Explorer");
	void ApplyDarkThemeCtrl2(HWND ctrl, bool bDark, const wchar_t* ThemeID_light = L"Explorer", const wchar_t* ThemeID_dark = L"DarkMode_Explorer");
	void ApplyRetroTheme(HWND);

	// One-shot version of darkening function for editboxes
	void DarkenEditLite(HWND ctrl);
	// One-shot version of darkening function for comboboxes
	void DarkenComboLite(HWND ctrl);

	// Returns if the dialog appears to be using dark mode (text brighter than background)
	bool IsDialogDark(HWND dlg, UINT msgSend = WM_CTLCOLORDLG);
	// Returns if the DC appears to be using dark mode (text brighter than background)
	bool IsDCDark(HDC dc);
	
	// Returns if these colors (text, background) look like dark theme
	bool IsThemeDark(COLORREF text, COLORREF background);

	bool IsHighContrast();


	// Handle WM_NCPAINT drawing dark frame
	void NCPaintDarkFrame(HWND wnd, HRGN rgn, param_t const&);
	void NCPaintDarkFrame(HWND wnd, HRGN rgn);

	COLORREF GetSysColor(int, param_t const &);
	COLORREF GetSysColor(int, bool bDark = true);

	LRESULT CustomDrawToolbar(NMHDR*, param_t const &);
	LRESULT CustomDrawHeader(NMHDR*, param_t const &);

	// Custom draw handler that paints registered darkened controls.
	std::optional<LRESULT> OnCustomDraw(NMHDR*, param_t const & p);


	static constexpr COLORREF colorUndefined = 0xFFFFFFFF;

	//! CHooks class: entrypoint class for all Dark Mode hacks. \n
	//! Usage: Keep CHooks m_dark = detectDarkMode(); as a member of your window class, replacing detectDarkMode() with your own function returning dark mode on/off state. \n
	//! When initializing your window (WM_CREATE, WM_INITDIALOG), call m_dark.AddDialogWithControls(m_hWnd); \n
	//! AddDialogWithControls() walks all child windows of your window; call other individual methods to finetune handling of Dark Mode if that's not acceptable in your case.
	class CHooks {
	public:
		CHooks(param_t const& p) : m_param(p) {}
		CHooks(bool enabled = false) : CHooks(param_t{ /*.bDark = */ enabled}) {}
		CHooks(const CHooks&) = delete;
		void operator=(const CHooks&) = delete;

		void AddDialog(HWND,COLORREF bkgnd=colorUndefined);
		void AddTabCtrl(HWND);
		void AddComboBox(HWND);
		void AddComboBoxEx(HWND);
		void AddButton(HWND);
		void AddEditBox(HWND);
		void AddPopup(HWND);
		void AddStatusBar(HWND);
		void AddScrollBar(HWND);
		void AddToolBar(HWND, bool bExplorerTheme = true);
		void AddReBar(HWND);
		void AddStatic(HWND);
		void AddUpDown(HWND);
		void AddListBox(HWND);
		void AddListView(HWND);
		void AddTreeView(HWND);
		void AddHeader(HWND);
		void AddPPListControl(HWND);


		// SetWindowTheme with DarkMode_Explorer <=> Explorer
		void AddGeneric(HWND, const wchar_t * name = L"explorer");
		// SetWindowTheme(wnd, L"", L"") for dark, Explorer theme for normal
		void AddClassic(HWND, const wchar_t* normalTheme = L"explorer");

		void AddCtrlAuto(HWND);
		void AddCtrlMsg(HWND);

		void AddDialogWithControls(HWND);
		void AddControls(HWND wndParent);

		void SetDark(bool v = true) { SetParam({ /*.bDark = */ v}); }
		bool SetParam(param_t const& p);
		const param_t& Param() const { return m_param; }
		bool IsDark() const { return m_param.IsDark(); }
		operator bool() const { return IsDark(); }

		~CHooks() { clear(); }
		void clear();

		void AddApp();

	private:
		template<typename obj_t> void addObj(obj_t* arg) { 
			m_apply.push_back([arg, this] { arg->SetDark(m_param); });
			m_cleanup.push_back([arg] { delete arg; });
		}
		void addOp(std::function<void()> f) { f(); m_apply.push_back(f); }
		param_t m_param;
		
		std::list<std::function<void()> > m_apply;
		std::list<std::function<void()> > m_cleanup;

		void flushMoveToBack();
		std::list<HWND> m_lstMoveToBack;
	}; 
}
