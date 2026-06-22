#pragma once

// ============================================
// ApiConstants.h - API Error Message Constants
// ============================================
// Unified error message constants for maintenance and reuse
// Usage: return {{"success", false}, {"error", ApiError::INVALID_PLAYLIST}};
// ============================================

namespace ApiError {
    // Index errors
    constexpr const char* INVALID_PLAYLIST = "Invalid playlist index";
    constexpr const char* INVALID_TRACK = "Invalid track index";
    constexpr const char* INVALID_INDEX = "Index out of range";
    
    // Parameter errors
    constexpr const char* INVALID_PARAMS = "Invalid parameters";
    constexpr const char* REQUIRED_PARAM_MISSING = "Required parameter missing";
    constexpr const char* PATH_REQUIRED = "path is required";
    constexpr const char* NAME_REQUIRED = "name is required";
    constexpr const char* URL_TOO_LONG = "URL exceeds maximum length (2048)";
    
    // State errors
    constexpr const char* PLAYLIST_LOCKED = "Playlist is locked";
    constexpr const char* NOT_FOUND = "Not found";
    constexpr const char* NO_ACTIVE_PLAYLIST = "No active playlist";
    constexpr const char* NO_PLAYING_TRACK = "No track playing";
    
    // Operation errors
    constexpr const char* OPERATION_FAILED = "Operation failed";
    constexpr const char* CANNOT_SEEK = "Cannot seek in current track";
    
    // Library errors
    constexpr const char* LIBRARY_NOT_ENABLED = "Library not enabled";

    // Multi-window errors
    constexpr const char* WINDOW_NOT_FOUND = "Window not found";
    constexpr const char* WINDOW_ID_REQUIRED = "windowId is required";
    constexpr const char* TARGET_WINDOW_NOT_FOUND = "Target window not found";
    constexpr const char* CANNOT_CLOSE_MAIN = "Cannot close main window via closePopup";
    constexpr const char* POPUP_CREATE_FAILED = "Failed to create popup window";
    constexpr const char* MAX_POPUPS_REACHED = "Maximum number of popup windows reached";
    constexpr const char* STANDALONE_ONLY = "This API is only available in standalone window mode";
}

// API success messages (rarely used, for consistency)
namespace ApiMessage {
    constexpr const char* SUCCESS = "Operation completed successfully";
}

// API numeric limits
namespace ApiLimits {
    // Maximum URL length accepted by streaming/playlist APIs.
    // Matches WinINet INTERNET_MAX_URL_LENGTH (2048) and the plugin's
    // own ParseUrl wchar_t path[2048] buffer in HttpApi.cpp. URLs
    // longer than this trigger ERROR_BUFFER_OVERFLOW deep inside
    // foo_httpstream.dll, which fb2k surfaces as a native error
    // dialog ("The file name is too long" / "参数过长") that bypasses
    // plugin-level error handling.
    constexpr size_t MAX_STREAM_URL_LENGTH = 2048;
}
