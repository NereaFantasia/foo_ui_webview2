#pragma once

#include "BridgeCore.h"

// ============================================
// ReplayGain Settings API
// ============================================
// replaygain.getSettings - Get all ReplayGain settings
// replaygain.getMode     - Get current mode
// replaygain.setMode     - Set processing mode
// replaygain.getPreamp   - Get preamp values
// replaygain.setPreamp   - Set preamp values

/** @brief Register the replaygain.* API handlers. */
void RegisterReplayGainApi();
