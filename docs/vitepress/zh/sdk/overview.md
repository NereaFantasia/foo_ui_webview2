# SDK 概述

**foo_ui_webview2 SDK** 是原生 bridge 之上带类型的高层封装：把直接调用 `window.fb2k.invoke()` 换成简洁的命名空间方法，并提供带类型的事件、响应式状态以及可选的 Web Components。怎样安装和加载它，见[在主题里加载 SDK](/zh/how-to/load-sdk)。

## 原生 bridge 与 SDK

| 原生调用方式 | SDK 调用方式 |
| --- | --- |
| `window.fb2k.invoke('playback.play', {})` | `fb.player.play()` |
| `window.fb2k.invoke('playlist.addPaths', { playlist: 0, paths })` | `fb.playlist.add(0, paths)` |
| `window.fb2k.invoke('playback.setVolume', { volume: 80 })` | `fb.player.setVolume(80)` |

## 包的入口

npm 包名为 `foo-webview-sdk`，公开的导出有：

| 导入路径 | 用途 |
| --- | --- |
| `foo-webview-sdk` | 聚合的 `fb` 对象、各命名空间导出、bridge、state 与公开类型 |
| `foo-webview-sdk/bridge` | 同一套 bridge 运行时 |
| `foo-webview-sdk/components` | Web Component 类与显式注册函数 |
| `foo-webview-sdk/smp-compat` | Spider Monkey Panel 兼容层 |
| `foo-webview-sdk/schema` | 宿主为每个方法接受的参数键，以及与宿主同样拒收的检查；供测试替身使用 |
| `foo-webview-sdk/bridge.global` | 安装 `window.fb` 的 IIFE 全局包 |
| `foo-webview-sdk/components.global` | 注册全部内置 `fb-*` 元素的 IIFE 全局包 |
| `foo-webview-sdk/smp-compat.global` | 兼容层的 IIFE 全局包 |

导入 `foo-webview-sdk/components` 不会自动注册元素。要调用 `registerComponents()`，它把注册的元素绑定到 `foo-webview-sdk` 导出的 SDK 实例。`components.global.js` 在加载时就注册全部元素。

## 可用性

```javascript
if (fb.isAvailable()) {
    console.log('原生 bridge 已就绪');
    await fb.player.play();
} else {
    console.log('没有 foobar2000 宿主：调用以 NOT_SUPPORTED 失败');
}
```

原生 bridge 注入得晚时，`fb.ready()` 在它可用后 resolve。在宿主之外，每个调用都会在 100 ms 后 resolve `{ success: false, code: 'NOT_SUPPORTED', error, details: { method } }`，事件不会触发。

## 结果与失败

每个命名空间方法都 resolve 宿主的信封：成功时是带 `success: true` 的结果字段，失败时是 `{ success: false, error, code, details? }`。方法不会因为它报告的失败而抛错，列表方法也不会把失败换成空数组。只有宿主直接拒绝的请求（方法不存在、handler 内部抛异常、30 秒内没有应答）才会 reject。唯一的例外是 [`fb.http.request()`](./http.md#request-url-options)：它等待结果事件，任何失败都 reject。

`unwrap(res)` 返回成功分支，`unwrap(res, key)` 返回其中一个字段；失败时两者都抛出带 `code` 与 `details` 的 `ApiCallError`。`<script>` 全局包里它们是 `fb.unwrap` 与 `fb.ApiCallError`。

```ts
import { fb, unwrap, ApiCallError } from 'foo-webview-sdk';

try {
  const playlists = unwrap(await fb.playlist.getAll(), 'playlists');
  console.log(playlists.length);
} catch (e) {
  if (e instanceof ApiCallError) console.warn(e.code, e.message);
}
```

## 给测试替身用的参数键表

`foo-webview-sdk/schema` 让代替宿主的测试替身拒收与宿主相同的调用，不必手抄一份声明。`findParamKeyProblem(method, params)` 按宿主在 handler 运行前的规则检查一次调用的键：方法没有声明的键、缺少或为 `null` 的必填键，连同所在路径一起报告。它只查键名，不查取值类型与范围。`API_PARAM_SHAPES` 是每个已声明方法的同一份数据。

```ts
import { findParamKeyProblem } from 'foo-webview-sdk/schema';

const problem = findParamKeyProblem('playlist.getTracks', { playlist: 0, limit: 10 });
if (problem) {
  // { path: 'limit', reason: 'unknown' }：宿主会以 INVALID_PARAMS 拒绝。
  console.warn(`${problem.reason} parameter ${problem.path}`);
}
```
