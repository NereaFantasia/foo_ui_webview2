// test_error_envelope.cpp — ErrorEnvelope 纯函数测试
#include "pch.h"
#include "compat/fb2k_types.h"  // console:: stub before ErrorEnvelope.h
#include "../src/api/ErrorEnvelope.h"

using json = nlohmann::json;

// ============================================
// ApiErrorCode constants
// ============================================

TEST(ApiErrorCode, FrameworkAndParamCodes) {
    EXPECT_STREQ(ApiErrorCode::INVALID_REQUEST,  "INVALID_REQUEST");
    EXPECT_STREQ(ApiErrorCode::METHOD_NOT_FOUND, "METHOD_NOT_FOUND");
    EXPECT_STREQ(ApiErrorCode::INTERNAL_ERROR,   "INTERNAL_ERROR");
    EXPECT_STREQ(ApiErrorCode::REQUIRED_PARAM,   "REQUIRED_PARAM");
    EXPECT_STREQ(ApiErrorCode::INVALID_PARAMS,   "INVALID_PARAMS");
    EXPECT_STREQ(ApiErrorCode::INVALID_INDEX,    "INVALID_INDEX");
}

TEST(ApiErrorCode, StateAndResourceCodes) {
    EXPECT_STREQ(ApiErrorCode::NOT_FOUND,        "NOT_FOUND");
    EXPECT_STREQ(ApiErrorCode::LOCKED,           "LOCKED");
    EXPECT_STREQ(ApiErrorCode::NOT_SUPPORTED,    "NOT_SUPPORTED");
    EXPECT_STREQ(ApiErrorCode::LIBRARY_DISABLED, "LIBRARY_DISABLED");
    EXPECT_STREQ(ApiErrorCode::NO_ACTIVE_ITEM,   "NO_ACTIVE_ITEM");
    EXPECT_STREQ(ApiErrorCode::OPERATION_FAILED, "OPERATION_FAILED");
}

TEST(ApiErrorCode, PathAndMediaCodes) {
    EXPECT_STREQ(ApiErrorCode::MISSING_PATH,     "MISSING_PATH");
    EXPECT_STREQ(ApiErrorCode::INVALID_PATH,     "INVALID_PATH");
    EXPECT_STREQ(ApiErrorCode::PERMISSION_DENIED,"PERMISSION_DENIED");
    EXPECT_STREQ(ApiErrorCode::INVALID_HANDLE,   "INVALID_HANDLE");
    EXPECT_STREQ(ApiErrorCode::NO_INFO,          "NO_INFO");
    EXPECT_STREQ(ApiErrorCode::DECODER_FAILED,   "DECODER_FAILED");
    EXPECT_STREQ(ApiErrorCode::DECODE_FAILED,    "DECODE_FAILED");
    EXPECT_STREQ(ApiErrorCode::UNKNOWN_ERROR,    "UNKNOWN_ERROR");
    EXPECT_STREQ(ApiErrorCode::EXCEPTION,        "EXCEPTION");
}

TEST(ApiErrorCode, AllCodesNonEmpty) {
    EXPECT_NE(std::strlen(ApiErrorCode::INVALID_REQUEST), 0u);
    EXPECT_NE(std::strlen(ApiErrorCode::METHOD_NOT_FOUND), 0u);
    EXPECT_NE(std::strlen(ApiErrorCode::INTERNAL_ERROR), 0u);
    EXPECT_NE(std::strlen(ApiErrorCode::UNKNOWN_ERROR), 0u);
    EXPECT_NE(std::strlen(ApiErrorCode::EXCEPTION), 0u);
}

// ============================================
// ApiEnvelope::MakeError
// ============================================

TEST(ApiEnvelope, MakeError_CStr) {
    auto err = ApiEnvelope::MakeError("bad param", ApiErrorCode::INVALID_PARAMS);
    EXPECT_FALSE(err["success"].get<bool>());
    EXPECT_EQ(err["error"].get<std::string>(), "bad param");
    EXPECT_EQ(err["code"].get<std::string>(),  "INVALID_PARAMS");
    EXPECT_FALSE(err.contains("details"));
}

