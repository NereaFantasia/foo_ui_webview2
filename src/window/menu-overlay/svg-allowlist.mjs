// SVG allowlist predicates for the self-drawn menu icon sanitizer (DESIGN 8.4).
// Pure logic only — no DOM. The overlay page inlines a browser build of these
// predicates plus a DOMParser-based cloner; node:test imports this module
// directly. Keep both paths in sync via scripts/gen_menu_overlay_page.mjs.

export const ALLOWED_ELEMENTS = Object.freeze([
  'path', 'circle', 'rect', 'line', 'polyline', 'polygon', 'ellipse', 'g',
]);

export const COMMON_ATTRS = Object.freeze([
  'fill', 'fill-rule', 'fill-opacity', 'stroke', 'stroke-width',
  'stroke-linecap', 'stroke-linejoin', 'stroke-opacity', 'opacity', 'transform',
]);

export const ELEMENT_ATTRS = Object.freeze({
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

export function isAllowedElement(name) {
  if (!name) return false;
  return ALLOWED_ELEMENTS.includes(String(name).toLowerCase());
}

export function isAllowedAttribute(elementName, attrName) {
  const el = String(elementName || '').toLowerCase();
  const attr = String(attrName || '').toLowerCase();
  if (!attr || attr.startsWith('on')) return false;
  if (attr === 'href' || attr === 'xlink:href' || attr.includes(':')) return false;
  if (COMMON_ATTRS.includes(attr)) return true;
  const extras = ELEMENT_ATTRS[el];
  return !!(extras && extras.includes(attr));
}

export function isFiniteInRange(n) {
  return typeof n === 'number' && Number.isFinite(n) && Math.abs(n) <= MAX_ABS;
}

export function parseNumberToken(s) {
  if (s == null) return null;
  const t = String(s).trim();
  if (!t) return null;
  if (!/^[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?$/.test(t)) return null;
  const n = Number(t);
  return isFiniteInRange(n) ? n : null;
}

export function isValidViewBox(value) {
  if (value == null) return false;
  const parts = String(value).trim().split(/[\s,]+/).filter(Boolean);
  if (parts.length !== 4) return false;
  const nums = parts.map(parseNumberToken);
  if (nums.some((n) => n == null)) return false;
  const [, , w, h] = nums;
  return w > 0 && h > 0;
}

export function isValidOpacity(value) {
  const n = parseNumberToken(value);
  return n != null && n >= 0 && n <= 1;
}

export function isValidFillOrStroke(value) {
  if (value == null) return false;
  const v = String(value).trim().toLowerCase();
  if (v.includes('url(')) return false;
  return v === 'currentcolor' || v === 'none';
}

export function isValidFillRule(value) {
  const v = String(value || '').trim().toLowerCase();
  return v === 'nonzero' || v === 'evenodd';
}

export function isValidLinecap(value) {
  const v = String(value || '').trim().toLowerCase();
  return v === 'butt' || v === 'round' || v === 'square';
}

export function isValidLinejoin(value) {
  const v = String(value || '').trim().toLowerCase();
  return v === 'miter' || v === 'round' || v === 'bevel';
}

export function isValidNumericAttr(value) {
  return parseNumberToken(value) != null;
}

/** d / points: path commands + numbers + commas + whitespace only; all numbers in range. */
export function isValidPathData(value) {
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
export function isValidTransform(value) {
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
    while (i < n && /[A-Za-z]/.test(s[i])) i += 1;
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
export function isSvgNamespace(uri) {
  return uri === 'http://www.w3.org/2000/svg';
}

export function isValidAttributeValue(elementName, attrName, value) {
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

export function isOversizedSvgContent(content) {
  return Buffer.byteLength(String(content || ''), 'utf8') > MAX_SINGLE_SVG_BYTES
    || String(content || '').length > MAX_SINGLE_SVG_BYTES;
}

/** Structural reject list for raw content before DOM parse (defense in depth). */
export function contentLooksUnsafe(content) {
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
export const BROWSER_MOUNT_HELPER = String.raw`
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

  var svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  svg.setAttribute('viewBox', vb);
  svg.setAttribute('width', '16');
  svg.setAttribute('height', '16');
  svg.setAttribute('aria-hidden', 'true');
  if (!cloneAllowed(src, svg)) return false;
  host.textContent = '';
  host.appendChild(svg);
  return true;
}
`;
