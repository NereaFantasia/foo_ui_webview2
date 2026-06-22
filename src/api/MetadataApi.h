#pragma once

#include "BridgeCore.h"

// Register all metadata editing related APIs
// metadata.write, metadata.writeBatch, metadata.embedArtwork, metadata.removeTag
// metadata.read, metadata.readByPath, metadata.readRaw, metadata.readBatch
// metadata.removeEmbeddedArt, metadata.removeField (alias of removeTag)
// rating.set, rating.get (cross-namespace)
/** @brief Register the metadata.* / rating.* API handlers. */
void RegisterMetadataApi();

// Exposed for sibling APIs (e.g., LyricsApi embedded tag writing)
nlohmann::json MetadataWriteTags(const nlohmann::json& params);
