/**
 * Resolution rules for redirects.json, shared by the build (which writes a redirect page for
 * every page that moved) and the theme (which forwards a fragment whose section moved to
 * another place while its page stayed). Nothing here touches the file system or the DOM.
 *
 * Pages are named by their source path relative to docs/vitepress, the way VitePress names
 * them: `guide/overview.md`, `zh/guide/overview.md`, `components/index.md`. A locale has its
 * own entries because a Chinese heading has a different anchor from its English one.
 *
 * An entry of redirects.json is keyed by the page it applies to:
 *
 *   "sdk/navigation.md": { "to": "sdk/queue.md", "anchors": { "remove-index": "sdk/queue.md#remove-target" } }
 *
 *   to        the page moved there; its old file must be gone. May carry a fragment, used when
 *             the old URL has none.
 *   anchors   { old id: "page.md#new id" } for sections that went somewhere else than `to`.
 *             An entry with only `anchors` keeps its page.
 *
 * A fragment the entry does not map follows the page to its new place unchanged.
 *
 * @module redirects
 */

/**
 * @typedef {{ to?: string, anchors?: Record<string, string> }} RedirectEntry
 * @typedef {Record<string, RedirectEntry>} RedirectMap
 * @typedef {{ page: string, id: string }} PageLocation
 */

/**
 * Split `page.md#id` into its page and its decoded-form id.
 * @param {string} target
 * @returns {PageLocation}
 */
export function splitTarget(target) {
  const hash = target.indexOf('#');
  return hash < 0 ? { page: target, id: '' } : { page: target.slice(0, hash), id: target.slice(hash + 1) };
}

/**
 * Where a page and fragment live now. Follows entries until none applies, so a page that
 * moved twice resolves to its last place.
 *
 * @param {RedirectMap} redirects
 * @param {string} page source path of the requested page
 * @param {string} id decoded fragment without `#`; empty for none
 * @returns {PageLocation | null} null when no entry applies
 * @throws {Error} when the entries form a cycle
 */
export function resolveMoved(redirects, page, id) {
  let current = { page, id };
  let moved = false;
  const seen = new Set();
  for (;;) {
    const key = `${current.page}#${current.id}`;
    if (seen.has(key)) throw new Error(`redirect cycle through ${key}`);
    seen.add(key);
    const entry = Object.hasOwn(redirects, current.page) ? redirects[current.page] : undefined;
    if (!entry) break;
    const anchored = current.id && entry.anchors && Object.hasOwn(entry.anchors, current.id) ? entry.anchors[current.id] : undefined;
    if (anchored) {
      current = splitTarget(anchored);
    } else if (entry.to) {
      const next = splitTarget(entry.to);
      current = { page: next.page, id: current.id || next.id };
    } else {
      break;
    }
    moved = true;
  }
  return moved ? current : null;
}

/**
 * The URL path of a page below the site base, as VitePress publishes it without clean URLs:
 * `a/b.md` is `a/b.html`, `a/index.md` is `a/`, `index.md` is the empty string.
 * @param {string} page
 * @returns {string}
 */
export function pageUrlPath(page) {
  if (page === 'index.md') return '';
  if (page.endsWith('/index.md')) return page.slice(0, -'index.md'.length);
  return page.replace(/\.md$/, '.html');
}

/**
 * The source path of the page a URL path names, the inverse of pageUrlPath. Also accepts the
 * extension-less form GitHub Pages serves (`a/b` for `a/b.html`).
 * @param {string} pathname URL path, with or without the site base
 * @param {string} base site base such as `/foo_ui_webview2/`
 * @returns {string}
 */
export function pageFromPathname(pathname, base) {
  let p = pathname;
  try {
    p = decodeURI(p);
  } catch {
    // A malformed escape keeps the raw path; it then names no page.
  }
  const root = base.endsWith('/') ? base : `${base}/`;
  if (p.startsWith(root)) p = p.slice(root.length);
  else if (`${p}/` === root) p = '';
  p = p.replace(/^\/+/, '');
  if (p === '' || p.endsWith('/')) return `${p}index.md`;
  if (p.endsWith('.html')) return `${p.slice(0, -'.html'.length)}.md`;
  return `${p}.md`;
}

/**
 * The site-relative URL of a location, with the fragment percent-encoded.
 * @param {PageLocation} location
 * @param {string} base site base such as `/foo_ui_webview2/`
 * @returns {string}
 */
export function locationUrl(location, base) {
  const root = base.endsWith('/') ? base : `${base}/`;
  const path = encodeURI(root + pageUrlPath(location.page));
  return location.id ? `${path}#${encodeURIComponent(location.id)}` : path;
}
