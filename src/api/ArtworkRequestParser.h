#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace artwork_request {

inline constexpr size_t kMaxUriBytes = 16 * 1024;
inline constexpr size_t kMaxQueryBytes = 8 * 1024;
inline constexpr size_t kMaxDecodedPathBytes = 4096;
inline constexpr size_t kMaxTypeBytes = 32;
inline constexpr int kMaxSizeLimit = 2048;

enum class Route {
    Fb2kArtwork,
    ArtworkScheme,
    SameOriginHttps,
};

enum class MaxSizeSource {
    Missing,
    ExplicitZero,
    ExplicitPositive,
};

enum class ParseError {
    None,
    UriTooLong,
    UnsupportedRoute,
    QueryTooLong,
    EmbeddedNul,
    MalformedPercentEscape,
    DecodedNul,
    MissingPath,
    PathTooLong,
    TypeTooLong,
    InvalidType,
    EmptyMaxSize,
    InvalidMaxSize,
    MaxSizeOverflow,
    MaxSizeOutOfRange,
    DuplicateParameter,
};

struct Request {
    Route route = Route::Fb2kArtwork;
    std::string path;
    std::string type = "front";
    int maxSize = 0;
    MaxSizeSource maxSizeSource = MaxSizeSource::Missing;
};

struct ParseResult {
    Request request;
    ParseError error = ParseError::None;

    [[nodiscard]] bool ok() const noexcept { return error == ParseError::None; }
    [[nodiscard]] int httpStatus() const noexcept;
};

struct UrlBuildResult {
    std::string url;
    std::string type = "front";
    int maxSize = 0;
    MaxSizeSource maxSizeSource = MaxSizeSource::Missing;
    ParseError error = ParseError::None;

    [[nodiscard]] bool ok() const noexcept { return error == ParseError::None; }
};

// Percent-encodes a complete media path as one URI component.
[[nodiscard]] std::string UrlEncode(std::string_view value);

// Builds a canonical fb2k:// artwork URL using the same type and size contract
// as Parse(). A present maxSize keeps explicit zero distinguishable from a
// missing value in the generated URL.
[[nodiscard]] UrlBuildResult BuildFb2kArtworkUrl(
    std::string_view path,
    std::string_view type,
    std::optional<std::int64_t> maxSize);

// Parses and normalizes all supported artwork resource routes.
[[nodiscard]] ParseResult Parse(std::string_view uri);

[[nodiscard]] const char* ParseErrorName(ParseError error) noexcept;

} // namespace artwork_request
