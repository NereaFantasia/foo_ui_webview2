/**
 * `<fb-properties-panel>` — track properties read-only viewer.
 *
 * Loads tag + technical info via `fb.metadata.read(path)` and renders
 * three optional groups:
 * - `metadata`  — TITLE / ARTIST / ALBUM / ALBUMARTIST / DATE / GENRE /
 *                 TRACKNUMBER / COMMENT (whitelist, in display order).
 * - `technical` — codec / bitrate / sample-rate / channels / duration.
 * - `location`  — absolute path.
 *
 * Attributes:
 * - `path`   — explicit track path; suppresses the
 *              `playback:trackChanged` auto-reload behaviour.
 * - `groups` — comma-separated subset of the three groups above,
 *              default `'metadata,technical,location'`.
 *
 * Reflects host attribute `track-path` once a load completes.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { formatTime } from './constants.js';
import { getFb } from './runtime.js';
import type { JsonObject } from '../types/json.js';

export class FbPropertiesPanel extends FbBaseElement {
    private _container!: HTMLElement;
    private _trackPath: string | null = null;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:block;overflow-y:auto}</style>` +
            `<div part="container"></div>`;
        this._container = this._$<HTMLElement>('[part=container]')!;
    }

    protected override _setupEvents(): void {
        /* no DOM listeners — viewer-only */
    }

    protected override _subscribe(): void {
        const path = this.getAttribute('path');
        if (!path) {
            this._sub('playback:trackChanged', () => {
                void this._loadProperties();
            });
        }
        void this._loadProperties();
    }

    private async _loadProperties(): Promise<void> {
        const fb = getFb();
        try {
            let path = this.getAttribute('path');
            if (!path) {
                const track = (await fb.player.getCurrentTrack()) as
                    | { path?: string }
                    | null;
                path = track?.path ?? null;
            }
            if (!path || path === this._trackPath) return;
            this._trackPath = path;

            // `metadata.read` returns the typed `MetadataReadResponse`
            // envelope directly, so no cast is needed.
            const result = await fb.metadata.read(path);
            if (!result?.success) return;

            const groups = (
                this.getAttribute('groups') ||
                'metadata,technical,location'
            ).split(',');
            let html = '';

            if (groups.includes('metadata') && result.tags) {
                html += this._buildGroup('metadata', 'Metadata', result.tags, [
                    'TITLE',
                    'ARTIST',
                    'ALBUM',
                    'ALBUMARTIST',
                    'DATE',
                    'GENRE',
                    'TRACKNUMBER',
                    'COMMENT',
                ]);
            }
            if (groups.includes('technical')) {
                const info = result.info || {};
                const channelLabel =
                    info.channels === 2
                        ? 'Stereo'
                        : info.channels === 1
                          ? 'Mono'
                          : info.channels
                            ? `${info.channels}ch`
                            : '';
                const tech: Record<string, string> = {
                    Codec: info.codec || '',
                    Bitrate: info.bitrate ? `${info.bitrate} kbps` : '',
                    'Sample Rate': info.sampleRate
                        ? `${info.sampleRate} Hz`
                        : '',
                    Channels: channelLabel,
                    Duration: info.duration ? formatTime(info.duration) : '',
                };
                html += this._buildGroup('technical', 'Technical', tech);
            }
            if (groups.includes('location')) {
                html += this._buildGroup('location', 'Location', { Path: path });
            }

            this._container.innerHTML = html;
            this.setAttribute('track-path', path);
        } catch {
            /* silent — leave previous render in place */
        }
    }

    private _buildGroup(
        id: string,
        title: string,
        data: JsonObject,
        keys?: string[],
    ): string {
        const entries = keys
            ? keys
                  .map<[string, unknown]>((k) => [k, data[k]])
                  .filter(([, v]) => v !== undefined && v !== null && v !== '')
            : Object.entries(data).filter(
                  ([, v]) => v !== undefined && v !== null && v !== '',
              );
        if (entries.length === 0) return '';
        return (
            `<div part="group" data-group="${id}">` +
            `<div part="group-title">${this._escHtml(title)}</div>` +
            entries
                .map(
                    ([k, v]) =>
                        `<div part="row">` +
                        `<span part="label">${this._escHtml(k)}</span>` +
                        `<span part="value">${this._escHtml(String(v))}</span>` +
                        `</div>`,
                )
                .join('') +
            `</div>`
        );
    }
}
