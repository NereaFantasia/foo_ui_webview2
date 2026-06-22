/**
 * `<fb-waveform>` — full-track waveform overview with playback cursor.
 *
 * Generates a complete waveform via `fb.audio.generateFullWaveform`
 * (host-side cache hit returns sync, otherwise we await the
 * `audio:fullWaveformReady`/`Failed` events with a 60 s timeout) and
 * renders it onto a Canvas in a SoundCloud-style asymmetric mirror:
 * tall top half (~72 %) with a faint reflection below.
 *
 * Click anywhere on the canvas to seek; we emit `fb-seek` so theme
 * code can intercept before the host commit.
 *
 * Attributes:
 * - `src`           — explicit path; when set, suppresses
 *                     `playback:trackChanged` reloads.
 * - `resolution`    — number of samples (default 200).
 * - `mode`          — `'rms' | 'peak'` (default `'rms'`).
 * - `cue-index`     — sub-song selector for cue-sheet sources.
 * - `gamma` / `normalize` (observed) — visual-transform tuning;
 *   `normalize` accepts `'adaptive'` (default) | `'gamma'` | `'histogram'`.
 *
 * Reflects host attributes:
 * - `status`   — `'pending' | 'ready' | 'failed'`.
 * - `progress` — `0…1` cursor position.
 * - `error`    — last failure code (only when `status="failed"`).
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { FullWaveformResult } from '../types/responses.js';
import type { FbSeekDetail } from './types.js';
import type { JsonObject } from '../types/json.js';

type NormalizeMode = 'adaptive' | 'gamma' | 'histogram';

export class FbWaveform extends FbBaseElement {
    private _canvas!: HTMLCanvasElement;
    private _ctx!: CanvasRenderingContext2D;
    private _cursor!: HTMLElement;
    private _waveform: number[] | null = null;
    private _duration = 0;
    private _gamma = 1.5;
    private _normalize: NormalizeMode = 'adaptive';
    private _resizeObserver: ResizeObserver | null = null;
    private _currentPath: string | null = null;
    /**
     * Monotonic counter; every `_loadWaveform()` call captures the current
     * value and bails if a newer load (or a disconnect) has bumped it before
     * the async host call returned. Guards against:
     *  - src / track / cue-index churn while `generateFullWaveform` is in flight
     *  - disconnect after dispatch: late results would otherwise write to a
     *    detached canvas and mutate attributes on an unmounted host.
     */
    private _loadToken = 0;

    static get observedAttributes(): string[] {
        return ['gamma', 'normalize'];
    }

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        // Structure only — themes style `[part=canvas]` (drawing surface
        // already painted via `getComputedStyle(this).color`) and
        // `[part=cursor]` (e.g. `background: currentColor`).
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{position:relative;display:block}` +
            `canvas{display:block;width:100%;height:100%}` +
            `div[part=cursor]{position:absolute;top:0;bottom:0;width:2px;pointer-events:none}</style>` +
            `<canvas part="canvas"></canvas>` +
            `<div part="cursor"></div>`;
        this._canvas = this._$<HTMLCanvasElement>('canvas')!;
        this._ctx = this._canvas.getContext('2d')!;
        this._cursor = this._$<HTMLElement>('[part=cursor]')!;
        const initial = (this.getAttribute('normalize') ||
            'adaptive') as NormalizeMode;
        this._normalize = initial;
    }

    protected override _setupEvents(): void {
        this._listen(this._canvas, 'click', (e: Event) => {
            if (!this._duration) return;
            const me = e as MouseEvent;
            const rect = this._canvas.getBoundingClientRect();
            // B5: zero-width guard avoids divide-by-zero on layout race.
            if (!rect.width) return;
            const x = me.clientX - rect.left;
            const progress = x / rect.width;
            const time = progress * this._duration;
            getFb()
                .player.seek(time)
                .catch(() => {
                    /* silent */
                });
            this._emit<FbSeekDetail>('fb-seek', {
                position: time,
                progress,
            });
        });
        this._resizeObserver = new ResizeObserver(() => this._resizeCanvas());
        this._resizeObserver.observe(this._canvas);
        this._resizeCanvas();
    }

    protected override _subscribe(): void {
        // Only follow track changes when no `src` override is in effect.
        if (!this.getAttribute('src')) {
            this._sub('playback:trackChanged', () => {
                void this._loadWaveform();
            });
            this._sub('playback:time', (data) => {
                const position =
                    (data as { position?: number } | undefined)?.position ?? 0;
                this._updateCursor(position);
            });
        }
        void this._loadWaveform();
    }

    attributeChangedCallback(
        name: string,
        _old: string | null,
        newVal: string | null,
    ): void {
        if (name === 'gamma') {
            const v = parseFloat(newVal || '');
            if (v > 0 && v <= 2) {
                this._gamma = v;
                if (this._waveform) this._drawWaveform();
            }
        }
        if (name === 'normalize') {
            const mode = (newVal || '').toLowerCase();
            if (mode === 'adaptive' || mode === 'gamma' || mode === 'histogram') {
                this._normalize = mode;
                if (this._waveform) this._drawWaveform();
            }
        }
    }

    override disconnectedCallback(): void {
        // Invalidate any in-flight `_loadWaveform()` dispatches before
        // tearing down observers / base-class subscriptions.
        this._loadToken++;
        if (this._resizeObserver) {
            this._resizeObserver.disconnect();
            this._resizeObserver = null;
        }
        super.disconnectedCallback();
    }

    private async _loadWaveform(): Promise<void> {
        const fb = getFb();
        const token = ++this._loadToken;
        try {
            let path = this.getAttribute('src');
            if (!path) {
                const track = (await fb.player.getCurrentTrack()) as
                    | { path?: string; duration?: number }
                    | null;
                if (token !== this._loadToken || !this.isConnected) return;
                if (!track?.path) return;
                path = track.path;
                this._duration = track.duration || 0;
            }

            this._currentPath = path;

            this.setAttribute('status', 'pending');
            this._waveform = null;

            const opts: JsonObject = {
                resolution:
                    parseInt(this.getAttribute('resolution') || '', 10) || 200,
                method: this.getAttribute('mode') || 'rms',
            };
            const cueIndex = this.getAttribute('cue-index');
            if (cueIndex !== null) opts.cueIndex = parseInt(cueIndex, 10);

            const result = (await fb.audio.generateFullWaveform(
                path,
                opts,
            )) as FullWaveformResult;

            // Stale-result guard: a newer load or disconnect happened
            // while the host was generating — drop the late result.
            if (token !== this._loadToken || !this.isConnected) return;
            if (this._currentPath !== path) return;

            if (result?.success && result.waveform) {
                this._waveform = result.waveform;
                this.setAttribute('status', 'ready');
                this._drawWaveform();
            } else {
                this.setAttribute('status', 'failed');
            }
        } catch (err) {
            if (token !== this._loadToken || !this.isConnected) return;
            this._waveform = null;
            this.setAttribute('status', 'failed');
            const code = (err as { error?: string } | undefined)?.error;
            this.setAttribute('error', code || 'Unknown error');
        }
    }

    private _resizeCanvas(): void {
        const rect = this._canvas.getBoundingClientRect();
        this._canvas.width = rect.width * devicePixelRatio;
        this._canvas.height = rect.height * devicePixelRatio;
        // C3: reset transform before scaling — avoids cumulative scale
        // when the element is resized multiple times in quick succession.
        this._ctx.setTransform(devicePixelRatio, 0, 0, devicePixelRatio, 0, 0);
        if (this._waveform) this._drawWaveform();
    }

    private _drawWaveform(): void {
        if (!this._waveform) return;
        const w = this._canvas.width / devicePixelRatio;
        const h = this._canvas.height / devicePixelRatio;
        this._ctx.clearRect(0, 0, w, h);
        // C2: pull foreground colour from computed style so theme tokens
        // apply without per-component CSS overrides.
        this._ctx.fillStyle = getComputedStyle(this).color || '#000';
        const barWidth = w / this._waveform.length;
        const gap =
            barWidth > 3
                ? Math.max(1, Math.round(barWidth * 0.25))
                : barWidth > 1.5
                  ? 1
                  : 0;
        const drawWidth = Math.max(0.5, barWidth - gap);

        let display: number[] | null;
        switch (this._normalize) {
            case 'histogram':
                display = this._histogramEqualize(this._waveform);
                break;
            case 'gamma':
                display = null;
                break;
            default:
                display = this._adaptiveTransform(this._waveform);
                break;
        }

        // SoundCloud-style asymmetric mirror — body 72 % + faint
        // 22 %-tall reflection below the centre line.
        const centerY = Math.round(h * 0.72);
        const midGap = Math.max(1, Math.round(h * 0.01));
        const topH = centerY;
        const bottomAvail = h - centerY - midGap;
        const mirrorRatio = 0.22;
        const mirrorAlpha = 0.22;

        for (let i = 0; i < this._waveform.length; i++) {
            const raw = display
                ? Math.abs(display[i])
                : Math.abs(this._waveform[i]);
            const val =
                this._normalize === 'gamma' ? Math.pow(raw, this._gamma) : raw;

            const topBarH = val * topH;
            this._ctx.globalAlpha = 1.0;
            this._ctx.fillRect(
                i * barWidth,
                centerY - topBarH,
                drawWidth,
                topBarH,
            );

            const bottomBarH = Math.min(topBarH * mirrorRatio, bottomAvail);
            this._ctx.globalAlpha = mirrorAlpha;
            this._ctx.fillRect(
                i * barWidth,
                centerY + midGap,
                drawWidth,
                bottomBarH,
            );
        }
        this._ctx.globalAlpha = 1.0;
    }

    /**
     * Adaptive visual transform — single continuous power curve with
     * no clipping. Works in three phases:
     * 1. Median-of-3 filter — kills isolated transient spikes.
     * 2. P98-percentile normalisation — maps the common loudness range
     *    into `[0, 1]` without subtracting a noise floor.
     * 3. Power compression `v^γ` (γ < 1) — boosts quiet detail and
     *    softens loud peaks; a smooth curve with no steps.
     */
    private _adaptiveTransform(data: number[]): number[] {
        const n = data.length;
        if (n < 3) return data;

        const med = new Array<number>(n);
        med[0] = Math.abs(data[0]);
        med[n - 1] = Math.abs(data[n - 1]);
        for (let i = 1; i < n - 1; i++) {
            const a = Math.abs(data[i - 1]);
            const b = Math.abs(data[i]);
            const c = Math.abs(data[i + 1]);
            med[i] =
                a > b ? (b > c ? b : Math.min(a, c)) : a > c ? a : Math.min(b, c);
        }

        const nonSilent = med.filter((v) => v > 0.01).sort((a, b) => a - b);
        if (nonSilent.length < 10) return med;
        const p98 = nonSilent[Math.floor(nonSilent.length * 0.98)];
        if (p98 < 0.001) return med;

        const gamma = 0.6;
        const scale = 0.85;
        const result = new Array<number>(n);
        for (let i = 0; i < n; i++) {
            if (med[i] <= 0.01) {
                result[i] = 0;
                continue;
            }
            const t = med[i] / p98;
            result[i] = Math.min(1.0, Math.pow(t, gamma)) * scale;
        }
        return result;
    }

    private _histogramEqualize(data: number[]): number[] {
        const nonZero: number[] = [];
        for (let i = 0; i < data.length; i++) {
            const v = Math.abs(data[i]);
            if (v > 0) nonZero.push(v);
        }
        if (nonZero.length < 2) return data;
        nonZero.sort((a, b) => a - b);

        const result = new Array<number>(data.length);
        for (let i = 0; i < data.length; i++) {
            const v = Math.abs(data[i]);
            if (v <= 0) {
                result[i] = 0;
                continue;
            }
            let lo = 0;
            let hi = nonZero.length;
            while (lo < hi) {
                const mid = (lo + hi) >> 1;
                if (nonZero[mid] < v) lo = mid + 1;
                else hi = mid;
            }
            result[i] = (lo + 0.5) / nonZero.length;
        }
        return result;
    }

    private _updateCursor(position: number): void {
        if (!this._duration) return;
        const progress = position / this._duration;
        this._cursor.style.left = `${progress * 100}%`;
        this.setAttribute('progress', progress.toFixed(4));
    }
}
