// test_network_share_resolver.cpp - NetworkShareResolver against a scripted probe
//
// Exercises the real implementation (the header pulls no foobar2000 SDK, same
// reasoning as test_path_security_canonical_form.cpp). The probe is a scripted
// fake so loopback / remote / unresolved outcomes and clock advancement can be
// produced without a real SMB share.
#include "pch.h"
#include "utils/NetworkShareResolver.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

using fb2k_utils::ISystemProbe;
using fb2k_utils::LocalVolume;
using fb2k_utils::MappedDrive;
using fb2k_utils::NetworkShareResolver;
using fb2k_utils::ObjectIdentity;
using fb2k_utils::ShareKind;

namespace {

// ObjectIdentity has no operator<; std::map needs an ordering for the
// (letter, identity) lookup key. A named comparator avoids specialising
// std::less, which MSVC rejects once the map member has already instantiated it.
struct ByIdKeyLess {
    bool operator()(const std::pair<wchar_t, ObjectIdentity>& a,
                    const std::pair<wchar_t, ObjectIdentity>& b) const {
        if (a.first != b.first) return a.first < b.first;
        if (a.second.volumeSerial != b.second.volumeSerial) return a.second.volumeSerial < b.second.volumeSerial;
        if (a.second.fileIdHigh != b.second.fileIdHigh) return a.second.fileIdHigh < b.second.fileIdHigh;
        return a.second.fileIdLow < b.second.fileIdLow;
    }
};

struct FakeProbe : ISystemProbe {
    // shareRoot (as the resolver asks, with trailing backslash) -> identity
    std::map<std::wstring, ObjectIdentity> identities;
    std::vector<LocalVolume> volumes;
    // (letter, identity) -> local root
    std::map<std::pair<wchar_t, ObjectIdentity>, std::wstring, ByIdKeyLess> byId;
    bool equivalent = true;
    std::vector<MappedDrive> mapped;
    uint64_t now = 1'000'000;

    int identityCalls = 0;
    int volumeCalls = 0;
    int resolveCalls = 0;
    int mappedCalls = 0;

