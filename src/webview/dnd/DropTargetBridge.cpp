// DropTargetBridge.cpp
#include "pch.h"
#include "webview/dnd/DropTargetBridge.h"

#include "api/BridgeCore.h"
#include "webview/dnd/DropEffectPolicy.h"
#include "webview/dnd/HdropReader.h"
#include "webview/dnd/ShortcutResolver.h"

namespace fb2k_dnd {
namespace {

int64_t NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

nlohmann::json PathsToJson(const std::vector<std::wstring>& paths) {
    nlohmann::json out = nlohmann::json::array();
    for (const std::wstring& path : paths) {
        out.push_back(WideToUtf8(path));
    }
    return out;
}

// The parallel shortcut-target array, always exactly as long as paths.
//
// Driven by the paths list rather than by resolved, so the equal-length part of
// the contract holds by construction: a resolver that returned a short array,
// or none at all, still yields one JSON element per path. An unknown target
// becomes null, never an empty string, since "" would read as a real path of
// zero length to a page that only checks for truthiness.
nlohmann::json ResolvedPathsToJson(const std::vector<std::wstring>& paths,
                                   const std::vector<ResolvedTarget>& resolved) {
    nlohmann::json out = nlohmann::json::array();
    for (size_t i = 0; i < paths.size(); ++i) {
        if (i < resolved.size() && resolved[i].has_value()) {
            out.push_back(WideToUtf8(*resolved[i]));
        } else {
            out.push_back(nlohmann::json(nullptr));
        }
    }
    return out;
}

}  // namespace

DropTargetBridge::DropTargetBridge(HWND target,
                                   std::unique_ptr<IDropTargetDelegate> delegate,
                                   EventSink sink)
    : target_(target), delegate_(std::move(delegate)), sink_(std::move(sink)) {}

void DropTargetBridge::BeginShutdown() {
    shuttingDown_ = true;
    if (enterForwarded_ && delegate_ && delegate_->IsValid()) {
        delegate_->Leave();
    }
    ClearActiveDrag();
    sessions_.Clear();
    // Shutdown is terminal, and nothing reopens the gate afterwards, so it is
    // closed with the store rather than left holding a trusted origin's verdict.
    pathsAllowed_ = false;
}

void DropTargetBridge::RestoreDisplacedTarget() {
    if (delegate_) {
        delegate_->Restore();
    }
}

DragPoint DropTargetBridge::MakePoint(POINTL screen) const {
    DragPoint point{};
    point.screen = screen;
    POINT client{screen.x, screen.y};
    if (target_) {
        ScreenToClient(target_, &client);
    }
    point.client = client;
    return point;
}

nlohmann::json DropTargetBridge::VisiblePaths(
    const std::vector<std::wstring>& paths) const {
    if (!pathsAllowed_) {
        return nlohmann::json::array();
    }
    return PathsToJson(paths);
}

nlohmann::json DropTargetBridge::VisibleResolvedPaths(
    const std::vector<std::wstring>& paths,
    const std::vector<ResolvedTarget>& resolved) const {
    if (!pathsAllowed_) {
        // Empty, not a list of nulls: VisiblePaths withholds the whole array in
        // this case, and the two must stay the same length. A null-filled array
        // would also leak the file count, which the empty one does not.
        return nlohmann::json::array();
    }
    return ResolvedPathsToJson(paths, resolved);
}

void DropTargetBridge::EmitCapabilitiesChanged(const nlohmann::json& payload) const {
    if (shuttingDown_) {
        return;
    }
    Emit("dnd:capabilitiesChanged", payload);
}

void DropTargetBridge::Emit(const char* event, const nlohmann::json& payload) const {
    if (!sink_) {
        return;
    }
    try {
        sink_(event, payload);
    } catch (...) {
        // A faulty listener must not abort the drag.
    }
}

std::string DropTargetBridge::ClearActiveDrag() noexcept {
    std::string sessionId;
    // Moved out rather than copied, so this cannot throw and the caller still
    // gets the id it needs for EndSession and the event payload.
    sessionId.swap(activeSessionId_);
    enterForwarded_ = false;
    return sessionId;
}

// IUnknown --------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE DropTargetBridge::QueryInterface(REFIID riid,
                                                           void** ppv) noexcept {
    if (!ppv) {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IDropTarget) {
        *ppv = static_cast<IDropTarget*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DropTargetBridge::AddRef() noexcept {
    return static_cast<ULONG>(InterlockedIncrement(&refCount_));
}

ULONG STDMETHODCALLTYPE DropTargetBridge::Release() noexcept {
    const LONG remaining = InterlockedDecrement(&refCount_);
    if (remaining == 0) {
        delete this;
    }
    return static_cast<ULONG>(remaining);
}

// IDropTarget -----------------------------------------------------------------

HRESULT STDMETHODCALLTYPE DropTargetBridge::DragEnter(IDataObject* data, DWORD keyState,
                                                      POINTL pt,
                                                      DWORD* effect) noexcept {
    // Both are contract violations, so they are rejected before any downstream
    // work: OLE requires an in/out effect slot and a data object here.
    if (!effect || !data) {
        if (effect) {
            *effect = DROPEFFECT_NONE;
        }
        return E_INVALIDARG;
    }
    // Saved before anything can overwrite it. This is the mask of effects the
    // drag source permits, and every later computation intersects with it.
    const DWORD allowedMask = *effect;
    // Cleared before anything can throw, so a failed re-entry cannot leave the
    // previous drag's flag set and forward an unpaired Over or Drop downstream.
    ClearActiveDrag();

    // Before the HDROP is read, so a drag arriving after shutdown cannot refill
    // the store BeginShutdown emptied with fresh absolute paths.
    if (shuttingDown_) {
        *effect = DROPEFFECT_NONE;
        return S_OK;
    }

    try {
        // A source may re-enter without a matching leave, and BeginSession
        // supersedes the previous session so no stale one survives into this drag.
        bool hadHdrop = false;
        std::vector<std::wstring> paths = ReadHdropPaths(data, &hadHdrop);
        const bool hasFiles = hadHdrop && !paths.empty();
        // Derived from the list ReadHdropPaths actually returned, so its
        // catch-all path (which clears paths) cannot leave a parallel array
        // behind: an empty list resolves to an empty array.
        //
        // Skipped outright when the origin is not trusted with paths.
        // VisibleResolvedPaths and dnd.getPathsAsync both withhold the array in
        // that case, so every target read would be discarded - and the reading
        // is shell and filesystem work on the thread the drag source is blocked
        // on. Not a leak, just a cost an untrusted document must not be able to
        // impose. BeginSession pads the empty vector to the length of paths, so
        // the length invariant holds without a resolution pass.
        std::vector<ResolvedTarget> resolved;
        if (pathsAllowed_) {
            resolved = ResolveShortcutTargets(paths);
        }
        const int64_t now = NowMs();
        activeSessionId_ = sessions_.BeginSession(paths, hasFiles, now, resolved);

        if (shuttingDown_) {
            *effect = DROPEFFECT_NONE;
            return S_OK;
        }

        const DragPoint point = MakePoint(pt);
        DWORD downstream = allowedMask;
        if (delegate_ && delegate_->IsValid()) {
            if (SUCCEEDED(delegate_->Enter(data, keyState, point, &downstream))) {
                enterForwarded_ = true;
            } else {
                downstream = DROPEFFECT_NONE;
            }
        } else {
            downstream = DROPEFFECT_NONE;
        }

        *effect = ChooseDropEffect(downstream, allowedMask, hasFiles);

        nlohmann::json payload;
        payload["sessionId"] = activeSessionId_;
        payload["paths"] = VisiblePaths(paths);
        payload["resolvedPaths"] = VisibleResolvedPaths(paths, resolved);
        payload["hasFiles"] = hasFiles;
        payload["x"] = point.client.x;
        payload["y"] = point.client.y;
        Emit("dnd:enter", payload);
        return S_OK;
    } catch (...) {
        // Returning a failure HRESULT would make the shell show an error dialog,
        // so the drag is refused quietly instead.
        *effect = DROPEFFECT_NONE;
        return S_OK;
    }
}

HRESULT STDMETHODCALLTYPE DropTargetBridge::DragOver(DWORD keyState, POINTL pt,
                                                     DWORD* effect) noexcept {
    if (!effect) {
        return E_INVALIDARG;
    }
    const DWORD allowedMask = *effect;

    try {
        if (shuttingDown_) {
            *effect = DROPEFFECT_NONE;
            return S_OK;
        }

        // Only a gesture still in progress may answer this. An empty id means the
        // last one already left or dropped, and Query treats an empty id as "most
        // recently ended", which would report that gesture's files as ours.
        bool hasFiles = false;
        if (!activeSessionId_.empty()) {
            if (const SessionData* session = sessions_.Query(activeSessionId_, NowMs())) {
                hasFiles = session->hasFiles;
            }
        }

        DWORD downstream = allowedMask;
        if (enterForwarded_ && delegate_ && delegate_->IsValid()) {
            const DragPoint point = MakePoint(pt);
            if (FAILED(delegate_->Over(keyState, point, &downstream))) {
                downstream = DROPEFFECT_NONE;
            }
        } else {
            downstream = DROPEFFECT_NONE;
        }

        *effect = ChooseDropEffect(downstream, allowedMask, hasFiles);
        // No dnd:over event: one drag produces tens to hundreds of DragOver
        // calls, so emitting per call would flood the bridge. Pages that need
        // cursor tracking use the HTML5 dragover event instead.
        return S_OK;
    } catch (...) {
        *effect = DROPEFFECT_NONE;
        return S_OK;
    }
}

HRESULT STDMETHODCALLTYPE DropTargetBridge::DragLeave() noexcept {
    // Whether the matching Enter reached the delegate, read before the state is
    // cleared: an unpaired leave must not go downstream.
    const bool wasForwarded = enterForwarded_;
    // Cleared before anything can throw, since the catch-all reports S_OK and a
    // stale flag would forward an Over or Drop for a gesture already gone.
    const std::string sessionId = ClearActiveDrag();

    try {
        if (wasForwarded && !shuttingDown_ && delegate_ && delegate_->IsValid()) {
            delegate_->Leave();
        }

        if (sessionId.empty()) {
            return S_OK;
        }
        sessions_.EndSession(sessionId, NowMs());
        if (!shuttingDown_) {
            nlohmann::json payload;
            payload["sessionId"] = sessionId;
            Emit("dnd:leave", payload);
        }
        return S_OK;
    } catch (...) {
        return S_OK;
    }
}

HRESULT STDMETHODCALLTYPE DropTargetBridge::Drop(IDataObject* data, DWORD keyState,
                                                 POINTL pt, DWORD* effect) noexcept {
    if (!effect || !data) {
        if (effect) {
            *effect = DROPEFFECT_NONE;
        }
        return E_INVALIDARG;
    }
    const DWORD allowedMask = *effect;
    // Read and cleared before anything can throw, for the same reason as in
    // DragEnter: the catch-all reports S_OK, and a stale flag or id would let the
    // next callback act on a gesture that already ended here.
    const bool wasForwarded = enterForwarded_;
    const std::string sessionId = ClearActiveDrag();

    try {
        if (shuttingDown_) {
            *effect = DROPEFFECT_NONE;
            return S_OK;
        }

        // Drop carries the authoritative list: the source may have changed it
        // since DragEnter.
        bool hadHdrop = false;
        std::vector<std::wstring> paths = ReadHdropPaths(data, &hadHdrop);
        const bool hasFiles = hadHdrop && !paths.empty();

        // Resolving shortcuts is the one step here that can block on the
        // filesystem, so it is done only when its result can be used: an empty
        // session id means no matching DragEnter arrived, and the code below
        // neither stores nor emits anything in that case. Paying a COM budget
        // there would stall the source inside DoDragDrop for nothing.
        //
        // Otherwise Drop usually repeats the list DragEnter already resolved.
        // Reusing that answer when the list is unchanged keeps the worst case (a
        // .lnk on an unreachable share) to one budget per gesture, not two. Only
        // ever our own session: an empty id makes Query answer for the most
        // recently ended gesture, whose targets are not ours to reuse.
        //
        // Gated on the origin verdict for the same reason DragEnter is: with
        // paths withheld the whole array is dropped on the way out, so the work
        // buys nothing and an untrusted document should not be able to order it.
        std::vector<ResolvedTarget> resolved;
        if (!sessionId.empty() && pathsAllowed_) {
            const SessionData* previous = sessions_.Query(sessionId, NowMs());
            if (previous && previous->paths == paths) {
                resolved = previous->resolvedPaths;
            } else {
                resolved = ResolveShortcutTargets(paths);
            }
        }
        sessions_.UpdatePaths(sessionId, paths, hasFiles, resolved);

        const DragPoint point = MakePoint(pt);
        DWORD downstream = allowedMask;
        if (wasForwarded && delegate_ && delegate_->IsValid()) {
            if (FAILED(delegate_->Drop(data, keyState, point, &downstream))) {
                downstream = DROPEFFECT_NONE;
            }
        } else {
            downstream = DROPEFFECT_NONE;
        }

        *effect = ChooseDropEffect(downstream, allowedMask, hasFiles);

        const int64_t now = NowMs();
        sessions_.EndSession(sessionId, now);

        // A drop with no session of ours means no matching DragEnter arrived. An
        // empty sessionId is falsy in the page's staleness check, so emitting it
        // would attach these paths to whichever session the page still holds.
        if (sessionId.empty()) {
            // Nothing of ours handled this drop, so the source must not be told
            // its files were copied.
            *effect = DROPEFFECT_NONE;
            return S_OK;
        }

        nlohmann::json payload;
        payload["sessionId"] = sessionId;
        payload["paths"] = VisiblePaths(paths);
        payload["resolvedPaths"] = VisibleResolvedPaths(paths, resolved);
        payload["x"] = point.client.x;
        payload["y"] = point.client.y;
        payload["keyState"] = static_cast<uint32_t>(keyState);
        Emit("dnd:drop", payload);
        return S_OK;
    } catch (...) {
        *effect = DROPEFFECT_NONE;
        return S_OK;
    }
}

}  // namespace fb2k_dnd
