/**
 * `dnd` — drag-and-drop namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    DndGetDropZonesResponse,
} from '../../types/responses.js';
import type {
    DndRegisterDropZoneParams,
    DndStartDragParams,
} from '../../types/generated/params.js';

export const dnd = {
    registerDropZone: (opts: DndRegisterDropZoneParams) =>
        bridge.invoke<{ zoneId: string }>('dnd.registerDropZone', opts),
    unregisterDropZone: (zoneId: string) =>
        bridge.invoke<BaseResponse & { script?: string }>(
            'dnd.unregisterDropZone',
            { zoneId },
        ),
    startDrag: (type: string, opts?: Omit<DndStartDragParams, 'type'>) =>
        bridge.invoke<BaseResponse & { trackCount?: number; note?: string }>(
            'dnd.startDrag',
            { type, ...(opts || {}) },
        ),
    getDropZones: () =>
        bridge.invoke<DndGetDropZonesResponse>('dnd.getDropZones'),
};
