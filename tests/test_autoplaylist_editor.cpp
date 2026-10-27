#include "pch.h"
#include "mocks/MockPlaylistService.h"
#include "../src/api/AutoplaylistEditor.h"
#include "../src/api/PlaylistTarget.h"
#include <memory>
#include <stdexcept>
#include <utility>

namespace {

namespace editor = api::autoplaylist_editor;

class AutoplaylistEditorProbeTest : public ::testing::Test {
protected:
    bool hasClient = true;
    bool compatibleLock = true;
    std::optional<bool> availability = true;
    bool queryThrows = false;
    int clientQueries = 0;
    int lockQueries = 0;
    int clientShows = 0;
    int lockShows = 0;
    size_t shownIndex = SIZE_MAX;
    editor::BackendHooks hooks;

    void SetUp() override {
        hooks.hasClient = [this](size_t) { return hasClient; };
        hooks.queryClient = [this](size_t) {
            ++clientQueries;
            if (queryThrows) throw std::runtime_error("query");
            return editor::Client{availability, [this](size_t index) {
                ++clientShows;
                shownIndex = index;
            }};
        };
        hooks.isCompatibleAutoplaylistLock = [this](size_t) {
            ++lockQueries;
            return compatibleLock;
        };
        hooks.showLock = [this](size_t index) {
            ++lockShows;
            shownIndex = index;
            return true;
        };
    }

    void TearDown() override {
        EXPECT_EQ(clientShows, 0);
        EXPECT_EQ(lockShows, 0);
    }
};

TEST_F(AutoplaylistEditorProbeTest, V2AvailableDoesNotProbeLockOrOpenUi) {
    const auto probe = editor::ProbeProvider(hooks, 3);
    EXPECT_EQ(probe.availability, editor::Availability::Available);
    EXPECT_EQ(probe.reason, editor::AvailabilityReason::Supported);
    EXPECT_TRUE(static_cast<bool>(probe.show));
    EXPECT_EQ(clientQueries, 1);
    EXPECT_EQ(lockQueries, 0);
}

TEST_F(AutoplaylistEditorProbeTest, V2UnavailableDoesNotFallBackToLock) {
    availability = false;
    const auto probe = editor::ProbeProvider(hooks, 3);
    EXPECT_EQ(probe.availability, editor::Availability::Unavailable);
    EXPECT_EQ(probe.reason, editor::AvailabilityReason::NoEditor);
    EXPECT_FALSE(static_cast<bool>(probe.show));
    EXPECT_EQ(clientQueries, 1);
    EXPECT_EQ(lockQueries, 0);
}

TEST_F(AutoplaylistEditorProbeTest, LegacyClientHasUnknownCapability) {
    availability = std::nullopt;
    const auto probe = editor::ProbeProvider(hooks, 3);
    EXPECT_EQ(probe.availability, editor::Availability::Unknown);
    EXPECT_EQ(probe.reason, editor::AvailabilityReason::LegacyClient);
    EXPECT_TRUE(static_cast<bool>(probe.show));
    EXPECT_EQ(lockQueries, 0);
}

TEST_F(AutoplaylistEditorProbeTest, CompatibleLockHasUnknownCapability) {
    hasClient = false;
    const auto probe = editor::ProbeProvider(hooks, 3);
    EXPECT_EQ(probe.availability, editor::Availability::Unknown);
    EXPECT_EQ(probe.reason, editor::AvailabilityReason::UnverifiedLock);
    EXPECT_TRUE(static_cast<bool>(probe.show));
    EXPECT_EQ(clientQueries, 0);
    EXPECT_EQ(lockQueries, 1);
}

TEST_F(AutoplaylistEditorProbeTest, OrdinaryPlaylistDoesNotOfferEditor) {
    hasClient = false;
    compatibleLock = false;
    const auto probe = editor::ProbeProvider(hooks, 3);
    EXPECT_EQ(probe.availability, editor::Availability::Unavailable);
    EXPECT_EQ(probe.reason, editor::AvailabilityReason::NotAutoplaylist);
    EXPECT_FALSE(static_cast<bool>(probe.show));
    EXPECT_EQ(clientQueries, 0);
    EXPECT_EQ(lockQueries, 1);
}

TEST_F(AutoplaylistEditorProbeTest, ClientQueryFailureDoesNotInspectLock) {
    queryThrows = true;
    EXPECT_THROW(editor::ProbeProvider(hooks, 3), std::runtime_error);
    EXPECT_EQ(clientQueries, 1);
    EXPECT_EQ(lockQueries, 0);
}

TEST_F(AutoplaylistEditorProbeTest, ClientPresenceFailureDoesNotInspectLock) {
    hooks.hasClient = [](size_t) -> bool { throw std::runtime_error("presence"); };
    EXPECT_THROW(editor::ProbeProvider(hooks, 3), std::runtime_error);
    EXPECT_EQ(clientQueries, 0);
    EXPECT_EQ(lockQueries, 0);
}

TEST_F(AutoplaylistEditorProbeTest, LockQueryFailurePropagatesWithoutUi) {
    hasClient = false;
    hooks.isCompatibleAutoplaylistLock = [](size_t) -> bool { throw std::runtime_error("lock"); };
    EXPECT_THROW(editor::ProbeProvider(hooks, 3), std::runtime_error);
}

TEST(AutoplaylistEditorProviderTest, ClientCallableRetainsProviderUntilItIsReleased) {
    auto provider = std::make_shared<size_t>(SIZE_MAX);
    std::weak_ptr<size_t> observed = provider;
    editor::BackendHooks hooks;
    hooks.hasClient = [](size_t) { return true; };
    hooks.queryClient = [provider](size_t) {
        return editor::Client{true, [provider](size_t index) { *provider = index; }};
    };
    auto probe = editor::ProbeProvider(hooks, 0);
    hooks = {};
    provider.reset();
    ASSERT_FALSE(observed.expired());
    ASSERT_TRUE(static_cast<bool>(probe.show));
    EXPECT_TRUE(probe.show(4));
    ASSERT_FALSE(observed.expired());
    EXPECT_EQ(*observed.lock(), 4u);
    probe = {};
    EXPECT_TRUE(observed.expired());
}

class AutoplaylistEditorTest : public ::testing::Test {
protected:
    MockPlaylistService service;
    api::AutoplaylistEditor coordinator;
    std::shared_ptr<bool> document = std::make_shared<bool>(true);
    std::optional<bool> availability = true;
    bool compatibleLock = false;
    bool queryThrows = false;
    bool providerThrows = false;
    bool lockShowResult = true;
    int clientQueries = 0;
    int lockQueries = 0;
    int clientShows = 0;
    int lockShows = 0;
    int acceptedCount = 0;
    int nextOperation = 0;
    std::vector<size_t> shownIndices;
    std::vector<std::string> order;
    std::vector<editor::Result> results;
    std::vector<std::function<void()>> queued;
    std::function<void(size_t)> duringShow;

