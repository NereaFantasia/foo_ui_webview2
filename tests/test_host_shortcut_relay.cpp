#include "pch.h"
#include <gtest/gtest.h>
#include "webview/HostShortcutRelay.h"

namespace {
namespace relay = host_shortcut_relay;

constexpr UINT C = MOD_CONTROL;
constexpr UINT A = MOD_ALT;
constexpr UINT S = MOD_SHIFT;
constexpr UINT W = MOD_WIN;

bool Relay(UINT vk, UINT mods, unsigned categories = relay::kDefaultCategories, bool devServer = false,
           bool typing = false) {
    return relay::ShouldRelay(vk, mods, categories, devServer, typing);
}

TEST(HostShortcutRelay, DefaultScopeTakesCtrlAltWinAndFunctionKeys) {
    EXPECT_TRUE(Relay('P', C));
    EXPECT_TRUE(Relay('N', C | S));
    EXPECT_TRUE(Relay('X', A));
    EXPECT_TRUE(Relay(VK_F9, W));
    EXPECT_TRUE(Relay(VK_F9, 0));
    EXPECT_TRUE(Relay(VK_F24, S));
    EXPECT_FALSE(Relay(VK_PAUSE, 0));
    EXPECT_FALSE(Relay(VK_MEDIA_PLAY_PAUSE, 0));
    EXPECT_FALSE(Relay(VK_VOLUME_UP, 0));
}

TEST(HostShortcutRelay, CategoriesAreSwitchedSeparately) {
    EXPECT_FALSE(Relay('P', C, relay::kAlt));
    EXPECT_TRUE(Relay('P', C | A, relay::kAlt));  // 任一修饰键的类别打开就算在内
    EXPECT_FALSE(Relay(VK_F9, 0, relay::kCtrl));
    EXPECT_TRUE(Relay(VK_F9, C, relay::kCtrl));
    EXPECT_TRUE(Relay(VK_MEDIA_NEXT_TRACK, 0, relay::kMedia));
    EXPECT_TRUE(Relay(VK_PAUSE, 0, relay::kBare));
    EXPECT_TRUE(Relay(VK_PAUSE, S, relay::kBare));
    EXPECT_FALSE(Relay(VK_PAUSE, C, relay::kBare));
    EXPECT_FALSE(Relay('P', C, 0));
}

TEST(HostShortcutRelay, TextEditingKeysStayWithThePage) {
    for (UINT vk : {static_cast<UINT>('A'), static_cast<UINT>('C'), static_cast<UINT>('V'), static_cast<UINT>('X'),
                    static_cast<UINT>('Y'), static_cast<UINT>('Z'), static_cast<UINT>(VK_INSERT)}) {
        EXPECT_FALSE(Relay(vk, C)) << vk;
    }
    EXPECT_FALSE(Relay('Z', C | S));
    EXPECT_TRUE(Relay('C', C | A));  // 只排除确切的编辑组合
    for (UINT vk : {static_cast<UINT>(VK_BACK), static_cast<UINT>(VK_DELETE), static_cast<UINT>(VK_ESCAPE)}) {
        EXPECT_FALSE(Relay(vk, C, relay::kAllCategories)) << vk;
        EXPECT_FALSE(Relay(vk, A, relay::kAllCategories)) << vk;
    }
}

TEST(HostShortcutRelay, NavigationAndEnterOnlyWithAltOrWinWithoutCtrl) {
    for (UINT vk : {static_cast<UINT>(VK_LEFT), static_cast<UINT>(VK_UP), static_cast<UINT>(VK_HOME),
                    static_cast<UINT>(VK_END), static_cast<UINT>(VK_RETURN)}) {
        EXPECT_FALSE(Relay(vk, C, relay::kAllCategories)) << vk;
        EXPECT_FALSE(Relay(vk, C | S, relay::kAllCategories)) << vk;
        EXPECT_FALSE(Relay(vk, S, relay::kAllCategories)) << vk;
        EXPECT_FALSE(Relay(vk, 0, relay::kAllCategories)) << vk;
        EXPECT_TRUE(Relay(vk, A)) << vk;
        EXPECT_TRUE(Relay(vk, W)) << vk;
        EXPECT_FALSE(Relay(vk, C | A)) << vk;
    }
}

TEST(HostShortcutRelay, TypingCombinationsComputedByTheCallerAreExcluded) {
    EXPECT_FALSE(Relay('E', C | A, relay::kDefaultCategories, false, true));  // AltGr
    EXPECT_TRUE(Relay(VK_F3, C | A, relay::kDefaultCategories, false, false));
}

TEST(HostShortcutRelay, ComponentKeysAndModifiersAreNeverRelayed) {
    EXPECT_FALSE(Relay(VK_F5, 0));
    EXPECT_FALSE(Relay(VK_F5, C));
    EXPECT_TRUE(Relay(VK_F12, 0));
    EXPECT_FALSE(Relay(VK_F12, 0, relay::kDefaultCategories, true));
    EXPECT_FALSE(Relay(VK_PROCESSKEY, 0, relay::kAllCategories));
    for (UINT vk : {static_cast<UINT>(VK_CONTROL), static_cast<UINT>(VK_LMENU), static_cast<UINT>(VK_RWIN),
                    static_cast<UINT>(VK_SHIFT)}) {
        EXPECT_FALSE(Relay(vk, C | A | W, relay::kAllCategories)) << vk;
    }
}

bool FromPage(UINT vk, UINT mods, unsigned categories = relay::kAllCategories, bool text = false) {
    return relay::ShouldRelayFromPage(vk, mods, categories, text);
}

TEST(HostShortcutRelay, PageEntryTakesTheKeysRelayLeavesToThePage) {
    EXPECT_TRUE(FromPage(VK_SPACE, 0, relay::kAllCategories, true));
    EXPECT_TRUE(FromPage('A', S, relay::kAllCategories, true));
    EXPECT_FALSE(FromPage('A', C, relay::kAllCategories, true));
    EXPECT_FALSE(FromPage('E', C | A, relay::kAllCategories, true));  // AltGr
    const UINT caretKeys[] = {VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_HOME, VK_END};
    for (UINT vk : caretKeys) {
        for (UINT m : {0u, S, C, C | S}) {
            EXPECT_TRUE(FromPage(vk, m)) << vk << ' ' << m;
            // ShouldRelay 不转同一组合，一次按键只经一条路径执行
            EXPECT_FALSE(Relay(vk, m, relay::kAllCategories)) << vk << ' ' << m;
        }
        EXPECT_FALSE(FromPage(vk, A)) << vk;
        EXPECT_FALSE(FromPage(vk, W)) << vk;
    }
    EXPECT_TRUE(FromPage(VK_PRIOR, 0));
    EXPECT_TRUE(FromPage(VK_NEXT, S));
    EXPECT_FALSE(FromPage(VK_NEXT, C));
    EXPECT_TRUE(Relay(VK_NEXT, C));
}

TEST(HostShortcutRelay, PageEntryLeavesEditingAndActivationKeys) {
    const UINT kept[] = {VK_RETURN, VK_BACK, VK_DELETE, VK_ESCAPE, VK_INSERT, VK_TAB, VK_F5, VK_PROCESSKEY, VK_SHIFT};
    for (UINT vk : kept) {
        EXPECT_FALSE(FromPage(vk, 0)) << vk;
        EXPECT_FALSE(FromPage(vk, S)) << vk;
    }
    const UINT editing[] = {'A', 'C', 'V', 'X', 'Y', 'Z'};
    for (UINT vk : editing) EXPECT_FALSE(FromPage(vk, C, relay::kAllCategories, true)) << vk;
}

TEST(HostShortcutRelay, PageEntryFollowsTheCategories) {
    EXPECT_FALSE(FromPage(VK_SPACE, 0, relay::kDefaultCategories, true));
    EXPECT_FALSE(FromPage(VK_LEFT, 0, relay::kDefaultCategories));
    EXPECT_TRUE(FromPage(VK_LEFT, C, relay::kDefaultCategories));  // Ctrl+导航键归 kCtrl
    EXPECT_TRUE(FromPage(VK_SPACE, 0, relay::kBare, true));
    EXPECT_FALSE(FromPage(VK_LEFT, C, relay::kBare));
}

TEST(HostShortcutRelay, KeyCodeMatchesTheSdkEncoding) {
    EXPECT_EQ(relay::KeyCode('P', C), static_cast<UINT>('P' | (HOTKEYF_CONTROL << 8)));
    EXPECT_EQ(relay::KeyCode(VK_F9, C | A | S | W),
              static_cast<UINT>(VK_F9 | ((HOTKEYF_CONTROL | HOTKEYF_ALT | HOTKEYF_SHIFT | HOTKEYF_EXT) << 8)));
    EXPECT_EQ(relay::KeyCode(VK_RETURN, A), static_cast<UINT>(VK_RETURN | (HOTKEYF_ALT << 8)));
}

}  // namespace
