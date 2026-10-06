#include "stdafx.h"

#include "GDIUtils.h"

HBITMAP CreateDIB24(CSize size) {
	struct {
		BITMAPINFOHEADER bmi;
	} bi = {};
	bi.bmi.biSize = sizeof(bi.bmi);
	bi.bmi.biWidth = size.cx;
	bi.bmi.biHeight = size.cy;
	bi.bmi.biPlanes = 1;
	bi.bmi.biBitCount = 24;
	bi.bmi.biCompression = BI_RGB;
	void* bitsPtr;
	return CreateDIBSection(NULL, reinterpret_cast<const BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bitsPtr, 0, 0);
}

HBITMAP CreateDIB16(CSize size) {
	struct {
		BITMAPINFOHEADER bmi;
	} bi = {};
	bi.bmi.biSize = sizeof(bi.bmi);
	bi.bmi.biWidth = size.cx;
	bi.bmi.biHeight = size.cy;
	bi.bmi.biPlanes = 1;
	bi.bmi.biBitCount = 16;
	bi.bmi.biCompression = BI_RGB;
	void* bitsPtr;
	return CreateDIBSection(NULL, reinterpret_cast<const BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bitsPtr, 0, 0);
}

HBITMAP CreateDIB8(CSize size, const COLORREF palette[256]) {
	struct {
		BITMAPINFOHEADER bmi;
		COLORREF colors[256];
	} bi = { };
	for (int c = 0; c < 256; ++c) bi.colors[c] = palette[c];
	bi.bmi.biSize = sizeof(bi.bmi);
	bi.bmi.biWidth = size.cx;
	bi.bmi.biHeight = size.cy;
	bi.bmi.biPlanes = 1;
	bi.bmi.biBitCount = 8;
	bi.bmi.biCompression = BI_RGB;
	bi.bmi.biClrUsed = 256;
	void* bitsPtr;
	return CreateDIBSection(NULL, reinterpret_cast<const BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bitsPtr, 0, 0);
}

void CreateScaledFont(CFont& out, CFontHandle in, double scale) {
	LOGFONT lf;
	WIN32_OP_D(in.GetLogFont(lf));
	int temp = pfc::rint32(scale * lf.lfHeight);
	if (temp == 0) temp = pfc::sgn_t(lf.lfHeight);
	lf.lfHeight = temp;
	WIN32_OP_D(out.CreateFontIndirect(&lf) != NULL);
}

void CreateScaledFontEx(CFont& out, CFontHandle in, double scale, int weight) {
	LOGFONT lf;
	WIN32_OP_D(in.GetLogFont(lf));
	int temp = pfc::rint32(scale * lf.lfHeight);
	if (temp == 0) temp = pfc::sgn_t(lf.lfHeight);
	lf.lfHeight = temp;
	lf.lfWeight = weight;
	WIN32_OP_D(out.CreateFontIndirect(&lf) != NULL);
}

void CreatePreferencesHeaderFont(CFont& out, CWindow source) {
	CreateScaledFontEx(out, source.GetFont(), 1.3, FW_BOLD);
}

void CreatePreferencesHeaderFont2(CFont& out, CWindow source) {
	CreateScaledFontEx(out, source.GetFont(), 1.1, FW_BOLD);
}

CSize GetBitmapSize(HBITMAP bmp) {
	PFC_ASSERT(bmp != NULL);
	CBitmapHandle h(bmp);
	BITMAP bm = {};
	WIN32_OP_D(h.GetBitmap(bm));
	return CSize(bm.bmWidth, bm.bmHeight);
}

CSize GetIconSize(HICON icon) {
	PFC_ASSERT(icon != NULL);
	CIconHandle h(icon);
	ICONINFO info = {};
	WIN32_OP_D( h.GetIconInfo(&info) );
	CSize ret;
	if (info.hbmColor != NULL) ret = GetBitmapSize(info.hbmColor);
	else if (info.hbmMask != NULL) ret = GetBitmapSize(info.hbmMask);
	else { PFC_ASSERT(!"???"); }
	if (info.hbmColor != NULL) DeleteObject(info.hbmColor);
	if (info.hbmMask != NULL) DeleteObject(info.hbmMask);
	return ret;
}