    void SetUp() override {
        for (unsigned long id = 1; id <= 2; ++id) {
            MockPlaylistService::MockPlaylist playlist;
            playlist.name = "Auto";
            playlist.guid.Data1 = id;
            playlist.isAutoplaylist = true;
            playlist.isLocked = true;
            service.playlists.push_back(playlist);
        }
        service.editorBackend.hasClient = [this](size_t index) {
            return service.autoplaylist_is_client_present(index);
        };
        service.editorBackend.queryClient = [this](size_t) {
            ++clientQueries;
            if (queryThrows) throw std::runtime_error("query");
            return editor::Client{availability, [this](size_t index) {
                ++clientShows;
                Show(index);
            }};
        };
        service.editorBackend.isCompatibleAutoplaylistLock = [this](size_t) {
            ++lockQueries;
            return compatibleLock;
        };
        service.editorBackend.showLock = [this](size_t index) {
            ++lockShows;
            Show(index);
            return lockShowResult;
        };
    }

    void TearDown() override {
        EXPECT_EQ(service.setActivePlaylistCallCount, 0);
        EXPECT_EQ(service.executeDefaultActionCallCount, 0);
        EXPECT_EQ(service.lockIsPresentCallCount, 0);
        EXPECT_EQ(service.lockQueryNameCallCount, 0);
        EXPECT_EQ(service.clearCallCount, 0);
        EXPECT_EQ(service.removeTracksCallCount, 0);
        EXPECT_EQ(service.undoBackupCallCount, 0);
    }

