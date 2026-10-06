/**
 * Locale-switch hash preservation helpers for VitePress 1.6.4.
 *
 * useLangs() may append hash.value to locale menu links, but the SPA router
 * still installs a window-level capture click handler before theme enhanceApp.
 * When the rendered href lacks a fragment, router.go() navigates without hash.
 * A theme listener that only preventDefault + location.assign can still lose:
 * router runs first on window capture, and loadPage may replaceState without
 * the fragment.
 *
 * Contract:
 * - Detect locale-switch anchors only (label and/or translations menu).
 * - Append live location.hash when the href has none.
 * - stopImmediatePropagation + preventDefault when forcing navigation.
 * - onBeforeRouteChange: rewrite hash-less corresponding-locale navigations.
 *
 * @module locale-hash
 */

/** @type {readonly string[]} */
export const LOCALE_SWITCH_LABELS = Object.freeze([
  '简体中文',
  '中文（根路由过渡）',
  'English',
]);

export const LOCALE_MENU_SELECTORS =
  '.VPMenu, .VPNavBarTranslations, .VPNavScreenTranslations';

export const DOCS_BASE = '/foo_ui_webview2';
export const PUBLIC_ORIGIN = 'https://nereafantasia.github.io';

/**
 * @typedef {{ action: 'ignore' } | { action: 'navigate', href: string }} LocaleHashDecision
 */

/**
 * @typedef {object} LocaleHashClickInput
 * @property {string | null | undefined} hrefAttr
 * @property {string | null | undefined} linkText
 * @property {boolean} inLangMenu
 * @property {string} currentHash
 * @property {string} origin
 */

/**
 * Strip site base and optional /zh locale prefix → comparable relative path.
 * @param {string} pathname
 * @param {string} [base]
 * @returns {string}
 */
export function localeRelativePath(pathname, base = DOCS_BASE) {
  let p = pathname || '/';
  try {
    // Allow full URLs
    if (/^https?:/i.test(p)) p = new URL(p).pathname;
  } catch {
    /* keep p */
  }
  if (!p.startsWith('/')) p = '/' + p;
  const baseNorm = base.endsWith('/') ? base.slice(0, -1) : base;
  if (p === baseNorm || p === baseNorm + '/') p = '/';
  else if (p.startsWith(baseNorm + '/')) p = p.slice(baseNorm.length);
  // p is now like /guide/overview.html or /zh/guide/overview.html
  if (p === '/zh' || p === '/zh/') return '/';
  if (p.startsWith('/zh/')) p = p.slice(3);
  // normalize trailing index
  p = p.replace(/\/index\.html$/, '/').replace(/\/index$/, '/');
  if (p.length > 1 && p.endsWith('/')) {
    // keep directory trailing slash semantics for components/
  }
  return p || '/';
}

/**
 * True when two pathnames are the same page under different locales.
 * @param {string} fromPathname
 * @param {string} toHrefOrPath
 * @param {string} [base]
 * @returns {boolean}
 */
export function isCorrespondingLocalePath(
  fromPathname,
  toHrefOrPath,
  base = DOCS_BASE,
) {
  let toPath = toHrefOrPath;
  try {
    toPath = new URL(toHrefOrPath, 'https://vitepress.local').pathname;
  } catch {
    /* keep */
  }
  const a = localeRelativePath(fromPathname, base);
  const b = localeRelativePath(toPath, base);
  if (a !== b) return false;
  // Must actually cross locale (root <-> zh), not same-locale navigation.
  const fromIsZh =
    /\/zh(\/|$)/.test(
      fromPathname.includes(base) ? fromPathname : base + fromPathname,
    ) || localePathIsZh(fromPathname, base);
  const toIsZh = localePathIsZh(toPath, base);
  return fromIsZh !== toIsZh;
}

/**
 * @param {string} pathname
 * @param {string} [base]
 * @returns {boolean}
 */
