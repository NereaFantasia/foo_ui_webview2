// IDropTargetDelegate.h - downstream target a DropTargetBridge forwards to.
#pragma once

#include <windows.h>

struct IDataObject;

namespace fb2k_dnd {

// Both coordinate spaces are carried because the two delegates disagree:
// CompositionController3 expects WebView client coordinates, while a chained
// Chromium IDropTarget follows the OLE contract and expects screen coordinates.
// Converting in the shared bridge would skew one of them.
struct DragPoint {
    POINTL screen;
    POINT  client;
};

class IDropTargetDelegate {
public:
    virtual ~IDropTargetDelegate() = default;

    virtual HRESULT Enter(IDataObject* data, DWORD keyState, const DragPoint& pt,
                          DWORD* effect) = 0;
    virtual HRESULT Over(DWORD keyState, const DragPoint& pt, DWORD* effect) = 0;
    virtual HRESULT Leave() = 0;
    virtual HRESULT Drop(IDataObject* data, DWORD keyState, const DragPoint& pt,
                         DWORD* effect) = 0;

    // False once the downstream interface is gone; the bridge then stops forwarding.
    virtual bool IsValid() const = 0;

    // Restores whatever this delegate displaced. No-op unless chaining.
    //
    // Returns whether the window ended up with a working drop target: true when
    // nothing was displaced, when the displaced target was put back, or when the
    // window already carries one. False only when the window was left without
    // any, which the caller reports rather than silently accepts.
    virtual bool Restore() { return true; }
};

}  // namespace fb2k_dnd
