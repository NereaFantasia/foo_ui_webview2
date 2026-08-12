// DropTargetBridge.cpp
#include "pch.h"
#include "webview/dnd/DropTargetBridge.h"

#include "api/BridgeCore.h"
#include "webview/dnd/DropEffectPolicy.h"
#include "webview/dnd/HdropReader.h"

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
        const int64_t now = NowMs();
        activeSessionId_ = sessions_.BeginSession(paths, hasFiles, now);

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
        sessions_.UpdatePaths(sessionId, paths, hasFiles);

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
