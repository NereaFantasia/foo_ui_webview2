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
 * `maximized` reflects the actual window state immediately. Only the
 * page's own window counts: an event whose `windowId` names another
 * window is ignored, and events that arrive before
 * `fb.ui.getCurrentWindowId()` answers are held until it does (see
 * {@link OwnWindowStateFilter}).
 */

import { FbBaseElement } from './FbBaseElement.js';
import { OwnWindowStateFilter, learnOwnWindowId } from './ownWindowState.js';
import { getFb } from './runtime.js';
import type { WindowStateChangedPayload } from '../types/generated/events.js';
import type { FbTitlebarDblclickDetail } from './types.js';

export class FbTitlebar extends FbBaseElement {
    private _dragRegion!: HTMLDivElement;
    /** Filter of the current connection; one left over from an earlier connection delivers nothing. */
    private _stateFilter: OwnWindowStateFilter<WindowStateChangedPayload> | null = null;

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
        const filter = new OwnWindowStateFilter<WindowStateChangedPayload>((state) => {
            if (this._stateFilter === filter) this._updateState({ isMaximized: !!state?.isMaximized });
        });
        this._stateFilter = filter;
        this._sub('window:stateChanged', (data) => filter.push(data));
        learnOwnWindowId(filter);
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
