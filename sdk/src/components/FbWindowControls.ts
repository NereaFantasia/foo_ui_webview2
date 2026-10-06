/**
 * `<fb-window-controls>` — minimize / maximize-or-restore / close
 * button group.
 *
 * Each button takes a slotted icon
 * (`<slot name="minimize-icon">`, `maximize-icon`, `restore-icon`,
 * `close-icon`) so themes can swap glyphs without touching the markup.
 *
 * Maximize and restore share a `[part=restore-icon]` button that's
 * shown only while the window is maximized; the maximize button is
 * hidden in that state. Both fire `fb.ui.toggleMaximize()`.
 *
 * Subscribes to `window:stateChanged` plus a one-shot
 * `fb.ui.isMaximized()` to keep the host attribute `maximized` and
 * the visible button in sync with the window state. Only the page's
 * own window counts: an event whose `windowId` names another window is
 * ignored, and events that arrive before `fb.ui.getCurrentWindowId()`
 * answers are held until it does (see {@link OwnWindowStateFilter}).
 */

import { FbBaseElement } from './FbBaseElement.js';
import { OwnWindowStateFilter, learnOwnWindowId } from './ownWindowState.js';
import { getFb } from './runtime.js';
import type { WindowStateChangedPayload } from '../types/generated/events.js';
import type {
    FbWindowCloseDetail,
    FbWindowMaximizeDetail,
    FbWindowMinimizeDetail,
} from './types.js';

export class FbWindowControls extends FbBaseElement {
    private _minBtn!: HTMLButtonElement;
    private _maxBtn!: HTMLButtonElement;
    private _restoreBtn!: HTMLButtonElement;
    private _closeBtn!: HTMLButtonElement;
    /** Filter of the current connection; one left over from an earlier connection delivers nothing. */
    private _stateFilter: OwnWindowStateFilter<WindowStateChangedPayload> | null = null;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}</style>` +
            `<div part="controls">` +
            `<button part="minimize-button" aria-label="Minimize"><slot name="minimize-icon">\u2014</slot></button>` +
            `<button part="maximize-button" aria-label="Maximize"><slot name="maximize-icon">\u25A1</slot></button>` +
            `<button part="restore-icon" hidden><slot name="restore-icon">\u2750</slot></button>` +
            `<button part="close-button" aria-label="Close"><slot name="close-icon">\u2715</slot></button>` +
            `</div>`;
        this._minBtn = this._$<HTMLButtonElement>('[part=minimize-button]')!;
        this._maxBtn = this._$<HTMLButtonElement>('[part=maximize-button]')!;
        this._restoreBtn = this._$<HTMLButtonElement>('[part=restore-icon]')!;
        this._closeBtn = this._$<HTMLButtonElement>('[part=close-button]')!;
    }

    protected override _setupEvents(): void {
        this._listen(this._minBtn, 'click', () => {
            try {
                void getFb().ui.minimize();
            } catch {
                /* silent */
            }
            this._emit<FbWindowMinimizeDetail>('fb-window-minimize', {});
        });
        this._listen(this._maxBtn, 'click', () => {
            try {
                void getFb().ui.toggleMaximize();
            } catch {
                /* silent */
            }
            this._emit<FbWindowMaximizeDetail>('fb-window-maximize', {});
        });
        this._listen(this._restoreBtn, 'click', () => {
            try {
                void getFb().ui.toggleMaximize();
            } catch {
                /* silent */
            }
            this._emit<FbWindowMaximizeDetail>('fb-window-maximize', {});
        });
        this._listen(this._closeBtn, 'click', () => {
            try {
                void getFb().ui.close();
            } catch {
                /* silent */
            }
            this._emit<FbWindowCloseDetail>('fb-window-close', {});
        });
    }

    protected override _subscribe(): void {
        const filter = new OwnWindowStateFilter<WindowStateChangedPayload>((state) => {
            if (this._stateFilter === filter) this._update(!!state?.isMaximized);
        });
        this._stateFilter = filter;
        this._sub('window:stateChanged', (data) => filter.push(data));
        learnOwnWindowId(filter);
        getFb()
            .ui.isMaximized()
            .then((r) =>
                this._update(
                    !!(r as { isMaximized?: boolean } | null)?.isMaximized,
                ),
            )
            .catch(() => {
                /* silent */
            });
    }

    private _update(maximized: boolean): void {
        this._maxBtn.hidden = maximized;
        this._restoreBtn.hidden = !maximized;
        if (maximized) this.setAttribute('maximized', '');
        else this.removeAttribute('maximized');
    }
}