    void Show(size_t index) {
        shownIndices.push_back(index);
        order.push_back("show");
        if (duringShow) duringShow(index);
        if (providerThrows) throw std::runtime_error("provider");
    }

    editor::Request MakeRequest(size_t index = 0) {
        const api::PinnedPlaylist target(service, index);
        editor::Request request;
        request.operationId = "op-" + std::to_string(++nextOperation);
        request.playlistGuid = api::PlaylistGuidOf(service, index);
        request.isCurrentDocument = [current = document] { return *current; };
        request.locate = [this, target] { return target.Locate(service); };
        request.probe = [this](size_t currentIndex) { return service.probe_autoplaylist_editor(currentIndex); };
        request.accepted = [this] {
            ++acceptedCount;
            order.push_back("accepted");
        };
        request.complete = [this](const editor::Result& result) {
            results.push_back(result);
            order.push_back("complete");
        };
        request.schedule = [this](std::function<void()> callback) {
            order.push_back("schedule");
            queued.push_back(std::move(callback));
        };
        return request;
    }

    void Run(size_t index = 0) {
        ASSERT_LT(index, queued.size());
        EXPECT_NO_THROW(queued[index]());
    }

    void ExpectFailure(editor::FailureReason reason) {
        ASSERT_EQ(results.size(), 1u);
        ASSERT_TRUE(results[0].failure.has_value());
        EXPECT_EQ(*results[0].failure, reason);
    }
};

TEST_F(AutoplaylistEditorTest, AcceptsBeforeSchedulingAndCallsProviderLater) {
    auto request = MakeRequest();
    const auto operation = request.operationId;
    const auto guid = request.playlistGuid;
    ASSERT_FALSE(coordinator.Start(std::move(request)).has_value());
    EXPECT_EQ(order, (std::vector<std::string>{"accepted", "schedule"}));
    EXPECT_EQ(clientQueries, 1);
    EXPECT_EQ(clientShows, 0);
    Run();
    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].failure.has_value());
    EXPECT_EQ(results[0].operationId, operation);
    EXPECT_EQ(results[0].playlistGuid, guid);
    EXPECT_EQ(clientQueries, 2);
    EXPECT_EQ(clientShows, 1);
    EXPECT_EQ(lockShows, 0);
}

TEST_F(AutoplaylistEditorTest, RejectsOrdinaryPlaylistBeforeAcceptance) {
    service.playlists[0].isAutoplaylist = false;
    EXPECT_EQ(coordinator.Start(MakeRequest()), editor::FailureReason::NotAutoplaylist);
    EXPECT_EQ(acceptedCount, 0);
    EXPECT_TRUE(queued.empty());
    EXPECT_TRUE(results.empty());
}

TEST_F(AutoplaylistEditorTest, RejectsUnavailableEditorWithoutFallback) {
    availability = false;
    compatibleLock = true;
    EXPECT_EQ(coordinator.Start(MakeRequest()), editor::FailureReason::NoEditor);
    EXPECT_EQ(acceptedCount, 0);
    EXPECT_EQ(lockQueries, 0);
    EXPECT_EQ(lockShows, 0);
}

TEST_F(AutoplaylistEditorTest, ConvertsInitialQueryExceptionToBusinessFailure) {
    queryThrows = true;
    EXPECT_EQ(coordinator.Start(MakeRequest()), editor::FailureReason::QueryFailed);
    EXPECT_EQ(acceptedCount, 0);
    EXPECT_EQ(lockQueries, 0);
    EXPECT_TRUE(results.empty());
}

TEST_F(AutoplaylistEditorTest, AllowsLegacyClientAttempt) {
    availability = std::nullopt;
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    Run();
    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].failure.has_value());
    EXPECT_EQ(clientShows, 1);
}

TEST_F(AutoplaylistEditorTest, AllowsCompatibleLockWithoutTrackLockPrecondition) {
    service.playlists[0].isAutoplaylist = false;
    compatibleLock = true;
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    Run();
    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].failure.has_value());
    EXPECT_EQ(clientShows, 0);
    EXPECT_EQ(lockShows, 1);
}

