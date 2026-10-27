#include "pch.h"
#include "api/MainMenuIdentity.h"

TEST(MainMenuIdentityTest, RejectsWrongAncestorEvenForAUniqueLeaf) {
    EXPECT_FALSE(main_menu::MatchesQuery("Show", "Show", "View/Lyrics/Show", true,
                                        "File/Lyrics/Show"));
}

TEST(MainMenuIdentityTest, DistinguishesIdenticalNamesUnderDifferentDynamicBranches) {
    EXPECT_TRUE(main_menu::MatchesQuery("Lock", "Lock", "View/Lyrics/Desktop/Lock", true,
                                       "View/Lyrics/Desktop/Lock"));
    EXPECT_FALSE(main_menu::MatchesQuery("Lock", "Lock", "View/Lyrics/Floating/Lock", true,
                                        "View/Lyrics/Desktop/Lock"));
}

TEST(MainMenuIdentityTest, RejectsUnprovenPathsWhileKeepingBareNameLookup) {
    EXPECT_FALSE(main_menu::MatchesQuery("Show", "Show", "View/Lyrics/Show", false,
                                        "View/Lyrics/Show"));
    EXPECT_TRUE(main_menu::MatchesQuery("Show", "Show", "", false, "Show"));
}

TEST(MainMenuIdentityTest, NormalizesEachSegmentWithoutSuffixMatching) {
    EXPECT_TRUE(main_menu::MatchesQuery("Show", "Show", "&View/Lyrics/&Show...\tCtrl+S", true,
                                       "view/Lyrics/Show"));
    EXPECT_FALSE(main_menu::MatchesQuery("Show", "Show", "View/Lyrics/Show", true,
                                        "Lyrics/Show"));
}

namespace {
main_menu::Entry MakeEntry(std::uintptr_t provider, const char* path, bool enabled = true) {
    main_menu::Entry entry;
    entry.provider = provider;
    entry.path = path;
    entry.name = entry.displayText = "Show";
    entry.guid = "owner";
    entry.state.enabled = enabled;
    return entry;
}
}

TEST(MainMenuIdentityTest, KeepsDisabledCandidatesAndDuplicateProviderAddressesAmbiguous) {
    const std::vector<main_menu::Entry> entries{
        MakeEntry(1, "View/A/Show", false), MakeEntry(2, "View/B/Show")};
    EXPECT_EQ(main_menu::FindCandidates(entries, "Show").size(), 2u);
    EXPECT_EQ(main_menu::AddressReason(entries[0], entries), "ambiguousMatch");
    EXPECT_EQ(main_menu::FindCandidates(entries, "View/A/Show").size(), 1u);
}

TEST(MainMenuIdentityTest, DynamicLeavesNeedTheirOwnIdentifiersAndParentsAreNotCommands) {
    auto parent = MakeEntry(1, "View/A");
    parent.isDynamic = parent.isDynamicParent = true;
    auto leaf = parent;
    leaf.isDynamicParent = false;
    leaf.node = 1;
    const std::vector<main_menu::Entry> entries{parent, leaf};
    EXPECT_EQ(main_menu::AddressReason(entries[0], entries), "dynamicParent");
    EXPECT_EQ(main_menu::AddressReason(entries[1], entries), "missingDynamicIdentifier");
    EXPECT_EQ(main_menu::FindAddress(entries, "owner", "").size(), 1u);
}

TEST(MainMenuIdentityTest, SameSubGuidOnDistinctLeavesRemainsAmbiguous) {
    auto one = MakeEntry(1, "View/A/Show");
    one.isDynamic = true;
    one.subGuid = "child";
    one.node = 1;
    auto two = one;
    two.node = 2;
    const std::vector<main_menu::Entry> entries{one, two};
    EXPECT_EQ(main_menu::AddressReason(one, entries), "ambiguousMatch");
}

TEST(MainMenuIdentityTest, DisabledLeafCanHaveACompleteUniqueAddress) {
    const std::vector<main_menu::Entry> entries{MakeEntry(1, "View/A/Show", false)};
    EXPECT_TRUE(main_menu::AddressReason(entries[0], entries).empty());
    EXPECT_FALSE(entries[0].state.enabled);
}

