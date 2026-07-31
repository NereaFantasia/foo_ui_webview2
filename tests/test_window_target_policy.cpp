// test_window_target_policy.cpp - WindowTargetPolicy 纯决策逻辑覆盖
//
// P0-b / R3: 本文件**直接链接真实生产符号** window_target_policy::*，
// 不在测试内重新实现被测逻辑。
//
// 断言依据是 WindowTargetResolver.cpp 的实现（P1' 已落实 Q7/Q8）：
//   显式 id → panel caller 判定 → caller → 失败
// 两种意图行为**完全一致**：均不回退主窗口（Q7-1）。
// 两者在「显式 id」分支都是直接 return ResolveById()，失败不回退。
//
// 真实 HWND 查找（ResolveById / ResolveByCallerHwnd 的窗口柚举、
// IsPanelCallerHwnd 的 WebViewContext 遍历、GetAncestor(GA_ROOT) 在 panel 下的
// 返回值）需要活动窗口，属手工验证清单，本文件不覆盖。
#include "pch.h"
#include "../src/window/WindowTargetPolicy.h"
#include "../src/api/ApiConstants.h"

using namespace window_target_policy;

namespace {

TargetRequest WithExplicitId(const char* id) {
    TargetRequest r;
    r.hasExplicitWindowId = true;
    r.explicitWindowId = id;
    return r;
}

TargetRequest WithCallerOnly() {
    TargetRequest r;
    r.hasCallerHwnd = true;
    return r;
}

TargetRequest WithNothing() {
    return TargetRequest{};
}

TargetRequest WithPanelCaller() {
    TargetRequest r;
    r.hasCallerHwnd = true;
    r.callerIsPanel = true;
    return r;
}

}  // namespace

// ============================================
// 显式 windowId 优先（两种意图行为相同）
// ============================================

TEST(WindowTargetPolicyExplicitId, MutationRoutesById) {
    auto d = SelectTarget(WithExplicitId("popup-1"), TargetIntent::Mutation);
    EXPECT_EQ(d.route, TargetRoute::ById);
    EXPECT_EQ(d.windowId, "popup-1");
    EXPECT_TRUE(d.error.empty());
}

TEST(WindowTargetPolicyExplicitId, ObservationRoutesById) {
    auto d = SelectTarget(WithExplicitId("popup-1"), TargetIntent::Observation);
    EXPECT_EQ(d.route, TargetRoute::ById);
    EXPECT_EQ(d.windowId, "popup-1");
}

// 显式 id 压过 caller —— 这是 D13 类缺陷的判定基准：
// resolver 会尊重显式 id，而绕过 resolver 直接用 GetCallerHwnd 的 handler 不会。
TEST(WindowTargetPolicyExplicitId, ExplicitIdOutranksCaller) {
    TargetRequest r = WithExplicitId("popup-2");
    r.hasCallerHwnd = true;
    auto d = SelectTarget(r, TargetIntent::Mutation);
    EXPECT_EQ(d.route, TargetRoute::ById);
    EXPECT_EQ(d.windowId, "popup-2");
}

TEST(WindowTargetPolicyExplicitId, MainIsJustAnotherId) {
    auto d = SelectTarget(WithExplicitId("main"), TargetIntent::Mutation);
    EXPECT_EQ(d.route, TargetRoute::ById);
    EXPECT_EQ(d.windowId, "main");
}

// 空字符串 windowId 等同「未提供」—— 对齐 resolver 的 !wid.empty() 判断。
TEST(WindowTargetPolicyExplicitId, EmptyStringIdIsTreatedAsAbsent) {
    TargetRequest r = WithExplicitId("");
    auto d = SelectTarget(r, TargetIntent::Mutation);
    EXPECT_EQ(d.route, TargetRoute::Fail);
    EXPECT_EQ(d.error, ApiError::WINDOW_NOT_FOUND);
}

// Q7-1 之前此例期望 FallbackMain；取消回退后 observation 与 mutation 一致失败。
TEST(WindowTargetPolicyExplicitId, EmptyStringIdFailsForObservationToo) {
    TargetRequest r = WithExplicitId("");
    auto d = SelectTarget(r, TargetIntent::Observation);
    EXPECT_EQ(d.route, TargetRoute::Fail);
    EXPECT_EQ(d.error, ApiError::WINDOW_NOT_FOUND);
}

TEST(WindowTargetPolicyExplicitId, EmptyStringIdStillHonorsCaller) {
    TargetRequest r = WithExplicitId("");
    r.hasCallerHwnd = true;
    auto d = SelectTarget(r, TargetIntent::Mutation);
    EXPECT_EQ(d.route, TargetRoute::ByCallerHwnd);
}

// 显式 id 查找失败时不得回退 —— 否则 setBounds({windowId:'typo'}) 会静默改主窗口。
TEST(WindowTargetPolicyExplicitId, ExplicitIdNeverFallsBack) {
    EXPECT_FALSE(SelectTarget(WithExplicitId("x"), TargetIntent::Mutation).explicitIdMayFallBack);
    EXPECT_FALSE(SelectTarget(WithExplicitId("x"), TargetIntent::Observation).explicitIdMayFallBack);
}

// ============================================
// caller 路径
// ============================================

