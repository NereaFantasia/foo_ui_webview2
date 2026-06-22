#pragma once

#include "BridgeCore.h"

// Registered APIs (29):
//   config.set
//   config.get
//   config.remove
//   config.getAll
//   config.export
//   config.getOutputDevices
//   config.getOutputConfig
//   config.setOutputDevice
//   config.setOutputBuffer
//   config.getAdvancedConfig
//   config.getAdvancedConfigValue
//   config.setAdvancedConfigValue
//   config.resetAdvancedConfig
//   config.getPreferencesPages
//   config.getPreferencesStandardGuids
//   config.getComponents
//   config.getVersionInfo
//   config.getLibraryStatus
//   config.getLibraryFilePatterns
//   config.showLibraryPreferences
//   config.getDspPresets
//   config.getActiveDspPreset
//   config.setActiveDspPreset
//   config.getCursorFollowPlayback
//   config.setCursorFollowPlayback
//   config.getPlaybackFollowCursor
//   config.setPlaybackFollowCursor
//   config.getReplaygainMode
//   config.setReplaygainMode

// Register all configuration/preferences related APIs
/** @brief Register the config.* (preferences) API handlers. */
void RegisterConfigApi();
