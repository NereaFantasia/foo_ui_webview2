// IconLoader.cpp - base64 icon decoding
#include "pch.h"
#include "utils/IconLoader.h"

std::unordered_map<std::string, HICON> IconLoader::s_cache;

static std::vector<uint8_t> Base64Decode(const std::string& input) {
    // int (not int8_t) to avoid bugprone-signed-char-misuse on kTable[c]
    // sign-extension; sentinel -1 marks invalid base64 characters.
    static const int kTable[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };

    std::vector<uint8_t> out;
    out.reserve(input.size() * 3 / 4);
    int val = 0, bits = -8;
    for (unsigned char c : input) {
        int v = kTable[c];
        if (v < 0) continue;
        val = (val << 6) + v;
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

HICON IconLoader::FromBase64(const std::string& base64) {
    if (base64.empty()) return nullptr;

    auto it = s_cache.find(base64);
    if (it != s_cache.end()) return it->second;

    std::vector<uint8_t> data = Base64Decode(base64);
    if (data.empty()) return nullptr;

    HICON hIcon = nullptr;
    // ICO: first ICONDIRENTRY starts at offset 6.
    if (data.size() >= 22) {
        DWORD size = 0;
        DWORD offset = 0;
        memcpy(&size, data.data() + 14, sizeof(size));
        memcpy(&offset, data.data() + 18, sizeof(offset));
        if (size > 0 && offset <= data.size() && size <= data.size() - offset) {
            hIcon = CreateIconFromResourceEx(
                data.data() + offset, size,
                TRUE, 0x00030000,
                0, 0, LR_DEFAULTCOLOR
            );
        }
    }

    if (hIcon) s_cache[base64] = hIcon;
    return hIcon;
}

void IconLoader::Clear() {
    for (auto& [key, hIcon] : s_cache) {
        if (hIcon) DestroyIcon(hIcon);
    }
    s_cache.clear();
}
