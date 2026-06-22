#pragma once

#include "BridgeCore.h"

// Register all file system related APIs
// file.read, file.write, file.exists, file.list, file.delete, file.mkdir
/** @brief Register the file.* (filesystem) API handlers. */
void RegisterFileApi();
