// DndRegistrar.h - decides where to register drag-drop and performs the transaction.
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <wil/com.h>

#include "webview/dnd/DropTargetBridge.h"

class WebViewHost;

namespace fb2k_dnd {

// Why the real-path side channel is unavailable while HTML5 drag-drop may still
// work. Reported to the page so it can explain the limitation instead of
// silently receiving empty path arrays.
enum class PathsUnavailableReason {
    None,
    RegisterFailed,
    ForwardUnavailable,
    InnerTargetNotFound,
    ChainFailed,
    Displaced,
    OriginUntrusted,
};

struct DndCapabilities {
    bool html5 = false;
    bool paths = false;
    bool visualHosting = false;
    PathsUnavailableReason reason = PathsUnavailableReason::None;
};

// The kebab-case name the page sees for a reason, or nullptr when paths are
// available and there is nothing to explain. Shared so the capability event and
// the getCapabilities response cannot drift apart.
const char* ReasonToWire(PathsUnavailableReason reason);

// Owns the IDropTarget registered on a host window and the paired teardown.
//
// One instance per WebView host. Registration is all-or-nothing: on any failure
// nothing of ours stays registered, because a drop target that swallows drags
// without forwarding them is worse than no drop target at all. In standard
// Controller mode a failure also puts Chromium's own drop target back, so the
// page keeps the drag events it had before.
class DndRegistrar {
public:
    // hostHwnd is the window OLE delivers drops to in Visual Hosting mode, and
    // the root of the search for Chromium's drop target in standard Controller
    // mode. generation is the caller's WebView generation, recorded so a stale
    // Unregister cannot tear down a newer registration.
    //
    // Returns false when nothing was registered; Capabilities() then carries the
    // reason, including whether HTML5 drag events still reach the page. Safe to
    // call again after a failure.
    bool Register(HWND hostHwnd, WebViewHost* host, uint64_t generation,
                  DropTargetBridge::EventSink sink);

    // Must run before the WebView is reset: RevokeDragDrop has to precede
    // releasing the interfaces the delegate forwards to.
    void Unregister(uint64_t generation);

    // Backstop only. Callers still have to Unregister explicitly while the
    // WebView interfaces are alive, but destroying without it would leave the
    // bridge registered on a window with a sink that captures a dead owner.
    ~DndRegistrar() { Unregister(generation_); }

    // Re-evaluates the path gate against the document currently loaded, so a
    // window that navigated to an untrusted origin stops seeing real paths.
    // Emits dnd:capabilitiesChanged when the outcome differs from what the page
    // was last told, but not for the first evaluation during registration, when
    // there is no previous state and no listener yet.
    // Returns true when paths are allowed afterwards.
    bool ApplyOriginGate(WebViewHost* host);

    DndCapabilities Capabilities() const { return caps_; }
    DropTargetBridge* Bridge() { return bridge_.get(); }

private:
    // Visual Hosting: the host window carries no drop target, so registering on
    // it is enough and forwarding goes through the composition controller.
    bool RegisterVisual(HWND hostHwnd, WebViewHost* host, uint64_t generation,
                        DropTargetBridge::EventSink sink);

    // Standard Controller: Chromium already owns a drop target on one of its
    // child windows, so ours has to take that place and forward to it.
    bool RegisterChained(HWND hostHwnd, WebViewHost* host, uint64_t generation,
                         DropTargetBridge::EventSink sink);

    // Records why the path side channel is unavailable. html5 says whether the
    // page still gets drag events, which differs per host mode: nothing is
    // registered in Visual Hosting, while Chromium keeps handling them in
    // standard Controller mode.
    void SetFailure(PathsUnavailableReason reason, bool html5);

    wil::com_ptr<DropTargetBridge> bridge_;
    HWND target_ = nullptr;
    uint64_t generation_ = 0;
    DndCapabilities caps_{};
    // Whether the page has been told a capability state, which distinguishes the
    // gate evaluation during registration from a later re-evaluation.
    bool capsPublished_ = false;
    // Whether the window's drop target property was observed to hold our own
    // bridge at registration time. Only then can teardown compare identities to
    // tell our registration from one Chromium made after revoking ours; where the
    // property is unreadable every teardown would look foreign and skip the
    // revoke, which is worse than the displacement the comparison guards against.
    bool identityObservable_ = false;
};

}  // namespace fb2k_dnd
