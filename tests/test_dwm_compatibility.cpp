#include "pch.h"
#include "../src/window/DwmCompatibility.h"

using dwm_compatibility::Capabilities;

TEST(DwmCompatibility, Windows10AndUnknownUseSolidBackdrop) {
    for (const DWORD build : {0u, 10240u, 17763u, 19045u}) {
        const Capabilities capabilities{build};
        for (const auto effect : {"mica", "mica-alt", "acrylic"}) {
            EXPECT_EQ(capabilities.Backdrop(effect), "none");
        }
        EXPECT_FALSE(capabilities.RoundedCorners());
        EXPECT_FALSE(capabilities.SupportsAttribute(33));
        EXPECT_FALSE(capabilities.SupportsAttribute(38));
        EXPECT_FALSE(capabilities.SupportsAttribute(1029));
        EXPECT_EQ(capabilities.Backdrop("none"), "none");
        EXPECT_EQ(capabilities.Backdrop("system"), "system");
        EXPECT_TRUE(capabilities.SupportsAttribute(3));
    }
}

TEST(DwmCompatibility, OriginalWindows11UsesLegacyMica) {
    const Capabilities capabilities{22000};
    EXPECT_EQ(capabilities.Backdrop("mica"), "mica");
    EXPECT_EQ(capabilities.Backdrop("mica-alt"), "mica");
    EXPECT_EQ(capabilities.Backdrop("acrylic"), "none");
    EXPECT_TRUE(capabilities.RoundedCorners());
    EXPECT_TRUE(capabilities.SupportsAttribute(1029));
    EXPECT_FALSE(capabilities.SupportsAttribute(38));
}

TEST(DwmCompatibility, ModernWindows11KeepsAllMaterials) {
    const Capabilities capabilities{22621};
    for (const auto effect : {"none", "system", "mica", "mica-alt", "acrylic"}) {
        EXPECT_EQ(capabilities.Backdrop(effect), effect);
    }
    EXPECT_TRUE(capabilities.RoundedCorners());
    EXPECT_TRUE(capabilities.SupportsAttribute(38));
}