TEST_F(AutoplaylistEditorTest, LockFalseReportsProviderFailure) {
    service.playlists[0].isAutoplaylist = false;
    compatibleLock = true;
    lockShowResult = false;
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    Run();
    ExpectFailure(editor::FailureReason::ProviderFailed);
    EXPECT_EQ(lockShows, 1);
}

TEST_F(AutoplaylistEditorTest, ResolvesReorderedTargetByPinnedGuid) {
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    std::swap(service.playlists[0], service.playlists[1]);
    Run();
    EXPECT_EQ(shownIndices, (std::vector<size_t>{1}));
    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].failure.has_value());
}

TEST_F(AutoplaylistEditorTest, RemovedTargetDoesNotOpenReplacementAtSameIndex) {
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    service.playlists.erase(service.playlists.begin());
    Run();
    ExpectFailure(editor::FailureReason::TargetGone);
    EXPECT_TRUE(shownIndices.empty());
    EXPECT_EQ(clientQueries, 1);
}

TEST_F(AutoplaylistEditorTest, MissingPinnedTargetBeforeAcceptanceIsTargetGone) {
    auto request = MakeRequest();
    service.playlists.erase(service.playlists.begin());
    EXPECT_EQ(coordinator.Start(std::move(request)), editor::FailureReason::TargetGone);
    EXPECT_EQ(acceptedCount, 0);
}

TEST_F(AutoplaylistEditorTest, ReprobesChangedProviderBeforeCalling) {
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    service.playlists[0].isAutoplaylist = false;
    compatibleLock = true;
    Run();
    EXPECT_EQ(clientShows, 0);
    EXPECT_EQ(lockShows, 1);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].failure.has_value());
}

TEST_F(AutoplaylistEditorTest, QueuedProviderBecomingOrdinaryReportsNotAutoplaylist) {
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    service.playlists[0].isAutoplaylist = false;
    Run();
    ExpectFailure(editor::FailureReason::NotAutoplaylist);
    EXPECT_TRUE(shownIndices.empty());
}

TEST_F(AutoplaylistEditorTest, QueuedProviderBecomingUnavailableReportsNoEditor) {
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    availability = false;
    compatibleLock = true;
    Run();
    ExpectFailure(editor::FailureReason::NoEditor);
    EXPECT_EQ(lockQueries, 0);
    EXPECT_TRUE(shownIndices.empty());
}

TEST_F(AutoplaylistEditorTest, QueuedQueryFailureDoesNotFallBackToLock) {
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    queryThrows = true;
    compatibleLock = true;
    Run();
    ExpectFailure(editor::FailureReason::QueryFailed);
    EXPECT_EQ(lockQueries, 0);
    EXPECT_TRUE(shownIndices.empty());
    EXPECT_EQ(acceptedCount, 1);
}

TEST_F(AutoplaylistEditorTest, CrossDocumentDuplicateIsRejectedBeforeAnotherProbe) {
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    document = std::make_shared<bool>(true);
    EXPECT_EQ(coordinator.Start(MakeRequest()), editor::FailureReason::RequestInProgress);
    EXPECT_EQ(clientQueries, 1);
    EXPECT_EQ(acceptedCount, 1);
    EXPECT_EQ(queued.size(), 1u);
}

TEST_F(AutoplaylistEditorTest, DifferentGuidsCanQueueAndCompleteIndependently) {
    ASSERT_FALSE(coordinator.Start(MakeRequest(0)).has_value());
    ASSERT_FALSE(coordinator.Start(MakeRequest(1)).has_value());
    EXPECT_EQ(acceptedCount, 2);
    Run(1);
    Run(0);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_NE(results[0].operationId, results[1].operationId);
    EXPECT_NE(results[0].playlistGuid, results[1].playlistGuid);
    EXPECT_EQ(shownIndices, (std::vector<size_t>{1, 0}));
}

TEST_F(AutoplaylistEditorTest, ExpiredDocumentBeforeAcceptanceDoesNothing) {
    *document = false;
    EXPECT_FALSE(coordinator.Start(MakeRequest()).has_value());
    EXPECT_EQ(acceptedCount, 0);
    EXPECT_EQ(clientQueries, 0);
    EXPECT_TRUE(queued.empty());
    EXPECT_TRUE(results.empty());
}

