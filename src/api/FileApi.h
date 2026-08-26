#pragma once

#include "BridgeCore.h"
#include <string>

// Register all file system related APIs
// file.read, file.write, file.exists, file.list, file.delete, file.mkdir
// file.copyAsync, file.moveAsync, file.deleteAsync, file.cancelOp (async family)
/** @brief Register the file.* (filesystem) API handlers. */
void RegisterFileApi();

// 取消指定窗口发起的所有在飞异步文件操作 (popup 关闭时调用)
void CancelAllFileOpsForWindow(const std::string& windowId);
