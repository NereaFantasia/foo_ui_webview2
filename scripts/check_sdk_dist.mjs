#!/usr/bin/env node
// scripts/check_sdk_dist.mjs
//
// PURPOSE
//   Fail closed when `sdk/dist/` is missing an artefact that
//   `sdk/package.json` promises to consumers.
//
//   The expected path set is derived from `types`, every string leaf under
//   `exports`, and any `main` / `module` entry, restricted to ./dist/ paths.
//   Each declared path must name a non-empty regular file.
//
//   A complete dist must contain the declared ESM, IIFE and declaration
//   entries together, including the root types entry and *.global.js bundles.
//   build:esm and watch do not produce the IIFE bundles; they must preserve
//   the output from build:iife. Full builds run clean:dist before rebuilding
//   both formats. The postbuild check detects missing or empty entries, but
//   does not establish freshness or validate their contents.
//
//   Sourcemaps are out of scope: `files` excludes dist/**/*.map from the
//   tarball, and audit_sourcemap.mjs checks their integrity separately.
//
// USAGE
//   node scripts/check_sdk_dist.mjs [--json] [--quiet]
//
// EXIT CODES
//   0  every declared artefact exists and is non-empty
//   1  at least one artefact is missing or empty
//   2  script/config error (manifest unreadable, nothing declared)

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const REPO_ROOT = path.resolve(__dirname, '..');
const SDK_DIR = path.join(REPO_ROOT, 'sdk');
const MANIFEST = path.join(SDK_DIR, 'package.json');

function fail(message) {
    process.stderr.write(`check_sdk_dist: ${message}\n`);
    process.exit(2);
}

/** Collect every string leaf of an `exports` subtree (conditions nest). */
function collectStringLeaves(node, out) {
    if (typeof node === 'string') {
        out.add(node);
    } else if (node && typeof node === 'object') {
        for (const value of Object.values(node)) collectStringLeaves(value, out);
    }
}

function declaredPaths(manifest) {
    const specifiers = new Set();
    collectStringLeaves(manifest.exports, specifiers);
    if (typeof manifest.types === 'string') specifiers.add(manifest.types);
    // Declared `main` / `module` entries are binding too.
    for (const key of ['main', 'module']) {
        if (typeof manifest[key] === 'string') specifiers.add(manifest[key]);
    }
    // Only build artefacts are ours to verify; a manifest may legitimately
    // point at checked-in files (LICENSE, README) that no build produces.
    return [...specifiers]
        .filter((p) => p.startsWith('./dist/'))
        .map((p) => p.slice(2))
        .sort();
}

const args = process.argv.slice(2);
if (args.includes('--help') || args.includes('-h')) {
    process.stdout.write('Usage: node scripts/check_sdk_dist.mjs [--json] [--quiet]\n');
    process.exit(0);
}
const asJson = args.includes('--json');
const quiet = args.includes('--quiet');

let manifest;
try {
    manifest = JSON.parse(fs.readFileSync(MANIFEST, 'utf8'));
} catch (error) {
    fail(`cannot read ${path.relative(REPO_ROOT, MANIFEST)}: ${error.message}`);
}

const expected = declaredPaths(manifest);
if (expected.length === 0) {
    fail('sdk/package.json declares no ./dist/ paths — nothing to verify');
}

const missing = [];
for (const rel of expected) {
    const abs = path.join(SDK_DIR, rel);
    let stat;
    try {
        stat = fs.statSync(abs);
    } catch {
        missing.push({ path: `sdk/${rel}`, reason: 'missing' });
        continue;
    }
    if (!stat.isFile()) {
        missing.push({ path: `sdk/${rel}`, reason: 'not a file' });
    } else if (stat.size === 0) {
        missing.push({ path: `sdk/${rel}`, reason: 'empty' });
    }
}

if (asJson) {
    process.stdout.write(
        `${JSON.stringify({ checked: expected.length, missing }, null, 2)}\n`,
    );
} else if (missing.length > 0) {
    process.stderr.write(
        `check_sdk_dist: ${missing.length} of ${expected.length} declared artefact(s) unusable:\n`,
    );
    for (const entry of missing) {
        process.stderr.write(`  ${entry.path} — ${entry.reason}\n`);
    }
    process.stderr.write('  run a full `npm --prefix sdk run build` (not build:esm)\n');
} else if (!quiet) {
    process.stdout.write(`check_sdk_dist: ${expected.length} declared artefacts present\n`);
}

process.exit(missing.length > 0 ? 1 : 0);
