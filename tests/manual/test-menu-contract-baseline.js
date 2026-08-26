/**
 * 菜单子系统 contract 基线探测脚本 (L4)
 *
 * 关联设计: docs/menu-subsystem/SPEC.md §2.3 (问题基线) / §9.2 (测试分层 L4)
 *
 * 目的:
 *   把 SPEC §2.3 的一次性 CDP 探测固化为可重复执行的基线脚本。重构前后各跑一次,
 *   对比输出即可判定 D1-D17 是否真的被消除, 而不是凭代码审查断言。
 *
 * 为什么必须是 L4 (需真实宿主):
 *   mainmenu_manager_v2 在中文汉化版 foobar2000 上抛 "找不到命令", 第三方插件
 *   (ESLyric 等) 的注册形态也只在真实宿主里成立。这些都无法用 L1-L3 证明。
 *
 * 覆盖项:
 *   T1 宿主与插件版本                — 记录环境, 否则跨次对比无意义
 *   T2 菜单层级来源 (v2 / v1 / flat) — SPEC §2.3 核心发现
 *   T3 叶子节点字段覆盖率            — D1 / D6 (状态与 guid 是否缺失)
 *   T4 寻址矩阵                      — D6 / D7 / D8 (哪种形式真能执行)
 *   T5 枚举 vs 真实菜单偏差          — D1 / D11 (幽灵条目、隐藏命令)
 *   T6 右键菜单枚举字段覆盖率        — D2 / D3
 *   T7 searchCommands 覆盖与折叠     — D14
 *   T8 getAllServices 汇总一致性     — D15
 *   T9 深度/截断标记                 — D12 / D13
 *
 * 运行方式:
 *   A) DevTools 控制台
 *      1. foobar2000 主窗口按 F12 打开 DevTools, 切到 Console
 *      2. 粘贴本文件全部内容并回车
 *      3. 结果存于 window.__fb2kMenuBaseline; 控制台打印摘要
 *
 *   B) MCP fb2k_evaluate (需设置 FB2K_ENABLE_EVAL=1)
 *      1. 读取本文件内容为字符串
 *      2. fb2k_evaluate({ expression: <文件内容> }) — IIFE 直接返回结果对象
 *      3. 复读: fb2k_evaluate({ expression: "JSON.stringify(window.__fb2kMenuBaseline)" })
 *
 * 安全性:
 *   - 默认 **只读**。T4 需要真正执行命令才能判定寻址是否可用, 因此默认跳过。
 *   - 如需 T4, 传 RUN_EXEC=true。届时只会执行"总在最上面 / Always on top"这一条
 *     可逆开关, 并在每次成功后立即再执行一次复原。
 *   - 绝不执行动态父槽位: SDK 对 mainmenu_commands_v2 动态槽位的 execute() 是
 *     未定义行为, 实测可能 uBugCheck 直接终止 foobar2000。
 *   - 绝不执行会弹模态框的命令 (如 "打开...")。
 */

