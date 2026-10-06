// DropTargetBridge.cpp
#include "pch.h"
#include "webview/dnd/DropTargetBridge.h"

#include "api/BridgeCore.h"
#include "api/generated/DndSchema.h"
#include "webview/dnd/DndTrace.h"
#include "webview/dnd/DragSourceMarker.h"
#include "webview/dnd/DropEffectPolicy.h"
#include "webview/dnd/HdropReader.h"
#include "webview/dnd/ShortcutResolver.h"

#include <sstream>

namespace fb2k_dnd {
namespace {

int64_t NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::vector<std::string> PathsToUtf8(const std::vector<std::wstring>& paths) {
    std::vector<std::string> out;
    out.reserve(paths.size());
    for (const std::wstring& path : paths) {
        out.push_back(WideToUtf8(path));
    }
    return out;
}

// The parallel shortcut-target array, always exactly as long as paths.
//
// Driven by the paths list rather than by resolved, so the equal-length part of
// the contract holds by construction: a resolver that returned a short array,
// or none at all, still yields one element per path. An unknown target is left
// empty and goes out as null, never as an empty string, since "" would read as
// a real path of zero length to a page that only checks for truthiness.
std::vector<std::optional<std::string>> ResolvedPathsToUtf8(
    const std::vector<std::wstring>& paths,
    const std::vector<ResolvedTarget>& resolved) {
    std::vector<std::optional<std::string>> out;
    out.reserve(paths.size());
    for (size_t i = 0; i < paths.size(); ++i) {
        if (i < resolved.size() && resolved[i].has_value()) {
            out.emplace_back(WideToUtf8(*resolved[i]));
        } else {
            out.emplace_back(std::nullopt);
        }
    }
    return out;
}

// One line of the dnd trace (DndTrace.h) for a drag callback. It records what
// the WebView answered (inner) and what went back to the drag source (out), to
// measure how soon and how reliably the renderer's answer reaches DragOver, and
// how long each forwarded call took, to find a callback that holds up the drag
// source. sinceEnter is -1 when no session of ours is active; inner means
// nothing when forwarded is false.
struct EffectTrace {
    const char* phase = "";
    HWND target = nullptr;
    int64_t sinceEnterMs = -1;
    bool hasFiles = false;
    bool forwarded = false;
    DWORD allowed = 0;
    DWORD inner = 0;
    DWORD out = 0;
    int64_t forwardMs = -1;  // time spent in the WebView call, -1 when not made
    std::string extra;       // phase-specific fields, already formatted
};

std::string HwndText(HWND hwnd) {
    std::ostringstream text;
    text << "0x" << std::hex << reinterpret_cast<uintptr_t>(hwnd);
    return text.str();
}

void TraceEffect(const EffectTrace& t) noexcept {
    if (!DndTraceEnabled()) {
        return;
    }
    try {
        std::ostringstream line;
        line << "[dnd] " << t.phase << " target=" << HwndText(t.target)
             << " t=" << t.sinceEnterMs << "ms files=" << (t.hasFiles ? 1 : 0)
             << " forwarded=" << (t.forwarded ? 1 : 0) << std::hex
             << " allowed=0x" << t.allowed << " inner=0x" << t.inner << " out=0x" << t.out
             << std::dec << " forwardMs=" << t.forwardMs;
        if (!t.extra.empty()) {
            line << ' ' << t.extra;
        }
        DndTrace(line.str());
    } catch (...) {
        // Diagnostics must never disturb the drag.
    }
}

// A single DragOver that took this long is written out even when its answer
// did not change: the drag source is blocked for the whole call.
constexpr int64_t kSlowOverMs = 100;

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

std::vector<std::string> DropTargetBridge::VisiblePaths(
    const std::vector<std::wstring>& paths) const {
    if (!pathsAllowed_) {
        return {};
    }
    return PathsToUtf8(paths);
}

std::vector<std::optional<std::string>> DropTargetBridge::VisibleResolvedPaths(
    const std::vector<std::wstring>& paths,
    const std::vector<ResolvedTarget>& resolved) const {
    if (!pathsAllowed_) {
        // Empty, not a list of nulls: VisiblePaths withholds the whole array in
        // this case, and the two must stay the same length. A null-filled array
        // would also leak the file count, which the empty one does not.
        return {};
    }
    return ResolvedPathsToUtf8(paths, resolved);
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

template <class E>
void DropTargetBridge::Emit(const typename E::Payload& payload) const {
    Emit(E::kName, ToJson(payload));
}

void DropTargetBridge::EmitCapabilitiesChanged(
    const api::dnd::CapabilitiesChangedPayload& payload) const {
    if (shuttingDown_) {
        return;
    }
    Emit<api::dnd::events::CapabilitiesChanged>(payload);
}

void DropTargetBridge::EmitDragEnded(const api::dnd::DragEndedPayload& payload) const {
    if (shuttingDown_) {
        return;
    }
    Emit<api::dnd::events::DragEnded>(payload);
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
        overTrace_ = OverTrace{};
        overForward_ = DragOverForwardState{};
        sawRendererEffect_ = false;
        const int64_t readStartMs = DndTraceNowMs();
        // A source may re-enter without a matching leave, and BeginSession
        // supersedes the previous session so no stale one survives into this drag.
        bool hadHdrop = false;
        std::vector<std::wstring> paths = ReadHdropPaths(data, &hadHdrop);
        const bool hasFiles = hadHdrop && !paths.empty();
        const int64_t readMs = DndTraceNowMs() - readStartMs;
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
        const int64_t resolveStartMs = DndTraceNowMs();
        std::vector<ResolvedTarget> resolved;
        if (pathsAllowed_) {
            resolved = ResolveShortcutTargets(paths);
        }
        const int64_t resolveMs = DndTraceNowMs() - resolveStartMs;
        // Compared against target_, the window a hosted drag stamps as its own:
        // in Visual Hosting both are the host window, and in a chained panel the
        // Chromium child never matches, since panels do not stamp their drags.
        const int64_t markerStartMs = DndTraceNowMs();
        const DragSource source =
            ClassifyDragSource(ReadDragSourceMarker(data), ::GetCurrentProcessId(),
                               static_cast<uint64_t>(reinterpret_cast<uintptr_t>(target_)));
        const int64_t markerMs = DndTraceNowMs() - markerStartMs;
        const int64_t now = NowMs();
        activeSessionId_ = sessions_.BeginSession(paths, hasFiles, now, resolved, source);

        if (shuttingDown_) {
            *effect = DROPEFFECT_NONE;
            return S_OK;
        }

        const DragPoint point = MakePoint(pt);
        DWORD downstream = allowedMask;
        int64_t forwardMs = -1;
        if (delegate_ && delegate_->IsValid()) {
            // Written before the call, so a call that never returns still shows.
            if (DndTraceEnabled()) {
                DndTrace("[dnd] enter.forward target=" + HwndText(target_));
            }
            const int64_t forwardStartMs = DndTraceNowMs();
            if (SUCCEEDED(delegate_->Enter(data, keyState, point, &downstream))) {
                enterForwarded_ = true;
                RecordForwardedDragOver(overForward_, pt, keyState, NowMs(), downstream);
            } else {
                downstream = DROPEFFECT_NONE;
            }
            forwardMs = DndTraceNowMs() - forwardStartMs;
        } else {
            downstream = DROPEFFECT_NONE;
        }

        sawRendererEffect_ = downstream != DROPEFFECT_NONE;
        *effect = ChooseDropEffect(downstream, allowedMask, hasFiles,
                                   RendererHasAnswered(sawRendererEffect_, 0));
        if (DndTraceEnabled()) {
            EffectTrace trace;
            trace.phase = "enter";
            trace.target = target_;
            trace.sinceEnterMs = 0;
            trace.hasFiles = hasFiles;
            trace.forwarded = enterForwarded_;
            trace.allowed = allowedMask;
            trace.inner = downstream;
            trace.out = *effect;
            trace.forwardMs = forwardMs;
            trace.extra = "paths=" + std::to_string(paths.size()) +
                          " source=" + DragSourceToWire(source) +
                          " readMs=" + std::to_string(readMs) +
                          " resolveMs=" + std::to_string(resolveMs) +
                          " markerMs=" + std::to_string(markerMs);
            TraceEffect(trace);
        }

        api::dnd::EnterPayload payload;
        payload.sessionId = activeSessionId_;
        payload.paths = VisiblePaths(paths);
        payload.resolvedPaths = VisibleResolvedPaths(paths, resolved);
        payload.hasFiles = hasFiles;
        payload.source = DragSourceToWire(source);
        payload.x = point.client.x;
        payload.y = point.client.y;
        Emit<api::dnd::events::Enter>(payload);
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
        const int64_t now = NowMs();
        const SessionData* session =
            activeSessionId_.empty() ? nullptr : sessions_.Query(activeSessionId_, now);
        const bool hasFiles = session && session->hasFiles;

        DWORD downstream = allowedMask;
        const bool forwarded = enterForwarded_ && delegate_ && delegate_->IsValid();
        int64_t forwardMs = -1;
        if (forwarded) {
            if (ShouldForwardDragOver(overForward_, pt, keyState, now)) {
                const DragPoint point = MakePoint(pt);
                const int64_t forwardStartMs = DndTraceNowMs();
                if (FAILED(delegate_->Over(keyState, point, &downstream))) {
                    downstream = DROPEFFECT_NONE;
                }
                forwardMs = DndTraceNowMs() - forwardStartMs;
                RecordForwardedDragOver(overForward_, pt, keyState, now, downstream);
                ++overTrace_.sent;
            } else {
                // The WebView still has the drag; its last answer stands.
                downstream = overForward_.lastAnswer;
            }
        } else {
            downstream = DROPEFFECT_NONE;
        }

        if (downstream != DROPEFFECT_NONE) {
            sawRendererEffect_ = true;
        }
        const bool answered = RendererHasAnswered(
            sawRendererEffect_, session ? now - session->startedAtMs : 0);
        *effect = ChooseDropEffect(downstream, allowedMask, hasFiles, answered);
        if (DndTraceEnabled()) {
            OverTrace& stats = overTrace_;
            ++stats.calls;
            stats.maxCallMs = (std::max)(stats.maxCallMs, forwardMs);
            // A change of the answer to the drag source covers the grace period
            // running out, which turns a pending NONE into a refusal.
            const bool changed = !stats.haveLast || stats.lastForwarded != forwarded ||
                                 stats.lastInner != downstream || stats.lastOut != *effect;
            if (changed || forwardMs >= kSlowOverMs) {
                EffectTrace trace;
                trace.phase = changed ? "over" : "over.slow";
                trace.target = target_;
                trace.sinceEnterMs = session ? now - session->startedAtMs : -1;
                trace.hasFiles = hasFiles;
                trace.forwarded = forwarded;
                trace.allowed = allowedMask;
                trace.inner = downstream;
                trace.out = *effect;
                trace.forwardMs = forwardMs;
                trace.extra = "call=" + std::to_string(stats.calls) +
                              " sent=" + std::to_string(stats.sent) +
                              " answered=" + (answered ? "1" : "0");
                TraceEffect(trace);
            }
            stats.haveLast = true;
            stats.lastForwarded = forwarded;
            stats.lastInner = downstream;
            stats.lastOut = *effect;
        }
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
        int64_t forwardMs = -1;
        if (wasForwarded && !shuttingDown_ && delegate_ && delegate_->IsValid()) {
            const int64_t forwardStartMs = DndTraceNowMs();
            delegate_->Leave();
            forwardMs = DndTraceNowMs() - forwardStartMs;
        }
        if (DndTraceEnabled()) {
            const SessionData* session =
                sessionId.empty() ? nullptr : sessions_.Query(sessionId, NowMs());
            EffectTrace trace;
            trace.phase = "leave";
            trace.target = target_;
            trace.sinceEnterMs = session ? NowMs() - session->startedAtMs : -1;
            trace.hasFiles = session && session->hasFiles;
            trace.forwarded = wasForwarded;
            trace.forwardMs = forwardMs;
            trace.extra = "overCalls=" + std::to_string(overTrace_.calls) +
                          " overSent=" + std::to_string(overTrace_.sent) +
                          " overMaxMs=" + std::to_string(overTrace_.maxCallMs);
            TraceEffect(trace);
        }

        if (sessionId.empty()) {
            return S_OK;
        }
        sessions_.EndSession(sessionId, NowMs());
        if (!shuttingDown_) {
            api::dnd::LeavePayload payload;
            payload.sessionId = sessionId;
            Emit<api::dnd::events::Leave>(payload);
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
        const int64_t readStartMs = DndTraceNowMs();
        bool hadHdrop = false;
        std::vector<std::wstring> paths = ReadHdropPaths(data, &hadHdrop);
        const bool hasFiles = hadHdrop && !paths.empty();
        const int64_t readMs = DndTraceNowMs() - readStartMs;

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
        const int64_t resolveStartMs = DndTraceNowMs();
        std::vector<ResolvedTarget> resolved;
        if (!sessionId.empty() && pathsAllowed_) {
            const SessionData* previous = sessions_.Query(sessionId, NowMs());
            if (previous && previous->paths == paths) {
                resolved = previous->resolvedPaths;
            } else {
                resolved = ResolveShortcutTargets(paths);
            }
        }
        const int64_t resolveMs = DndTraceNowMs() - resolveStartMs;
        sessions_.UpdatePaths(sessionId, paths, hasFiles, resolved);

        const DragPoint point = MakePoint(pt);
        DWORD downstream = allowedMask;
        int64_t forwardMs = -1;
        if (wasForwarded && delegate_ && delegate_->IsValid()) {
            // Written before the call, so a drop that never returns still shows.
            if (DndTraceEnabled()) {
                DndTrace("[dnd] drop.forward target=" + HwndText(target_));
            }
            const int64_t forwardStartMs = DndTraceNowMs();
            if (FAILED(delegate_->Drop(data, keyState, point, &downstream))) {
                downstream = DROPEFFECT_NONE;
            }
            forwardMs = DndTraceNowMs() - forwardStartMs;
        } else {
            downstream = DROPEFFECT_NONE;
        }

        const int64_t now = NowMs();
        if (downstream != DROPEFFECT_NONE) {
            sawRendererEffect_ = true;
        }
        const SessionData* dropped = sessionId.empty() ? nullptr : sessions_.Query(sessionId, now);
        // A drop released before the page answered keeps the optimistic COPY: the
        // page may still take it, and dnd:drop below reports it either way.
        *effect = ChooseDropEffect(
            downstream, allowedMask, hasFiles,
            RendererHasAnswered(sawRendererEffect_, dropped ? now - dropped->startedAtMs : 0));

        sessions_.EndSession(sessionId, now);
        const SessionData* ended = sessionId.empty() ? nullptr : sessions_.Query(sessionId, now);
        const DragSource source = ended ? ended->source : DragSource::External;

        // A drop with no session of ours means no matching DragEnter arrived. An
        // empty sessionId is falsy in the page's staleness check, so emitting it
        // would attach these paths to whichever session the page still holds.
        if (sessionId.empty()) {
            // Nothing of ours handled this drop, so the source must not be told
            // its files were copied.
            *effect = DROPEFFECT_NONE;
        }
        if (DndTraceEnabled()) {
            EffectTrace trace;
            trace.phase = "drop";
            trace.target = target_;
            trace.sinceEnterMs = ended ? now - ended->startedAtMs : -1;
            trace.hasFiles = hasFiles;
            trace.forwarded = wasForwarded;
            trace.allowed = allowedMask;
            trace.inner = downstream;
            trace.out = *effect;
            trace.forwardMs = forwardMs;
            trace.extra = "paths=" + std::to_string(paths.size()) +
                          " source=" + DragSourceToWire(source) +
                          " readMs=" + std::to_string(readMs) +
                          " resolveMs=" + std::to_string(resolveMs) +
                          " overCalls=" + std::to_string(overTrace_.calls) +
                          " overSent=" + std::to_string(overTrace_.sent) +
                          " overMaxMs=" + std::to_string(overTrace_.maxCallMs);
            TraceEffect(trace);
        }
        if (sessionId.empty()) {
            return S_OK;
        }

        api::dnd::DropPayload payload;
        payload.sessionId = sessionId;
        payload.paths = VisiblePaths(paths);
        payload.resolvedPaths = VisibleResolvedPaths(paths, resolved);
        payload.x = point.client.x;
        payload.y = point.client.y;
        payload.keyState = static_cast<std::int64_t>(keyState);
        payload.source = DragSourceToWire(source);
        const int64_t emitStartMs = DndTraceNowMs();
        Emit<api::dnd::events::Drop>(payload);
        if (DndTraceEnabled()) {
            DndTrace("[dnd] drop.emitted target=" + HwndText(target_) +
                     " emitMs=" + std::to_string(DndTraceNowMs() - emitStartMs));
        }
        return S_OK;
    } catch (...) {
        *effect = DROPEFFECT_NONE;
        return S_OK;
    }
}

}  // namespace fb2k_dnd
