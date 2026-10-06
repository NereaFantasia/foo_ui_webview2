// PlaylistHandlerTestSupport.h - Fixture shared by the PlaylistApi handler tests
// PlaylistHandlerTest backs test_playlist_handlers_p3.cpp and is the base of
// PlaylistHandlerP4Test in test_playlist_handlers_p4.cpp.
#pragma once

#include <gtest/gtest.h>
#include "mocks/MockPlaylistService.h"

// ==========================================================================
// Test fixture
// ==========================================================================
class PlaylistHandlerTest : public ::testing::Test {
protected:
    MockPlaylistService mock;

    void SetUp() override {
        mock.Reset();
    }

    void AddPlaylists(std::initializer_list<std::pair<std::string, size_t>> items) {
        for (const auto& [name, count] : items) {
            mock.playlists.push_back({name, count, false, false, ""});
        }
    }
};
