/**
 * `panel` — webview panel-level config namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    PanelGetConfigResponse,
} from '../../types/responses.js';
import type { PanelSetConfigParams } from '../../types/generated/params.js';

export const panel = {
    getConfig: () => bridge.invoke<PanelGetConfigResponse>('panel.getConfig'),
    setConfig: (opts: PanelSetConfigParams) =>
        bridge.invoke<BaseResponse & { changed?: boolean }>(
            'panel.setConfig',
            opts,
        ),
};
