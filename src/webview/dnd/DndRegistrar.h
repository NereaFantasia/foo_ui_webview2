// DndRegistrar.h - decides where to register drag-drop and performs the transaction.
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <WebView2.h>
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

// Why the page cannot drag content out to other applications.
//
// A separate enum from PathsUnavailableReason on purpose: the two capabilities
// fail for disjoint sets of causes, and one shared enum would let a value that
// is meaningless for the other capability be reported for it.
enum class DragOutUnavailableReason {
    None,
    // Standard Controller mode: Chromium owns the drag source and there is no
    // composition controller to take it over through.
    NotVisualHosting,
    // The WebView2 runtime predates ICoreWebView2CompositionController5, so the
    // drag-start event this feature is built on does not exist.
    RuntimeTooOld,
    // Drag-drop registration did not complete, so nothing was probed.
    RegisterFailed,
};

struct DndCapabilities {
    bool html5 = false;
    bool paths = false;
    bool visualHosting = false;
    PathsUnavailableReason reason = PathsUnavailableReason::None;
    // Whether the page may drag content out of the window. Independent of
    // paths: dragging out needs a runtime feature, while paths needs a trusted
    // origin, and either can be missing on its own.
    bool dragOut = false;
    DragOutUnavailableReason dragOutReason = DragOutUnavailableReason::None;
};

// The kebab-case name the page sees for a reason, or nullptr when paths are
// available and there is nothing to explain. Shared so the capability event and
// the getCapabilities response cannot drift apart.
const char* ReasonToWire(PathsUnavailableReason reason);

// The kebab-case name the page sees for a drag-out reason, or nullptr when
// dragging out is available and there is nothing to explain. Shared for the same
// reason as ReasonToWire above.
const char* DragOutReasonToWire(DragOutUnavailableReason reason);

// Fills a dnd.getCapabilities result or a dnd:capabilitiesChanged payload. Both
// are generated from one declaration and have the same members, so the method
// and the event cannot describe a window differently. Each reason is set only
// when its capability is unavailable, so a page tests for the key rather than
// comparing it against a "none" value.
template <class Out>
void FillCapabilities(const DndCapabilities& caps, Out& out) {
    out.html5 = caps.html5;
    out.paths = caps.paths;
    out.hosting = caps.visualHosting ? "visual" : "standard";
    out.dragOut = caps.dragOut;
    if (const char* reason = ReasonToWire(caps.reason)) {
        out.pathsUnavailableReason = reason;
    }
    if (const char* reason = DragOutReasonToWire(caps.dragOutReason)) {
        out.dragOutUnavailableReason = reason;
    }
}

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

    // Runs for every drag started inside the WebView, this feature's or not, and
    // splits them by whether the token carrier is present before considering
    // whether the token is any good. noexcept because it is called from a
    // WebView2 event callback.
    HRESULT OnDragStarting(ICoreWebView2DragStartingEventArgs* args) noexcept;

    // Tells the page the host attached no files to a drag out of this window.
    void EmitDragFailed(const char* code, const char* error) const;

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

    // The drag-start subscription, kept so teardown can remove it before the
    // handler's captured this goes away. Set only in Visual Hosting mode on a
    // runtime new enough to offer the event.
    wil::com_ptr<ICoreWebView2CompositionController5> dragSource_;
    EventRegistrationToken dragStartingToken_{};
};

}  // namespace fb2k_dnd
