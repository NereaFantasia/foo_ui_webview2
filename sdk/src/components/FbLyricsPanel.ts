/**
 * `<fb-lyrics-panel>` — synchronised LRC lyrics display.
 *
 * Loads lyrics for the active track via `fb.lyrics.get(path, source)`,
 * parses LRC timestamps, then highlights the current line each time
 * `playback:time` ticks.
 *
 * Behaviour highlights:
 * - 350 ms debounce after `playback:trackChanged` to avoid hammering
 *   the host while users mash skip / back.
 * - Per-track load token guards against late results from a previous
 *   track overwriting the new one.
 * - `_scrollCurrentRow` only auto-scrolls when the row is not already
 *   comfortably in view, with a 350 ms / 900 ms cooldown depending on
 *   whether the document has focus (avoids stealing scroll from the
 *   user when the window is in the background).
 *
 * Attributes:
 * - `source`     — passed through to `fb.lyrics.get` (default `'any'`).
 * - `scroll`     — `'smooth'` (default) or anything else for `'auto'`.
 *
 * Reflects host attributes:
 * - `has-lyrics` — set when at least one line was rendered.
 * - `current-line` — index of the highlighted row (or `'-1'`).
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type {
    FbLyricsLineChangeDetail,
    FbLyricsLoadedDetail,
    FbLyricsSeekDetail,
} from './types.js';
import type { JsonObject } from '../types/json.js';

interface LyricsLine {
    time: number;
    text: string;
}

interface LyricsResult {
    available?: boolean;
    lyrics?: string;
    synced?: boolean;
}

/**
 * Parse an LRC document into `{ time, text }[]`. Extra timestamp
 * suffixes (`[01:23.456]`) are normalised to milliseconds.
 */
function parseLRC(lrc: string): LyricsLine[] {
    const lines: LyricsLine[] = [];
    const regex = /\[(\d{2,}):(\d{2})(?:\.(\d{2,3}))?\](.*)$/gm;
    let match: RegExpExecArray | null;
    while ((match = regex.exec(lrc)) !== null) {
        const minutes = parseInt(match[1], 10);
        const seconds = parseInt(match[2], 10);
        const ms = match[3] ? parseInt(match[3].padEnd(3, '0'), 10) : 0;
        const time = minutes * 60 + seconds + ms / 1000;
        const text = match[4].trim();
        lines.push({ time, text });
    }
    return lines.sort((a, b) => a.time - b.time);
}

