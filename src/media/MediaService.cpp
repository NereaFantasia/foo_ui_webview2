#include "pch.h"
#include "media/MediaService.h"
#include "domain/PathSecurity.h"
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

namespace media {
namespace {
std::string RandomToken() {
    unsigned char bytes[16]{};
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG))) return {};
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for (const auto byte : bytes) {
        result += hex[byte >> 4];
        result += hex[byte & 15];
    }
    return result;
}
}
MediaTokens& Tokens() {
    static MediaTokens tokens(RandomToken, [] { return GetTickCount64(); });
    return tokens;
}
bool CanRead(const std::wstring& path) {
    std::wstring error;
    return PathSecurity::Instance().ValidateMediaAccess(path, error);
}
std::string Utf8(const std::wstring& text) {
    return pfc::stringcvt::string_utf8_from_wide(text.c_str()).get_ptr();
}
std::wstring Wide(const std::string& text) {
    return pfc::stringcvt::string_wide_from_utf8(text.c_str()).get_ptr();
}
}
