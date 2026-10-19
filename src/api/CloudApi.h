#pragma once

#include "pch.h"

// Cloud API — 只读云状态（元数据）端点。
//
// Registered APIs (1):
//   cloud.getStates
//
// 红线口径：只读文件属性（GetFileAttributesW；directory 模式另用 FindFirstFileW
// 取目录项的属性），不打开、不读取任何文件内容 ⇒ 不触发 OneDrive 水合。这是
// **非内容读**：主题侧的红线计数应记在独立的元数据计数里（建议 cloudMetaCalls），
// 不得混入 fileReadCalls / configCalls（那两把尺子量的是内容读）。

/** @brief Register the cloud.* API handlers. */
void RegisterCloudApi();
