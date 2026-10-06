// test_playlist_handlers_p5.cpp - PlaylistApi undo, sort, lock, autoplaylist and reorder handlers
// Validates PlaylistApi handler logic using MockPlaylistService.
// Uses reimpl pattern: handler functions mirror product code but accept
// IPlaylistService* instead of calling playlist_manager::get().
#include "pch.h"
#include "mocks/MockPlaylistService.h"
#include "mocks/MockPlaybackService.h"

using json = nlohmann::json;

// ==========================================================================
// P5 reimplemented handler functions (Undo/Redo/Sort/Shuffle/Lock/Autoplaylist/Reorder)
// ==========================================================================
namespace reimpl_p5 {

static json MakePlaylistLockedError(size_t playlistIndex) {
    return {
        {"success", false},
        {"error", "Playlist is locked"},
        {"details", {{"playlist", playlistIndex}, {"isLocked", true}}}
    };
}

// Helper: mirrors GetPlaylistIndexFromParams
static size_t GetPlaylistIndexFromParams(const json& params) {
    if (params.contains("playlist")) return params.value("playlist", SIZE_MAX);
    return params.value("index", SIZE_MAX);
}

// 1. PlaylistUndo
json PlaylistUndo(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false} };
    return { {"success", svc->undo_restore(idx)} };
}

// 2. PlaylistRedo
json PlaylistRedo(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false} };
    return { {"success", svc->redo_restore(idx)} };
}

// 3. PlaylistSort
json PlaylistSort(IPlaylistService* svc, const json& params) {
    size_t idx = GetPlaylistIndexFromParams(params);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(idx))
        return MakePlaylistLockedError(idx);

    std::string pattern = params.value("pattern", "%title%");
    bool descending = params.value("descending", false);
    bool selectedOnly = params.value("selectedOnly", false);

    svc->playlist_undo_backup(idx);
    svc->sort_by_format(idx, pattern.c_str(), selectedOnly);

    if (descending) {
        size_t count = svc->playlist_get_item_count(idx);
        if (count > 1) {
            std::vector<size_t> order(count);
            for (size_t i = 0; i < count; i++) order[i] = count - 1 - i;
            svc->reorder_items(idx, order.data(), count);
        }
    }

    return { {"success", true} };
}

// 4. PlaylistShuffle
json PlaylistShuffle(IPlaylistService* svc, const json& params) {
    size_t idx = GetPlaylistIndexFromParams(params);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(idx))
        return MakePlaylistLockedError(idx);

    size_t count = svc->playlist_get_item_count(idx);
    if (count <= 1) return { {"success", true} };

    svc->playlist_undo_backup(idx);
    std::vector<size_t> order(count);
    for (size_t i = 0; i < count; i++) order[i] = i;
    std::random_device rd;
    std::mt19937 gen(rd());
    for (size_t i = count - 1; i > 0; i--) {
        std::uniform_int_distribution<size_t> dist(0, i);
        std::swap(order[i], order[dist(gen)]);
    }
    svc->reorder_items(idx, order.data(), count);
    return { {"success", true} };
}

// 5. PlaylistGetLockInfo
json PlaylistGetLockInfo(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"playlist", idx}, {"isLocked", false} };
    return { {"playlist", idx}, {"isLocked", svc->playlist_lock_is_present(idx)} };
}

// 6. PlaylistIsLocked
json PlaylistIsLocked(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"isLocked", false}, {"error", "Invalid playlist"} };
    return { {"success", true}, {"isLocked", svc->playlist_lock_is_present(idx)} };
}

// 7. PlaylistReverse
json PlaylistReverse(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(idx))
        return MakePlaylistLockedError(idx);
    size_t count = svc->playlist_get_item_count(idx);
    if (count < 2) return { {"success", true} };
    svc->playlist_undo_backup(idx);
    std::vector<size_t> order(count);
    for (size_t i = 0; i < count; i++) order[i] = count - 1 - i;
    svc->reorder_items(idx, order.data(), count);
    return { {"success", true} };
}

