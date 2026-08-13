#include "pch.h"
#include "../src/utils/PlaylistFormatUtils.h"

TEST(PlaylistFormatUtils, BuiltInExtensionsMatchWithoutRegisteredExtensions) {
    const PlaylistFormatUtils::ExtensionSet registeredExtensions;
    constexpr const char* locations[] = {
        "playlist.pls",
        "playlist.m3u",
        "playlist.m3u8",
        "playlist.asx",
        "playlist.wpl",
        "playlist.xspf",
        "playlist.fpl",
        "playlist.cue",
    };

    for (const char* location : locations) {
        EXPECT_TRUE(PlaylistFormatUtils::LooksLikePlaylistWrapper(
            location, registeredExtensions))
            << location;
    }
}

TEST(PlaylistFormatUtils, RegisteredExtensionAcceptsLoaderAndNormalizedForms) {
    const PlaylistFormatUtils::ExtensionSet loaderStyle{"B4S"};
    const PlaylistFormatUtils::ExtensionSet normalizedStyle{".b4s"};

    EXPECT_TRUE(PlaylistFormatUtils::LooksLikePlaylistWrapper(
        "radio.b4s", loaderStyle));
    EXPECT_TRUE(PlaylistFormatUtils::LooksLikePlaylistWrapper(
        "radio.B4S", loaderStyle));
    EXPECT_TRUE(PlaylistFormatUtils::LooksLikePlaylistWrapper(
        "radio.b4s", normalizedStyle));
}

TEST(PlaylistFormatUtils, KnownExtensionIgnoresQueryAndFragment) {
    const PlaylistFormatUtils::ExtensionSet registeredExtensions;

    EXPECT_TRUE(PlaylistFormatUtils::LooksLikePlaylistWrapper(
        "https://example.test/list.m3u8?token=abc", registeredExtensions));
    EXPECT_TRUE(PlaylistFormatUtils::LooksLikePlaylistWrapper(
        "https://example.test/list.xspf#chapter", registeredExtensions));
}

TEST(PlaylistFormatUtils, HashInLocalFilenameIsNotAUrlFragment) {
    const PlaylistFormatUtils::ExtensionSet registeredExtensions;

    EXPECT_TRUE(PlaylistFormatUtils::LooksLikePlaylistWrapper(
        R"(C:\Music\rock#live.m3u)", registeredExtensions));
}

TEST(PlaylistFormatUtils, ExtendedLengthPathQuestionMarkIsNotAQuery) {
    const PlaylistFormatUtils::ExtensionSet registeredExtensions;

    EXPECT_TRUE(PlaylistFormatUtils::LooksLikePlaylistWrapper(
        R"(\\?\C:\Music\list.m3u)", registeredExtensions));
}

TEST(PlaylistFormatUtils, FileProtocolHashRemainsPartOfLocalFilename) {
    const PlaylistFormatUtils::ExtensionSet registeredExtensions;

    EXPECT_TRUE(PlaylistFormatUtils::LooksLikePlaylistWrapper(
        R"(file://C:\Music\rock#live.m3u)", registeredExtensions));
}

TEST(PlaylistFormatUtils, DotInDirectoryDoesNotCountAsFileExtension) {
    const PlaylistFormatUtils::ExtensionSet registeredExtensions;

    EXPECT_FALSE(PlaylistFormatUtils::LooksLikePlaylistWrapper(
        R"(C:\Albums.2026\playlist)", registeredExtensions));
}

TEST(PlaylistFormatUtils, LocationWithoutExtensionDoesNotMatch) {
    const PlaylistFormatUtils::ExtensionSet registeredExtensions;

    EXPECT_FALSE(PlaylistFormatUtils::LooksLikePlaylistWrapper(
        R"(C:\Music\playlist)", registeredExtensions));
}

TEST(PlaylistFormatUtils, OrdinaryMediaExtensionDoesNotMatch) {
    const PlaylistFormatUtils::ExtensionSet registeredExtensions;

    EXPECT_FALSE(PlaylistFormatUtils::LooksLikePlaylistWrapper(
        R"(C:\Music\track.flac)", registeredExtensions));
}

TEST(PlaylistFormatUtils, DotBeforeLastPathSeparatorDoesNotMatch) {
    const PlaylistFormatUtils::ExtensionSet registeredExtensions;

    EXPECT_FALSE(PlaylistFormatUtils::LooksLikePlaylistWrapper(
        "https://media.example.test/live/stream", registeredExtensions));
}
