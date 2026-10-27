# Load the SDK in a theme

The component automatically injects the native Bridge object `window.fb2k` into every loaded page; invocations via `fb2k.invoke()` and `fb2k.on()` require no external dependencies. The typed `fb.*` API wrappers and `fb-*` Web Components are packaged within the npm module `foo-webview-sdk`, which must be bundled or included in your theme assets.

| Invocation approach | Requires `foo-webview-sdk` |
| --- | --- |
| `window.fb2k.invoke` / `window.fb2k.on` | No |
| `fb.*` typed methods and `fb-*` Web Components | Yes |

## With a bundler

Install the npm dependency:

```bash
npm install foo-webview-sdk
```

Import and initialize as needed. `registerComponents()` registers the `fb-*` custom elements, bound to the same SDK instance:

```js
import fb from 'foo-webview-sdk';
import { registerComponents } from 'foo-webview-sdk/components';

registerComponents();
await fb.player.play();
```

Build the project and copy its production bundle into a template ([Install a theme](./install-theme.md)). [Build your first theme](/tutorials/first-theme) walks through a complete setup with Vite.

<a id="without-a-bundler"></a>

## Without a bundler (Plain HTML/JS)

Copy the pre-bundled global scripts (`node_modules/foo-webview-sdk/dist/`) into your template directory and load them via `<script>` tags, prioritizing the bridge script:

```html
<!-- Paths relative to index.html; files placed in ./sdk/ -->
<script src="./sdk/bridge.global.js"></script>
<script src="./sdk/components.global.js"></script>
```

`bridge.global.js` mounts `window.fb`; `components.global.js` registers every `fb-*` element upon load. The `./sdk/` folder is user-managed; the component creates no directories automatically. [Write a theme without a build step](./plain-html.md) demonstrates a complete page using this structure.

## Next steps {#next}

- [SDK overview](/sdk/overview) covers package modules and entry points.
- [SDK namespaces](/sdk/namespaces) documents all typed `fb.*` methods.
- [Components](/components/) details available `fb-*` custom elements.
