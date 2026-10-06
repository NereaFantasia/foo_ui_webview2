// DragOutTokens.cpp
#include "pch.h"
#include "webview/dnd/DragOutTokens.h"

#include <bcrypt.h>

#include <chrono>
#include <string>

#pragma comment(lib, "bcrypt.lib")

namespace fb2k_dnd {
namespace {

int64_t MonotonicMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// 128 bits, hex encoded. Wide enough that guessing is not a strategy, short
// enough to sit inside a dataTransfer string.
constexpr size_t kTokenBytes = 16;

// Returns an empty string when the platform cannot supply entropy, which the
// store turns into a failed mint.
//
// Deliberately not the mt19937_64 pattern used for operation ids elsewhere in
// src/api: those are correlation handles, whereas this token is the only thing
// authorising a drag of already-validated paths, and a Mersenne Twister becomes
// fully predictable once enough of its output has been observed. Falling back to
// a weaker source on failure would turn the token from an authorisation into a
// formality, so failure is reported instead.
std::string NewToken() {
    unsigned char bytes[kTokenBytes] = {};
    const NTSTATUS status = ::BCryptGenRandom(
        nullptr, bytes, static_cast<ULONG>(sizeof(bytes)), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(status)) {
        return {};
    }

    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(sizeof(bytes) * 2);
    for (unsigned char byte : bytes) {
        out.push_back(kHex[byte >> 4]);
        out.push_back(kHex[byte & 0x0F]);
    }
    return out;
}

}  // namespace

DragTokenStore& DragOutTokenStore() {
    // Function-local static, so construction happens on first use and stays out
    // of static initialisation order.
    static DragTokenStore store(&MonotonicMs, &NewToken);
    return store;
}

}  // namespace fb2k_dnd
