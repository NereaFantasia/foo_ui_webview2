// test_dnd_shortcut_resolver.cpp - .lnk target resolution: extension filter,
// budgets, and the parallel-array length invariant.
//
// The COM half (ResolveOneShortcut) is not exercised here: it needs a shell,
// real .lnk files on disk, and an apartment. What is exercised is the policy
// that decides which entries reach COM at all, which is where the blocking
// risk and the index contract live. The resolver and the clock are injected.
#include "pch.h"
#include "../src/webview/dnd/ShortcutResolver.h"

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using fb2k_dnd::IsShortcutPath;
using fb2k_dnd::kShortcutBatchBudgetMs;
using fb2k_dnd::kShortcutItemBudgetMs;
using fb2k_dnd::ResolvedTarget;
using fb2k_dnd::ResolveShortcutTargetsWith;

namespace {

// Records which paths reached the resolver, so a test can assert that a
// non-shortcut cost no COM call at all rather than one that returned nothing.
struct FakeResolver {
    std::vector<std::wstring> seen;
    // Target per input path; absent means the resolver reports no target.
    std::map<std::wstring, std::wstring> targets;
    // Milliseconds the fake clock advances per call, keyed the same way.
    std::map<std::wstring, int64_t> costMs;
    int64_t* clock = nullptr;

    ResolvedTarget operator()(const std::wstring& path) {
        seen.push_back(path);
        if (clock) {
            const auto cost = costMs.find(path);
            *clock += cost == costMs.end() ? 1 : cost->second;
        }
        const auto hit = targets.find(path);
        if (hit == targets.end()) {
            return std::nullopt;
        }
        return hit->second;
    }
};

// Runs the batch policy against a clock the fake resolver advances itself, so
// elapsed time only moves when resolution actually happens.
std::vector<ResolvedTarget> RunBatch(const std::vector<std::wstring>& paths,
                                     FakeResolver& resolver, int64_t& clock) {
    resolver.clock = &clock;
    return ResolveShortcutTargetsWith(
        paths, [&resolver](const std::wstring& p) { return resolver(p); },
        [&clock]() { return clock; });
}

}  // namespace

// --- Extension filter --------------------------------------------------------

TEST(DndShortcutPath, RecognisesLnkCaseInsensitively) {
    EXPECT_TRUE(IsShortcutPath(L"C:\\Music\\album.lnk"));
    EXPECT_TRUE(IsShortcutPath(L"C:\\Music\\album.LNK"));
    EXPECT_TRUE(IsShortcutPath(L"C:\\Music\\album.Lnk"));
}

TEST(DndShortcutPath, RejectsEverythingElse) {
    EXPECT_FALSE(IsShortcutPath(L"C:\\Music\\track.mp3"));
    EXPECT_FALSE(IsShortcutPath(L"C:\\Music\\folder"));
    // Out of scope by design: each of these needs its own resolver.
    EXPECT_FALSE(IsShortcutPath(L"C:\\Music\\station.url"));
    EXPECT_FALSE(IsShortcutPath(L"C:\\Music\\view.library-ms"));
    // The extension has to be at the end, not merely present.
    EXPECT_FALSE(IsShortcutPath(L"C:\\Music\\album.lnk.mp3"));
}

TEST(DndShortcutPath, RejectsAPathThatIsNothingButTheExtension) {
    // A file literally named ".lnk" has no stem; treating it as a shortcut
    // would send a COM call after a path no shell would produce.
    EXPECT_FALSE(IsShortcutPath(L".lnk"));
    EXPECT_FALSE(IsShortcutPath(L".ln"));
    EXPECT_FALSE(IsShortcutPath(L""));
}

// --- Length invariant --------------------------------------------------------

TEST(DndShortcutResolver, ResultLengthAlwaysMatchesInput) {
    int64_t clock = 0;
    FakeResolver resolver;
    resolver.targets[L"C:\\a.lnk"] = L"C:\\real\\a.mp3";

    const std::vector<std::wstring> paths{L"C:\\a.lnk", L"C:\\b.mp3", L"C:\\c.lnk"};
    const auto resolved = RunBatch(paths, resolver, clock);
    ASSERT_EQ(resolved.size(), paths.size());
}

TEST(DndShortcutResolver, EmptyInputYieldsEmptyOutput) {
    int64_t clock = 0;
    FakeResolver resolver;
    // The exception path in ReadHdropPaths clears the path list, and the
    // parallel array is derived from that list, so this is the shape the
    // caller sees after a failed HDROP read.
    const auto resolved = RunBatch({}, resolver, clock);
    EXPECT_TRUE(resolved.empty());
    EXPECT_TRUE(resolver.seen.empty());
}

// --- Which entries reach COM ------------------------------------------------

