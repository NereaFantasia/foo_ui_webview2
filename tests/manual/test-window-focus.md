# window.focus 自动化测试

> **测试目标**: 验证 `window.focus` 在 AttachThreadInput 修复后的行为正确性与性能。  
> **关联提交**: `c216a59` — fix: window.focus 绕过前台锁超时，AttachThreadInput 秒切弹窗  
> **用户反馈原文**: "window.focus 如果从主页发送指令让弹窗置顶或者通过消息传递让弹窗自己激活置顶，界面反应有时快有时慢（尤其弹窗页面尺寸大一点时），但在控制台执行 window.focus 秒切换。是不是抢进程？"

## 缺陷背景

Windows `ForegroundLockTimeout` 机制会在调用 `SetForegroundWindow` 的进程 **不是当前前台进程** 时，对激活请求强制延迟 200ms–5s（默认），期间只闪烁任务栏而不真正置顶。这就解释了为何用户感知到的响应"时快时慢"：

1. 用户点击 fb2k 主窗口里的按钮（或 DevTools 里的控制台）发出 `fb2k.invoke('window.focus', {windowId})` 指令。
2. 指令异步回到 C++ UI 线程执行 `SetForegroundWindow(popupHwnd)`。
3. 在指令到达 UI 线程的过程中，用户可能已经切到其他窗口（输入法条、别的 app、DevTools），此时 fb2k 不是前台进程 → `ForegroundLockTimeout` 生效 → 看到"卡顿"。

`c216a59` 修复通过 `AttachThreadInput` 临时把当前线程附着到前台线程，使 `SetForegroundWindow` 视为同进程调用，绕过限制；调用完毕立即 `AttachThreadInput(..., FALSE)` 断开，避免留下残留连接。

实现见 `src/api/WindowApi.cpp:930-946`（`WindowFocus` 函数）。

## 测试用例清单

| # | 用例名 | 自动化 | 验证点 | 预期 |
|---|--------|--------|--------|------|
| T1 | 基础流程 — 创建小弹窗并 focus | ✅ | `success=true`，无异常 | pass |
| T2 | 弹窗内自身 focus 无参数 | 🖐 | 在弹窗内调用 `focus()` | pass |
| T3 | 大尺寸弹窗 focus (1600×1000) | ✅ | 延迟 < 500ms | pass |
| T4 | focus 不存在的 windowId | ✅ | `success=false, error="Window not found"` | pass |
| T5 | 最小化后 focus 恢复 | 🖐 | 窗口从最小化恢复并聚焦 | pass |
| T6 | 快速连续 focus 稳定性 | ✅ | n=20, p95 < 200ms | pass |
| T7 | 前台锁绕过（跨进程场景）| 🖐 | 切到 Notepad 后 focus 弹窗 < 100ms | pass |
| T8 | 多弹窗轮询切换 | ✅ | 3 个 popup × 3 轮，max < 500ms | pass |
| T9 | focus("main") 回到主窗口 | ✅ | `success=true` | pass |
| T10 | 弹窗自身 focus 自己 | 🖐 | 弹窗内 focus(self.windowId) | pass |

**图例**：✅ = 自动化覆盖；🖐 = 需要人工配合。

自动化部分由 `test-window-focus.js` 执行；人工部分见本文"手工复现流程"。

---

## 运行方式 A：DevTools 控制台（推荐快速验证）

1. 启动 foobar2000（已安装 foo_ui_webview2 组件）。
2. 在主窗口按 `F12` 打开 DevTools（若未启用，见 `docs/DEVTOOLS.md`）。
3. 切到 **Console** 面板。
4. 打开 `tests/manual/test-window-focus.js`，复制全部内容。
5. 粘贴到 Console 并回车。
6. 观察输出：每个用例一行 `✅/❌/⏭️`，末尾输出总结。
7. 完整结果对象保存在 `window.__fb2kFocusTest`，可进一步查看：

```javascript
// 查看所有计时
window.__fb2kFocusTest.timings;

// 查看失败的用例
window.__fb2kFocusTest.cases.filter(c => c.status === 'fail');

// T6 延迟直方图
window.__fb2kFocusTest.cases.find(c => c.name === 'T6-rapid-succession');
```

### 期望输出示例（修复后）

```
✅ T1-basic-focus: focus(popup_1) ok in 3.2ms
✅ T3-large-popup: large focus in 8.7ms (阈值 < 500ms)
✅ T4-invalid-id: 正确拒绝: Window not found
✅ T6-rapid-succession: n=20 mean=2.1 p50=1.8 p95=4.3 p99=5.9 max=6.1 (ms)
✅ T8-multi-cycle: n=9 max=5.4ms (阈值 < 500ms)
✅ T9-focus-main: focus(main) ok in 2.7ms
⏭️ T5-minimized-restore: 程序化不可行 ...
⏭️ T2-self-focus: 需在弹窗页面内执行 ...
⏭️ T7-foreground-lock-bypass: 需人工切到 Notepad ...
⏭️ T10-popup-self-focus: 需在弹窗 DevTools 内执行 ...

═══ window.focus 测试总结 ═══
  通过: 6  失败: 0  跳过: 4
  通过率: 100.0% (6/6)
```

---

## 运行方式 B：MCP（fb2k_page_evaluate）

仅当需要 AI/脚本化回归时使用。要求：

- MCP Server 启动时设置环境变量 `FB2K_ENABLE_EVAL=1`
- fb2k 运行中，CDP 端口（默认 9222）可达

### 一键执行脚本（PowerShell）

```powershell
$script = Get-Content -Path "tests/manual/test-window-focus.js" -Raw -Encoding UTF8

# 通过已启动的 MCP 客户端调用 fb2k_page_evaluate（伪代码，取决于 MCP 宿主）
# mcp_foo-ui-webvie_fb2k_page_evaluate -Expression $script
```

