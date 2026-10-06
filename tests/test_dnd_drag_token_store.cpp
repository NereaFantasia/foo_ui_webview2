// test_dnd_drag_token_store.cpp - one-shot, window-bound, expiring drag-out tokens.
//
// The clock is injected, so every expiry case advances time by assignment
// rather than by sleeping.
#include "pch.h"
#include "../src/webview/dnd/DragTokenStore.h"

#include <string>

using fb2k_dnd::DragTokenStore;
using fb2k_dnd::kDragTokenTtlMs;

namespace {

// Two arbitrary but distinct window handles.
constexpr intptr_t kWindowA = 0x1000;
constexpr intptr_t kWindowB = 0x2000;

const std::vector<std::wstring> kPaths = {L"C:\\a.flac", L"D:\\b.mp3"};

// Drives the store with a clock and a token source the test controls. The
// counter source is fine here but would be a forgery hazard in production,
// which is exactly why the real source is injected rather than built in.
class Harness {
public:
    Harness()
        : store([this] { return nowMs; },
                [this] { return "tok-" + std::to_string(++issued); }) {}

    int64_t nowMs = 1'000'000;  // arbitrary non-zero start
    int issued = 0;
    DragTokenStore store;
};

}  // namespace

TEST(DragTokenStore, MintedTokenYieldsThePathsItWasMintedWith) {
    Harness h;
    const std::string token = h.store.Mint(kWindowA, kPaths);
    ASSERT_FALSE(token.empty());

    std::vector<std::wstring> out;
    ASSERT_TRUE(h.store.Consume(token, kWindowA, out));
    EXPECT_EQ(out, kPaths);
}

TEST(DragTokenStore, TokenIsSingleUse) {
    Harness h;
    const std::string token = h.store.Mint(kWindowA, kPaths);

    std::vector<std::wstring> first;
    ASSERT_TRUE(h.store.Consume(token, kWindowA, first));

    std::vector<std::wstring> second;
    EXPECT_FALSE(h.store.Consume(token, kWindowA, second));
    EXPECT_TRUE(second.empty());
}

TEST(DragTokenStore, UsableUpToButNotAtTheTtl) {
    Harness h;
    const std::string token = h.store.Mint(kWindowA, kPaths);

    h.nowMs += kDragTokenTtlMs - 1;
    std::vector<std::wstring> out;
    EXPECT_TRUE(h.store.Consume(token, kWindowA, out));

    Harness expired;
    const std::string stale = expired.store.Mint(kWindowA, kPaths);
    expired.nowMs += kDragTokenTtlMs;
    std::vector<std::wstring> none;
    EXPECT_FALSE(expired.store.Consume(stale, kWindowA, none));
    EXPECT_TRUE(none.empty());
}

TEST(DragTokenStore, ExpiryIsMeasuredFromMintNotFromFirstUse) {
    Harness h;
    const std::string token = h.store.Mint(kWindowA, kPaths);
    h.nowMs += kDragTokenTtlMs * 2;

    std::vector<std::wstring> out;
    EXPECT_FALSE(h.store.Consume(token, kWindowA, out));
}

TEST(DragTokenStore, TokenCannotBeRedeemedFromAnotherWindow) {
    Harness h;
    const std::string token = h.store.Mint(kWindowA, kPaths);

    std::vector<std::wstring> out;
    EXPECT_FALSE(h.store.Consume(token, kWindowB, out));
    EXPECT_TRUE(out.empty());

    // Still redeemable by the window that minted it: the rejection above must
    // not have consumed it.
    EXPECT_TRUE(h.store.Consume(token, kWindowA, out));
    EXPECT_EQ(out, kPaths);
}

TEST(DragTokenStore, MintingAgainInvalidatesTheWindowsPreviousToken) {
    // Stockpiling tokens would defeat single use and the TTL together.
    Harness h;
    const std::string first = h.store.Mint(kWindowA, kPaths);
    const std::string second = h.store.Mint(kWindowA, {L"C:\\c.flac"});
    ASSERT_NE(first, second);

    std::vector<std::wstring> out;
    EXPECT_FALSE(h.store.Consume(first, kWindowA, out));
    ASSERT_TRUE(h.store.Consume(second, kWindowA, out));
    EXPECT_EQ(out, std::vector<std::wstring>{L"C:\\c.flac"});
}

TEST(DragTokenStore, MintingForOneWindowLeavesAnotherWindowsTokenAlone) {
    Harness h;
    const std::string a = h.store.Mint(kWindowA, kPaths);
    const std::string b = h.store.Mint(kWindowB, {L"C:\\c.flac"});

    std::vector<std::wstring> out;
    EXPECT_TRUE(h.store.Consume(a, kWindowA, out));
    EXPECT_EQ(out, kPaths);
    EXPECT_TRUE(h.store.Consume(b, kWindowB, out));
}

TEST(DragTokenStore, UnknownTokenIsRejected) {
    Harness h;
    std::vector<std::wstring> out;
    EXPECT_FALSE(h.store.Consume("never-minted", kWindowA, out));

    // A wrong guess must not invalidate the token the window really holds.
    const std::string real = h.store.Mint(kWindowA, kPaths);
    EXPECT_FALSE(h.store.Consume("wrong", kWindowA, out));
    EXPECT_TRUE(h.store.Consume(real, kWindowA, out));
}

TEST(DragTokenStore, EmptyTokenIsRejected) {
    Harness h;
    h.store.Mint(kWindowA, kPaths);
    std::vector<std::wstring> out;
    EXPECT_FALSE(h.store.Consume("", kWindowA, out));
}

TEST(DragTokenStore, ClearWindowInvalidatesThatWindowsToken) {
    Harness h;
    const std::string a = h.store.Mint(kWindowA, kPaths);
    const std::string b = h.store.Mint(kWindowB, kPaths);

    h.store.ClearWindow(kWindowA);

    std::vector<std::wstring> out;
    EXPECT_FALSE(h.store.Consume(a, kWindowA, out));
    EXPECT_TRUE(h.store.Consume(b, kWindowB, out));
}

TEST(DragTokenStore, ClearInvalidatesEverything) {
    Harness h;
    const std::string a = h.store.Mint(kWindowA, kPaths);
    const std::string b = h.store.Mint(kWindowB, kPaths);

    h.store.Clear();

    std::vector<std::wstring> out;
    EXPECT_FALSE(h.store.Consume(a, kWindowA, out));
    EXPECT_FALSE(h.store.Consume(b, kWindowB, out));
}

TEST(DragTokenStore, MintRefusesAnEmptyPathList) {
    // An HDROP with no files is not a drag payload, so there is nothing to
    // authorise; HdropBuilder refuses the same input.
    Harness h;
    EXPECT_TRUE(h.store.Mint(kWindowA, {}).empty());
}

TEST(DragTokenStore, MintFailsWhenTheTokenSourceProducesNothing) {
    // A source that cannot produce entropy must not degrade to a guessable
    // token; it must fail the mint instead.
    int64_t now = 0;
    DragTokenStore store([&now] { return now; }, [] { return std::string(); });
    EXPECT_TRUE(store.Mint(kWindowA, kPaths).empty());

    std::vector<std::wstring> out;
    EXPECT_FALSE(store.Consume("", kWindowA, out));
}