(async () => {
    'use strict';

    // 置 true 才跑 T4 寻址矩阵 (会执行一条可逆的置顶开关并立即复原)
    const RUN_EXEC = false;

    const out = {
        version: 'menu-baseline-v1',
        spec: 'docs/menu-subsystem/SPEC.md §2.3',
        startedAt: new Date().toISOString(),
        runExec: RUN_EXEC,
        env: {},
        findings: {},
        defects: {},
        errors: [],
    };

    const invoke = async (method, params) => {
        try {
            return await window.fb2k.invoke(method, params);
        } catch (e) {
            out.errors.push({ method, error: String((e && e.message) || e) });
            return { __error: String((e && e.message) || e) };
        }
    };

    const walk = (items, prefix, sink) => {
        for (const it of items || []) {
            const path = prefix ? `${prefix}/${it.label}` : it.label;
            if (it.type === 'command') sink.push({ ...it, _path: path });
            walk(it.items || it.children, path, sink);
        }
        return sink;
    };

    const pct = (n, d) => (d === 0 ? 'n/a' : `${Math.round((n / d) * 100)}%`);

    // ---- T1 环境 ----------------------------------------------------------
    const ver = await invoke('config.getVersionInfo');
    out.env = {
        foobar2000: ver && ver.foobar2000,
        plugin: ver && ver.plugin && ver.plugin.version,
        isPortable: ver && ver.isPortable,
    };

    // ---- T2 菜单层级来源 --------------------------------------------------
    const mm = await invoke('menu.getMainMenu', { withAvailability: true });
    const leaves = walk(mm.items, mm.root || '', []);
    out.findings.T2_tier = {
        source: mm.source ?? null,
        fallback: mm.fallback ?? null,
        rootMatched: mm.rootMatched,
        // SPEC §2.3: 汉化版实测为 v1-hmenu; 若此处变成 v2 说明宿主环境不同,
        // 后续所有对比都要重新建立基线。
        note: mm.source === 'v1-hmenu'
            ? 'v2 unavailable — matches SPEC baseline'
            : 'tier differs from SPEC baseline; re-baseline before comparing',
    };

    // ---- T3 叶子字段覆盖率 (D1 / D6) --------------------------------------
    const withField = (f) => leaves.filter((l) => l[f] !== undefined).length;
    out.findings.T3_leafFields = {
        total: leaves.length,
        withGuid: withField('guid'),
        withFlags: withField('flags'),
        withChecked: withField('checked'),
        withAvailable: withField('available'),
        withEnabled: withField('enabled'),   // 重构后应出现
        withHidden: withField('hidden'),     // 重构后应出现
        withAddress: withField('address'),   // 重构后应出现
        guidCoverage: pct(withField('guid'), leaves.length),
    };
    out.defects.D6_v1LeavesUnaddressable = withField('guid') === 0 && leaves.length > 0;

    // ---- T4 寻址矩阵 (D6 / D7 / D8) --------------------------------------
    // 只用一条可逆开关。找不到就跳过, 绝不退化到别的命令。
    const disc = await invoke('discovery.getMainMenuCommands', { expandDynamic: false });
    const discCmds = (disc && disc.commands) || [];
    const toggle = discCmds.find((c) => /最上面|always on top/i.test(c.name || ''));

    if (!RUN_EXEC) {
        out.findings.T4_addressing = { skipped: 'set RUN_EXEC=true to run (executes one reversible toggle)' };
    } else if (!toggle) {
        out.findings.T4_addressing = { skipped: 'always-on-top command not found; refusing to substitute another command' };
    } else {
        const leaf = leaves.find((l) => l.label === toggle.name);
        const forms = [
            ['guid', toggle.guid],
            ['exact_get_name', toggle.name],
            ['path', leaf ? leaf._path : null],
            ['displayPath', leaf ? leaf.displayPath : null],
            ['numeric_commandId', leaf ? leaf.commandId : null],
        ];
        const matrix = {};
        for (const [form, value] of forms) {
            if (value === null || value === undefined) { matrix[form] = 'not-emitted'; continue; }
            const r = await invoke('menu.runMainMenuCommand', { command: value });
            const ok = !!(r && r.success);
            matrix[form] = ok ? 'ok' : (r && r.__error) || 'failed';
            if (ok) await invoke('menu.runMainMenuCommand', { command: value }); // 立即复原
        }
        out.findings.T4_addressing = matrix;
        out.defects.D7_pathFormUnusable = matrix.path !== 'ok';
        out.defects.D8_nameFormUnusable = matrix.exact_get_name !== 'ok';
        out.defects.D17_numericCommandIdTypeError =
            typeof matrix.numeric_commandId === 'string' &&
            matrix.numeric_commandId.includes('type_error');
    }

    // ---- T5 枚举 vs 真实菜单 (D1 / D11) ----------------------------------
    const discFull = await invoke('discovery.getMainMenuCommands', { expandDynamic: true });
    const full = (discFull && discFull.commands) || [];
    const realLabels = new Set(leaves.map((l) => l.label));
    const notInRealMenu = full.filter((c) => !c.isDynamicParent && !realLabels.has(c.name));
    const emptyNamed = full.filter((c) => !c.name);
    const dupNames = {};
    for (const c of full) { if (c.name) dupNames[c.name] = (dupNames[c.name] || 0) + 1; }

    out.findings.T5_enumerationDelta = {
        realMenuCommands: leaves.length,
        realMenuDisabled: leaves.filter((l) => l.available === false).length,
        discoveryEntries: discFull.count,
        discoveryDynamic: discFull.dynamicCount,
        staticEntriesWithFlags: full.filter((c) => !c.isDynamic && c.flags !== undefined).length,
        notInRealMenuCount: notInRealMenu.length,
        notInRealMenuSample: notInRealMenu.slice(0, 10).map((c) => c.name),
        emptyNamedCount: emptyNamed.length,
        duplicateLabels: Object.entries(dupNames).filter(([, k]) => k > 1)
            .map(([n, k]) => `${n} x${k}`),
    };
    out.defects.D1_staticEntriesLackState =
        full.filter((c) => !c.isDynamic).length > 0 &&
        full.filter((c) => !c.isDynamic && c.flags !== undefined).length === 0;
    out.defects.D11_phantomEmptyNode = emptyNamed.length > 0;

    // ---- T6 右键菜单枚举 (D2 / D3) ---------------------------------------
    const cmc = await invoke('discovery.getContextMenuCommands');
    const cmcCmds = (cmc && cmc.commands) || [];
    out.findings.T6_contextMenu = {
        count: cmc && cmc.count,
        entryKeys: cmcCmds.length ? Object.keys(cmcCmds[0]) : [],
        withFlags: cmcCmds.filter((c) => c.flags !== undefined).length,
        withAvailable: cmcCmds.filter((c) => c.available !== undefined).length,
        withEnabled: cmcCmds.filter((c) => c.enabled !== undefined).length,  // 重构后应出现
        withPath: cmcCmds.filter((c) => c.path !== undefined).length,
        withSubGuid: cmcCmds.filter((c) => c.subGuid !== undefined).length,  // 重构后应出现
    };
    out.defects.D2_contextLacksState =
        cmcCmds.length > 0 && cmcCmds.filter((c) => c.flags !== undefined).length === 0;
    out.defects.D3_contextDynamicNotExpanded =
        cmcCmds.length > 0 && cmcCmds.filter((c) => c.subGuid !== undefined).length === 0;

    // ---- T7 searchCommands (D14) -----------------------------------------
    // 注意: ASCII 大小写折叠实测是正常的 (lyric / LYRIC / Lyric 命中数相同),
    // CJK 本身无大小写可折叠。D14 的真实缺陷是**覆盖范围**只有主菜单, 以及
    // ::tolower 对负值 char 属 UB (静态缺陷, 无法从响应观测)。
    const sLower = await invoke('discovery.searchCommands', { query: 'lyric' });
    const sUpper = await invoke('discovery.searchCommands', { query: 'LYRIC' });
    const sCjk = await invoke('discovery.searchCommands', { query: '歌词' });
    const nOf = (r) => (r && (r.count ?? ((r.results || r.commands || []).length))) || 0;
    const types = new Set(((sCjk && sCjk.results) || []).map((r) => r.type));
    out.findings.T7_search = {
        lower: nOf(sLower),
        upper: nOf(sUpper),
        cjk: nOf(sCjk),
        asciiCaseFoldingWorks: nOf(sLower) === nOf(sUpper) && nOf(sLower) > 0,
        typesSeen: [...types],
        coversContextMenu: types.has('contextmenu'),  // 重构后应为 true
    };
    out.defects.D14_searchMainMenuOnly = !types.has('contextmenu');

    // ---- T8 getAllServices 汇总 (D15) ------------------------------------
    const all = await invoke('discovery.getAllServices');
    const svc = (all && all.services) || {};
    out.findings.T8_summary = {
        keys: Object.keys(svc),
        mainMenuCommands: svc.mainMenuCommands,
        contextMenuCommands: svc.contextMenuCommands,  // 重构后应出现
        totalServices: all && all.totalServices,
        listingContextCount: cmc && cmc.count,
    };
    out.defects.D15_summaryMissingContextMenu = svc.contextMenuCommands === undefined;

    // ---- T9 截断标记 (D12 / D13) -----------------------------------------
    // getContextMenuTree 需要有选中曲目或正在播放, 否则返回错误信封。
    // 必须显式区分"未测"与"已测且无缺陷", 否则会得出假阴性结论。
    const tree = await invoke('discovery.getContextMenuTree');
    if (!tree || tree.success === false || tree.__error) {
        out.findings.T9_truncation = {
            skipped: (tree && (tree.error || tree.__error)) || 'unavailable',
            hint: 'select a track (or start playback) in foobar2000, then re-run',
        };
        out.defects.D12_silentTruncation = null;  // null = 未测, 不是"无缺陷"
    } else {
        const root = tree.tree ?? tree.root ?? tree.nodes ?? tree;
        const scan = (node, acc) => {
            if (!node || typeof node !== 'object') return acc;
            if (Array.isArray(node)) { for (const n of node) scan(n, acc); return acc; }
            acc.nodes++;
            if (node.truncated !== undefined) acc.withTruncatedFlag++;
            const kids = node.children || [];
            if (node.childCount !== undefined && node.childCount !== kids.length) {
                acc.childCountMismatch++;
            }
            for (const k of kids) scan(k, acc);
            return acc;
        };
        const acc = scan(root, { nodes: 0, withTruncatedFlag: 0, childCountMismatch: 0 });
        out.findings.T9_truncation = acc;
        // 只有真的走过树才能判定
        out.defects.D12_silentTruncation =
            acc.nodes > 1 && acc.childCountMismatch > 0 && acc.withTruncatedFlag === 0;
    }

    // ---- 汇总 --------------------------------------------------------------
    out.finishedAt = new Date().toISOString();
    const present = Object.entries(out.defects).filter(([, v]) => v === true).map(([k]) => k);
    const untested = Object.entries(out.defects).filter(([, v]) => v === null).map(([k]) => k);
    out.summary = {
        defectsStillPresent: present,
        defectsStillPresentCount: present.length,
        defectsNotTested: untested,
        errorCount: out.errors.length,
    };

    window.__fb2kMenuBaseline = out;

    console.log('=== menu contract baseline ===');
    console.log(`env: ${out.env.foobar2000} / plugin ${out.env.plugin}`);
    console.log(`tier: ${out.findings.T2_tier.source} — ${out.findings.T2_tier.note}`);
    console.log(`defects still present (${present.length}): ${present.join(', ') || 'none'}`);
    if (out.errors.length) console.warn('errors:', out.errors);
    console.log('full result -> window.__fb2kMenuBaseline');

    return out;
})();