    std::optional<ObjectIdentity> QueryShareRootIdentity(std::wstring_view shareRoot) override {
        ++identityCalls;
        auto it = identities.find(std::wstring(shareRoot));
        if (it == identities.end()) return std::nullopt;
        return it->second;
    }
    std::vector<LocalVolume> EnumerateLocalVolumes() override { ++volumeCalls; return volumes; }
    std::optional<std::wstring> ResolveLocalPathById(wchar_t letter, const ObjectIdentity& id) override {
        ++resolveCalls;
        auto it = byId.find({letter, id});
        if (it == byId.end()) return std::nullopt;
        return it->second;
    }
    bool AreEquivalent(std::wstring_view, std::wstring_view) override { return equivalent; }
    std::vector<MappedDrive> EnumerateMappedDrives() override { ++mappedCalls; return mapped; }
    uint64_t NowMs() override { return now; }
};

const ObjectIdentity kIdE{0xE0E0'0001, 0x1111, 0x2222};
const ObjectIdentity kIdC{0xC0C0'0001, 0x3333, 0x4444};

// A probe describing a machine with local volumes C: and E:, where
// \\localhost\E$ is a loopback share of E:\ and \\nas\music is truly remote.
std::unique_ptr<FakeProbe> TypicalMachine() {
    auto p = std::make_unique<FakeProbe>();
    p->volumes = {{L'C', kIdC.volumeSerial}, {L'E', kIdE.volumeSerial}};
    p->identities[L"\\\\localhost\\E$\\"] = kIdE;
    p->identities[L"\\\\127.0.0.1\\E$\\"] = kIdE;
    p->identities[L"\\\\nas\\music\\"] = ObjectIdentity{0x9999'0001, 0x5, 0x6};
    p->byId[{L'E', kIdE}] = L"E:\\";
    return p;
}

}  // namespace

// ============================================
// Classification
// ============================================

TEST(NetworkShareResolver, LoopbackWhenSerialMatchesAndIdResolves) {
    auto probe = TypicalMachine();
    NetworkShareResolver r(std::move(probe));
    const auto info = r.Classify(L"\\\\localhost\\E$");
    EXPECT_EQ(info.kind, ShareKind::Loopback);
    EXPECT_EQ(info.localRoot, L"E:\\");
}

TEST(NetworkShareResolver, RemoteWhenSerialMatchesNoLocalVolume) {
    NetworkShareResolver r(TypicalMachine());
    const auto info = r.Classify(L"\\\\nas\\music");
    EXPECT_EQ(info.kind, ShareKind::Remote);
    EXPECT_TRUE(info.localRoot.empty());
}

TEST(NetworkShareResolver, RemoteWhenShareRootCannotBeOpened) {
    NetworkShareResolver r(TypicalMachine());
    EXPECT_EQ(r.Classify(L"\\\\offline\\share").kind, ShareKind::Remote);
}

TEST(NetworkShareResolver, UnresolvedWhenSerialMatchesButIdLookupFails) {
    auto probe = TypicalMachine();
    probe->byId.clear();  // serial says "on E:", but the object cannot be reopened
    NetworkShareResolver r(std::move(probe));
    EXPECT_EQ(r.Classify(L"\\\\localhost\\E$").kind, ShareKind::LoopbackUnresolved);
}

TEST(NetworkShareResolver, UnresolvedWhenEquivalenceRecheckFails) {
    auto probe = TypicalMachine();
    probe->equivalent = false;
    NetworkShareResolver r(std::move(probe));
    EXPECT_EQ(r.Classify(L"\\\\localhost\\E$").kind, ShareKind::LoopbackUnresolved);
}

// ============================================
// Caching
// ============================================

TEST(NetworkShareResolver, FailureClassificationIsCached) {
    auto probe = TypicalMachine();
    FakeProbe* raw = probe.get();
    NetworkShareResolver r(std::move(probe));
    r.Classify(L"\\\\offline\\share");
    r.Classify(L"\\\\offline\\share");
    EXPECT_EQ(raw->identityCalls, 1);
}

TEST(NetworkShareResolver, DifferentSpellingsOfSameShareShareOneEntry) {
    auto probe = TypicalMachine();
    FakeProbe* raw = probe.get();
    NetworkShareResolver r(std::move(probe));
    r.Classify(L"\\\\localhost\\E$");
    r.Classify(L"\\\\127.0.0.1\\E$");
    // The second spelling must still probe once for its identity (that is how
    // it learns the key), but volume enumeration and id resolution are reused.
    EXPECT_EQ(raw->identityCalls, 2);
    EXPECT_EQ(raw->volumeCalls, 1);
    EXPECT_EQ(raw->resolveCalls, 1);
}

TEST(NetworkShareResolver, ExpiredEntryIsRevalidatedNotDropped) {
    auto probe = TypicalMachine();
    FakeProbe* raw = probe.get();
    NetworkShareResolver r(std::move(probe));
    r.Classify(L"\\\\localhost\\E$");
    raw->now += NetworkShareResolver::kTtlMs + 1;
    // Still enumerated while expired ...
    EXPECT_EQ(r.KnownLoopbackSharesCovering(L"E:\\OST\\x.mp3").size(), 1u);
    // ... and re-probed on the next Classify.
    r.Classify(L"\\\\localhost\\E$");
    EXPECT_EQ(raw->identityCalls, 2);
}

// ============================================
// Mapped drives
// ============================================

TEST(NetworkShareResolver, ShareRootOfLetterReadsMappedTable) {
    auto probe = TypicalMachine();
    probe->mapped = {{L'Z', L"\\\\localhost\\E$"}};
    NetworkShareResolver r(std::move(probe));
    EXPECT_EQ(r.ShareRootOfLetter(L'Z'), std::optional<std::wstring>(L"\\\\localhost\\E$"));
    EXPECT_EQ(r.ShareRootOfLetter(L'z'), std::optional<std::wstring>(L"\\\\localhost\\E$"));
    EXPECT_FALSE(r.ShareRootOfLetter(L'E').has_value());
}

TEST(NetworkShareResolver, LettersForMatchesNormalisedShareRoot) {
    auto probe = TypicalMachine();
    probe->mapped = {{L'Z', L"\\\\LOCALHOST\\e$"}, {L'Y', L"\\\\nas\\music"}};
    NetworkShareResolver r(std::move(probe));
    const auto letters = r.LettersFor(L"\\\\localhost\\E$\\");
    ASSERT_EQ(letters.size(), 1u);
    EXPECT_EQ(letters[0], L'Z');
}

TEST(NetworkShareResolver, LettersForLoopbackIncludesOtherSpellingsOfSameIdentity) {
    auto probe = TypicalMachine();
    probe->mapped = {{L'Z', L"\\\\127.0.0.1\\E$"}};
    NetworkShareResolver r(std::move(probe));
    r.Classify(L"\\\\localhost\\E$");
    r.Classify(L"\\\\127.0.0.1\\E$");
    // Asked about the localhost spelling, but Z: is mapped to the 127.0.0.1
    // spelling of the same object; identity keying must bridge the two.
    const auto letters = r.LettersFor(L"\\\\localhost\\E$");
    ASSERT_EQ(letters.size(), 1u);
    EXPECT_EQ(letters[0], L'Z');
}

TEST(NetworkShareResolver, LettersForDoesNotBridgeRemoteSpellingsOfSameIdentity) {
    // Two spellings of a truly remote share resolve to the same identity, but
    // identity bridging is reserved for loopback shares; asking about one
    // spelling must not surface a drive letter mapped to the other.
    auto probe = TypicalMachine();
    probe->identities[L"\\\\nas2\\music\\"] = probe->identities[L"\\\\nas\\music\\"];
    probe->mapped = {{L'Z', L"\\\\nas2\\music"}};
    NetworkShareResolver r(std::move(probe));
    r.Classify(L"\\\\nas\\music");
    r.Classify(L"\\\\nas2\\music");
    EXPECT_TRUE(r.LettersFor(L"\\\\nas\\music").empty());
}

TEST(NetworkShareResolver, MappedTableRefreshesAfterTtl) {
    auto probe = TypicalMachine();
    FakeProbe* raw = probe.get();
    NetworkShareResolver r(std::move(probe));
    EXPECT_FALSE(r.ShareRootOfLetter(L'Z').has_value());
    raw->mapped = {{L'Z', L"\\\\localhost\\E$"}};
    EXPECT_FALSE(r.ShareRootOfLetter(L'Z').has_value()) << "within TTL the stale table is served";
    raw->now += NetworkShareResolver::kTtlMs + 1;
    EXPECT_TRUE(r.ShareRootOfLetter(L'Z').has_value());
    EXPECT_EQ(raw->mappedCalls, 2);
}

TEST(NetworkShareResolver, MappedTableIsLoadedOnFirstUseEvenAtSmallTickCounts) {
    // GetTickCount64 can be below the TTL shortly after boot; an unloaded table
    // must still be fetched rather than mistaken for a fresh empty one.
    auto probe = TypicalMachine();
    FakeProbe* raw = probe.get();
    raw->now = 5;
    raw->mapped = {{L'Z', L"\\\\localhost\\E$"}};
    NetworkShareResolver r(std::move(probe));
    EXPECT_TRUE(r.ShareRootOfLetter(L'Z').has_value());
}

TEST(NetworkShareResolver, MappedDriveTargetsAreClassifiedOnRefresh) {
    // A local path must expand to the UNC spelling of a loopback share that is
    // only known through the mapped-drive table, without any UNC call having
    // been made first. Mirrors a local path being checked while the share is
    // known only via a mapped drive.
    auto probe = TypicalMachine();
    probe->mapped = {{L'Y', L"\\\\localhost\\E$"}};
    NetworkShareResolver r(std::move(probe));
    const auto covering = r.KnownLoopbackSharesCovering(L"E:\\OST\\x.mp3");
    ASSERT_EQ(covering.size(), 1u);
    EXPECT_EQ(covering[0].first, L"\\\\localhost\\E$");
    EXPECT_EQ(covering[0].second, L"E:\\");
}

TEST(NetworkShareResolver, MappedTableKeepsOriginalSpellingForCovering) {
    // The user registers library roots with whatever spelling they typed; the
    // rewrite handed to is_path_addable must reproduce it, not a lowercased key.
    auto probe = TypicalMachine();
    probe->identities[L"\\\\LOCALHOST\\E$\\"] = kIdE;
    probe->mapped = {{L'Y', L"\\\\LOCALHOST\\E$"}};
    NetworkShareResolver r(std::move(probe));
    const auto covering = r.KnownLoopbackSharesCovering(L"E:\\OST\\x.mp3");
    ASSERT_EQ(covering.size(), 1u);
    EXPECT_EQ(covering[0].first, L"\\\\LOCALHOST\\E$");
}

// ============================================
// KnownLoopbackSharesCovering
// ============================================

TEST(NetworkShareResolver, CoveringReturnsEverySpellingOfTheShare) {
    NetworkShareResolver r(TypicalMachine());
    r.Classify(L"\\\\localhost\\E$");
    r.Classify(L"\\\\127.0.0.1\\E$");
    const auto covering = r.KnownLoopbackSharesCovering(L"E:\\OST\\x.mp3");
    ASSERT_EQ(covering.size(), 2u);
    for (const auto& [share, local] : covering) {
        EXPECT_EQ(local, L"E:\\");
    }
}

TEST(NetworkShareResolver, CoveringIsSeparatorAware) {
    auto probe = TypicalMachine();
    probe->byId[{L'E', kIdE}] = L"E:\\OST";
    NetworkShareResolver r(std::move(probe));
    r.Classify(L"\\\\localhost\\E$");
    EXPECT_EQ(r.KnownLoopbackSharesCovering(L"E:\\OST\\x.mp3").size(), 1u);
    EXPECT_EQ(r.KnownLoopbackSharesCovering(L"E:\\OST").size(), 1u);
    // "E:\OSTevil" shares the prefix bytes but is a sibling directory.
    EXPECT_EQ(r.KnownLoopbackSharesCovering(L"E:\\OSTevil\\x.mp3").size(), 0u);
}

TEST(NetworkShareResolver, CoveringIgnoresRemoteAndUnresolved) {
    auto probe = TypicalMachine();
    probe->byId.clear();
    NetworkShareResolver r(std::move(probe));
    r.Classify(L"\\\\localhost\\E$");  // unresolved
    r.Classify(L"\\\\nas\\music");     // remote
    EXPECT_TRUE(r.KnownLoopbackSharesCovering(L"E:\\OST\\x.mp3").empty());
}

TEST(NetworkShareResolver, CoveringIsEmptyForUnknownShares) {
    NetworkShareResolver r(TypicalMachine());
    // Never classified in this process; the resolver cannot know it exists.
    EXPECT_TRUE(r.KnownLoopbackSharesCovering(L"E:\\OST\\x.mp3").empty());
}

TEST(NetworkShareResolver, SpellingRebindsWhenItsIdentityDisappears) {
    // Two spellings of one loopback share sit on the same identity entry. When
    // the share goes away, the spelling that is re-probed must move to a
    // name-keyed Remote entry and stop producing loopback rewrites, while the
    // spelling that has not been re-probed yet stays on the identity entry.
    auto probe = TypicalMachine();
    FakeProbe* raw = probe.get();
    NetworkShareResolver r(std::move(probe));
    r.Classify(L"\\\\localhost\\E$");
    r.Classify(L"\\\\127.0.0.1\\E$");
    raw->now += NetworkShareResolver::kTtlMs + 1;
    raw->identities.erase(L"\\\\localhost\\E$\\");
    EXPECT_EQ(r.Classify(L"\\\\localhost\\E$").kind, ShareKind::Remote);
    const auto covering = r.KnownLoopbackSharesCovering(L"E:\\OST\\x.mp3");
    ASSERT_EQ(covering.size(), 1u);
    EXPECT_EQ(covering[0].first, L"\\\\127.0.0.1\\E$");
}

TEST(NetworkShareResolver, IdentityEntryIsDroppedOnceItsLastSpellingRebinds) {
    // Only one spelling was ever bound to the identity entry. When that
    // spelling is re-probed and the share is gone, the identity entry loses its
    // last spelling and is erased while the resolver still holds a reference to
    // the freshly inserted name-keyed entry; this must not crash, the rebound
    // spelling must classify as Remote, and no loopback rewrite may remain.
    auto probe = TypicalMachine();
    FakeProbe* raw = probe.get();
    NetworkShareResolver r(std::move(probe));
    r.Classify(L"\\\\localhost\\E$");
    raw->now += NetworkShareResolver::kTtlMs + 1;
    raw->identities.erase(L"\\\\localhost\\E$\\");
    EXPECT_EQ(r.Classify(L"\\\\localhost\\E$").kind, ShareKind::Remote);
    EXPECT_TRUE(r.KnownLoopbackSharesCovering(L"E:\\OST\\x.mp3").empty());
}

// ============================================
// String helpers
// ============================================

TEST(NetworkShareResolver, SplitUncShareRoot) {
    using fb2k_utils::SplitUncShareRoot;
    auto r = SplitUncShareRoot(L"\\\\host\\share\\a\\b.flac");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->first, L"\\\\host\\share");
    EXPECT_EQ(r->second, L"\\a\\b.flac");

    r = SplitUncShareRoot(L"\\\\host\\share");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->first, L"\\\\host\\share");
    EXPECT_EQ(r->second, L"");

    r = SplitUncShareRoot(L"\\\\host\\share\\");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->second, L"");

