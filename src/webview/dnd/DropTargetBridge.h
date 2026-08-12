// DropTargetBridge.h - the IDropTarget we register on the host window.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <oleidl.h>
#include <nlohmann/json.hpp>

#include "webview/dnd/DragSession.h"
#include "webview/dnd/IDropTargetDelegate.h"

namespace fb2k_dnd {

// Reads CF_HDROP for the path side channel, then forwards the very same
// IDataObject downstream so the page still gets standard HTML5 drag events.
//
// The IDataObject handed to these callbacks is borrowed: it is used only for
// the duration of the call, never Release()d, and DragFinish is never called
// (that is for WM_DROPFILES HDROPs).
class DropTargetBridge : public IDropTarget {
public:
    // Emits dnd:* events for this window only. Never broadcast: paths are
    // sensitive and other windows must not observe this drag.
    using EventSink = std::function<void(const char* event, const nlohmann::json&)>;

    DropTargetBridge(HWND target, std::unique_ptr<IDropTargetDelegate> delegate,
                     EventSink sink);

    // Stops forwarding and event emission. Called before teardown so a drag
    // in flight cannot reach a half-destroyed WebView.
    void BeginShutdown();

    // Puts back whatever the delegate displaced to make room for this bridge, so
    // the window is never left without the drop target it started with. A no-op
    // unless the delegate chains. Only valid once this bridge's own registration
    // has been revoked, since the window can hold only one drop target.
    void RestoreDisplacedTarget();

    // Whether the page may see real filesystem paths. When false, dnd:enter and
    // dnd:drop report an empty paths array while hasFiles stays accurate, since
    // that flag reveals nothing about the filesystem. Sessions keep the real
    // paths so the gate can reopen on navigation without losing the drag.
    void SetPathsAllowed(bool allowed) { pathsAllowed_ = allowed; }
    bool PathsAllowed() const { return pathsAllowed_; }

    // Reports a capability change to this window. Exposed so the registrar can
    // reach the same point-to-point sink the drag events use instead of keeping
    // a second one. Silent once shutdown has begun.
    void EmitCapabilitiesChanged(const nlohmann::json& payload) const;

    DragSessionStore& Sessions() { return sessions_; }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) noexcept override;
    ULONG   STDMETHODCALLTYPE AddRef() noexcept override;
    ULONG   STDMETHODCALLTYPE Release() noexcept override;

    // IDropTarget. noexcept is load-bearing, not documentation: an exception
    // escaping here would unwind into OLE across the COM ABI, so each method
    // catches internally and reports DROPEFFECT_NONE with S_OK instead.
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data, DWORD keyState, POINTL pt,
                                        DWORD* effect) noexcept override;
    HRESULT STDMETHODCALLTYPE DragOver(DWORD keyState, POINTL pt,
                                       DWORD* effect) noexcept override;
    HRESULT STDMETHODCALLTYPE DragLeave() noexcept override;
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* data, DWORD keyState, POINTL pt,
                                   DWORD* effect) noexcept override;

private:
    // Fills both coordinate spaces; each delegate picks the one it needs.
    DragPoint MakePoint(POINTL screen) const;

    // The paths array as the page is allowed to see it: the real list when the
    // origin gate is open, an empty array otherwise. Single choke point so no
    // event payload can bypass the gate.
    nlohmann::json VisiblePaths(const std::vector<std::wstring>& paths) const;

    // Emits through sink_ if one was supplied, swallowing sink failures so a
    // faulty listener cannot break the drag.
    void Emit(const char* event, const nlohmann::json& payload) const;

    // Ends this gesture locally and returns the session id it held. The flag and
    // the id must move together, or Query answers for a gesture already over.
    std::string ClearActiveDrag() noexcept;

    LONG refCount_ = 1;
    HWND target_ = nullptr;
    std::unique_ptr<IDropTargetDelegate> delegate_;
    EventSink sink_;
    DragSessionStore sessions_;
    std::string activeSessionId_;
    bool enterForwarded_ = false;
    bool shuttingDown_ = false;
    bool pathsAllowed_ = false;
};

}  // namespace fb2k_dnd