HBRUSH MakeTempBrush(HDC dc, COLORREF color) noexcept {
	SetDCBrushColor(dc, color); return (HBRUSH)GetStockObject(DC_BRUSH);
}

void NCPaintFrame(HWND wnd_, HRGN rgn_, COLORREF colorLeft, COLORREF colorTop, COLORREF colorRight, COLORREF colorBottom) {
	// rgn is in SCREEN COORDINATES, possibly (HRGN)1 to indicate no clipping / whole nonclient area redraw
	// we're working with SCREEN COORDINATES until actual DC painting
	CWindow wnd = wnd_;

	CRect rcWindow, rcClient;
	WIN32_OP_D(wnd.GetWindowRect(rcWindow));
	WIN32_OP_D(wnd.GetClientRect(rcClient));
	WIN32_OP_D(wnd.ClientToScreen(rcClient)); // transform all to same coordinate system

	CRgn rgnClip;
	WIN32_OP_D(rgnClip.CreateRectRgnIndirect(rcWindow) != NULL);
	if (rgn_ != NULL && rgn_ != (HRGN)1) {
		// we have a valid HRGN from caller?
		if (rgnClip.CombineRgn(rgn_, RGN_AND) == NULLREGION) return; // nothing to draw, exit early
	}

	{
		// Have scroll bars? Have DefWindowProc() them then exclude from our rgnClip.
		SCROLLBARINFO si = { sizeof(si) };
		if (::GetScrollBarInfo(wnd, OBJID_VSCROLL, &si) && (si.rgstate[0] & STATE_SYSTEM_INVISIBLE) == 0 && si.rcScrollBar.left < si.rcScrollBar.right) {
			CRect rc = si.rcScrollBar;
			// rcClient.right = rc.right;
			CRgn rgn; WIN32_OP_D(rgn.CreateRectRgnIndirect(rc));
			int status = SIMPLEREGION;
			if (rgnClip) {
				status = rgn.CombineRgn(rgn, rgnClip, RGN_AND);
			}
			if (status != NULLREGION) {
				DefWindowProc(wnd, WM_NCPAINT, (WPARAM)rgn.m_hRgn, 0);
				rgnClip.CombineRgn(rgn, RGN_DIFF); // exclude from further drawing
			}
		}
		if (::GetScrollBarInfo(wnd, OBJID_HSCROLL, &si) && (si.rgstate[0] & STATE_SYSTEM_INVISIBLE) == 0 && si.rcScrollBar.top < si.rcScrollBar.bottom) {
			CRect rc = si.rcScrollBar;
			// rcClient.bottom = rc.bottom;
			CRgn rgn; WIN32_OP_D(rgn.CreateRectRgnIndirect(rc));
			int status = SIMPLEREGION;
			if (rgnClip) {
				status = rgn.CombineRgn(rgn, rgnClip, RGN_AND);
			}
			if (status != NULLREGION) {
				DefWindowProc(wnd, WM_NCPAINT, (WPARAM)rgn.m_hRgn, 0);
				rgnClip.CombineRgn(rgn, RGN_DIFF); // exclude from further drawing
			}
		}
	}

	CWindowDC dc(wnd);
	if (dc.IsNull()) {
		PFC_ASSERT(!"???");
		return;
	}


	// Window DC has (0,0) in upper-left corner of our window (not screen, not client)
	// Turn rcWindow to (0,0), (winWidth, winHeight)
	CPoint origin = rcWindow.TopLeft();
	rcWindow.OffsetRect(-origin);
	rcClient.OffsetRect(-origin);

	if (!rgnClip.IsNull()) {
		// rgnClip is still in screen coordinates, fix this here
		rgnClip.OffsetRgn(-origin);
		dc.SelectClipRgn(rgnClip);
	}

	// bottom
	dc.FillSolidRect(CRect(rcClient.left, rcClient.bottom, rcWindow.right, rcWindow.bottom), colorBottom);
	// right
	dc.FillSolidRect(CRect(rcClient.right, rcWindow.top, rcWindow.right, rcClient.bottom), colorRight);
	// top
	dc.FillSolidRect(CRect(rcWindow.left, rcWindow.top, rcWindow.right, rcClient.top), colorTop);
	// left
	dc.FillSolidRect(CRect(rcWindow.left, rcClient.top, rcClient.left, rcWindow.bottom), colorLeft);
}