    r = SplitUncShareRoot(L"//host/share/x");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->first, L"\\\\host\\share");
    EXPECT_EQ(r->second, L"\\x");

    EXPECT_FALSE(SplitUncShareRoot(L"\\\\host").has_value());
    EXPECT_FALSE(SplitUncShareRoot(L"\\\\").has_value());
    EXPECT_FALSE(SplitUncShareRoot(L"C:\\x").has_value());
}

TEST(NetworkShareResolver, NormalizeShareRoot) {
    using fb2k_utils::NormalizeShareRoot;
    EXPECT_EQ(NormalizeShareRoot(L"\\\\LOCALHOST\\E$\\"), L"\\\\localhost\\e$");
    EXPECT_EQ(NormalizeShareRoot(L"//nas/Music"), L"\\\\nas\\music");
}

TEST(NetworkShareResolver, JoinLocalRoot) {
    using fb2k_utils::JoinLocalRoot;
    EXPECT_EQ(JoinLocalRoot(L"E:\\", L"\\OST\\x.mp3"), L"E:\\OST\\x.mp3");
    EXPECT_EQ(JoinLocalRoot(L"E:\\OST", L"\\x.mp3"), L"E:\\OST\\x.mp3");
    EXPECT_EQ(JoinLocalRoot(L"E:\\OST", L""), L"E:\\OST");
    EXPECT_EQ(JoinLocalRoot(L"E:\\", L""), L"E:\\");
    EXPECT_EQ(JoinLocalRoot(L"E:\\OST", L"x.mp3"), L"E:\\OST\\x.mp3");
}
