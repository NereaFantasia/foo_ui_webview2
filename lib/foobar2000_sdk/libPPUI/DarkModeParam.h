#pragma once
#include <optional>

namespace DarkMode {
	// msgSetDarkMode
	// return 1 if understood, 0 otherwise
	// WPARAM = 0, DISABLE dark mode
	// WPARAM = 1, ENABLE dark mode
	// WPARAM = -1, query if supported
	UINT msgSetDarkMode();
	static constexpr WPARAM msgSetDarkMode_wParam_query = (WPARAM)-1;


	struct msgParams_t {
		WPARAM wp; LPARAM lp;
	};
	struct param_t {
		bool bDark = false;
		bool bRetro = false;
		COLORREF clrTint = 0;

		bool IsDark() const { return bDark; }
		bool IsRetroLight() const { return bRetro && !bDark; }

		static bool equals(const param_t& v1, const param_t v2) {
			return v1.bDark == v2.bDark && v1.bRetro == v2.bRetro && v1.clrTint == v2.clrTint;
		}
		bool operator==(const param_t& arg) const { return equals(*this, arg); }
		bool operator!=(const param_t& arg) const { return !equals(*this, arg); }

		msgParams_t msgParams() const;
		static std::optional<param_t> importMsgParams(msgParams_t const&);
		void sendMessage(HWND to) const;


		static constexpr LPARAM clrTintMask = 0xFFFFFF;
		static constexpr LPARAM flagRetro = 0x1000000;

		COLORREF GetSysColor(int) const;
	};

}