// 8. PlaylistReorder
json PlaylistReorder(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };
    if (svc->playlist_lock_is_present(idx))
        return MakePlaylistLockedError(idx);

    auto newOrder = params.value("newOrder", json::array());
    size_t count = svc->playlist_get_item_count(idx);
    if (newOrder.size() != count)
        return { {"success", false}, {"error", "newOrder length mismatch"} };

    std::vector<size_t> order(count);
    for (size_t i = 0; i < count; i++) {
        if (!newOrder[i].is_number())
            return { {"success", false}, {"error", "newOrder must contain numbers"} };
        size_t v = newOrder[i].get<size_t>();
        if (v >= count)
            return { {"success", false}, {"error", "Index out of range"} };
        order[i] = v;
    }

    svc->playlist_undo_backup(idx);
    svc->reorder_items(idx, order.data(), count);
    return { {"success", true}, {"playlist", idx}, {"itemCount", count} };
}

// 9. PlaylistIsAutoplaylist
json PlaylistIsAutoplaylist(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"playlist", idx}, {"isAutoplaylist", false} };

    auto det = svc->detect_autoplaylist(idx);
    json r = { {"playlist", idx}, {"isAutoplaylist", det.isAutoplaylist} };
    if (!det.lockName.empty()) r["lockName"] = det.lockName;
    return r;
}

// 10. PlaylistCreateAutoplaylist
json PlaylistCreateAutoplaylist(IPlaylistService* svc, const json& params) {
    std::string name = params.value("name", "New Autoplaylist");
    std::string query = params.value("query", "");
    std::string sort = params.value("sort", "");
    bool keepSorted = params.value("keepSorted", false);

    if (query.empty())
        return { {"success", false}, {"error", "Query string is required"} };

    uint32_t flags = keepSorted ? 1 : 0;  // autoplaylist_flag_sort == 1
    size_t idx = svc->create_playlist(name, SIZE_MAX);
    if (idx == SIZE_MAX)
        return { {"success", false}, {"error", "Failed to create playlist"} };

    try {
        svc->add_autoplaylist_client(idx, query.c_str(), sort.c_str(), flags);
    } catch (...) {
        svc->remove_playlist(idx);
        return { {"success", false}, {"error", "Failed to configure autoplaylist"} };
    }

    return {
        {"success", true}, {"index", idx}, {"playlist", idx},
        {"name", name}, {"query", query}
    };
}

// 11. PlaylistConvertToAutoplaylist
json PlaylistConvertToAutoplaylist(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };

    std::string query = params.value("query", "");
    std::string sort = params.value("sort", "");
    bool keepSorted = params.value("keepSorted", false);
    if (query.empty())
        return { {"success", false}, {"error", "Query string is required"} };

    uint32_t flags = keepSorted ? 1 : 0;
    try {
        svc->add_autoplaylist_client(idx, query.c_str(), sort.c_str(), flags);
    } catch (...) {
        return { {"success", false}, {"error", "Failed to convert to autoplaylist"} };
    }
    return { {"success", true}, {"playlist", idx} };
}

// 12. PlaylistRemoveAutoplaylist
json PlaylistRemoveAutoplaylist(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };

    if (svc->autoplaylist_is_client_present(idx)) {
        svc->remove_autoplaylist_client(idx);
        return { {"success", true}, {"playlist", idx}, {"source", "sdk"} };
    }

    auto det = svc->detect_autoplaylist(idx);
    if (det.isDuiAutoplaylist)
        return {
            {"success", true}, {"playlist", idx}, {"source", "dui"},
            {"note", "DUI autoplaylist lock detected; proceed with playlist.remove to delete playlist"}
        };

    return { {"success", false}, {"error", "Not an autoplaylist"} };
}

// 13. PlaylistGetAutoplaylistInfo
json PlaylistGetAutoplaylistInfo(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };

    auto det = svc->detect_autoplaylist(idx);
    if (!det.isAutoplaylist)
        return { {"isAutoplaylist", false}, {"playlist", idx} };

    uint32_t flags = 0;
    if (!det.isDuiAutoplaylist)
        flags = svc->get_autoplaylist_flags(idx);

    json r = {
        {"isAutoplaylist", true}, {"playlist", idx},
        {"keepSorted", (flags & 1) != 0},
        {"source", det.isDuiAutoplaylist ? "dui" : "sdk"}
    };
    if (!det.lockName.empty()) r["lockName"] = det.lockName;
    return r;
}

