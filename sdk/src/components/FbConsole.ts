/**
 * `<fb-console>` — foobar2000 console log viewer.
 *
 * Polls `fb.log.read(maxLines)` on a fixed interval and renders one
 * row per line. Auto-scrolls to the bottom whenever the buffer grows
 * (toggle off via `auto-scroll="false"`).
 *
 * Attributes:
 * - `max-lines`   — max lines kept in the read buffer (default 200).
 * - `auto-scroll` — `'false'` disables auto-scroll (default on).
 * - `refresh`     — poll interval in ms (default 1000).
 *
 * Emits `fb-console-update` once per successful poll.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { FbConsoleUpdateDetail } from './types.js';

export class FbConsole extends FbBaseElement {
    private _container!: HTMLElement;
    private _maxLines = 200;
    private _autoScroll = true;
    private _interval: ReturnType<typeof setInterval> | null = null;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        // Structure only — `font-family: monospace` is a visual decision
        // and belongs to the theme (`[part=line] { font-family: monospace }`).
        // `white-space: pre-wrap` is preserved: it's tied to the semantic
        // requirement that console output preserve its original whitespace.
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:block;overflow-y:auto}` +
            `div[part=line]{white-space:pre-wrap}</style>` +
            `<div part="container"></div>`;
        this._container = this._$<HTMLElement>('[part=container]')!;
        this._maxLines =
            parseInt(this.getAttribute('max-lines') || '', 10) || 200;
        this._autoScroll = this.getAttribute('auto-scroll') !== 'false';
    }

    protected override _setupEvents(): void {
        /* viewer-only — no DOM listeners */
    }

    protected override _subscribe(): void {
        void this._loadLog();
        const refreshMs =
            parseInt(this.getAttribute('refresh') || '', 10) || 1000;
        this._interval = setInterval(() => {
            void this._loadLog();
        }, refreshMs);
    }

    override disconnectedCallback(): void {
        if (this._interval) clearInterval(this._interval);
        this._interval = null;
        super.disconnectedCallback();
    }

    private async _loadLog(): Promise<void> {
        try {
            const result = (await getFb().log.read(this._maxLines)) as
                | { success?: boolean; lines?: string[] }
                | null;
            if (!result?.success) return;
            const lines = result.lines || [];
            this._container.innerHTML = lines
                .map(
                    (line) => `<div part="line">${this._escHtml(line)}</div>`,
                )
                .join('');
            if (this._autoScroll) {
                this._container.scrollTop = this._container.scrollHeight;
            }
            this._emit<FbConsoleUpdateDetail>('fb-console-update', {
                lineCount: lines.length,
            });
        } catch {
            /* silent */
        }
    }
}
