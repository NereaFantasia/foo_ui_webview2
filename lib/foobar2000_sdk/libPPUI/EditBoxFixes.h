#pragma once

namespace PP {
	// One-line methods to inject our edit box shims: Ctrl+A, Ctrl+Backspace, etc
	void editBoxFix(HWND wndEdit);
	void editBoxFixLite(HWND wndEdit); // Only axe esc key takeover if needed
	void comboBoxFix(HWND wndCombo);
}