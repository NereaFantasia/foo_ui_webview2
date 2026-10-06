#pragma once

#include "BridgeCore.h"

// Register all lyrics related APIs
// lyrics.get, lyrics.save, lyrics.exists
/** @brief Register the lyrics.* API handlers. */
void RegisterLyricsApi();

/** Clear cached hits and misses after a file save or visible metadata change. */
void InvalidateLyricsCache();
