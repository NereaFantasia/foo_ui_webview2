// DndApi.cpp - Drag and Drop API
//
// Exposes the host-side drag-drop state to the page:
//   dnd.getPathsAsync    real filesystem paths of a drag session
//   dnd.getCapabilities  what the current host mode can actually deliver
//   dnd.prepareDrag      exchange validated paths for a one-shot drag token
//   dnd.startDrag        dragging out of the window, not implemented
//
// The path side channel lives in the host: CF_HDROP is read by the native
// IDropTarget and the resulting session is kept in host memory. Pages query it
// instead of relying on a snapshot pushed to them, because a fast drag can
// deliver the page's drop event before any push completes.
//
// Shapes are declared in src/api/schema/dnd.ts; the structs and the parameter
// reader come from the generated DndSchema.h.

#include "pch.h"
#include "api/DndApi.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "api/ErrorEnvelope.h"
#include "api/TypedApi.h"
#include "api/generated/DndSchema.h"
#include "settings/SecurityConfig.h"
#include "core/WebViewContext.h"
#include "core/WebViewPanel.h"
#include "utils/PathExpansion.h"
#include "webview/WebViewHost.h"
#include "webview/dnd/DndOriginPolicy.h"
#include "webview/dnd/DndRegistrar.h"
#include "webview/dnd/DragOutPathPolicy.h"
#include "webview/dnd/DragOutTokens.h"
#include "webview/dnd/DragSourceMarker.h"
#include "window/WindowManager.h"
#include "window/MainWindow.h"
#include "window/PopupWindow.h"

namespace {
    using json = nlohmann::json;
    namespace dnd = api::dnd;

