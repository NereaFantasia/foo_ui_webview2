// test_caller_context.cpp - CallerContext routing contract tests
// Uses reimpl pattern to test routing logic without Win32/WebViewContext dependencies.
#include "pch.h"
#include "harness/BridgeDispatchSimulator.h"

using json = nlohmann::json;

// ==========================================================================
// Reimplemented CallerContext struct (mirrors src/api/CallerContext contract)
// Uses BridgeDispatchSimulator* instead of BridgeCore*, no HWND dependency.
// ==========================================================================
namespace reimpl {

struct CallerContext {
    int callerHwnd = 0;  // simplified: use int instead of HWND
    std::string windowId;
    BridgeDispatchSimulator* bridge = nullptr;

    bool IsValid() const { return bridge != nullptr; }

    void EmitEvent(const std::string& event, const json& data,
                   BridgeDispatchSimulator* globalFallback = nullptr) const {
        if (bridge) {
            bridge->EmitEvent(event, data);
        } else if (globalFallback) {
            globalFallback->EmitEvent(event, data);
        }
    }

    bool TryEmitEvent(const std::string& event, const json& data) const {
        if (bridge) {
            bridge->EmitEvent(event, data);
            return true;
        }
        return false;
    }

    // Reimpl of FromParams routing logic:
    // 1. No _callerHwnd -> global fallback
    // 2. Invalid/zero _callerHwnd -> global fallback
    // 3. _callerHwnd in registry -> matched bridge
    // 4. _callerHwnd not in registry -> global fallback
    static CallerContext FromParams(
        const json& params,
        const std::unordered_map<int, BridgeDispatchSimulator*>& registry,
        BridgeDispatchSimulator* globalBridge)
    {
        CallerContext ctx;

        if (!params.contains("_callerHwnd")) {
            ctx.bridge = globalBridge;
            return ctx;
        }

        int hwnd = params.value("_callerHwnd", 0);
        if (hwnd == 0) {
            ctx.bridge = globalBridge;
            return ctx;
        }

        ctx.callerHwnd = hwnd;

        // Direct lookup
        auto it = registry.find(hwnd);
        if (it != registry.end()) {
            ctx.bridge = it->second;
            ctx.windowId = "window-" + std::to_string(hwnd);
            return ctx;
        }

        // Fallback to global
        ctx.bridge = globalBridge;
        return ctx;
    }
};

} // namespace reimpl

// ==========================================================================
// Test fixture
// ==========================================================================
class CallerContextTest : public ::testing::Test {
protected:
    BridgeDispatchSimulator globalBridge;
    BridgeDispatchSimulator instanceBridge1;
    BridgeDispatchSimulator instanceBridge2;
    std::unordered_map<int, BridgeDispatchSimulator*> registry;

    void SetUp() override {
        registry[1001] = &instanceBridge1;
        registry[2002] = &instanceBridge2;
    }
};

// ==========================================================================
// FromParams routing
// ==========================================================================

TEST_F(CallerContextTest, NoCallerHwnd_FallsBackToGlobal) {
    json params = {{"volume", 0.5}};
    auto ctx = reimpl::CallerContext::FromParams(params, registry, &globalBridge);
    EXPECT_TRUE(ctx.IsValid());
    EXPECT_EQ(ctx.bridge, &globalBridge);
    EXPECT_EQ(ctx.callerHwnd, 0);
}

TEST_F(CallerContextTest, ZeroCallerHwnd_FallsBackToGlobal) {
    json params = {{"_callerHwnd", 0}};
    auto ctx = reimpl::CallerContext::FromParams(params, registry, &globalBridge);
    EXPECT_TRUE(ctx.IsValid());
    EXPECT_EQ(ctx.bridge, &globalBridge);
}

TEST_F(CallerContextTest, ValidCallerHwnd_MatchesInstance) {
    json params = {{"_callerHwnd", 1001}};
    auto ctx = reimpl::CallerContext::FromParams(params, registry, &globalBridge);
    EXPECT_TRUE(ctx.IsValid());
    EXPECT_EQ(ctx.bridge, &instanceBridge1);
    EXPECT_EQ(ctx.callerHwnd, 1001);
    EXPECT_EQ(ctx.windowId, "window-1001");
}

