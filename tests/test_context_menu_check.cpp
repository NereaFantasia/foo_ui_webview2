// test_context_menu_check.cpp - checks behind a checked menu.runContextCommandById.
#include "pch.h"
#include "compat/fb2k_types.h"  // console:: stub: the generated headers include ErrorEnvelope.h
#include "api/ContextMenuCheck.h"

using namespace context_menu_check;
using api::menu::MenuTreeNode;

namespace {

MenuTreeNode Command(std::int64_t id, const std::string& path) {
    MenuTreeNode node;
    node.type = "command";
    node.commandId = id;
    node.path = path;
    node.enabled = true;
    return node;
}

MenuTreeNode Submenu(const std::string& path, std::vector<MenuTreeNode> children) {
    MenuTreeNode node;
    node.type = "submenu";
    node.path = path;
    node.children = std::move(children);
    return node;
}

const std::vector<TrackIdentity> kTwo{{"file://C:\\a.flac", 0}, {"file://C:\\b.cue", 2}};

}  // namespace

TEST(ContextMenuTargetKey, SameTargetGivesSameKey) {
    EXPECT_EQ(TargetKey("handles", kTwo, ""), TargetKey("handles", kTwo, ""));
    EXPECT_EQ(TargetKey("handles", kTwo, "").size(), 16u);
}

TEST(ContextMenuTargetKey, OrderRepeatsAndSubsongCount) {
    const auto base = TargetKey("handles", kTwo, "");
    EXPECT_NE(base, TargetKey("handles", {kTwo[1], kTwo[0]}, ""));
    EXPECT_NE(base, TargetKey("handles", {kTwo[0], kTwo[1], kTwo[1]}, ""));
    EXPECT_NE(base, TargetKey("handles", {kTwo[0], {"file://C:\\b.cue", 3}}, ""));
}

TEST(ContextMenuTargetKey, ModeAndPlaylistCount) {
    const auto base = TargetKey("selection", kTwo, "{guid-a}");
    EXPECT_NE(base, TargetKey("selection", kTwo, "{guid-b}"));
    EXPECT_NE(base, TargetKey("handles", kTwo, "{guid-a}"));
}

TEST(ContextMenuTargetKey, FieldsDoNotRunTogether) {
    EXPECT_NE(TargetKey("handles", {{"ab", 0}, {"c", 0}}, ""), TargetKey("handles", {{"a", 0}, {"bc", 0}}, ""));
    EXPECT_NE(TargetKey("handles", {{"a", 12}}, ""), TargetKey("handles", {{"a1", 2}}, ""));
}

TEST(ContextMenuCheckCommand, MatchingCommandPasses) {
    const std::vector<MenuTreeNode> roots{Command(0, "Play"), Submenu("Tag", {Command(1, "Tag/Reload")})};
    EXPECT_EQ(CheckCommand(roots, 1, "Tag/Reload"), CommandCheck::Ok);
}

TEST(ContextMenuCheckCommand, MissingIdOrOtherPathIsCommandChanged) {
    const std::vector<MenuTreeNode> roots{Command(0, "Play"), Command(1, "Queue")};
    EXPECT_EQ(CheckCommand(roots, 5, "Queue"), CommandCheck::CommandChanged);
    EXPECT_EQ(CheckCommand(roots, 0, "Queue"), CommandCheck::CommandChanged);
}

TEST(ContextMenuCheckCommand, SubmenuIsNotACommand) {
    auto menu = Submenu("Send", {});
    menu.commandId = 3;
    EXPECT_EQ(CheckCommand({menu}, 3, "Send"), CommandCheck::CommandChanged);
}

TEST(ContextMenuCheckCommand, SharedPathIsAmbiguous) {
    const std::vector<MenuTreeNode> roots{
        Submenu("Send", {Command(4, "Send/Favorites"), Command(5, "Send/Favorites")})};
    EXPECT_EQ(CheckCommand(roots, 4, "Send/Favorites"), CommandCheck::AmbiguousPath);
}

TEST(ContextMenuCheckCommand, DisabledOrHiddenCommandOrParentIsRefused) {
    auto disabled = Command(1, "Play");
    disabled.enabled = false;
    EXPECT_EQ(CheckCommand({disabled}, 1, "Play"), CommandCheck::Disabled);

    auto hidden = Command(1, "Play");
    hidden.hidden = true;
    EXPECT_EQ(CheckCommand({hidden}, 1, "Play"), CommandCheck::Disabled);

    auto parent = Submenu("Tag", {Command(2, "Tag/Reload")});
    parent.enabled = false;
    EXPECT_EQ(CheckCommand({parent}, 2, "Tag/Reload"), CommandCheck::Disabled);
}
