// test_dnd_session.cpp - drag session state machine and layered expiry.
#include "pch.h"
#include "../src/webview/dnd/DragSession.h"

using namespace fb2k_dnd;

TEST(DragSession, EnterOverDropThenQueryable) {
    DragSessionStore store;
    auto id = store.BeginSession({L"C:\\a.mp3"}, true, 1000);
    EXPECT_FALSE(id.empty());
    EXPECT_TRUE(store.HasActiveSession());
    ASSERT_NE(store.Query(id, 1000), nullptr);
    EXPECT_EQ(store.Query(id, 1000)->startedAtMs, 1000);

    store.UpdatePaths(id, {L"C:\\a.mp3", L"C:\\b.mp3"}, true);
    store.EndSession(id, 2000);
    EXPECT_FALSE(store.HasActiveSession());

    const auto* s = store.Query(id, 2100);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->paths.size(), 2u);
}

TEST(DragSession, EnterLeaveClears) {
    DragSessionStore store;
    auto id = store.BeginSession({L"C:\\a.mp3"}, true, 1000);
    store.EndSession(id, 1500);
    EXPECT_FALSE(store.HasActiveSession());
}

TEST(DragSession, ReentrantEnterDiscardsOldSession) {
    DragSessionStore store;
    auto first = store.BeginSession({L"C:\\a.mp3"}, true, 1000);
    auto second = store.BeginSession({L"C:\\b.mp3"}, true, 1200);
    EXPECT_NE(first, second);

    // The superseded session must no longer be readable.
    EXPECT_EQ(store.Query(first, 1300), nullptr);

    const auto* s = store.Query(second, 1300);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->paths[0], L"C:\\b.mp3");
}

TEST(DragSession, HostSessionExpiresAfterTtl) {
    DragSessionStore store;
    auto id = store.BeginSession({L"C:\\a.mp3"}, true, 0);
    store.EndSession(id, 100);

    EXPECT_NE(store.Query(id, 100 + kHostSessionTtlMs - 1), nullptr);
    EXPECT_EQ(store.Query(id, 100 + kHostSessionTtlMs + 1), nullptr);
}

TEST(DragSession, PageSnapshotExpiresMuchEarlierThanHostSession) {
    // Layered cleanup: the page-visible snapshot goes stale long before the
    // host copy, shrinking the window for stale path reads.
    DragSessionStore store;
    auto id = store.BeginSession({L"C:\\a.mp3"}, true, 0);
    store.EndSession(id, 100);

    const auto* s = store.Query(id, 100 + kPageSnapshotTtlMs + 1);
    ASSERT_NE(s, nullptr) << "host session must still be queryable";
    EXPECT_FALSE(IsPageSnapshotFresh(*s, 100 + kPageSnapshotTtlMs + 1));
    EXPECT_TRUE(IsPageSnapshotFresh(*s, 100 + kPageSnapshotTtlMs - 1));
}

TEST(DragSession, ActiveSessionSnapshotStaysFreshWhileHovering) {
    // A hover has no known duration, so the page snapshot does not age while the
    // drag is still active. Sampled inside the host retention bound.
    DragSessionStore store;
    auto id = store.BeginSession({L"C:\\a.mp3"}, true, 0);
    const int64_t late = kActiveSessionMaxAgeMs - 1;
    const auto* s = store.Query(id, late);
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(IsPageSnapshotFresh(*s, late));
}

TEST(DragSession, AbandonedActiveSessionStopsBeingReadable) {
    // The source can die mid-hover, so the leave and drop never arrive. Without
    // an active bound the paths would stay readable for the window's lifetime.
    DragSessionStore store;
    auto id = store.BeginSession({L"C:\\a.mp3"}, true, 500);

    EXPECT_NE(store.Query(id, 500 + kActiveSessionMaxAgeMs - 1), nullptr);
    EXPECT_EQ(store.Query(id, 500 + kActiveSessionMaxAgeMs + 1), nullptr);
}

TEST(DragSession, ActiveBoundIsLooserThanEndedTtl) {
    // Ordering the two bounds this way is what keeps a completed drag from
    // outliving one still in progress.
    static_assert(kActiveSessionMaxAgeMs > kHostSessionTtlMs);

    DragSessionStore store;
    auto id = store.BeginSession({L"C:\\a.mp3"}, true, 0);
    const int64_t past = kHostSessionTtlMs + 1;
    ASSERT_NE(store.Query(id, past), nullptr) << "an active drag is not bound by the ended ttl";

    store.EndSession(id, past);
    EXPECT_EQ(store.Query(id, past + kHostSessionTtlMs + 1), nullptr);
}

TEST(DragSession, QueryUnknownSessionIdReturnsNull) {
    DragSessionStore store;
    store.BeginSession({L"C:\\a.mp3"}, true, 0);
    EXPECT_EQ(store.Query("does-not-exist", 10), nullptr);
}