TEST_F(AutoplaylistEditorTest, ExpiredQueuedDocumentCancelsAndReleasesGuid) {
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    *document = false;
    Run();
    EXPECT_TRUE(results.empty());
    EXPECT_TRUE(shownIndices.empty());
    document = std::make_shared<bool>(true);
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    Run(1);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].failure.has_value());
}

TEST_F(AutoplaylistEditorTest, NewDocumentCanReplaceExpiredQueuedRequestBeforeDispatch) {
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    *document = false;
    document = std::make_shared<bool>(true);
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    Run(0);
    EXPECT_TRUE(results.empty());
    Run(1);
    EXPECT_EQ(results.size(), 1u);
    EXPECT_EQ(clientShows, 1);
}

TEST_F(AutoplaylistEditorTest, NavigationDuringProviderCallKeepsGuidReservedUntilReturn) {
    duringShow = [this](size_t) {
        *document = false;
        document = std::make_shared<bool>(true);
        const int queriesBefore = clientQueries;
        EXPECT_EQ(coordinator.Start(MakeRequest()), editor::FailureReason::RequestInProgress);
        EXPECT_EQ(clientQueries, queriesBefore);
    };
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    Run();
    EXPECT_TRUE(results.empty());
    duringShow = {};
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    Run(1);
    EXPECT_EQ(results.size(), 1u);
}

TEST_F(AutoplaylistEditorTest, TargetDeletionDuringProviderCallReportsTargetGone) {
    duringShow = [this](size_t) { service.playlists.erase(service.playlists.begin()); };
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    Run();
    ExpectFailure(editor::FailureReason::TargetGone);
}

TEST_F(AutoplaylistEditorTest, TargetDeletionTakesPriorityOverProviderException) {
    duringShow = [this](size_t) { service.playlists.erase(service.playlists.begin()); };
    providerThrows = true;
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    Run();
    ExpectFailure(editor::FailureReason::TargetGone);
}

TEST_F(AutoplaylistEditorTest, ProviderExceptionDoesNotFallBackAndAllowsLaterRequest) {
    compatibleLock = true;
    providerThrows = true;
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    Run();
    ExpectFailure(editor::FailureReason::ProviderFailed);
    EXPECT_EQ(lockShows, 0);
    providerThrows = false;
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    Run(1);
    EXPECT_EQ(results.size(), 2u);
}

TEST_F(AutoplaylistEditorTest, SchedulingExceptionOnlyCompletesAcceptedRequest) {
    auto request = MakeRequest();
    request.schedule = [](std::function<void()>) { throw std::runtime_error("dispatch"); };
    EXPECT_FALSE(coordinator.Start(std::move(request)).has_value());
    EXPECT_EQ(acceptedCount, 1);
    ExpectFailure(editor::FailureReason::DispatchFailed);
    EXPECT_TRUE(shownIndices.empty());
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
}

TEST_F(AutoplaylistEditorTest, ScheduledThenThrownCallbackCannotCompleteTwice) {
    auto request = MakeRequest();
    request.schedule = [this](std::function<void()> callback) {
        queued.push_back(std::move(callback));
        throw std::runtime_error("dispatch");
    };
    ASSERT_FALSE(coordinator.Start(std::move(request)).has_value());
    ExpectFailure(editor::FailureReason::DispatchFailed);
    Run();
    EXPECT_EQ(results.size(), 1u);
    EXPECT_TRUE(shownIndices.empty());
}

TEST_F(AutoplaylistEditorTest, RepeatedScheduledCallbackDoesNotReenterOrEmitTwice) {
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    Run();
    Run();
    EXPECT_EQ(clientShows, 1);
    EXPECT_EQ(results.size(), 1u);
}

TEST_F(AutoplaylistEditorTest, ReentrantScheduledCallbackDoesNotEnterProviderTwice) {
    duringShow = [this](size_t) { Run(); };
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    Run();
    EXPECT_EQ(clientShows, 1);
    EXPECT_EQ(results.size(), 1u);
}

