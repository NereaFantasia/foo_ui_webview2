#include "pch.h"
#include <gtest/gtest.h>
#include "api/LyricsFilePolicy.h"

namespace {
namespace policy = lyrics_file_policy;

policy::FileContext Track(bool wholeFile = true) {
    return { L"C:\\music\\audio.wav", wholeFile, L"First Artist", L"Title" };
}

std::vector<std::wstring> CandidateNames(const policy::FileContext& track,
                                         std::wstring_view filename, std::string_view format) {
    std::vector<std::wstring> names;
    for (const auto& path : policy::BuildFileCandidates(track, filename, format)) {
        names.push_back(path.wstring());
    }
    return names;
}

TEST(LyricsFilePolicy, RecognizesOnlyTheFiveKnownTagsInPriorityOrder) {
    const std::vector<std::string> actual(policy::kKnownLyricsTags.begin(), policy::kKnownLyricsTags.end());
    EXPECT_EQ(actual, (std::vector<std::string>{
        "LYRICS", "UNSYNCED LYRICS", "UNSYNCEDLYRICS", "SYNCEDLYRICS", "SYNCED LYRICS" }));
}

TEST(LyricsFilePolicy, RequiresPositiveProofOfAWholeFileTrack) {
    EXPECT_TRUE(policy::IsWholeFileSingleTrack(0, 1, 0));
    EXPECT_FALSE(policy::IsWholeFileSingleTrack(7, 1, 0));
    EXPECT_FALSE(policy::IsWholeFileSingleTrack(0, 2, 0));
    EXPECT_FALSE(policy::IsWholeFileSingleTrack(0, 1, 7));
    EXPECT_FALSE(policy::IsWholeFileSingleTrack(0, 0, std::nullopt));
    EXPECT_FALSE(policy::IsWholeFileSingleTrack(0, std::nullopt, std::nullopt));
}

TEST(LyricsFilePolicy, AcceptsEmptyOrPlainUnicodeFilenames) {
    for (const auto* name : { L"", L"track.lrc", L"alternate.words", L"CONCERT.lrc", L"COM10.txt", L"\u6b4c\u8bcd.txt" }) {
        EXPECT_TRUE(policy::IsValidFilename(name));
    }
}

TEST(LyricsFilePolicy, RejectsWindowsPathSyntaxAndInvalidCharacters) {
    for (const auto* name : { L".", L"..", L"../track.lrc", L"dir\\track.lrc", L"/track.lrc",
                             L"C:track.lrc", L"track.lrc:stream", L"a*b", L"a?b", L"a\"b",
                             L"a<b", L"a>b", L"a|b", L"track.lrc ", L"track.lrc.", L"a\nb" }) {
        EXPECT_FALSE(policy::IsValidFilename(name));
    }
    EXPECT_FALSE(policy::IsValidFilename(std::wstring(L"a\0b.lrc", 7)));
}

TEST(LyricsFilePolicy, RejectsReservedDevicesWithAnyExtension) {
    for (const auto* name : { L"CON", L"prn.lrc", L"AUX.foo.bar", L"NUL.txt", L"COM1.lrc",
                             L"lpt9.txt", L"COM\u00b9.lrc", L"LPT\u00b2.txt", L"COM\u00b3.lrc",
                             L"CONIN$.lrc", L"CONOUT$.txt", L"CON .lrc" }) {
        EXPECT_FALSE(policy::IsValidFilename(name));
    }
}

TEST(LyricsFilePolicy, OrdersEveryLrcCandidateBeforeTxtCandidates) {
    EXPECT_EQ(CandidateNames(Track(), L"", "any"),
        (std::vector<std::wstring>{ L"C:\\music\\audio.lrc", L"C:\\music\\First Artist - Title.lrc",
                                            L"C:\\music\\audio.txt", L"C:\\music\\First Artist - Title.txt" }));
}

TEST(LyricsFilePolicy, UnknownOrContainerTracksOnlyUseTheExactTagName) {
    EXPECT_EQ(CandidateNames(Track(false), L"", "lrc"),
        (std::vector<std::wstring>{ L"C:\\music\\First Artist - Title.lrc" }));
}

TEST(LyricsFilePolicy, ExplicitFilenameIsTheOnlyCandidateRegardlessOfFormat) {
    EXPECT_EQ(CandidateNames(Track(false), L"chosen.words", "txt"),
        (std::vector<std::wstring>{ L"C:\\music\\chosen.words" }));
}

TEST(LyricsFilePolicy, InvalidExplicitFilenameNeverFallsBackToAutomaticCandidates) {
    EXPECT_TRUE(policy::BuildFileCandidates(Track(), L"..\\bad.lrc", "any").empty());
}

TEST(LyricsFilePolicy, MissingTagsNeedAnExplicitFilenameUnlessWholeFileIsProven) {
    auto track = Track(false);
    track.artist.clear();
    EXPECT_TRUE(policy::BuildFileCandidates(track, L"", "lrc").empty());
    track = Track(false);
    track.title.clear();
    EXPECT_TRUE(policy::BuildFileCandidates(track, L"", "txt").empty());
    track.wholeFileSingleTrack = true;
    EXPECT_EQ(CandidateNames(track, L"", "txt"),
        (std::vector<std::wstring>{ L"C:\\music\\audio.txt" }));
}

TEST(LyricsFilePolicy, ReplacesInvalidTagCharactersWithoutFuzzyMatching) {
    auto track = Track(false);
    track.artist = L"Artist/One";
    track.title = L"Title:Part?";
    EXPECT_EQ(CandidateNames(track, L"", "lrc"),
        (std::vector<std::wstring>{ L"C:\\music\\Artist_One - Title_Part_.lrc" }));
}

TEST(LyricsFilePolicy, DeduplicatesTheAudioAndTaggedCandidates) {
    auto track = Track();
    track.audioPath = L"C:\\music\\First Artist - Title.wav";
    EXPECT_EQ(CandidateNames(track, L"", "any"),
        (std::vector<std::wstring>{ L"C:\\music\\First Artist - Title.lrc",
                                            L"C:\\music\\First Artist - Title.txt" }));
}

} // namespace
