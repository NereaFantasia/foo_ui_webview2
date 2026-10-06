/**
 * Keeps every URL the docs site has published working after pages move, merge or split.
 *
 * published-pages.json lists the source path of every page the site has published, in both
 * locales, and its entries are permanent. A listed page must either still exist or resolve
 * through redirects.json (rules in redirects.mjs) to a page that does. After a build, a small
 * page is written at the old URL of every page that moved; it forwards the visitor, fragment
 * included, and the build fails when a target page or anchor is missing from the output.
 *
 *   node docs/vitepress/.vitepress/published-urls.mjs --check   report problems, exit 1 if any
 *   node docs/vitepress/.vitepress/published-urls.mjs --add     list every current page as published
 *
 * --check also fails on a page the list does not have yet, so a new page is listed in the
 * commit that adds it and is protected from the day it ships. An entry for a page that never
 * shipped may be removed by hand.
 *
 * The redirect pages exist only in the build output; `vitepress dev` answers an old URL with
 * its 404 page.
 *
 * @module published-urls
 */
import fs from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import { fileURLToPath } from 'node:url';

import { locationUrl, resolveMoved, splitTarget } from './redirects.mjs';

// The docs root when run as a script. VitePress bundles this module into its config, so the
// build passes its srcDir instead of relying on where the module file sits.
export const DOCS_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const dataFile = (docsRoot, name) => path.join(docsRoot, '.vitepress', name);

const SKIPPED_DIRS = new Set(['node_modules', 'dist', 'public']);

/**
 * Source paths of the pages VitePress builds, relative to the docs root, sorted.
 * @param {string} docsRoot
 * @returns {string[]}
 */
export function listSourcePages(docsRoot) {
  const out = [];
  (function walk(dir) {
    for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
      if (e.name.startsWith('.') || SKIPPED_DIRS.has(e.name)) continue;
      const abs = path.join(dir, e.name);
      if (e.isDirectory()) walk(abs);
      else if (e.name.endsWith('.md')) out.push(path.relative(docsRoot, abs).split(path.sep).join('/'));
    }
  })(docsRoot);
  return out.sort();
}

function readJson(file, fallback) {
  return fs.existsSync(file) ? JSON.parse(fs.readFileSync(file, 'utf8')) : fallback;
}

/**
 * @param {string} docsRoot
 * @returns {{ published: string[], redirects: import('./redirects.mjs').RedirectMap }}
 */
export function loadUrlData(docsRoot) {
  return {
    published: readJson(dataFile(docsRoot, 'published-pages.json'), []),
    redirects: readJson(dataFile(docsRoot, 'redirects.json'), {}),
  };
}

