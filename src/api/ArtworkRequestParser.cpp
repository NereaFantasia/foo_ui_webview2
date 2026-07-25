#include "pch.h"
#include "ArtworkRequestParser.h"

#include <limits>
#include <optional>

namespace artwork_request {
namespace {

constexpr std::string_view kFb2kPrefix = "fb2k://artwork/";
constexpr std::string_view kArtworkPrefix = "artwork://";
constexpr std::string_view kSameOriginPrefix = "https://foo-ui-webview2.local/fb2k-artwork/";

struct RouteMatch {
    Route route;
    size_t pathStart;
};

int HexValue(char value) noexcept {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

std::optional<RouteMatch> MatchRoute(std::string_view uri) noexcept {
    if (uri.starts_with(kFb2kPrefix)) {
        return RouteMatch{Route::Fb2kArtwork, kFb2kPrefix.size()};
    }
    if (uri.starts_with(kArtworkPrefix)) {
        return RouteMatch{Route::ArtworkScheme, kArtworkPrefix.size()};
    }
    if (uri.starts_with(kSameOriginPrefix)) {
        return RouteMatch{Route::SameOriginHttps, kSameOriginPrefix.size()};
    }
    return std::nullopt;
}

ParseError ValidateEscapes(std::string_view value) noexcept {
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '%') continue;
        if (i + 2 >= value.size()) return ParseError::MalformedPercentEscape;
        const int high = HexValue(value[i + 1]);
        const int low = HexValue(value[i + 2]);
        if (high < 0 || low < 0) return ParseError::MalformedPercentEscape;
        if ((high << 4 | low) == 0) return ParseError::DecodedNul;
        i += 2;
    }
    return ParseError::None;
}

ParseError Decode(std::string_view value, bool plusAsSpace, std::string& output) {
    output.clear();
    output.reserve(value.size());

    for (size_t i = 0; i < value.size(); ++i) {
        const char current = value[i];
        if (current == '%') {
            if (i + 2 >= value.size()) return ParseError::MalformedPercentEscape;
            const int high = HexValue(value[i + 1]);
            const int low = HexValue(value[i + 2]);
            if (high < 0 || low < 0) return ParseError::MalformedPercentEscape;
            const char decoded = static_cast<char>(high << 4 | low);
            if (decoded == '\0') return ParseError::DecodedNul;
            output.push_back(decoded);
            i += 2;
        } else if (current == '+' && plusAsSpace) {
            output.push_back(' ');
        } else {
            if (current == '\0') return ParseError::EmbeddedNul;
            output.push_back(current);
        }
    }
    return ParseError::None;
}

struct QueryValue {
    std::optional<std::string_view> value;
    bool duplicate = false;
};

QueryValue FindQueryValue(std::string_view query, std::string_view key) noexcept {
    QueryValue result;
    size_t itemStart = 0;
    while (itemStart <= query.size()) {
        const size_t itemEnd = query.find('&', itemStart);
        const std::string_view item = query.substr(
            itemStart,
            itemEnd == std::string_view::npos ? std::string_view::npos : itemEnd - itemStart);
        const size_t equals = item.find('=');
        const std::string_view itemKey = item.substr(0, equals);
        if (itemKey == key) {
            if (result.value.has_value()) {
                result.duplicate = true;
                return result;
            }
            result.value = equals == std::string_view::npos
                ? std::string_view{}
                : item.substr(equals + 1);
        }
        if (itemEnd == std::string_view::npos) break;
        itemStart = itemEnd + 1;
    }
    return result;
}

ParseError NormalizeType(std::string_view encodedType, std::string& output) {
    std::string decoded;
    if (const ParseError error = Decode(encodedType, true, decoded); error != ParseError::None) {
        return error;
    }
    if (decoded.size() > kMaxTypeBytes) return ParseError::TypeTooLong;

    if (decoded.empty() || decoded == "front" || decoded == "cover_front") {
        output = "front";
    } else if (decoded == "back" || decoded == "cover_back") {
        output = "back";
    } else if (decoded == "disc" || decoded == "icon" || decoded == "artist") {
        output = decoded;
    } else {
        return ParseError::InvalidType;
    }
    return ParseError::None;
}

ParseError ParseMaxSize(std::string_view value, int& output, MaxSizeSource& source) noexcept {
    if (value.empty()) return ParseError::EmptyMaxSize;

    unsigned int parsed = 0;
    for (const char digit : value) {
        if (digit < '0' || digit > '9') return ParseError::InvalidMaxSize;
        const unsigned int numeric = static_cast<unsigned int>(digit - '0');
        if (parsed > (std::numeric_limits<unsigned int>::max() - numeric) / 10) {
            return ParseError::MaxSizeOverflow;
        }
        parsed = parsed * 10 + numeric;
    }
    if (parsed > static_cast<unsigned int>(kMaxSizeLimit)) return ParseError::MaxSizeOutOfRange;

    output = static_cast<int>(parsed);
    source = parsed == 0 ? MaxSizeSource::ExplicitZero : MaxSizeSource::ExplicitPositive;
    return ParseError::None;
}

ParseError NormalizeMaxSize(
    std::optional<std::int64_t> value,
    int& output,
    MaxSizeSource& source) noexcept {
    if (!value.has_value()) {
        output = 0;
        source = MaxSizeSource::Missing;
        return ParseError::None;
    }
    if (*value < 0) return ParseError::InvalidMaxSize;
    if (*value > kMaxSizeLimit) return ParseError::MaxSizeOutOfRange;
    output = static_cast<int>(*value);
    source = output == 0 ? MaxSizeSource::ExplicitZero : MaxSizeSource::ExplicitPositive;
    return ParseError::None;
}

ParseResult Failure(ParseError error) noexcept {
    ParseResult result;
    result.error = error;
    return result;
}

} // namespace