TEST(DndShortcutResolver, NonShortcutCostsNoResolverCall) {
    int64_t clock = 0;
    FakeResolver resolver;
    const std::vector<std::wstring> paths{L"C:\\a.mp3", L"C:\\b.flac", L"C:\\dir"};

    const auto resolved = RunBatch(paths, resolver, clock);
    EXPECT_TRUE(resolver.seen.empty()) << "no shortcut in the batch must mean no COM";
    for (const auto& entry : resolved) {
        EXPECT_FALSE(entry.has_value());
    }
}

TEST(DndShortcutResolver, ResolvesOnlyTheShortcutEntries) {
    int64_t clock = 0;
    FakeResolver resolver;
    resolver.targets[L"C:\\link.lnk"] = L"D:\\Music\\real.mp3";
    const std::vector<std::wstring> paths{L"C:\\a.mp3", L"C:\\link.lnk", L"C:\\b.mp3"};

    const auto resolved = RunBatch(paths, resolver, clock);
    ASSERT_EQ(resolved.size(), 3u);
    EXPECT_FALSE(resolved[0].has_value());
    ASSERT_TRUE(resolved[1].has_value());
    EXPECT_EQ(*resolved[1], L"D:\\Music\\real.mp3");
    EXPECT_FALSE(resolved[2].has_value());

    ASSERT_EQ(resolver.seen.size(), 1u);
    EXPECT_EQ(resolver.seen[0], L"C:\\link.lnk");
}

TEST(DndShortcutResolver, BrokenLinkReportsNothingWithoutFailingTheBatch) {
    int64_t clock = 0;
    FakeResolver resolver;
    // No entry in targets stands for "the COM half produced no target": a
    // namespace object, a recorded target too long to be read back intact, or an
    // unavailable apartment all arrive here the same way.
    //
    // Not a dangling link, despite this test's name, which predates the
    // measurement: IShellLinkW::GetPath answers S_OK with the path the .lnk
    // recorded whether or not the file is still there, so the real resolver
    // reports a target for a broken shortcut. What is asserted below is only the
    // batch-policy shape - one empty entry must not abandon the rest.
    resolver.targets[L"C:\\good.lnk"] = L"D:\\real.mp3";
    const std::vector<std::wstring> paths{L"C:\\broken.lnk", L"C:\\good.lnk"};

    const auto resolved = RunBatch(paths, resolver, clock);
    ASSERT_EQ(resolved.size(), 2u);
    EXPECT_FALSE(resolved[0].has_value()) << "must be empty, never an empty string";
    ASSERT_TRUE(resolved[1].has_value())
        << "one broken link must not abandon the entries after it";
    EXPECT_EQ(*resolved[1], L"D:\\real.mp3");
}

TEST(DndShortcutResolver, MixedBatchKeepsEveryIndexAligned) {
    // The acceptance case: 10 audio files, 3 shortcuts, 1 broken shortcut.
    int64_t clock = 0;
    FakeResolver resolver;
    std::vector<std::wstring> paths;
    for (int i = 0; i < 10; ++i) {
        paths.push_back(L"C:\\audio" + std::to_wstring(i) + L".mp3");
    }
    for (int i = 0; i < 3; ++i) {
        const std::wstring lnk = L"C:\\link" + std::to_wstring(i) + L".lnk";
        paths.push_back(lnk);
        resolver.targets[lnk] = L"D:\\target" + std::to_wstring(i) + L".mp3";
    }
    paths.push_back(L"C:\\broken.lnk");

    const auto resolved = RunBatch(paths, resolver, clock);
    ASSERT_EQ(resolved.size(), 14u);
    for (size_t i = 0; i < 10; ++i) {
        EXPECT_FALSE(resolved[i].has_value()) << "audio entry " << i;
    }
    for (size_t i = 0; i < 3; ++i) {
        ASSERT_TRUE(resolved[10 + i].has_value()) << "shortcut entry " << i;
        EXPECT_EQ(*resolved[10 + i], L"D:\\target" + std::to_wstring(i) + L".mp3");
    }
    EXPECT_FALSE(resolved[13].has_value());
}

// --- Budgets ----------------------------------------------------------------

TEST(DndShortcutResolver, StopsCallingComOnceTheBatchBudgetIsSpent) {
    int64_t clock = 0;
    FakeResolver resolver;
    // Each item costs half the per-item budget, so no item overruns and only
    // the batch bound can end the pass. The counts are derived from the two
    // constants rather than written out, so tuning either keeps this honest.
    const int64_t perItem = kShortcutItemBudgetMs / 2;
    ASSERT_GT(perItem, 0);
    const size_t affordable = static_cast<size_t>(
        (kShortcutBatchBudgetMs + perItem - 1) / perItem);

    std::vector<std::wstring> paths;
    const size_t total = affordable + 4;
    for (size_t i = 0; i < total; ++i) {
        const std::wstring lnk = L"C:\\link" + std::to_wstring(i) + L".lnk";
        paths.push_back(lnk);
        resolver.targets[lnk] = L"D:\\t" + std::to_wstring(i) + L".mp3";
        resolver.costMs[lnk] = perItem;
    }

    const auto resolved = RunBatch(paths, resolver, clock);
    ASSERT_EQ(resolved.size(), total);
    EXPECT_EQ(resolver.seen.size(), affordable)
        << "the batch budget must stop further COM calls, not merely discard results";
    for (size_t i = affordable; i < total; ++i) {
        EXPECT_FALSE(resolved[i].has_value()) << "entry " << i << " must be skipped";
    }
}

