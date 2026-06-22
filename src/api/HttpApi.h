#pragma once

#include "BridgeCore.h"
#include <string>

// Register all HTTP related APIs
// http.get, http.post, http.download, http.head
/** @brief Register the http.* API handlers. */
void RegisterHttpApi();

// 取消指定窗口的所有未完成异步 HTTP 请求 (popup 关闭时调用)
void CancelAllHttpRequestsForWindow(const std::string& windowId);
