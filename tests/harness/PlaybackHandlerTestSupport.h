// PlaybackHandlerTestSupport.h - Fixture shared by the PlaybackApi handler tests
// Used by test_playback_handlers.cpp, test_playback_handlers_p1.cpp and
// test_playback_handlers_p2.cpp.
#pragma once

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "mocks/MockPlaybackService.h"

using json = nlohmann::json;

// ==========================================================================
// Test fixture
// ==========================================================================
class PlaybackHandlerTest : public ::testing::Test {
protected:
    MockPlaybackService mock;
    json emptyParams = json::object();

    void SetUp() override {
        mock.Reset();
    }
};
