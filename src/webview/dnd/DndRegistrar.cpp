// DndRegistrar.cpp
#include "pch.h"
#include "webview/dnd/DndRegistrar.h"

#include "core/SecurityConfig.h"
#include "webview/WebViewHost.h"
#include "webview/dnd/ChainedDelegate.h"
#include "webview/dnd/CompositionDelegate.h"
#include "webview/dnd/DndOriginPolicy.h"

namespace fb2k_dnd {

const char* ReasonToWire(PathsUnavailableReason reason) {
    switch (reason) {
    case PathsUnavailableReason::RegisterFailed:      return "register-failed";
    case PathsUnavailableReason::ForwardUnavailable:  return "forward-unavailable";
    case PathsUnavailableReason::InnerTargetNotFound: return "inner-target-not-found";
    case PathsUnavailableReason::ChainFailed:         return "chain-failed";
    case PathsUnavailableReason::Displaced:           return "displaced";
    case PathsUnavailableReason::OriginUntrusted:     return "origin-untrusted";
    case PathsUnavailableReason::None:
    default:
        return nullptr;
    }
}

void DndRegistrar::SetFailure(PathsUnavailableReason reason, bool html5) {
    // The detected host mode is kept: it describes where the WebView lives, not
    // whether registration worked, and the page uses it to interpret the reason.
    const bool visualHosting = caps_.visualHosting;
    caps_ = DndCapabilities{};
    caps_.visualHosting = visualHosting;
    caps_.html5 = html5;
    caps_.reason = reason;
}

bool DndRegistrar::Register(HWND hostHwnd, WebViewHost* host, uint64_t generation,
                            DropTargetBridge::EventSink sink) {
    if (bridge_) {
        // Registering again over a live registration would chain our own bridge
        // to itself in standard Controller mode, burying Chromium's target one
        // level down where Unregister can no longer reach it.
        //
        // The generation is adopted even though nothing is registered for this
        // caller: the existing registration now outlives the WebView that made
        // it, and this caller is the only one that will ever tear it down.
        generation_ = generation;
        // False because the sink passed in never goes live; the first Register's
        // sink keeps receiving the events. caps_ is left alone: it describes the
        // registration that is still running.
        LOG("DndRegistrar: already registered, second Register ignored");
        return false;
    }
    if (!hostHwnd || !host) {
        caps_ = DndCapabilities{};
        caps_.reason = PathsUnavailableReason::RegisterFailed;
        return false;
    }

    // A composition controller exists only in Visual Hosting mode, and that
    // decides both which window to register on and what to forward to. It is the
    // discriminator to use because the child window tree does not distinguish
    // the modes reliably: children exist under a Visual Hosting host as well.
    const bool visualHosting = host->GetCompositionController() != nullptr;
    caps_.visualHosting = visualHosting;

    // Exception barrier. This runs inside a WebView2 completion callback, so an
    // exception leaving here would unwind across the COM ABI. The chained path
    // additionally keeps its revoke-to-register window free of throwing calls,
    // so reaching this handler never means the window lost its drop target.
    try {
        return visualHosting ? RegisterVisual(hostHwnd, host, generation, std::move(sink))
                             : RegisterChained(hostHwnd, host, generation, std::move(sink));
    } catch (...) {
        // No logging here: LOG allocates and could throw a second time out of
        // this handler, back across the ABI the barrier exists to protect.
        if (bridge_) {
            // Registered and live; only the gate evaluation after it failed.
            return true;
        }
        // Chromium keeps handling drag events in standard Controller mode, while
        // nothing at all is registered in Visual Hosting.
        SetFailure(PathsUnavailableReason::RegisterFailed, /*html5=*/!visualHosting);
        return false;
    }
}

bool DndRegistrar::RegisterVisual(HWND hostHwnd, WebViewHost* host, uint64_t generation,
                                  DropTargetBridge::EventSink sink) {
    // Forwarding must be possible before anything is registered. A drop target
    // that has nowhere to forward to would consume the drag and leave the page
    // with no drag events at all, which is worse than not registering.
    //
    // GetCompositionController3 returns a borrowed pointer; assigning it to a
    // com_ptr takes the reference this object needs to keep.
    wil::com_ptr<ICoreWebView2CompositionController3> controller =
        host->GetCompositionController3();
    if (!controller) {
        // Reported as html5-capable on purpose. Whether drag events still reach
        // the page depends on runtime behaviour this code cannot observe, and a
        // page that hides its drop UI on a false negative loses working
        // functionality, while a false positive only leaves an idle drop zone.
        SetFailure(PathsUnavailableReason::ForwardUnavailable, /*html5=*/true);
        LOG("DndRegistrar: no CompositionController3, drag-drop not registered");
        return false;
    }

    auto delegate = std::make_unique<CompositionDelegate>(std::move(controller));
    wil::com_ptr<DropTargetBridge> bridge;
    // attach, not a raw-pointer construction: the bridge starts at one
    // reference, which this com_ptr takes over rather than adding to.
    bridge.attach(new DropTargetBridge(hostHwnd, std::move(delegate), std::move(sink)));

    const HRESULT hr = RegisterDragDrop(hostHwnd, bridge.get());
    if (FAILED(hr)) {
        // Nothing is registered on this window now, so the page gets no drag
        // events either: in this mode the host window is the only drop target.
        SetFailure(PathsUnavailableReason::RegisterFailed, /*html5=*/false);
        LOG("DndRegistrar: RegisterDragDrop failed, hr=", pfc::format_hex((uint32_t)hr));
        return false;
    }

    bridge_ = std::move(bridge);
    target_ = hostHwnd;
    generation_ = generation;
    // Whether the drop target property is readable back on this window, which
    // decides if Unregister may compare identities. Recorded now, while ours is
    // known to be the registration in place.
    identityObservable_ =
        GetWindowDropTarget(hostHwnd) == static_cast<IDropTarget*>(bridge_.get());
    caps_ = DndCapabilities{};
    caps_.html5 = true;
    caps_.visualHosting = true;

    // Paths stay closed until the gate says otherwise, so an untrusted document
    // never sees a real path even for a drag that starts during this call.
    ApplyOriginGate(host);
    return true;
}

bool DndRegistrar::RegisterChained(HWND hostHwnd, WebViewHost* host, uint64_t generation,
                                   DropTargetBridge::EventSink sink) {
    // Step 1: locate the drop target the WebView registered for itself. Failing
    // here leaves that registration untouched, so the page keeps the drag events
    // it already had and only the path side channel is missing.
    const HWND target = FindDescendantWithDropTarget(hostHwnd);
    if (!target) {
        SetFailure(PathsUnavailableReason::InnerTargetNotFound, /*html5=*/true);
        LOG("DndRegistrar: no descendant drop target found, chaining skipped");
        return false;
    }

    // Step 2: take a reference before revoking anything. GetPropW hands back a
    // borrowed pointer, and RevokeDragDrop releases the registration reference,
    // which may be the last one. attach takes over the manual AddRef instead of
    // adding a second one, which a raw-pointer construction would do.
    IDropTarget* raw = GetWindowDropTarget(target);
    if (!raw) {
        SetFailure(PathsUnavailableReason::InnerTargetNotFound, /*html5=*/true);
        LOG("DndRegistrar: descendant drop target disappeared before chaining");
        return false;
    }
    raw->AddRef();
    wil::com_ptr<IDropTarget> inner;
    inner.attach(raw);

    // Step 3: build everything while the window is still intact. Both
    // allocations below can throw, and doing that after the revoke would leave
    // the window with no drop target at all while the stack unwinds. innerRaw
    // stays valid because the delegate owns the reference moved into it, and is
    // what the window is registered back to if the registration fails.
    IDropTarget* innerRaw = inner.get();
    auto delegate = std::make_unique<ChainedDelegate>(target, std::move(inner));
    wil::com_ptr<DropTargetBridge> bridge;
    bridge.attach(new DropTargetBridge(target, std::move(delegate), std::move(sink)));

    // Step 4: free the window so it can take our target. On failure the window
    // still carries the original registration and the protective reference is
    // released with the bridge below.
    HRESULT hr = RevokeDragDrop(target);
    if (FAILED(hr)) {
        SetFailure(PathsUnavailableReason::ChainFailed, /*html5=*/true);
        LOG("DndRegistrar: RevokeDragDrop on inner target failed, left untouched, hr=",
            pfc::format_hex((uint32_t)hr));
        bridge.reset();
        return false;
    }

    // Step 5: register ours. Nothing between the revoke above and this call can
    // throw, so the window is never left targetless by an exception.
    hr = RegisterDragDrop(target, bridge.get());
    if (FAILED(hr)) {
        const HRESULT restoreHr = RegisterDragDrop(target, innerRaw);
        if (SUCCEEDED(restoreHr)) {
            // The window is back to what it was, so drag events keep working.
            SetFailure(PathsUnavailableReason::ChainFailed, /*html5=*/true);
            LOG("DndRegistrar: RegisterDragDrop on inner target failed, original "
                "restored, hr=", pfc::format_hex((uint32_t)hr));
        } else {
            // This window now carries no drop target at all, so the page has
            // lost the drag events it had before this call.
            SetFailure(PathsUnavailableReason::ChainFailed, /*html5=*/false);
            LOG("ERROR: DndRegistrar: could not restore inner drop target, window "
                "left without one, hr=", pfc::format_hex((uint32_t)restoreHr));
        }
        // Releases the bridge and with it the delegate's reference to the inner
        // target. A successful restore gave OLE its own reference.
        bridge.reset();
        return false;
    }

    // Step 6: ours is registered and the delegate owns the reference that keeps
    // the displaced target alive for as long as drags are forwarded to it.
    bridge_ = std::move(bridge);
    target_ = target;
    generation_ = generation;
    // Always true on this path: the target was found by reading the very same
    // property, so it is readable here by construction. Recorded rather than
    // assumed so the two modes set the flag the same way.
    identityObservable_ =
        GetWindowDropTarget(target) == static_cast<IDropTarget*>(bridge_.get());
    caps_ = DndCapabilities{};
    caps_.html5 = true;
    caps_.visualHosting = false;

    ApplyOriginGate(host);
    return true;
}

bool DndRegistrar::ApplyOriginGate(WebViewHost* host) {
    if (!bridge_) {
        return false;
    }

    // The live document origin, not the configured start URL: a popup may have
    // navigated to a third-party page since it was created.
    //
    // Deliberately not WebViewHost::IsOriginAllowed. That is the invoke
    // transport allow-list, and popups register arbitrary third-party URLs into
    // it, so reusing it here would open real paths to every popup.
    const std::wstring origin = host ? host->GetCurrentOriginNormalized() : std::wstring();
    const bool allowed = AllowsPaths(origin, security_config::UseDevServer());

    const DndCapabilities before = caps_;

    bridge_->SetPathsAllowed(allowed);
    caps_.paths = allowed;
    if (!allowed) {
        caps_.reason = PathsUnavailableReason::OriginUntrusted;
    } else if (caps_.reason == PathsUnavailableReason::OriginUntrusted) {
        caps_.reason = PathsUnavailableReason::None;
    }

    // Every field getCapabilities reports is compared, so the page cannot hold a
    // stale value that no event corrects. The first evaluation happens during
    // registration, before the document exists, so there is no listener yet.
    const bool changed = before.paths != caps_.paths || before.html5 != caps_.html5 ||
                         before.reason != caps_.reason ||
                         before.visualHosting != caps_.visualHosting;
    if (capsPublished_ && changed) {
        nlohmann::json payload;
        payload["html5"] = caps_.html5;
        payload["paths"] = caps_.paths;
        payload["hosting"] = caps_.visualHosting ? "visual" : "standard";
        if (const char* wire = ReasonToWire(caps_.reason)) {
            payload["pathsUnavailableReason"] = wire;
        }
        bridge_->EmitCapabilitiesChanged(payload);
    }
    capsPublished_ = true;

    return allowed;
}

void DndRegistrar::Unregister(uint64_t generation) {
    if (!bridge_) {
        return;
    }
    if (generation != generation_) {
        // A teardown left over from a superseded WebView must not remove the
        // registration the current one depends on.
        LOG("DndRegistrar: stale Unregister ignored");
        return;
    }

    // Stops forwarding and event emission first, so a callback arriving during
    // revocation cannot reach a WebView that is about to go away.
    bridge_->BeginShutdown();

    // Chromium may have revoked ours and registered its own target again around
    // a navigation or a renderer swap; RevokeDragDrop does not check identity, so
    // it would take out that newer target and Restore would put a stale one in
    // its place. Only a different non-null target proves that happened: a null
    // one is what a destroyed window and a failed property read both look like,
    // and neither may skip the revoke below.
    //
    // Gated on the flag because the comparison is only meaningful if the property
    // was observed to hold our own pointer at registration time. Where it does
    // not, every teardown would look like a foreign target and skip the revoke,
    // which is worse than the displacement this guards against.
    IDropTarget* const current =
        identityObservable_ ? GetWindowDropTarget(target_) : nullptr;
    if (current != nullptr && current != static_cast<IDropTarget*>(bridge_.get())) {
        // OLE released its reference when it revoked ours, so this destroys the
        // bridge and with it the reference to the target it displaced. Nothing is
        // restored: the window already carries a live drop target.
        LOG("DndRegistrar: window carries another drop target, teardown skipped");
        bridge_.reset();
        target_ = nullptr;
        identityObservable_ = false;
        caps_ = DndCapabilities{};
        capsPublished_ = false;
        return;
    }

    const HRESULT hr = RevokeDragDrop(target_);
    if (SUCCEEDED(hr)) {
        // The window is free again, so whatever was displaced to make room for
        // ours goes back before the bridge that holds it is released. A no-op
        // unless this registration displaced something.
        bridge_->RestoreDisplacedTarget();
        // OLE has dropped its reference, so releasing ours destroys the bridge.
        bridge_.reset();
    } else {
        // The window still points at our bridge, so restoring what it displaced
        // would collide with a registration that is still in place. Our own
        // reference goes anyway: OLE holds one of its own and keeps the object
        // alive for late callbacks, which are harmless now that it is shut down,
        // and the displaced target is released with the bridge when OLE lets go.
        LOG("DndRegistrar: RevokeDragDrop failed, bridge left to OLE, hr=",
            pfc::format_hex((uint32_t)hr));
        bridge_.reset();
    }

    target_ = nullptr;
    identityObservable_ = false;
    caps_ = DndCapabilities{};
    // A later registration starts from nothing published, so its own first gate
    // evaluation stays silent as it does on a fresh registrar.
    capsPublished_ = false;
}

}  // namespace fb2k_dnd