TEST(MainMenuIdentityTest, PreservesGroupIdentityWithoutInventingInlinePathSegments) {
    const std::vector<main_menu::Group> groups{
        {"ordinary", "root", "", false}, {"popup", "ordinary", "Lyrics", true},
        {"inline", "popup", "Hidden grouping", false}};
    const auto path = main_menu::ResolveGroupPath(groups, "inline", {{"root", "View"}});
    EXPECT_TRUE(path.complete);
    EXPECT_EQ(path.path, "View/Lyrics");
    EXPECT_EQ(path.ancestors.size(), 4u);
}

TEST(MainMenuIdentityTest, RefusesMissingCyclicAndDuplicateGroups) {
    const std::vector<main_menu::Group> groups{
        {"cycle", "cycle", "Cycle", true}, {"dup", "root", "A", true}, {"dup", "root", "B", true}};
    for (const auto* group : {"absent", "cycle", "dup"}) {
        EXPECT_FALSE(main_menu::ResolveGroupPath(groups, group, {{"root", "View"}}).complete);
    }
}

namespace {
struct FakeMenuNode {
    menu_node::Kind kind = menu_node::Kind::Command;
    std::string label = "Show";
    std::string guid = "child";
    bool enabled = true;
    bool failIdentity = false;
    bool failState = false;
    int executions = 0;
    std::vector<FakeMenuNode*> children;
};

struct FakeNodeAdapter {
    bool valid(const FakeMenuNode* node) const { return node != nullptr; }
    menu_node::Kind kind(const FakeMenuNode* node) const { return node->kind; }
    std::pair<std::string, menu_node::State> display(const FakeMenuNode* node) const {
        if (node->failState) throw std::runtime_error("stateUnavailable");
        menu_node::State state;
        state.enabled = node->enabled;
        return {node->label, state};
    }
    std::string guid(const FakeMenuNode* node) const {
        if (node->failIdentity) throw std::runtime_error("incompleteHierarchy");
        return node->guid;
    }
    size_t count(const FakeMenuNode* node) const { return node->children.size(); }
    FakeMenuNode* child(const FakeMenuNode* node, size_t i) const { return node->children.at(i); }
};

std::vector<main_menu::Entry> ReadFakeTree(FakeMenuNode& root) {
    FakeNodeAdapter adapter;
    std::vector<main_menu::Entry> entries;
    auto emit = [&](FakeMenuNode* node, const std::string& path, const std::string& label,
                    const menu_node::State& state, const std::string& guid) {
        auto entry = MakeEntry(1, path.c_str());
        entry.name = entry.displayText = label;
        entry.isDynamic = true;
        entry.subGuid = guid;
        entry.node = reinterpret_cast<std::uintptr_t>(node);
        entry.state = state;
        entries.push_back(std::move(entry));
    };
    main_menu::VisitDynamic(&root, "View/Provider", 0, adapter, emit);
    return entries;
}
}

TEST(MainMenuIdentityTest, DynamicPathUsesTheActualRootAndExecutesTheRetainedLeaf) {
    FakeMenuNode child;
    FakeMenuNode root;
    root.kind = menu_node::Kind::Submenu;
    root.label = "Visible root";
    root.children = {&child};
    int instances = 0;
    auto construct = [&] { ++instances; return &root; };
    const auto tree = main_menu::ReadConstructedTree(construct, [](auto* node) { return ReadFakeTree(*node); },
                                                    [](auto* node) { return node != nullptr; });
    ASSERT_TRUE(tree.has_value());
    ASSERT_EQ(tree->size(), 1u);
    EXPECT_EQ(tree->front().path, "View/Provider/Visible root/Show");
    auto matches = main_menu::FindCandidates(*tree, "View/Provider/Visible root/Show");
    EXPECT_TRUE(main_menu::ExecuteUnique(*tree, matches,
        [&](const auto&) { return child.enabled; },
        [](const auto& entry) { ++reinterpret_cast<FakeMenuNode*>(entry.node)->executions; }).empty());
    EXPECT_EQ(child.executions, 1);
    EXPECT_EQ(root.executions, 0);
    EXPECT_EQ(instances, 1);
}

