/** 只读 API 的容器、便携路径与落点拒绝回归；媒体内容来自隔离的 FLAC 副本。 */
import { readFileSync, writeFileSync, symlinkSync, unlinkSync } from "node:fs";
import { dirname, join, resolve, win32 } from "node:path";
import { blockedError, closeClient, connectBridgePage, createBridge, createRecorder, report } from "./lib/e2e-harness.mjs";
import { resolveFixtureArea, waitForLibraryEntry } from "./lib/fixture-area.mjs";
import { storedZip, unpackPath } from "./lib/archive-fixture.mjs";
import { parseFlac } from "./lib/flac-fixture.mjs";

const recorder = createRecorder();
const runId = `media-read-paths-${Date.now()}`;
let client, bridge, area, junction, fatalError;
let blocked = false;
let context;

function withoutPath(rows) {
    return rows?.map(({ path, ...row }) => row);
}

try {
    ({ client } = await connectBridgePage());
    bridge = createBridge(client.Runtime, { invokeTimeoutMs: 10000 });
    const version = await bridge.invoke("config.getVersionInfo", {});
    if (!version.isPortable || !version.profilePath) throw new Error("需要便携实例的 profile 路径");
    const instance = dirname(version.profilePath);
    const resolved = await resolveFixtureArea(bridge, { runId });
    area = resolved.area;
    context = { profilePath: version.profilePath, fixtureDir: area.dir };
    const portable = (path) => {
        const relative = win32.relative(instance, path);
        if (win32.isAbsolute(relative)) throw new Error("夹具与便携实例必须位于同一卷");
        return `file-relative://${relative}`;
    };
    const firstTitle = `${runId}-first`;
    const secondTitle = `${runId}-second`;
    const first = area.taggedAudio("first.flac", [["TITLE", firstTitle], ["ARTIST", runId]]).targetPath;
    const second = area.taggedAudio("second.flac", [["TITLE", secondTitle], ["ARTIST", runId]]).targetPath;
    const blocks = parseFlac(readFileSync(first)).blocks;
    if (blocks.some((block) => block.type === 5)) throw blockedError("需要没有 CUESHEET 块的单轨 FLAC 作为源文件");
    const streamInfo = blocks.find((block) => block.type === 0).payload;
    const packed = streamInfo.readBigUInt64BE(10);
    const seconds = Number(packed & ((1n << 36n) - 1n)) / Number(packed >> 44n);
    const frame = Math.floor(seconds * 75 / 2);
    if (frame < 1) throw blockedError("源音频太短，无法分成两个 CUE 子曲目");
    const position = [Math.floor(frame / 4500), Math.floor(frame / 75) % 60, frame % 75]
        .map((part) => String(part).padStart(2, "0")).join(":");
    const cueText = `PERFORMER "${runId}"\nTITLE "chapters"\nFILE "cue.flac" WAVE\n` +
        `  TRACK 01 AUDIO\n    TITLE "first chapter"\n    INDEX 01 00:00:00\n` +
        `  TRACK 02 AUDIO\n    TITLE "second chapter"\n    INDEX 01 ${position}\n`;
    const cue = area.taggedAudio("cue.flac", [["TITLE", runId], ["CUESHEET", cueText]]).targetPath;
    const zip = storedZip([["第一首.flac", readFileSync(first)], ["second.flac", readFileSync(second)], ["cue.flac", readFileSync(cue)]]);
    const archive = join(area.dir, "专辑.zip");
    const outer = join(area.dir, "outer.zip");
    writeFileSync(archive, zip);
    writeFileSync(outer, storedZip([["inner.zip", zip]]));
    const native = unpackPath(`file://${archive}`, "第一首.flac");
    const relative = unpackPath(portable(archive), "第一首.flac");
    const next = unpackPath(portable(archive), "second.flac");
    const nested = unpackPath(unpackPath(portable(outer), "inner.zip"), "第一首.flac");
    const paths = [native, relative, next, nested];
    if (!await waitForLibraryEntry(bridge, native, { rescan: false, timeoutMs: 15000, pollMs: 100 })) {
        throw blockedError("压缩包夹具尚未进入媒体库");
    }

    const formatted = await bridge.invoke("titleformat.evalBatch", { paths, pattern: "%title%" });
    recorder.assertCase("MRP-01 原生、便携、不同条目与嵌套容器保留曲目身份",
        formatted.success && formatted.results?.every((r) => r.success && r.infoAvailable) &&
        JSON.stringify(formatted.results.map((r) => r.result)) === JSON.stringify([firstTitle, firstTitle, secondTitle, firstTitle]),
        formatted, { titles: [firstTitle, firstTitle, secondTitle, firstTitle] });

    const get = await bridge.invoke("playcount.get", { paths });
    const batch = await bridge.invoke("playcount.getBatch", { paths });
    recorder.assertCase("MRP-02 两个统计入口一致，并按原请求路径逐行回显",
        get.success && batch.success && get.count === paths.length &&
        JSON.stringify(get) === JSON.stringify(batch) && batch.results?.every((r, i) => r.success && r.path === paths[i] && Number.isInteger(r.playCount)),
        { get, batch }, { success: true, count: paths.length, sameResults: true });
    recorder.assertCase("MRP-03 同一包内曲目的两种容器拼写给出相同统计",
        batch.success && batch.results?.length === paths.length &&
        JSON.stringify(withoutPath(batch.results.slice(0, 1))) === JSON.stringify(withoutPath(batch.results.slice(1, 2))),
        batch.results?.slice(0, 2), { sameStatistics: true });

    const single = await bridge.invoke("titleformat.eval", { path: relative, pattern: "%title%" });
    const fields = await bridge.invoke("titleformat.evalFields", { path: relative, fields: { title: "%title%" } });
    const fieldsBatch = await bridge.invoke("titleformat.evalFieldsBatch", { paths: [relative, next], fields: { title: "%title%" } });
    recorder.assertCase("MRP-04 其它 titleformat 入口也接受原始便携容器",
        single.success && single.result === firstTitle && fields.success && fields.title === firstTitle &&
        fieldsBatch.success && fieldsBatch.results?.[0]?.title === firstTitle && fieldsBatch.results?.[1]?.title === secondTitle,
        { single, fields, fieldsBatch }, { firstTitle, secondTitle });

    const metadata = await bridge.invoke("metadata.read", { path: relative });
    const replaygain = await bridge.invoke("replaygain.get", { paths: [relative] });
    const lyrics = await bridge.invoke("lyrics.get", { path: relative, source: "embedded" });
    const artwork = await bridge.invoke("artwork.getByPath", { path: relative });
    recorder.assertCase("MRP-05 同类读取到达处理器，空内容不伪装成权限拒绝",
        metadata.success && replaygain.success && replaygain.results?.[0]?.success &&
        lyrics.success && artwork.success,
        { metadata, replaygain, lyrics, artwork }, { authorizationPassed: true });

    const plain = await bridge.invoke("titleformat.evalBatch", { paths: [first, portable(first), portable(first) + "|subsong:abc"], pattern: "%title%" });
    recorder.assertCase("MRP-06 普通便携路径与既有无效轨号回退保持一致",
        plain.success && plain.results?.length === 3 && plain.results.every((r) => r.success && r.result === firstTitle),
        plain, { title: firstTitle, rows: 3 });

    const cuePaths = [cue, unpackPath(portable(archive), "cue.flac")]
        .flatMap((path) => [path + "|subsong:1", path + "|subsong:2"]);
    const chapters = await bridge.invoke("titleformat.evalBatch", { paths: cuePaths, pattern: "%title%" });
    const chapterTitles = ["first chapter", "second chapter", "first chapter", "second chapter"];
    recorder.assertCase("MRP-09 同文件的两个 CUE 子曲目在包内外都保留轨号",
        chapters.success && chapters.results?.length === 4 &&
        chapters.results.every((r, i) => r.success && r.infoAvailable && r.result === chapterTitles[i]),
        chapters, { titles: chapterTitles });

    const invalid = await bridge.invoke("playcount.getBatch", { paths: ["unpack://zip|999999|file-relative://bad|a"] });
    const suffix = await bridge.invoke("playcount.getBatch", { paths: [portable(first) + "|subsong:1/../secret"] });
    recorder.assertCase("MRP-07 损坏容器与后缀遍历仍被授权层拒绝",
        invalid.code === "PERMISSION_DENIED" && suffix.code === "PERMISSION_DENIED",
        { invalid, suffix }, { code: "PERMISSION_DENIED" });

    const link = join(area.dir, "system-link");
    symlinkSync(process.env.SystemRoot, link, "junction");
    junction = link;
    const protectedPath = join(junction, "win.ini");
    const denied = await bridge.invoke("titleformat.evalBatch", { paths: [portable(protectedPath)], pattern: "%title%" });
    const nativeDenied = await bridge.invoke("titleformat.evalBatch", { paths: [protectedPath], pattern: "%title%" });
    recorder.assertCase("MRP-08 监视目录中的便携路径不能绕过 junction 实际落点检查",
        denied.code === "PERMISSION_DENIED" && nativeDenied.code === "PERMISSION_DENIED",
        { denied, nativeDenied }, { code: "PERMISSION_DENIED" });
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
} finally {
    if (junction) {
        try { unlinkSync(junction); junction = undefined; } catch (error) { fatalError ??= error; }
    }
    if (area && !junction) {
        const base = resolve(area.base) + "\\";
        if (!resolve(area.dir).startsWith(base)) fatalError = new Error("夹具清理路径越界");
        else context.cleanup = area.remove();
    }
    await closeClient(client);
}
process.exit(report({ recorder, fatalError, blocked, extra: { runId, context } }));
