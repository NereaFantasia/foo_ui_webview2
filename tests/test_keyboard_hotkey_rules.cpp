#include "pch.h"
#include <gtest/gtest.h>
#include "api/KeyboardHotkeyRules.h"

namespace {
namespace rules = keyboard_hotkey_rules;

struct Parsed {
    bool ok;
    UINT mods;
    UINT vk;
};

Parsed Parse(const std::string& key) {
    Parsed p{};
    p.ok = rules::ParseKeyString(key, p.mods, p.vk);
    return p;
}

const char* Reject(const std::string& key) {
    const Parsed p = Parse(key);
    EXPECT_TRUE(p.ok) << key;
    return rules::RejectReason(p.mods, p.vk);
}

TEST(KeyboardHotkeyParse, ModifiersAreCaseInsensitiveAndOrderFree) {
    const Parsed a = Parse("ctrl+ALT+k");
    ASSERT_TRUE(a.ok);
    EXPECT_EQ(a.mods, static_cast<UINT>(MOD_CONTROL | MOD_ALT));
    EXPECT_EQ(a.vk, static_cast<UINT>('K'));

    const Parsed b = Parse("Alt+Control+K");
    ASSERT_TRUE(b.ok);
    EXPECT_EQ(b.mods, a.mods);
    EXPECT_EQ(b.vk, a.vk);
}

TEST(KeyboardHotkeyParse, NamedKeysMapToVirtualKeys) {
    EXPECT_EQ(Parse("Win+F12").vk, static_cast<UINT>(VK_F12));
    EXPECT_EQ(Parse("Win+F12").mods, static_cast<UINT>(MOD_WIN));
    EXPECT_EQ(Parse("Shift+PlayPause").vk, static_cast<UINT>(VK_MEDIA_PLAY_PAUSE));
    EXPECT_EQ(Parse("Ctrl+Alt+Space").vk, static_cast<UINT>(VK_SPACE));
    EXPECT_EQ(Parse("Ctrl+\\").vk, static_cast<UINT>(VK_OEM_5));
}

TEST(KeyboardHotkeyParse, UnknownKeyNameFails) {
    EXPECT_FALSE(Parse("Ctrl+Hyper").ok);
    EXPECT_FALSE(Parse("Ctrl+F13").ok);
}

TEST(KeyboardHotkeyRules, NeedsCtrlAltOrWin) {
    EXPECT_NE(Reject("K"), nullptr);
    EXPECT_NE(Reject("5"), nullptr);
    EXPECT_NE(Reject("F5"), nullptr);
    EXPECT_NE(Reject("Shift+K"), nullptr);
    EXPECT_NE(Reject("Shift+F12"), nullptr);

    EXPECT_EQ(Reject("Ctrl+Alt+K"), nullptr);
    EXPECT_EQ(Reject("Win+F9"), nullptr);
    EXPECT_EQ(Reject("Alt+Shift+L"), nullptr);
    EXPECT_EQ(Reject("Ctrl+F5"), nullptr);
}

TEST(KeyboardHotkeyRules, MediaKeysMayStandAlone) {
    for (const char* key : {"PlayPause", "MediaStop", "NextTrack", "PrevTrack", "VolumeUp", "VolumeDown", "VolumeMute",
                            "Shift+NextTrack"}) {
        EXPECT_EQ(Reject(key), nullptr) << key;
    }
}

TEST(KeyboardHotkeyRules, TextEditingAndWindowsCombinationsAreRefused) {
    for (const char* key : {"Ctrl+A", "Ctrl+C", "Ctrl+V", "Ctrl+X", "Ctrl+Y", "Ctrl+Z", "Ctrl+Shift+Z",
                            "Alt+F4", "Alt+Tab", "Alt+Space", "Ctrl+Esc"}) {
        EXPECT_NE(Reject(key), nullptr) << key;
    }
}

TEST(KeyboardHotkeyRules, OnlyTheExactModifierSetIsRefused) {
    EXPECT_EQ(Reject("Ctrl+Alt+C"), nullptr);
    EXPECT_EQ(Reject("Ctrl+Shift+C"), nullptr);
    EXPECT_EQ(Reject("Ctrl+Alt+Z"), nullptr);
    EXPECT_EQ(Reject("Ctrl+Alt+F4"), nullptr);
    EXPECT_EQ(Reject("Ctrl+Shift+Esc"), nullptr);
}

}  // namespace
