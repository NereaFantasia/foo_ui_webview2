/**
 * IIFE entry — auto-registers every component shipped in this bundle.
 *
 * Loaded via `<script src="dist/components.global.js">` in theme HTML;
 * runs {@link registerComponents} as a side effect at parse time so
 * every `fb-*` tag is usable immediately afterwards.
 *
 * Default export is the same `registerComponents` function so
 * `<script>` consumers that need to re-register a custom subset can
 * still reach the helper through `window.__fbComponents.default`.
 */

import { registerComponents } from './register.js';

registerComponents();

export default registerComponents;