export class FbLyricsPanel extends FbBaseElement {
    private _container!: HTMLElement;
    private _lines: LyricsLine[] = [];
    private _rows: HTMLElement[] = [];
    private _currentIndex = -1;
    private _trackPath: string | null = null;
    private _debounceTimer: ReturnType<typeof setTimeout> | null = null;
    private _loadToken = 0;
    private _lastScrollAt = 0;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:block;overflow-y:auto}` +
            `div[part=container]{display:flex;flex-direction:column}</style>` +
            `<div part="container"></div>`;
        this._container = this._$<HTMLElement>('[part=container]')!;
    }

    protected override _setupEvents(): void {
        this._listen(this._container, 'click', (e: Event) => {
            const target = e.target as HTMLElement | null;
            const row = target?.closest('[part=line]') as HTMLElement | null;
            if (!row) return;
            const time = parseFloat(row.dataset.time || '');
            if (isFinite(time)) {
                getFb()
                    .player.seek(time)
                    .catch(() => {
                        /* silent */
                    });
                this._emit<FbLyricsSeekDetail>('fb-lyrics-seek', { time });
            }
        });
    }

    protected override _subscribe(): void {
        this._sub('playback:trackChanged', () => {
            // R7: 350 ms debounce — avoids racing other plugins (e.g.
            // lyrics-search) that wake up on the same track-change tick.
            if (this._debounceTimer) clearTimeout(this._debounceTimer);
            this._debounceTimer = setTimeout(() => {
                void this._loadLyrics();
            }, 350);
        });
        this._sub('playback:time', (data) => {
            const position =
                (data as { position?: number } | undefined)?.position ?? 0;
            this._updateCurrent(position);
        });
        void this._loadLyrics();
    }

    override disconnectedCallback(): void {
        if (this._debounceTimer) clearTimeout(this._debounceTimer);
        this._debounceTimer = null;
        // Bump the load token so any in-flight async result is dropped.
        this._loadToken += 1;
        super.disconnectedCallback();
    }

    private async _loadLyrics(): Promise<void> {
        const loadToken = ++this._loadToken;
        const fb = getFb();
        try {
            const track = (await fb.player.getCurrentTrack()) as
                | { path?: string }
                | null;
            if (loadToken !== this._loadToken) return;

            const path = track?.path || '';
            if (!path || path === this._trackPath) return;
            this._trackPath = path;

            const source = this.getAttribute('source') || 'any';
            // The bridge spreads the second argument as an object, so
            // passing the `source` string is effectively a no-op and
            // the host falls back to its default `'any'` source. The
            // call is preserved for forward-compatibility with hosts
            // that may accept the positional form.
            const result = (await fb.lyrics.get(
                path,
                source as unknown as JsonObject,
            )) as LyricsResult | null;
            if (loadToken !== this._loadToken) return;

            if (result?.available && result.lyrics) {
                const rawLines: LyricsLine[] = result.synced
                    ? parseLRC(result.lyrics)
                    : result.lyrics
                          .split('\n')
                          .map((text, i) => ({ time: i, text }));
                this._lines = rawLines;
                this._renderLines();
                this._currentIndex = -1;
                this._lastScrollAt = 0;
                this.setAttribute('has-lyrics', '');
                this._emit<FbLyricsLoadedDetail>('fb-lyrics-loaded', {
                    lineCount: this._lines.length,
                    synced: !!result.synced,
                });
            } else {
                this._lines = [];
                this._rows = [];
                this._currentIndex = -1;
                this._container.innerHTML =
                    '<div part="line" class="empty">No lyrics</div>';
                this.removeAttribute('has-lyrics');
            }
        } catch {
            if (loadToken !== this._loadToken) return;
            this._lines = [];
            this._rows = [];
            this._currentIndex = -1;
            this._container.innerHTML = '';
            this.removeAttribute('has-lyrics');
        }
    }

    private _renderLines(): void {
        this._container.innerHTML = this._lines
            .map(
                (line, i) =>
                    `<div part="line" data-index="${i}" data-time="${line.time}" class="future">${this._escHtml(line.text)}</div>`,
            )
            .join('');
        this._rows = Array.from(this._$$<HTMLElement>('[part=line]'));
    }

    private _setRowState(index: number, state: string): void {
        const row = this._rows[index];
        if (!row) return;
        row.classList.remove('past', 'current', 'future');
        row.classList.add(state);
    }

    private _setRangeState(start: number, end: number, state: string): void {
        if (end < start) return;
        const safeStart = Math.max(0, start);
        const safeEnd = Math.min(this._rows.length - 1, end);
        for (let i = safeStart; i <= safeEnd; i++) {
            this._setRowState(i, state);
        }
    }

    private _isRowComfortablyVisible(row: HTMLElement | undefined): boolean {
        if (!row) return false;
        const viewportTop = this.scrollTop;
        const viewportBottom = viewportTop + this.clientHeight;
        const padding = Math.max(24, Math.floor(this.clientHeight * 0.2));
        const rowTop = row.offsetTop;
        const rowBottom = rowTop + row.offsetHeight;
        return (
            rowTop >= viewportTop + padding &&
            rowBottom <= viewportBottom - padding
        );
    }

    private _scrollCurrentRow(row: HTMLElement | undefined): void {
        if (!row) return;
        const now =
            typeof performance !== 'undefined' && performance.now
                ? performance.now()
                : Date.now();
        const hasFocus =
            typeof document !== 'undefined' &&
            typeof document.hasFocus === 'function'
                ? document.hasFocus()
                : true;
        const requestedMode = this.getAttribute('scroll') || 'smooth';
        const behavior: ScrollBehavior =
            requestedMode === 'smooth' && hasFocus ? 'smooth' : 'auto';
        const needScroll = !this._isRowComfortablyVisible(row);
        const cooldownMs = hasFocus ? 350 : 900;

        if (!needScroll && now - this._lastScrollAt < cooldownMs) return;

        row.scrollIntoView({ behavior, block: 'center' });
        this._lastScrollAt = now;
    }

    private _updateCurrent(position: number): void {
        if (this._lines.length === 0 || this._rows.length === 0) return;

        let newIndex = -1;
        for (let i = this._lines.length - 1; i >= 0; i--) {
            if (this._lines[i].time <= position) {
                newIndex = i;
                break;
            }
        }

        const previousIndex = this._currentIndex;
        if (newIndex === previousIndex) return;
        this._currentIndex = newIndex;

        if (newIndex === -1) {
            this._setRangeState(0, previousIndex, 'future');
        } else if (previousIndex === -1 || newIndex > previousIndex) {
            this._setRangeState(
                previousIndex >= 0 ? previousIndex : 0,
                newIndex - 1,
                'past',
            );
        } else if (newIndex < previousIndex) {
            this._setRangeState(newIndex + 1, previousIndex, 'future');
        }

        if (newIndex >= 0) {
            this._setRowState(newIndex, 'current');
        }

        if (newIndex >= 0) {
            this._scrollCurrentRow(this._rows[newIndex]);
        }

        this.setAttribute('current-line', newIndex.toString());
        if (newIndex >= 0) {
            this._emit<FbLyricsLineChangeDetail>('fb-lyrics-line-change', {
                index: newIndex,
                text: this._lines[newIndex].text,
                time: this._lines[newIndex].time,
            });
        }
    }
}
