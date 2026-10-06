# Load the SDK in a theme

Every page gets the native bridge `window.fb2k` from the component; nothing needs to be installed for `fb2k.invoke()` and `fb2k.on()`. The `fb.*` wrappers and the `fb-*` Web Components come from the npm package `foo-webview-sdk`, which the component does not install into templates.

| You use | Needs `foo-webview-sdk` |
| --- | --- |
| `window.fb2k.invoke` / `window.fb2k.on` | No |
| `fb.*` wrappers and `fb-*` components | Yes |

## With a bundler

Install the package:

```bash
npm install foo-webview-sdk
```

Import it where you use it. `registerComponents()` defines the `fb-*` elements, bound to the same SDK instance:

```js
import fb from 'foo-webview-sdk';
import { registerComponents } from 'foo-webview-sdk/components';

registerComponents();
await fb.player.play();
```

Build the project and install its output as a template ([Install a theme](./install-theme.md)). [Build your first theme](/tutorials/first-theme) shows a complete Vite project.

## Without a bundler

Copy the global bundles from the package (`node_modules/foo-webview-sdk/dist/`) into the template folder and load them with `<script>` tags, the bridge first. `bridge.global.js` installs `window.fb`; `components.global.js` registers every `fb-*` element when it loads.

```html
<!-- Paths relative to index.html; here the files were copied into ./sdk/ -->
<script src="./sdk/bridge.global.js"></script>
<script src="./sdk/components.global.js"></script>
```

The `./sdk/` folder in this example is one you create; the component does not make it. [Write a theme without a build step](./plain-html.md) shows a whole page set up this way.

## Next

- [SDK overview](/sdk/overview) lists the package's entry points.
- [SDK namespaces](/sdk/namespaces) lists the `fb.*` methods.
- [Components](/components/) lists the `fb-*` elements.
