/**
 * LibraryCallback - Monitors media library changes
 * 
 * Automatically invalidates cache when library is modified.
 */

#pragma once

#include "pch.h"

/**
 * Initialize library callbacks (called from MainWindow)
 */
void InitLibraryCallbacks();
