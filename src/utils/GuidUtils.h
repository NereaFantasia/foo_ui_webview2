#pragma once
// GuidUtils.h - GUID <-> string conversion helpers
// Shared across MenuApi, ConfigApi, DiscoveryApi to avoid duplication

#include <string>
#include <cstdio>
#include <Windows.h>

namespace GuidUtils {

inline std::string GuidToString(const GUID& guid) {
    char buf[64];
    sprintf_s(buf, "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return buf;
}

inline bool StringToGuid(const std::string& str, GUID& guid) {
    unsigned int d1, d2, d3;
    unsigned int d4[8];
    int consumed = 0;
    int result = sscanf_s(str.c_str(),
        "{%8X-%4X-%4X-%2X%2X-%2X%2X%2X%2X%2X%2X}%n",
        &d1, &d2, &d3,
        &d4[0], &d4[1], &d4[2], &d4[3],
        &d4[4], &d4[5], &d4[6], &d4[7],
        &consumed);

    if (result != 11) return false;
    // Reject trailing garbage after the closing '}'
    if (consumed == 0 || static_cast<size_t>(consumed) != str.size()) return false;

    guid.Data1 = d1;
    guid.Data2 = (unsigned short)d2;
    guid.Data3 = (unsigned short)d3;
    for (int i = 0; i < 8; i++) {
        guid.Data4[i] = (unsigned char)d4[i];
    }
    return true;
}

} // namespace GuidUtils
