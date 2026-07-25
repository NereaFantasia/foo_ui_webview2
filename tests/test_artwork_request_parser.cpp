// test_artwork_request_parser.cpp - Artwork resource URI contract tests
#include "pch.h"
#include "../src/api/ArtworkRequestParser.h"

using artwork_request::MaxSizeSource;
using artwork_request::ParseError;
using artwork_request::Route;

// ---------------------------------------------------------------------------
// Route parsing
// ---------------------------------------------------------------------------

TEST(ArtworkRequestParser, ParsesSupportedRoutes) {
    struct ValidRouteCase {
        const char* uri;
        Route route;
        const char* path;
    };
    static const ValidRouteCase cases[] = {
        {"fb2k://artwork/E%3A%5CMusic%5Ctrack.flac",
            Route::Fb2kArtwork, "E:\\Music\\track.flac"},
        {"fb2k://artwork/?path=E%3A%5CMusic%5Ctrack.flac",
            Route::Fb2kArtwork, "E:\\Music\\track.flac"},
        {"artwork://E%3A%5CMusic%5Ctrack.flac",
            Route::ArtworkScheme, "E:\\Music\\track.flac"},
        {"https://foo-ui-webview2.local/fb2k-artwork/E%3A%5CMusic%5Ctrack.flac",
            Route::SameOriginHttps, "E:\\Music\\track.flac"},
    };
    for (const auto& c : cases) {
        const auto result = artwork_request::Parse(c.uri);
        ASSERT_TRUE(result.ok()) << c.uri << ": " << artwork_request::ParseErrorName(result.error);
        EXPECT_EQ(result.httpStatus(), 200) << c.uri;
        EXPECT_EQ(result.request.route, c.route) << c.uri;
        EXPECT_EQ(result.request.path, c.path) << c.uri;
        EXPECT_EQ(result.request.type, "front") << c.uri;
        EXPECT_EQ(result.request.maxSize, 0) << c.uri;
        EXPECT_EQ(result.request.maxSizeSource, MaxSizeSource::Missing) << c.uri;
    }
}

// ---------------------------------------------------------------------------
// Type normalization
// ---------------------------------------------------------------------------

TEST(ArtworkRequestParser, NormalizesAcceptedTypes) {
    struct TypeCase {
        const char* input;
        const char* normalized;
    };
    static const TypeCase cases[] = {
        {"", "front"},
        {"front", "front"},
        {"cover_front", "front"},
        {"back", "back"},
        {"cover_back", "back"},
        {"disc", "disc"},
        {"icon", "icon"},
        {"artist", "artist"},
    };
    for (const auto& c : cases) {
        const std::string uri = std::string("artwork://track?type=") + c.input;
        const auto result = artwork_request::Parse(uri);
        ASSERT_TRUE(result.ok()) << "type=" << c.input << ": "
            << artwork_request::ParseErrorName(result.error);
        EXPECT_EQ(result.request.type, c.normalized) << "type=" << c.input;
    }
}

// ---------------------------------------------------------------------------
// maxSize contract
// ---------------------------------------------------------------------------

TEST(ArtworkRequestParser, AcceptsFrozenMaxSizeContract) {
    struct MaxSizeCase {
        const char* input;
        int normalized;
        MaxSizeSource source;
    };
    static const MaxSizeCase cases[] = {
        {"0",    0,    MaxSizeSource::ExplicitZero},
        {"000",  0,    MaxSizeSource::ExplicitZero},
        {"1",    1,    MaxSizeSource::ExplicitPositive},
        {"2048", 2048, MaxSizeSource::ExplicitPositive},
    };
    for (const auto& c : cases) {
        const std::string uri = std::string("artwork://track?maxSize=") + c.input;
        const auto result = artwork_request::Parse(uri);
        ASSERT_TRUE(result.ok()) << "maxSize=" << c.input << ": "
            << artwork_request::ParseErrorName(result.error);
        EXPECT_EQ(result.request.maxSize, c.normalized) << "maxSize=" << c.input;
        EXPECT_EQ(result.request.maxSizeSource, c.source) << "maxSize=" << c.input;
    }
}

// ---------------------------------------------------------------------------
// Rejection matrix
// ---------------------------------------------------------------------------

