#pragma once
#include "pch.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ============================================
// Queue API Registration
// ============================================
//
// Registered APIs (16):
//   -- queue.* (8) --
//   queue.get
//   queue.add
//   queue.addPaths
//   queue.remove
//   queue.clear
//   queue.getCount
//   queue.moveToTop
//   queue.flush
//   -- jitQueue.* (8) --
//   jitQueue.playNow
//   jitQueue.enqueueNext
//   jitQueue.skip
//   jitQueue.stop
//   jitQueue.clear
//   jitQueue.getState
//   jitQueue.notifyEmpty
//   jitQueue.preloadBatch
//

// Register all playback queue-related APIs with BridgeCore
// Includes both native queue (queue.*) and JIT queue (jitQueue.*)
/** @brief Register the queue.* (native playback queue) API handlers. */
void RegisterQueueApi();

// 原生队列专用播放列表名称
constexpr const char* QUEUE_PLAYLIST_NAME = "[WebView Queue]";

// ============================================
// JIT Queue API Registration
// ============================================

// Register JIT queue APIs for streaming media
// Uses dual-layer architecture: frontend logic + backend execution
/** @brief Register the jitQueue.* (just-in-time streaming queue) API handlers. */
void RegisterJitQueueApi();

// Initialize JIT Queue Manager (call during plugin startup)
void InitializeJitQueue();

