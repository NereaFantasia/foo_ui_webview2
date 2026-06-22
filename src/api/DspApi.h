#pragma once

#include "BridgeCore.h"

// ============================================
// DSP Chain Management API
// ============================================
// dsp.getChain     - Get current DSP chain configuration
// dsp.setChain     - Set DSP chain configuration
// dsp.getPresets   - Get available DSP presets
// dsp.applyPreset  - Apply a DSP preset
// dsp.getAvailable - Get list of available DSP processors
// dsp.addDsp       - Add a DSP to the chain
// dsp.removeDsp    - Remove a DSP from the chain
// dsp.moveDsp      - Move a DSP within the chain

/** @brief Register the dsp.* API handlers. */
void RegisterDspApi();