TEST_F(AutoplaylistEditorTest, AcceptedCallbackExceptionCannotReplyAgainOrLeakReservation) {
    auto request = MakeRequest();
    request.accepted = [this] {
        ++acceptedCount;
        throw std::runtime_error("accepted delivery");
    };
    EXPECT_NO_THROW(EXPECT_FALSE(coordinator.Start(std::move(request)).has_value()));
    EXPECT_EQ(acceptedCount, 1);
    EXPECT_TRUE(queued.empty());
    ExpectFailure(editor::FailureReason::DispatchFailed);
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
}

TEST_F(AutoplaylistEditorTest, CompletionCallbackExceptionCannotEscapeOrLeakReservation) {
    auto request = MakeRequest();
    int completions = 0;
    request.complete = [&completions](const editor::Result&) {
        ++completions;
        throw std::runtime_error("event delivery");
    };
    ASSERT_FALSE(coordinator.Start(std::move(request)).has_value());
    Run();
    Run();
    EXPECT_EQ(completions, 1);
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
}

TEST_F(AutoplaylistEditorTest, QueuedDocumentCheckExceptionIsContainedAndReleasesGuid) {
    auto request = MakeRequest();
    bool throws = false;
    request.isCurrentDocument = [&throws] {
        if (throws) throw std::runtime_error("document");
        return true;
    };
    ASSERT_FALSE(coordinator.Start(std::move(request)).has_value());
    throws = true;
    Run();
    EXPECT_TRUE(results.empty());
    EXPECT_TRUE(shownIndices.empty());
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
}

TEST_F(AutoplaylistEditorTest, QueuedLocateExceptionIsContainedAsQueryFailure) {
    auto request = MakeRequest();
    bool throws = false;
    request.locate = [&throws]() -> std::optional<size_t> {
        if (throws) throw std::runtime_error("locate");
        return 0;
    };
    ASSERT_FALSE(coordinator.Start(std::move(request)).has_value());
    throws = true;
    Run();
    ExpectFailure(editor::FailureReason::QueryFailed);
    EXPECT_TRUE(shownIndices.empty());
}

TEST_F(AutoplaylistEditorTest, DocumentInvalidationDuringReprobePreventsNativeCall) {
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    service.editorBackend.queryClient = [this](size_t) {
        *document = false;
        return editor::Client{true, [this](size_t index) { Show(index); }};
    };
    Run();
    EXPECT_TRUE(shownIndices.empty());
    EXPECT_TRUE(results.empty());
}

TEST_F(AutoplaylistEditorTest, ReorderDuringReprobeDoesNotOpenThePreviousIndex) {
    const auto originalQuery = service.editorBackend.queryClient;
    service.editorBackend.queryClient = [this, originalQuery](size_t index) {
        auto client = originalQuery(index);
        if (clientQueries == 2) std::swap(service.playlists[0], service.playlists[1]);
        return client;
    };
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    Run();
    EXPECT_EQ(shownIndices, (std::vector<size_t>{1}));
    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].failure.has_value());
}

TEST_F(AutoplaylistEditorTest, DeletionDuringReprobeDoesNotOpenAReplacement) {
    const auto originalQuery = service.editorBackend.queryClient;
    service.editorBackend.queryClient = [this, originalQuery](size_t index) {
        auto client = originalQuery(index);
        if (clientQueries == 2) service.playlists.erase(service.playlists.begin());
        return client;
    };
    ASSERT_FALSE(coordinator.Start(MakeRequest()).has_value());
    Run();
    EXPECT_TRUE(shownIndices.empty());
    ExpectFailure(editor::FailureReason::TargetGone);
}

TEST_F(AutoplaylistEditorTest, DeletionDuringInitialProbeIsRefusedBeforeAcceptance) {
    const auto originalQuery = service.editorBackend.queryClient;
    service.editorBackend.queryClient = [this, originalQuery](size_t index) {
        auto client = originalQuery(index);
        service.playlists.erase(service.playlists.begin());
        return client;
    };
    EXPECT_EQ(coordinator.Start(MakeRequest()), editor::FailureReason::TargetGone);
    EXPECT_EQ(acceptedCount, 0);
    EXPECT_TRUE(queued.empty());
    EXPECT_TRUE(results.empty());
}

}  // namespace