TEST(WindowTargetPolicyCaller, MutationRoutesByCaller) {
    auto d = SelectTarget(WithCallerOnly(), TargetIntent::Mutation);
    EXPECT_EQ(d.route, TargetRoute::ByCallerHwnd);
    EXPECT_TRUE(d.error.empty());
}

TEST(WindowTargetPolicyCaller, ObservationRoutesByCaller) {
    auto d = SelectTarget(WithCallerOnly(), TargetIntent::Observation);
    EXPECT_EQ(d.route, TargetRoute::ByCallerHwnd);
}

TEST(WindowTargetPolicyCaller, CallerRouteCarriesNoWindowId) {
    auto d = SelectTarget(WithCallerOnly(), TargetIntent::Mutation);
    EXPECT_TRUE(d.windowId.empty());
}

// ============================================
// 两者皆缺：意图决定行为
// ============================================

TEST(WindowTargetPolicyNoTarget, MutationFails) {
    auto d = SelectTarget(WithNothing(), TargetIntent::Mutation);
    EXPECT_EQ(d.route, TargetRoute::Fail);
    EXPECT_EQ(d.error, ApiError::WINDOW_NOT_FOUND);
}

// Q7-1: observation 不再回退主窗口。
//
// 回退会向非主窗口的调用方返回属于另一个窗口的几何/状态值，
// 调用方无从分辨——比返回错误更有害。
TEST(WindowTargetPolicyNoTarget, ObservationFailsInsteadOfFallingBack) {
    auto d = SelectTarget(WithNothing(), TargetIntent::Observation);
    EXPECT_EQ(d.route, TargetRoute::Fail);
    EXPECT_EQ(d.error, ApiError::WINDOW_NOT_FOUND);
}

TEST(WindowTargetPolicyNoTarget, BothIntentsAgree) {
    auto m = SelectTarget(WithNothing(), TargetIntent::Mutation);
    auto o = SelectTarget(WithNothing(), TargetIntent::Observation);
    EXPECT_EQ(m.route, o.route);
    EXPECT_EQ(m.error, o.error);
}

// ============================================
// panel caller 显式失败（Q7-2 / Q8）
// ============================================

// 面板不实现 WindowShellBase，永远不可能成为 target。
// 旧实现在此处去调 ResolveById("panel_N")，而该函数只认 "main" 与 popup id，
// 故那是一条结构上永远走不通的路径（Q8）。
TEST(WindowTargetPolicyPanelCaller, MutationFailsWithPanelReason) {
    auto d = SelectTarget(WithPanelCaller(), TargetIntent::Mutation);
    EXPECT_EQ(d.route, TargetRoute::FailPanelCaller);
    EXPECT_EQ(d.error, ApiError::PANEL_CALLER_UNSUPPORTED);
}

TEST(WindowTargetPolicyPanelCaller, ObservationFailsWithPanelReason) {
    auto d = SelectTarget(WithPanelCaller(), TargetIntent::Observation);
    EXPECT_EQ(d.route, TargetRoute::FailPanelCaller);
    EXPECT_EQ(d.error, ApiError::PANEL_CALLER_UNSUPPORTED);
}

// panel 判定必须先于 caller 判定，否则会退化成泛化的 WINDOW_NOT_FOUND，
// 丢失确切原因。
TEST(WindowTargetPolicyPanelCaller, PanelOutranksCallerRoute) {
    auto d = SelectTarget(WithPanelCaller(), TargetIntent::Mutation);
    EXPECT_NE(d.route, TargetRoute::ByCallerHwnd);
}

// 但显式 windowId 仍应压过 panel 判定：面板里的页面可以显式操作其他窗口
//（例如 window.focus("main")），那是合法的跳窗口调用，不应被面板身份拦下。
TEST(WindowTargetPolicyPanelCaller, ExplicitIdStillOutranksPanel) {
    TargetRequest r = WithExplicitId("main");
    r.hasCallerHwnd = true;
    r.callerIsPanel = true;
    auto d = SelectTarget(r, TargetIntent::Mutation);
    EXPECT_EQ(d.route, TargetRoute::ById);
    EXPECT_EQ(d.windowId, "main");
}

// ============================================
// caller 查找失败后一律不回退（Q7-1）
// ============================================

TEST(WindowTargetPolicyFallback, MutationDisallowsFallbackAfterCallerMiss) {
    EXPECT_FALSE(AllowsFallbackAfterCallerMiss(TargetIntent::Mutation));
}

// Q7-1 之前此例期望 true。
TEST(WindowTargetPolicyFallback, ObservationAlsoDisallowsFallbackAfterCallerMiss) {
    EXPECT_FALSE(AllowsFallbackAfterCallerMiss(TargetIntent::Observation));
}

// ============================================
// 错误常量不得与 ApiError 漂移
// ============================================

TEST(WindowTargetPolicyError, UsesApiErrorConstant) {
    EXPECT_STREQ(kErrorWindowNotFound, ApiError::WINDOW_NOT_FOUND);
}

TEST(WindowTargetPolicyError, PanelErrorUsesApiErrorConstant) {
    EXPECT_STREQ(kErrorPanelCallerUnsupported, ApiError::PANEL_CALLER_UNSUPPORTED);
}