TEST(MainMenuIdentityTest, OnlyConstructionFailurePermitsFallback) {
    int reads = 0;
    const auto missing = main_menu::ReadConstructedTree([]() -> FakeMenuNode* { throw std::runtime_error("factory"); },
        [&](FakeMenuNode*) { ++reads; return 1; }, [](auto* node) { return node != nullptr; });
    EXPECT_FALSE(missing.has_value());
    EXPECT_EQ(reads, 0);
    FakeMenuNode node;
    node.failState = true;
    int fallbacks = 0;
    EXPECT_THROW({
        const auto value = main_menu::ReadConstructedTree([&] { return &node; },
            [](auto* root) { return ReadFakeTree(*root); }, [](auto* root) { return root != nullptr; });
        if (!value) ++fallbacks;
    }, std::runtime_error);
    EXPECT_EQ(fallbacks, 0);
}

TEST(MainMenuIdentityTest, FailedIdentityReadCannotBecomeAnUnidentifiedSibling) {
    FakeMenuNode node;
    node.failIdentity = true;
    EXPECT_THROW(ReadFakeTree(node), std::runtime_error);
    EXPECT_EQ(node.executions, 0);
}

TEST(MainMenuIdentityTest, MissingDynamicAddressAndFreshDisabledStateNeverDispatch) {
    FakeMenuNode node;
    node.guid.clear();
    auto entries = ReadFakeTree(node);
    int stateReads = 0, executions = 0;
    EXPECT_EQ(main_menu::ExecuteUnique(entries, std::vector<const main_menu::Entry*>{&entries.front()},
        [&](const auto&) { ++stateReads; return true; }, [&](const auto&) { ++executions; }), "missingDynamicIdentifier");
    EXPECT_EQ(stateReads, 0);
    EXPECT_EQ(executions, 0);
    node.guid = "child";
    entries = ReadFakeTree(node);
    node.enabled = false;
    EXPECT_EQ(main_menu::ExecuteUnique(entries, main_menu::FindAddress(entries, "owner", "child"),
        [&](const auto&) { return node.enabled; }, [&](const auto&) { ++executions; }), "disabled");
    EXPECT_EQ(executions, 0);
    EXPECT_EQ(main_menu::ExecuteUnique(entries, main_menu::FindAddress(entries, "owner", "removed"),
        [&](const auto&) { return true; }, [&](const auto&) { ++executions; }), "notFound");
    EXPECT_EQ(executions, 0);
}

TEST(MainMenuIdentityTest, ExecutionExceptionIsNotReportedAsSuccessOrRetried) {
    const std::vector<main_menu::Entry> entries{MakeEntry(1, "View/Show")};
    int executions = 0;
    EXPECT_THROW(main_menu::ExecuteUnique(entries, main_menu::FindAddress(entries, "owner", ""),
        [](const auto&) { return true; }, [&](const auto&) { ++executions; throw std::runtime_error("provider"); }),
        std::runtime_error);
    EXPECT_EQ(executions, 1);
}

TEST(MainMenuIdentityTest, RepeatedTreePositionsDeduplicateOnlyTheSameProviderLeaf) {
    auto one = MakeEntry(1, "View/A/Show");
    auto two = MakeEntry(2, "View/B/Show");
    const std::vector<const main_menu::Entry*> same{&one, &one};
    const std::vector<const main_menu::Entry*> different{&one, &two};
    EXPECT_EQ(main_menu::UniqueTreeMatches(same, [](const auto* value) { return value; }).size(), 1u);
    EXPECT_EQ(main_menu::UniqueTreeMatches(different, [](const auto* value) { return value; }).size(), 2u);
}

TEST(MainMenuIdentityTest, WideDynamicTreesFailInsteadOfProvingUniquenessOnATruncatedTree) {
    FakeMenuNode child, root;
    root.kind = menu_node::Kind::Submenu;
    root.children.assign(menu_node::kMaxChildrenPerNode + 1, &child);
    EXPECT_THROW(ReadFakeTree(root), std::runtime_error);
}
