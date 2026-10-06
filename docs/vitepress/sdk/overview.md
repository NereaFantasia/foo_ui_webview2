# SDK overview

The **foo_ui_webview2 SDK** is a typed, high-level facade over the native bridge. It turns direct `window.fb2k.invoke()` calls into concise namespace methods and provides typed events, reactive state, and optional Web Components. [Load the SDK in a theme](/how-to/load-sdk) shows how to install and load it.

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

Importing `foo-webview-sdk/components` does not register elements automatically. Call `registerComponents()`, which binds the elements it registers to the SDK instance that `foo-webview-sdk` exports. `components.global.js` registers every element as a side effect of loading.

## Availability

```javascript
if (fb.isAvailable()) {
    console.log('The native bridge is ready');
    await fb.player.play();
} else {
    console.log('No foobar2000 host: calls resolve with NOT_SUPPORTED');
}
```

`fb.ready()` resolves when a late-injected native bridge becomes available. Outside the host, every call resolves after 100 ms with `{ success: false, code: 'NOT_SUPPORTED', error, details: { method } }`, and events never fire.

## Results and failures

Every namespace method resolves with the host's envelope: the result fields with `success: true`, or `{ success: false, error, code, details? }` when the call failed. A method does not throw for a failure it reports, and list methods do not replace a failure with an empty array. Only a request the host refuses outright (an unknown method, an exception in a handler, no answer within 30 s) rejects. [`fb.http.request()`](./http.md#request-url-options) is the one exception: it waits for the result event and rejects for every failure.

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

A test double that stands in for the host can refuse the same calls the host refuses without
copying the declarations by hand. `findParamKeyProblem(method, params)` checks the keys of a call
the way the host does before a handler runs: a key the method does not declare, or a required
key that is absent or `null`, is reported with its path. It checks key names only, not value
types or ranges. `API_PARAM_SHAPES` holds the same data for every declared method.

```ts
import { findParamKeyProblem } from 'foo-webview-sdk/schema';

const problem = findParamKeyProblem('playlist.getTracks', { playlist: 0, limit: 10 });
if (problem) {
  // { path: 'limit', reason: 'unknown' }: the host answers with INVALID_PARAMS.
  console.warn(`${problem.reason} parameter ${problem.path}`);
}
```