TEST_F(CallerContextTest, SecondInstance_MatchesCorrectBridge) {
    json params = {{"_callerHwnd", 2002}};
    auto ctx = reimpl::CallerContext::FromParams(params, registry, &globalBridge);
    EXPECT_EQ(ctx.bridge, &instanceBridge2);
    EXPECT_EQ(ctx.windowId, "window-2002");
}

TEST_F(CallerContextTest, UnregisteredCallerHwnd_FallsBackToGlobal) {
    json params = {{"_callerHwnd", 9999}};
    auto ctx = reimpl::CallerContext::FromParams(params, registry, &globalBridge);
    EXPECT_TRUE(ctx.IsValid());
    EXPECT_EQ(ctx.bridge, &globalBridge);
}

// ==========================================================================
// IsValid
// ==========================================================================

TEST_F(CallerContextTest, IsValid_WithBridge_True) {
    reimpl::CallerContext ctx;
    ctx.bridge = &instanceBridge1;
    EXPECT_TRUE(ctx.IsValid());
}

TEST_F(CallerContextTest, IsValid_NullBridge_False) {
    reimpl::CallerContext ctx;
    EXPECT_FALSE(ctx.IsValid());
}

// ==========================================================================
// EmitEvent routing
// ==========================================================================

TEST_F(CallerContextTest, EmitEvent_RoutesToInstanceBridge) {
    json params = {{"_callerHwnd", 1001}};
    auto ctx = reimpl::CallerContext::FromParams(params, registry, &globalBridge);
    ctx.EmitEvent("playback:stateChanged", {{"state", "playing"}}, &globalBridge);

    EXPECT_EQ(instanceBridge1.GetMessageCount(), 1u);
    EXPECT_EQ(globalBridge.GetMessageCount(), 0u);
    EXPECT_EQ(instanceBridge1.LastMessage()["event"], "playback:stateChanged");
}

TEST_F(CallerContextTest, EmitEvent_FallsBackToGlobal_WhenNoBridge) {
    reimpl::CallerContext ctx;
    ctx.bridge = nullptr;
    ctx.EmitEvent("playback:stateChanged", {{"state", "paused"}}, &globalBridge);

    EXPECT_EQ(globalBridge.GetMessageCount(), 1u);
    EXPECT_EQ(globalBridge.LastMessage()["event"], "playback:stateChanged");
}

// ==========================================================================
// TryEmitEvent
// ==========================================================================

TEST_F(CallerContextTest, TryEmitEvent_Success_WhenBridgeValid) {
    json params = {{"_callerHwnd", 1001}};
    auto ctx = reimpl::CallerContext::FromParams(params, registry, &globalBridge);
    bool ok = ctx.TryEmitEvent("audio:spectrum", {{"bins", json::array()}});
    EXPECT_TRUE(ok);
    EXPECT_EQ(instanceBridge1.GetMessageCount(), 1u);
}

TEST_F(CallerContextTest, TryEmitEvent_Fails_WhenNoBridge) {
    reimpl::CallerContext ctx;
    bool ok = ctx.TryEmitEvent("audio:spectrum", {{}});
    EXPECT_FALSE(ok);
}

// ==========================================================================
// Additional params preserved
// ==========================================================================

TEST_F(CallerContextTest, OtherParams_NotAffected) {
    json params = {{"_callerHwnd", 1001}, {"volume", 0.7}, {"mute", false}};
    auto ctx = reimpl::CallerContext::FromParams(params, registry, &globalBridge);
    EXPECT_EQ(ctx.bridge, &instanceBridge1);
    // _callerHwnd is peeled off but other params remain in the json
    EXPECT_DOUBLE_EQ(params["volume"].get<double>(), 0.7);
    EXPECT_FALSE(params["mute"].get<bool>());
}
