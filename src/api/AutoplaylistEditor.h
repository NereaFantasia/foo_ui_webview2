#pragma once
// AutoplaylistEditor.h - Provider capability and document-bound editor request policy.
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace api::autoplaylist_editor {

enum class Availability { Available, Unavailable, Unknown };
enum class AvailabilityReason { Supported, NoEditor, LegacyClient, UnverifiedLock, NotAutoplaylist };
enum class FailureReason {
    TargetGone, NotAutoplaylist, NoEditor, RequestInProgress, QueryFailed, DispatchFailed, ProviderFailed
};

struct Client {
    std::optional<bool> availability;
    std::function<void(size_t)> show;
};

struct BackendHooks {
    std::function<bool(size_t)> hasClient;
    std::function<Client(size_t)> queryClient;
    std::function<bool(size_t)> isCompatibleAutoplaylistLock;
    std::function<bool(size_t)> showLock;
};

struct Probe {
    Availability availability = Availability::Unavailable;
    AvailabilityReason reason = AvailabilityReason::NotAutoplaylist;
    std::function<bool(size_t)> show;
};

/** @brief Inspect the current provider without invoking UI; query failures propagate. */
Probe ProbeProvider(const BackendHooks& backend, size_t index);

struct Result {
    std::string operationId;
    std::string playlistGuid;
    std::optional<FailureReason> failure;
};

struct Request {
    std::string operationId;
    std::string playlistGuid;
    std::function<bool()> isCurrentDocument;
    std::function<std::optional<size_t>()> locate;
    std::function<Probe(size_t)> probe;
    std::function<void()> accepted;
    std::function<void(const Result&)> complete;
    std::function<void(std::function<void()>)> schedule;
};

}  // namespace api::autoplaylist_editor

namespace api {

/** @brief Serialize editor calls by canonical GUID on the main thread. */
class AutoplaylistEditor {
public:
    AutoplaylistEditor();
    ~AutoplaylistEditor();
    AutoplaylistEditor(const AutoplaylistEditor&) = delete;
    AutoplaylistEditor& operator=(const AutoplaylistEditor&) = delete;

    /** @brief Accept before scheduling; accepted requests report only through complete.
        Expired documents receive neither callback. All callbacks run on the main thread. */
    std::optional<autoplaylist_editor::FailureReason> Start(autoplaylist_editor::Request request);

private:
    struct State;
    std::shared_ptr<State> m_state;
};

}  // namespace api
