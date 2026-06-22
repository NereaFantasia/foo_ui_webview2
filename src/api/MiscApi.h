#pragma once

#include "BridgeCore.h"

// Register all misc/app related APIs
// misc.getFoobarPath, misc.getProfilePath, misc.getComponentPath,
// misc.showConsole, misc.showPreferences, misc.showLibrarySearch, misc.showPopupMessage,
// misc.restart, misc.exit
/** @brief Register the misc.* API handlers. */
void RegisterMiscApi();
