// test_api_constants.cpp - ApiConstants.h constexpr drift guard
#include "pch.h"
#include "../src/api/ApiConstants.h"

// ============================================
// ApiError constants - verify value stability
// ============================================

TEST(ApiError, IndexErrors) {
    EXPECT_STREQ(ApiError::INVALID_PLAYLIST, "Invalid playlist index");
    EXPECT_STREQ(ApiError::INVALID_TRACK, "Invalid track index");
    EXPECT_STREQ(ApiError::INVALID_INDEX, "Index out of range");
}

TEST(ApiError, ParameterErrors) {
    EXPECT_STREQ(ApiError::INVALID_PARAMS, "Invalid parameters");
    EXPECT_STREQ(ApiError::REQUIRED_PARAM_MISSING, "Required parameter missing");
    EXPECT_STREQ(ApiError::PATH_REQUIRED, "path is required");
    EXPECT_STREQ(ApiError::NAME_REQUIRED, "name is required");
}

TEST(ApiError, StateErrors) {
    EXPECT_STREQ(ApiError::PLAYLIST_LOCKED, "Playlist is locked");
    EXPECT_STREQ(ApiError::NOT_FOUND, "Not found");
    EXPECT_STREQ(ApiError::NO_ACTIVE_PLAYLIST, "No active playlist");
    EXPECT_STREQ(ApiError::NO_PLAYING_TRACK, "No track playing");
}

TEST(ApiError, OperationErrors) {
    EXPECT_STREQ(ApiError::OPERATION_FAILED, "Operation failed");
    EXPECT_STREQ(ApiError::CANNOT_SEEK, "Cannot seek in current track");
}

TEST(ApiError, LibraryErrors) {
    EXPECT_STREQ(ApiError::LIBRARY_NOT_ENABLED, "Library not enabled");
}

TEST(ApiError, WindowErrors) {
    EXPECT_STREQ(ApiError::WINDOW_NOT_FOUND, "Window not found");
    EXPECT_STREQ(ApiError::WINDOW_ID_REQUIRED, "windowId is required");
    EXPECT_STREQ(ApiError::TARGET_WINDOW_NOT_FOUND, "Target window not found");
    EXPECT_STREQ(ApiError::CANNOT_CLOSE_MAIN, "Cannot close main window via closePopup");
    EXPECT_STREQ(ApiError::POPUP_CREATE_FAILED, "Failed to create popup window");
    EXPECT_STREQ(ApiError::MAX_POPUPS_REACHED, "Maximum number of popup windows reached");
    EXPECT_STREQ(ApiError::STANDALONE_ONLY, "This API is only available in standalone window mode");
}

TEST(ApiMessage, SuccessConstant) {
    EXPECT_STREQ(ApiMessage::SUCCESS, "Operation completed successfully");
}

TEST(ApiError, AllNonEmpty) {
    EXPECT_NE(std::strlen(ApiError::INVALID_PLAYLIST), 0u);
    EXPECT_NE(std::strlen(ApiError::WINDOW_NOT_FOUND), 0u);
    EXPECT_NE(std::strlen(ApiError::STANDALONE_ONLY), 0u);
    EXPECT_NE(std::strlen(ApiMessage::SUCCESS), 0u);
}