int ParseResult::httpStatus() const noexcept {
    if (error == ParseError::None) return 200;
    return error == ParseError::UriTooLong ? 414 : 400;
}

std::string UrlEncode(std::string_view value) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size());

    for (const unsigned char current : value) {
        const bool unreserved =
            (current >= 'A' && current <= 'Z') ||
            (current >= 'a' && current <= 'z') ||
            (current >= '0' && current <= '9') ||
            current == '-' || current == '_' || current == '.' || current == '~';
        if (unreserved) {
            encoded.push_back(static_cast<char>(current));
        } else {
            encoded.push_back('%');
            encoded.push_back(kHex[current >> 4]);
            encoded.push_back(kHex[current & 0x0F]);
        }
    }
    return encoded;
}

UrlBuildResult BuildFb2kArtworkUrl(
    std::string_view path,
    std::string_view type,
    std::optional<std::int64_t> maxSize) {
    UrlBuildResult result;
    if (path.empty()) {
        result.error = ParseError::MissingPath;
        return result;
    }
    if (path.find('\0') != std::string_view::npos) {
        result.error = ParseError::EmbeddedNul;
        return result;
    }
    if (path.size() > kMaxDecodedPathBytes) {
        result.error = ParseError::PathTooLong;
        return result;
    }
    if (const ParseError error = NormalizeType(type, result.type); error != ParseError::None) {
        result.error = error;
        return result;
    }
    if (const ParseError error = NormalizeMaxSize(
            maxSize, result.maxSize, result.maxSizeSource);
        error != ParseError::None) {
        result.error = error;
        return result;
    }

    result.url = "fb2k://artwork/?path=";
    result.url += UrlEncode(path);
    result.url += "&type=";
    result.url += result.type;
    if (maxSize.has_value()) {
        result.url += "&maxSize=";
        result.url += std::to_string(result.maxSize);
    }
    return result;
}

ParseResult Parse(std::string_view uri) {
    if (uri.size() > kMaxUriBytes) return Failure(ParseError::UriTooLong);
    if (uri.find('\0') != std::string_view::npos) return Failure(ParseError::EmbeddedNul);

    const std::optional<RouteMatch> route = MatchRoute(uri);
    if (!route) return Failure(ParseError::UnsupportedRoute);

    const size_t queryMarker = uri.find('?', route->pathStart);
    const std::string_view encodedPathComponent = uri.substr(
        route->pathStart,
        queryMarker == std::string_view::npos ? std::string_view::npos : queryMarker - route->pathStart);
    const std::string_view query = queryMarker == std::string_view::npos
        ? std::string_view{}
        : uri.substr(queryMarker + 1);

    if (query.size() > kMaxQueryBytes) return Failure(ParseError::QueryTooLong);
    if (const ParseError error = ValidateEscapes(encodedPathComponent); error != ParseError::None) {
        return Failure(error);
    }
    if (const ParseError error = ValidateEscapes(query); error != ParseError::None) {
        return Failure(error);
    }

    ParseResult result;
    result.request.route = route->route;

    const QueryValue queryPath = FindQueryValue(query, "path");
    const QueryValue queryType = FindQueryValue(query, "type");
    const QueryValue queryMaxSize = FindQueryValue(query, "maxSize");
    if (queryPath.duplicate || queryType.duplicate || queryMaxSize.duplicate) {
        return Failure(ParseError::DuplicateParameter);
    }

    const std::string_view encodedPath = queryPath.value
        ? *queryPath.value
        : encodedPathComponent;
    const bool pathIsQueryValue = queryPath.value.has_value();
    if (const ParseError error = Decode(encodedPath, pathIsQueryValue, result.request.path);
        error != ParseError::None) {
        return Failure(error);
    }
    if (result.request.path.empty()) return Failure(ParseError::MissingPath);
    if (result.request.path.size() > kMaxDecodedPathBytes) return Failure(ParseError::PathTooLong);

    if (queryType.value) {
        if (const ParseError error = NormalizeType(*queryType.value, result.request.type); error != ParseError::None) {
            return Failure(error);
        }
    }

    if (queryMaxSize.value) {
        std::string decodedMaxSize;
        if (const ParseError error = Decode(*queryMaxSize.value, true, decodedMaxSize);
            error != ParseError::None) {
            return Failure(error);
        }
        if (const ParseError error = ParseMaxSize(
                decodedMaxSize, result.request.maxSize, result.request.maxSizeSource);
            error != ParseError::None) {
            return Failure(error);
        }
    }

    return result;
}

const char* ParseErrorName(ParseError error) noexcept {
    switch (error) {
    case ParseError::None: return "none";
    case ParseError::UriTooLong: return "uri_too_long";
    case ParseError::UnsupportedRoute: return "unsupported_route";
    case ParseError::QueryTooLong: return "query_too_long";
    case ParseError::EmbeddedNul: return "embedded_nul";
    case ParseError::MalformedPercentEscape: return "malformed_percent_escape";
    case ParseError::DecodedNul: return "decoded_nul";
    case ParseError::MissingPath: return "missing_path";
    case ParseError::PathTooLong: return "path_too_long";
    case ParseError::TypeTooLong: return "type_too_long";
    case ParseError::InvalidType: return "invalid_type";
    case ParseError::EmptyMaxSize: return "empty_max_size";
    case ParseError::InvalidMaxSize: return "invalid_max_size";
    case ParseError::MaxSizeOverflow: return "max_size_overflow";
    case ParseError::MaxSizeOutOfRange: return "max_size_out_of_range";
    case ParseError::DuplicateParameter: return "duplicate_parameter";
    }
    return "unknown";
}

} // namespace artwork_request
