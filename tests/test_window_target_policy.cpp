// test_window_target_policy.cpp - WindowTargetPolicy 纯决策逻辑覆盖
//
// P0-b / R3: 本文件**直接链接真实生产符号** window_target_policy::*，
// 不在测试内重新实现被测逻辑。
//
// 断言依据是 WindowTargetResolver.cpp 的**现状**实现（P0 不改变行为）：
//   ResolveForMutation:    显式 id → caller → 失败（禁止回退 main）
//   ResolveForObservation: 显式 id → caller → 回退 main
// 两者在「显式 id」分支都是直接 return ResolveById()，失败不回退。
//
// 真实 HWND 查找（ResolveById / ResolveByCallerHwnd 的窗口枚举、
// GetAncestor(GA_ROOT) 在 panel 下的返回值）需要活动窗口，属手工验证清单，
// 本文件不覆盖。
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

TEST(WindowTargetPolicyExplicitId, EmptyStringIdFallsBackForObservation) {
    TargetRequest r = WithExplicitId("");
    auto d = SelectTarget(r, TargetIntent::Observation);
    EXPECT_EQ(d.route, TargetRoute::FallbackMain);
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

TEST(WindowTargetPolicyNoTarget, ObservationFallsBackToMain) {
    auto d = SelectTarget(WithNothing(), TargetIntent::Observation);
    EXPECT_EQ(d.route, TargetRoute::FallbackMain);
    EXPECT_TRUE(d.error.empty());
}

// ============================================
// caller 查找失败后的回退差异
// ============================================

TEST(WindowTargetPolicyFallback, MutationDisallowsFallbackAfterCallerMiss) {
    EXPECT_FALSE(AllowsFallbackAfterCallerMiss(TargetIntent::Mutation));
}

TEST(WindowTargetPolicyFallback, ObservationAllowsFallbackAfterCallerMiss) {
    EXPECT_TRUE(AllowsFallbackAfterCallerMiss(TargetIntent::Observation));
}

// ============================================
// 错误常量不得与 ApiError 漂移
// ============================================

TEST(WindowTargetPolicyError, UsesApiErrorConstant) {
    EXPECT_STREQ(kErrorWindowNotFound, ApiError::WINDOW_NOT_FOUND);
}
