# foo_ui_webview2 starter theme

A small player page for [foo_ui_webview2](https://github.com/NereaFantasia/foo_ui_webview2), built with [Vite](https://vite.dev/) and the [`foo-webview-sdk`](https://www.npmjs.com/package/foo-webview-sdk) package. It is the finished result of the tutorial [Build your first theme](https://nereafantasia.github.io/foo_ui_webview2/tutorials/first-theme) ([中文](https://nereafantasia.github.io/foo_ui_webview2/zh/tutorials/first-theme)), which explains every file.

Requires Node.js 20.19 or later, and foobar2000 2.x with the component installed.

```bash
npm install
npm run dev     # development server on http://localhost:5173
npm run build   # production files in dist/
```

While `npm run dev` runs, point foobar2000 at it: **File → Preferences → Display → WebView2 UI → Developer**, tick **Use development server (HMR hot reload)**, enter `http://localhost:5173` and press **Apply**. To install the theme, copy the contents of `dist/` into a template folder under `<profile>\webview-ui\`.
