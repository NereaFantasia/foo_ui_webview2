#include "pch.h"

#include "../src/utils/ArtworkCacheKey.h"

TEST(ArtworkCacheKey, ExternalSourcePathSharedAcrossTrackRequests) {
    const std::string coverPath = R"(E:\OST\void\(C81) void - Altersist (FLAC)\cover.png)";
    const std::string track1 = R"(E:\OST\void\(C81) void - Altersist (FLAC)\01. void — Arctic Embrace.flac)";
    const std::string track2 = R"(E:\OST\void\(C81) void - Altersist (FLAC)\14. void — the war is over..flac)";

    const std::string key1 = BuildScaledArtworkCacheKey(track1, "front", 300, {coverPath});
    const std::string key2 = BuildScaledArtworkCacheKey(track2, "front", 300, {coverPath});

    EXPECT_EQ(key1, key2);
}

TEST(ArtworkCacheKey, EmbeddedArtworkFallbackStaysTrackSpecific) {
    const std::string track1 = R"(E:\OST\Diverse System\[DVSP-0068] AD：TRANCE (C80)\01. t+pazolite — The Biggest Roaster.flac)";
    const std::string track2 = R"(E:\OST\Diverse System\[DVSP-0068] AD：TRANCE (C80)\02. Xacla — The surreal sky.flac)";

    const std::string key1 = BuildScaledArtworkCacheKey(track1, "front", 300, {});
    const std::string key2 = BuildScaledArtworkCacheKey(track2, "front", 300, {});

    EXPECT_NE(key1, key2);
}

TEST(ArtworkCacheKey, SourcePathsAreNormalizedAndOrderIndependent) {
    const std::string track = R"(E:\Music\Album\01. Track.flac)";

    const std::string key1 = BuildScaledArtworkCacheKey(
        track,
        "front",
        300,
        {R"(E:\Music\Album\cover.png)", R"(E:\Music\Album\folder.png)"}
    );
    const std::string key2 = BuildScaledArtworkCacheKey(
        track,
        "front",
        300,
        {R"(e:/music/album/FOLDER.PNG)", R"(e:\music\album\COVER.PNG)"}
    );

    EXPECT_EQ(key1, key2);
}