TEST(DragSession, QueryWithEmptyIdReturnsCurrent) {
    DragSessionStore store;
    auto id = store.BeginSession({L"C:\\a.mp3"}, true, 0);
    const auto* s = store.Query("", 10);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->sessionId, id);
}

TEST(DragSession, EndSessionWithMismatchedIdIsIgnored) {
    DragSessionStore store;
    store.BeginSession({L"C:\\a.mp3"}, true, 0);
    store.EndSession("other-id", 100);
    EXPECT_TRUE(store.HasActiveSession());
}

TEST(DragSession, NonFileDragHasEmptyPaths) {
    DragSessionStore store;
    auto id = store.BeginSession({}, false, 0);
    const auto* s = store.Query(id, 10);
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->paths.empty());
    EXPECT_FALSE(s->hasFiles);
}

TEST(DragSession, ClearRemovesEverything) {
    DragSessionStore store;
    auto id = store.BeginSession({L"C:\\a.mp3"}, true, 0);
    store.Clear();
    EXPECT_FALSE(store.HasActiveSession());
    EXPECT_EQ(store.Query(id, 10), nullptr);
}

// --- Shortcut targets: the parallel array ------------------------------------

TEST(DragSession, ShortcutTargetsAreStoredAtTheMatchingIndex) {
    DragSessionStore store;
    auto id = store.BeginSession({L"C:\\a.mp3", L"C:\\b.lnk"}, true, 0,
                                 {std::nullopt, std::wstring(L"D:\\real.mp3")});
    const auto* s = store.Query(id, 10);
    ASSERT_NE(s, nullptr);
    ASSERT_EQ(s->resolvedPaths.size(), 2u);
    EXPECT_FALSE(s->resolvedPaths[0].has_value());
    ASSERT_TRUE(s->resolvedPaths[1].has_value());
    EXPECT_EQ(*s->resolvedPaths[1], L"D:\\real.mp3");
}

TEST(DragSession, OmittedShortcutTargetsBecomeAnEqualLengthEmptyArray) {
    // Every existing caller that predates the parallel array lands here, and a
    // reader indexing it with a paths index must not fall off the end.
    DragSessionStore store;
    auto id = store.BeginSession({L"C:\\a.mp3", L"C:\\b.mp3", L"C:\\c.mp3"}, true, 0);
    const auto* s = store.Query(id, 10);
    ASSERT_NE(s, nullptr);
    ASSERT_EQ(s->resolvedPaths.size(), s->paths.size());
    for (const auto& entry : s->resolvedPaths) {
        EXPECT_FALSE(entry.has_value());
    }
}

TEST(DragSession, ShortShortcutArrayIsPaddedToThePathCount) {
    DragSessionStore store;
    auto id = store.BeginSession({L"C:\\a.lnk", L"C:\\b.mp3", L"C:\\c.mp3"}, true, 0,
                                 {std::wstring(L"D:\\real.mp3")});
    const auto* s = store.Query(id, 10);
    ASSERT_NE(s, nullptr);
    ASSERT_EQ(s->resolvedPaths.size(), 3u);
    ASSERT_TRUE(s->resolvedPaths[0].has_value());
    EXPECT_FALSE(s->resolvedPaths[2].has_value());
}

TEST(DragSession, LongShortcutArrayIsTruncatedToThePathCount) {
    // A longer array would publish a target with no path beside it, which has
    // no index the page could pair it with.
    DragSessionStore store;
    auto id = store.BeginSession({L"C:\\a.lnk"}, true, 0,
                                 {std::wstring(L"D:\\one.mp3"),
                                  std::wstring(L"D:\\two.mp3")});
    const auto* s = store.Query(id, 10);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->resolvedPaths.size(), 1u);
}

TEST(DragSession, DropReplacesBothArraysTogether) {
    // Drop carries the authoritative list, so a stale target must not survive
    // beside a path it no longer belongs to.
    DragSessionStore store;
    auto id = store.BeginSession({L"C:\\a.lnk"}, true, 0,
                                 {std::wstring(L"D:\\enter.mp3")});
    store.UpdatePaths(id, {L"C:\\b.lnk", L"C:\\c.mp3"}, true,
                      {std::wstring(L"D:\\drop.mp3"), std::nullopt});

    const auto* s = store.Query(id, 10);
    ASSERT_NE(s, nullptr);
    ASSERT_EQ(s->paths.size(), 2u);
    ASSERT_EQ(s->resolvedPaths.size(), 2u);
    ASSERT_TRUE(s->resolvedPaths[0].has_value());
    EXPECT_EQ(*s->resolvedPaths[0], L"D:\\drop.mp3");
    EXPECT_FALSE(s->resolvedPaths[1].has_value());
}

TEST(DragSession, EmptyPathListLeavesNoShortcutTargets) {
    // The shape after ReadHdropPaths hits its catch-all and clears the list.
    DragSessionStore store;
    auto id = store.BeginSession({}, false, 0, {std::wstring(L"D:\\stale.mp3")});
    const auto* s = store.Query(id, 10);
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->paths.empty());
    EXPECT_TRUE(s->resolvedPaths.empty());
}
