# SDK overview

The **foo_ui_webview2 SDK** is a typed, high-level facade over the native bridge. It organizes direct `window.fb2k.invoke()` calls into clean domain-driven namespaces, providing typed event subscriptions, reactive state management, and ready-to-use Web Components. See [Load the SDK in a theme](/how-to/load-sdk) for installation and bundle usage.

## Native bridge versus SDK

| Native bridge | SDK facade |
| --- | --- |
| `window.fb2k.invoke('playback.play', {})` | `fb.player.play()` |
| `window.fb2k.invoke('playlist.addPaths', { playlist: 0, paths: [...] })` | `fb.playlist.add(0, paths)` |
| `window.fb2k.invoke('playback.setVolume', { volume: 80 })` | `fb.player.setVolume(80)` |

## Package entry points

The npm package is named `foo-webview-sdk`. Its public exports are:

| Import | Purpose |
| --- | --- |
| `foo-webview-sdk` | Aggregate `fb` object, namespace exports, bridge, state, and public types |
| `foo-webview-sdk/bridge` | The same bridge runtime surface |
| `foo-webview-sdk/components` | Web Component classes and explicit registration helpers |
| `foo-webview-sdk/smp-compat` | Spider Monkey Panel compatibility layer |
| `foo-webview-sdk/schema` | The parameter keys the host accepts for each method, and a check that refuses what the host refuses; for test doubles |
| `foo-webview-sdk/bridge.global` | IIFE bundle that installs `window.fb` |
| `foo-webview-sdk/components.global` | IIFE bundle that registers all shipped `fb-*` elements |
| `foo-webview-sdk/smp-compat.global` | IIFE compatibility bundle |

Importing `foo-webview-sdk/components` does not register elements in the custom elements registry automatically. Call `registerComponents()` to register them and bind elements to the exported SDK instance. By contrast, the `components.global.js` script bundle registers all built-in elements upon loading.

## Availability

```javascript
if (fb.isAvailable()) {
    console.log('The native bridge is ready');
    await fb.player.play();
} else {
    console.log('No foobar2000 host: calls resolve with NOT_SUPPORTED');
}
```

`fb.ready()` resolves when a late-injected native bridge becomes available. In non-foobar2000 host environments, API calls resolve after 100 ms with `{ success: false, code: 'NOT_SUPPORTED', error, details: { method } }`, and events are not dispatched.

## Results and failures

Every namespace method resolves with the host's standard envelope: `success: true` with data payload on success, or `{ success: false, error, code, details? }` on failure. Business-level errors do not reject the promise, and collection queries do not silently replace failures with empty arrays. Rejection occurs only when requests are refused by the framework (unknown method, uncaught handler exception, or 30 s timeout). The sole exception is [`fb.http.request()`](./http.md#request-url-options), which awaits async operation events and rejects on error.

`unwrap(res)` returns the success branch and `unwrap(res, key)` one field of it; for a failure both throw an `ApiCallError` carrying `code` and `details`. In the `<script>` bundle they are `fb.unwrap` and `fb.ApiCallError`.

```ts
import { fb, unwrap, ApiCallError } from 'foo-webview-sdk';

try {
  const playlists = unwrap(await fb.playlist.getAll(), 'playlists');
  console.log(playlists.length);
} catch (e) {
  if (e instanceof ApiCallError) console.warn(e.code, e.message);
}
```

## Parameter keys for test doubles

With the `foo-webview-sdk/schema` module, a test double that stands in for the host can refuse the same calls the host refuses without copying the declarations by hand. `findParamKeyProblem(method, params)` mirrors pre-dispatch host validation: detecting undeclared parameter keys, missing or `null` required fields, and returning the offending property path. It validates property key structures without asserting runtime types or value ranges. `API_PARAM_SHAPES` exposes the parameter shapes behind these checks, keyed by declared method name.

```ts
import { findParamKeyProblem } from 'foo-webview-sdk/schema';

const problem = findParamKeyProblem('playlist.getTracks', { playlist: 0, limit: 10 });
if (problem) {
  // { path: 'limit', reason: 'unknown' }: the host answers with INVALID_PARAMS
  console.warn(`${problem.reason} parameter ${problem.path}`);
}
```
