import { call } from '../call.js';
import type { PanelSetConfigParams } from '../../types/generated/params.js';

/**
 * `panel` — webview panel-level config namespace.
 */
export const panel = {
    /** Fails with `code: 'NOT_FOUND'` on a standalone window, which has no panel. */
    getConfig: () => call('panel.getConfig'),
    setConfig: (opts: PanelSetConfigParams) =>
        call('panel.setConfig', opts),
};
