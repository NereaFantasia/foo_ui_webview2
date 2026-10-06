// test_port_registry.cpp - which ports close with a window, with a page, or by id.
//
// PortHub announces every port these calls return with port:disconnected, so
// "returned" here means "announced" there.
#include "pch.h"
#include "../src/api/PortRegistry.h"
#include <algorithm>
#include <string>
#include <vector>
using port_registry::PageKey;
using port_registry::Port;
using port_registry::PortRegistry;
namespace {
// Two pages in the same window, as two panels under one top-level window
// report, and a page of another window.
// Elements of one array, so the three keys are distinct addresses.
int gPages[3] = {};
const PageKey kA = &gPages[0];
const PageKey kB = &gPages[1];
const PageKey kC = &gPages[2];

std::vector<std::string> Ids(const std::vector<Port>& ports) {
    std::vector<std::string> ids;
    for (const auto& port : ports) ids.push_back(port.portId);
    return ids;
}

std::vector<std::string> Channel(const PortRegistry& registry, const std::string& name) {
    std::vector<std::string> ids;
    for (const auto* port : registry.ChannelPorts(name)) ids.push_back(port->portId);
    std::sort(ids.begin(), ids.end());
    return ids;
}

// port_1 and port_2 from page A, port_3 from page B (both window "panel"),
// port_4 from page C (window "main"); ports 1, 3 and 4 share channel "sync".
PortRegistry Populated() {
    PortRegistry registry;
    registry.Add({"port_1", "sync", "panel", kA});
    registry.Add({"port_2", "other", "panel", kA});
    registry.Add({"port_3", "sync", "panel", kB});
    registry.Add({"port_4", "sync", "main", kC});
    return registry;
}
}  // namespace

TEST(PortRegistry, RemovePageClosesOnlyThatPagesPorts) {
    auto registry = Populated();
    const auto removed = registry.RemovePage(kA);
    EXPECT_EQ(Ids(removed), (std::vector<std::string>{"port_1", "port_2"}));
    EXPECT_EQ(removed[0].name, "sync");
    EXPECT_EQ(removed[0].windowId, "panel");
    EXPECT_EQ(registry.Find("port_1"), nullptr);
    EXPECT_EQ(registry.Find("port_2"), nullptr);
    // Page B shares page A's window id but keeps its port.
    EXPECT_NE(registry.Find("port_3"), nullptr);
    EXPECT_EQ(Channel(registry, "sync"), (std::vector<std::string>{"port_3", "port_4"}));
    EXPECT_TRUE(Channel(registry, "other").empty());
    EXPECT_EQ(registry.All().size(), 2u);
}

TEST(PortRegistry, RemovePageTwiceAnnouncesOnce) {
    auto registry = Populated();
    EXPECT_EQ(registry.RemovePage(kA).size(), 2u);
    EXPECT_TRUE(registry.RemovePage(kA).empty());
}

TEST(PortRegistry, PortOpenedAfterPageCleanupSurvivesIt) {
    // The reloaded page opens a port on the same handle after the cleanup ran;
    // only a later cleanup, the next navigation, takes it.
    auto registry = Populated();
    registry.RemovePage(kA);
    registry.Add({"port_5", "sync", "panel", kA});
    EXPECT_NE(registry.Find("port_5"), nullptr);
    EXPECT_EQ(Ids(registry.RemovePage(kA)), (std::vector<std::string>{"port_5"}));
}

TEST(PortRegistry, UnknownPageIsNeverCleanedByPage) {
    PortRegistry registry;
    registry.Add({"port_1", "sync", "main", nullptr});
    EXPECT_TRUE(registry.RemovePage(nullptr).empty());
    EXPECT_NE(registry.Find("port_1"), nullptr);
    EXPECT_EQ(Ids(registry.RemoveWindow("main")), (std::vector<std::string>{"port_1"}));
}

TEST(PortRegistry, RemoveWindowClosesEveryPageOfIt) {
    auto registry = Populated();
    EXPECT_EQ(Ids(registry.RemoveWindow("panel")), (std::vector<std::string>{"port_1", "port_2", "port_3"}));
    EXPECT_TRUE(registry.RemovePage(kA).empty());
    EXPECT_TRUE(registry.RemovePage(kB).empty());
    EXPECT_EQ(Channel(registry, "sync"), (std::vector<std::string>{"port_4"}));
}

TEST(PortRegistry, RemoveByIdDropsItFromPageAndWindow) {
    auto registry = Populated();
    const auto removed = registry.Remove("port_1");
    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(removed->page, kA);
    EXPECT_FALSE(registry.Remove("port_1").has_value());
    EXPECT_EQ(Ids(registry.RemovePage(kA)), (std::vector<std::string>{"port_2"}));
    EXPECT_EQ(Ids(registry.RemoveWindow("panel")), (std::vector<std::string>{"port_3"}));
}
