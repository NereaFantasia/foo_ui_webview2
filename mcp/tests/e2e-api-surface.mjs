/**
 * Reconciles the static C++ registration surface against the live runtime
 * registration table of a running foobar2000.
 *
 * Static registration does not imply runtime reachability: a whole API file can
 * be missing from the build, or its Register*Api() call can be missing from
 * WebViewPanel::RegisterAllApis(). Source grep and graph extraction find
 * registration sites, but do not establish that the loaded DLL includes and
 * executes them.
 *
 * The oracle is independent of the source scan: system.listAvailableApis is
 * served from BridgeCore::GetRegisteredApiNames(), which unions the synchronous
 * and deferred handler tables (src/api/BridgeCore.cpp), so it reports what the
 * loaded DLL actually dispatches.
 *
 * Read-only: every call in this script is a query. No fixtures, no writes.
 *
 * Usage: node mcp/tests/e2e-api-surface.mjs   (needs fb2k running with
 * --remote-debugging-port, see FB2K_CDP_PORT)
 *        node mcp/tests/e2e-api-surface.mjs --static-only   (source scan only,
 * prints the surface totals without touching a running instance)
 */

import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import {
    blockedError,
    closeClient,
    connectBridgePage,
    createBridge,
    createRecorder,
    envInt,
    report,
    requireResponsiveBridge,
} from "./lib/e2e-harness.mjs";

const API_DIR = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "src", "api");

/**
 * Same reading as mcp/tests/api-existence-gate.test.ts: the scan runs over whole
 * file text so a registration whose literal sits on the next line still counts,
 * and RegisterApiDeferred joins the same surface. The lookbehind keeps
 * UnregisterApi out.
 */
const REGISTER_API_RE =
    /(?<![A-Za-z0-9_])Register(?:Undeclared)?Api(?:Deferred)?\s*\(\s*"([^"]+)"/g;

const DIFF_SAMPLE_LIMIT = 25;

function sample(names) {
    return names.length > DIFF_SAMPLE_LIMIT
        ? [...names.slice(0, DIFF_SAMPLE_LIMIT), `... ${names.length - DIFF_SAMPLE_LIMIT} more`]
        : names;
}

function countByNamespace(names) {
    const counts = {};
    for (const name of names) {
        const ns = name.split(".")[0];
        counts[ns] = (counts[ns] ?? 0) + 1;
    }
    return counts;
}

function diffCounts(left, right) {
    const differences = {};
    for (const ns of new Set([...Object.keys(left), ...Object.keys(right)])) {
        if ((left[ns] ?? 0) !== (right[ns] ?? 0)) {
            differences[ns] = { static: left[ns] ?? 0, runtime: right[ns] ?? 0 };
        }
    }
    return differences;
}

function scanStaticSurface() {
    if (!existsSync(API_DIR)) {
        throw blockedError(`src/api not found at ${API_DIR}; run this from a repository checkout`);
    }

    const names = new Set();
    let sites = 0;
    for (const file of readdirSync(API_DIR).filter((name) => name.endsWith(".cpp"))) {
        const content = readFileSync(join(API_DIR, file), "utf-8");
        REGISTER_API_RE.lastIndex = 0;
        let match;
        while ((match = REGISTER_API_RE.exec(content)) !== null) {
            names.add(match[1]);
            sites += 1;
        }
    }
    return { names: [...names].sort(), sites };
}

