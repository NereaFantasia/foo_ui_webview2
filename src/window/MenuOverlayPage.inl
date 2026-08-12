// AUTO-GENERATED — do not edit.
// Source: src/window/menu-overlay/{menu-overlay.html,menu-overlay.css,menu-overlay.protected.css,menu-overlay.js,menu-overlay-layout.mjs,svg-allowlist.mjs}
// Regenerate: node scripts/gen_menu_overlay_page.mjs --write
// Drift gate: node scripts/gen_menu_overlay_page.mjs --check
#pragma once
inline std::wstring BuildMenuOverlayHtmlFromSources() {
    return LR"MENUHTML(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<style id="fb-default">
  html,body{margin:0;padding:0;width:100%;height:100%;overflow:hidden;background:transparent;font:13px "Segoe UI",sans-serif;}
  #block{position:fixed;inset:0;}
  /* Visible display is NOT locked here: default block, or user flex/grid.
     Hidden state uses .fb-hidden + inline display:none !important (see layout helpers). */
  .fb-menu{position:fixed;min-width:170px;max-height:100vh;overflow:auto;background:#2b2b2b;color:#eaeaea;border:1px solid #555;border-radius:6px;padding:4px 0;box-shadow:0 6px 20px rgba(0,0,0,.45);z-index:10;}
  .fb-zone{display:block;}
  .fb-zone-separator{height:1px;background:#555;margin:4px 0;}
  .fb-item{display:flex;align-items:center;min-width:0;padding:6px 30px 6px 26px;cursor:default;white-space:nowrap;position:relative;}
  .fb-item>[part="item-label"]{min-width:0;overflow:hidden;text-overflow:ellipsis;}
  .fb-item:hover,.fb-item.active{background:#3d6fd0;color:#fff;}
  .fb-item.disabled{color:#777;}
  .fb-item.disabled:hover,.fb-item.disabled.active{background:transparent;color:#777;}
  .fb-item.checked::before{content:"\2713";position:absolute;left:8px;}
  .fb-arrow{position:absolute;right:8px;font-size:10px;opacity:.85;}
  /* Inline monochrome icon column (普通项；仅当本层有项带 iconSvg 时出现，保证文字左缘对齐)。*/
  .fb-item-ico{flex:0 0 16px;width:16px;height:16px;margin-right:8px;display:inline-flex;align-items:center;justify-content:center;}
  .fb-item-ico svg{width:16px;height:16px;display:block;fill:currentColor;}
  .fb-menu.has-icons .fb-item.nrm{padding-left:8px;}
  /* 勾选项的 ✓ 落在图标列（left:8px）；若该项自带图标则隐藏其图标，避免 ✓ 与图标重叠。*/
  .fb-menu.has-icons .fb-item.nrm.checked .fb-item-ico{visibility:hidden;}
  .fb-sep{height:1px;background:#555;margin:4px 0;}
  #probe{position:fixed;left:8px;top:8px;padding:4px 8px;font-size:11px;color:#fff;background:rgba(0,0,0,.5);border-radius:5px;pointer-events:none;opacity:.5;}
  /* ContentSized：去全覆盖拦截层；根菜单入场动画（参 QQNT，opacity+transform）。子菜单纯 CSS 在固定窗内右展，窗口全程不 resize。*/
  body.content-sized #block{display:none;}
  body.content-sized #probe{display:none;}
  body.content-sized #menu{opacity:0;transform:translateY(4px) scale(.985);transition:opacity .13s ease-out,transform .13s ease-out;}
  body.content-sized #menu.in{opacity:1;transform:none;}
  /* 退场动画（opt-in，镜像入场）：收到 menu:__hide 时 #menu 去 .in 加 .out 播淡出；前端可经 css 覆盖 #menu.out。*/
  body.content-sized #menu.out{opacity:0;transform:translateY(4px) scale(.985);transition:opacity .13s ease-out,transform .13s ease-out;}
  /* Respect prefers-reduced-motion for default enter/exit only.
     Hidden protocol / closeAnimationMs are unchanged. Not in protected CSS. */
  @media (prefers-reduced-motion: reduce){
    body.content-sized #menu,
    body.content-sized #menu.in,
    body.content-sized #menu.out{
      transition:none !important;
      transform:none !important;
    }
  }
  /* 富菜单项（仅 webview 自绘）：nowplaying 歌曲卡 / rating 星级 / slider 音量滑块。*/
  .fb-np{display:flex;align-items:center;gap:10px;padding:8px 12px;cursor:default;}
  .fb-np-cover{width:40px;height:40px;flex:0 0 auto;border-radius:4px;background:#444 center/cover no-repeat;}
  .fb-np-text{min-width:0;overflow:hidden;}
  .fb-np-title{font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:240px;}
  .fb-np-sub{margin-top:2px;opacity:.65;font-size:12px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:240px;}
  .fb-rating,.fb-slider{display:grid;grid-template-columns:minmax(0,auto) minmax(96px,1fr) minmax(26px,auto);align-items:center;gap:8px;min-width:0;}
  .fb-rating-label,.fb-slider-label{min-width:0;overflow:hidden;text-overflow:ellipsis;opacity:.85;}
  .fb-stars{margin-left:auto;display:inline-flex;gap:2px;}
  .fb-star{color:#666;cursor:pointer;font-size:15px;line-height:1;}
  .fb-star.on{color:#ffcc44;}
  .fb-slider-track{position:relative;display:block;min-width:96px;height:4px;margin:0 2px;background:#555;border-radius:2px;cursor:pointer;touch-action:none;}
  .fb-slider-fill{position:absolute;left:0;top:0;height:100%;background:#3d6fd0;border-radius:2px;}
  .fb-slider-thumb{position:absolute;top:50%;width:10px;height:10px;margin-left:-5px;border-radius:50%;background:#fff;transform:translateY(-50%);box-shadow:0 0 2px rgba(0,0,0,.5);}
  .fb-slider-val{flex:0 0 auto;min-width:26px;text-align:right;opacity:.8;}
  /* Vertical slider: track grows tall; fill from bottom; thumb on bottom axis. */
  .fb-slider[data-orientation="vertical"]{display:grid;grid-template-columns:minmax(0,1fr) minmax(20px,auto) minmax(26px,auto);align-items:center;min-height:112px;}
  .fb-slider[data-orientation="vertical"] .fb-slider-track{--fb-slider-visual-width:4px;justify-self:center;min-width:20px;width:20px;min-height:88px;height:88px;margin:4px 8px;background:linear-gradient(90deg,transparent 8px,#555 8px,#555 12px,transparent 12px);}
  .fb-slider[data-orientation="vertical"] .fb-slider-control{position:absolute;inset:-6px;}
  .fb-slider[data-orientation="vertical"] .fb-slider-fill{width:var(--fb-slider-visual-width);left:50%;bottom:0;top:auto;height:0;transform:translateX(-50%);}
  .fb-slider[data-orientation="vertical"] .fb-slider-thumb{left:50%;top:auto;bottom:0;margin-left:0;margin-bottom:-5px;transform:translateX(-50%);}
  /* segmented 分段单选（仅 webview 自绘）：一行互斥选项；选中段用强调色高亮。各段可放 svg 图标或文字。*/
  .fb-seg{display:grid;grid-template-columns:minmax(0,auto) minmax(0,1fr);align-items:center;gap:10px;min-width:0;}
  .fb-seg-label{min-width:0;overflow:hidden;text-overflow:ellipsis;opacity:.85;}
  .fb-seg-group{min-width:0;display:flex;flex-wrap:wrap;justify-content:flex-end;gap:2px;}
  .fb-seg-btn{display:inline-flex;min-width:0;max-width:120px;align-items:center;justify-content:center;padding:2px 8px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;cursor:pointer;border-radius:4px;color:#cfcfcf;}
  .fb-seg-btn svg{width:16px;height:16px;display:block;fill:currentColor;}
  .fb-seg-btn.on{background:#3d6fd0;color:#fff;}
  .fb-seg-btn.disabled{co)MENUHTML"
        LR"MENUHTML(lor:#666;cursor:default;}
  /* 富项 hover/active 用克制的灰底（覆盖上面 .fb-item 蓝底），保留控件辨识度。*/
  .fb-np:hover,.fb-np.active,.fb-rating:hover,.fb-rating.active,.fb-slider:hover,.fb-slider.active,.fb-seg:hover,.fb-seg.active{background:#3a3a3a;color:#eaeaea;}
</style>
<!-- Frontend style takeover: default(fb-default) -> user(fb-user) -> protected(fb-protected). -->
<style id="fb-user"></style>
<style id="fb-protected">
  /* Protected structural layer: viewport geometry, menu
     box model / fixed positioning / overflow, and hidden fallback only.
     Visible display (block/flex/grid) is owned by default CSS or user CSS. */
  html,body{margin:0!important;padding:0!important;width:100%!important;height:100%!important;overflow:hidden!important;box-sizing:border-box!important;}
  #viewport{position:fixed!important;inset:0!important;overflow:hidden!important;}
  .fb-menu{box-sizing:border-box!important;position:fixed!important;overflow:auto!important;}
  #menu.fb-hidden,.fb-menu.fb-hidden{display:none!important;}
  /* Internal infra (debug probe / fullscreen intercept) — content-sized only. */
  body.content-sized #block{display:none!important;}
  body.content-sized #probe{display:none!important;}
</style></head>
<body>
<div id="block"></div>
  <div id="viewport">
    <div id="menu" class="fb-menu fb-hidden" role="menu" data-depth="0" part="root"></div>
  </div>
  <div id="probe">self-drawn-menu</div>
<script>
// SVG allowlist predicates for the self-drawn menu icon sanitizer (DESIGN 8.4).
// Pure logic only — no DOM. The overlay page inlines a browser build of these
// predicates plus a DOMParser-based cloner; node:test imports this module
// directly. Keep both paths in sync via scripts/gen_menu_overlay_page.mjs.

const ALLOWED_ELEMENTS = Object.freeze([
  'path', 'circle', 'rect', 'line', 'polyline', 'polygon', 'ellipse', 'g',
]);

const COMMON_ATTRS = Object.freeze([
  'fill', 'fill-rule', 'fill-opacity', 'stroke', 'stroke-width',
  'stroke-linecap', 'stroke-linejoin', 'stroke-opacity', 'opacity', 'transform',
]);

const ELEMENT_ATTRS = Object.freeze({
  path: ['d'],
  circle: ['cx', 'cy', 'r'],
  ellipse: ['cx', 'cy', 'rx', 'ry'],
  rect: ['x', 'y', 'width', 'height', 'rx', 'ry'],
  line: ['x1', 'y1', 'x2', 'y2'],
  polyline: ['points'],
  polygon: ['points'],
  g: [],
});

const MAX_ABS = 1e6;
const MAX_SINGLE_SVG_BYTES = 32 * 1024;

function isAllowedElement(name) {
  if (!name) return false;
  return ALLOWED_ELEMENTS.includes(String(name).toLowerCase());
}

function isAllowedAttribute(elementName, attrName) {
  const el = String(elementName || '').toLowerCase();
  const attr = String(attrName || '').toLowerCase();
  if (!attr || attr.startsWith('on')) return false;
  if (attr === 'href' || attr === 'xlink:href' || attr.includes(':')) return false;
  if (COMMON_ATTRS.includes(attr)) return true;
  const extras = ELEMENT_ATTRS[el];
  return !!(extras && extras.includes(attr));
}

function isFiniteInRange(n) {
  return typeof n === 'number' && Number.isFinite(n) && Math.abs(n) <= MAX_ABS;
}

function parseNumberToken(s) {
  if (s == null) return null;
  const t = String(s).trim();
  if (!t) return null;
  if (!/^[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?$/.test(t)) return null;
  const n = Number(t);
  return isFiniteInRange(n) ? n : null;
}

function isValidViewBox(value) {
  if (value == null) return false;
  const parts = String(value).trim().split(/[\s,]+/).filter(Boolean);
  if (parts.length !== 4) return false;
  const nums = parts.map(parseNumberToken);
  if (nums.some((n) => n == null)) return false;
  const [, , w, h] = nums;
  return w > 0 && h > 0;
}

function isValidOpacity(value) {
  const n = parseNumberToken(value);
  return n != null && n >= 0 && n <= 1;
}

function isValidFillOrStroke(value) {
  if (value == null) return false;
  const v = String(value).trim().toLowerCase();
  if (v.includes('url(')) return false;
  return v === 'currentcolor' || v === 'none';
}

function isValidFillRule(value) {
  const v = String(value || '').trim().toLowerCase();
  return v === 'nonzero' || v === 'evenodd';
}

function isValidLinecap(value) {
  const v = String(value || '').trim().toLowerCase();
  return v === 'butt' || v === 'round' || v === 'square';
}

function isValidLinejoin(value) {
  const v = String(value || '').trim().toLowerCase();
  return v === 'miter' || v === 'round' || v === 'bevel';
}

function isValidNumericAttr(value) {
  return parseNumberToken(value) != null;
}

/** d / points: path commands + numbers + commas + whitespace only; all numbers in range. */
function isValidPathData(value) {
  if (value == null) return false;
  const s = String(value);
  if (!/^[MmZzLlHhVvCcSsQqTtAa0-9eE+.,\s\-]*$/.test(s)) return false;
  const nums = s.match(/[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?/g) || [];
  for (const tok of nums) {
    if (parseNumberToken(tok) == null) return false;
  }
  return true;
}

/** SVG transform function arity (DESIGN 8.4). rotate allows 1 or 3 only. */
const TRANSFORM_ARITY = Object.freeze({
  matrix: { min: 6, max: 6 },
  translate: { min: 1, max: 2 },
  scale: { min: 1, max: 2 },
  rotate: { min: 1, max: 3 },
  skewx: { min: 1, max: 1 },
  skewy: { min: 1, max: 1 },
});

function isTransformWsp(ch) {
  return ch === ' ' || ch === '\t' || ch === '\n' || ch === '\r' || ch === '\f';
}

/**
 * Strict transform allowlist: only matrix/translate/scale/rotate/skewX/skewY
 * with finite in-range args and correct arity. Rejects leading junk, junk or
 * commas between functions, empty argument lists, and wrong arity.
 */
function isValidTransform(value) {
  if (value == null) return false;
  const s = String(value).trim();
  if (!s) return false;
  if (/url\s*\(/i.test(s)) return false;

  let i = 0;
  const n = s.length;
  let sawOne = false;

  const skipWsp = () => {
    while (i < n && isTransformWsp(s[i])) i += 1;
  };

  while (i < n) {
    skipWsp();
    if (i >= n) break;

    const nameStart = i;
    if (!/[A-Za-z]/.test(s[i])) return false;
    i += 1;
    while )MENUHTML"
        LR"MENUHTML((i < n && /[A-Za-z]/.test(s[i])) i += 1;
    const name = s.slice(nameStart, i).toLowerCase();
    const arity = TRANSFORM_ARITY[name];
    if (!arity) return false;

    skipWsp();
    if (i >= n || s[i] !== '(') return false;
    i += 1;
    skipWsp();

    const args = [];
    if (i < n && s[i] !== ')') {
      for (;;) {
        const numStart = i;
        if (i < n && (s[i] === '+' || s[i] === '-')) i += 1;
        let sawDigit = false;
        while (i < n && s[i] >= '0' && s[i] <= '9') {
          sawDigit = true;
          i += 1;
        }
        if (i < n && s[i] === '.') {
          i += 1;
          while (i < n && s[i] >= '0' && s[i] <= '9') {
            sawDigit = true;
            i += 1;
          }
        }
        if (!sawDigit) return false;
        if (i < n && (s[i] === 'e' || s[i] === 'E')) {
          i += 1;
          if (i < n && (s[i] === '+' || s[i] === '-')) i += 1;
          const expStart = i;
          while (i < n && s[i] >= '0' && s[i] <= '9') i += 1;
          if (i === expStart) return false;
        }
        const tok = s.slice(numStart, i);
        if (parseNumberToken(tok) == null) return false;
        args.push(tok);

        skipWsp();
        if (i < n && s[i] === ',') {
          i += 1;
          skipWsp();
          if (i >= n || s[i] === ')') return false; // trailing comma
          continue;
        }
        // Whitespace-separated next number (SVG allows comma-wsp or wsp).
        if (
          i < n &&
          s[i] !== ')' &&
          (s[i] === '+' || s[i] === '-' || s[i] === '.' || (s[i] >= '0' && s[i] <= '9'))
        ) {
          continue;
        }
        break;
      }
    }

    if (i >= n || s[i] !== ')') return false;
    i += 1;

    if (args.length < arity.min || args.length > arity.max) return false;
    // SVG rotate is 1 or 3 parameters — never 2.
    if (name === 'rotate' && args.length === 2) return false;

    sawOne = true;
    const afterClose = i;
    skipWsp();
    // Between functions: at least one whitespace is required (reject zero-sep
    // forms like translate(1)scale(2)). Commas / junk between functions are
    // also rejected — only wsp separators are allowed.
    if (i < n) {
      if (i === afterClose) return false;
      if (!/[A-Za-z]/.test(s[i])) return false;
    }
  }

  return sawOne;
}

/** Live-DOM SVG nodes must be in the SVG namespace (empty NS rejected). */
function isSvgNamespace(uri) {
  return uri === 'http://www.w3.org/2000/svg';
}

function isValidAttributeValue(elementName, attrName, value) {
  const attr = String(attrName || '').toLowerCase();
  if (!isAllowedAttribute(elementName, attr)) return false;
  if (value == null) return false;
  const v = String(value);
  if (/url\s*\(/i.test(v)) return false;
  switch (attr) {
    case 'fill':
    case 'stroke':
      return isValidFillOrStroke(v);
    case 'fill-rule':
      return isValidFillRule(v);
    case 'fill-opacity':
    case 'stroke-opacity':
    case 'opacity':
      return isValidOpacity(v);
    case 'stroke-linecap':
      return isValidLinecap(v);
    case 'stroke-linejoin':
      return isValidLinejoin(v);
    case 'd':
    case 'points':
      return isValidPathData(v);
    case 'transform':
      return isValidTransform(v);
    default:
      return isValidNumericAttr(v);
  }
}

function isOversizedSvgContent(content) {
  return String(content || '').length > MAX_SINGLE_SVG_BYTES;
}

/** Structural reject list for raw content before DOM parse (defense in depth). */
function contentLooksUnsafe(content) {
  const s = String(content || '');
  if (!s) return true;
  if (isOversizedSvgContent(s)) return true;
  if (/<\s*(script|foreignObject|style|animate|animateTransform|animateMotion|set)\b/i.test(s)) {
    return true;
  }
  if (/\son[a-z]+\s*=/i.test(s)) return true;
  if (/\b(xlink:)?href\s*=/i.test(s)) return true;
  if (/url\s*\(/i.test(s)) return true;
  return false;
}

/**
 * Browser-side helper source (no export). Generator inlines this after the
 * predicate functions so the overlay can mount a sanitized SVG without
 * raw innerHTML of untrusted content.
 */

function mountSanitizedSvgIcon(host, viewBox, content) {
  if (!host || contentLooksUnsafe(content)) return false;
  if (!isValidViewBox(viewBox || '0 0 24 24')) return false;
  var vb = viewBox || '0 0 24 24';
  var wrapped = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="' +
    String(vb).replace(/&/g,'&amp;').replace(/"/g,'&quot;').replace(/</g,'&lt;') +
    '">' + content + '</svg>';
  var parser = new DOMParser();
  var doc = parser.parseFromString(wrapped, 'image/svg+xml');
  if (!doc || doc.querySelector('parsererror')) return false;
  var src = doc.documentElement;
  if (!src || String(src.localName || '').toLowerCase() !== 'svg') return false;
  if (!isSvgNamespace(src.namespaceURI)) return false;

  function cloneAllowed(from, into) {
    for (var i = 0; i < from.childNodes.length; i++) {
      var node = from.childNodes[i];
      if (node.nodeType === 3 || node.nodeType === 4) { // text / cdata
        if (String(node.nodeValue || '').trim() !== '') return false;
        continue;
      }
      if (node.nodeType === 8) continue; // comment
      if (node.nodeType !== 1) return false;
      var name = String(node.localName || '').toLowerCase();
      if (!isAllowedElement(name)) return false;
      if (!isSvgNamespace(node.namespaceURI)) return false;
      var out = document.createElementNS('http://www.w3.org/2000/svg', name);
      if (node.attributes) {
        for (var a = 0; a < node.attributes.length; a++) {
          var at = node.attributes[a];
          var an = String(at.name || '').toLowerCase();
          if (an.indexOf('xmlns') === 0) continue;
          if (!isValidAttributeValue(name, an, at.value)) return false;
          out.setAttribute(an, at.value);
        }
      }
      if (!cloneAllowed(node, out)) return false;
      into.appendChild(out);
    }
    return true;
  }

  var svg = document.createElementNS('http://www.w3)MENUHTML"
        LR"MENUHTML(.org/2000/svg', 'svg');
  svg.setAttribute('viewBox', vb);
  svg.setAttribute('width', '16');
  svg.setAttribute('height', '16');
  svg.setAttribute('aria-hidden', 'true');
  if (!cloneAllowed(src, svg)) return false;
  host.textContent = '';
  host.appendChild(svg);
  return true;
}

// Pure layout / DOM-model helpers for the self-drawn menu overlay.
// Node tests import this module directly. The overlay page inlines a stripped
// browser build via scripts/gen_menu_overlay_page.mjs — keep both paths in sync.

const ZONE_IDS = Object.freeze(['top', 'playback', 'bottom']);

/** Resolve public data-kind for an item payload. */
function resolveItemKind(it) {
  if (!it || typeof it !== 'object') return 'normal';
  const t = it.type;
  if (t === 'separator') return 'separator';
  if (t === 'nowplaying') return 'nowplaying';
  if (t === 'rating') return 'rating';
  if (t === 'slider') return 'slider';
  if (t === 'segmented') return 'segmented';
  if (t === 'submenu' || (it.submenu && it.submenu.length)) return 'submenu';
  return 'normal';
}

/**
 * Stable public CSS hooks for an item node. Does not invent public ids.
 * `data-item-token` is internal (single-show opaque identity) — not a public contract.
 */
function buildItemStableAttrs(it, { depth = 0, zone = undefined } = {}) {
  const kind = resolveItemKind(it);
  const attrs = {
    'data-kind': kind,
    'data-depth': String(depth),
  };
  if (zone != null && zone !== '') attrs['data-zone'] = String(zone);
  if (it && typeof it.id === 'string' && it.id.length > 0) {
    attrs['data-item-id'] = it.id;
  }
  if (it && typeof it._token === 'string' && it._token.length > 0) {
    attrs['data-item-token'] = it._token;
  }
  return attrs;
}

/** Separator attrs (role=separator + zone/depth). */
function buildSeparatorStableAttrs({ depth = 0, zone = undefined, zoneSeparator = false } = {}) {
  const attrs = {
    role: 'separator',
    'data-kind': 'separator',
    'data-depth': String(depth),
  };
  if (zone != null && zone !== '') attrs['data-zone'] = String(zone);
  if (zoneSeparator) attrs['data-zone-separator'] = '1';
  return attrs;
}

/**
 * Hide protocol: class first, then inline display:none !important.
 * Visible state must NOT write inline display.
 */
function hideMenuProtocol() {
  return Object.freeze([
    { op: 'addClass', className: 'fb-hidden' },
    { op: 'setProperty', name: 'display', value: 'none', priority: 'important' },
  ]);
}

/** Show protocol: remove inline display first, then clear .fb-hidden. */
function showMenuProtocol() {
  return Object.freeze([
    { op: 'removeProperty', name: 'display' },
    { op: 'removeClass', className: 'fb-hidden' },
  ]);
}

/**
 * Build a virtual DOM tree for flat / zones / legacy menu.show models.
 * Nodes: { tag, id?, className, attrs, children, text? }
 */
function buildMenuDocumentModel({
  layoutMode = 'flat',
  model = 'legacyItems',
  items = [],
  zones = [],
  depth = 0,
} = {}) {
  const menuAttrs = { role: 'menu', 'data-depth': String(depth) };
  const menu = {
    tag: 'div',
    id: 'menu',
    className: 'fb-menu fb-hidden',
    attrs: menuAttrs,
    children: [],
  };

  const useZones = model === 'trayZones' && layoutMode === 'zones';

  if (useZones) {
    const present = (zones || []).filter((z) => z && Array.isArray(z.items) && z.items.length > 0);
    for (let i = 0; i < present.length; i++) {
      if (i > 0) {
        const prevZone = present[i - 1].id || ZONE_IDS[i - 1] || 'top';
        menu.children.push({
          tag: 'div',
          className: 'fb-sep fb-zone-separator',
          attrs: buildSeparatorStableAttrs({ depth, zone: prevZone, zoneSeparator: true }),
          children: [],
        });
      }
      const z = present[i];
      const zoneId = z.id || ZONE_IDS[i] || 'top';
      const zoneEl = {
        tag: 'div',
        className: 'fb-zone',
        attrs: { 'data-zone': zoneId },
        children: buildItemNodes(z.items, { depth, zone: zoneId }),
      };
      menu.children.push(zoneEl);
    }
  } else {
    menu.children = buildItemNodes(items || [], { depth, zoneFromItem: true });
  }

  return {
    tag: 'div',
    id: 'viewport',
    className: '',
    attrs: {},
    children: [menu],
  };
}

function buildItemNodes(items, { depth, zone, zoneFromItem = false }) {
  const out = [];
  for (const it of items || []) {
    if (!it) continue;
    const itemZone = zoneFromItem ? (it._zone || zone) : zone;
    if (it.type === 'separator') {
      out.push({
        tag: 'div',
        className: 'fb-sep',
        attrs: buildSeparatorStableAttrs({ depth, zone: itemZone }),
        children: [],
      });
      continue;
    }
    const kind = resolveItemKind(it);
    let className = 'fb-item';
    if (kind === 'nowplaying') className += ' fb-np';
    else if (kind === 'rating') className += ' fb-rating';
    else if (kind === 'slider') className += ' fb-slider';
    else if (kind === 'segmented') className += ' fb-seg';
    else className += ' nrm';
    const node = {
      tag: 'div',
      className,
      attrs: buildItemStableAttrs(it, { depth, zone: itemZone }),
      children: [],
    };
    out.push(node);
  }
  return out;
}

/** Direct-child `.fb-item` nodes under #menu (flat / legacy contract). */
function menuDirectItems(doc) {
  const menu = findById(doc, 'menu');
  if (!menu) return [];
  return (menu.children || []).filter((c) => hasClass(c, 'fb-item'));
}

/** Direct-child separators under #menu (excludes zone separators). */
function menuDirectSeparators(doc) {
  const menu = findById(doc, 'menu');
  if (!menu) return [];
  return (menu.children || []).filter((c) => hasClass(c, 'fb-sep') && !hasClass(c, 'fb-zone-separator'));
}

function menuZones(doc) {
  const menu = findById(doc, 'menu');
  if (!menu) return [];
  return (menu.children || []).filter((c) => hasClass(c, 'fb-zone'));
}

function hasClass(node, name) {
  const parts = String(node && node.className || '').split(/\s+/).filter(Boolean);
  return parts.includes(name)MENUHTML"
        LR"MENUHTML();
}

function findById(node, id) {
  if (!node) return null;
  if (node.id === id) return node;
  for (const c of node.children || []) {
    const hit = findById(c, id);
    if (hit) return hit;
  }
  return null;
}

/** Source-file contract checks for protected CSS. */
function analyzeProtectedCss(css) {
  const text = String(css || '');
  return {
    hasViewportRule: /#viewport\s*\{/.test(text),
    forcesVisibleBlock: /#menu:not\(\.fb-hidden\)\s*\{[^}]*display\s*:\s*block\s*!important/.test(text),
    hasHiddenFallback: /\.fb-hidden[^}]*display\s*:\s*none\s*!important|#menu\.fb-hidden[^}]*display\s*:\s*none\s*!important/.test(text),
    hasMenuBoxModel: /\.fb-menu\s*\{[^}]*box-sizing\s*:\s*border-box\s*!important/.test(text),
  };
}

/** Default visual CSS must not lock .fb-menu to display:none. */
function analyzeDefaultCss(css) {
  const text = String(css || '');
  return {
    locksMenuDisplayNone: /\.fb-menu\s*\{[^}]*display\s*:\s*none/.test(text),
  };
}

/** Default rich-control CSS must provide usable geometry without user CSS. */
function analyzeRichControlCss(css) {
  const text = String(css || '');
  const verticalRow = text.match(/\.fb-slider\[data-orientation=["']vertical["']\]\s*\{([^}]*)\}/);
  const verticalTrack = text.match(/\.fb-slider\[data-orientation=["']vertical["']\]\s+\.fb-slider-track\s*\{([^}]*)\}/);
  const verticalFill = text.match(/\.fb-slider\[data-orientation=["']vertical["']\]\s+\.fb-slider-fill\s*\{([^}]*)\}/);
  const verticalThumb = text.match(/\.fb-slider\[data-orientation=["']vertical["']\]\s+\.fb-slider-thumb\s*\{([^}]*)\}/);
  const segmentedRow = text.match(/\.fb-seg\s*\{([^}]*)\}/);
  const segmentedGroup = text.match(/\.fb-seg-group\s*\{([^}]*)\}/);
  const segmentedButton = text.match(/\.fb-seg-btn\s*\{([^}]*)\}/);
  return {
    verticalUsesGrid: !!verticalRow && /display\s*:\s*grid/.test(verticalRow[1]),
    verticalHasTallTrack: !!verticalTrack
      && /min-height\s*:\s*(?:[6-9]\d|\d{3,})px/.test(verticalTrack[1])
      && /min-width\s*:\s*(?:1[2-9]|[2-9]\d|\d{3,})px/.test(verticalTrack[1]),
    verticalVisualTrackIsNarrow: !!verticalTrack
      && /--fb-slider-visual-width\s*:\s*(?:[2-8](?:\.\d+)?)px/.test(verticalTrack[1]),
    verticalFillUsesVisualWidth: !!verticalFill
      && /width\s*:\s*var\(--fb-slider-visual-width\)/.test(verticalFill[1])
      && /left\s*:\s*50%/.test(verticalFill[1])
      && /translateX\(-50%\)/.test(verticalFill[1]),
    verticalThumbCentered: !!verticalThumb
      && /left\s*:\s*50%/.test(verticalThumb[1])
      && /translateX\(-50%\)/.test(verticalThumb[1]),
    segmentedCanWrap: !!segmentedGroup && /flex-wrap\s*:\s*wrap/.test(segmentedGroup[1]),
    segmentedCanShrink: !!segmentedRow && /min-width\s*:\s*0/.test(segmentedRow[1])
      && !!segmentedGroup && /min-width\s*:\s*0/.test(segmentedGroup[1]),
    segmentedButtonsBoundText: !!segmentedButton
      && /max-width\s*:/.test(segmentedButton[1])
      && /text-overflow\s*:\s*ellipsis/.test(segmentedButton[1]),
  };
}

/**
 * Convert physical placement geometry from C++ to CSS-pixel constraints.
 * Missing geometry keeps the legacy root-at-origin fallback.
 */
function normalizePlacedGeometry(ev, dpr = 1) {
  const scale = Number.isFinite(+dpr) && +dpr > 0 ? +dpr : 1;
  const g = ev && ev.geometry;
  if (!g || typeof g !== 'object') return null;
  const read = (key) => Number.isFinite(+g[key]) && +g[key] >= 0 ? +g[key] / scale : null;
  const viewportW = read('viewportW');
  const viewportH = read('viewportH');
  const rootSlotW = read('rootSlotW');
  const subSlotW = read('subSlotW');
  const rootTop = read('rootTop');
  if (viewportW == null || viewportH == null || rootSlotW == null
      || subSlotW == null || rootTop == null) return null;
  const virtualViewportH = read('virtualViewportH');
  const virtualRootTop = read('virtualRootTop');
  return {
    viewportW,
    viewportH,
    rootSlotW,
    subSlotW,
    rootTop,
    virtualViewportH: virtualViewportH == null ? viewportH : virtualViewportH,
    virtualRootTop: virtualRootTop == null ? rootTop : virtualRootTop,
  };
}

/**
 * Place an independently hosted first-level submenu in the original virtual
 * content-sized canvas. The root HWND is tight, so its screen origin is
 * virtualRootTop pixels below that canvas origin.
 */
function submenuWindowPlacement({ rootSlotW, viewportH, virtualViewportH, virtualRootTop, parentTop, submenuHeight }) {
  const canvasH = Math.max(0, Number(virtualViewportH ?? viewportH) || 0);
  const height = Math.max(0, Math.min(Math.max(0, Number(submenuHeight) || 0), canvasH));
  const desiredTop = Math.max(0, Number(virtualRootTop) || 0) + Math.max(0, Number(parentTop) || 0);
  const top = Math.max(0, Math.min(desiredTop, Math.max(0, canvasH - height)));
  return {
    x: Math.max(0, Number(rootSlotW) || 0),
    y: top,
    h: height,
  };
}

/**
 * ArrowLeft navigation policy. An independently hosted submenu begins at
 * depth zero, unlike an in-DOM submenu layer, so it must explicitly return to
 * its root surface instead of becoming a no-op at depth zero.
 */
function resolveMenuArrowLeftAction({ windowModel, depth } = {}) {
  if (windowModel === 'submenu') return { kind: 'dismissSubmenu', reason: 'submenu-left' };
  return Number.isFinite(+depth) && +depth > 0
    ? { kind: 'closeLayer' }
    : { kind: 'noop' };
}

/** Source-level contract for the independent first-level submenu surface. */
function analyzeIndependentSubmenuWindowContract(source) {
  const text = String(source || '');
  const arrowLeftMatch = text.match(/case "ArrowLeft":\s*\{([\s\S]*?)case "Enter"/);
  const arrowLeftBody = arrowLeftMatch ? arrowLeftMatch[1] : '';
  const escapeMatch = text.match(/case "Escape":\s*([\s\S]*?)default:/);
  const escapeBody = escapeMatch ? escapeMatch[1] : '';
  return {
    reportsMonotonicSequence: /submenuPanelSequence\s*=\s*0/.test(text)
      && /sequence\s*:\s*\+\+submenuPanelSequence/.test(text),
    reportsParentToken: /parentToken\s*:\s*\(L\.rows\[rowIdx)MENUHTML"
        LR"MENUHTML(\]\.item&&L\.rows\[rowIdx\]\.item\._token\)/.test(text),
    computesVirtualWindowPlacement: /submenuWindowPlacement\(\{[\s\S]*?virtualRootTop:geom\.virtualRootTop/.test(text),
    treatsSubmenuAsLeafSurface: /cur\.windowModel==="submenu"\) return/.test(text),
    reconcilesNativeSubmenuClose: /fb2k\.on\("menu:__submenuClosed"/.test(text),
    waitsForNativeVisibilityBeforeFocus: /fb2k\.on\("menu:__submenuVisible"[\s\S]*?setActive\(0,firstNav\(0\),\{focus:true\}\)/.test(text),
    announcesSurfaceReadiness: /fb2k\.invoke\("menu\.__submenuPanel"[\s\S]*?ready:true/.test(text),
    submenuArrowLeftReturnsToRoot: /resolveMenuArrowLeftAction\(\{windowModel:cur&&cur\.windowModel,depth:depth\}\)/.test(arrowLeftBody)
      && /leftAction\.kind==="dismissSubmenu"[\s\S]*?dismiss\(leftAction\.reason\)[\s\S]*?e\.preventDefault\(\)/.test(arrowLeftBody),
    submenuEscapeRemainsWholeSurfaceDismiss: /else\s*\{\s*dismiss\("escape"\);\s*\}/.test(escapeBody),
  };
}

/**
 * Source structure contract for root close: the child HWND must be hidden and
 * invalidated before the root chooses its animated or immediate branch, and
 * stale child callbacks must be blocked while the root is closing.
 */
function analyzeRootCloseSubmenuPolicy(source) {
  const text = String(source || '');
  const hideMatch = text.match(/void MenuOverlayHost::Hide\(const std::string& reason\)\s*\{([\s\S]*?)\n\}/);
  const hideBody = hideMatch ? hideMatch[1] : '';
  const policyIdx = hideBody.indexOf('ResolveRootClosePolicy(');
  const hideChildIdx = hideBody.indexOf('HideSubmenuWindow(closePolicy.restoreRootFocus');
  const animatedBranchIdx = hideBody.indexOf('if (closePolicy.animateRoot)');
  return {
    resolvesRootClosePolicy: policyIdx >= 0,
    hidesChildBeforeRootCloseBranch: hideChildIdx >= 0 && animatedBranchIdx > hideChildIdx,
    childCloseAvoidsFocusOrRootNotification: /HideSubmenuWindow\(closePolicy\.restoreRootFocus, closePolicy\.notifyRootRenderer,\s*closePolicy\.clearSubmenuStateImmediately\)/.test(hideBody),
    rejectsPanelUpdatesWhileClosing: /bool MenuOverlayHost::OnSubmenuPanelChanged[\s\S]*?if \(!visible_ \|\| closing_\)/.test(text),
    rejectsReadyWhileClosing: /bool MenuOverlayHost::OnSubmenuSurfaceReady[\s\S]*?!visible_ \|\| closing_/.test(text),
  };
}

/** Default style for the reserved submenu HWND slot: non-visible, non-hit-test. */
function hiddenSubmenuSlotStyle(rootSlotW, viewportH) {
  return {
    left: `${Math.max(0, rootSlotW)}px`,
    top: '0px',
    width: '0px',
    height: `${Math.max(0, viewportH)}px`,
    pointerEvents: 'none',
  };
}

/** Visible submenu panel constraints inside the reserved slot. */
function visibleSubmenuStyle({ rootSlotW, subSlotW, viewportH, parentTop, submenuHeight }) {
  const maxTop = Math.max(0, viewportH - Math.min(Math.max(0, submenuHeight), viewportH));
  const top = Math.max(0, Math.min(Math.max(0, parentTop), maxTop));
  return {
    left: `${Math.max(0, rootSlotW)}px`,
    top: `${top}px`,
    maxWidth: `${Math.max(0, subSlotW)}px`,
    maxHeight: `${Math.max(0, viewportH)}px`,
  };
}

/** Inline-important geometry owned by the ContentSized host. */
function contentSizedGeometryPlan({ left, top, maxWidth, maxHeight }) {
  return Object.freeze([
    { op: 'setProperty', name: 'left', value: String(left), priority: 'important' },
    { op: 'setProperty', name: 'top', value: String(top), priority: 'important' },
    { op: 'setProperty', name: 'max-width', value: String(maxWidth), priority: 'important' },
    { op: 'setProperty', name: 'max-height', value: String(maxHeight), priority: 'important' },
    { op: 'setProperty', name: 'min-width', value: '0px', priority: 'important' },
    { op: 'setProperty', name: 'overflow', value: 'auto', priority: 'important' },
  ]);
}

/** Remove every ContentSized-owned property without touching display/theme layout. */
function clearContentSizedGeometryPlan() {
  return Object.freeze([
    { op: 'removeProperty', name: 'left' },
    { op: 'removeProperty', name: 'top' },
    { op: 'removeProperty', name: 'max-width' },
    { op: 'removeProperty', name: 'max-height' },
    { op: 'removeProperty', name: 'min-width' },
    { op: 'removeProperty', name: 'overflow' },
  ]);
}

/** Source contract: apply/clear helpers must own geometry, never visible display. */
function analyzeContentSizedGeometryContract(source) {
  const text = String(source || '');
  const applyMatch = text.match(/function applyContentSizedGeometry\s*\([^)]*\)\s*\{([\s\S]*?)\n\s*\}/);
  const clearMatch = text.match(/function clearContentSizedGeometry\s*\([^)]*\)\s*\{([\s\S]*?)\n\s*\}/);
  const applyBody = applyMatch ? applyMatch[1] : '';
  const clearBody = clearMatch ? clearMatch[1] : '';
  const required = ['left', 'top', 'max-width', 'max-height', 'min-width', 'overflow'];
  return {
    hasApplyHelper: !!applyMatch,
    hasClearHelper: !!clearMatch,
    appliesImportantGeometry: required.every((name) =>
      new RegExp(`setProperty\\(\\s*["']${name}["'][^\\n]*["']important["']`).test(applyBody)),
    clearsGeometry: required.every((name) =>
      new RegExp(`removeProperty\\(\\s*["']${name}["']\\s*\\)`).test(clearBody)),
    forcesVisibleDisplay: /setProperty\(\s*["']display["']/.test(applyBody),
    renderClearsBeforeModeBranch: /function render\(st\)\{[\s\S]*?clearContentSizedGeometry\(root\)[\s\S]*?var content\s*=/.test(text),
    rootUsesHelper: /function applyPlacedGeometry\(ev\)[\s\S]*?applyContentSizedGeometry\(root\s*,/.test(text),
    submenuUsesHelper: /function openSub\(parentDepth, rowIdx\)[\s\S]*?if\(content\)[\s\S]*?applyContentSizedGeometry\(sub\s*,/.test(text),
  };
}

/** HTML shell must wrap #menu in #viewport. */
function analyzeHtmlShell(html) {
  const text = String(html || '');
  const hasViewport = /id\s*=\s*["']viewport["']/.test(text);
  const menuInsideViewport = /id\s*=\s*["']viewport["'][\s\S]*id\s*=\s*["']menu["']/.test(text);
  return { hasViewport, menuInsideViewport };
}

// ── Slider orientation / range / paint / pointer)MENUHTML"
        LR"MENUHTML( / keyboard ─────────────────

/**
 * Resolve slider orientation. Only exact "vertical" is vertical; missing /
 * "horizontal" / unknown → "horizontal". Non-slider kinds still return
 * horizontal so callers can safely stamp data-orientation only on sliders.
 */
function resolveSliderOrientation(it) {
  if (it && it.orientation === 'vertical') return 'vertical';
  return 'horizontal';
}

/**
 * Normalize slider range: swap max<min; clamp value; detect constant.
 * Returns { min, max, value, constant, orientation }.
 */
function normalizeSliderRange(it = {}) {
  let min = Number.isFinite(+it.min) ? Math.round(+it.min) : 0;
  let max = Number.isFinite(+it.max) ? Math.round(+it.max) : 100;
  if (max < min) {
    const t = min; min = max; max = t;
  }
  let value = Number.isFinite(+it.value) ? Math.round(+it.value) : min;
  if (value < min) value = min;
  if (value > max) value = max;
  return {
    min,
    max,
    value,
    constant: max === min,
    orientation: resolveSliderOrientation(it),
  };
}

/**
 * Map a pointer client coordinate to a slider value.
 * Horizontal: min left, max right (clientX / width).
 * Vertical: min bottom, max top (clientY / height).
 */
function sliderValueFromPointer({ orientation, min, max, clientX, clientY, rect }) {
  if (!rect || max === min) return min;
  let p;
  if (orientation === 'vertical') {
    const h = rect.height || 1;
    // bottom = min, top = max
    p = (rect.bottom - clientY) / h;
  } else {
    const w = rect.width || 1;
    p = (clientX - rect.left) / w;
  }
  if (!Number.isFinite(p)) p = 0;
  if (p < 0) p = 0;
  if (p > 1) p = 1;
  return Math.round(min + p * (max - min));
}

/**
 * Paint properties for a slider fill/thumb.
 * Horizontal: fill width %, thumb left %.
 * Vertical: fill height %, thumb bottom %.
 */
function sliderPaintStyles({ orientation, min, max, value }) {
  const span = max - min;
  let p = span === 0 ? 1 : (value - min) / span;
  if (!Number.isFinite(p)) p = 0;
  if (p < 0) p = 0;
  if (p > 1) p = 1;
  const pct = (p * 100) + '%';
  if (orientation === 'vertical') {
    return {
      fill: { width: '', height: pct, left: '', bottom: '0', top: 'auto' },
      thumb: { left: '50%', bottom: pct, top: 'auto' },
    };
  }
  return {
    fill: { width: pct, height: '100%', left: '0', top: '0', bottom: 'auto' },
    thumb: { left: pct, top: '50%', bottom: 'auto' },
  };
}

/** Build the physical-pixel menu.__ready payload from stable CSS measurements. */
function buildMeasurePayload({ menuId = '', root, submenus = [], dpr = 1 } = {}) {
  const scale = Number.isFinite(+dpr) && +dpr > 0 ? +dpr : 1;
  const edge = (v) => Math.ceil(Math.max(0, Number.isFinite(+v) ? +v : 0) * scale);
  const rw = edge(root && root.w);
  const rh = edge(root && root.h);
  let maxW = 0;
  let maxH = 0;
  for (const sub of submenus || []) {
    maxW = Math.max(maxW, edge(sub && sub.w));
    maxH = Math.max(maxH, edge(sub && sub.h));
  }
  return {
    menuId: String(menuId || ''),
    root: { w: rw, h: rh },
    submenu: { maxW, maxH },
  };
}

/** Exact equality used by the two-consecutive-frame measurement stabilizer. */
function sameMeasurePayload(a, b) {
  return !!a && !!b
    && a.root && b.root && a.submenu && b.submenu
    && a.root.w === b.root.w && a.root.h === b.root.h
    && a.submenu.maxW === b.submenu.maxW && a.submenu.maxH === b.submenu.maxH;
}

/**
 * Keyboard delta for slider keys. Positive = increase.
 * Up/Right increase; Down/Left decrease; Home → min; End → max.
 * Returns { kind:'delta', delta } | { kind:'edge', edge:'min'|'max' } | null.
 */
function sliderKeyAction(key, orientation) {
  void orientation; // both orientations share the same key semantics
  switch (key) {
    case 'ArrowUp':
    case 'ArrowRight':
      return { kind: 'delta', delta: 1 };
    case 'ArrowDown':
    case 'ArrowLeft':
      return { kind: 'delta', delta: -1 };
    case 'Home':
      return { kind: 'edge', edge: 'min' };
    case 'End':
      return { kind: 'edge', edge: 'max' };
    default:
      return null;
  }
}

/** Apply a slider key action to a current value. Constant → unchanged. */
function applySliderKey({ key, orientation, min, max, value, step = 1 }) {
  if (max === min) return min;
  const action = sliderKeyAction(key, orientation);
  if (!action) return value;
  let next = value;
  if (action.kind === 'edge') next = action.edge === 'min' ? min : max;
  else next = value + action.delta * step;
  if (next < min) next = min;
  if (next > max) next = max;
  return next;
}

/**
 * Whether a value change should be emitted for a slider.
 * Constant sliders never emit; NaN / out-of-range rejected.
 */
function shouldEmitSliderValue({ min, max, value, previous }) {
  if (max === min) return false;
  if (!Number.isFinite(value)) return false;
  if (value < min || value > max) return false;
  if (value === previous) return false;
  return true;
}

/** Throttle gate for pointermove (50ms). force always passes. */
function shouldThrottleEmit(now, lastSent, force, throttleMs = 50) {
  if (force) return true;
  return (now - lastSent) >= throttleMs;
}

// ── ARIA / focus helpers ────────────────────────────────────────────────────

/**
 * Row role for navigation mode.
 * checkable → menuitemcheckbox; rich → menuitem; separator → separator; else menuitem.
 */
function resolveNavRowRole({ kind, checkable, checked }) {
  if (kind === 'separator') return 'separator';
  if (checkable || kind === 'checkbox') return 'menuitemcheckbox';
  void checked;
  return 'menuitem';
}

/**
 * Build aria-label for a rich control row in navigation mode.
 * `locale` selects the text language: 'zh' for Chinese, anything else English.
 * It arrives via the overlay state JSON, where the host derives it from the
 * Windows user UI language, so it matches the rest of the native chrome instead
 * of the WebView2 browser language.
 * The literal key name "Enter" stays untranslated in both languages because it
 * names a physical key, not a word.
 */
function buildRichNa)MENUHTML"
        LR"MENUHTML(vAriaLabel({ kind, label, value, min, max, locale }) {
  const zh = locale === 'zh';
  const name = label || kind || (zh ? '控件' : 'control');
  const hint = zh ? '按 Enter 键调整' : 'Press Enter to adjust';
  if (kind === 'slider') {
    return zh
      ? `${name}，${value}，范围 ${min} 到 ${max}。${hint}`
      : `${name}, ${value}, range ${min} to ${max}. ${hint}`;
  }
  if (kind === 'rating') {
    return zh
      ? `${name}，5 星中的 ${value} 星。${hint}`
      : `${name}, ${value} of 5 stars. ${hint}`;
  }
  if (kind === 'segmented') {
    return zh
      ? `${name}，选项 ${value}。${hint}`
      : `${name}, option ${value}. ${hint}`;
  }
  return zh ? `${name}。${hint}` : `${name}. ${hint}`;
}

/** Detect explicit checkable identity (field present, including false). */
function isExplicitCheckable(it) {
  if (!it || typeof it !== 'object') return false;
  if (it.type === 'checkbox') return true;
  if (Object.prototype.hasOwnProperty.call(it, 'checked')) return true;
  if (it.checkable === true) return true;
  return false;
}

/** Default CSS must honor prefers-reduced-motion for entrance/exit. */
function analyzeReducedMotionCss(css) {
  const text = String(css || '');
  return {
    hasReducedMotionQuery: /@media\s*\(\s*prefers-reduced-motion\s*:\s*reduce\s*\)/.test(text),
    disablesTransformOrTransition: /prefers-reduced-motion\s*:\s*reduce[\s\S]{0,400}(transition\s*:\s*none|transform\s*:\s*none)/.test(text),
  };
}

// ── Pointer focus + segmented roving + hide cleanup ─────────────────────────

/**
 * setActive focus gate. Pointer hover may update active/roving, but must not
 * steal real keyboard/editor focus unless an explicit path opts in.
 * Default (undefined / true) focuses; only focus:false suppresses.
 */
function shouldFocusOnActivate(opts) {
  const o = opts || {};
  return o.focus !== false;
}

/** Segment enabled? Missing/undefined enabled means enabled. */
function isSegmentEnabled(seg) {
  return !!(seg && seg.enabled !== false);
}

/** Next enabled segment index from `from` stepping by `dir` (±1). Bounds clamp (no wrap). */
function nextEnabledSegmentIndex(segments, from, dir) {
  const segs = segments || [];
  const n = segs.length;
  const step = dir >= 0 ? 1 : -1;
  for (let k = 1; k <= n; k++) {
    const i2 = from + step * k;
    if (i2 < 0 || i2 >= n) return -1;
    if (isSegmentEnabled(segs[i2])) return i2;
  }
  return -1;
}

/** First enabled segment index, or -1. */
function firstEnabledSegmentIndex(segments) {
  const segs = segments || [];
  for (let i = 0; i < segs.length; i++) {
    if (isSegmentEnabled(segs[i])) return i;
  }
  return -1;
}

/**
 * Editor-mode segmented roving plan for a selected index.
 * Disabled options never receive tabindex 0 / focus.
 * @returns {{ focusIndex: number, tabIndexes: string[], ariaChecked: string[] }}
 */
function planSegmentedEditorRoving({ segments, selectedIndex } = {}) {
  const segs = segments || [];
  const n = segs.length;
  const tabIndexes = Array(n).fill('-1');
  const ariaChecked = Array(n).fill('false');
  let focusIndex = -1;
  const sel = Number.isFinite(selectedIndex) ? selectedIndex : -1;
  if (sel >= 0 && sel < n && isSegmentEnabled(segs[sel])) {
    focusIndex = sel;
  } else {
    focusIndex = firstEnabledSegmentIndex(segs);
  }
  if (sel >= 0 && sel < n && isSegmentEnabled(segs[sel])) {
    ariaChecked[sel] = 'true';
  }
  if (focusIndex >= 0) tabIndexes[focusIndex] = '0';
  return { focusIndex, tabIndexes, ariaChecked };
}

/**
 * Left/Right plan in segmented editor: next enabled selection + focus/tabindex.
 * @returns {null | { nextIndex: number, focusIndex: number, tabIndexes: string[], ariaChecked: string[] }}
 */
function planSegmentedEditorAdjust({ segments, current, delta } = {}) {
  const segs = segments || [];
  const cur = Number.isFinite(current) ? current : -1;
  const d = delta >= 0 ? 1 : -1;
  const base = cur < 0 ? (d > 0 ? -1 : segs.length) : cur;
  const nextIndex = nextEnabledSegmentIndex(segs, base, d);
  if (nextIndex < 0) return null;
  const plan = planSegmentedEditorRoving({ segments: segs, selectedIndex: nextIndex });
  return {
    nextIndex,
    focusIndex: plan.focusIndex,
    tabIndexes: plan.tabIndexes,
    ariaChecked: plan.ariaChecked,
  };
}

/**
 * Navigation-mode segmented internals: all radios tabindex -1 (subtree inert).
 * Does not invent checked state beyond selectedIndex.
 */
function planSegmentedNavigationInternals({ segments, selectedIndex } = {}) {
  const segs = segments || [];
  const n = segs.length;
  const tabIndexes = Array(n).fill('-1');
  const ariaChecked = Array(n).fill('false');
  const sel = Number.isFinite(selectedIndex) ? selectedIndex : -1;
  if (sel >= 0 && sel < n && isSegmentEnabled(segs[sel])) ariaChecked[sel] = 'true';
  return { tabIndexes, ariaChecked, ariaHidden: true };
}

/**
 * Static contract: every mouseenter callback that calls setActive must use a
 * non-stealing path (setActiveFromPointer, or setActive(..., {focus:false})).
 * Rating star hover preview (paint only) is allowed without setActive.
 */
function analyzeMouseenterSetActiveFocus(jsSource) {
  const text = String(jsSource || '');
  const re = /addEventListener\(\s*["']mouseenter["']\s*,\s*function\s*\([^)]*\)\s*\{/g;
  const handlers = [];
  let m;
  while ((m = re.exec(text))) {
    const start = m.index + m[0].length;
    let depth = 1;
    let i = start;
    for (; i < text.length && depth > 0; i++) {
      const ch = text[i];
      if (ch === '{') depth++;
      else if (ch === '}') depth--;
    }
    const body = text.slice(start, i - 1);
    const setActiveCalls = [];
    const callRe = /\b(setActiveFromPointer|setActive)\s*\(/g;
    let cm;
    while ((cm = callRe.exec(body))) {
      // Capture argument list with nested braces/parens for {focus:false}.
      let p = cm.index + cm[0].length;
      let pd = 1;
      let argsStart = p;
      for (; p < body.length && pd > 0; p++) {
        if (body[p] === '(') pd++;
        else if (body[p] === ')') pd--;
        else if (bod)MENUHTML"
        LR"MENUHTML(y[p] === '{') {
          // still inside args
        }
      }
      const args = body.slice(argsStart, p - 1);
      const name = cm[1];
      const nonFocusing =
        name === 'setActiveFromPointer' ||
        /focus\s*:\s*false/.test(args);
      setActiveCalls.push({ name, args, nonFocusing });
    }
    handlers.push({
      body,
      setActiveCalls,
      hasSetActive: setActiveCalls.length > 0,
      allNonFocusing: setActiveCalls.every((c) => c.nonFocusing),
    });
  }
  return {
    handlers,
    setActiveHandlerCount: handlers.filter((h) => h.hasSetActive).length,
    allSetActiveNonFocusing: handlers.filter((h) => h.hasSetActive).every((h) => h.allNonFocusing),
  };
}

/**
 * Static contract: hide / select / dismiss / render must clear editor interaction state.
 */
function analyzeMenuInteractionCleanup(jsSource) {
  const text = String(jsSource || '');
  const hasCleanupHelper = /function\s+cleanupMenuInteraction\s*\(/.test(text);
  // menu:__hide must invoke cleanup (helper or explicit exitEditor + pendingRootFocus clear).
  const hideMatch = text.match(/fb2k\.on\(\s*["']menu:__hide["']\s*,\s*function\s*\([^)]*\)\s*\{([\s\S]*?)\}\s*\)/);
  const hideBody = hideMatch ? hideMatch[1] : '';
  const hideCleans =
    /cleanupMenuInteraction\s*\(/.test(hideBody) ||
    (/exitEditor\s*\(/.test(hideBody) && /pendingRootFocus\s*=\s*false/.test(hideBody) && /editorCtx\s*=\s*null/.test(hideBody));
  const selectMatch = text.match(/function\s+select\s*\([^)]*\)\s*\{([\s\S]*?)\}/);
  const selectBody = selectMatch ? selectMatch[1] : '';
  const selectCleans =
    /cleanupMenuInteraction\s*\(/.test(selectBody) ||
    /exitEditor\s*\(/.test(selectBody);
  const dismissMatch = text.match(/function\s+dismiss\s*\([^)]*\)\s*\{([\s\S]*?)\}/);
  const dismissBody = dismissMatch ? dismissMatch[1] : '';
  const dismissCleans =
    /cleanupMenuInteraction\s*\(/.test(dismissBody) ||
    /exitEditor\s*\(/.test(dismissBody);
  const renderMatch = text.match(/function\s+render\s*\([^)]*\)\s*\{([\s\S]*?)\n\s*function\s+/);
  // render is large; fall back to a windowed search from "function render"
  let renderCleans = false;
  const renderIdx = text.indexOf('function render(st)');
  if (renderIdx >= 0) {
    const window = text.slice(renderIdx, renderIdx + 900);
    renderCleans =
      /cleanupMenuInteraction\s*\(/.test(window) ||
      (/exitEditor\s*\(/.test(window) && /pendingRootFocus\s*=\s*false/.test(window) && /editorCtx\s*=\s*null/.test(window));
  }
  return {
    hasCleanupHelper,
    hideCleans,
    selectCleans,
    dismissCleans,
    renderCleans,
    allPathsClean: !!(hasCleanupHelper && hideCleans && selectCleans && dismissCleans && renderCleans),
  };
}

    var cur = null;
    var layers = [];   // index=depth: {el, rows:[{el,item,navigable,hasSub}], active}
    // Single interaction mode — navigation | editor.
    var interactionMode = "navigation";  // "navigation" | "editor"
    var editorCtx = null;                // { depth, rowIdx, focusEl, exit }
    var pendingRootFocus = false;        // ContentSized: focus after first placed
    var placedGeometry = null;           // ContentSized CSS-pixel slot geometry
    var submenuPanelSequence = 0;        // monotonic root→host panel state reports
    var submenuPanelOpen = false;        // host confirms that the child HWND is visible
    var uiLocale = "en";                 // host UI language for aria text; refreshed per render

    // Clear editor/focus pending state without stealing focus. Used by dismiss/select/hide/render.
    function cleanupMenuInteraction(){
      exitEditor(false);
      interactionMode="navigation";
      editorCtx=null;
      pendingRootFocus=false;
    }
    function dismiss(reason){ try{ cleanupMenuInteraction(); if(window.fb2k&&fb2k.invoke) fb2k.invoke("menu.__dismiss",{menuId:(cur&&cur.menuId)||"",reason:reason}); }catch(e){} }
    function select(token){ try{ cleanupMenuInteraction(); if(window.fb2k&&fb2k.invoke) fb2k.invoke("menu.__select",{menuId:(cur&&cur.menuId)||"",token:token}); }catch(e){} }   // 只回 opaque token；C++ 侧 token 索引解析出 publicId + 可信 origin
    function valueChange(token, value){ try{ if(window.fb2k&&fb2k.invoke) fb2k.invoke("menu.__valueChanged",{menuId:(cur&&cur.menuId)||"",token:token,value:value}); }catch(e){} }
    function reportSubmenuSurfaceReady(){ try{ if(window.fb2k&&fb2k.invoke) fb2k.invoke("menu.__submenuPanel",{menuId:(cur&&cur.menuId)||"",ready:true,parentToken:(cur&&cur.parentToken)||""}); }catch(e){} }
    function clampInt(v,lo,hi){ v=Math.round((+v)||0); if(v<lo)v=lo; if(v>hi)v=hi; return v; }
    function numOr(v,d){ return (typeof v==="number"&&!isNaN(v))?v:d; }
    function attrEsc(s){ return (""+s).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;"); }
    function isNormalKind(it){ return !!it && it.type!=="separator" && it.type!=="nowplaying" && it.type!=="rating" && it.type!=="slider" && it.type!=="segmented"; }   // 仅普通/子菜单项参与图标列（富项含 segmented 不参与，镜像 C++ TrayItemKindRendersIcon）
    function isRichKind(k){ return k==="rating"||k==="slider"||k==="segmented"; }

    // Shared hide/show protocol + stable attrs (from menu-overlay-layout.mjs).
    function applyAttrs(el, attrs){
      if(!el||!attrs) return;
      for(var k in attrs){ if(Object.prototype.hasOwnProperty.call(attrs,k)) el.setAttribute(k, attrs[k]); }
    }
    function runProtocol(el, steps){
      if(!el||!steps) return;
      for(var i=0;i<steps.length;i++){
        var s=steps[i];
        if(s.op==="addClass") el.classList.add(s.className);
        else if(s.op==="removeClass") el.classList.remove(s.className);
        else if(s.op==="setProperty") el.style.setProperty(s.name, s.value, s.priority||"");
        else if(s.op==="removeProperty") el.style.removeProperty(s.name);
      }
    }
    function hideMenuEl(el){ runProtocol(el, hideMenuProtocol()); }
    function showMenuEl(el){ runProtocol(el, showMenuProtocol()); }
    function itemZone(it, fallback)MENUHTML"
        LR"MENUHTML(){ return (it&&it._zone)||fallback||""; }
    function mountHost(){ return document.getElementById("viewport")||document.body; }

    function applyContentSizedGeometry(el, geometry){
      if(!el||!geometry) return;
      el.style.setProperty("left",geometry.left,"important");
      el.style.setProperty("top",geometry.top,"important");
      el.style.setProperty("max-width",geometry.maxWidth,"important");
      el.style.setProperty("max-height",geometry.maxHeight,"important");
      el.style.setProperty("min-width","0px","important");
      el.style.setProperty("overflow","auto","important");
    }
    function clearContentSizedGeometry(el){
      if(!el) return;
      el.style.removeProperty("left");
      el.style.removeProperty("top");
      el.style.removeProperty("max-width");
      el.style.removeProperty("max-height");
      el.style.removeProperty("min-width");
      el.style.removeProperty("overflow");
    }

    function applyPlacedGeometry(ev){
      placedGeometry=normalizePlacedGeometry(ev, window.devicePixelRatio||1);
      if(!placedGeometry) return;
      var viewport=mountHost(), root=document.getElementById("menu");
      if(viewport){ viewport.style.width=placedGeometry.viewportW+"px"; viewport.style.height=placedGeometry.viewportH+"px"; }
      if(root){
        applyContentSizedGeometry(root,{
          left:"0px",top:placedGeometry.rootTop+"px",
          maxWidth:placedGeometry.rootSlotW+"px",
          maxHeight:(placedGeometry.viewportH-placedGeometry.rootTop)+"px"
        });
      }
    }

    function setSubtreeInert(root, inert){
      if(!root) return;
      if(inert){ root.setAttribute("aria-hidden","true"); }
      else { root.removeAttribute("aria-hidden"); }
      var nodes=root.querySelectorAll("*");
      for(var i=0;i<nodes.length;i++){
        if(inert){ nodes[i].setAttribute("tabindex","-1"); }
        else if(nodes[i].getAttribute("data-fb-focusable")==="1"){ /* restored by editor enter */ }
        else { nodes[i].setAttribute("tabindex","-1"); }
      }
    }

    function syncRovingTabindex(depth){
      var L=layers[depth]; if(!L) return;
      for(var i=0;i<L.rows.length;i++){
        var r=L.rows[i]; if(!r||!r.el) continue;
        if(!r.navigable){ r.el.setAttribute("tabindex","-1"); continue; }
        r.el.setAttribute("tabindex", (interactionMode==="navigation" && i===L.active) ? "0" : "-1");
      }
    }

    function setActive(depth, idx, opts){
      opts=opts||{};
      var L=layers[depth]; if(!L) return;
      if(L.active>=0 && L.rows[L.active]) L.rows[L.active].el.classList.remove("active");
      L.active=idx;
      syncRovingTabindex(depth);
      if(idx>=0 && L.rows[idx]){
        var e=L.rows[idx].el; e.classList.add("active");
        try{ e.scrollIntoView({block:"nearest"}); }catch(x){}
        // Pointer hover must not steal keyboard/editor focus.
        if(interactionMode==="navigation" && shouldFocusOnActivate(opts)){
          try{ e.focus({preventScroll:true}); }catch(x2){ try{ e.focus(); }catch(x3){} }
        }
      }
    }
    // Pointer/mouse hover path: update active/roving only, never real focus().
    function setActiveFromPointer(depth, idx){ setActive(depth, idx, {focus:false}); }
    function firstNav(depth){ var L=layers[depth]; if(!L) return -1; for(var i=0;i<L.rows.length;i++) if(L.rows[i].navigable) return i; return -1; }
    function lastNav(depth){ var L=layers[depth]; if(!L) return -1; for(var i=L.rows.length-1;i>=0;i--) if(L.rows[i].navigable) return i; return -1; }
    function moveActive(depth, dir){
      var L=layers[depth]; if(!L||!L.rows.length) return;
      var n=L.rows.length, i=(L.active<0?(dir>0?-1:0):L.active);
      for(var k=0;k<n;k++){ i=(i+dir+n)%n; if(L.rows[i].navigable){ setActive(depth,i); return; } }
    }
    function closeLayersFrom(depth){
      // 固定窗设计：子菜单收起仅移除其 DOM；窗口不 resize（无闪），无需重测/缩窗。
      if(interactionMode==="editor" && editorCtx && editorCtx.depth>=depth) exitEditor(false);
      for(var i=layers.length-1;i>=depth;i--){ if(i>0 && layers[i] && layers[i].el && layers[i].el.parentNode) layers[i].el.parentNode.removeChild(layers[i].el); }
      if(layers.length>depth) layers.length=depth;
      // Restore parent expanded state.
      if(depth>0){
        var pL=layers[depth-1];
        if(pL){
          for(var ri=0;ri<pL.rows.length;ri++){
            if(pL.rows[ri] && pL.rows[ri].hasSub){
              pL.rows[ri].el.setAttribute("aria-expanded","false");
            }
          }
        }
      }
      if(depth<=1 && cur && cur.windowModel==="contentSized") reportSubmenuPanel(null);
    }

    function enterEditor(depth, rowIdx){
      var L=layers[depth]; if(!L||!L.rows[rowIdx]) return;
      var row=L.rows[rowIdx];
      if(!row.navigable || !isRichKind(row.kind) || !row.enterEditor) return;
      interactionMode="editor";
      editorCtx={ depth:depth, rowIdx:rowIdx, row:row };
      row.el.setAttribute("role","none");
      row.el.setAttribute("tabindex","-1");
      if(row.controlRoot) setSubtreeInert(row.controlRoot, false);
      var focusEl=row.enterEditor();
      editorCtx.focusEl=focusEl||null;
      if(focusEl){ try{ focusEl.focus({preventScroll:true}); }catch(x){ try{ focusEl.focus(); }catch(x2){} } }
    }
    function exitEditor(restoreFocus){
      if(interactionMode!=="editor" || !editorCtx) { interactionMode="navigation"; editorCtx=null; return; }
      var row=editorCtx.row;
      if(row && row.exitEditor) row.exitEditor();
      if(row && row.controlRoot) setSubtreeInert(row.controlRoot, true);
      if(row && row.el){
        // Restore navigation role/label.
        if(row.applyNavAria) row.applyNavAria();
      }
      var d=editorCtx.depth, idx=editorCtx.rowIdx;
      interactionMode="navigation";
      editorCtx=null;
      if(restoreFocus!==false) setActive(d, idx, {focus:true});
      else syncRovingTabindex(d);
    }

    function stampItem(el, it, depth, zone){
      applyAttrs(el, buildItemStableAttrs(it, { depth: depth, zone: zone }));)MENUHTML"
        LR"MENUHTML(
    }
    function stampSep(el, depth, zone, zoneSeparator){
      applyAttrs(el, buildSeparatorStableAttrs({ depth: depth, zone: zone, zoneSeparator: !!zoneSeparator }));
    }

    // ---- 富菜单项构建器（仅根层出现；均 push 一个 row 供键盘导航/active 高亮）----
    function applyDisabledAria(el, en){ if(!en) el.setAttribute("aria-disabled","true"); else el.removeAttribute("aria-disabled"); }

    function buildNowPlaying(menuEl, L, it, en, depth, zone){
      var d=document.createElement("div");
      d.className="fb-item fb-np"+(!en?" disabled":"");
      d.setAttribute("part","item");
      stampItem(d, it, depth, zone);
      d.setAttribute("role","menuitem");
      d.setAttribute("tabindex","-1");
      applyDisabledAria(d, en);
      var cv=document.createElement("div"); cv.className="fb-np-cover"; cv.setAttribute("part","nowplaying-cover"); cv.setAttribute("aria-hidden","true");
      if(it.cover){ var cvv=""+it.cover, u=null;
        if(/^data:image\/[a-z0-9.+-]+;base64,[A-Za-z0-9+\/=]+$/i.test(cvv) || /^https?:\/\/[^\s"')]+$/i.test(cvv)){ u=cvv; }   // data: 或 http(s) 白名单（排除引号/括号/空白，杜绝 url() 注入/坏图）
        else if(/^[A-Za-z0-9+\/=]+$/.test(cvv)){ u="data:image/jpeg;base64,"+cvv; }   // 裸 base64 兼容
        if(u){ cv.style.backgroundImage='url("'+u+'")'; } }
      d.appendChild(cv);
      var tx=document.createElement("div"); tx.className="fb-np-text";
      var t=document.createElement("div"); t.className="fb-np-title"; t.setAttribute("part","nowplaying-title"); t.textContent=(it.title||it.label||""); tx.appendChild(t);
      if(it.subtitle){ var sb=document.createElement("div"); sb.className="fb-np-sub"; sb.setAttribute("part","nowplaying-subtitle"); sb.textContent=it.subtitle; tx.appendChild(sb); }
      d.appendChild(tx);
      menuEl.appendChild(d);
      var ridx=L.rows.length;
      var row={el:d, item:it, navigable:en, hasSub:false, kind:"nowplaying", zone:zone};
      L.rows.push(row);
      d.addEventListener("mouseenter", function(){ if(interactionMode==="editor") return; closeLayersFrom(depth+1); setActiveFromPointer(depth, ridx); });
      d.addEventListener("click", function(){ if(row.navigable) select(it._token); });   // 点击=普通项：回传 {id} + 关闭
    }

    function buildRating(menuEl, L, it, en, depth, zone){
      var d=document.createElement("div");
      d.className="fb-item fb-rating"+(!en?" disabled":"");
      d.setAttribute("part","item");
      stampItem(d, it, depth, zone);
      applyDisabledAria(d, en);
      if(it.label){ var lb=document.createElement("span"); lb.className="fb-rating-label"; lb.textContent=it.label; d.appendChild(lb); }
      var box=document.createElement("span"); box.className="fb-stars"; box.setAttribute("part","rating-stars");
      var ctl={v:clampInt(it.value,0,5)};
      var stars=[];
      // Internal focusable slider control (stars remain visual/pointer only).
      var focusEl=document.createElement("span");
      focusEl.className="fb-rating-control";
      focusEl.setAttribute("role","slider");
      focusEl.setAttribute("aria-valuemin","0");
      focusEl.setAttribute("aria-valuemax","5");
      focusEl.setAttribute("aria-valuenow", String(ctl.v));
      focusEl.setAttribute("aria-orientation","horizontal");
      focusEl.setAttribute("tabindex","-1");
      focusEl.setAttribute("data-fb-focusable","1");
      function paint(n){
        for(var k=0;k<5;k++) stars[k].classList.toggle("on", k<n);
        focusEl.setAttribute("aria-valuenow", String(n));
        if(interactionMode==="navigation") applyNavAria();
      }
      for(var k=0;k<5;k++){ (function(k){
        var s=document.createElement("span"); s.className="fb-star"; s.setAttribute("part","rating-star"); s.textContent="\u2605";
        s.setAttribute("aria-hidden","true"); s.setAttribute("tabindex","-1");
        s.addEventListener("mouseenter", function(){ if(en) paint(k+1); });                                  // hover 预览
        s.addEventListener("click", function(ev){ ev.stopPropagation(); if(!en) return; var nv=((k+1)===ctl.v)?0:(k+1); ctl.v=nv; paint(nv); valueChange(it._token, nv); });   // 点同星=清 0
        box.appendChild(s); stars.push(s);
      })(k); }
      box.addEventListener("mouseleave", function(){ paint(ctl.v); });                                       // 移出还原实际值
      box.appendChild(focusEl);
      d.appendChild(box);
      paint(ctl.v);
      menuEl.appendChild(d);
      var ridx=L.rows.length;
      function applyNavAria(){
        d.setAttribute("role","menuitem");
        d.setAttribute("tabindex","-1");
        d.setAttribute("aria-label", buildRichNavAriaLabel({kind:"rating", label:it.label, value:ctl.v, locale:uiLocale}));
        setSubtreeInert(box, true);
      }
      applyNavAria();
      var row={el:d, item:it, navigable:en, hasSub:false, kind:"rating", zone:zone, controlRoot:box, applyNavAria:applyNavAria,
        adjust:function(delta){ if(!en) return; var nv=clampInt(ctl.v+delta,0,5); if(nv!==ctl.v){ ctl.v=nv; paint(nv); valueChange(it._token, nv); } },
        enterEditor:function(){ d.setAttribute("role","none"); d.removeAttribute("aria-label"); setSubtreeInert(box,false); focusEl.setAttribute("tabindex","0"); return focusEl; },
        exitEditor:function(){ focusEl.setAttribute("tabindex","-1"); applyNavAria(); },
        editorKey:function(e){
          if(e.key==="ArrowRight"||e.key==="ArrowUp"){ row.adjust(1); return true; }
          if(e.key==="ArrowLeft"||e.key==="ArrowDown"){ row.adjust(-1); return true; }
          if(e.key==="Home"){ if(ctl.v!==0){ ctl.v=0; paint(0); valueChange(it._token,0); } return true; }
          if(e.key==="End"){ if(ctl.v!==5){ ctl.v=5; paint(5); valueChange(it._token,5); } return true; }
          if(e.key==="Escape"||e.key==="Enter"){ exitEditor(true); return true; }
          return false;
        }};
      L.rows.push(row);
      d.addEventListener("mouseenter", function(){ if(interactionMode==="editor") return; closeLayersFrom(depth+1); setActiveFromPointer(depth, ridx); });
    }

    function buildSlider(menuEl,)MENUHTML"
        LR"MENUHTML( L, it, en, depth, zone){
      var norm=normalizeSliderRange(it);
      var mn=norm.min, mx=norm.max, orient=norm.orientation, constant=norm.constant;
      var ctl={v:norm.value};
      var d=document.createElement("div");
      d.className="fb-item fb-slider"+(!en?" disabled":"");
      d.setAttribute("part","item");
      stampItem(d, it, depth, zone);
      d.setAttribute("data-orientation", orient);
      applyDisabledAria(d, en);
      if(it.label){ var lb=document.createElement("span"); lb.className="fb-slider-label"; lb.textContent=it.label; d.appendChild(lb); }
      var track=document.createElement("span"); track.className="fb-slider-track"; track.setAttribute("part","slider-track");
      var fill=document.createElement("span"); fill.className="fb-slider-fill"; track.appendChild(fill);
      var thumb=document.createElement("span"); thumb.className="fb-slider-thumb"; track.appendChild(thumb);
      // Internal focusable slider control.
      var focusEl=document.createElement("span");
      focusEl.className="fb-slider-control";
      focusEl.setAttribute("role","slider");
      focusEl.setAttribute("aria-valuemin", String(mn));
      focusEl.setAttribute("aria-valuemax", String(mx));
      focusEl.setAttribute("aria-valuenow", String(ctl.v));
      focusEl.setAttribute("aria-orientation", orient);
      focusEl.setAttribute("tabindex","-1");
      focusEl.setAttribute("data-fb-focusable","1");
      track.appendChild(focusEl);
      d.appendChild(track);
      var out=document.createElement("span"); out.className="fb-slider-val"; out.setAttribute("part","slider-value"); d.appendChild(out);
      function applyPaintStyles(v){
        var st=sliderPaintStyles({orientation:orient, min:mn, max:mx, value:v});
        fill.style.width=st.fill.width; fill.style.height=st.fill.height;
        fill.style.left=st.fill.left; fill.style.top=st.fill.top; fill.style.bottom=st.fill.bottom;
        thumb.style.left=st.thumb.left; thumb.style.top=st.thumb.top; thumb.style.bottom=st.thumb.bottom;
        out.textContent=""+v;
        focusEl.setAttribute("aria-valuenow", String(v));
        if(interactionMode==="navigation") applyNavAria();
      }
      var lastSent=0, dragging=false;
      function emit(v, force){
        if(constant) return;
        if(!shouldEmitSliderValue({min:mn,max:mx,value:v,previous:NaN}) && !force) return;
        // Always gate constant; for force on same value still emit end value once.
        if(!Number.isFinite(v) || v<mn || v>mx) return;
        var now=Date.now();
        if(shouldThrottleEmit(now, lastSent, force, 50)){
          lastSent=now;
          if(!force && v===ctl._lastEmitted) return;
          ctl._lastEmitted=v;
          valueChange(it._token, v);
        }
      }
      function setFromEvent(e, force){
        if(!en || constant) return;
        var r=track.getBoundingClientRect();
        var nv=sliderValueFromPointer({orientation:orient, min:mn, max:mx, clientX:e.clientX, clientY:e.clientY, rect:r});
        if(nv!==ctl.v){ ctl.v=nv; applyPaintStyles(nv); emit(nv, force); }
        else if(force){ emit(nv, true); }
      }
      track.addEventListener("pointerdown", function(e){ if(!en||constant) return; e.stopPropagation(); dragging=true; try{ track.setPointerCapture(e.pointerId); }catch(x){} setFromEvent(e, false); });
      track.addEventListener("pointermove", function(e){ if(dragging) setFromEvent(e, false); });
      track.addEventListener("pointerup", function(e){ if(dragging){ dragging=false; try{ track.releasePointerCapture(e.pointerId); }catch(x){} setFromEvent(e, true); } });
      track.addEventListener("pointercancel", function(){ if(dragging){ dragging=false; emit(ctl.v, true); } });
      track.addEventListener("click", function(ev){ ev.stopPropagation(); });   // 阻止冒泡到 row（不应选中/关闭）
      applyPaintStyles(ctl.v);
      menuEl.appendChild(d);
      var ridx=L.rows.length;
      var step=constant?0:Math.max(1, Math.round((mx-mn)/20));
      function applyNavAria(){
        d.setAttribute("role","menuitem");
        d.setAttribute("tabindex","-1");
        d.setAttribute("aria-label", buildRichNavAriaLabel({kind:"slider", label:it.label, value:ctl.v, min:mn, max:mx, locale:uiLocale}));
        setSubtreeInert(track, true);
      }
      applyNavAria();
      var row={el:d, item:it, navigable:en, hasSub:false, kind:"slider", zone:zone, controlRoot:track, applyNavAria:applyNavAria,
        adjust:function(delta){
          if(!en||constant) return;
          var nv=clampInt(ctl.v+delta*step,mn,mx);
          if(nv!==ctl.v){ ctl.v=nv; applyPaintStyles(nv); valueChange(it._token, nv); }
        },
        enterEditor:function(){ d.setAttribute("role","none"); d.removeAttribute("aria-label"); setSubtreeInert(track,false); focusEl.setAttribute("tabindex","0"); return focusEl; },
        exitEditor:function(){ focusEl.setAttribute("tabindex","-1"); applyNavAria(); },
        editorKey:function(e){
          if(e.key==="Escape"||e.key==="Enter"){ exitEditor(true); return true; }
          if(constant) return (e.key==="ArrowUp"||e.key==="ArrowDown"||e.key==="ArrowLeft"||e.key==="ArrowRight"||e.key==="Home"||e.key==="End");
          var next=applySliderKey({key:e.key, orientation:orient, min:mn, max:mx, value:ctl.v, step:step});
          if(next===ctl.v && e.key!=="Home" && e.key!=="End" && e.key!=="ArrowUp" && e.key!=="ArrowDown" && e.key!=="ArrowLeft" && e.key!=="ArrowRight") return false;
          if(sliderKeyAction(e.key, orient)){
            if(next!==ctl.v){ ctl.v=next; applyPaintStyles(next); valueChange(it._token, next); }
            return true;
          }
          return false;
        }};
      L.rows.push(row);
      d.addEventListener("mouseenter", function(){ if(interactionMode==="editor") return; closeLayersFrom(depth+1); setActiveFromPointer(depth, ridx); });
    }

    // segmented 分段单选：读 it.segments（互斥选项），it.value=选中段索引。点启用段→paint+valueChange（走现有 value 通道，不关菜单）；
    // Left/Right 经 row.adjust 移到上/下一个【启用】段并 valueCh)MENUHTML"
        LR"MENUHTML(ange（clamp 边界）。index→业务语义由前端映射。
    // Editor 态：选中迁移时同步 roving tabindex + 真实 focus（键盘）；pointer pick 至少同步 tabindex。
    function buildSegmented(menuEl, L, it, en, depth, zone){
      var d=document.createElement("div"); d.className="fb-item fb-seg"+(!en?" disabled":""); d.setAttribute("part","item");
      stampItem(d, it, depth, zone);
      applyDisabledAria(d, en);
      if(it.label){ var lb=document.createElement("span"); lb.className="fb-seg-label"; lb.setAttribute("part","segmented-label"); lb.textContent=it.label; d.appendChild(lb); }
      var grp=document.createElement("span"); grp.className="fb-seg-group"; grp.setAttribute("part","segmented-group");
      grp.setAttribute("role","radiogroup");
      var segs=(it.segments||[]), btns=[]; var ctl={v:(typeof it.value==="number")?it.value:-1};
      function applySegmentPlan(plan, doFocus){
        if(!plan) return;
        for(var k=0;k<btns.length;k++){
          btns[k].classList.toggle("on", plan.ariaChecked[k]==="true");
          btns[k].setAttribute("aria-checked", plan.ariaChecked[k]);
          if(interactionMode==="editor") btns[k].setAttribute("tabindex", plan.tabIndexes[k]);
        }
        if(interactionMode==="editor" && doFocus && plan.focusIndex>=0 && btns[plan.focusIndex]){
          try{ btns[plan.focusIndex].focus({preventScroll:true}); }catch(x){ try{ btns[plan.focusIndex].focus(); }catch(x2){} }
        }
        if(interactionMode==="navigation") applyNavAria();
      }
      function paint(sel){
        var plan=planSegmentedEditorRoving({segments:segs, selectedIndex:sel});
        // paint always updates checked/on; tabindex only in editor (nav uses setSubtreeInert).
        for(var k=0;k<btns.length;k++){
          btns[k].classList.toggle("on", plan.ariaChecked[k]==="true");
          btns[k].setAttribute("aria-checked", plan.ariaChecked[k]);
        }
        if(interactionMode==="navigation") applyNavAria();
      }
      // opts: {focus:true|false} — keyboard paths focus; pointer click syncs tabindex only.
      function pick(idx, opts){
        var s=segs[idx]; if(!en||!s||!isSegmentEnabled(s)) return;
        ctl.v=idx;
        var plan=planSegmentedEditorRoving({segments:segs, selectedIndex:idx});
        if(interactionMode==="editor"){
          applySegmentPlan(plan, !!(opts&&opts.focus));
          // pointer: still sync tabindex even when not focusing outside menu.
          if(!(opts&&opts.focus)){
            for(var k=0;k<btns.length;k++) btns[k].setAttribute("tabindex", plan.tabIndexes[k]);
          }
        } else {
          paint(idx);
        }
        valueChange(it._token, idx);
      }
      for(var i=0;i<segs.length;i++){ (function(c, idx){
        var b=document.createElement("span"); b.className="fb-seg-btn"+((c&&c.enabled===false)?" disabled":""); b.setAttribute("part","segmented-option"); b.title=(c&&c.label)||"";
        b.setAttribute("role","radio");
        b.setAttribute("aria-checked","false");
        b.setAttribute("tabindex","-1");
        b.setAttribute("data-fb-focusable","1");
        if(c&&c.enabled===false) b.setAttribute("aria-disabled","true");
        if(c&&c.iconSvg&&c.iconSvg.content){ mountSanitizedSvgIcon(b, (c.iconSvg.viewBox)||"0 0 24 24", c.iconSvg.content); }
        else { b.textContent=(c&&c.label)||""; }
        b.addEventListener("click", function(ev){ ev.stopPropagation(); pick(idx, {focus:false}); });
        grp.appendChild(b); btns.push(b);
      })(segs[i], i); }
      paint(ctl.v); d.appendChild(grp); menuEl.appendChild(d);
      var ridx=L.rows.length;
      function applyNavAria(){
        d.setAttribute("role","menuitem");
        d.setAttribute("tabindex","-1");
        d.setAttribute("aria-label", buildRichNavAriaLabel({kind:"segmented", label:it.label, value:ctl.v, locale:uiLocale}));
        setSubtreeInert(grp, true);
      }
      applyNavAria();
      var row={el:d, item:it, navigable:en, hasSub:false, kind:"segmented", zone:zone, controlRoot:grp, applyNavAria:applyNavAria,
        adjust:function(delta){
          if(!en) return;
          var plan=planSegmentedEditorAdjust({segments:segs, current:ctl.v, delta:delta});
          if(!plan) return;
          ctl.v=plan.nextIndex;
          if(interactionMode==="editor") applySegmentPlan(plan, true);
          else paint(plan.nextIndex);
          valueChange(it._token, plan.nextIndex);
        },
        enterEditor:function(){
          d.setAttribute("role","none"); d.removeAttribute("aria-label"); setSubtreeInert(grp,false);
          var plan=planSegmentedEditorRoving({segments:segs, selectedIndex:ctl.v});
          for(var k=0;k<btns.length;k++) btns[k].setAttribute("tabindex", plan.tabIndexes[k]);
          return plan.focusIndex>=0?btns[plan.focusIndex]:null;
        },
        exitEditor:function(){ for(var k=0;k<btns.length;k++) btns[k].setAttribute("tabindex","-1"); applyNavAria(); },
        editorKey:function(e){
          if(e.key==="ArrowRight"){ row.adjust(1); return true; }
          if(e.key==="ArrowLeft"){ row.adjust(-1); return true; }
          if(e.key==="Escape"||e.key==="Enter"){ exitEditor(true); return true; }
          return false;
        }};
      L.rows.push(row);
      d.addEventListener("mouseenter", function(){ if(interactionMode==="editor") return; closeLayersFrom(depth+1); setActiveFromPointer(depth, ridx); });
    }

    function appendItems(menuEl, L, items, depth, zone, layerHasIcon){
      for(var i=0;i<items.length;i++){
        var it=items[i];
        var z=itemZone(it, zone);
        if(it && it.type==="separator"){
          var s=document.createElement("div"); s.className="fb-sep"; s.setAttribute("part","separator");
          stampSep(s, depth, z, false);
          menuEl.appendChild(s); continue;
        }
        if(it && it.type==="nowplaying"){ buildNowPlaying(menuEl, L, it, !(it.enabled===false), depth, z); continue; }
        if(it && it.type==="rating"){ buildRating(menuEl, L, it, !(it.enabled===false), depth, z); continue; }
       )MENUHTML"
        LR"MENUHTML( if(it && it.type==="slider"){ buildSlider(menuEl, L, it, !(it.enabled===false), depth, z); continue; }
        if(it && it.type==="segmented"){ buildSegmented(menuEl, L, it, !(it.enabled===false), depth, z); continue; }
        var hasSub=!!(it && it.submenu && it.submenu.length);
        var en=!(it && it.enabled===false);
        var checkable=isExplicitCheckable(it);
        var d=document.createElement("div");
        d.className="fb-item nrm"+(!en?" disabled":"")+(it&&it.checked?" checked":"")+(hasSub?" has-sub":"");
        d.setAttribute("part","item");
        stampItem(d, it, depth, z);
        // ARIA: normal / checkable / submenu. Shared menu.show path benefits.
        var role=resolveNavRowRole({kind:hasSub?"submenu":(it&&it.type)||"normal", checkable:checkable, checked:!!(it&&it.checked)});
        d.setAttribute("role", role);
        d.setAttribute("tabindex","-1");
        applyDisabledAria(d, en);
        if(checkable) d.setAttribute("aria-checked", it&&it.checked?"true":"false");
        if(hasSub){ d.setAttribute("aria-haspopup","menu"); d.setAttribute("aria-expanded","false"); }
        if(layerHasIcon){
          var ic=document.createElement("span"); ic.className="fb-item-ico"; ic.setAttribute("part","item-icon"); ic.setAttribute("aria-hidden","true");
          var iv=it&&it.iconSvg;
          if(iv&&iv.content){ mountSanitizedSvgIcon(ic, iv.viewBox||"0 0 24 24", iv.content); }
          d.appendChild(ic);
        }
        var lab=document.createElement("span"); lab.setAttribute("part","item-label"); lab.textContent=(it&&it.label)||""; d.appendChild(lab);
        if(hasSub){ var ar=document.createElement("span"); ar.className="fb-arrow"; ar.setAttribute("part","submenu-arrow"); ar.setAttribute("aria-hidden","true"); ar.textContent="\u25B6"; d.appendChild(ar); }
        menuEl.appendChild(d);
        var row={el:d, item:it, navigable:(en&&(hasSub||(it&&it._token))), hasSub:hasSub, zone:z, checkable:checkable};
        var idx=L.rows.length; L.rows.push(row);
        (function(r, ridx){
          r.el.addEventListener("mouseenter", function(){
            if(interactionMode==="editor") return;
            if(r.navigable && r.hasSub && layers[depth+1] && layers[depth+1].parentRowIdx===ridx){ setActiveFromPointer(depth, ridx); return; }   // 幂等：本项子菜单已开则不重开（bug2 防抖）
            closeLayersFrom(depth+1);
            setActiveFromPointer(depth, ridx);
            if(r.navigable && r.hasSub) openSub(depth, ridx);
          });
          r.el.addEventListener("click", function(){
            if(!r.navigable) return;
            if(r.hasSub){ closeLayersFrom(depth+1); openSub(depth, ridx); setActive(depth+1, firstNav(depth+1)); }
            else select(r.item._token);
          });
        })(row, idx);
      }
    }

    function itemsHaveIcon(items){
      for(var hi=0;hi<(items||[]).length;hi++){ var hit=items[hi]; if(isNormalKind(hit)&&hit.iconSvg&&hit.iconSvg.content) return true; }
      return false;
    }

    function buildLayer(menuEl, items, depth, zone){
      var L={el:menuEl, rows:[], active:-1};
      layers[depth]=L; if(layers.length>depth+1) layers.length=depth+1;
      var layerHasIcon=itemsHaveIcon(items);
      if(layerHasIcon) menuEl.classList.add("has-icons");
      appendItems(menuEl, L, items||[], depth, zone, layerHasIcon);
      return L;
    }

    // Zones model: one navigation layer spanning all zones; icon column unified at root.
    function buildZonesRoot(menuEl, zones, depth){
      var L={el:menuEl, rows:[], active:-1};
      layers[depth]=L; if(layers.length>depth+1) layers.length=depth+1;
      var present=[];
      for(var zi=0;zi<(zones||[]).length;zi++){
        var z=zones[zi];
        if(z && z.items && z.items.length) present.push(z);
      }
      var layerHasIcon=false;
      for(var pi=0;pi<present.length;pi++){ if(itemsHaveIcon(present[pi].items)){ layerHasIcon=true; break; } }
      if(layerHasIcon) menuEl.classList.add("has-icons");
      for(var i=0;i<present.length;i++){
        if(i>0){
          var zs=document.createElement("div");
          zs.className="fb-sep fb-zone-separator";
          zs.setAttribute("part","separator");
          // Inter-zone separator inherits the preceding zone.
          var prevZone=present[i-1].id||"";
          stampSep(zs, depth, prevZone, true);
          menuEl.appendChild(zs);
        }
        var zoneId=present[i].id||"top";
        var zoneEl=document.createElement("div");
        zoneEl.className="fb-zone";
        zoneEl.setAttribute("data-zone", zoneId);
        menuEl.appendChild(zoneEl);
        appendItems(zoneEl, L, present[i].items, depth, zoneId, layerHasIcon);
      }
      return L;
    }

    function openSub(parentDepth, rowIdx){
      var L=layers[parentDepth]; if(!L || !L.rows[rowIdx]) return;
      var content = !!(cur && cur.windowModel==="contentSized");
      if(content && parentDepth>=1) return;   // 内容窗仅 1 层子菜单；2+ 层不展开（深层）
      if(cur && cur.windowModel==="submenu") return;  // independent submenu remains a leaf surface
      var parentEl=L.rows[rowIdx].el; var items=L.rows[rowIdx].item.submenu;
      var parentZone=L.rows[rowIdx].zone||itemZone(L.rows[rowIdx].item, "");
      var parentId=(L.rows[rowIdx].item&&L.rows[rowIdx].item.id)||"";
      var sub=document.createElement("div");
      sub.className="fb-menu fb-submenu fb-hidden";
      sub.setAttribute("part","submenu");
      sub.setAttribute("role","menu");
      sub.setAttribute("data-depth", String(parentDepth+1));
      if(parentId) sub.setAttribute("data-parent-item-id", parentId);
      if(parentZone) sub.setAttribute("data-zone", parentZone);
      // Insert hidden, build content, measure while invisible, position, then show.
      hideMenuEl(sub);
      mountHost().appendChild(sub);
      buildLayer(sub, items, parentDepth+1, parentZone);
      layers[parentDepth+1].parentRowIdx = rowIdx;   // 子菜单归属父项（hover 幂等，bug2）
      parentEl.setAttribute("aria-expanded","true");
      // Temporary measure )MENUHTML"
        LR"MENUHTML(pass: keep the node invisible to the user, then clear
      // inline display so the final visible state stays CSS-owned.
      // Off-screen measure must not steal focus.
      sub.style.setProperty("visibility","hidden","important");
      sub.style.setProperty("display","block","important");
      var pr=parentEl.getBoundingClientRect(), sw=sub.offsetWidth, sh=sub.offsetHeight;
      if(content){
        // The submenu is rendered by a distinct tight HWND. This temporary DOM
        // node is used only to measure the panel before its native host is
        // positioned; no DWM-backed reservation exists in the root HWND.
        var geom=placedGeometry||{rootSlotW:pr.right,subSlotW:Math.max(0,window.innerWidth-pr.right),viewportH:window.innerHeight,virtualViewportH:window.innerHeight,virtualRootTop:0};
        var subStyle=visibleSubmenuStyle({rootSlotW:geom.rootSlotW,subSlotW:geom.subSlotW,viewportH:geom.viewportH,parentTop:pr.top,submenuHeight:sh});
        applyContentSizedGeometry(sub,subStyle);
      } else {
        var x=pr.right-2; if(x+sw>window.innerWidth) x=Math.max(0, pr.left-sw+2);
        var y=pr.top-4; if(y+sh>window.innerHeight) y=Math.max(0, window.innerHeight-sh);
        sub.style.left=x+"px"; sub.style.top=y+"px";
      }
      sub.style.removeProperty("display");
      sub.style.removeProperty("visibility");
      showMenuEl(sub);
      if(content){
        var virtualPlacement=submenuWindowPlacement({
          rootSlotW:geom.rootSlotW, viewportH:geom.viewportH,
          virtualViewportH:geom.virtualViewportH, virtualRootTop:geom.virtualRootTop,
          parentTop:pr.top, submenuHeight:sh
        });
        var dpr=window.devicePixelRatio||1;
        reportSubmenuPanel({
          x:Math.round(virtualPlacement.x*dpr), y:Math.round(virtualPlacement.y*dpr),
          w:Math.ceil(sw*dpr), h:Math.ceil(virtualPlacement.h*dpr),
          parentToken:(L.rows[rowIdx].item&&L.rows[rowIdx].item._token)||""
        });
      }
    }

    function placeRoot(menuEl, x, y){
      var aw=window.innerWidth, ah=window.innerHeight, mw=menuEl.offsetWidth, mh=menuEl.offsetHeight;
      if(x+mw>aw) x=Math.max(0, aw-mw);
      if(y+mh>ah) y=Math.max(0, ah-mh);
      menuEl.style.left=x+"px"; menuEl.style.top=y+"px";
    }

    function render(st){
      cur=st;
      cleanupMenuInteraction();
      closeLayersFrom(0); layers.length=0;
      var root=document.getElementById("menu"); root.innerHTML=""; root.classList.remove("in"); root.classList.remove("out");
      root.classList.toggle("fb-submenu", !!(st && st.windowModel==="submenu"));
      clearContentSizedGeometry(root);
      placedGeometry=null;
      root.setAttribute("role","menu");
      root.setAttribute("data-depth","0");
      var content = !!(st && st.windowModel==="contentSized");
      var submenuSurface = !!(st && st.windowModel==="submenu");
      // 宿主下发的 UI 语言（源自 C++ utils/I18n.h 的 fb2k UI 语言判定）。
      // 必须在 buildZonesRoot / buildLayer 之前赋值：aria-label 在建行时就生成。
      uiLocale = (st && st.locale === "zh") ? "zh" : "en";
      document.body.classList.toggle("content-sized", content);
      var useZones = !!(st && st.overlayModel==="trayZones" && st.layoutMode==="zones" && st.zones && st.zones.length);
      var hasContent = useZones
        ? st.zones.some(function(z){ return z && z.items && z.items.length; })
        : !!(st && st.items && st.items.length);
      if(!st || !st.visible || !hasContent){ hideMenuEl(root); return; }
      if(useZones) buildZonesRoot(root, st.zones, 0);
      else buildLayer(root, st.items, 0, "");
      // 前端样式接管（S-CSS）：每次 render 覆盖 fb-user 防串味；cssReplace=true 时禁用默认样式（仅留 fb-user+fb-protected）。
      // 务必在 measureAndReport / placeRoot 之前应用，保证测量基于最终样式。
      var userStyle=document.getElementById("fb-user");
      if(userStyle) userStyle.textContent=(st.css||"");
      var defStyle=document.getElementById("fb-default");
      if(defStyle) defStyle.disabled=!!st.cssReplace;
      showMenuEl(root);
      if(content){
        // 内容窗：菜单贴客户区原点；窗口尺寸/定位由 C++ OnContentMeasured 据测量回报完成。
        // Off-screen measure must not steal focus — focus after first placed.
        applyContentSizedGeometry(root,{left:"0px",top:"0px",maxWidth:"none",maxHeight:"none"});
        setActive(0, firstNav(0), {focus:false});
        pendingRootFocus=true;
        measureAndReport();
      } else if(!submenuSurface) {
        var dpr=window.devicePixelRatio||1;   // anchor 为物理像素，转 CSS 像素再定位（高 DPI 修正）
        placeRoot(root, (st.anchorX||0)/dpr, (st.anchorY||0)/dpr);
        setActive(0, firstNav(0));   // WAI-ARIA：菜单打开焦点置首项
      }
      if(submenuSurface){
        setActive(0, firstNav(0), {focus:false});
        reportSubmenuSurfaceReady();
      }
    }

    function reportSubmenuPanel(rect){
      try{
        if(!(window.fb2k&&fb2k.invoke)) return;
        if(!rect) submenuPanelOpen=false;
        var payload={menuId:(cur&&cur.menuId)||"",visible:!!rect,sequence:++submenuPanelSequence};
        if(rect){ payload.x=rect.x; payload.y=rect.y; payload.w=rect.w; payload.h=rect.h; payload.parentToken=rect.parentToken||""; }
        var pending=fb2k.invoke("menu.__submenuPanel",payload);
        if(rect&&pending&&pending.then){ pending.then(function(result){
          if((!result||result.success===false)&&!submenuPanelOpen){ requestAnimationFrame(function(){ reportSubmenuPanel(rect); }); }
        }); }
      }catch(e){}
    }

    function measureElement(el){
      var r=el.getBoundingClientRect();
      return {w:Math.max(el.scrollWidth,Math.ceil(r.width)),h:Math.max(el.scrollHeight,Math.ceil(r.height))};
    }

    function measureFirstLevelSubmenus(){
      var out=[];
      var rootLayer=layers[0]; if(!rootLayer) return out;
      for(var i=0;i<rootLayer.rows.length;i++){
        var row=rootLayer.rows[i];
        if(!row||!row.hasSub||!row.item||!row.item.submenu) continue;
        var sub=document.createElement("div");
        sub.className="fb-menu fb-submenu fb-measure-submenu";
        sub.setAttribute)MENUHTML"
        LR"MENUHTML(("role","menu");
        sub.style.setProperty("position","fixed","important");
        sub.style.setProperty("left","-32000px","important");
        sub.style.setProperty("top","-32000px","important");
        sub.style.setProperty("display","block","important");
        sub.style.setProperty("visibility","hidden","important");
        sub.style.setProperty("max-width","none","important");
        sub.style.setProperty("max-height","none","important");
        mountHost().appendChild(sub);
        var oldLength=layers.length;
        buildLayer(sub,row.item.submenu,1,row.zone||"");
        out.push(measureElement(sub));
        if(sub.parentNode) sub.parentNode.removeChild(sub);
        layers.length=oldLength;
        layers[0]=rootLayer;
      }
      return out;
    }

    // Wait for fonts and two equal animation-frame measurements before reporting
    // actual root + first-level submenu maxima. The host validates all integers
    // before atomically consuming the measure state.
    var _measureRaf=0;
    var _measureGeneration=0;
    function measureAndReport(){
      if(!cur || cur.windowModel!=="contentSized") return;
      if(_measureRaf){ try{ cancelAnimationFrame(_measureRaf); }catch(e){} }
      var generation=++_measureGeneration;
      var begin=function(){
        var previous=null, equalFrames=0;
        function sample(){
          _measureRaf=0;
          if(generation!==_measureGeneration||!cur||cur.windowModel!=="contentSized") return;
          var root=document.getElementById("menu"); if(!root) return;
          var payload=buildMeasurePayload({menuId:(cur&&cur.menuId)||"",root:measureElement(root),submenus:measureFirstLevelSubmenus(),dpr:window.devicePixelRatio||1});
          if(payload.root.w<=0||payload.root.h<=0) return;
          if(sameMeasurePayload(previous,payload)) equalFrames++; else equalFrames=0;
          previous=payload;
          if(equalFrames>=1){
            try{ if(window.fb2k&&fb2k.invoke) fb2k.invoke("menu.__ready",payload); }catch(e){}
            return;
          }
          _measureRaf=requestAnimationFrame(sample);
        }
        _measureRaf=requestAnimationFrame(sample);
      };
      try{
        if(document.fonts&&document.fonts.ready&&document.fonts.ready.then){ document.fonts.ready.then(begin,begin); }
        else begin();
      }catch(e){ begin(); }
    }

    function onPlaced(ev){
      // 固定窗设计：窗口不 resize，子菜单无需隐藏淡入；仅首次定位触发入场动画。
      if(ev && ev.first){
        applyPlacedGeometry(ev);
        var root=document.getElementById("menu");
        requestAnimationFrame(function(){
          root.classList.add("in");
          // ContentSized: real focus only after first visible place.
          if(pendingRootFocus){
            pendingRootFocus=false;
            setActive(0, layers[0]?layers[0].active:firstNav(0), {focus:true});
          }
        });
      }
    }

    // Single window-capture keydown: mode split first, handle once.
    function onKey(e){
      if(!layers.length) return;
      // Editor mode: only editor handler; navigation never sees the same key.
      if(interactionMode==="editor"){
        if(editorCtx && editorCtx.row && editorCtx.row.editorKey){
          if(editorCtx.row.editorKey(e)) e.preventDefault();
        }
        return;
      }
      var depth=layers.length-1, L=layers[depth]; if(!L) return;
      var r=L.rows[L.active];
      switch(e.key){
        case "ArrowDown": moveActive(depth,1); e.preventDefault(); break;
        case "ArrowUp":   moveActive(depth,-1); e.preventDefault(); break;
        case "Home": { var fi=firstNav(depth); if(fi>=0) setActive(depth,fi); e.preventDefault(); break; }
        case "End":  { var li=lastNav(depth); if(li>=0) setActive(depth,li); e.preventDefault(); break; }
        case "ArrowRight": {
          if(r&&r.navigable&&r.hasSub){ openSub(depth,L.active); setActive(depth+1, firstNav(depth+1)); e.preventDefault(); break; }
          if(r&&isRichKind(r.kind)){ enterEditor(depth, L.active); e.preventDefault(); break; }
          e.preventDefault(); break;
        }
        case "ArrowLeft": {
          var leftAction=resolveMenuArrowLeftAction({windowModel:cur&&cur.windowModel,depth:depth});
          if(leftAction.kind==="dismissSubmenu"){
            // The independent submenu HWND has depth zero. Ask its host to
            // close only this child surface and restore the root parent row.
            dismiss(leftAction.reason);
          } else if(leftAction.kind==="closeLayer"){
            var parentIdx=(layers[depth]&&layers[depth].parentRowIdx!=null)?layers[depth].parentRowIdx:-1;
            closeLayersFrom(depth);
            if(parentIdx>=0) setActive(depth-1, parentIdx, {focus:true});
          }
          e.preventDefault(); break;
        }
        case "Enter": case " ": {
          if(r&&r.navigable){
            if(r.hasSub){ openSub(depth,L.active); setActive(depth+1, firstNav(depth+1)); }
            else if(isRichKind(r.kind)){ enterEditor(depth, L.active); }
            else select(r.item._token);
          }
          e.preventDefault(); break;
        }
        case "Escape":
          if(depth>0){
            var pIdx=(layers[depth]&&layers[depth].parentRowIdx!=null)?layers[depth].parentRowIdx:-1;
            closeLayersFrom(depth);
            if(pIdx>=0) setActive(depth-1, pIdx, {focus:true});
          } else { dismiss("escape"); }
          e.preventDefault(); break;
        default: break;
      }
    }

    function pull(){ try{ if(window.fb2k&&fb2k.invoke){ var p=fb2k.invoke("menu.__getMenuState"); if(p&&p.then) p.then(render); } }catch(e){} }
    function inMenu(t){ return t && t.closest && t.closest(".fb-menu"); }
    window.addEventListener("mousedown", function(e){ if(!inMenu(e.target)) dismiss("outside"); }, true);
    window.addEventListener("contextmenu", function(e){ e.preventDefault(); if(!inMenu(e.target)) dismiss("outside"); }, true);
    window.addEventListener("keydown", onKey, true);
    if(window.fb2k && fb2k.on){ f)MENUHTML"
        LR"MENUHTML(b2k.on("menu:show", pull); fb2k.on("menu:__placed", onPlaced); fb2k.on("menu:__submenuOpened", function(){ submenuPanelOpen=true; }); fb2k.on("menu:__submenuVisible", function(){ setActive(0,firstNav(0),{focus:true}); }); fb2k.on("menu:__submenuClosed", function(){ submenuPanelOpen=false; var parentIdx=(layers[1]&&layers[1].parentRowIdx!=null)?layers[1].parentRowIdx:-1; closeLayersFrom(1); if(parentIdx>=0) setActive(0,parentIdx,{focus:true}); }); fb2k.on("menu:__hide", function(){ cleanupMenuInteraction(); var root=document.getElementById("menu"); if(root){ root.classList.remove("in"); root.classList.add("out"); } }); }
    pull();
</script>
</body></html>
)MENUHTML";
}
