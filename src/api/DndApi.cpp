// DndApi.cpp - Drag and Drop API
//
// Exposes the host-side drag-drop state to the page:
//   dnd.getPathsAsync    real filesystem paths of a drag session
//   dnd.getCapabilities  what the current host mode can actually deliver
//   dnd.startDrag        dragging out of the window, not implemented
//
// The path side channel lives in the host: CF_HDROP is read by the native
// IDropTarget and the resulting session is kept in host memory. Pages query it
// instead of relying on a snapshot pushed to them, because a fast drag can
// deliver the page's drop event before any push completes.

#include "pch.h"
#include "api/DndApi.h"
#include "api/BridgeCore.h"
#include "api/ErrorEnvelope.h"
#include "core/WebViewContext.h"
#include "core/WebViewPanel.h"
#include "webview/dnd/DndRegistrar.h"
#include "window/WindowManager.h"
#include "window/MainWindow.h"
#include "window/PopupWindow.h"

namespace {
    using json = nlohmann::json;

    int64_t NowMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    // Resolves the DnD registrar of the instance that issued this call.
    //
    // Every lookup below compares HWNDs for exact equality and never promotes to
    // GA_ROOT. Promotion would collapse all DUI panels sharing the foobar2000
    // frame into one window and hand a panel another panel's paths. For the same
    // reason there is no fallback to "some other window" when nothing matches.
    //
    // Two lookups are needed because the two registration paths differ:
    //
    //   - DUI and CUI panels pass themselves to RegisterInstance, so
    //     WebViewContext can return the panel directly.
    //   - Standalone windows and popups register through the overload that
    //     leaves that pointer null, so they are unreachable that way and are
    //     found through WindowManager instead.
    //
    // Populating the pointer for standalone windows would be the smaller change
    // here, but WebViewContext's panel pointer is also what marks a caller as a
    // DUI/CUI panel elsewhere; filling it in for every window would silently
    // change window.getMode, panel.getConfig and panel.setConfig.
    fb2k_dnd::DndRegistrar* ResolveCallerRegistrar(const json& params) {
        if (!params.contains("_callerHwnd") || !params["_callerHwnd"].is_number_integer()) {
            return nullptr;
        }
        auto hwnd = reinterpret_cast<HWND>(params["_callerHwnd"].get<intptr_t>());
        if (!hwnd || !::IsWindow(hwnd)) {
            return nullptr;
        }

        if (auto* panel = WebViewContext::GetInstance().GetPanelByHwnd(hwnd)) {
            return panel->GetDndRegistrar();
        }

        auto& wm = WindowManager::GetInstance();
        if (auto* mainWin = wm.GetMainWindow(); mainWin && mainWin->GetHwnd() == hwnd) {
            return mainWin->GetDndRegistrar();
        }
        for (const auto& id : wm.GetAllWindowIds()) {
            if (id == "main") continue;
            auto* popup = wm.GetPopup(id);
            if (popup && popup->GetHwnd() == hwnd) {
                return popup->GetDndRegistrar();
            }
        }
        return nullptr;
    }

    //==========================================================================
    // dnd.getPathsAsync - Query the host for a drag session's real paths
    //
    // Params:
    //   sessionId (string, optional) - omit to query the session that is active
    //                                  or most recently ended for this window.
    //
    // Reliable source of paths for a page: it reads host memory, so it does not
    // depend on the delivery order of dnd:* messages, and it stays usable after
    // an await because it never touches event.dataTransfer. Reports an empty
    // array, not a failure, when the session expired or carried no file list.
    //==========================================================================
    json DndGetPathsAsync(const json& params) {
        auto* registrar = ResolveCallerRegistrar(params);
        if (!registrar) {
            return ApiEnvelope::MakeError(
                "Caller window has no drag-drop registration.",
                ApiErrorCode::NOT_FOUND);
        }

        auto* bridge = registrar->Bridge();
        const std::string requested = params.value("sessionId", std::string());
        const fb2k_dnd::SessionData* session =
            bridge ? bridge->Sessions().Query(requested, NowMs()) : nullptr;
        if (!session) {
            return {
                {"success", true},
                {"sessionId", std::string()},
                {"paths", json::array()}
            };
        }

        // Same gate as the event payloads: when the document origin is not
        // trusted for the path side channel the session id is still reported,
        // since it reveals nothing, but the paths are withheld.
        json paths = json::array();
        if (bridge->PathsAllowed()) {
            for (const std::wstring& path : session->paths) {
                paths.push_back(WideToUtf8(path));
            }
        }

        return {
            {"success", true},
            {"sessionId", session->sessionId},
            {"paths", paths}
        };
    }

    //==========================================================================
    // dnd.getCapabilities - What the current host mode can deliver
    //
    // html5 and paths are independent: a panel-mode host can lose the path side
    // channel while HTML5 drag events keep working, because Chromium handles
    // those itself. pathsUnavailableReason is present only when paths is false.
    //==========================================================================
    json DndGetCapabilities(const json& params) {
        auto* registrar = ResolveCallerRegistrar(params);
        if (!registrar) {
            return ApiEnvelope::MakeError(
                "Caller window has no drag-drop registration.",
                ApiErrorCode::NOT_FOUND);
        }

        const fb2k_dnd::DndCapabilities caps = registrar->Capabilities();
        const char* hosting = caps.visualHosting ? "visual" : "standard";
        const char* reason = fb2k_dnd::ReasonToWire(caps.reason);

        if (!reason) {
            return {
                {"success", true},
                {"html5", caps.html5},
                {"paths", caps.paths},
                {"hosting", hosting}
            };
        }

        return {
            {"success", true},
            {"html5", caps.html5},
            {"paths", caps.paths},
            {"hosting", hosting},
            {"pathsUnavailableReason", reason}
        };
    }

    //==========================================================================
    // dnd.startDrag - Not implemented
    //
    // Dragging content out of the window needs an IDropSource implementation
    // and a host-produced data object; neither exists. Reported as an explicit
    // failure so callers cannot build on a fake success response.
    //==========================================================================
    json DndStartDrag(const json& /*params*/) {
        return ApiEnvelope::MakeError(
            "Dragging out of the window is not implemented; it requires an "
            "IDropSource implementation.",
            ApiErrorCode::NOT_SUPPORTED);
    }

} // anonymous namespace

//==========================================================================
// Register Drag-and-Drop API
//==========================================================================
void RegisterDndApi() {
    auto& bridge = BridgeCore::GetInstance();

    // dnd.getPathsAsync - Real paths of a drag session, queried from the host
    bridge.RegisterApi("dnd.getPathsAsync", DndGetPathsAsync);

    // dnd.getCapabilities - Host drag-drop capability for the calling window
    bridge.RegisterApi("dnd.getCapabilities", DndGetCapabilities);

    // dnd.startDrag - Always reports NOT_SUPPORTED
    bridge.RegisterApi("dnd.startDrag", DndStartDrag);

    LOG("Drag-and-Drop API registered (3 APIs)");
}