async function run(bridge, recorder, staticSurface) {
    await requireResponsiveBridge(bridge);

    // Without this, a regex that matched nothing would make every later case
    // pass on an empty set.
    recorder.assertCase(
        "API-01 static scan of src/api yields a plausible surface",
        staticSurface.names.length >= 100 && staticSurface.sites >= staticSurface.names.length,
        { endpoints: staticSurface.names.length, sites: staticSurface.sites },
        { endpointsAtLeast: 100, sitesAtLeastEndpoints: true },
    );

    const runtimeList = await bridge.invoke("system.listAvailableApis", {
        includeInternal: true,
        includeExternal: true,
    });

    if (runtimeList?.success !== true || !Array.isArray(runtimeList.apis)) {
        throw new Error(
            `system.listAvailableApis returned ${JSON.stringify(runtimeList)?.slice(0, 200)}, expected { success: true, apis: [] }`,
        );
    }
    const runtimeApis = runtimeList.apis;

    const runtimeInternal = [];
    const runtimeExternal = [];
    for (const entry of runtimeApis) {
        if (typeof entry?.fullName !== "string") continue;
        (entry.isExternal === true ? runtimeExternal : runtimeInternal).push(entry.fullName);
    }
    runtimeInternal.sort();
    runtimeExternal.sort();

    const runtimeSet = new Set([...runtimeInternal, ...runtimeExternal]);
    const staticSet = new Set(staticSurface.names);

    const unreachable = staticSurface.names.filter((name) => !runtimeSet.has(name));
    recorder.assertCase(
        "API-02 every statically registered endpoint is reachable at runtime",
        unreachable.length === 0,
        { unreachableCount: unreachable.length, unreachable: sample(unreachable) },
        { unreachableCount: 0 },
    );

    const unsourced = runtimeInternal.filter((name) => !staticSet.has(name));
    recorder.assertCase(
        "API-03 every built-in runtime endpoint has a static registration site",
        unsourced.length === 0,
        { unsourcedCount: unsourced.length, unsourced: sample(unsourced) },
        { unsourcedCount: 0 },
    );

    const staticCounts = countByNamespace(staticSurface.names);
    const runtimeCounts = countByNamespace(runtimeInternal);
    const countDifferences = diffCounts(staticCounts, runtimeCounts);
    recorder.assertCase(
        "API-04 per-namespace endpoint counts agree",
        Object.keys(countDifferences).length === 0,
        {
            staticNamespaces: Object.keys(staticCounts).length,
            runtimeNamespaces: Object.keys(runtimeCounts).length,
            differences: countDifferences,
        },
        { differences: {} },
    );

    const stats = await bridge.invoke("system.getApiStats", {});
    recorder.assertCase(
        "API-05 system.getApiStats agrees with system.listAvailableApis",
        stats?.totalApis === runtimeApis.length &&
            stats?.internalApis === runtimeInternal.length &&
            stats?.externalApis === runtimeExternal.length,
        {
            stats: {
                totalApis: stats?.totalApis,
                internalApis: stats?.internalApis,
                externalApis: stats?.externalApis,
            },
            listed: {
                totalApis: runtimeApis.length,
                internalApis: runtimeInternal.length,
                externalApis: runtimeExternal.length,
            },
        },
        { equal: true },
    );

    return {
        staticEndpoints: staticSurface.names.length,
        staticSites: staticSurface.sites,
        staticNamespaces: Object.keys(staticCounts).length,
        runtimeTotal: runtimeApis.length,
        runtimeInternal: runtimeInternal.length,
        runtimeExternal: runtimeExternal.length,
        externalEndpoints: sample(runtimeExternal),
    };
}

const recorder = createRecorder();
let client;
let blocked = false;
let fatalError;
let detail = {};

try {
    const staticSurface = scanStaticSurface();

    if (process.argv.includes("--static-only")) {
        const counts = countByNamespace(staticSurface.names);
        console.log(
            JSON.stringify(
                {
                    mode: "static-only",
                    staticEndpoints: staticSurface.names.length,
                    staticSites: staticSurface.sites,
                    staticNamespaces: Object.keys(counts).length,
                    byNamespace: counts,
                },
                null,
                2,
            ),
        );
        process.exit(0);
    }

    const connection = await connectBridgePage();
    client = connection.client;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    detail.targets = connection.candidates;
    const bridge = createBridge(client.Runtime, {
        invokeTimeoutMs: envInt("FB2K_E2E_TIMEOUT_MS", 5000),
    });
    detail = { ...detail, ...(await run(bridge, recorder, staticSurface)) };
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    await closeClient(client);
}

process.exit(
    report({
        recorder,
        blocked,
        fatalError,
        extra: { targetPort: envInt("FB2K_CDP_PORT", 9222), ...detail },
    }),
);
