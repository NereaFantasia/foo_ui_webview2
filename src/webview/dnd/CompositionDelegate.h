// CompositionDelegate.h - forwards drags into a Visual Hosting WebView.
#pragma once

#include <WebView2.h>
#include <wil/com.h>

#include "webview/dnd/IDropTargetDelegate.h"

namespace fb2k_dnd {

// Hands the borrowed IDataObject to ICoreWebView2CompositionController3 so the
// page still receives standard HTML5 drag events.
//
// Uses DragPoint::client: the composition controller takes physical WebView
// client pixels and divides by the rasterization scale itself, so no DPI
// conversion happens here.
//
// Holds the controller interface rather than the WebViewHost that produced it,
// so a drag still in flight cannot reach a destroyed host.
class CompositionDelegate final : public IDropTargetDelegate {
public:
    explicit CompositionDelegate(wil::com_ptr<ICoreWebView2CompositionController3> controller);

    HRESULT Enter(IDataObject* data, DWORD keyState, const DragPoint& pt,
                  DWORD* effect) override;
    HRESULT Over(DWORD keyState, const DragPoint& pt, DWORD* effect) override;
    HRESULT Leave() override;
    HRESULT Drop(IDataObject* data, DWORD keyState, const DragPoint& pt,
                 DWORD* effect) override;

    bool IsValid() const override;

private:
    wil::com_ptr<ICoreWebView2CompositionController3> controller_;
};

}  // namespace fb2k_dnd
