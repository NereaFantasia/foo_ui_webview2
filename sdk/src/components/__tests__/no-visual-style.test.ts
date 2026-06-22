// sdk/src/components/__tests__/no-visual-style.test.ts
//
// Phase 4 drift guard — enforces `AGENTS.md §2.6.1` "功能骨架，零视觉主张":
//
//   * Structural CSS (display / position / overflow / flex / grid / gap /
//     width / height / cursor / pointer-events / user-select / box-sizing /
//     transform:translate|rotate / white-space / text-overflow / z-index)
//     is allowed inside each component's `<style>…</style>`.
//   * Visual-intent CSS (color, background*, font-*, border*, border-radius,
//     box-shadow, text-shadow, line-height, text-decoration, letter-spacing,
//     transition, animation, transform:scale|skew, margin*) is rejected.
//
// We do not instantiate the components — instead we parse each `Fb*.ts`
// source file statically:
//
//   1. strip `//` and `/* … */` comments to avoid false positives from
//      the theme-author docstrings that legitimately mention banned
//      properties (e.g. `background: currentColor`);
//   2. extract every `<style> … </style>` segment that survives the
//      strip pass (these segments cover multi-line template literals
//      because we work on the flat file text);
//   3. run every banned pattern against the concatenated style text.
//
// Any drift fails the test with the offending property name and file
// name so the regression is obvious in CI output.
//
// We use Vite/Vitest's `import.meta.glob(..., { query: '?raw' })` to
// pull component sources as strings at test time. This avoids adding
// `@types/node` just to read files from disk.

/// <reference types="vite/client" />

import { describe, expect, it } from 'vitest';

const sources = import.meta.glob('../Fb*.ts', {
    query: '?raw',
    import: 'default',
    eager: true,
}) as Record<string, string>;

/**
 * Remove `//` line comments and `/* … *​/` block comments from a source
 * file, preserving string literals (we don't actually parse strings —
 * we just keep them verbatim, which is fine because `<style>` text
 * lives inside template strings and never contains `//` or `/*`).
 */