TEST(ArtworkRequestParser, RejectsInvalidInput) {
    struct InvalidCase {
        std::string uri;
        ParseError error;
        int status;
    };
    const std::vector<InvalidCase> cases = {
        {"https://example.com/artwork/track",                     ParseError::UnsupportedRoute,       400},
        {"artwork://",                                            ParseError::MissingPath,             400},
        {"artwork://?path=",                                      ParseError::MissingPath,             400},
        {"artwork://track%",                                      ParseError::MalformedPercentEscape,  400},
        {"artwork://track%2",                                     ParseError::MalformedPercentEscape,  400},
        {"artwork://track%XZ",                                    ParseError::MalformedPercentEscape,  400},
        {"artwork://track%00",                                    ParseError::DecodedNul,              400},
        {"artwork://track?type=unknown",                          ParseError::InvalidType,             400},
        {"artwork://track?type=" + std::string(33, 'a'),          ParseError::TypeTooLong,             400},
        {"artwork://track?maxSize=",                              ParseError::EmptyMaxSize,            400},
        {"artwork://track?maxSize=-1",                            ParseError::InvalidMaxSize,          400},
        {"artwork://track?maxSize=12px",                          ParseError::InvalidMaxSize,          400},
        {"artwork://track?maxSize=1.5",                           ParseError::InvalidMaxSize,          400},
        {"artwork://track?maxSize=42949672960",                   ParseError::MaxSizeOverflow,         400},
        {"artwork://track?maxSize=2049",                          ParseError::MaxSizeOutOfRange,       400},
        {"artwork://track?path=a&path=b",                         ParseError::DuplicateParameter,      400},
        {"artwork://track?type=front&type=back",                  ParseError::DuplicateParameter,      400},
        {"artwork://track?maxSize=1&maxSize=2",                   ParseError::DuplicateParameter,      400},
        {"artwork://" + std::string(4097, 'p'),                   ParseError::PathTooLong,             400},
        {"artwork://track?x=" + std::string(artwork_request::kMaxQueryBytes + 1, 'q'),
                                                                  ParseError::QueryTooLong,            400},
        {"artwork://" + std::string(artwork_request::kMaxUriBytes, 'u'),
                                                                  ParseError::UriTooLong,              414},
    };
    for (const auto& c : cases) {
        const auto result = artwork_request::Parse(c.uri);
        EXPECT_FALSE(result.ok()) << "expected error for: " << c.uri.substr(0, 60);
        EXPECT_EQ(result.error, c.error)
            << "uri=" << c.uri.substr(0, 60)
            << " got=" << artwork_request::ParseErrorName(result.error);
        EXPECT_EQ(result.httpStatus(), c.status)
            << "uri=" << c.uri.substr(0, 60);
    }
}

TEST(ArtworkRequestParser, MissingMaxSizeRemainsLegacyZero) {
    const auto result = artwork_request::Parse("artwork://track");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.request.maxSize, 0);
    EXPECT_EQ(result.request.maxSizeSource, MaxSizeSource::Missing);
}

TEST(ArtworkRequestParser, PathComponentKeepsPlusLiteral) {
    const auto result = artwork_request::Parse("artwork://A+B.flac");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.request.path, "A+B.flac");
}

TEST(ArtworkRequestParser, QueryPathUsesFormCompatiblePlusAsSpace) {
    const auto result = artwork_request::Parse("fb2k://artwork/?path=A+B.flac");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.request.path, "A B.flac");
}

TEST(ArtworkRequestParser, KeepsExactPositiveSizeWithoutBucketing) {
    const auto result = artwork_request::Parse("artwork://track?maxSize=257");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.request.maxSize, 257);
}

TEST(ArtworkRequestParser, DecodesPercentEncodedMaxSize) {
    const auto result = artwork_request::Parse("artwork://track?maxSize=%32%35%37");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.request.maxSize, 257);
}

TEST(ArtworkRequestParser, AppliesTypeAndSizeContractToEveryRoute) {
    constexpr const char* routes[] = {
        "fb2k://artwork/track?type=cover_back&maxSize=512",
        "artwork://track?type=cover_back&maxSize=512",
        "https://foo-ui-webview2.local/fb2k-artwork/track?type=cover_back&maxSize=512",
    };
    for (const char* uri : routes) {
        const auto result = artwork_request::Parse(uri);
        ASSERT_TRUE(result.ok()) << uri;
        EXPECT_EQ(result.request.type, "back");
        EXPECT_EQ(result.request.maxSize, 512);
    }
}

TEST(ArtworkRequestParser, CanonicalBuilderRoundTripsWindowsCueUtf8AndPlusPath) {
    const std::string path = "E:\\Music\\\xE9\x9F\xB3\xE4\xB9\x90+A.flac|subsong:2";
    const auto built = artwork_request::BuildFb2kArtworkUrl(path, "cover_front", 300);
    ASSERT_TRUE(built.ok()) << artwork_request::ParseErrorName(built.error);
    EXPECT_EQ(built.type, "front");

    const auto parsed = artwork_request::Parse(built.url);
    ASSERT_TRUE(parsed.ok()) << artwork_request::ParseErrorName(parsed.error);
    EXPECT_EQ(parsed.request.path, path);
    EXPECT_EQ(parsed.request.type, "front");
    EXPECT_EQ(parsed.request.maxSize, 300);
}

TEST(ArtworkRequestParser, CanonicalBuilderPreservesExplicitZero) {
    const auto built = artwork_request::BuildFb2kArtworkUrl("track.flac", "front", 0);
    ASSERT_TRUE(built.ok());
    EXPECT_NE(built.url.find("&maxSize=0"), std::string::npos);
    const auto parsed = artwork_request::Parse(built.url);
    ASSERT_TRUE(parsed.ok());
    EXPECT_EQ(parsed.request.maxSizeSource, MaxSizeSource::ExplicitZero);
}

TEST(ArtworkRequestParser, AcceptsDecodedPathAtLimit) {
    const auto result = artwork_request::Parse("artwork://" + std::string(4096, 'p'));
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.request.path.size(), 4096u);
}

TEST(ArtworkRequestParser, AcceptsQueryAtLimit) {
    const std::string padding(artwork_request::kMaxQueryBytes - 2, 'q');
    const auto result = artwork_request::Parse("artwork://track?x=" + padding);
    ASSERT_TRUE(result.ok());
}

TEST(ArtworkRequestParser, RejectsEmbeddedNul) {
    std::string uri = "artwork://track";
    uri.push_back('\0');
    const auto result = artwork_request::Parse(uri);
    EXPECT_EQ(result.error, ParseError::EmbeddedNul);
}
