#pragma once

#include "BridgeCore.h"
#include "api/ApiResult.h"

/** Execute a unique current main-menu leaf; dynamic leaves require the child GUID. */
std::optional<api::Failure> ExecuteMainMenuAddress(const GUID& guid, const GUID& subGuid);

// Register menu-related APIs
// menu.runMainMenuCommand, menu.runContextCommand
// menu.getMainMenu, menu.getContextMenu, menu.runContextCommandById
// menu.showNativePopup
/** @brief Register the menu.* API handlers. */
void RegisterMenuApi();
