#pragma once

#include "BridgeCore.h"
#include <string>

// Register all metadata editing related APIs
// metadata.write, metadata.writeBatch, metadata.embedArtwork, metadata.removeTag
// metadata.read, metadata.readByPath, metadata.readRaw, metadata.readBatch
// metadata.probeBatchAsync, metadata.cancelProbe (async cancellable probe)
// metadata.removeEmbeddedArt, metadata.removeField (alias of removeTag)
// rating.set, rating.get (cross-namespace)
/** @brief Register the metadata.* / rating.* API handlers. */
void RegisterMetadataApi();

// Exposed for sibling APIs (e.g., LyricsApi embedded tag writing)
nlohmann::json MetadataWriteTags(const nlohmann::json& params);

// 取消指定窗口发起的所有在飞探测 (popup 关闭时调用)
void CancelAllProbesForWindow(const std::string& windowId);
