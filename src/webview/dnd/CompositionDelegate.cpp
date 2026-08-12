// CompositionDelegate.cpp
#include "pch.h"
#include "webview/dnd/CompositionDelegate.h"

namespace fb2k_dnd {

CompositionDelegate::CompositionDelegate(
    wil::com_ptr<ICoreWebView2CompositionController3> controller)
    : controller_(std::move(controller)) {}

HRESULT CompositionDelegate::Enter(IDataObject* data, DWORD keyState, const DragPoint& pt,
                                   DWORD* effect) {
    if (!controller_) return E_FAIL;
    return controller_->DragEnter(data, keyState, pt.client, effect);
}

HRESULT CompositionDelegate::Over(DWORD keyState, const DragPoint& pt, DWORD* effect) {
    if (!controller_) return E_FAIL;
    return controller_->DragOver(keyState, pt.client, effect);
}

HRESULT CompositionDelegate::Leave() {
    if (!controller_) return E_FAIL;
    return controller_->DragLeave();
}

HRESULT CompositionDelegate::Drop(IDataObject* data, DWORD keyState, const DragPoint& pt,
                                  DWORD* effect) {
    if (!controller_) return E_FAIL;
    return controller_->Drop(data, keyState, pt.client, effect);
}

bool CompositionDelegate::IsValid() const {
    return controller_ != nullptr;
}

}  // namespace fb2k_dnd
