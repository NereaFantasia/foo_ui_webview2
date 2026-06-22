/**
 * MetadbCallback - Monitors all metadata changes
 * 
 * Catches metadata modifications including:
 * - Rating changes (from foo_playcount)
 * - Tag edits
 * - Playback statistics updates
 * 
 * Emits 'metadb:changed' event to WebView for real-time updates.
 */

#pragma once

#include "pch.h"

/**
 * Initialize metadb callbacks (called from MainWindow)
 */
void InitMetadbCallbacks();
