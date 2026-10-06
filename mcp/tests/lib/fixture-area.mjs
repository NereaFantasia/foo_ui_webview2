/**
 * Builds the on-disk fixture area the write-side e2e cases need.
 *
 * Write endpoints (metadata.write, rating.set, artwork embed, tag removal) alter
 * the file or the library database, so they cannot be aimed at the user's own
 * music. Tests of library-backed rating and playcount need indexed copies, so
 * the default area is inside the first monitored root rather than %TEMP%.
 * FB2K_LIBRARY_FIXTURE_DIR overrides that location without a containment check;
 * the caller must choose a monitored directory when library membership matters.
 * The caller removes the copies afterwards; fb2k drops missing files on rescan.
 *
 * Nothing here is tagged through the bridge. Tags are written by editing the
 * VORBIS_COMMENT block directly (see flac-fixture.mjs), so a fixture and the
 * endpoint under test never share an implementation.
 *
 * Environment:
 *   FB2K_LIBRARY_FIXTURE_DIR  fixture area; default <first library root>\tests
 *   FB2K_FIXTURE_SOURCE       FLAC to copy; default the smallest eligible FLAC
 *                             on the queried page (400 tracks by default)
 */

import { cpSync, existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";

import { writeTaggedFlacCopy } from "./flac-fixture.mjs";

/**
 * Picks the audio source from FB2K_FIXTURE_SOURCE or the supplied library page.
 * The override is checked only for existence; it bypasses ranking and excludeDir,
 * and need not be in a monitored root. The caller must supply a valid FLAC.
 * Without an override, existing .flac paths with positive reported fileSize are
 * ranked by that size. resolveFixtureArea queries at most 400 tracks by default,
 * so the selection is not necessarily the library's smallest FLAC. Neither
 * branch verifies containment in a monitored root.
 *
 * A smaller source reduces fixture copy cost, but small files are not required:
 * when all eligible candidates are large, the selected source is large too.
 * Tagged fixtures replace the metadata and still need a valid audio stream.
 *
 * `excludeDir` excludes paths with that case-insensitive prefix from ranking.
 * An indexed copy left by an interrupted run may be smaller after tags or
 * pictures are removed; excluding it avoids reusing a fixture as the next
 * run's source. This filter does not apply to FB2K_FIXTURE_SOURCE.
 */
export function pickSourceTrack(tracks, { excludeDir } = {}) {
    const override = process.env.FB2K_FIXTURE_SOURCE;
    if (override) {
        if (!existsSync(override)) {
            throw new Error(`FB2K_FIXTURE_SOURCE does not exist: ${override}`);
        }
        return { absolutePath: override, chosenBy: "environment" };
    }

    const excluded = excludeDir ? excludeDir.toLowerCase() : undefined;
    const candidates = tracks
        .filter((track) => String(track?.absolutePath || "").toLowerCase().endsWith(".flac"))
        .map((track) => ({
            absolutePath: String(track.absolutePath),
            sizeBytes: Number(track.fileSize) || 0,
        }))
        .filter((entry) => entry.sizeBytes > 0 && existsSync(entry.absolutePath))
        .filter(
            (entry) =>
                excluded === undefined ||
                !entry.absolutePath.toLowerCase().startsWith(excluded),
        )
        .sort((a, b) => a.sizeBytes - b.sizeBytes);

    if (candidates.length === 0) {
        throw new Error(
            "the scanned library page holds no readable FLAC; set FB2K_FIXTURE_SOURCE to choose one",
        );
    }
    return { ...candidates[0], chosenBy: "smallest FLAC on the scanned page" };
}

/** The directory the per-run fixture areas are created under. */
export function fixtureAreaBase(libraryRoot) {
    return process.env.FB2K_LIBRARY_FIXTURE_DIR || join(libraryRoot, "tests");
}

/**
 * Creates the area and returns handles for building fixtures inside it.
 *
 * The effective base, including any FB2K_LIBRARY_FIXTURE_DIR override, must be
 * monitored for library-backed cases. This function does not check membership.
 * The caller is responsible for calling remove(); see each suite's finally block.
 */
export function createFixtureArea({ libraryRoot, runId, source }) {
    const base = fixtureAreaBase(libraryRoot);
    const dir = join(base, `e2e-${runId}`);
    mkdirSync(dir, { recursive: true });

    const created = [];

    /** Copies the source audio under a new name, tags untouched. */
    function copyAudio(name) {
        const target = join(dir, name);
        cpSync(source.absolutePath, target);
        created.push(target);
        return target;
    }

    /**
     * Copies the source audio and replaces its whole tag set out of band.
     * `pictures` embeds PICTURE blocks the same way: absent means the copy is
     * guaranteed to carry no artwork at all.
     */
    function taggedAudio(name, entries, pictures = []) {
        const target = join(dir, name);
        const written = writeTaggedFlacCopy({
            sourcePath: source.absolutePath,
            targetPath: target,
            entries,
            pictures,
        });
        created.push(target);
        return written;
    }

    function textFile(name, body) {
        const target = join(dir, name);
        writeFileSync(target, body, "utf8");
        created.push(target);
        return target;
    }

    function copyFile(sourcePath, name) {
        const target = join(dir, name);
        cpSync(sourcePath, target);
        created.push(target);
        return target;
    }

    function remove() {
        rmSync(dir, { recursive: true, force: true });
        return { removed: existsSync(dir) === false, files: created.length };
    }

    return { base, dir, created, copyAudio, taggedAudio, textFile, copyFile, remove };
}

/**
 * Waits for the library to report a path, so a case can act on a fresh copy.
 *
 * A file dropped into a monitored root is not instantly in the library: fb2k
 * picks it up on its own schedule. library.rescan asks for that sweep, but the
 * sweep is asynchronous, so the only reliable signal is asking whether the path
 * has arrived. Returns the entry, or null when it never showed up - a caller
 * that needs library membership should treat null as a blocked precondition
 * rather than a failed assertion.
 */
export async function waitForLibraryEntry(
    bridge,
    path,
    { timeoutMs = 40000, pollMs = 1000, rescan = true } = {},
) {
    if (rescan) {
        await bridge.invokeRaw("library.rescan", {}).catch(() => undefined);
    }
    const deadline = Date.now() + timeoutMs;
    for (;;) {
        const found = await bridge.invokeRaw("library.getByPath", { path });
        const entry = found?.kind === "result" ? found.value : undefined;
        if (entry?.found === true) {
            return { entry, waitedMs: timeoutMs - (deadline - Date.now()) };
        }
        if (Date.now() >= deadline) return null;
        await new Promise((resolve) => setTimeout(resolve, pollMs));
    }
}

/**
 * Resolves the fixture area against a live instance with at least one library
 * root. The effective base must be writable by node. Source selection uses one
 * library.query page of up to scanLimit tracks, even with an environment override.
 */
export async function resolveFixtureArea(bridge, { runId, scanLimit = 400 }) {
    const roots = await bridge.invoke("library.getRoots", {});
    const root = (roots?.roots ?? [])[0];
    if (!root?.absolutePath) {
        throw new Error(
            "the library reports no monitored root, so no fixture can be made library-addressable",
        );
    }

    const page = await bridge.invoke("library.query", { query: "ALL", limit: scanLimit });
    const source = pickSourceTrack(Array.isArray(page?.tracks) ? page.tracks : [], {
        excludeDir: fixtureAreaBase(root.absolutePath),
    });
    const area = createFixtureArea({ libraryRoot: root.absolutePath, runId, source });

    return { root, source, area };
}