function localePathIsZh(pathname, base = DOCS_BASE) {
  let p = pathname || '/';
  try {
    if (/^https?:/i.test(p)) p = new URL(p).pathname;
  } catch {
    /* keep */
  }
  const baseNorm = base.endsWith('/') ? base.slice(0, -1) : base;
  if (p.startsWith(baseNorm + '/')) p = p.slice(baseNorm.length);
  else if (p === baseNorm) p = '/';
  return p === '/zh' || p === '/zh/' || p.startsWith('/zh/');
}

/**
 * Build the public SEO URLs for the current root/zh route.
 * This mirrors config.ts transformHead for client-side SPA navigation.
 *
 * @param {string} pathname
 * @param {string} [base]
 * @param {string} [origin]
 * @returns {{ canonical: string, en: string, zh: string, xDefault: string }}
 */
export function localeSeoUrls(
  pathname,
  base = DOCS_BASE,
  origin = PUBLIC_ORIGIN,
) {
  const relative = localeRelativePath(pathname, base);
  let suffix = relative;
  if (relative === '/') suffix = '/';
  else if (!relative.startsWith('/')) suffix = `/${relative}`;
  const normalizedBase = base.endsWith('/') ? base.slice(0, -1) : base;
  const normalizedOrigin = origin.endsWith('/') ? origin.slice(0, -1) : origin;
  const en = `${normalizedOrigin}${normalizedBase}${suffix}`;
  const zh = `${normalizedOrigin}${normalizedBase}/zh${suffix}`;
  return {
    canonical: localePathIsZh(pathname, base) ? zh : en,
    en,
    zh,
    xDefault: en,
  };
}

/**
 * @param {LocaleHashClickInput} input
 * @returns {LocaleHashDecision}
 */
export function resolveLocaleHashNavigation(input) {
  const hrefAttr = (input.hrefAttr || '').trim();
  if (!hrefAttr) return { action: 'ignore' };
  if (/^(https?:|mailto:|tel:|javascript:)/i.test(hrefAttr)) {
    return { action: 'ignore' };
  }

  const text = (input.linkText || '').trim();
  const isLocaleLabel = LOCALE_SWITCH_LABELS.includes(text);
  if (!isLocaleLabel && !input.inLangMenu) return { action: 'ignore' };

  const hash = input.currentHash || '';
  try {
    const url = new URL(hrefAttr, input.origin || 'https://127.0.0.1');
    if (
      !url.pathname.includes(DOCS_BASE) &&
      !/^\/(zh\/?)?$/.test(url.pathname) &&
      !url.pathname.startsWith('/zh/') &&
      !/^\/(tutorials|how-to|concepts|guide|api|sdk|mcp|reference|components)(\/|$)/.test(url.pathname)
    ) {
      return { action: 'ignore' };
    }

    const nextHash = url.hash || hash;
    if (!nextHash) return { action: 'ignore' };

    return {
      action: 'navigate',
      href: url.pathname + url.search + nextHash,
    };
  } catch {
    return { action: 'ignore' };
  }
}

/**
 * Apply a navigate decision. stopImmediatePropagation is required so a later
 * same-target capture listener cannot call router.go() with a hash-less href.
 *
 * @param {{
 *   preventDefault: () => void,
 *   stopPropagation?: () => void,
 *   stopImmediatePropagation?: () => void,
 * }} event
 * @param {LocaleHashDecision} decision
 * @param {{ assign: (href: string) => void }} locationLike
 * @returns {boolean}
 */
export function applyLocaleHashClick(event, decision, locationLike) {
  if (decision.action !== 'navigate') return false;
  event.preventDefault();
  if (typeof event.stopImmediatePropagation === 'function') {
    event.stopImmediatePropagation();
  } else if (typeof event.stopPropagation === 'function') {
    event.stopPropagation();
  }
  locationLike.assign(decision.href);
  return true;
}

/**
 * If pending hash is missing from href, return corrected href; else null.
 * @param {string} href
 * @param {string} pendingHash
 * @returns {string | null}
 */
export function rewriteHrefWithPendingHash(href, pendingHash) {
  if (!pendingHash?.startsWith('#')) return null;
  try {
    const url = new URL(href, 'https://vitepress.local');
    if (url.hash) return null;
    return url.pathname + url.search + pendingHash;
  } catch {
    return null;
  }
}
