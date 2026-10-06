// sdk/src/components/__tests__/playlistViewHarness.ts
//
// Drives `<fb-playlist-view>` without a DOM. The node test environment has no
// `HTMLElement`; `FakeElement` stands in with just the members the element
// touches before it renders rows. The view is never connected, so it builds
// no Shadow DOM and `_updateRows` returns early: tests read the state the
// rows would be rendered from.
//
// No vitest import: this file sits outside the *.test.ts exclusion and is also
// type-checked with the published sources. Callers stub the globals.

type Handler = (data: unknown) => void;

/** A `playback.getCurrentTrackIndex` answer. */
export interface Location {
    success: boolean;
    found?: boolean;
    playlist?: number | null;
    playlistGuid?: string | null;
    index?: number | null;
    error?: string;
    code?: string;
}

/** The private state and hooks of the view the tests drive or read. */
export interface ViewInternals {
    _source: number | string | null;
    _shown: string | null | undefined;
    _playingIndex: number;
    _selection: Set<number>;
    _focusedIndex: number;
    _shiftAnchor: number;
    _dataCache: Map<number, object>;
    _domReady: boolean;
    _subscribe(): void;
    attributeChangedCallback(name: string): void;
    getAttribute(name: string): string | null;
    setAttribute(name: string, value: string): void;
    removeAttribute(name: string): void;
}

/** Stand-in for `HTMLElement`; install it as the global before importing the view. */
export class FakeElement {
    shadowRoot: object | null = null;
    private readonly _attrs = new Map<string, string>();
    attachShadow(): object {
        this.shadowRoot = {};
        return this.shadowRoot;
    }
    getAttribute(name: string): string | null {
        return this._attrs.get(name) ?? null;
    }
    setAttribute(name: string, value: string): void {
        this._attrs.set(name, String(value));
    }
    removeAttribute(name: string): void {
        this._attrs.delete(name);
    }
    dispatchEvent(): boolean {
        return true;
    }
}

/** Let every pending promise callback run. */
export function settle(): Promise<void> {
    return new Promise((r) => setTimeout(r, 0));
}

/** The GUID the double gives the playlist it first lists at `index`. */
export function guidOf(index: number): string {
    return `{00000000-0000-0000-0000-${String(index).padStart(12, '0')}}`;
}

/** A location in the playlist the double first lists at `playlist`, as the host reports it. */
export function playingAt(playlist: number, index: number): Location {
    return { success: true, found: true, playlist, playlistGuid: guidOf(playlist), index };
}

/** A function that records the arguments of every call in `calls`. */
type Recorded<A extends unknown[], R> = ((...args: A) => R) & { calls: A[] };

function recorded<A extends unknown[], R>(impl: (...args: A) => R): Recorded<A, R> {
    const calls: A[] = [];
    const fn = (...args: A): R => {
        calls.push(args);
        return impl(...args);
    };
    return Object.assign(fn, { calls });
}

/**
 * A `window.fb` double: records subscriptions, answers the playlist reads
 * the view makes, and answers `player.getCurrentTrackIndex` with `initial`
 * until told otherwise. It lists eight playlists, `guidOf(0)` to
 * `guidOf(7)` in that order, with the one at `activePlaylist` active;
 * `-1` makes none active.
 */
export function makeFb(initial: Location, activePlaylist = 0) {
    const handlers = new Map<string, Handler[]>();
    const answers: Array<Promise<Location>> = [];
    let fallback: Location = initial;
    let playlists = Array.from({ length: 8 }, (_, i) => guidOf(i));
    let active: string | null = activePlaylist >= 0 ? (playlists[activePlaylist] ?? null) : null;
    const getCurrentTrackIndex = recorded(
        (): Promise<Location> => answers.shift() ?? Promise.resolve(fallback),
    );
    const fb = {
        on: (event: string, handler: Handler): (() => void) => {
            const list = handlers.get(event) ?? [];
            list.push(handler);
            handlers.set(event, list);
            return () => {
                handlers.set(
                    event,
                    (handlers.get(event) ?? []).filter((h) => h !== handler),
                );
            };
        },
        playlist: {
            getActive: recorded(async () => {
                const index = active === null ? -1 : playlists.indexOf(active);
                return index < 0
                    ? { success: true, found: false }
                    : { success: true, found: true, index, guid: active };
            }),
            getAll: recorded(async () => ({
                success: true,
                count: playlists.length,
                playlists: playlists.map((guid, index) => ({
                    index,
                    guid,
                    name: `list ${index}`,
                    trackCount: 20,
                    isActive: guid === active,
                })),
            })),
            getCount: recorded(async (_playlist: number | string) => ({ success: true, count: 20 })),
        },
        player: { getCurrentTrackIndex },
        // The components accept window.fb only when it has the SDK's invoke; the view never calls it.
        invoke: async () => ({ success: false, code: 'NOT_SUPPORTED' }),
    };
    return {
        fb,
        getCurrentTrackIndex,
        /** Answer later reads with `loc`. */
        answerWith(loc: Location): void {
            fallback = loc;
        },
        /** Hold the next read until the returned function is called. */
        holdNext(): (loc: Location) => void {
            let resolve!: (loc: Location) => void;
            answers.push(
                new Promise<Location>((r) => {
                    resolve = r;
                }),
            );
            return resolve;
        },
        /** Make the playlist `guid` names active; `null` leaves none active. */
        activate(guid: string | null): void {
            active = guid;
        },
        /** Replace the playlists the double lists, by GUID, in playlist order. */
        setPlaylists(guids: string[]): void {
            playlists = [...guids];
        },
        emit(event: string, data: unknown): void {
            for (const h of handlers.get(event) ?? []) h(data);
        },
    };
}

/**
 * Create a view showing `playlist`, an index or a GUID (`null` follows the
 * active playlist), subscribe it and wait for its first load. `HTMLElement`
 * and `window.fb` must already be stubbed, on a fresh module registry.
 */
export async function mount(playlist: number | string | null): Promise<ViewInternals> {
    const { FbPlaylistView } = await import('../FbPlaylistView.js');
    const view = new FbPlaylistView() as unknown as ViewInternals;
    if (playlist !== null) view.setAttribute('playlist', String(playlist));
    view._source = playlist;
    view._domReady = true;
    view._subscribe();
    await settle();
    return view;
}
