#pragma once

#include "DarkMode.h"

namespace DarkMode {
	void PaintTabsErase(CTabCtrl, CDCHandle, param_t const &);
	void PaintTabs(CTabCtrl, CDCHandle, const RECT* rcPaint /*= nullptr*/, param_t const &);
}