// 14. PlaylistGetAutoplaylistQuery
json PlaylistGetAutoplaylistQuery(IPlaylistService* svc, const json& params) {
    size_t idx = params.value("playlist", SIZE_MAX);
    if (idx == SIZE_MAX) idx = svc->get_active_playlist();
    if (idx >= svc->get_playlist_count())
        return { {"success", false}, {"error", "Invalid playlist index"} };

    auto det = svc->detect_autoplaylist(idx);
    if (!det.isAutoplaylist)
        return { {"isAutoplaylist", false}, {"playlist", idx}, {"query", nullptr} };

    uint32_t flags = 0;
    if (!det.isDuiAutoplaylist)
        flags = svc->get_autoplaylist_flags(idx);

    json r = {
        {"isAutoplaylist", true}, {"playlist", idx}, {"query", nullptr},
        {"keepSorted", (flags & 1) != 0},
        {"source", det.isDuiAutoplaylist ? "dui" : "sdk"},
        {"note", "Query string not exposed by SDK"}
    };
    if (!det.lockName.empty()) r["lockName"] = det.lockName;
    return r;
}

// 15. PlaylistReorderPlaylists
json PlaylistReorderPlaylists(IPlaylistService* svc, const json& params) {
    auto newOrder = params.value("newOrder", json::array());
    size_t count = svc->get_playlist_count();

    if (newOrder.size() != count)
        return { {"success", false}, {"error", "newOrder length mismatch"},
                 {"expected", count}, {"got", newOrder.size()} };

    std::vector<size_t> order(count);
    for (size_t i = 0; i < count; i++) {
        if (!newOrder[i].is_number())
            return { {"success", false}, {"error", "newOrder must contain numbers"} };
        size_t v = newOrder[i].get<size_t>();
        if (v >= count)
            return { {"success", false}, {"error", "Index out of range"}, {"index", v} };
        order[i] = v;
    }

    bool ok = svc->reorder_playlists(order.data(), count);
    return { {"success", ok}, {"count", count} };
}

} // namespace reimpl_p5

// ==========================================================================
// P5 Test Fixture
// ==========================================================================

class PlaylistHandlerP5Test : public ::testing::Test {
protected:
    MockPlaylistService mock;

    void SetUpBasic() {
        mock.Reset();
        mock.playlists = {
            {"Default", 10, false, false},
            {"Rock", 5, false, false}
        };
    }

    void SetUpLocked() {
        SetUpBasic();
        mock.playlists[0].isLocked = true;
    }
};

// ==========================================================================
// P5 Tests: Undo / Redo
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, Undo_Valid) {
    SetUpBasic();
    mock.undoRestoreResult = true;
    auto r = reimpl_p5::PlaylistUndo(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.undoRestoreCallCount, 1);
    EXPECT_EQ(mock.lastUndoRestorePlaylist, 0u);
}

TEST_F(PlaylistHandlerP5Test, Undo_Failure) {
    SetUpBasic();
    mock.undoRestoreResult = false;
    auto r = reimpl_p5::PlaylistUndo(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, Undo_DefaultActive) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistUndo(&mock, json::object());
    EXPECT_EQ(mock.lastUndoRestorePlaylist, 0u);
}

TEST_F(PlaylistHandlerP5Test, Undo_InvalidPlaylist) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistUndo(&mock, {{"playlist", 99}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(mock.undoRestoreCallCount, 0);
}

TEST_F(PlaylistHandlerP5Test, Redo_Valid) {
    SetUpBasic();
    mock.redoRestoreResult = true;
    auto r = reimpl_p5::PlaylistRedo(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.redoRestoreCallCount, 1);
}

TEST_F(PlaylistHandlerP5Test, Redo_Failure) {
    SetUpBasic();
    mock.redoRestoreResult = false;
    auto r = reimpl_p5::PlaylistRedo(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
}

// ==========================================================================
// P5 Tests: Sort
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, Sort_Default) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistSort(&mock, json::object());
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.sortByFormatCallCount, 1);
    EXPECT_EQ(mock.lastSortPlaylist, 0u);
    EXPECT_EQ(mock.lastSortPattern, "%title%");
    EXPECT_FALSE(mock.lastSortSelectedOnly);
}

TEST_F(PlaylistHandlerP5Test, Sort_CustomPattern) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistSort(&mock, {{"playlist", 1}, {"pattern", "%artist%"}, {"selectedOnly", true}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastSortPlaylist, 1u);
    EXPECT_EQ(mock.lastSortPattern, "%artist%");
    EXPECT_TRUE(mock.lastSortSelectedOnly);
}