function stripComments(src: string): string {
    // Block comments (non-greedy, multi-line)
    let out = src.replace(/\/\*[\s\S]*?\*\//g, '');
    // Line comments — be conservative: only strip `//` that starts at
    // line boundary or after whitespace, so we don't eat `https://`
    // inside string literals.
    out = out.replace(/(^|\s)\/\/[^\n]*/g, '$1');
    return out;
}

/**
 * Pull every `<style> … </style>` body out of the stripped source. The
 * template literals may span several `+` concatenated lines, but the
 * flat file text still contains the raw `<style>` / `</style>`
 * delimiters inside the backtick strings.
 */
function extractStyleContent(stripped: string): string {
    const parts: string[] = [];
    const re = /<style>([\s\S]*?)<\/style>/g;
    let m: RegExpExecArray | null;
    while ((m = re.exec(stripped)) !== null) {
        parts.push(m[1]);
    }
    return parts.join('\n');
}

interface BannedRule {
    name: string;
    re: RegExp;
}

/**
 * Matchers are anchored at CSS property boundaries (`^` / `;` / `{`)
 * so `box-sizing:border-box` doesn't trip the `border:` rule, and
 * `text-overflow:` doesn't trip the `text-decoration:` rule.
 */
const BANNED: BannedRule[] = [
    // Colour / fill
    { name: 'color', re: /(?:^|[\s;{])color\s*:/i },
    { name: 'background*', re: /(?:^|[\s;{])background(?:-[a-z]+)?\s*:/i },
    // Borders
    { name: 'border', re: /(?:^|[\s;{])border\s*:/i },
    {
        name: 'border-*',
        re: /(?:^|[\s;{])border-(?:top|bottom|left|right|style|color|width|radius|image)\s*:/i,
    },
    { name: 'outline', re: /(?:^|[\s;{])outline\s*:/i },
    // Shadows
    { name: 'box-shadow', re: /(?:^|[\s;{])box-shadow\s*:/i },
    { name: 'text-shadow', re: /(?:^|[\s;{])text-shadow\s*:/i },
    // Typography
    {
        name: 'font-*',
        re: /(?:^|[\s;{])font-(?:family|size|weight|style|variant|stretch|feature-settings)\s*:/i,
    },
    { name: 'line-height', re: /(?:^|[\s;{])line-height\s*:/i },
    { name: 'text-decoration', re: /(?:^|[\s;{])text-decoration\s*:/i },
    { name: 'letter-spacing', re: /(?:^|[\s;{])letter-spacing\s*:/i },
    { name: 'word-spacing', re: /(?:^|[\s;{])word-spacing\s*:/i },
    // Motion
    { name: 'transition', re: /(?:^|[\s;{])transition\s*:/i },
    { name: 'animation', re: /(?:^|[\s;{])animation\s*:/i },
    { name: 'transform:scale|skew', re: /transform\s*:\s*[^;]*(?:scale|skew)\(/i },
    // Outer spacing
    {
        name: 'margin*',
        re: /(?:^|[\s;{])margin(?:-(?:top|bottom|left|right|inline|block))?\s*:/i,
    },
];

/** Whitelist override: components legitimately publish `padding:`
 * declarations as hit-target wideners, and those are documented in
 * the immediately adjacent comment line. `padding` isn't on the
 * banned list, but we spot-check that every `padding:` inside a
 * `<style>` block has an adjacent AGENTS.md §2.6.1 citation to keep
 * future contributors from sneaking in visual padding. */
function assertPaddingIsDocumented(
    fileName: string,
    rawSource: string,
): void {
    // We inspect the *raw* source (pre-strip) so comments are still present.
    const re = /<style>([\s\S]*?)<\/style>/g;
    let m: RegExpExecArray | null;
    while ((m = re.exec(rawSource)) !== null) {
        const block = m[1];
        // Any `padding:` inside the block must have a sibling comment
        // somewhere in the same template-literal cluster mentioning
        // either "hit-target" or "§2.6.1" to signal intent.
        if (/(?:^|[\s;{])padding\s*:/i.test(block)) {
            // Walk backwards ~400 chars from the block's start to find
            // the documenting comment; if we can't find one, fail.
            const blockStart = m.index;
            const window = rawSource.slice(
                Math.max(0, blockStart - 600),
                blockStart + block.length,
            );
            const documented =
                /hit-target/i.test(window) || /§\s*2\.6\.1/.test(window);
            if (!documented) {
                throw new Error(
                    `${fileName}: <style> contains \`padding:\` without an ` +
                        `adjacent "hit-target" / "§2.6.1" justification ` +
                        `comment — see AGENTS.md §2.6.1 whitelist rules.`,
                );
            }
        }
    }
}

/** Extract `FbSomething.ts` from a relative glob key like `../FbSomething.ts`. */
function basename(path: string): string {
    const idx = path.lastIndexOf('/');
    return idx < 0 ? path : path.slice(idx + 1);
}

describe('components drift guard — no visual-intent CSS in <style>', () => {
    const entries: Array<[string, string]> = Object.entries(sources)
        .map(([path, raw]) => [basename(path), raw] as [string, string])
        .filter(([name]) => /^Fb.*\.ts$/.test(name))
        .sort(([a], [b]) => a.localeCompare(b));

    // Sanity check — the 36 `fb-*` components plus the base class.
    it('discovers the expected component source files', () => {
        const names = entries.map(([n]) => n);
        expect(names.length).toBeGreaterThanOrEqual(30);
        expect(names).toContain('FbBaseElement.ts');
    });

    for (const [f, raw] of entries) {
        it(`${f} has no visual-intent CSS in its shadow <style>`, () => {
            const stripped = stripComments(raw);
            const styles = extractStyleContent(stripped);

            // Some components (buttons with external JSX-only markup)
            // don't ship a `<style>` block; skip them silently.
            if (!styles.trim()) return;

            for (const rule of BANNED) {
                const match = styles.match(rule.re);
                expect(
                    match,
                    `${f}: banned property "${rule.name}" found in <style>:\n` +
                        `  match = ${match?.[0]?.trim()}\n` +
                        `  Move the declaration to the theme layer ` +
                        `(see AGENTS.md §2.6.1).`,
                ).toBeNull();
            }

            // `padding` isn't banned outright (hit-target whitelist),
            // but every occurrence must be documented.
            assertPaddingIsDocumented(f, raw);
        });
    }
});
