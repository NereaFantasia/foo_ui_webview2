#include "pch.h"
#include "media/MediaTokens.h"

namespace {
std::string Token(unsigned n) { return std::string(31, '0') + "0123456789abcdef"[n % 16]; }
}

TEST(MediaTokens, BindsTheCompleteDocumentIdentity) {
    media::MediaTokens store([] { return Token(1); }, [] { return 1u; }, 2);
    const media::DocumentOwner owner{1, 2, 3};
    const auto token = store.Mint(owner, "https://app.local", L"E:\\movie.mp4", {100, 10}, "video/mp4");
    ASSERT_TRUE(token);
    EXPECT_TRUE(store.Find(*token, owner));
    EXPECT_FALSE(store.Find(*token, {2, 2, 3}));
    EXPECT_FALSE(store.Find(*token, {1, 3, 3}));
    EXPECT_FALSE(store.Find(*token, {1, 2, 4}));
    EXPECT_FALSE(store.Find("unknown", owner));
}

TEST(MediaTokens, EvictsOnlyTheLeastRecentlyUsedTokenOfThatDocument) {
    unsigned next = 0;
    std::uint64_t now = 0;
    media::MediaTokens store([&] { return Token(++next); }, [&] { return ++now; }, 2);
    const media::DocumentOwner a{1, 1, 1}, b{2, 1, 1};
    const auto mint = [&](media::DocumentOwner owner) { return store.Mint(owner, "https://app.local", L"E:\\a", {1, 1}, "x"); };
    const auto first = mint(a), second = mint(a), other = mint(b);
    ASSERT_TRUE(first && second && other);
    store.Touch(*first);
    const auto third = mint(a);
    ASSERT_TRUE(third);
    EXPECT_TRUE(store.Find(*first, a));
    EXPECT_FALSE(store.Find(*second, a));
    EXPECT_TRUE(store.Find(*third, a));
    EXPECT_TRUE(store.Find(*other, b));
}

TEST(MediaTokens, LookupsDoNotExtendLifetimeAndDocumentChangesRevokeTokens) {
    unsigned next = 0;
    media::MediaTokens store([&] { return Token(++next); }, [] { return 1u; }, 1);
    const media::DocumentOwner a{1, 1, 1}, b{1, 2, 1};
    const auto first = store.Mint(a, "a", L"a", {1, 1}, "x");
    const auto second = store.Mint(b, "b", L"b", {1, 1}, "x");
    ASSERT_TRUE(first && second);
    store.InvalidateOwner(a);
    EXPECT_FALSE(store.Find(*first, a));
    EXPECT_TRUE(store.Find(*second, b));
    store.InvalidateHost(1);
    EXPECT_FALSE(store.Find(*second, b));
}

TEST(MediaTokens, CollidingOrInvalidRandomValuesFailWithoutReplacingEntries) {
    media::MediaTokens store([] { return Token(1); }, [] { return 1u; }, 2);
    const media::DocumentOwner owner{1, 1, 1};
    const auto first = store.Mint(owner, "origin", L"original", {1, 1}, "x");
    ASSERT_TRUE(first);
    EXPECT_FALSE(store.Mint(owner, "other", L"replacement", {2, 2}, "y"));
    EXPECT_EQ(store.Find(*first, owner)->path, L"original");
    media::MediaTokens invalid([] { return ""; }, [] { return 1u; }, 2);
    EXPECT_FALSE(invalid.Mint(owner, "a", L"a", {1, 1}, "x"));
}