TEST_F(PlaylistHandlerP5Test, Sort_Descending) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistSort(&mock, {{"playlist", 0}, {"descending", true}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.sortByFormatCallCount, 1);
    EXPECT_EQ(mock.reorderItemsCallCount, 1);
    // Verify reverse order: 10 items -> [9,8,7,6,5,4,3,2,1,0]
    ASSERT_EQ(mock.lastReorderItemsOrder.size(), 10u);
    EXPECT_EQ(mock.lastReorderItemsOrder[0], 9u);
    EXPECT_EQ(mock.lastReorderItemsOrder[9], 0u);
}

TEST_F(PlaylistHandlerP5Test, Sort_Locked) {
    SetUpLocked();
    auto r = reimpl_p5::PlaylistSort(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "Playlist is locked");
    EXPECT_EQ(mock.sortByFormatCallCount, 0);
}

TEST_F(PlaylistHandlerP5Test, Sort_InvalidPlaylist) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistSort(&mock, {{"playlist", 99}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, Sort_IndexKey) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistSort(&mock, {{"index", 1}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastSortPlaylist, 1u);
}

// ==========================================================================
// P5 Tests: Shuffle
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, Shuffle_Valid) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistShuffle(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.reorderItemsCallCount, 1);
    ASSERT_EQ(mock.lastReorderItemsOrder.size(), 10u);
}

TEST_F(PlaylistHandlerP5Test, Shuffle_SingleItem) {
    SetUpBasic();
    mock.playlists[0].trackCount = 1;
    auto r = reimpl_p5::PlaylistShuffle(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.reorderItemsCallCount, 0);
}

TEST_F(PlaylistHandlerP5Test, Shuffle_EmptyPlaylist) {
    SetUpBasic();
    mock.playlists[0].trackCount = 0;
    auto r = reimpl_p5::PlaylistShuffle(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.reorderItemsCallCount, 0);
}

