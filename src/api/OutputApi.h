#pragma once

#include "BridgeCore.h"

// ============================================
// Output Format Configuration API
// ============================================
// output.getDevices   - Get available output devices
// output.getActive    - Get currently active output device
// output.getSettings  - Get output format settings (bitdepth, dither)

/** @brief Register the output.* API handlers. */
void RegisterOutputApi();
