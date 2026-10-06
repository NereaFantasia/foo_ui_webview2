#pragma once

#ifdef _WIN32

#include <SDK/ui_element.h>
#include <libPPUI/DarkMode.h>

// fb2k::CDarkModeHooks
// foobar2000 specific helper on top of libPPUI DarkMode::CHooks
// Automatically determines whether dark mode should be on or off
// Keeps track of dark mode preference changes at runtime
// Does nothing if used in foobar2000 older than 2.0

// IMPORTANT
// See also: SDK/coreDarkMode.h
// Using CCoreDarkMode lets you invoke foobar2000's instance of this code instead of static linking it, resulting in much smaller component binary.
// Using CDarkModeHooks directly is good mainly for debugging or troubleshooting.

namespace fb2k {
	bool isDarkMode();
	DarkMode::param_t darkParams();
	DarkMode::param_t darkParams(bool v);
#ifndef CDarkModeHooks
	class CDarkModeHooks : public DarkMode::CHooks, private ui_config_callback_impl {
	public:
		CDarkModeHooks() : CHooks(darkParams()) {}
		
	private:
		void ui_fonts_changed() override {}
		void ui_colors_changed() override { this->SetParam(darkParams()); }
	};
#endif

	template<typename api_t>
	DarkMode::param_t readDarkModeParam(api_t api) {
		DarkMode::param_t ret;
		if (api) {
			if (api->is_dark_mode()) {
				ret.bDark = true;
				t_ui_color tint = 0;
				if (api->query_color(ui_color_darkmode_tint, tint)) {
					ret.clrTint = tint & ret.clrTintMask;
				}
			} else {
				t_ui_color test = 0;
				if (api->query_color(ui_color_retromode, test)) {
					if (test != 0) ret.bRetro = true;
				}
			}
		}
		return ret;
	}

}

#endif