TEST_F(PlaylistHandlerP5Test, Shuffle_Locked) {
    SetUpLocked();
    auto r = reimpl_p5::PlaylistShuffle(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(mock.reorderItemsCallCount, 0);
}

// ==========================================================================
// P5 Tests: GetLockInfo / IsLocked
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, GetLockInfo_Unlocked) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistGetLockInfo(&mock, {{"playlist", 0}});
    EXPECT_EQ(r["playlist"], 0u);
    EXPECT_FALSE(r["isLocked"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, GetLockInfo_Locked) {
    SetUpLocked();
    auto r = reimpl_p5::PlaylistGetLockInfo(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["isLocked"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, GetLockInfo_DefaultActive) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistGetLockInfo(&mock, json::object());
    EXPECT_EQ(r["playlist"], 0u);
}

TEST_F(PlaylistHandlerP5Test, IsLocked_Valid) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistIsLocked(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_FALSE(r["isLocked"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, IsLocked_InvalidPlaylist) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistIsLocked(&mock, {{"playlist", 99}});
    EXPECT_FALSE(r["success"].get<bool>());
}

// ==========================================================================
// P5 Tests: Reverse
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, Reverse_Valid) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistReverse(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.reorderItemsCallCount, 1);
    // 10 items: [9,8,7,...,0]
    ASSERT_EQ(mock.lastReorderItemsOrder.size(), 10u);
    EXPECT_EQ(mock.lastReorderItemsOrder[0], 9u);
    EXPECT_EQ(mock.lastReorderItemsOrder[9], 0u);
}

TEST_F(PlaylistHandlerP5Test, Reverse_SingleItem) {
    SetUpBasic();
    mock.playlists[0].trackCount = 1;
    auto r = reimpl_p5::PlaylistReverse(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.reorderItemsCallCount, 0);
}

TEST_F(PlaylistHandlerP5Test, Reverse_Locked) {
    SetUpLocked();
    auto r = reimpl_p5::PlaylistReverse(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(mock.reorderItemsCallCount, 0);
}

// ==========================================================================
// P5 Tests: Reorder (track-level)
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, Reorder_Valid) {
    SetUpBasic();
    mock.playlists[0].trackCount = 3;
    auto r = reimpl_p5::PlaylistReorder(&mock, {{"playlist", 0}, {"newOrder", {2, 0, 1}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["playlist"], 0u);
    EXPECT_EQ(r["itemCount"], 3u);
    ASSERT_EQ(mock.lastReorderItemsOrder, (std::vector<size_t>{2, 0, 1}));
}

TEST_F(PlaylistHandlerP5Test, Reorder_LengthMismatch) {
    SetUpBasic();
    mock.playlists[0].trackCount = 3;
    auto r = reimpl_p5::PlaylistReorder(&mock, {{"playlist", 0}, {"newOrder", {0, 1}}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(mock.reorderItemsCallCount, 0);
}

TEST_F(PlaylistHandlerP5Test, Reorder_IndexOutOfRange) {
    SetUpBasic();
    mock.playlists[0].trackCount = 3;
    auto r = reimpl_p5::PlaylistReorder(&mock, {{"playlist", 0}, {"newOrder", {0, 1, 99}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, Reorder_Locked) {
    SetUpLocked();
    auto r = reimpl_p5::PlaylistReorder(&mock, {{"playlist", 0}, {"newOrder", {0}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

// ==========================================================================
// P5 Tests: IsAutoplaylist
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, IsAutoplaylist_False) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {false, false, ""};
    auto r = reimpl_p5::PlaylistIsAutoplaylist(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["isAutoplaylist"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, IsAutoplaylist_SdkTrue) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {true, false, ""};
    auto r = reimpl_p5::PlaylistIsAutoplaylist(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["isAutoplaylist"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, IsAutoplaylist_DuiWithLock) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {true, true, "Autoplaylist"};
    auto r = reimpl_p5::PlaylistIsAutoplaylist(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["isAutoplaylist"].get<bool>());
    EXPECT_EQ(r["lockName"], "Autoplaylist");
}

TEST_F(PlaylistHandlerP5Test, IsAutoplaylist_DefaultActive) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {false, false, ""};
    auto r = reimpl_p5::PlaylistIsAutoplaylist(&mock, json::object());
    EXPECT_EQ(mock.lastDetectPlaylist, 0u);
}

// ==========================================================================
// P5 Tests: CreateAutoplaylist
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, CreateAutoplaylist_Valid) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistCreateAutoplaylist(&mock, {
        {"name", "My Auto"}, {"query", "artist IS test"}, {"sort", "%title%"}, {"keepSorted", true}
    });
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["name"], "My Auto");
    EXPECT_EQ(r["query"], "artist IS test");
    EXPECT_EQ(mock.addAutoplaylistClientCallCount, 1);
    EXPECT_EQ(mock.lastAddAutoQuery, "artist IS test");
    EXPECT_EQ(mock.lastAddAutoSort, "%title%");
    EXPECT_EQ(mock.lastAddAutoFlags, 1u);
}

TEST_F(PlaylistHandlerP5Test, CreateAutoplaylist_EmptyQuery) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistCreateAutoplaylist(&mock, {{"name", "Test"}, {"query", ""}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "Query string is required");
}

TEST_F(PlaylistHandlerP5Test, CreateAutoplaylist_NoKeepSorted) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistCreateAutoplaylist(&mock, {{"query", "test"}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(mock.lastAddAutoFlags, 0u);
}

// ==========================================================================
// P5 Tests: ConvertToAutoplaylist
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, ConvertAutoplaylist_Valid) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistConvertToAutoplaylist(&mock, {{"playlist", 0}, {"query", "genre IS rock"}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["playlist"], 0u);
    EXPECT_EQ(mock.addAutoplaylistClientCallCount, 1);
}

TEST_F(PlaylistHandlerP5Test, ConvertAutoplaylist_EmptyQuery) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistConvertToAutoplaylist(&mock, {{"playlist", 0}, {"query", ""}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, ConvertAutoplaylist_InvalidPlaylist) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistConvertToAutoplaylist(&mock, {{"playlist", 99}, {"query", "test"}});
    EXPECT_FALSE(r["success"].get<bool>());
}

// ==========================================================================
// P5 Tests: RemoveAutoplaylist
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, RemoveAutoplaylist_SdkClient) {
    SetUpBasic();
    mock.playlists[0].isAutoplaylist = true;
    auto r = reimpl_p5::PlaylistRemoveAutoplaylist(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["source"], "sdk");
    EXPECT_EQ(mock.removeAutoplaylistClientCallCount, 1);
}

TEST_F(PlaylistHandlerP5Test, RemoveAutoplaylist_DuiFallback) {
    SetUpBasic();
    // Not an autoplaylist_manager client, but DUI autoplaylist lock
    mock.detectAutoplaylistResult = {true, true, "Autoplaylist"};
    auto r = reimpl_p5::PlaylistRemoveAutoplaylist(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["source"], "dui");
}

TEST_F(PlaylistHandlerP5Test, RemoveAutoplaylist_NotAutoplaylist) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {false, false, ""};
    auto r = reimpl_p5::PlaylistRemoveAutoplaylist(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "Not an autoplaylist");
}

TEST_F(PlaylistHandlerP5Test, RemoveAutoplaylist_InvalidPlaylist) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistRemoveAutoplaylist(&mock, {{"playlist", 99}});
    EXPECT_FALSE(r["success"].get<bool>());
}

// ==========================================================================
// P5 Tests: GetAutoplaylistInfo
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, GetAutoplaylistInfo_SdkAutoplaylist) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {true, false, ""};
    mock.autoplaylistFlagsResult = 1;  // autoplaylist_flag_sort
    auto r = reimpl_p5::PlaylistGetAutoplaylistInfo(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["isAutoplaylist"].get<bool>());
    EXPECT_TRUE(r["keepSorted"].get<bool>());
    EXPECT_EQ(r["source"], "sdk");
    EXPECT_EQ(mock.getAutoplaylistFlagsCallCount, 1);
}

TEST_F(PlaylistHandlerP5Test, GetAutoplaylistInfo_DuiAutoplaylist) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {true, true, "Autoplaylist"};
    auto r = reimpl_p5::PlaylistGetAutoplaylistInfo(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["isAutoplaylist"].get<bool>());
    EXPECT_EQ(r["source"], "dui");
    EXPECT_EQ(r["lockName"], "Autoplaylist");
    EXPECT_EQ(mock.getAutoplaylistFlagsCallCount, 0);  // DUI -> skip flags
}

TEST_F(PlaylistHandlerP5Test, GetAutoplaylistInfo_NotAutoplaylist) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {false, false, ""};
    auto r = reimpl_p5::PlaylistGetAutoplaylistInfo(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["isAutoplaylist"].get<bool>());
}

// ==========================================================================
// P5 Tests: GetAutoplaylistQuery
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, GetAutoplaylistQuery_SdkAutoplaylist) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {true, false, ""};
    mock.autoplaylistFlagsResult = 0;
    auto r = reimpl_p5::PlaylistGetAutoplaylistQuery(&mock, {{"playlist", 0}});
    EXPECT_TRUE(r["isAutoplaylist"].get<bool>());
    EXPECT_TRUE(r["query"].is_null());
    EXPECT_FALSE(r["keepSorted"].get<bool>());
    EXPECT_EQ(r["source"], "sdk");
}

