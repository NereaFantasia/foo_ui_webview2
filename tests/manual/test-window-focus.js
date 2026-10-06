/**
 * window.focus 自动化回归脚本
 *
 * 关联提交: c216a59  — fix: window.focus 绕过前台锁超时，AttachThreadInput 秒切弹窗
 *
 * 覆盖目标:
 *   1. 功能正确性 — 从主窗口 focus(popupId) 稳定返回 success，且 invoke 调用不报错
 *   2. 延迟稳定性 — 快速连续调用 20 次，p50/p95/p99/max 均在合理阈值内
 *   3. 边界处理   — focus 不存在的 windowId 必须返回 success=false
 *   4. 多窗口切换 — 3 个 popup 轮询 focus，max 延迟必须 < 500ms
 *   5. 回到主窗口 — focus({windowId: "main"}) 可用
 *
 * 运行方式:
 *   A) DevTools 控制台
 *      1. 在 foobar2000 主窗口按 F12 打开 DevTools
 *      2. 切到 Console
 *      3. 粘贴本文件全部内容并回车
 *      4. 查看控制台输出；完整结果存于 window.__fb2kFocusTest
 *
 *   B) MCP fb2k_page_evaluate（需启动 MCP 服务时设置 FB2K_ENABLE_EVAL=1）
 *      1. 读取本文件内容为字符串
 *      2. 调用 fb2k_page_evaluate({ expression: <本文件内容> })
 *      3. 返回值即为测试结果对象 (IIFE 返回 out)
 *      4. 另行调用 fb2k_page_evaluate({ expression: "JSON.stringify(window.__fb2kFocusTest)" })
 *         可重复读取上次结果
 *
 * 注意:
 *   - 脚本本身只能验证"功能 + 进程内延迟"；要复现 Windows ForegroundLockTimeout
 *     导致的前台锁场景，需按 test-window-focus.md 的"手工复现流程"操作。
 *   - 运行时会创建/销毁多个 popup，会短暂抢占前台。请在空闲状态下执行。
 */