const isPage = (s) => typeof s === 'string' && /^[^#\s]+\.md$/.test(s);
const isTarget = (s) => typeof s === 'string' && /^[^#\s]+\.md(#.+)?$/.test(s);

/**
 * Everything wrong with the published list and the redirect map, as one sentence each.
 *
 * @param {{ pages: string[], published: string[], redirects: import('./redirects.mjs').RedirectMap, requireListed?: boolean }} input
 *   pages: the source pages that exist now; requireListed: also report pages the list lacks
 * @returns {string[]}
 */
export function checkPublishedUrls({ pages, published, redirects, requireListed = true }) {
  const problems = [];
  const exists = new Set(pages);
  const listed = new Set(published);
  const settle = (page, id, what) => {
    try {
      const loc = resolveMoved(redirects, page, id) ?? { page, id };
      if (!exists.has(loc.page)) problems.push(`${what} ends at ${loc.page}, which does not exist`);
    } catch (e) {
      problems.push(`${what}: ${e.message}`);
    }
  };

  for (const [key, entry] of Object.entries(redirects)) {
    if (!isPage(key)) {
      problems.push(`redirects.json: "${key}" is not a page path such as guide/overview.md`);
      continue;
    }
    if (!entry || typeof entry !== 'object' || Array.isArray(entry)) {
      problems.push(`redirects.json: ${key} must be an object with "to" and/or "anchors"`);
      continue;
    }
    const extra = Object.keys(entry).filter((k) => k !== 'to' && k !== 'anchors');
    if (extra.length) problems.push(`redirects.json: ${key} has unknown key(s) ${extra.join(', ')}`);
    const anchors = entry.anchors ?? {};
    if (entry.to !== undefined && !isTarget(entry.to)) problems.push(`redirects.json: ${key} "to" must be a page path, optionally with #id`);
    if (typeof anchors !== 'object' || Array.isArray(anchors)) {
      problems.push(`redirects.json: ${key} "anchors" must map ids to page.md#id`);
      continue;
    }
    for (const [id, target] of Object.entries(anchors)) {
      if (!isTarget(target) || !splitTarget(target).id) problems.push(`redirects.json: ${key} anchor ${id} must point at page.md#id`);
    }
    if (entry.to !== undefined) {
      if (exists.has(key)) problems.push(`redirects.json: ${key} still exists, so its redirect would replace it; remove the page or the "to"`);
      settle(key, '', `redirect of ${key}`);
    } else if (!Object.keys(anchors).length) {
      problems.push(`redirects.json: ${key} has neither "to" nor "anchors"`);
    } else if (!exists.has(key)) {
      problems.push(`redirects.json: ${key} maps anchors but no longer exists; give it a "to"`);
    }
    for (const id of Object.keys(anchors)) settle(key, id, `anchor ${key}#${id}`);
  }

  for (const page of published) {
    if (exists.has(page)) continue;
    let moved = null;
    try {
      moved = resolveMoved(redirects, page, '');
    } catch {
      // The cycle is already reported for its entry.
      continue;
    }
    if (!moved) problems.push(`${page} was published and is gone; add its new place to redirects.json`);
  }
  if (requireListed) {
    for (const page of pages) {
      if (!listed.has(page)) problems.push(`${page} is not in published-pages.json; run \`node docs/vitepress/.vitepress/published-urls.mjs --add\``);
    }
  }
  return problems;
}

const escapeHtml = (s) => s.replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/</g, '&lt;').replace(/>/g, '&gt;');

// The ids an old URL may carry that some entry on its way maps elsewhere.
function mappedIds(redirects, page) {
  const ids = new Set();
  const seen = new Set();
  let current = page;
  while (Object.hasOwn(redirects, current) && !seen.has(current)) {
    seen.add(current);
    const entry = redirects[current];
    for (const id of Object.keys(entry.anchors ?? {})) ids.add(id);
    if (!entry.to) break;
    current = splitTarget(entry.to).page;
  }
  return [...ids];
}

/**
 * The redirect page for every entry with a "to", keyed by its output file relative to outDir.
 * Script navigation keeps the fragment; without script a meta refresh goes to the page itself.
 *
 * @param {{ redirects: import('./redirects.mjs').RedirectMap, base: string, origin: string }} input
 * @returns {{ file: string, html: string }[]}
 */
export function renderRedirectPages({ redirects, base, origin }) {
  const out = [];
  for (const [key, entry] of Object.entries(redirects)) {
    if (!entry.to) continue;
    const landing = resolveMoved(redirects, key, '');
    const pageUrl = locationUrl({ page: landing.page, id: '' }, base);
    const fallback = locationUrl(landing, base);
    const anchors = Object.fromEntries(mappedIds(redirects, key).map((id) => [id, locationUrl(resolveMoved(redirects, key, id), base)]));
    const zh = key.startsWith('zh/');
    const text = zh
      ? { lang: 'zh-CN', title: '页面已移动', body: (a) => `此页已移至 ${a}。` }
      : { lang: 'en-US', title: 'Page moved', body: (a) => `This page has moved to ${a}.` };
    const data = JSON.stringify({ anchors, page: pageUrl, fallback }).replace(/</g, '\\u003c');
    const link = `<a href="${escapeHtml(fallback)}">${escapeHtml(decodeURI(pageUrl))}</a>`;
    const html = [
      '<!doctype html>',
      `<html lang="${text.lang}">`,
      '<head>',
      '<meta charset="utf-8">',
      `<title>${text.title}</title>`,
      '<meta name="robots" content="noindex">',
      `<link rel="canonical" href="${escapeHtml(origin + pageUrl)}">`,
      `<script>(function(){var d=${data};var raw=location.hash.slice(1),id=raw;try{id=decodeURIComponent(raw)}catch(e){}location.replace(raw?(Object.prototype.hasOwnProperty.call(d.anchors,id)?d.anchors[id]:d.page+'#'+raw):d.fallback)})()</script>`,
      `<noscript><meta http-equiv="refresh" content="0; url=${escapeHtml(fallback)}"></noscript>`,
      '</head>',
      `<body><p>${text.body(link)}</p></body>`,
      '</html>',
      '',
    ].join('\n');
    out.push({ file: key.replace(/\.md$/, '.html'), html });
  }
  return out;
}

/**
 * Every place a redirect sends visitors that must exist in the build output: the landing page
 * of each moved page, and each mapped anchor with its id.
 *
 * @param {{ outDir: string, redirects: import('./redirects.mjs').RedirectMap }} input
 * @returns {string[]} problems
 */
export function checkBuiltTargets({ outDir, redirects }) {
  const problems = [];
  const cache = new Map();
  const htmlOf = (page) => {
    if (!cache.has(page)) {
      const file = path.join(outDir, page.replace(/\.md$/, '.html'));
      cache.set(page, fs.existsSync(file) ? fs.readFileSync(file, 'utf8') : null);
    }
    return cache.get(page);
  };
  const expect = (loc, what) => {
    const html = htmlOf(loc.page);
    if (html === null) problems.push(`${what} goes to ${loc.page}, which the build did not produce`);
    else if (loc.id && !html.includes(`id="${escapeHtml(loc.id)}"`)) problems.push(`${what} goes to ${loc.page}#${loc.id}, which has no such id`);
  };
  for (const [key, entry] of Object.entries(redirects)) {
    if (entry.to) expect(resolveMoved(redirects, key, ''), `redirect of ${key}`);
    for (const id of Object.keys(entry.anchors ?? {})) expect(resolveMoved(redirects, key, id), `anchor ${key}#${id}`);
  }
  return problems;
}

/**
 * The build step: check the map, write the redirect pages into outDir, then check that every
 * target made it into the output. Throws with the full list of problems so the build fails.
 *
 * @param {{ docsRoot?: string, outDir: string, base: string, origin: string }} input
 */
export function writeRedirectPages({ docsRoot, outDir, base, origin }) {
  const { published, redirects } = loadUrlData(docsRoot);
  const pages = listSourcePages(docsRoot);
  const problems = checkPublishedUrls({ pages, published, redirects, requireListed: false });
  if (!problems.length) {
    for (const { file, html } of renderRedirectPages({ redirects, base, origin })) {
      const abs = path.join(outDir, file);
      fs.mkdirSync(path.dirname(abs), { recursive: true });
      fs.writeFileSync(abs, html);
    }
    problems.push(...checkBuiltTargets({ outDir, redirects }));
  }
  if (problems.length) throw new Error(`published-urls:\n  ${problems.join('\n  ')}`);
}

function main(argv) {
  const mode = argv.find((a) => a === '--check' || a === '--add');
  if (!mode) {
    console.error('usage: node docs/vitepress/.vitepress/published-urls.mjs --check | --add');
    return 2;
  }
  const { published, redirects } = loadUrlData(DOCS_ROOT);
  const pages = listSourcePages(DOCS_ROOT);
  if (mode === '--add') {
    const next = [...new Set([...published, ...pages])].sort();
    fs.writeFileSync(dataFile(DOCS_ROOT, 'published-pages.json'), `${JSON.stringify(next, null, 2)}\n`);
    console.log(`published-urls: ${next.length - published.length} page(s) added, ${next.length} listed.`);
    return 0;
  }
  const problems = checkPublishedUrls({ pages, published, redirects });
  for (const p of problems) console.error(`published-urls: ${p}`);
  if (!problems.length) console.log(`published-urls: ${published.length} published page(s) resolve; ${Object.keys(redirects).length} redirect entr${Object.keys(redirects).length === 1 ? 'y' : 'ies'}.`);
  return problems.length ? 1 : 0;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  process.exit(main(process.argv.slice(2)));
}
