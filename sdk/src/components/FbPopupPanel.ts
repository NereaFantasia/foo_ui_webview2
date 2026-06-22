/**
 * `<fb-popup-panel>` — declarative wrapper around `fb.ui.createPopup`.
 *
 * Click anywhere on the host element opens the popup
 * (host `[part=]` exposed slot is the only child container). Once
 * open, subsequent clicks are no-ops; close via the imperative
 * {@link FbPopupPanel.close} method or by closing the popup window
 * itself.
 *
 * Attribute → `fb.ui.createPopup` opt mapping:
 * | Attribute        | Opt key         | Default |
 * |------------------|-----------------|---------|
 * | `url`            | `url`           | `''`    |
 * | `width`          | `width`         | `400`   |
 * | `height`         | `height`        | `300`   |
 * | `popup-title`    | `title`         | `''`    |
 * | `resizable`      | `resizable`     | `true` (set `'false'` to disable) |
 * | `always-on-top`  | `alwaysOnTop`   | presence-based |
 * | `show-in-taskbar`| `showInTaskbar` | presence-based |
 * | `frame`          | `frame`         | `true` (set `'false'` to disable) |
 * | `transparent`    | `transparent`   | presence-based |
 * | `before-close`   | `beforeClose`   | presence-based |
 *
 * Subscribes to `window:popupClosed` (clears state on close) and
 * `window:message` (re-emits as `fb-popup-message` for theme handlers).
 *
 * `disconnectedCallback` proactively closes the popup so detached
 * components don't leak their child windows.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type {
    FbPopupCloseDetail,
    FbPopupMessageDetail,
    FbPopupOpenDetail,
} from './types.js';

interface CreatePopupResult {
    success?: boolean;
    windowId?: string;
}

export class FbPopupPanel extends FbBaseElement {
    private _windowId: string | null = null;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}</style>` + `<slot></slot>`;
        this._windowId = null;
    }

    protected override _setupEvents(): void {
        this._listen(this, 'click', () => {
            if (!this._windowId) void this.open();
        });
    }

    protected override _subscribe(): void {
        this._sub('window:popupClosed', (data) => {
            const d = data as { windowId?: string } | null;
            if (d?.windowId === this._windowId) {
                const closedId = this._windowId;
                this._windowId = null;
                this.removeAttribute('open');
                if (closedId)
                    this._emit<FbPopupCloseDetail>('fb-popup-close', {
                        windowId: closedId,
                    });
            }
        });
        this._sub('window:message', (data) => {
            this._emit<FbPopupMessageDetail>(
                'fb-popup-message',
                data as FbPopupMessageDetail,
            );
        });
    }

    /**
     * Open the popup window declared by this element's attributes.
     * Subsequent calls while a popup is already open are no-ops.
     *
     * @returns the popup `windowId` once it's open, or `null` on
     * failure.
     */
    async open(): Promise<string | null> {
        if (this._windowId) return this._windowId;
        try {
            const result = (await getFb().ui.createPopup({
                url: this.getAttribute('url') || '',
                width: parseInt(this.getAttribute('width') || '400', 10) || 400,
                height:
                    parseInt(this.getAttribute('height') || '300', 10) || 300,
                title: this.getAttribute('popup-title') || '',
                resizable: this.getAttribute('resizable') !== 'false',
                alwaysOnTop: this.hasAttribute('always-on-top'),
                showInTaskbar: this.hasAttribute('show-in-taskbar'),
                frame: this.getAttribute('frame') !== 'false',
                transparent: this.hasAttribute('transparent'),
                beforeClose: this.hasAttribute('before-close'),
            })) as CreatePopupResult | null;
            if (result?.success && result.windowId) {
                this._windowId = result.windowId;
                this.setAttribute('open', '');
                this.setAttribute('window-id', this._windowId);
                this._emit<FbPopupOpenDetail>('fb-popup-open', {
                    windowId: this._windowId,
                });
            }
            return this._windowId;
        } catch {
            return null;
        }
    }

    /** Close the popup if open. No-op otherwise. */
    async close(): Promise<void> {
        if (!this._windowId) return;
        try {
            await getFb().ui.closePopup(this._windowId);
        } catch {
            /* silent */
        }
    }

    /** Current popup window id, or `null` if no popup is open. */
    get windowId(): string | null {
        return this._windowId;
    }

    override disconnectedCallback(): void {
        if (this._windowId) {
            getFb()
                .ui.closePopup(this._windowId)
                .catch(() => {
                    /* silent */
                });
        }
        super.disconnectedCallback();
    }
}
