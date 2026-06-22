/**
 * `<fb-titlebar>` — custom titlebar drag region.
 *
 * Two slots (`left` and `right`) for icons / window-control children
 * are kept above the OS-level drag area
 * (`-webkit-app-region: drag`) using `::slotted(*) { -webkit-app-region:
 * no-drag }` so child clicks still register normally.
 *
 * Pointer behaviour:
 * - Left-button mousedown on `[part=drag-region]` → `fb.ui.startDrag()`.
 * - Double-click → `fb.ui.toggleMaximize()` and emit
 *   `fb-titlebar-dblclick` so themes can override.
 * - Right-click → `fb.ui.showSystemMenu(screenX, screenY)`.
 *
 * Subscribes to `window:stateChanged` and to a one-shot
 * `fb.ui.isMaximized()` query at connect time so the host attribute
 * `maximized` reflects the actual window state immediately.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { FbTitlebarDblclickDetail } from './types.js';

export class FbTitlebar extends FbBaseElement {
    private _dragRegion!: HTMLDivElement;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:flex;align-items:center;-webkit-app-region:drag}` +
            `::slotted(*){-webkit-app-region:no-drag}</style>` +
            `<slot name="left"></slot>` +
            `<div part="drag-region" style="flex:1;min-height:32px"></div>` +
            `<slot name="right"></slot>`;
        this._dragRegion = this._$<HTMLDivElement>('[part=drag-region]')!;
    }

    protected override _setupEvents(): void {
        this._listen<MouseEvent>(this._dragRegion, 'mousedown', (e) => {
            if (e.button === 0) {
                try {
                    void getFb().ui.startDrag();
                } catch {
                    /* silent */
                }
            }
        });
        this._listen(this._dragRegion, 'dblclick', () => {
            try {
                void getFb().ui.toggleMaximize();
            } catch {
                /* silent */
            }
            this._emit<FbTitlebarDblclickDetail>('fb-titlebar-dblclick', {});
        });
        this._listen<MouseEvent>(this._dragRegion, 'contextmenu', (e) => {
            e.preventDefault();
            try {
                void getFb().ui.showSystemMenu(e.screenX, e.screenY);
            } catch {
                /* silent */
            }
        });
    }

    protected override _subscribe(): void {
        this._sub('window:stateChanged', (data) =>
            this._updateState(data as { isMaximized?: boolean }),
        );
        getFb()
            .ui.isMaximized()
            .then((r) =>
                this._updateState({
                    isMaximized: !!(r as { isMaximized?: boolean } | null)
                        ?.isMaximized,
                }),
            )
            .catch(() => {
                /* silent */
            });
    }

    private _updateState(data: { isMaximized?: boolean }): void {
        if (data.isMaximized) this.setAttribute('maximized', '');
        else this.removeAttribute('maximized');
    }
}
