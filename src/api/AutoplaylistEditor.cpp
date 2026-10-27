#include "pch.h"
#include "AutoplaylistEditor.h"
#include <map>
#include <utility>

namespace api::autoplaylist_editor {

Probe ProbeProvider(const BackendHooks& backend, size_t index) {
    if (backend.hasClient(index)) {
        auto client = backend.queryClient(index);
        if (client.availability == false) {
            return {Availability::Unavailable, AvailabilityReason::NoEditor, {}};
        }
        const bool supported = client.availability.has_value();
        return {
            supported ? Availability::Available : Availability::Unknown,
            supported ? AvailabilityReason::Supported : AvailabilityReason::LegacyClient,
            [show = std::move(client.show)](size_t currentIndex) {
                show(currentIndex);
                return true;
            }
        };
    }
    if (backend.isCompatibleAutoplaylistLock(index)) {
        return {Availability::Unknown, AvailabilityReason::UnverifiedLock, backend.showLock};
    }
    return {};
}

}  // namespace api::autoplaylist_editor

namespace api {

namespace {

using autoplaylist_editor::FailureReason;
using autoplaylist_editor::Request;

bool IsCurrentDocument(const Request& request) noexcept {
    try {
        return request.isCurrentDocument();
    } catch (...) {
        return false;
    }
}

std::optional<FailureReason> ProbeFailure(const autoplaylist_editor::Probe& probe) {
    if (probe.availability != autoplaylist_editor::Availability::Unavailable) return std::nullopt;
    return probe.reason == autoplaylist_editor::AvailabilityReason::NotAutoplaylist
        ? FailureReason::NotAutoplaylist : FailureReason::NoEditor;
}

}  // namespace

struct AutoplaylistEditor::State {
    enum class Phase { Queued, Inspecting, Calling, Finished };

    struct Entry {
        explicit Entry(Request value) : request(std::move(value)) {}
        Request request;
        Phase phase = Phase::Queued;
    };

    std::map<std::string, std::shared_ptr<Entry>> pending;

    void Release(const std::shared_ptr<Entry>& entry) noexcept {
        entry->phase = Phase::Finished;
        const auto found = pending.find(entry->request.playlistGuid);
        if (found != pending.end() && found->second == entry) pending.erase(found);
    }

    void Finish(const std::shared_ptr<Entry>& entry, std::optional<FailureReason> failure) noexcept {
        if (entry->phase == Phase::Finished) return;
        Release(entry);
        if (!IsCurrentDocument(entry->request)) return;
        try {
            entry->request.complete({entry->request.operationId, entry->request.playlistGuid, failure});
        } catch (...) {
            // 结果投递失败不能逃出主线程回调，也不能再次发送同一操作的结果。
        }
    }

    bool IsReserved(const std::string& guid) noexcept {
        const auto found = pending.find(guid);
        if (found == pending.end()) return false;
        const auto entry = found->second;
        if (entry->phase == Phase::Queued && !IsCurrentDocument(entry->request)) {
            Release(entry);
            return false;
        }
        return true;
    }

    void Run(const std::shared_ptr<Entry>& entry) noexcept {
        if (entry->phase != Phase::Queued) return;
        entry->phase = Phase::Inspecting;
        try {
            if (!IsCurrentDocument(entry->request)) {
                Release(entry);
                return;
            }
            const auto index = entry->request.locate();
            if (!index) {
                Finish(entry, FailureReason::TargetGone);
                return;
            }
            auto provider = entry->request.probe(*index);
            if (!IsCurrentDocument(entry->request)) {
                Release(entry);
                return;
            }
            // 能力查询也会进入第三方代码；返回后索引可能已变化，不能把 UI 交给顶替列表。
            const auto callIndex = entry->request.locate();
            if (!callIndex) {
                Finish(entry, FailureReason::TargetGone);
                return;
            }
            if (const auto failure = ProbeFailure(provider)) {
                Finish(entry, failure);
                return;
            }

            // 第三方 UI 可运行嵌套消息循环；直到调用返回都保留 GUID，导航不能解除占位。
            entry->phase = Phase::Calling;
            bool succeeded = false;
            try {
                succeeded = provider.show(*callIndex);
            } catch (...) {
                succeeded = false;
            }
            if (!IsCurrentDocument(entry->request)) {
                Release(entry);
                return;
            }
            if (!entry->request.locate()) {
                Finish(entry, FailureReason::TargetGone);
                return;
            }
            Finish(entry, succeeded ? std::nullopt : std::optional{FailureReason::ProviderFailed});
        } catch (...) {
            Finish(entry, FailureReason::QueryFailed);
        }
    }
};

AutoplaylistEditor::AutoplaylistEditor() : m_state(std::make_shared<State>()) {}
AutoplaylistEditor::~AutoplaylistEditor() = default;

std::optional<FailureReason> AutoplaylistEditor::Start(Request request) {
    std::shared_ptr<State::Entry> entry;
    try {
        if (!IsCurrentDocument(request)) return std::nullopt;
        if (m_state->IsReserved(request.playlistGuid)) return FailureReason::RequestInProgress;
        const auto index = request.locate();
        if (!index) return FailureReason::TargetGone;
        const auto provider = request.probe(*index);
        if (!IsCurrentDocument(request)) return std::nullopt;
        if (!request.locate()) return FailureReason::TargetGone;
        if (const auto failure = ProbeFailure(provider)) return failure;

        entry = std::make_shared<State::Entry>(std::move(request));
        if (!m_state->pending.emplace(entry->request.playlistGuid, entry).second) {
            return FailureReason::RequestInProgress;
        }
    } catch (...) {
        return FailureReason::QueryFailed;
    }

    // 调用 accepted 后，即使回包或调度抛异常，也只允许走结果事件通道。
    try {
        entry->request.accepted();
        entry->request.schedule([state = m_state, entry]() noexcept { state->Run(entry); });
    } catch (...) {
        m_state->Finish(entry, FailureReason::DispatchFailed);
    }
    return std::nullopt;
}

}  // namespace api
