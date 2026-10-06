#include "pch.h"
#include "../src/api/WindowCallerIdentity.h"

#include <algorithm>
#include <map>
#include <optional>

namespace {

enum class PanelMode { Standalone, DuiPanel, CuiPanel, Unknown };

struct TestWindow {
    HWND hwnd = nullptr;
    HWND GetHwnd() const { return hwnd; }
};

struct TestPanel {
    PanelMode mode;
    std::string panelName;
    PanelMode GetMode() const { return mode; }
    bool IsPanelMode() const { return mode != PanelMode::Standalone; }
};

struct TestManager {
    TestWindow main;
    std::map<std::string, TestWindow> popups;

    TestWindow* GetMainWindow() { return main.hwnd ? &main : nullptr; }

    std::vector<std::string> GetAllWindowIds() const {
        std::vector<std::string> ids;
        if (main.hwnd) ids.push_back("main");
        for (const auto& entry : popups) ids.push_back(entry.first);
        return ids;
    }

    TestWindow* GetPopup(const std::string& id) {
        const auto found = popups.find(id);
        return found == popups.end() ? nullptr : &found->second;
    }
};

struct TestContext {
    std::map<HWND, std::string> windowIds;
    std::map<HWND, TestPanel> panels;
    std::vector<HWND> panelQueries;

    std::vector<HWND> GetAllInstances() const {
        std::vector<HWND> instances;
        for (const auto& entry : windowIds) instances.push_back(entry.first);
        return instances;
    }

    TestPanel* GetPanelByHwnd(HWND hwnd) {
        panelQueries.push_back(hwnd);
        // 与 WebViewContext::GetPanelByHwnd 相同：只有未登记的 HWND 才查顶层。
        if (!windowIds.contains(hwnd)) hwnd = ::GetAncestor(hwnd, GA_ROOT);
        const auto found = panels.find(hwnd);
        return found == panels.end() ? nullptr : &found->second;
    }

    std::string GetWindowIdByHwnd(HWND hwnd) const {
        const auto found = windowIds.find(hwnd);
        return found == windowIds.end() ? "" : found->second;
    }
};

class WindowCallerIdentityTest : public testing::Test {
protected:
    HWND MakeWindow(HWND parent = nullptr, bool popup = false) {
        const DWORD style = popup ? WS_POPUP : parent ? WS_CHILD : WS_OVERLAPPED;
        HWND hwnd = ::CreateWindowExW(0, L"STATIC", L"", style, 0, 0, 10, 10,
            parent, nullptr, ::GetModuleHandleW(nullptr), nullptr);
        EXPECT_NE(hwnd, nullptr);
        if (hwnd) windows_.push_back(hwnd);
        return hwnd;
    }

    void TearDown() override {
        for (auto it = windows_.rbegin(); it != windows_.rend(); ++it) {
            if (::IsWindow(*it)) ::DestroyWindow(*it);
        }
    }

    window_caller::Identity Resolve(HWND hwnd, bool fallbackPanelMode = false) {
        return window_caller::Resolve(hwnd, manager_, context_, fallbackPanelMode);
    }

    TestPanel& RegisterPanel(HWND hwnd, const std::string& id, PanelMode mode) {
        context_.windowIds[hwnd] = id;
        return context_.panels.insert_or_assign(hwnd, TestPanel{mode, ""}).first->second;
    }

