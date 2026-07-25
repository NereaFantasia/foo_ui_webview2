#include "pch.h"
#include "../src/webview/WebViewCrashPolicy.h"

using namespace webview_crash_policy;

// ==========================================================================
// WebView2 进程崩溃处置策略
//
// 覆盖的僵尸态：BROWSER_PROCESS_EXITED (kind=0) 之后 environment 与
// controller 一并失效，后续所有 COM 调用返回 0x8007139F
// (ERROR_INVALID_STATE)，表现为主窗口渲染丢失且 DevTools 无法唤出，
// 而纯 Win32 的 tray 菜单仍正常响应。此类失败 Reload 无法自愈，
// 必须整窗重建，故 needRebuild 契约必须在消费端被处置。
// ==========================================================================

// ---------- 渲染进程类：Reload 可自愈 ----------

TEST(WebViewCrashPolicyTest, RenderCrashWithSuccessfulReloadIsSelfHealed) {
    const auto d = Decide(FailureClass::Render, /*reloadSucceeded=*/true);
    EXPECT_TRUE(d.recovered);
    EXPECT_FALSE(d.markPanelDead);
    // controller 与 environment 均未失效，不得作废共享环境。
    EXPECT_FALSE(d.invalidateEnvironment);
}

TEST(WebViewCrashPolicyTest, RenderCrashWithFailedReloadMarksPanelDead) {
    const auto d = Decide(FailureClass::Render, /*reloadSucceeded=*/false);
    EXPECT_FALSE(d.recovered);
    // Reload 失败后页面不可用，门卫必须拦住后续调用。
    EXPECT_TRUE(d.markPanelDead);
    // 但 environment 仍属浏览器进程存活状态，不应作废。
    EXPECT_FALSE(d.invalidateEnvironment);
}

// ---------- 浏览器进程退出：本次现场 ----------

TEST(WebViewCrashPolicyTest, BrowserExitMarksDeadAndInvalidatesEnvironment) {
    const auto d = Decide(FailureClass::Browser, /*reloadSucceeded=*/false);
    EXPECT_FALSE(d.recovered);
    EXPECT_TRUE(d.markPanelDead);
    // environment 与浏览器进程同生共死；不作废会让重建复用失效对象。
    EXPECT_TRUE(d.invalidateEnvironment);
}

TEST(WebViewCrashPolicyTest, BrowserExitIgnoresReloadOutcome) {
    // 浏览器进程退出时不会发起 Reload，该入参不得影响判定。
    const auto withReload = Decide(FailureClass::Browser, true);
    const auto withoutReload = Decide(FailureClass::Browser, false);
    EXPECT_EQ(withReload.recovered, withoutReload.recovered);
    EXPECT_EQ(withReload.markPanelDead, withoutReload.markPanelDead);
    EXPECT_EQ(withReload.invalidateEnvironment,
              withoutReload.invalidateEnvironment);
}

// ---------- GPU / utility 等：运行时自愈 ----------

TEST(WebViewCrashPolicyTest, OtherProcessExitLeavesWebViewUsable) {
    const auto d = Decide(FailureClass::Other, /*reloadSucceeded=*/false);
    EXPECT_TRUE(d.recovered);
    EXPECT_FALSE(d.markPanelDead);
    EXPECT_FALSE(d.invalidateEnvironment);
}

// ---------- 跨分类不变量 ----------

TEST(WebViewCrashPolicyTest, EnvironmentInvalidationIsExclusiveToBrowserExit) {
    // 作废进程级共享 environment 会影响所有窗口，只允许浏览器进程退出触发。
    for (const bool reloaded : {false, true}) {
        EXPECT_FALSE(Decide(FailureClass::Render, reloaded).invalidateEnvironment);
        EXPECT_FALSE(Decide(FailureClass::Other, reloaded).invalidateEnvironment);
        EXPECT_TRUE(Decide(FailureClass::Browser, reloaded).invalidateEnvironment);
    }
}

TEST(WebViewCrashPolicyTest, RecoveredAndMarkDeadAreMutuallyExclusive) {
    // recovered 是上报给消费端的"无需干预"信号，与僵尸标记不可同时成立，
    // 否则门卫状态与事件 payload 会互相矛盾。
    for (const auto failure :
         {FailureClass::Render, FailureClass::Browser, FailureClass::Other}) {
        for (const bool reloaded : {false, true}) {
            const auto d = Decide(failure, reloaded);
            EXPECT_NE(d.recovered, d.markPanelDead);
        }
    }
}

TEST(WebViewCrashPolicyTest, EnvironmentInvalidationImpliesPanelDead) {
    // 作废 environment 时页面必然已不可用，两个标记不能脱耦。
    for (const auto failure :
         {FailureClass::Render, FailureClass::Browser, FailureClass::Other}) {
        for (const bool reloaded : {false, true}) {
            const auto d = Decide(failure, reloaded);
            if (d.invalidateEnvironment) {
                EXPECT_TRUE(d.markPanelDead);
            }
        }
    }
}

// ==========================================================================
// ClassifyFailedKind：把 WebView2 的 kind 谓词映射到分类
// ==========================================================================

TEST(WebViewCrashPolicyTest, ClassifyMapsRenderAndBrowserPredicates) {
    EXPECT_EQ(webview_crash_policy::ClassifyFailedKind(true, false),
              FailureClass::Render);
    EXPECT_EQ(webview_crash_policy::ClassifyFailedKind(false, true),
              FailureClass::Browser);
    EXPECT_EQ(webview_crash_policy::ClassifyFailedKind(false, false),
              FailureClass::Other);
}

TEST(WebViewCrashPolicyTest, ClassifyPrefersRenderWhenBothPredicatesSet) {
    // 两个谓词互斥；若调用方误传双真，选 Render 以先尝试 Reload 自愈，
    // 失败后仍会经 Decide 收敛到僵尸标记，不会漏掉门卫保护。
    EXPECT_EQ(webview_crash_policy::ClassifyFailedKind(true, true),
              FailureClass::Render);
}

TEST(WebViewCrashPolicyTest, BrowserExitCrashFromRealIncidentInvalidatesEnvironment) {
    // 现场回归：kind=0 (BROWSER_PROCESS_EXITED)、exitCode=0xC0000005。
    // 此前该路径只写日志，导致主窗口僵死 7 分钟且重建复用失效 environment。
    const auto failure = webview_crash_policy::ClassifyFailedKind(false, true);
    const auto d = Decide(failure, /*reloadSucceeded=*/false);
    EXPECT_FALSE(d.recovered);
    EXPECT_TRUE(d.markPanelDead);
    EXPECT_TRUE(d.invalidateEnvironment);
}
