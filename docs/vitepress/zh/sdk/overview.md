# SDK 概述

**foo_ui_webview2 SDK** 是构建在原生 bridge 基础上的强类型高层封装：将底层的 `window.fb2k.invoke()` 调用组织为清晰的领域命名空间方法，并提供类型安全的事件订阅、响应式状态管理以及可选的 Web Components。安装与引入指引参见[在主题中加载 SDK](/zh/how-to/load-sdk)。

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

直接导入 `foo-webview-sdk/components` 不会自动注册自定义元素。调用 `registerComponents()` 可完成注册，并将组件绑定至当前导出的 SDK 实例。作为对照，`components.global.js` 全局脚本包在加载时即自动注册全部内置元素。

## 可用性

```javascript
if (fb.isAvailable()) {
    console.log('原生 bridge 已就绪');
    await fb.player.play();
} else {
    console.log('没有 foobar2000 宿主：调用以 NOT_SUPPORTED 失败');
}
```

若原生 bridge 初始化稍有延迟，可调用 `fb.ready()` 等待其就绪并 resolve。在非 foobar2000 宿主环境中，所有 API 调用均在 100 ms 后以 `{ success: false, code: 'NOT_SUPPORTED', error, details: { method } }` 响应，且不会派发任何事件。

## 结果与失败

各命名空间方法均 resolve 宿主返回的标准结果信封：成功时包含 `success: true` 与业务数据字段；失败时返回 `{ success: false, error, code, details? }`。业务调用失败不会触发 Promise rejection，列表查询亦不会在失败时静默替换为空数组。仅当请求被框架级直接拒绝（如方法未注册、处理函数抛出未捕获异常或 30 秒请求超时）时才会导致 Promise reject。唯一的例外是 [`fb.http.request()`](./http.md#request-url-options)：该方法采用异步任务等待模式，任何阶段失败均会 reject。

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

借助 `foo-webview-sdk/schema` 模块，替代宿主的测试替身可以按宿主的规则拒绝同样的调用，无需手动复制声明。`findParamKeyProblem(method, params)` 模拟宿主分发前的校验逻辑：检测未声明的未知参数键、缺失或为 `null` 的必填字段，并定位所在属性路径。该工具专注于键名形态检查，不验证参数类型与取值范围。`API_PARAM_SHAPES` 按方法名列出各已声明方法的参数形态，即上述检查所依据的数据。

```ts
import { findParamKeyProblem } from 'foo-webview-sdk/schema';

const problem = findParamKeyProblem('playlist.getTracks', { playlist: 0, limit: 10 });
if (problem) {
  // { path: 'limit', reason: 'unknown' }：宿主会以 INVALID_PARAMS 拒绝
  console.warn(`${problem.reason} parameter ${problem.path}`);
}
```
