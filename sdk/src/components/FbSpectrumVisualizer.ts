/**
 * `<fb-spectrum-visualizer>` — real-time FFT spectrum display.
 *
 * Subscribes to the host's spectrum stream via `audio.subscribeSpectrum`
 * and, depending on the `mode` attribute:
 * - `bars` (default): draws a vertical bar chart on a Canvas with
 *   per-band lerp smoothing (fast rise / slow fall) driven by `rAF`.
 * - `wave`: draws a smoothed `quadraticCurveTo` waveform.
 * - `raw` : skips Canvas drawing and re-emits the raw band array as
 *   the `fb-spectrum-data` CustomEvent (theme owns the rendering).
 *
 * Attributes:
 * - `bands`        — band count (default 64).
 * - `fps`          — host stream rate (default 30).
 * - `fft-size`     — power-of-two override (auto = `bands * 2`).
 * - `mode`         — `'bars' | 'wave' | 'raw'` (default `'bars'`).
 * - `dpr`          — pixel-ratio override (defaults to `devicePixelRatio`).
 * - `fall-speed` / `rise-speed` — lerp coefficients (0…1, observed).
 *
 * Cleanup: cancels the rAF loop, the colour-cache interval, the
 * `ResizeObserver`, and unsubscribes the spectrum stream on disconnect.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { AudioSpectrumPayload } from '../types/events.js';
import type { FbSpectrumDataDetail } from './types.js';

type SpectrumPayload = AudioSpectrumPayload | { spectrum?: number[] };

export class FbSpectrumVisualizer extends FbBaseElement {
    private _canvas!: HTMLCanvasElement | null;
    private _ctx!: CanvasRenderingContext2D | null;
    private _spectrum: ArrayLike<number> | null = null;
    private _display: Float32Array | null = null;
    private _resizeObserver: ResizeObserver | null = null;
    private _rafId = 0;
    private _colorTimer: ReturnType<typeof setInterval> | 0 = 0;
    private _cachedColor = '#000';
    private _cssW = 0;
    private _cssH = 0;
    private _subscriptionId = '';
    private _riseBase = 0.65;
    private _fallBase = 0.28;
    private _lastFrameTime = 0;

    static get observedAttributes(): string[] {
        return ['fall-speed', 'rise-speed'];
    }

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `canvas{display:block;width:100%;height:100%}</style>` +
            `<canvas part="canvas"></canvas>`;
        this._canvas = this._$<HTMLCanvasElement>('canvas');
        this._ctx = this._canvas?.getContext('2d') ?? null;
    }

    protected override _setupEvents(): void {
        if (!this._canvas) return;
        this._resizeObserver = new ResizeObserver(() => this._resizeCanvas());
        this._resizeObserver.observe(this._canvas);
        this._resizeCanvas();
        // Cache the foreground colour every second instead of every
        // frame; `getComputedStyle` per rAF tick is expensive.
        this._refreshColor();
        this._colorTimer = setInterval(() => this._refreshColor(), 1000);
    }

    protected override _subscribe(): void {
        const bands = parseInt(this.getAttribute('bands') || '', 10) || 64;
        const fps = parseInt(this.getAttribute('fps') || '', 10) || 30;
        const mode = this.getAttribute('mode') || 'bars';

        let fftSize = parseInt(this.getAttribute('fft-size') || '', 10) || 0;
        if (!fftSize || fftSize < 256) {
            fftSize = Math.max(256, bands * 2);
        }
        fftSize = Math.pow(2, Math.ceil(Math.log2(fftSize)));

        this._subscriptionId =
            globalThis.crypto &&
            typeof globalThis.crypto.randomUUID === 'function'
                ? globalThis.crypto.randomUUID()
                : `spectrum_${Date.now()}_${Math.random().toString(36).slice(2)}`;

        // Raw `invoke` is used (rather than the wrapper-managed
        // lifecycle) so the same `subscriptionId` can be passed to
        // both subscribe and unsubscribe, ensuring a clean teardown
        // when the component disconnects.
        getFb()
            .invoke('audio.subscribeSpectrum', {
                subscriptionId: this._subscriptionId,
                fftSize,
                fps,
                bands,
            })
            .catch(() => {
                /* silent */
            });

        this._sub('audio:spectrum', (data: SpectrumPayload) => {
            const payload = data as { spectrum?: number[] } | number[];
            const spectrum = Array.isArray(payload)
                ? payload
                : payload?.spectrum;
            if (!spectrum) return;
            this._spectrum = spectrum;
            if (mode === 'raw') {
                this._emit<FbSpectrumDataDetail>('fb-spectrum-data', {
                    bands: new Float32Array(spectrum),
                });
            }
        });

        if (mode !== 'raw') {
            this._startRAF();
        }
    }

    attributeChangedCallback(name: string, _old: string | null, newVal: string | null): void {
        if (name === 'fall-speed') {
            const v = parseFloat(newVal || '');
            if (v > 0 && v < 1) this._fallBase = v;
        }
        if (name === 'rise-speed') {
            const v = parseFloat(newVal || '');
            if (v > 0 && v < 1) this._riseBase = v;
        }
    }

    override disconnectedCallback(): void {
        if (this._rafId) {
            cancelAnimationFrame(this._rafId);
            this._rafId = 0;
        }
        if (this._colorTimer) {
            clearInterval(this._colorTimer);
            this._colorTimer = 0;
        }
        if (this._resizeObserver) {
            this._resizeObserver.disconnect();
            this._resizeObserver = null;
        }
        if (this._subscriptionId) {
            getFb()
                .invoke('audio.unsubscribeSpectrum', {
                    subscriptionId: this._subscriptionId,
                })
                .catch(() => {
                    /* silent */
                });
            this._subscriptionId = '';
        }
        // D7: drop large buffers so GC can reclaim them promptly.
        this._spectrum = null;
        this._display = null;
        this._ctx = null;
        this._canvas = null;
        this._lastFrameTime = 0;
        super.disconnectedCallback();
    }

    /** Theme escape-hatch — exposes the underlying 2D context. */
    getContext(): CanvasRenderingContext2D | null {
        return this._ctx;
    }

    private _refreshColor(): void {
        this._cachedColor = getComputedStyle(this).color || '#000';
    }

    private _startRAF(): void {
        if (this._rafId) return;
        this._lastFrameTime = 0;
        const loop = (timestamp: number): void => {
            const shouldContinue = this._smoothAndDraw(timestamp);
            if (!this.isConnected || shouldContinue === false) {
                this._rafId = 0;
                return;
            }
            this._rafId = requestAnimationFrame(loop);
        };
        this._rafId = requestAnimationFrame(loop);
    }

    /**
     * Per-band lerp smoothing (fast rise, slower fall) plus mode draw.
     * Returns `false` when there's nothing to draw so the rAF loop can
     * pause itself; resize / re-attach restarts it.
     */
    private _smoothAndDraw(timestamp: number): boolean {
        // D6: zero-size guard — pause when we can't paint anything.
        if (!this._cssW || !this._cssH) return false;
        if (!this._spectrum) return true;

        // D1: time-constant driven smoothing decouples motion from FPS.
        let dt = 33.3;
        if (timestamp && this._lastFrameTime) {
            dt = timestamp - this._lastFrameTime;
            if (dt > 200) dt = 33.3;
        }
        this._lastFrameTime = timestamp || 0;
        const dtNorm = dt / 33.3;
        const len = this._spectrum.length;

        if (!this._display || this._display.length !== len) {
            this._display = new Float32Array(len);
        }

        const rise = 1.0 - Math.pow(1.0 - this._riseBase, dtNorm);
        const fallDefault = 1.0 - Math.pow(1.0 - this._fallBase, dtNorm);
        const fallFast =
            1.0 -
            Math.pow(1.0 - Math.min(0.55, this._fallBase * 1.6), dtNorm);
        const bassEnd = Math.floor(len * 0.25);
        for (let i = 0; i < len; i++) {
            const target = this._spectrum[i];
            const current = this._display[i];
            if (target >= current) {
                this._display[i] = current + (target - current) * rise;
            } else {
                const fall =
                    i < bassEnd
                        ? fallFast + (fallDefault - fallFast) * (i / bassEnd)
                        : fallDefault;
                this._display[i] = current + (target - current) * fall;
            }
        }

        const mode = this.getAttribute('mode') || 'bars';
        if (mode === 'bars') this._drawBars();
        else if (mode === 'wave') this._drawWave();
        return true;
    }

    private _resizeCanvas(): void {
        if (!this._canvas || !this._ctx) return;
        const rect = this._canvas.getBoundingClientRect();
        const dpr =
            parseFloat(this.getAttribute('dpr') || '') || devicePixelRatio;
        this._canvas.width = rect.width * dpr;
        this._canvas.height = rect.height * dpr;
        this._ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        this._cssW = rect.width;
        this._cssH = rect.height;
        if (
            this._cssW &&
            this._cssH &&
            !this._rafId &&
            (this.getAttribute('mode') || 'bars') !== 'raw'
        ) {
            this._startRAF();
        }
    }

    private _drawBars(): void {
        if (!this._ctx) return;
        const data = this._display || this._spectrum;
        if (!data) return;
        const w = this._cssW;
        const h = this._cssH;
        this._ctx.clearRect(0, 0, w, h);
        const barWidth = w / data.length;
        const gap = barWidth > 2 ? 1 : 0;
        // D5: keep ≥ 0.5 px so high-DPI fractional widths still render.
        const drawWidth = Math.max(0.5, barWidth - gap);
        this._ctx.fillStyle = this._cachedColor;
        for (let i = 0; i < data.length; i++) {
            const barHeight = data[i] * h;
            this._ctx.fillRect(
                i * barWidth,
                h - barHeight,
                drawWidth,
                barHeight,
            );
        }
    }

    private _drawWave(): void {
        if (!this._ctx) return;
        const data = this._display || this._spectrum;
        if (!data || data.length < 2) return;
        const w = this._cssW;
        const h = this._cssH;
        this._ctx.clearRect(0, 0, w, h);
        this._ctx.strokeStyle = this._cachedColor;
        this._ctx.lineWidth = 2;
        this._ctx.beginPath();
        // D2: midpoint quadratic curve smoothing eliminates folded-line
        // artefacts that vanilla `lineTo` produces at low resolutions.
        const step = w / data.length;
        const getX = (i: number): number => i * step;
        const getY = (i: number): number => h - data[i] * h;
        this._ctx.moveTo(getX(0), getY(0));
        for (let i = 1; i < data.length; i++) {
            const cpx = (getX(i - 1) + getX(i)) / 2;
            const cpy = (getY(i - 1) + getY(i)) / 2;
            this._ctx.quadraticCurveTo(getX(i - 1), getY(i - 1), cpx, cpy);
        }
        this._ctx.lineTo(
            getX(data.length - 1),
            getY(data.length - 1),
        );
        this._ctx.stroke();
    }
}
