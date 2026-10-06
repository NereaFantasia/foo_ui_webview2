import { call } from '../call.js';
import type {
    DiscoveryExecuteContextMenuByPathParams,
    DiscoveryExecuteContextMenuCommandParams,
    DiscoveryGetContextMenuCommandsParams,
    DiscoveryGetMainMenuCommandsParams,
    DiscoverySearchCommandsParams,
} from '../../types/generated/params.js';

/**
 * `discovery` — service / menu / component discovery namespace.
 */
export const discovery = {
    getAllServices: () =>
        call(
            'discovery.getAllServices',
        ),
    /**
     * Lists main-menu commands. Components that build their submenu at runtime
     * (`mainmenu_commands_v2`, e.g. ESLyric) are expanded by default, so the
     * result includes their child commands in addition to the parent slot.
     * Pass `{ expandDynamic: false }` for the raw static registry only.
     *
     * Entries the host would not show — a command whose `get_display()` returns
     * false, or one carrying `flag_defaulthidden` — are omitted by default,
     * because they are not reachable from the real menu. Pass
     * `{ includeHidden: true }` to get the unfiltered superset.
     */
    getMainMenuCommands: (opts?: DiscoveryGetMainMenuCommandsParams) =>
        call(
            'discovery.getMainMenuCommands',
            opts,
        ),
    getMainMenuGroups: () =>
        call(
            'discovery.getMainMenuGroups',
        ),
    /**
     * Executes a main-menu command. For a command expanded from a dynamic
     * submenu, pass the entry's `subGuid` as well; without it only the static
     * command GUID is dispatched. A GUID no command owns fails with `NOT_FOUND`.
     */
    executeMainMenuCommand: (guid: string, subGuid?: string) =>
        call(
            'discovery.executeMainMenuCommand',
            subGuid ? { guid, subGuid } : { guid },
        ),
    /**
     * Lists context-menu commands with their state.
     *
     * `enabled` / `checked` are only observable when a track is selected or
     * playing, because the SDK evaluates display data against a track set.
     * Check the response's `stateKnown` before trusting them: when it is false,
     * only `hidden` is meaningful. `FORCE_OFF` entries are shortcut-list-only
     * per the SDK and are omitted unless `includeHidden` is set.
     */
    getContextMenuCommands: (opts?: DiscoveryGetContextMenuCommandsParams) =>
        call(
            'discovery.getContextMenuCommands',
            opts,
        ),
    /**
     * Executes a context-menu command by GUID against the current selection (or
     * the playing track).
     *
     * A `FORCE_OFF` command is refused rather than dispatched: the SDK treats
     * that state as "keyboard-shortcut list only", so the host never draws it and
     * running it would perform something the user could not have clicked. The
     * refusal fails with `NOT_SUPPORTED` and carries `hidden: true`. Pass
     * `{ force: true }` to dispatch anyway. `DEFAULT_OFF` commands (hidden unless
     * Shift is held) are still invocable and are never refused. With nothing
     * selected or playing the call fails with `NO_ACTIVE_ITEM`.
     */
    executeContextMenuCommand: (opts: DiscoveryExecuteContextMenuCommandParams) =>
        call(
            'discovery.executeContextMenuCommand',
            opts,
        ),
    /**
     * Executes a context-menu command addressed by its display path, e.g.
     * `'Playback Statistics/Rating/5'`.
     *
     * Each path segment must match a menu label exactly once, after normalization
     * (mnemonic `&`, a trailing ellipsis, accelerator text and ASCII case are
     * ignored). Matching is never a substring test, so `'Rating/1'` cannot
     * resolve to `'Rating/10'`.
     *
     * A path that matches several commands is refused instead of guessed —
     * duplicated labels are common in real hosts. That failure carries
     * `match: 'ambiguous'` and a `candidates` list of full names; use it to
     * refine the path, or address the command by GUID via
     * {@link executeContextMenuCommand}, which is the only stable identifier.
     */
    executeContextMenuByPath: (opts: DiscoveryExecuteContextMenuByPathParams) =>
        call(
            'discovery.executeContextMenuByPath',
            opts,
        ),
    /**
     * Dumps the full context-menu tree for the current selection (or the playing
     * track), as the host would build it.
     *
     * The walk is bounded in depth and in children per node, and any clipping is
     * reported rather than silent: check `truncated` on the response for the whole
     * tree, or on an individual node for its subtree. A popup node carries both
     * `childCount` (the host's real count) and `childrenReturned` (what this
     * response contains) so the two can be reconciled without counting.
     */
    getContextMenuTree: () =>
        call(
            'discovery.getContextMenuTree',
        ),
    getInputFormats: () =>
        call(
            'discovery.getInputFormats',
        ),
    getComponents: () =>
        call('discovery.getComponents'),
    getUIElements: () =>
        call('discovery.getUIElements'),
    getDspEntries: () =>
        call('discovery.getDspEntries'),
    getOutputDevices: () =>
        call(
            'discovery.getOutputDevices',
        ),
    getPreferencePages: () =>
        call(
            'discovery.getPreferencePages',
        ),
    /**
     * Case-insensitive substring search over command names, descriptions and menu
     * paths, across both menu families.
     *
     * Each hit carries `type` (`'mainmenu'` / `'contextmenu'`) plus the same state
     * fields the enumeration endpoints return, so a caller can tell whether a hit
     * is invocable without a second round trip. Pass `{ scope: 'mainmenu' }` or
     * `{ scope: 'contextmenu' }` to search one family only.
     *
     * Entries the host would not show are excluded, matching the enumeration
     * endpoints; pass `{ includeHidden: true }` for the unfiltered superset.
     * Dynamic submenus are expanded by default — pass `{ expandDynamic: false }`
     * for the static registry only.
     *
     * Context-menu state is only observable with a track selected or playing.
     * When the context family was searched without one, the response's
     * `stateKnown` is false and its hits' `enabled` / `checked` must not be
     * filtered on.
     */
    searchCommands: (query: string, opts?: Omit<DiscoverySearchCommandsParams, 'query'>) =>
        call('discovery.searchCommands', {
            query,
            ...opts,
        }),
};