    int64_t NowMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    // Resolves the DnD registrar of the instance that issued this call.
    //
    // Every lookup below compares HWNDs for exact equality and never promotes to
    // GA_ROOT. Promotion would collapse all DUI panels sharing the foobar2000
    // frame into one window and hand a panel another panel's paths. For the same
    // reason there is no fallback to "some other window" when nothing matches,
    // and only the caller's own HWND is used: CallerContext's bridge and windowId
    // resolve through the top-level window and would reintroduce that collapse.
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
    fb2k_dnd::DndRegistrar* ResolveCallerRegistrar(HWND hwnd) {
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

    api::Failure NoRegistration() {
        return api::Fail("Caller window has no drag-drop registration.", ApiErrorCode::NOT_FOUND);
    }

    //==========================================================================
    // dnd.getPathsAsync - Query the host for a drag session's real paths
    //
    // Reliable source of paths for a page: it reads host memory, so it does not
    // depend on the delivery order of dnd:* messages, and it stays usable after
    // an await because it never touches event.dataTransfer. Reports an empty
    // array, not a failure, when the session expired or carried no file list.
    //
    // resolvedPaths is the parallel shortcut-target array, always the same
    // length as paths. No shell work happens here: the targets were read once
    // by the drop target and stored with the session, so a page may query this
    // as often as it likes without touching the filesystem.
    //==========================================================================
    api::Result<dnd::GetPathsAsyncResult> DndGetPathsAsync(const dnd::GetPathsAsyncParams& p,
                                                           const CallerContext& caller) {
        auto* registrar = ResolveCallerRegistrar(caller.callerHwnd);
        if (!registrar) return NoRegistration();

        auto* bridge = registrar->Bridge();
        const std::string requested = p.sessionId.value_or("");
        const fb2k_dnd::SessionData* session =
            bridge ? bridge->Sessions().Query(requested, NowMs()) : nullptr;

        dnd::GetPathsAsyncResult result;
        if (!session) {
            return result;
        }

        // Same gate as the event payloads: when the document origin is not
        // trusted for the path side channel the session id is still reported,
        // since it reveals nothing, but the paths are withheld. A shortcut
        // target is a real path, so it is withheld by the same test, and both
        // arrays stay empty together rather than one turning into nulls.
        result.sessionId = session->sessionId;
        result.source = fb2k_dnd::DragSourceToWire(session->source);
        if (bridge->PathsAllowed()) {
            // Indexed off paths so the two arrays match in length whatever the
            // session holds, which is what lets a page pair them by index.
            for (size_t i = 0; i < session->paths.size(); ++i) {
                result.paths.push_back(WideToUtf8(session->paths[i]));
                const bool known = i < session->resolvedPaths.size() &&
                                   session->resolvedPaths[i].has_value();
                if (known) {
                    result.resolvedPaths.emplace_back(WideToUtf8(*session->resolvedPaths[i]));
                } else {
                    result.resolvedPaths.emplace_back(std::nullopt);
                }
            }
        }
        return result;
    }

    //==========================================================================
    // dnd.getCapabilities - What the current host mode can deliver
    //
    // html5, paths and dragOut are independent. A panel-mode host can lose the
    // path side channel while HTML5 drag events keep working, because Chromium
    // handles those itself; dragOut additionally depends on a WebView2 runtime
    // feature, so it can be missing on a host where paths work fine. Each
    // *UnavailableReason key is present only when its own capability is false.
    //==========================================================================
    api::Result<dnd::GetCapabilitiesResult> DndGetCapabilities(const dnd::GetCapabilitiesParams&,
                                                               const CallerContext& caller) {
        auto* registrar = ResolveCallerRegistrar(caller.callerHwnd);
        if (!registrar) return NoRegistration();

        // The same filling as the dnd:capabilitiesChanged payload, so the two
        // cannot drift apart.
        dnd::GetCapabilitiesResult result;
        fb2k_dnd::FillCapabilities(registrar->Capabilities(), result);
        return result;
    }

    // "file-relative://" is the form a portable install emits for media on the
    // same volume as the program; what it is relative to is fb2k's business, so
    // only filesystem::g_get_native_path can turn it into a real path. This is
    // the same call the library API's absolutePath field goes through, and it is
    // the one scheme other than file:// that this endpoint resolves. The result
    // is run through extract_native_path afterwards so anything that is not a
    // plain filesystem path is still rejected.
    bool ResolveFileRelative(const char* in, pfc::string8& native) {
        if (strncmp(in, "file-relative://", 16) != 0) {
            return false;
        }
        pfc::string8 resolved;
        try {
            if (!filesystem::g_get_native_path(in, resolved)) {
                return false;
            }
        } catch (...) {
            return false;
        }
        return !resolved.is_empty() &&
               foobar2000_io::extract_native_path(resolved.get_ptr(), native);
    }

    // Maps one page-supplied location to a native filesystem path, using the
    // very function foobar2000 itself uses, so this endpoint agrees with the app
    // about what "the file behind this track" means.
    //
    // The plain variant, not the _ex one: _ex resolves every unknown scheme
    // through filesystem_v3::getNativePath, which may land in a third-party
    // component doing I/O, and this runs on the main thread where
    // ApiPerfGuard's blocking threshold is 500 ms. The one scheme let through
    // to the core's own resolver is file-relative://, either bare or as the
    // container of an unpack:// entry; a portable install produces exactly that
    // for archived tracks, and the page has no other field to hand in.
    //
    // Variables are expanded here rather than left to the validator, which
    // expands internally before checking: the token carries whatever this
    // returns, and the string that was validated has to be the string that ends
    // up in CF_HDROP. SDK normalisation passes anything without a scheme through
    // untouched, so a literal %profile%\... would otherwise survive to the token
    // while a different, expanded path was the one approved.
    bool ResolveNativePath(const std::wstring& input, std::wstring& native) {
        const std::wstring expanded = PathExpansion::Expand(input);
        const std::string utf8 = WideToUtf8(expanded);
        pfc::string8 resolved;
        if (!foobar2000_io::extract_native_path_archive_aware(utf8.c_str(), resolved)) {
            pfc::string8 container;
            const char* candidate = utf8.c_str();
            if (archive_impl::g_is_unpack_path(candidate)) {
                pfc::string8 member;
                if (!archive_impl::g_parse_unpack_path(candidate, container, member)) {
                    return false;
                }
                candidate = container.get_ptr();
            }
            if (!ResolveFileRelative(candidate, resolved)) {
                return false;
            }
        }
        native = Utf8ToWide(resolved.get_ptr());
        return true;
    }

    //==========================================================================
    // dnd.prepareDrag - Exchange validated paths for a one-shot drag token
    //
    // Returns { success: true, token } on success. The page puts that token in
    // dataTransfer during dragstart; the host redeems it for these paths when the
    // drag actually starts, so no path ever travels through the page at drag
    // time.
    //
    // The token must be obtained BEFORE dragstart. A dataTransfer store is only
    // writable during the synchronous part of that handler, so awaiting this call
    // inside dragstart makes the later setData fail silently.
    //
    // Container semantics: what lands in CF_HDROP is always a physical file. A
    // track inside a cue or a multi-subsong container drags the whole container,
    // and an archive:// or unpack:// entry drags the whole archive, so several
    // requested entries can collapse into one file.
    //==========================================================================
    api::Result<dnd::PrepareDragResult> DndPrepareDrag(const dnd::PrepareDragParams& p,
                                                       const CallerContext& caller) {
        // The order of the checks below is deliberate and load-bearing; the note
        // at the registration site explains why the declaration gives `paths` no
        // @security. In short: the generated reader has only checked the shape
        // of the caller's own JSON by now, and the origin decision must happen
        // before any path is resolved or validated, which the declarative
        // wrapper would do first.
        auto* registrar = ResolveCallerRegistrar(caller.callerHwnd);
        if (!registrar) return NoRegistration();

        // 1. Origin, decided live rather than read from the per-window cached
        //    flag, which only refreshes on registration and NavigationCompleted:
        //    a document that navigated to an untrusted origin can run script
        //    before that refresh lands. The registrar does not hold the host, so
        //    the host is resolved from the caller window again.
        //
        //    Nothing below has resolved a path yet, and nothing must until this
        //    passes: MediaRead falls back to a media-library membership test, so
        //    validating first would turn the endpoint into a probe for what is
        //    in the user's library.
        const HWND callerHwnd = caller.callerHwnd;
        WebViewHost* host = WebViewContext::GetInstance().GetWebViewHost(callerHwnd);
        const std::wstring origin =
            host ? host->GetCurrentOriginNormalized() : std::wstring();
        if (!fb2k_dnd::AllowsPaths(origin, WebViewHost::CurrentDevServerOrigin())) {
            return api::Fail("Document origin is not trusted for dragging files out.",
                             ApiErrorCode::ORIGIN_DENIED);
        }

        // 2. Capability, so a page on a runtime without the drag-start event is
        //    told now instead of after a drag that does nothing.
        if (!registrar->Capabilities().dragOut) {
            return api::Fail("Dragging out is not available for this window.",
                             ApiErrorCode::NOT_SUPPORTED);
        }

        // 3. Normalise first, then validate the normalised result. Doing it the
        //    other way round lets the validated string and the string that ends
        //    up in CF_HDROP be different things: MediaRead lets any unknown
        //    protocol through untouched, and it validates an archive entry by its
        //    container while the original archive:// string would have been the
        //    one dragged.
        std::vector<std::wstring> requested;
        requested.reserve(p.paths.size());
        for (const auto& entry : p.paths) {
            requested.push_back(Utf8ToWide(entry));
        }
        const fb2k_dnd::DragOutPlan plan =
            fb2k_dnd::BuildDragOutPlan(requested, ResolveNativePath);
        if (!plan.ok) {
            if (plan.reason == fb2k_dnd::DragOutReject::EmptyList) {
                // Unreachable from a page: the reader enforces @minItems 1.
                return api::Fail("dnd.prepareDrag: 'paths' must contain at least one entry.",
                                 ApiErrorCode::INVALID_PARAMS);
            }
            // Position and cause only. The path itself must never appear in an
            // error payload, because a page that cannot be trusted with paths
            // must not learn them from a rejection either.
            return api::Fail(
                "dnd.prepareDrag: paths[" + std::to_string(plan.rejectedIndex) +
                    "] has no local file that can be dragged out.",
                ApiErrorCode::INVALID_PATH);
        }

        // The list being validated is the normalised one, which is also exactly
        // what the token will carry and what CF_HDROP will hold. The spec is
        // written out to match the declarative form character for character,
        // because the validator has to be the same one either way.
        json normalized = json::object();
        normalized["paths"] = json::array();
        for (const auto& path : plan.nativePaths) {
            normalized["paths"].push_back(WideToUtf8(path));
        }
        const PathSecuritySpec spec{"paths", SecurityLevel::MediaRead, true};
        const auto validation = ValidatePathParam(normalized, spec, "dnd.prepareDrag");
        if (!validation.success) {
            return api::Fail(validation.errorMsg,
                             validation.shapeError ? ApiErrorCode::INVALID_PARAMS
                                                   : ApiErrorCode::PERMISSION_DENIED);
        }

        // 4. Only now does a token exist, bound to this window and to this
        //    snapshot of already-validated paths.
        const std::string token = fb2k_dnd::DragOutTokenStore().Mint(
            reinterpret_cast<intptr_t>(callerHwnd), plan.nativePaths);
        if (token.empty()) {
            return api::Fail("Could not mint a drag token.", ApiErrorCode::OPERATION_FAILED);
        }

        dnd::PrepareDragResult result;
        result.token = token;
        return result;
    }

    //==========================================================================
    // dnd.startDrag - Not implemented
    //
    // Dragging content out of the window needs an IDropSource implementation
    // and a host-produced data object; neither exists. Reported as an explicit
    // failure so callers cannot build on a fake success response.
    //==========================================================================
    api::Result<void> DndStartDrag(const dnd::StartDragParams&) {
        return api::Fail(
            "Dragging out of the window is not implemented; it requires an "
            "IDropSource implementation.",
            ApiErrorCode::NOT_SUPPORTED);
    }

} // anonymous namespace

//==========================================================================
// Register Drag-and-Drop API
//==========================================================================
void RegisterDndApi() {
    // dnd.getPathsAsync - Real paths of a drag session, queried from the host
    api::RegisterApi("dnd.getPathsAsync", DndGetPathsAsync);

    // dnd.getCapabilities - Host drag-drop capability for the calling window
    api::RegisterApi("dnd.getCapabilities", DndGetCapabilities);

    // dnd.prepareDrag - Validated paths in, one-shot drag token out.
    //
    // The declaration deliberately gives `paths` no @security, even though it is
    // a path parameter. A declared security level would make the wrapper run
    // ValidatePathParam before the handler is entered, and this endpoint needs
    // the opposite order: the origin decision has to come first, because
    // MediaRead falls back to a media-library membership test, so validating
    // paths for an untrusted page would answer "is this file in the user's
    // library" one path at a time.
    //
    // The handler therefore calls the very same ValidatePathParam itself, with a
    // spec written to match the declarative form exactly, and the assertion below
    // keeps the declaration honest. The order is spelled out again at the top of
    // DndPrepareDrag and in src/api/schema/dnd.ts, so none of the three notes can
    // be removed on its own.
    static_assert(api::dnd::PrepareDragParams::kPathParams.empty(),
                  "dnd.prepareDrag validates its paths itself, after the origin check");
    api::RegisterApi("dnd.prepareDrag", DndPrepareDrag);

    // dnd.startDrag - Always reports NOT_SUPPORTED
    api::RegisterApi("dnd.startDrag", DndStartDrag);

    LOG("Drag-and-Drop API registered (4 APIs)");
}