TEST(ApiEnvelope, MakeError_StdString) {
    std::string msg = "track not found";
    auto err = ApiEnvelope::MakeError(msg, ApiErrorCode::NOT_FOUND);
    EXPECT_FALSE(err["success"].get<bool>());
    EXPECT_EQ(err["error"].get<std::string>(), msg);
    EXPECT_EQ(err["code"].get<std::string>(),  "NOT_FOUND");
}

TEST(ApiEnvelope, MakeError_WithDetails) {
    json details = {{"field", "volume"}, {"max", 100}};
    auto err = ApiEnvelope::MakeError("out of range", ApiErrorCode::INVALID_PARAMS, details);
    EXPECT_FALSE(err["success"].get<bool>());
    EXPECT_EQ(err["details"]["field"].get<std::string>(), "volume");
    EXPECT_EQ(err["details"]["max"].get<int>(), 100);
}

// ============================================
// ApiEnvelope::MakeFailureEvent
// ============================================

TEST(ApiEnvelope, MakeFailureEvent_NoTaskId) {
    auto payload = ApiEnvelope::MakeFailureEvent("decode error", ApiErrorCode::DECODE_FAILED);
    EXPECT_EQ(payload["error"].get<std::string>(), "decode error");
    EXPECT_EQ(payload["code"].get<std::string>(),  "DECODE_FAILED");
    EXPECT_FALSE(payload.contains("taskId"));
}

TEST(ApiEnvelope, MakeFailureEvent_WithTaskId) {
    auto payload = ApiEnvelope::MakeFailureEvent("timeout", ApiErrorCode::OPERATION_FAILED, "task-42");
    EXPECT_EQ(payload["taskId"].get<std::string>(), "task-42");
}

TEST(ApiEnvelope, MakeFailureEvent_WithPath) {
    auto payload = ApiEnvelope::MakeFailureEvent("not found", ApiErrorCode::NOT_FOUND, "task-1", "/music/a.mp3");
    EXPECT_EQ(payload["path"].get<std::string>(), "/music/a.mp3");
    EXPECT_EQ(payload["taskId"].get<std::string>(), "task-1");
}

TEST(ApiEnvelope, MakeFailureEvent_EmptyTaskIdOmitted) {
    auto payload = ApiEnvelope::MakeFailureEvent("err", ApiErrorCode::INTERNAL_ERROR, "");
    EXPECT_FALSE(payload.contains("taskId"));
}

TEST(ApiEnvelope, MakeError_EmptyMessage) {
    auto err = ApiEnvelope::MakeError("", ApiErrorCode::UNKNOWN_ERROR);
    EXPECT_EQ(err["error"].get<std::string>(), "");
    EXPECT_FALSE(err["success"].get<bool>());
}

TEST(ApiEnvelope, MakeError_StdString_WithDetails) {
    std::string msg = "validation failed";
    json details = {{"fields", json::array({"name", "path"})}};
    auto err = ApiEnvelope::MakeError(msg, ApiErrorCode::INVALID_PARAMS, details);
    EXPECT_TRUE(err.contains("details"));
    EXPECT_EQ(err["details"]["fields"].size(), 2u);
}

// ============================================
// FailureHook smoke tests (no crash)
// ============================================

TEST(FailureHook, LogSync_NoCrash) {
    EXPECT_NO_THROW(FailureHook::LogSync("playback.play", "INTERNAL_ERROR", "test msg"));
    EXPECT_NO_THROW(FailureHook::LogSync("test", "ERR", "msg", true));
}

TEST(FailureHook, LogAsync_NoCrash) {
    EXPECT_NO_THROW(FailureHook::LogAsync("playback:error", "DECODE_FAILED", "decode err"));
    EXPECT_NO_THROW(FailureHook::LogAsync("ev", "CODE", "msg", "task-99"));
}

TEST(FailureHook, LogFramework_NoCrash) {
    EXPECT_NO_THROW(FailureHook::LogFramework("INVALID_REQUEST", "bad json"));
    EXPECT_NO_THROW(FailureHook::LogFramework("ERR", "msg", "playback.play"));
}