TEST_F(PlaylistHandlerP5Test, GetAutoplaylistQuery_NotAutoplaylist) {
    SetUpBasic();
    mock.detectAutoplaylistResult = {false, false, ""};
    auto r = reimpl_p5::PlaylistGetAutoplaylistQuery(&mock, {{"playlist", 0}});
    EXPECT_FALSE(r["isAutoplaylist"].get<bool>());
    EXPECT_TRUE(r["query"].is_null());
}

// ==========================================================================
// P5 Tests: ReorderPlaylists (playlist-level)
// ==========================================================================

TEST_F(PlaylistHandlerP5Test, ReorderPlaylists_Valid) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistReorderPlaylists(&mock, {{"newOrder", {1, 0}}});
    EXPECT_TRUE(r["success"].get<bool>());
    EXPECT_EQ(r["count"], 2u);
    ASSERT_EQ(mock.lastReorderPlaylistsOrder, (std::vector<size_t>{1, 0}));
}

TEST_F(PlaylistHandlerP5Test, ReorderPlaylists_LengthMismatch) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistReorderPlaylists(&mock, {{"newOrder", {0}}});
    EXPECT_FALSE(r["success"].get<bool>());
    EXPECT_EQ(r["error"], "newOrder length mismatch");
}

TEST_F(PlaylistHandlerP5Test, ReorderPlaylists_IndexOutOfRange) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistReorderPlaylists(&mock, {{"newOrder", {0, 99}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, ReorderPlaylists_NonNumeric) {
    SetUpBasic();
    auto r = reimpl_p5::PlaylistReorderPlaylists(&mock, {{"newOrder", {"a", "b"}}});
    EXPECT_FALSE(r["success"].get<bool>());
}

TEST_F(PlaylistHandlerP5Test, ReorderPlaylists_Failure) {
    SetUpBasic();
    mock.reorderPlaylistsResult = false;
    auto r = reimpl_p5::PlaylistReorderPlaylists(&mock, {{"newOrder", {1, 0}}});
    EXPECT_FALSE(r["success"].get<bool>());
}
