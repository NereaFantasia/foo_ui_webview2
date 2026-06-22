#pragma once

// ============================================
// ErrorEnvelope.h - Unified Failure Envelope Contract
// ============================================
//
// Defines machine-readable error codes, unified error constructors,
// and the failure sampling-log hook.
//
// Minimal contract:
//   Sync failure: { success: false, error: string, code: string }
//   Async failure event: { error: string, code: string, taskId?: string }
//   Framework failure: { error: string, code: string }
//
// Optional extension fields: source, details, retryable
// ============================================

#include <nlohmann/json.hpp>
using json = nlohmann::json;

// ============================================
// Machine-readable error code enum
// ============================================
// Naming convention: UPPER_SNAKE_CASE
// Code values from actually existing or unified error set
namespace ApiErrorCode {
    // --- Framework level (BridgeCore SendError) ---
    constexpr const char* INVALID_REQUEST   = "INVALID_REQUEST";    // JSON-RPC -32600
    constexpr const char* METHOD_NOT_FOUND  = "METHOD_NOT_FOUND";   // JSON-RPC -32601
    constexpr const char* INTERNAL_ERROR    = "INTERNAL_ERROR";     // handler uncaught exception

    // --- Parameter errors ---
    constexpr const char* REQUIRED_PARAM    = "REQUIRED_PARAM";     // required param missing
    constexpr const char* INVALID_PARAMS    = "INVALID_PARAMS";     // param value invalid
    constexpr const char* INVALID_INDEX     = "INVALID_INDEX";      // index out of range

    // --- State/resource errors ---
    constexpr const char* NOT_FOUND         = "NOT_FOUND";          // resource not found
    constexpr const char* LOCKED            = "LOCKED";             // playlist locked etc.
    constexpr const char* NOT_SUPPORTED     = "NOT_SUPPORTED";      // panel mode unsupported etc.
    constexpr const char* LIBRARY_DISABLED  = "LIBRARY_DISABLED";   // library not enabled
    constexpr const char* NO_ACTIVE_ITEM    = "NO_ACTIVE_ITEM";     // no active playlist/track

    // --- Operation failures ---
    constexpr const char* OPERATION_FAILED  = "OPERATION_FAILED";   // generic operation failure

    // --- Path/media related (existing in AudioApi) ---
    constexpr const char* MISSING_PATH      = "MISSING_PATH";
    constexpr const char* INVALID_PATH      = "INVALID_PATH";
    constexpr const char* PERMISSION_DENIED  = "PERMISSION_DENIED";  // path security tier violation
    constexpr const char* INVALID_HANDLE    = "INVALID_HANDLE";
    constexpr const char* NO_INFO           = "NO_INFO";
    constexpr const char* DECODER_FAILED    = "DECODER_FAILED";
    constexpr const char* DECODE_FAILED     = "DECODE_FAILED";
    constexpr const char* UNKNOWN_ERROR     = "UNKNOWN_ERROR";
    constexpr const char* EXCEPTION         = "EXCEPTION";
}

// ============================================
// Unified error constructors
// ============================================
namespace ApiEnvelope {

    // Minimal sync error envelope: { success: false, error, code }
    inline json MakeError(const char* message, const char* code) {
        return {
            {"success", false},
            {"error",   message},
            {"code",    code}
        };
    }

    inline json MakeError(const std::string& message, const char* code) {
        return {
            {"success", false},
            {"error",   message},
            {"code",    code}
        };
    }

    // Sync error envelope with details
    inline json MakeError(const char* message, const char* code, const json& details) {
        return {
            {"success", false},
            {"error",   message},
            {"code",    code},
            {"details", details}
        };
    }

    inline json MakeError(const std::string& message, const char* code, const json& details) {
        return {
            {"success", false},
            {"error",   message},
            {"code",    code},
            {"details", details}
        };
    }

    // Async failure event payload: { error, code, taskId? }
    inline json MakeFailureEvent(const std::string& error, const char* code,
                                  const std::string& taskId = "") {
        json payload;
        payload["error"] = error;
        payload["code"]  = code;
        if (!taskId.empty()) {
            payload["taskId"] = taskId;
        }
        return payload;
    }

    // Async failure event payload with path: { error, code, taskId, path }
    inline json MakeFailureEvent(const std::string& error, const char* code,
                                  const std::string& taskId, const std::string& path) {
        json payload;
        payload["error"]  = error;
        payload["code"]   = code;
        payload["taskId"] = taskId;
        payload["path"]   = path;
        return payload;
    }
}

// ============================================
// Failure sampling-log hook
// ============================================
// Records all failures produced via MakeError / MakeFailureEvent / SendError
// for post-hoc error distribution analysis. Does not alter product behavior.
namespace FailureHook {

    // Sync API failure hook
    inline void LogSync(const char* api, const char* code, const char* message,
                        bool hasDetails = false) {
        console::printf("[FailureEnvelope] syncOrAsync=sync api=%s code=%s message=%s hasDetails=%s",
            api, code, message, hasDetails ? "true" : "false");
    }

    // Async event failure hook
    inline void LogAsync(const char* eventName, const char* code, const char* message,
                          const char* taskId = "") {
        console::printf("[FailureEnvelope] syncOrAsync=async eventName=%s code=%s taskId=%s message=%s",
            eventName, code, taskId, message);
    }

    // Framework-level failure hook (SendError)
    inline void LogFramework(const char* code, const char* message, const std::string& method = "") {
        console::printf("[FailureEnvelope] syncOrAsync=framework code=%s method=%s message=%s",
            code, method.c_str(), message);
    }
}