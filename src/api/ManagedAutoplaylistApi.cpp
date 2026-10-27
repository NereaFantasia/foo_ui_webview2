#include "pch.h"
#include "ManagedAutoplaylistApi.h"
#include "api/EventEmit.h"
#include "api/PlaylistApi.h"
#include "api/PlaylistTarget.h"
#include "api/TypedApi.h"
#include "api/generated/PlaylistSchema.h"
#include "domain/autoplaylist/ManagedAutoplaylist.h"

namespace {

json SnapshotJson(const std::string& guid, const managed_autoplaylist::Snapshot& snapshot) {
    return {{"playlistGuid", guid}, {"query", snapshot.config.query}, {"sort", snapshot.config.sort},
        {"keepSorted", snapshot.config.keepSorted}, {"revision", snapshot.revision}};
}

template <class R>
api::Result<R> Convert(const managed_autoplaylist::Operation& operation) {
    if (const auto* snapshot = std::get_if<managed_autoplaylist::Snapshot>(&operation.result)) {
        return R{operation.playlistGuid, snapshot->config.query, snapshot->config.sort,
            snapshot->config.keepSorted, snapshot->revision};
    }
    const auto& failure = std::get<managed_autoplaylist::Failure>(operation.result);
    json details{{"reason", failure.reason}, {"stage", failure.stage}, {"state", failure.state}};
    if (failure.current) details["current"] = SnapshotJson(operation.playlistGuid, *failure.current);
    if (failure.invalidParams) details["param"] = failure.param;
    if (operation.cleanup) {
        details["cleanup"] = *operation.cleanup;
        if (!operation.playlistGuid.empty()) details["playlistGuid"] = operation.playlistGuid;
    }
    return api::Fail("Autoplaylist configuration operation failed", failure.invalidParams ? ApiErrorCode::INVALID_PARAMS
        : ApiErrorCode::OPERATION_FAILED, {{"details", std::move(details)}});
}

template <class P>
std::optional<api::Failure> ResolveManaged(const P& params, std::string& guid) {
    try {
        const auto& service = *GetPlaylistService();
        size_t index = SIZE_MAX;
        if (auto failure = api::ResolvePlaylistTarget(service, params.playlist, params.playlistGuid, index,
            api::PlaylistOmitted::Refuse)) return failure;
        guid = api::PlaylistGuidOf(service, index);
        if (managed_autoplaylist::IsManaged(index)) return std::nullopt;
        const auto detected = service.detect_autoplaylist(index);
        const char* reason = !detected.isAutoplaylist ? "notAutoplaylist"
            : detected.isDuiAutoplaylist ? "unverifiedLock" : "externalProvider";
        return api::Fail("This playlist does not expose managed configuration", ApiErrorCode::NOT_SUPPORTED,
            {{"details", json{{"reason", reason}}}});
    } catch (...) {
        return api::Fail("Could not resolve the autoplaylist provider", ApiErrorCode::OPERATION_FAILED,
            {{"details", json{{"reason", "resolveFailed"}, {"stage", "resolve"}, {"state", "unknown"}}}});
    }
}

api::Result<api::playlist::CreateManagedAutoplaylistResult> CreateManaged(const api::playlist::CreateManagedAutoplaylistParams& params) {
    return Convert<api::playlist::CreateManagedAutoplaylistResult>(managed_autoplaylist::Create(params.name,
        {params.query, params.sort, params.keepSorted}));
}

api::Result<api::playlist::GetAutoplaylistConfigResult> GetConfig(const api::playlist::GetAutoplaylistConfigParams& params) {
    std::string guid;
    if (auto failure = ResolveManaged(params, guid)) return *failure;
    return Convert<api::playlist::GetAutoplaylistConfigResult>(managed_autoplaylist::Read(guid));
}

api::Result<api::playlist::SetAutoplaylistConfigResult> SetConfig(const api::playlist::SetAutoplaylistConfigParams& params) {
    std::string guid;
    if (auto failure = ResolveManaged(params, guid)) return *failure;
    return Convert<api::playlist::SetAutoplaylistConfigResult>(managed_autoplaylist::Update(guid,
        {params.query, params.sort, params.keepSorted}, params.expectedRevision));
}

} // namespace

void RegisterManagedAutoplaylistApi() {
    managed_autoplaylist::SetChangeCallback([](const std::string& guid, const std::string& revision) {
        api::emit::Broadcast<api::playlist::events::AutoplaylistConfigChanged>({guid, revision});
    });
    api::RegisterApi("playlist.createManagedAutoplaylist", CreateManaged);
    api::RegisterApi("playlist.getAutoplaylistConfig", GetConfig);
    api::RegisterApi("playlist.setAutoplaylistConfig", SetConfig);
}