(async () => {
    'use strict';

    const out = {
        version: 'c216a59-regression-v1',
        startedAt: new Date().toISOString(),
        cases: [],
        timings: [],
        passed: 0,
        failed: 0,
        skipped: 0,
    };

    const log = (name, status, detail, extra) => {
        const entry = { name, status, detail };
        if (extra) Object.assign(entry, extra);
        out.cases.push(entry);
        if (status === 'pass') out.passed++;
        else if (status === 'fail') out.failed++;
        else if (status === 'skip') out.skipped++;
        const icon = status === 'pass' ? '\u2705' : status === 'fail' ? '\u274C' : '\u23ED\uFE0F';
        console.log(`${icon} ${name}: ${detail}`);
    };

    const hasBridge = typeof fb2k === 'object' && fb2k !== null && typeof fb2k.invoke === 'function';
    if (!hasBridge) {
        log('preflight', 'fail', 'fb2k.invoke 不可用（必须在 foobar2000 主窗口 WebView2 DevTools 或 MCP 中执行）');
        window.__fb2kFocusTest = out;
        return out;
    }

    const invoke = (method, params) => fb2k.invoke(method, params || {});

    const timedInvoke = async (method, params) => {
        const t0 = performance.now();
        const result = await invoke(method, params);
        const ms = performance.now() - t0;
        return { result, ms };
    };

    const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

    const cleanup = async () => {
        try { await invoke('window.closeAllPopups'); } catch (_) { /* ignore */ }
    };

    // 预清理
    await cleanup();
    await sleep(150);

    let popupSmall = null;
    let popupLarge = null;

    // ────────────────────────────────────────────────────
    // T1: 基础流程 — 创建小弹窗并从主窗口 focus
    // ────────────────────────────────────────────────────
    try {
        const r = await invoke('window.createPopup', {
            url: 'about:blank',
            width: 400, height: 300,
            title: 'FocusTest T1 (small)',
        });
        if (!r || !r.success) throw new Error((r && r.error) || 'createPopup failed');
        popupSmall = r.windowId;
        await sleep(250); // 等待 WebView2 首帧，避免干扰后续计时

        const focusRes = await timedInvoke('window.focus', { windowId: popupSmall });
        out.timings.push({ case: 'T1', windowId: popupSmall, ms: focusRes.ms });
        if (focusRes.result && focusRes.result.success) {
            log('T1-basic-focus', 'pass',
                `focus(${popupSmall}) ok in ${focusRes.ms.toFixed(1)}ms`);
        } else {
            log('T1-basic-focus', 'fail',
                (focusRes.result && focusRes.result.error) || 'unknown error');
        }
    } catch (e) {
        log('T1-basic-focus', 'fail', e && e.message ? e.message : String(e));
    }

    // ────────────────────────────────────────────────────
    // T3: 大尺寸弹窗 — 验证 SetForegroundWindow 不因窗口大小退化
    // ────────────────────────────────────────────────────
    try {
        const r = await invoke('window.createPopup', {
            url: 'about:blank',
            width: 1600, height: 1000,
            title: 'FocusTest T3 (large 1600x1000)',
        });
        if (!r || !r.success) throw new Error((r && r.error) || 'createPopup failed');
        popupLarge = r.windowId;
        await sleep(500); // 大窗口首帧更慢

        const focusRes = await timedInvoke('window.focus', { windowId: popupLarge });
        out.timings.push({ case: 'T3', windowId: popupLarge, ms: focusRes.ms });
        const threshold = 500;
        const pass = !!(focusRes.result && focusRes.result.success) && focusRes.ms < threshold;
        log('T3-large-popup', pass ? 'pass' : 'fail',
            `large focus in ${focusRes.ms.toFixed(1)}ms (阈值 < ${threshold}ms)`);
    } catch (e) {
        log('T3-large-popup', 'fail', e && e.message ? e.message : String(e));
    }

    // ────────────────────────────────────────────────────
    // T4: 不存在的 windowId — 必须 success=false + 明确错误
    // ────────────────────────────────────────────────────
    try {
        const r = await invoke('window.focus', { windowId: 'popup_does_not_exist_xyz' });
        const looksCorrect = r && r.success === false && /not found/i.test(r.error || '');
        log('T4-invalid-id', looksCorrect ? 'pass' : 'fail',
            looksCorrect
                ? `正确拒绝: ${r.error}`
                : `异常返回: ${JSON.stringify(r)}`);
    } catch (e) {
        log('T4-invalid-id', 'fail', e && e.message ? e.message : String(e));
    }

    // ────────────────────────────────────────────────────
    // T6: 快速连续 focus — 稳定性 + 延迟直方图
    //     注: 首次调用可能跨进程（DevTools 前台），后续同进程；
    //     AttachThreadInput 修复应保证所有采样均 < 200ms。
    // ────────────────────────────────────────────────────
    try {
        const targets = [popupSmall, popupLarge].filter(Boolean);
        if (targets.length === 0) throw new Error('前置用例未创建任何 popup');

        const N = 20;
        const samples = [];
        for (let i = 0; i < N; i++) {
            const target = targets[i % targets.length];
            const r = await timedInvoke('window.focus', { windowId: target });
            if (!r.result || !r.result.success) {
                throw new Error(`iter ${i}: ${(r.result && r.result.error) || 'no success'}`);
            }
            samples.push(r.ms);
            out.timings.push({ case: 'T6', windowId: target, iter: i, ms: r.ms });
            await sleep(15); // 给 UI 线程一个消息泵 tick
        }
        samples.sort((a, b) => a - b);
        const pick = (p) => samples[Math.min(samples.length - 1, Math.floor(samples.length * p))];
        const p50 = pick(0.5), p95 = pick(0.95), p99 = pick(0.99);
        const max = samples[samples.length - 1];
        const mean = samples.reduce((a, b) => a + b, 0) / samples.length;
        // 阈值：p95 < 200ms（进程内 fast path），所有采样 < 1000ms
        const pass = samples.every((ms) => ms < 1000) && p95 < 200;
        log('T6-rapid-succession', pass ? 'pass' : 'fail',
            `n=${N} mean=${mean.toFixed(1)} p50=${p50.toFixed(1)} p95=${p95.toFixed(1)} p99=${p99.toFixed(1)} max=${max.toFixed(1)} (ms)`,
            { n: N, mean, p50, p95, p99, max });
    } catch (e) {
        log('T6-rapid-succession', 'fail', e && e.message ? e.message : String(e));
    }

    // ────────────────────────────────────────────────────
    // T8: 多弹窗轮询切换 — 3 个 popup 共 9 次切换
    // ────────────────────────────────────────────────────
    const t8Popups = [];
    try {
        for (let i = 0; i < 3; i++) {
            const r = await invoke('window.createPopup', {
                url: 'about:blank',
                width: 320, height: 240,
                title: `FocusTest T8-${i}`,
            });
            if (r && r.success) t8Popups.push(r.windowId);
        }
        if (t8Popups.length !== 3) throw new Error(`仅创建 ${t8Popups.length}/3 个 popup`);
        await sleep(400);

        const cycleSamples = [];
        for (let i = 0; i < 9; i++) {
            const target = t8Popups[i % t8Popups.length];
            const r = await timedInvoke('window.focus', { windowId: target });
            if (!r.result || !r.result.success) {
                throw new Error(`cycle ${i}: ${(r.result && r.result.error) || 'no success'}`);
            }
            cycleSamples.push(r.ms);
            out.timings.push({ case: 'T8', windowId: target, iter: i, ms: r.ms });
            await sleep(60);
        }
        const maxCycle = Math.max.apply(null, cycleSamples);
        const pass = maxCycle < 500;
        log('T8-multi-cycle', pass ? 'pass' : 'fail',
            `n=${cycleSamples.length} max=${maxCycle.toFixed(1)}ms (阈值 < 500ms)`);
    } catch (e) {
        log('T8-multi-cycle', 'fail', e && e.message ? e.message : String(e));
    }

    // ────────────────────────────────────────────────────
    // T9: focus("main") — 回到主窗口
    // ────────────────────────────────────────────────────
    try {
        const r = await timedInvoke('window.focus', { windowId: 'main' });
        out.timings.push({ case: 'T9', windowId: 'main', ms: r.ms });
        const pass = !!(r.result && r.result.success);
        log('T9-focus-main', pass ? 'pass' : 'fail',
            pass
                ? `focus(main) ok in ${r.ms.toFixed(1)}ms`
                : (r.result && r.result.error) || 'unknown');
    } catch (e) {
        log('T9-focus-main', 'fail', e && e.message ? e.message : String(e));
    }

    // ────────────────────────────────────────────────────
    // T5: 最小化后 focus 恢复 — 标记为 skip（需人工）
    //     window.minimize 基于 caller hwnd，不接受 windowId，
    //     无法从主窗口程序化最小化指定 popup，统一放入手工流程。
    // ────────────────────────────────────────────────────
    log('T5-minimized-restore', 'skip',
        '程序化不可行（window.minimize 仅对 caller 生效），见 test-window-focus.md 手工流程');

    // ────────────────────────────────────────────────────
    // T2, T7, T10: 跨页面/跨进程场景 — 标记为 skip
    // ────────────────────────────────────────────────────
    log('T2-self-focus', 'skip',
        '需在弹窗页面内执行 fb2k.invoke("window.focus") — 见 test-window-focus.md');
    log('T7-foreground-lock-bypass', 'skip',
        'Windows ForegroundLockTimeout 复现需人工切到 Notepad — 见 test-window-focus.md');
    log('T10-popup-self-focus', 'skip',
        '需在弹窗 DevTools 内执行 — 见 test-window-focus.md');

    // 清理
    await cleanup();

    // ────────────────────────────────────────────────────
    // Summary
    // ────────────────────────────────────────────────────
    out.finishedAt = new Date().toISOString();
    const total = out.passed + out.failed;
    const pct = total ? ((out.passed / total) * 100).toFixed(1) : '0';
    console.log('\n\u2550\u2550\u2550 window.focus \u6d4b\u8bd5\u603b\u7ed3 \u2550\u2550\u2550');
    console.log(`  \u901a\u8fc7: ${out.passed}  \u5931\u8d25: ${out.failed}  \u8df3\u8fc7: ${out.skipped}`);
    console.log(`  \u5f00\u59cb: ${out.startedAt}`);
    console.log(`  \u7ed3\u675f: ${out.finishedAt}`);
    console.log(`  \u901a\u8fc7\u7387: ${pct}% (${out.passed}/${total})`);
    if (out.failed > 0) {
        console.warn('\u26A0\uFE0F \u6709\u5931\u8d25\u7528\u4f8b\uff0c\u67e5\u770b\u4e0a\u65b9\u8be6\u7ec6\u8f93\u51fa');
    }
    console.log('\u5b8c\u6574\u7ed3\u679c: window.__fb2kFocusTest');

    window.__fb2kFocusTest = out;
    return out;
})();
