// ChainedDelegate.h - forwards drags to the drop target we displaced.
#pragma once

#include <oleidl.h>
#include <wil/com.h>

#include "webview/dnd/IDropTargetDelegate.h"

namespace fb2k_dnd {

// The IDropTarget currently registered on this window, or null when it has none.
//
// Reads the window property ole32 stores RegisterDragDrop's argument under. That
// property is an undocumented implementation detail, so a null result is a
// normal outcome and never an error. Reading it is what makes chaining possible
// at all: it is the only way to reach the drop target the WebView registered for
// itself, which has no accessor.
//
// The returned pointer is borrowed and carries no reference of its own.
IDropTarget* GetWindowDropTarget(HWND window);

// Depth-first search for the first descendant carrying an OLE drop target.
//
// Matching on the property rather than the window class is deliberate: a panel
// host has fourteen sibling Chrome_WidgetWin_0 windows and the real target is a
// Chrome_WidgetWin_1 two levels below the host, so neither a class-name match
// nor a direct-children-only scan finds it.
//
// Never walks upward, and never inspects the host itself. In a Default User
// Interface layout foobar2000 owns drop targets on the main window, the
// playlist, the tab strip and the status bar; taking one of those over would
// break the drag handling the user expects from the player.
//
// Returns null when no descendant carries the property. Callers must treat that
// as "leave this WebView alone", not as a reason to widen the search.
HWND FindDescendantWithDropTarget(HWND host);

// Forwards to the IDropTarget that was registered on the WebView child window
// before our own registration replaced it, so the page keeps receiving the
// standard HTML5 drag events that target handles.
//
// Uses DragPoint::screen. IDropTarget follows the OLE contract and expects
// screen coordinates, the opposite of CompositionDelegate; passing client
// coordinates would place the drag far from the cursor.
//
// Holds one reference to the inner target, taken before the registration was
// revoked, so the interface cannot die while a drag is in flight.
class ChainedDelegate final : public IDropTargetDelegate {
public:
    // target is the window the inner drop target was registered on, needed to
    // put it back later. inner must already carry a reference this object owns.
    ChainedDelegate(HWND target, wil::com_ptr<IDropTarget> inner);

    HRESULT Enter(IDataObject* data, DWORD keyState, const DragPoint& pt,
                  DWORD* effect) override;
    HRESULT Over(DWORD keyState, const DragPoint& pt, DWORD* effect) override;
    HRESULT Leave() override;
    HRESULT Drop(IDataObject* data, DWORD keyState, const DragPoint& pt,
                 DWORD* effect) override;

    bool IsValid() const override;

    // Registers the inner target on the window again and releases our
    // reference, so stepping out of the chain never leaves the window without a
    // drop target. Only valid once our own registration has been revoked.
    //
    // Skips the re-registration when the window is gone, or when it already
    // carries a target that someone else registered after ours was revoked;
    // displacing that one would orphan a live registration to reinstate a stale
    // one. Idempotent: a second call reports what the first reached.
    bool Restore() override;

private:
    HWND target_ = nullptr;
    wil::com_ptr<IDropTarget> inner_;
    // Whether the window is known to carry a working drop target again, which
    // makes a repeated Restore report the first call's outcome.
    bool restored_ = false;
};

}  // namespace fb2k_dnd
