#pragma once

// ============================================
// LibrarySync.h - library.getChanges 的修订号落盘与回调接入
//
// 变化表见 LibraryChangeLog.h。以下函数都只在主线程调用。
// ============================================

#include "domain/library/LibraryChangeLog.h"

namespace library_sync {

// 由 on_library_initialized 调用：读出落盘的修订号，开始记录。重复调用无害。
void OnLibraryLoaded();

// 载入完成之前为假。回调据此跳过载入批，不为整库算 handle。
bool Recording();

// 一次增、删、改回调带来的曲目 handle。
void OnItems(const std::vector<std::string>& handles, bool removed);

// 只在 Recording() 为真时调用。
const std::string& Epoch();
const ChangeLog& Log();

}  // namespace library_sync
