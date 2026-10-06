#pragma once

// foobar2000 v2.0+ only
#if FOOBAR2000_TARGET_VERSION >= 81
#include <helpers/DarkMode.h>

// One-line fix for UI colors in CListControl, use CListControlFb2kColors< myCListControSubClass > to use fb2k colors
template<typename parent_t>
class CListControlFb2kColors : public parent_t, protected ui_config_callback_impl {
public:
	template<typename ... arg_t>
	CListControlFb2kColors(arg_t && ... args) : parent_t(std::forward<arg_t>(args) ... ) {
		this->SetDarkMode( fb2k::readDarkModeParam( m_uiConfig ));
	}
protected:
	void ui_colors_changed() override {
		this->SetDarkMode(fb2k::readDarkModeParam(m_uiConfig));
		this->Invalidate();
	}
	COLORREF GetSysColorHook(int colorIndex) const override {
		return m_uiConfig->getSysColor(colorIndex);
	}
	const ui_config_manager::ptr m_uiConfig = ui_config_manager::get();
};

#endif

// For use in UI elements
// Not completely selfcontained, host must pass ui_element_instance_callback and call ui_colors_changed()
template<typename parent_t>
class CListControlFb2kColorsUIElem : public parent_t {
public:
	template<typename ... arg_t>
	CListControlFb2kColorsUIElem(ui_element_instance_callback::ptr callback, arg_t && ... args) : parent_t(std::forward<arg_t>(args) ...), m_callback(callback) {
		this->SetDarkMode(fb2k::readDarkModeParam(m_callback));
	}
	void ui_colors_changed() { // host must call this in response to notify()
		this->SetDarkMode(fb2k::readDarkModeParam(m_callback));
		this->Invalidate();
	}
protected:
	COLORREF GetSysColorHook(int colorIndex) const override {
		return m_callback->getSysColor(colorIndex);
	}
	const ui_element_instance_callback_ptr m_callback;
};