在支持 MCP 的 AI Agent 里，直接调用：

```
fb2k_page_evaluate({ expression: "<粘贴整个 test-window-focus.js 内容>" })
```

**返回值**即为 IIFE 返回的结果对象（JSON 化），包含 `passed/failed/skipped/cases/timings/version`。

读取保存的上次结果：

```
fb2k_page_evaluate({ expression: "JSON.stringify(window.__fb2kFocusTest, null, 2)" })
```

---

## 手工复现流程

> 自动化脚本无法复现 `ForegroundLockTimeout` 场景，因为 DevTools/CDP 执行时 fb2k 与 JS 都在同进程上下文中。以下流程用来验证 **T2/T5/T7/T10** 这几个需要真实用户行为的关键用例。

### T7 — 前台锁绕过（最重要的回归用例）

**目的**：验证 fb2k 不是前台进程时，从主窗口 focus 弹窗也能立即生效。

1. 启动 foobar2000，打开主窗口。
2. 在主窗口 DevTools（F12）执行：

   ```javascript
   // 准备：创建 1 个大尺寸 popup，放在右侧不碍眼的位置
   const { windowId: P } = await fb2k.invoke('window.createPopup', {
     url: 'about:blank',
     width: 1600, height: 1000,
     x: 100, y: 100,
     title: 'Focus Test T7'
   });
   window.__T7_popup = P;
   console.log('准备好 popup:', P);
   ```

3. 打开一个 **Notepad**（记事本），用 Alt+Tab 切到 Notepad，使 fb2k **完全不在前台**。
4. 保持 Notepad 前台，按 `Win + R` 调出运行框 → 再次确保 fb2k 不是前台。
5. 通过一个预置的定时器回到 fb2k（关键：让浏览器代码在 fb2k **非前台** 时跑起来）：

   > 由于 DevTools 直接粘贴会把前台切回 fb2k，必须用 `setTimeout` 延时。

   在 fb2k 主窗口 DevTools 里 **预先** 粘贴下面代码并回车，然后**立刻** Alt+Tab 到 Notepad：

   ```javascript
   // 延迟 5 秒再发 focus — 期间请 Alt+Tab 到 Notepad
   setTimeout(async () => {
     const t0 = performance.now();
     const r = await fb2k.invoke('window.focus', { windowId: window.__T7_popup });
     const ms = performance.now() - t0;
     console.log(`[T7] focus: success=${r.success}, invoke=${ms.toFixed(1)}ms`);
     // 观测: 弹窗应该立即跳到前台（修复前会延迟 0.2-5s 或只闪烁任务栏）
   }, 5000);
   console.log('5 秒后自动执行 focus — 立刻 Alt+Tab 到 Notepad');
   ```

6. 观测：
   - **修复前（无 AttachThreadInput）**：5 秒后，popup 未立即置顶，任务栏图标闪烁；几秒后才切回。
   - **修复后**：5 秒后，popup 秒切到前台，Notepad 被压到后面。

7. 清理：`await fb2k.invoke('window.closeAllPopups');`

---

### T2 / T10 — 弹窗内部 focus

1. 创建一个带真实页面的弹窗（不是 about:blank）：

   ```javascript
   const { windowId: P } = await fb2k.invoke('window.createPopup', {
     url: 'test.html',  // 或任何已在 resources/dist/ 下的页面
     width: 600, height: 400, title: 'T2/T10 popup'
   });
   ```

2. 在弹窗内打开 DevTools（右键 → Inspect，或 F12）。
3. 在弹窗的 Console 执行：

   ```javascript
   // T2 — 无参数 focus（应聚焦 caller = 本弹窗）
   await fb2k.invoke('window.focus');

   // T10 — 显式传入本窗口 ID
   const id = (await fb2k.invoke('window.getCurrentWindowId')).windowId;
   await fb2k.invoke('window.focus', { windowId: id });
   ```

4. 两次都应返回 `{success: true}`，且弹窗保持前台。

---

### T5 — 最小化后 focus 恢复

1. 在主窗口 DevTools：

   ```javascript
   const { windowId: P } = await fb2k.invoke('window.createPopup', {
     url: 'about:blank', width: 400, height: 300,
     title: 'T5 minimize test'
   });
   window.__T5 = P;
   ```

2. 在弹窗内打开 DevTools，执行 `await fb2k.invoke('window.minimize');`。弹窗应最小化到任务栏。
3. 回到主窗口 DevTools：

   ```javascript
   const r = await fb2k.invoke('window.focus', { windowId: window.__T5 });
   console.log('T5:', r);  // 期望 {success: true}
   ```

4. 弹窗应从最小化恢复并置顶。

---

## 通过/失败判定

**PASS 要求**（全部自动化用例都必须满足）：

- T1/T3/T9：`success=true` 且 `invoke` 延迟 < 500ms
- T4：`success=false` 且 error 含 "not found"
- T6：n=20 采样，p95 < 200ms，max < 1000ms
- T8：n=9 采样，max < 500ms
- 手工用例 T2/T5/T7/T10：按观测判断（T7 最关键）

**FAIL 信号**：

- 任何用例 `success=false` 或抛异常
- T6 p95 超过 200ms：提示 UI 线程被阻塞或存在死锁
- T6 max 超过 1000ms：提示前台锁逻辑可能回归

## 回归负责人

- **提出人**：用户反馈（2026-04）
- **修复者**：NereaFantasia
- **测试脚本**：`tests/manual/test-window-focus.js`
- **C++ 实现**：`src/api/WindowApi.cpp:905-949`
