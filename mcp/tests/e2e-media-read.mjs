/**
 * 在独立弹窗验证媒体地址、容器信息与真实 HTMLMediaElement。
 * 文件全部复制到本次临时目录；不改播放队列、当前曲目或主页面。
 */
import CDP from "chrome-remote-interface";
import { createServer } from "node:http";
import { spawnSync } from "node:child_process";
import { appendFileSync, copyFileSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import {
    closeClient, connectBridgePage, createBridge, createRecorder, report, resolvePort,
} from "./lib/e2e-harness.mjs";

const repo = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const recorder = createRecorder();
const popups = [];
const delay = (ms) => new Promise((done) => setTimeout(done, ms));
let mainClient, mainBridge, server, scratch, fatalError;
let blocked = false;

async function waitReady(bridge, id, href, previousDocument) {
    const deadline = Date.now() + 15000;
    while (Date.now() < deadline) {
        try {
            if (href && await bridge.evaluateValue("location.href") !== href) { await delay(100); continue; }
            if (previousDocument && await bridge.evaluateValue("window.__mediaDocumentId") === previousDocument) { await delay(100); continue; }
            const current = await bridge.invokeRaw("window.getCurrentWindowId", {});
            if (current?.value?.windowId === id && await bridge.evaluateValue("typeof fb?.media === 'object'")) return;
        } catch { /* 导航时旧执行上下文会被释放。 */ }
        await delay(100);
    }
    throw new Error("媒体测试弹窗未就绪");
}

// 宿主只信任它自己把窗口导航过去的来源，弹窗直接开到本地服务器的绝对地址拿不到桥。
// 所以临时把开发服务器指向本地服务器，开一个相对地址的弹窗，由宿主导航并登记这个来源；
// 弹窗页面就绪后再改回原设置，已登记的信任不随设置撤销。
async function openPopup(url) {
    const original = await mainBridge.invoke("window.getDevServerConfig", {});
    try {
        await mainBridge.invoke("window.setDevServerConfig", { useDevServer: true, devServerUrl: url });
        const created = await mainBridge.invoke("window.createPopup", {
            url: "", title: "Media API validation", width: 320, height: 240,
            behavior: { noActivate: true },
        });
        if (!created?.success || typeof created.windowId !== "string") throw new Error(JSON.stringify(created));
        const popup = { id: created.windowId };
        popups.push(popup);
        const deadline = Date.now() + 15000;
        while (Date.now() < deadline) {
            const target = (await CDP.List({ port: resolvePort() })).find((page) => {
                try {
                    const pageUrl = new URL(page.url);
                    return page.type === "page" && pageUrl.origin === new URL(url).origin &&
                        pageUrl.searchParams.get("windowId") === popup.id;
                } catch { return false; }
            });
            if (target) {
                popup.client = await CDP({ port: resolvePort(), target });
                popup.bridge = createBridge(popup.client.Runtime, { invokeTimeoutMs: 10000 });
                await waitReady(popup.bridge, popup.id);
                return popup;
            }
            await delay(100);
        }
        throw new Error("未找到媒体测试弹窗的 CDP 页面");
    } finally {
        await mainBridge.invoke("window.setDevServerConfig", {
            useDevServer: original?.useDevServer === true,
            devServerUrl: String(original?.devServerUrl ?? ""),
        });
    }
}

async function request(bridge, url, options = {}) {
    return bridge.evaluateValue(`(async () => {
        const response = await fetch(${JSON.stringify(url)}, {
            ...${JSON.stringify(options)}, signal: AbortSignal.timeout(8000)
        });
        const bytes = new Uint8Array(await response.arrayBuffer());
        return { status: response.status, length: bytes.length, first: Array.from(bytes.slice(0, 16)),
            range: response.headers.get('Content-Range'),
            lengthHeader: response.headers.get('Content-Length'),
            etag: response.headers.get('ETag') };
    })()`);
}

async function issue(bridge, path) {
    const value = await bridge.invoke("media.getStreamUrl", { path });
    recorder.assertCase("media.getStreamUrl 签发文档地址", value?.success && typeof value.url === "string", value, "success + url");
    if (!value?.success) throw new Error(`媒体地址签发失败: ${JSON.stringify(value)}`);
    return value;
}

async function checkContainers(bridge, paths) {
    for (const [container, path] of [["mp4", paths.mp4], ["matroska", paths.mkv]]) {
        const info = await bridge.invoke("media.getContainerInfo", { path });
        const video = info?.tracks?.find((track) => track.type === "video");
        const audio = info?.tracks?.find((track) => track.type === "audio");
        recorder.assertCase(`media.getContainerInfo ${container} 容器与三类轨道`,
            info?.success && info.recognized && info.container === container &&
            video?.width === 32 && video.height === 24 &&
            audio?.sampleRate === 48000 && audio.channels === 2 && audio.language === "jpn" &&
            info.tracks.some((track) => track.type === "subtitle"), info, "32×24 视频、48kHz 双声道日语音轨、字幕");
        if (container === "matroska") recorder.assertCase("MKV 附件只列元数据",
            info?.attachments?.some((item) => item.name === "fixture.txt" && item.mimeType === "text/plain"),
            info?.attachments, "fixture.txt / text/plain");
    }
    // foobar2000 把每章拆成一个子曲目，标识从 1 起；宿主核对时长后把标识填进章节。
    for (const [name, path] of [["M4A", paths.chaptersMp4], ["MKA", paths.chaptersMkv]]) {
        const info = await bridge.invoke("media.getContainerInfo", { path });
        const chapters = info?.chapters ?? [];
        recorder.assertCase(`media.getContainerInfo ${name} 章节与子曲目对应`,
            info?.success && chapters.length === 3 && chapters[2].title === "第三章" &&
            chapters.every((chapter, i) => Math.abs(chapter.start - i * 0.5) < 1e-6 &&
                Math.abs(chapter.end - (i * 0.5 + 0.5)) < 1e-6 && chapter.subsong === i + 1),
            chapters, "0、0.5、1 秒起的三章，subsong 1–3，第三章标题");
        if (name === "M4A") recorder.assertCase("MP4 章节文本轨不报告为字幕",
            info?.tracks?.length === 2 && info.tracks[1].type === "other", info?.tracks, "第二条轨道 type 为 other");
    }
    const unknown = await bridge.invoke("media.getContainerInfo", { path: paths.large });
    recorder.assertCase("未识别文件明确返回 recognized:false", unknown?.success && unknown.recognized === false && unknown.tracks.length === 0,
        unknown, "success + recognized:false + empty tracks");
    const malformed = await bridge.invoke("media.getContainerInfo", { path: paths.broken });
    recorder.assertCase("损坏 MP4 返回失败", malformed?.success === false && malformed.code === "OPERATION_FAILED", malformed, "OPERATION_FAILED");
    const network = await bridge.invoke("media.getStreamUrl", { path: "https://example.invalid/a.mp4" });
    recorder.assertCase("媒体接口拒绝网络路径", network?.success === false && network.code === "INVALID_PARAMS", network, "INVALID_PARAMS");
    const missing = await bridge.invoke("media.getStreamUrl", { path: join(scratch, "missing.mp4") });
    recorder.assertCase("不存在的允许路径返回 NOT_FOUND", missing?.success === false && missing.code === "NOT_FOUND", missing, "NOT_FOUND");
    const extra = await bridge.invoke("media.getContainerInfo", { path: paths.mp4, undeclared: true });
    recorder.assertCase("统一参数解析拒收未声明键", extra?.success === false && extra.code === "INVALID_PARAMS", extra, "INVALID_PARAMS");
}

async function checkRanges(bridge, paths) {
    const small = await issue(bridge, paths.mp4);
    const expected = readFileSync(paths.mp4);
    const whole = await request(bridge, small.url);
    recorder.assertCase("小文件无 Range 返回完整正文", whole.status === 200 && whole.length === expected.length &&
        JSON.stringify(whole.first) === JSON.stringify([...expected.subarray(0, 16)]), whole, { status: 200, length: expected.length });
    const suffix = await request(bridge, small.url, { headers: { Range: "bytes=-16" } });
    recorder.assertCase("后缀 Range 返回原文件尾部", suffix.status === 206 && suffix.length === 16 &&
        JSON.stringify(suffix.first) === JSON.stringify([...expected.subarray(-16)]), suffix, "206 + 文件末16字节");
    const head = await request(bridge, small.url, { method: "HEAD" });
    recorder.assertCase("HEAD 只返回元数据", head.status === 200 && head.length === 0 && Number(head.lengthHeader) === expected.length && !!head.etag,
        head, { status: 200, bodyLength: 0, contentLength: expected.length });
    for (const range of ["bytes=999999999-", "bytes=0-1,4-5"]) {
        const value = await request(bridge, small.url, { headers: { Range: range } });
        recorder.assertCase(`无效 Range ${range}`, value.status === 416 && value.range === `bytes */${expected.length}`,
            value, { status: 416, range: `bytes */${expected.length}` });
    }
    const unknownUrl = small.url.slice(0, small.url.lastIndexOf("/") + 1) + "not-a-token";
    const unknown = await request(bridge, unknownUrl);
    recorder.assertCase("未知 token 的 403 可被合法来源读取", unknown.status === 403, unknown, { status: 403 });
    const large = await issue(bridge, paths.large);
    const noRange = await request(bridge, large.url);
    recorder.assertCase("大文件必须使用 Range", noRange.status === 416, noRange, { status: 416 });
    const bounded = await request(bridge, large.url, { headers: { Range: "bytes=0-" } });
    recorder.assertCase("开区间限制每段 2 MiB", bounded.status === 206 && bounded.length === 2 * 1024 * 1024 &&
        bounded.range === `bytes 0-2097151/${large.size}`, bounded, "206 / 2097152 bytes");
    appendFileSync(paths.large, Buffer.from([99]));
    const changed = await request(bridge, large.url, { headers: { Range: "bytes=0-3" } });
    recorder.assertCase("文件变化使旧地址失效", changed.status === 410, changed, { status: 410 });
    return small.url;
}

async function checkElement(popup, path) {
    const ranges = [];
    await popup.client.Network.enable();
    const onResponse = ({ response }) => {
        if (response.url.includes("/fb2k-media/") && response.status === 206) {
            const range = Object.entries(response.headers).find(([name]) => name.toLowerCase() === "content-range")?.[1];
            ranges.push({ url: response.url, range });
        }
    };
    popup.client.Network.responseReceived(onResponse);
    const result = await popup.bridge.evaluateValue(`(async () => {
        const element = document.createElement('video');
        let follower;
        const waitFor = async (predicate) => {
            const deadline = performance.now() + 8000;
            while (!predicate()) {
                if (performance.now() > deadline) throw new Error('media condition timeout');
                await new Promise(resolve => setTimeout(resolve, 20));
            }
        };
        try {
            element.muted = true; element.crossOrigin = 'anonymous'; element.preload = 'metadata';
            document.body.append(element);
            const result = fb.unwrap(await fb.media.getStreamUrl(${JSON.stringify(path)}));
            element.src = result.url; element.load();
            await waitFor(() => element.readyState >= 1 || element.error);
            if (element.error) throw new Error('media error ' + element.error.code);
            await element.play();
            const playing = !element.paused;
            element.pause();
            const dimensions = [element.videoWidth, element.videoHeight];
            const duration = element.duration;
            element.currentTime = 3.25;
            await waitFor(() => !element.seeking && element.readyState >= 2);
            const seekPosition = element.currentTime;
            element.removeAttribute('src'); element.load();
            // 用可控时钟隔离 Follower 与浏览器元素的行为；此处不证明真实宿主时间轴同步。
            let position = 1.25;
            const listeners = new Set();
            const clock = { ready: Promise.resolve(), state: 'paused', duration,
                position: () => position, resync: async () => {},
                onChange: listener => { listeners.add(listener); return () => listeners.delete(listener); } };
            const change = reason => listeners.forEach(listener => listener({ reason, state: clock.state, position }));
            follower = new fb.MediaElementFollower(element, { clock });
            await follower.setSource(${JSON.stringify(path)});
            const assigned = element.src.startsWith('https://foo-ui-webview2.local/fb2k-media/');
            await waitFor(() => element.readyState >= 1 && !element.seeking && Math.abs(element.currentTime - position) < 0.02);
            const initiallyAligned = element.paused && Math.abs(element.currentTime - 1.25) < 0.02;
            clock.state = 'playing'; change('state');
            await waitFor(() => !element.paused);
            const resumed = !element.paused;
            clock.position = () => element.currentTime + 0.12;
            await waitFor(() => element.playbackRate > 1);
            const rate = element.playbackRate;
            clock.position = () => position;
            clock.state = 'paused'; position = 2.75; change('seek');
            await waitFor(() => element.paused && !element.seeking && Math.abs(element.currentTime - 2.75) < 0.02);
            const pausedAligned = element.paused && Math.abs(element.currentTime - 2.75) < 0.02;
            await follower.setSource(null);
            return { dimensions, duration, playing, seekPosition, streamUrl: result.url, initiallyAligned, resumed, rate,
                pausedAligned, assigned, cleared: !element.getAttribute('src'), muted: element.muted };
        } finally {
            follower?.dispose(); element.pause(); element.removeAttribute('src'); element.load(); element.remove();
        }
    })()`, true, 60000);
    popup.client.removeListener("Network.responseReceived", onResponse);
    recorder.assertCase("真实 video 元素通过媒体路由读取 metadata 并静音播放", result?.playing && result.dimensions?.[0] === 640 &&
        result.dimensions[1] === 360 && result.duration >= 4, result, "640×360 / 4s / playing");
    const videoRanges = ranges.filter((entry) => entry.url === result.streamUrl);
    recorder.assertCase("大媒体 seek 使用多个有界分段", Math.abs(result.seekPosition - 3.25) < 0.1 && videoRanges.length >= 2 &&
        videoRanges.every(({ range }) => {
            const match = /^bytes (\d+)-(\d+)\/(\d+)$/.exec(range ?? "");
            return match && Number(match[2]) - Number(match[1]) + 1 <= 2 * 1024 * 1024;
        }), videoRanges, "至少两段206，每段≤2MiB，seek到3.25s");
    recorder.assertCase("受控借用时钟驱动真实元素对齐、播放、调速与暂停", result.initiallyAligned && result.resumed &&
        result.rate > 1 && result.rate <= 1.05 && result.pausedAligned, result, "位置对齐、暂停正确、校正率≤1.05");
    recorder.assertCase("SDK Follower 在真实元素上设置与清空源", result?.assigned && result.cleared && result.muted,
        result, "assigned + cleared + muted");
}

async function checkOwnership(popup, url, baseUrl, path) {
    const other = await openPopup(baseUrl);
    const cross = await request(other.bridge, url);
    recorder.assertCase("其他弹窗不能使用签发方 token", cross.status === 403, cross, { status: 403 });
    const opaque = await popup.bridge.evaluateValue(`new Promise((resolve, reject) => {
        const iframe = document.createElement('iframe'); iframe.sandbox = 'allow-scripts';
        const timer = setTimeout(() => { cleanup(); reject(new Error('opaque frame timeout')); }, 8000);
        const cleanup = () => { clearTimeout(timer); window.removeEventListener('message', receive); iframe.remove(); };
        const receive = (event) => { if (event.source !== iframe.contentWindow) return; cleanup(); resolve(event.data); };
        window.addEventListener('message', receive);
        iframe.srcdoc = '<script>fetch(' + ${JSON.stringify(JSON.stringify(url))} +
            ').then(r=>r.arrayBuffer()).then(()=>parent.postMessage("readable","*"),()=>parent.postMessage("denied","*"))<' + '/script>';
        document.body.append(iframe);
    })`);
    recorder.assertCase("Origin:null iframe 不能读取正文", opaque === "denied", opaque, "denied");
    const previousDocument = await popup.bridge.evaluateValue("window.__mediaDocumentId");
    const nextUrl = `${baseUrl}?windowId=${encodeURIComponent(popup.id)}&generation=2`;
    const navigation = await popup.client.Page.navigate({ url: nextUrl });
    if (navigation.errorText) throw new Error(navigation.errorText);
    await waitReady(popup.bridge, popup.id, nextUrl, previousDocument);
    // 先证明新文档能读取新地址，避免把整个页面失去访问权限误判成旧令牌隔离成功。
    const fresh = await issue(popup.bridge, path);
    const freshRead = await request(popup.bridge, fresh.url);
    recorder.assertCase("导航后的新文档可以签发并读取新地址", freshRead.status === 200, freshRead, { status: 200 });
    const navigated = await request(popup.bridge, url);
    recorder.assertCase("同源导航后旧 token 失效", navigated.status === 403, navigated, { status: 403 });
}

try {
    const connection = await connectBridgePage();
    mainClient = connection.client;
    mainBridge = createBridge(mainClient.Runtime);
    const bundle = readFileSync(join(repo, "sdk/dist/bridge.global.js"));
    scratch = mkdtempSync(join(tmpdir(), "fb2k-media-"));
    const paths = { mp4: join(scratch, "clip.mp4"), mkv: join(scratch, "clip.mkv"), video: join(scratch, "large.mp4"),
        large: join(scratch, "large.bin"), broken: join(scratch, "broken.mp4"),
        chaptersMp4: join(scratch, "chapters.m4a"), chaptersMkv: join(scratch, "chapters.mka") };
    copyFileSync(join(repo, "tests/fixtures/media/av-subtitle.mp4"), paths.mp4);
    copyFileSync(join(repo, "tests/fixtures/media/av-subtitle.mkv"), paths.mkv);
    copyFileSync(join(repo, "tests/fixtures/media/chapters.m4a"), paths.chaptersMp4);
    copyFileSync(join(repo, "tests/fixtures/media/chapters.mka"), paths.chaptersMkv);
    writeFileSync(paths.large, Buffer.alloc(2 * 1024 * 1024 + 64, 42));
    // ftyp 合法，后面的 moov 声明长度超过文件末尾。
    writeFileSync(paths.broken, Buffer.from("000000186674797069736f6d0000000069736f6d6d703432000001006d6f6f76", "hex"));
    const encoded = spawnSync(process.env.FB2K_E2E_FFMPEG || "ffmpeg", [
        "-hide_banner", "-loglevel", "error", "-f", "lavfi", "-i", "testsrc2=size=640x360:rate=25",
        "-t", "4", "-an", "-c:v", "libx264", "-preset", "ultrafast", "-b:v", "8M", "-minrate", "8M",
        "-maxrate", "8M", "-bufsize", "8M", "-x264-params", "nal-hrd=cbr", "-movflags", "+faststart", paths.video,
    ], { timeout: 30000, encoding: "utf8", windowsHide: true });
    if (encoded.error || encoded.status !== 0) throw new Error(`无法生成大媒体样本: ${encoded.error || encoded.stderr}`);
    if (readFileSync(paths.video).length <= 2 * 1024 * 1024) throw new Error("大媒体样本不足 2 MiB");
    server = createServer((req, response) => {
        response.setHeader("Cache-Control", "no-store");
        response.setHeader("Content-Type", req.url === "/sdk.js" ? "text/javascript" : "text/html");
        response.end(req.url === "/sdk.js" ? bundle : '<!doctype html><title>Media API validation</title><script>window.__mediaDocumentId=crypto.randomUUID()</script><script src="/sdk.js"></script>');
    });
    await new Promise((done, reject) => { server.once("error", reject); server.listen(0, "127.0.0.1", done); });
    const baseUrl = `http://127.0.0.1:${server.address().port}/`;
    const popup = await openPopup(baseUrl);
    await checkContainers(popup.bridge, paths);
    const url = await checkRanges(popup.bridge, paths);
    await checkElement(popup, paths.video);
    await checkOwnership(popup, url, baseUrl, paths.mp4);
} catch (error) {
    fatalError = error;
    blocked = recorder.cases.length === 0 && (error?.blocked === true || error?.code === "ECONNREFUSED");
} finally {
    for (const popup of popups.reverse()) {
        await closeClient(popup.client);
        try {
            const closed = await mainBridge.invoke("window.closePopup", { windowId: popup.id });
            let gone = false;
            const deadline = Date.now() + 5000;
            while (Date.now() < deadline) {
                const windows = await mainBridge.invoke("window.getAllWindows", {});
                if (windows?.success && Array.isArray(windows.items) && !windows.items.some((item) => item.windowId === popup.id)) {
                    gone = true; break;
                }
                await delay(100);
            }
            recorder.assertCase(`关闭本次弹窗 ${popup.id}`, closed?.success && gone, { closed, gone }, "close成功且窗口消失");
        } catch (error) {
            recorder.assertCase(`关闭本次弹窗 ${popup.id}`, false, String(error), "close成功且窗口消失");
        }
    }
    await closeClient(mainClient);
    if (server) { server.closeAllConnections(); await new Promise((done) => server.close(done)); }
    if (scratch && dirname(resolve(scratch)) === resolve(tmpdir()) && basename(scratch).startsWith("fb2k-media-"))
        rmSync(scratch, { recursive: true, force: true });
}
process.exitCode = report({ recorder, blocked, fatalError,
    extra: { fixtures: "FFmpeg 合成媒体；未修改用户素材或播放器传输状态" } });
