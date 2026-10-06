// Globals that documentation examples use without importing anything.
//
// This is a script file on purpose (no top-level import/export): only then are the
// declarations below global. Types come from the SDK the site documents, resolved
// through node_modules like any consumer would.

type DocsSdk = typeof import('foo-webview-sdk');

// The SDK does not publish its method map yet, so native calls are typed from the
// generated source directly. Path is relative to docs/vitepress. test.echo and test.ping
// have no declaration; their types are the SDK's hand-written ones.
type DocsApiMethodMap = import('../../sdk/src/types/generated/index.js').ApiMethodMap & {
  'test.echo': [{ message?: unknown }, import('foo-webview-sdk').TestEchoResponse];
  'test.ping': [Record<string, never>, import('foo-webview-sdk').TestPingResponse];
};
type DocsEventName = import('foo-webview-sdk').FBEventName;

interface DocsNativeFb2k {
  invoke<M extends keyof DocsApiMethodMap>(
    method: M,
    params?: DocsApiMethodMap[M][0],
  ): Promise<DocsApiMethodMap[M][1]>;
  // on() returns a function that removes the handler; off() returns nothing
  // (kBridgeBootstrapScript in src/webview/BridgeBootstrapScript.inl).
  on(event: DocsEventName, handler: (data: any) => void): () => void;
  off(event: DocsEventName, handler: (data: any) => void): void;
}

/** The SDK facade that the IIFE build installs as `window.fb`. */
declare const fb: DocsSdk['default'];
/** The bridge that the host injects into every page. */
declare const fb2k: DocsNativeFb2k;

/** The parts of WebView2's `window.chrome.webview` that examples handling shared buffers use. */
interface DocsWebview {
  addEventListener(
    type: 'sharedbufferreceived',
    listener: (event: { getBuffer(): ArrayBuffer; additionalData: any }) => void,
  ): void;
  releaseBuffer(buffer: ArrayBuffer): void;
}

interface Window {
  fb: DocsSdk['default'];
  chrome: { webview: DocsWebview };
}

/** Stylesheets imported for their side effect, which a bundler such as Vite resolves. */
declare module '*.css';