TEST(DndShortcutResolver, OneOverrunningItemAbandonsTheRest) {
    // IPersistFile::Load takes no timeout, so a single slow item cannot be
    // capped; the only bound on total damage is to stop after seeing one.
    int64_t clock = 0;
    FakeResolver resolver;
    const std::wstring slow = L"C:\\slow.lnk";
    resolver.targets[slow] = L"\\\\dead-share\\music.mp3";
    resolver.costMs[slow] = kShortcutItemBudgetMs + 1;
    resolver.targets[L"C:\\after.lnk"] = L"D:\\after.mp3";

    const std::vector<std::wstring> paths{slow, L"C:\\after.lnk", L"C:\\also.lnk"};
    const auto resolved = RunBatch(paths, resolver, clock);

    ASSERT_EQ(resolved.size(), 3u);
    ASSERT_TRUE(resolved[0].has_value())
        << "the overrunning item still keeps the answer it paid for";
    EXPECT_FALSE(resolved[1].has_value());
    EXPECT_FALSE(resolved[2].has_value());
    ASSERT_EQ(resolver.seen.size(), 1u);
    EXPECT_EQ(resolver.seen[0], slow);
}

TEST(DndShortcutResolver, ItemExactlyOnBudgetDoesNotAbandonTheRest) {
    // The bound is an overrun, not a match: a batch of items each landing on
    // the limit is still bounded by the batch budget above.
    int64_t clock = 0;
    FakeResolver resolver;
    const std::wstring first = L"C:\\a.lnk";
    resolver.targets[first] = L"D:\\a.mp3";
    resolver.costMs[first] = kShortcutItemBudgetMs;
    resolver.targets[L"C:\\b.lnk"] = L"D:\\b.mp3";

    const auto resolved = RunBatch({first, L"C:\\b.lnk"}, resolver, clock);
    ASSERT_EQ(resolved.size(), 2u);
    EXPECT_TRUE(resolved[1].has_value());
}

TEST(DndShortcutResolver, ManyNonShortcutsNeverTouchTheBudget) {
    // 4096 audio files is the reserve cap in HdropReader; none of them may
    // consume budget, or a large ordinary drop could starve a trailing .lnk.
    int64_t clock = 0;
    FakeResolver resolver;
    std::vector<std::wstring> paths;
    for (int i = 0; i < 4096; ++i) {
        paths.push_back(L"C:\\audio" + std::to_wstring(i) + L".mp3");
    }
    const std::wstring lnk = L"C:\\tail.lnk";
    paths.push_back(lnk);
    resolver.targets[lnk] = L"D:\\tail.mp3";

    const auto resolved = RunBatch(paths, resolver, clock);
    ASSERT_EQ(resolved.size(), 4097u);
    ASSERT_TRUE(resolved[4096].has_value())
        << "a trailing shortcut must still be resolved after a large ordinary drop";
    EXPECT_EQ(resolver.seen.size(), 1u);
}

// --- Degenerate inputs ------------------------------------------------------

TEST(DndShortcutResolver, MissingCallablesYieldAllEmptyEntriesOfTheRightLength) {
    // Not reachable from the production wiring, but the length contract is what
    // every consumer indexes against, so it must hold unconditionally.
    const std::vector<std::wstring> paths{L"C:\\a.lnk", L"C:\\b.mp3"};
    const auto resolved = ResolveShortcutTargetsWith(paths, nullptr, nullptr);
    ASSERT_EQ(resolved.size(), 2u);
    EXPECT_FALSE(resolved[0].has_value());
    EXPECT_FALSE(resolved[1].has_value());
}

TEST(DndShortcutResolver, AThrowingResolverIsAbsorbed) {
    // The production resolver is noexcept, but this function is called from a
    // noexcept drop-target callback, so an escape here would abort the process.
    const std::vector<std::wstring> paths{L"C:\\a.lnk", L"C:\\b.lnk"};
    const auto resolved = ResolveShortcutTargetsWith(
        paths, [](const std::wstring&) -> ResolvedTarget { throw std::runtime_error("boom"); },
        []() { return int64_t{0}; });
    // The array was sized before the pass, so it is still index-aligned even
    // though the pass stopped on the first entry.
    ASSERT_EQ(resolved.size(), paths.size());
    for (const auto& entry : resolved) {
        EXPECT_FALSE(entry.has_value());
    }
}
