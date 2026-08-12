#pragma once

// Test project precompiled header
// Keep minimal -- only widely used test infrastructure headers

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <cstdint>
#include <random>
#include <mutex>

// Drag-and-drop tests build DROPFILES blobs by hand, so they need the Win32
// and Shell declarations. DROPFILES itself lives in ShlObj_core.h; shellapi.h
// only brings the DragQueryFile family. The test project uses this PCH
// exclusively and never sees src/pch.h.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>
#include <ShlObj_core.h>

#define LOG(...) ((void)0)
