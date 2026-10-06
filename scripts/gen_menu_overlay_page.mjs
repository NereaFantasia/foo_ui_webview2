#!/usr/bin/env node
// Generate src/window/MenuOverlayPage.inl from menu-overlay sources.
//   node scripts/gen_menu_overlay_page.mjs --write
//   node scripts/gen_menu_overlay_page.mjs --check   # drift gate (exit 3 on mismatch)
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const REPO = path.resolve(__dirname, '..');
const SRC_DIR = path.join(REPO, 'src/window/menu-overlay');
const OUT_INL = path.join(REPO, 'src/window/MenuOverlayPage.inl');
const EXIT_OK = 0;
const EXIT_ERR = 1;
const EXIT_DRIFT = 3;

function read(rel) {
  return fs.readFileSync(path.join(SRC_DIR, rel), 'utf8');
}

function stripExports(mjsSource) {
  let s = mjsSource;
  // Drop the browser-only helper export (re-injected separately).
  s = s.replace(/\nexport\s+const\s+BROWSER_MOUNT_HELPER[\s\S]*$/m, '\n');
  s = s.replace(/^export\s+/gm, '');
  // Node Buffer is unavailable in the overlay page; length check is enough
  // (C++ already caps single SVG at 32 KiB before persistence / show).
  s = s.replace(
    /function isOversizedSvgContent\(content\) \{[\s\S]*?\n\}/,
    `function isOversizedSvgContent(content) {
  return String(content || '').length > MAX_SINGLE_SVG_BYTES;
}`,
  );
  return s.trim() + '\n';
}

function stripLayoutForBrowser(mjsSource) {
  // Browser overlay is a classic script: drop ESM export keywords only.
  // Virtual-tree helpers stay available for applyStableAttrs / hide-show protocol.
  return String(mjsSource || '').replace(/^export\s+/gm, '').trim() + '\n';
}

async function buildPageHtml() {
  const allowlistMod = await import(pathToFileURL(path.join(SRC_DIR, 'svg-allowlist.mjs')).href);
  const allowlistSrc = stripExports(read('svg-allowlist.mjs'));
  const mountHelper = allowlistMod.BROWSER_MOUNT_HELPER.trim() + '\n';
  const layoutSrc = stripLayoutForBrowser(read('menu-overlay-layout.mjs'));
  const css = read('menu-overlay.css');
  const protectedCss = read('menu-overlay.protected.css');
  const js = read('menu-overlay.js');
  let html = read('menu-overlay.html');
  html = html
    .replace('__FB_DEFAULT_CSS__', css.trimEnd())
    .replace('__FB_PROTECTED_CSS__', protectedCss.trimEnd())
    .replace('__FB_SVG_ALLOWLIST__', allowlistSrc + '\n' + mountHelper)
    .replace('__FB_LAYOUT__', layoutSrc)
    .replace('__FB_MENU_JS__', js.trimEnd());
  return html;
}

function toInl(pageHtml) {
  // Split into chunks so MSVC raw-string length stays manageable (same pattern
  // as the previous hand-maintained LR"MENUHTML" concatenation).
  const chunkSize = 6000;
  const chunks = [];
  for (let i = 0; i < pageHtml.length; i += chunkSize) {
    chunks.push(pageHtml.slice(i, i + chunkSize));
  }
  const body = chunks.map((c) => `LR"MENUHTML(${c})MENUHTML"`).join('\n        ');
  return `// AUTO-GENERATED — do not edit.
// Source: src/window/menu-overlay/{menu-overlay.html,menu-overlay.css,menu-overlay.protected.css,menu-overlay.js,menu-overlay-layout.mjs,svg-allowlist.mjs}
// Regenerate: node scripts/gen_menu_overlay_page.mjs --write
// Drift gate: node scripts/gen_menu_overlay_page.mjs --check
#pragma once
inline std::wstring BuildMenuOverlayHtmlFromSources() {
    return ${body};
}
`;
}

async function main() {
  const args = new Set(process.argv.slice(2));
  if (args.has('--help') || args.size === 0) {
    console.log('Usage: node scripts/gen_menu_overlay_page.mjs --write|--check');
    process.exit(args.has('--help') ? EXIT_OK : EXIT_ERR);
  }
  const page = await buildPageHtml();
  // Guardrails: no raw innerHTML of SVG content; sanitizer present; token IPC.
  if (/innerHTML\s*=\s*'<svg/.test(page) || /innerHTML\s*=\s*"<svg/.test(page)) {
    console.error('generated page still assigns raw SVG via innerHTML');
    process.exit(EXIT_ERR);
  }
  if (!page.includes('mountSanitizedSvgIcon')) {
    console.error('generated page missing mountSanitizedSvgIcon');
    process.exit(EXIT_ERR);
  }
  if (!page.includes('menu.__select') || !page.includes('_token')) {
    console.error('generated page missing token-based select IPC');
    process.exit(EXIT_ERR);
  }
  const next = toInl(page);
  if (args.has('--check')) {
    if (!fs.existsSync(OUT_INL)) {
      console.error('missing', OUT_INL);
      process.exit(EXIT_DRIFT);
    }
    const cur = fs.readFileSync(OUT_INL, 'utf8');
    if (cur !== next) {
      console.error('[drift] MenuOverlayPage.inl is out of date; run --write');
      process.exit(EXIT_DRIFT);
    }
    console.log('[ok] MenuOverlayPage.inl up to date');
    process.exit(EXIT_OK);
  }
  if (args.has('--write')) {
    fs.writeFileSync(OUT_INL, next, 'utf8');
    console.log('[write]', path.relative(REPO, OUT_INL), `(${next.length} bytes)`);
    process.exit(EXIT_OK);
  }
  console.error('pass --write or --check');
  process.exit(EXIT_ERR);
}

main().catch((err) => {
  console.error(err);
  process.exit(EXIT_ERR);
});
