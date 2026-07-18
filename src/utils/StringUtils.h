#pragma once
// StringUtils.h - Shared string helpers for JSON-safe UTF-8 and related utilities
//
// Menu labels and plugin-provided names may contain truncated/invalid UTF-8
// (e.g. incomplete 3-byte Chinese sequences ending at 0xE6). nlohmann::json
// dump() throws type_error.316 on incomplete UTF-8; sanitize before assign.

#include <cstring>
#include <string>
#include <string_view>

namespace StringUtils {

namespace detail {

inline bool IsContinuation(unsigned char value) {
    return value >= 0x80 && value <= 0xBF;
}

inline void AppendReplacement(std::string& output) {
    output.append("\xEF\xBF\xBD");  // U+FFFD
}

inline size_t ValidSequenceLength(std::string_view input, size_t index) {
    const auto first = static_cast<unsigned char>(input[index]);
    const size_t remaining = input.size() - index;

    if (first < 0x80) return 1;

    if (first >= 0xC2 && first <= 0xDF && remaining >= 2 &&
        IsContinuation(static_cast<unsigned char>(input[index + 1]))) {
        return 2;
    }

    if (first >= 0xE0 && first <= 0xEF && remaining >= 3) {
        const auto second = static_cast<unsigned char>(input[index + 1]);
        const auto third = static_cast<unsigned char>(input[index + 2]);
        if (IsContinuation(second) && IsContinuation(third) &&
            (first != 0xE0 || second >= 0xA0) &&
            (first != 0xED || second <= 0x9F)) {
            return 3;
        }
    }

    if (first >= 0xF0 && first <= 0xF4 && remaining >= 4) {
        const auto second = static_cast<unsigned char>(input[index + 1]);
        const auto third = static_cast<unsigned char>(input[index + 2]);
        const auto fourth = static_cast<unsigned char>(input[index + 3]);
        if (IsContinuation(second) && IsContinuation(third) && IsContinuation(fourth) &&
            (first != 0xF0 || second >= 0x90) &&
            (first != 0xF4 || second <= 0x8F)) {
            return 4;
        }
    }

    return 0;
}

}  // namespace detail

// Ensure a byte string is valid UTF-8 for nlohmann::json serialization.
// Invalid or truncated sequences are replaced with U+FFFD. The validator
// rejects overlong encodings, UTF-16 surrogates, and code points above U+10FFFF.
inline std::string SafeUtf8(std::string_view input) {
    if (input.empty()) return {};

    std::string result;
    result.reserve(input.size());

    size_t index = 0;
    while (index < input.size()) {
        const size_t sequenceLength = detail::ValidSequenceLength(input, index);
        if (sequenceLength == 0) {
            detail::AppendReplacement(result);
            ++index;
            continue;
        }

        result.append(input.substr(index, sequenceLength));
        index += sequenceLength;
    }

    return result;
}

inline std::string SafeUtf8(const char* str) {
    return str ? SafeUtf8(std::string_view(str)) : std::string{};
}

}  // namespace StringUtils
