// test_track_number.cpp - Track number extraction from filenames
// Inline reimplementation of MetadataApi.cpp static functions
#include "pch.h"
#include <algorithm>

// ============================================
// Reimplementation for testing
// ============================================
namespace reimpl {

static int ParseTrackNumberToken(const std::string& value) {
    if (value.empty() || value.length() > 3 ||
        !std::all_of(value.begin(), value.end(), ::isdigit)) {
        return 0;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return 0;
    }
}

static int ExtractLeadingTrackNumber(const std::string& filename) {
    if (filename.length() <= 2 || !std::isdigit(filename[0])) {
        return 0;
    }
    size_t endPos = 0;
    while (endPos < filename.length() && std::isdigit(filename[endPos])) {
        endPos++;
    }
    if (endPos == 0 || endPos > 3 || endPos >= filename.length()) {
        return 0;
    }
    const char separator = filename[endPos];
    if (separator != '.' && separator != ' ' && separator != '-' && separator != '_') {
        return 0;
    }
    return ParseTrackNumberToken(filename.substr(0, endPos));
}

static int ExtractWrappedTrackNumber(const std::string& filename) {
    if (filename.length() <= 3 || (filename[0] != '(' && filename[0] != '[')) {
        return 0;
    }
    const char closeChar = filename[0] == '(' ? ')' : ']';
    const size_t closePos = filename.find(closeChar);
    if (closePos == std::string::npos || closePos > 4) {
        return 0;
    }
    return ParseTrackNumberToken(filename.substr(1, closePos - 1));
}

static int ExtractTrackNumberFromFilename(const std::string& filename) {
    if (int trackNumber = ExtractLeadingTrackNumber(filename); trackNumber > 0) {
        return trackNumber;
    }
    return ExtractWrappedTrackNumber(filename);
}

} // namespace reimpl

// ============================================
// ParseTrackNumberToken
// ============================================

TEST(ParseTrackNumberToken, Empty) {
    EXPECT_EQ(reimpl::ParseTrackNumberToken(""), 0);
}

TEST(ParseTrackNumberToken, TooLong) {
    EXPECT_EQ(reimpl::ParseTrackNumberToken("1234"), 0);
}

TEST(ParseTrackNumberToken, NonDigit) {
    EXPECT_EQ(reimpl::ParseTrackNumberToken("abc"), 0);
    EXPECT_EQ(reimpl::ParseTrackNumberToken("1a"), 0);
}

TEST(ParseTrackNumberToken, ValidNumbers) {
    EXPECT_EQ(reimpl::ParseTrackNumberToken("1"), 1);
    EXPECT_EQ(reimpl::ParseTrackNumberToken("01"), 1);
    EXPECT_EQ(reimpl::ParseTrackNumberToken("99"), 99);
    EXPECT_EQ(reimpl::ParseTrackNumberToken("123"), 123);
}

TEST(ParseTrackNumberToken, Zero) {
    EXPECT_EQ(reimpl::ParseTrackNumberToken("0"), 0);
    EXPECT_EQ(reimpl::ParseTrackNumberToken("000"), 0);
}

// ============================================
// ExtractLeadingTrackNumber
// ============================================

TEST(ExtractLeadingTrackNumber, DotSeparator) {
    EXPECT_EQ(reimpl::ExtractLeadingTrackNumber("01. Song.flac"), 1);
    EXPECT_EQ(reimpl::ExtractLeadingTrackNumber("99.Track"), 99);
}

TEST(ExtractLeadingTrackNumber, DashSeparator) {
    EXPECT_EQ(reimpl::ExtractLeadingTrackNumber("05-Song Name"), 5);
}

TEST(ExtractLeadingTrackNumber, SpaceSeparator) {
    EXPECT_EQ(reimpl::ExtractLeadingTrackNumber("12 Title"), 12);
}

TEST(ExtractLeadingTrackNumber, NoSeparator) {
    EXPECT_EQ(reimpl::ExtractLeadingTrackNumber("01Song"), 0);
}

TEST(ExtractLeadingTrackNumber, TooShort) {
    EXPECT_EQ(reimpl::ExtractLeadingTrackNumber("1"), 0);
    EXPECT_EQ(reimpl::ExtractLeadingTrackNumber("ab"), 0);
}

TEST(ExtractLeadingTrackNumber, StartsWithNonDigit) {
    EXPECT_EQ(reimpl::ExtractLeadingTrackNumber("Song 01"), 0);
}

TEST(ExtractLeadingTrackNumber, FourDigitsRejected) {
    EXPECT_EQ(reimpl::ExtractLeadingTrackNumber("1234.Song"), 0);
}

// ============================================
// ExtractWrappedTrackNumber
// ============================================

TEST(ExtractWrappedTrackNumber, Parentheses) {
    EXPECT_EQ(reimpl::ExtractWrappedTrackNumber("(01) Song.flac"), 1);
    EXPECT_EQ(reimpl::ExtractWrappedTrackNumber("(5) Track"), 5);
}

TEST(ExtractWrappedTrackNumber, Brackets) {
    EXPECT_EQ(reimpl::ExtractWrappedTrackNumber("[05] Track Name"), 5);
    EXPECT_EQ(reimpl::ExtractWrappedTrackNumber("[99] x"), 99);
}

TEST(ExtractWrappedTrackNumber, NoWrapper) {
    EXPECT_EQ(reimpl::ExtractWrappedTrackNumber("Song Name"), 0);
    EXPECT_EQ(reimpl::ExtractWrappedTrackNumber(""), 0);
}

TEST(ExtractWrappedTrackNumber, TooShort) {
    EXPECT_EQ(reimpl::ExtractWrappedTrackNumber("(1)"), 0);
}

// ============================================
// ExtractTrackNumberFromFilename (integration)
// ============================================

TEST(ExtractTrackNumberFromFilename, LeadingWins) {
    EXPECT_EQ(reimpl::ExtractTrackNumberFromFilename("01. Song.flac"), 1);
}

TEST(ExtractTrackNumberFromFilename, FallbackToWrapped) {
    EXPECT_EQ(reimpl::ExtractTrackNumberFromFilename("(07) Song.flac"), 7);
}

TEST(ExtractTrackNumberFromFilename, Neither) {
    EXPECT_EQ(reimpl::ExtractTrackNumberFromFilename("Song Name.flac"), 0);
}