    TestManager manager_;
    TestContext context_;

private:
    std::vector<HWND> windows_;
};

TEST_F(WindowCallerIdentityTest, ExactSiblingPanelsKeepTheirOwnIdentity) {
    HWND root = MakeWindow();
    HWND dui = MakeWindow(root);
    HWND cui = MakeWindow(root);
    manager_.main.hwnd = root;
    RegisterPanel(dui, "dui-a", PanelMode::DuiPanel);
    RegisterPanel(cui, "cui-b", PanelMode::CuiPanel);

    for (HWND hwnd : {dui, cui}) {
        const auto identity = Resolve(hwnd);
        EXPECT_EQ(identity.panelHwnd, hwnd);
        EXPECT_EQ(identity.windowId, hwnd == dui ? "dui-a" : "cui-b");
        EXPECT_EQ(identity.mode, hwnd == dui ? "dui" : "cui");
        EXPECT_TRUE(identity.panelMode);
        EXPECT_EQ(identity.CurrentWindowId(), identity.ModeWindowId());
    }
}

TEST_F(WindowCallerIdentityTest, NestedChildrenResolveTheNearestPanel) {
    HWND root = MakeWindow();
    HWND outerPanel = MakeWindow(root);
    HWND innerPanel = MakeWindow(outerPanel);
    HWND child = MakeWindow(MakeWindow(innerPanel));
    RegisterPanel(outerPanel, "dui-outer", PanelMode::DuiPanel);
    RegisterPanel(innerPanel, "cui-inner", PanelMode::CuiPanel);

    const auto identity = Resolve(child);
    EXPECT_EQ(identity.panelHwnd, innerPanel);
    EXPECT_EQ(identity.CurrentWindowId(), "cui-inner");
}

TEST_F(WindowCallerIdentityTest, PanelConfigurationTargetsRemainIsolated) {
    HWND root = MakeWindow();
    HWND first = MakeWindow(root);
    HWND second = MakeWindow(root);
    auto& firstPanel = RegisterPanel(first, "dui-a", PanelMode::DuiPanel);
    auto& secondPanel = RegisterPanel(second, "dui-b", PanelMode::DuiPanel);
    firstPanel.panelName = "first";
    secondPanel.panelName = "second";

    const auto firstTarget = Resolve(MakeWindow(first)).panelHwnd;
    const auto secondTarget = Resolve(MakeWindow(second)).panelHwnd;
    auto* firstConfigTarget = context_.GetPanelByHwnd(firstTarget);
    auto* secondConfigTarget = context_.GetPanelByHwnd(secondTarget);
    ASSERT_EQ(firstConfigTarget, &firstPanel);
    ASSERT_EQ(secondConfigTarget, &secondPanel);
    EXPECT_EQ(firstConfigTarget->panelName, "first");
    EXPECT_EQ(secondConfigTarget->panelName, "second");
    firstConfigTarget->panelName = "first changed";
    EXPECT_EQ(secondConfigTarget->panelName, "second");
    secondConfigTarget->panelName = "second changed";
    EXPECT_EQ(firstConfigTarget->panelName, "first changed");
    EXPECT_EQ(Resolve(root).panelHwnd, nullptr);
}

TEST_F(WindowCallerIdentityTest, MainAndItsChildrenStayStandaloneWithPanelsPresent) {
    HWND main = MakeWindow();
    manager_.main.hwnd = main;
    RegisterPanel(MakeWindow(main), "dui-a", PanelMode::DuiPanel);
    for (HWND hwnd : {main, MakeWindow(main)}) {
        const auto identity = Resolve(hwnd, true);
        EXPECT_EQ(identity.CurrentWindowId(), "main");
        EXPECT_EQ(identity.ModeWindowId(), "main");
        EXPECT_EQ(identity.mode, "standalone");
        EXPECT_FALSE(identity.panelMode);
        EXPECT_EQ(identity.panelHwnd, nullptr);
    }
}

TEST_F(WindowCallerIdentityTest, OwnedPopupsAndTheirChildrenKeepDistinctIds) {
    HWND main = MakeWindow();
    manager_.main.hwnd = main;
    HWND panel = MakeWindow(main);
    RegisterPanel(panel, "dui-a", PanelMode::DuiPanel);
    HWND first = MakeWindow(main, true);
    HWND second = MakeWindow(panel, true);
    manager_.popups["popup_1"].hwnd = first;
    manager_.popups["popup_2"].hwnd = second;

    for (HWND popup : {first, second}) {
        for (HWND hwnd : {popup, MakeWindow(popup)}) {
            const auto identity = Resolve(hwnd, true);
            EXPECT_EQ(identity.CurrentWindowId(), popup == first ? "popup_1" : "popup_2");
            EXPECT_EQ(identity.ModeWindowId(), identity.CurrentWindowId());
            EXPECT_EQ(identity.mode, "standalone");
            EXPECT_FALSE(identity.panelMode);
            EXPECT_EQ(identity.panelHwnd, nullptr);
        }
    }
}

TEST_F(WindowCallerIdentityTest, AnUnregisteredPopupDoesNotFollowItsOwner) {
    HWND owner = MakeWindow();
    RegisterPanel(owner, "dui-owner", PanelMode::DuiPanel);
    HWND popup = MakeWindow(owner, true);
    ASSERT_EQ(::GetWindow(popup, GW_OWNER), owner);

    const auto identity = Resolve(MakeWindow(popup));
    EXPECT_EQ(identity.CurrentWindowId(), "main");
    EXPECT_EQ(identity.panelHwnd, nullptr);
    EXPECT_TRUE(context_.panelQueries.empty());
}

TEST_F(WindowCallerIdentityTest, UnmatchedCallersKeepTheExistingFallback) {
    HWND root = MakeWindow();
    RegisterPanel(MakeWindow(root), "dui-a", PanelMode::DuiPanel);
    for (HWND caller : {root, MakeWindow(root), static_cast<HWND>(nullptr)}) {
        for (bool fallbackPanelMode : {false, true}) {
            const auto identity = Resolve(caller, fallbackPanelMode);
            EXPECT_EQ(identity.CurrentWindowId(), "main");
            EXPECT_EQ(identity.ModeWindowId(), "main");
            EXPECT_EQ(identity.panelHwnd, nullptr);
            EXPECT_EQ(identity.panelMode, fallbackPanelMode);
            EXPECT_EQ(identity.mode, fallbackPanelMode ? "panel" : "standalone");
        }
    }
}

TEST_F(WindowCallerIdentityTest, UnknownPanelKindsAndMissingIdsKeepTheirFallbacks) {
    HWND panel = MakeWindow();
    RegisterPanel(panel, "", PanelMode::Unknown);
    const auto identity = Resolve(panel);
    EXPECT_EQ(identity.mode, "unknown");
    EXPECT_TRUE(identity.panelMode);
    EXPECT_EQ(identity.CurrentWindowId(), "main");
    EXPECT_EQ(identity.ModeWindowId(), "panel");
    EXPECT_EQ(identity.panelHwnd, panel);
}

TEST_F(WindowCallerIdentityTest, ExactRegistrationPreventsContextRootFallbackFromSkippingPanel) {
    HWND root = MakeWindow();
    HWND panel = MakeWindow(root);
    HWND child = MakeWindow(panel);
    auto& rootPanel = RegisterPanel(root, "dui-root", PanelMode::DuiPanel);
    RegisterPanel(panel, "cui-nearest", PanelMode::CuiPanel);
    ASSERT_EQ(context_.GetPanelByHwnd(child), &rootPanel);
    context_.panelQueries.clear();

    const auto identity = Resolve(child);
    EXPECT_EQ(identity.panelHwnd, panel);
    EXPECT_EQ(identity.CurrentWindowId(), "cui-nearest");
    EXPECT_EQ(identity.mode, "cui");
    EXPECT_EQ(context_.panelQueries, std::vector<HWND>{panel});
}

TEST_F(WindowCallerIdentityTest, RegisteredInstancesWithoutPanelsContinueToTheParent) {
    HWND root = MakeWindow();
    HWND child = MakeWindow(root);
    RegisterPanel(root, "dui-parent", PanelMode::DuiPanel);
    context_.windowIds[child] = "registered-without-panel";

    const auto identity = Resolve(child);
    EXPECT_EQ(identity.panelHwnd, root);
    EXPECT_EQ(identity.CurrentWindowId(), "dui-parent");
    EXPECT_EQ(identity.mode, "dui");
}

} // namespace
