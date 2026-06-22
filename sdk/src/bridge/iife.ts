/**
 * IIFE entry — exposes the aggregate `fb` object as the sole export so
 * the bundled `<script>` consumer sees `window.fb` directly (no
 * `fb.default` indirection).
 */

import fb from './index.js';

export default fb;
