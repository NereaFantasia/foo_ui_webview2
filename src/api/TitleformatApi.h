#pragma once
#include "pch.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ============================================
// Titleformat API Registration
// ============================================

// Register all titleformat-related APIs with BridgeCore
/** @brief Register the titleformat.* API handlers. */
void RegisterTitleformatApi();
