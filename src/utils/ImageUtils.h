#pragma once

#include <vector>
#include <cstddef>

// Resize image using Windows Imaging Component (WIC)
// - If maxSize <= 0, returns empty vector
// - If resize is not needed, returns empty vector (caller should use original data)
// - On success, returns resized JPEG data in the vector
std::vector<uint8_t> ResizeImageWIC(
    const uint8_t* srcData,
    size_t srcLen,
    int maxSize,
    int& outWidth,
    int& outHeight,
    const char*& outMimeType);
