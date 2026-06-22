/**
 * `<fb-library-filesystem-tree>` — filesystem-style library browser.
 *
 * Walks the typed library tree starting from `library.getRoots()`,
 * lazily expanding directories via `library.browseTree`. When
 * `include-files="true"` is set, file rows are also rendered and
 * selectable.
 *
 * Selection model is identical to {@link FbLibraryTree} (single /
 * Ctrl-toggle / Shift-range / DUI-style right-click promotion).
 *
 * Events:
 * - `fb-library-fs-select`  — selection changed.
 * - `fb-library-fs-play`    — row double-clicked.
 * - `fb-library-fs-context` — right-click (`{ ..., x, y }`).
 *
 * Attributes (observed):
 * - `root-id`        — pin the tree to a specific root (skips the
 *                      multi-root header). Default unset.
 * - `include-files`  — `'true'` to render track rows as well.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { formatTime } from './constants.js';
import { getFb } from './runtime.js';
import type {
    FbLibraryFsContextDetail,
    FbLibraryFsPlayDetail,
    FbLibraryFsSelectDetail,
} from './types.js';
import type { JsonObject } from '../types/json.js';

interface FsRoot {
    id: string;
    displayName: string;
    absolutePath: string;
    trackCount?: number;
}

interface FsDirectory {
    pathId: string;
    name?: string;
    displayName?: string;
    absolutePath?: string;
    trackCount?: number;
    childDirectoryCount?: number;
    hasChildren?: boolean;
}

interface FsFile {
    title?: string;
    filename?: string;
    duration?: number;
    path?: string;
    absolutePath?: string;
    subsong?: number;
    
}

interface FsBrowseResult {
    success?: boolean;
    directories?: FsDirectory[];
    files?: FsFile[];
}

interface FsCacheEntry {
    directories: FsDirectory[];
    files: FsFile[];
}

export class FbLibraryFilesystemTree extends FbBaseElement {
    private _container!: HTMLElement;
    private _rootId: string | null = null;
    private _includeFiles = false;
    private _roots: FsRoot[] = [];
    private _expanded = new Set<string>();
    private _loading = new Set<string>();
    private _nodeCache = new Map<string, FsCacheEntry>();
    private _lastSelected: HTMLElement | null = null;
    /**
     * Monotonic counter bumped on any generation-changing event:
     * `root-id` / `include-files` change, or disconnect. Every async
     * `_loadRoots` / `_loadChildren` captures the current value and
     * bails if a newer generation started, preventing stale trees from
     * overwriting the active view or mutating a detached DOM.
     */
    private _generation = 0;

    static get observedAttributes(): string[] {
        return ['root-id', 'include-files'];
    }

    attributeChangedCallback(name: string): void {
        if (!this._domReady) return;
        if (name === 'root-id') {
            this._rootId = this.getAttribute('root-id') || null;
            this._expanded.clear();
            this._nodeCache.clear();
            this._generation++;
            void this._loadRoots();
        }
        if (name === 'include-files') {
            this._includeFiles =
                this.getAttribute('include-files') === 'true';
            this._nodeCache.clear();
            this._generation++;
            this._rebuildTree();
        }
    }

    override disconnectedCallback(): void {
        // Invalidate any in-flight `_loadRoots` / `_loadChildren`
        // dispatches before the base class tears down subscriptions.
        this._generation++;
        super.disconnectedCallback();
    }

    protected override _buildDOM(): void {
        this._rootId = this.getAttribute('root-id') || null;
        this._includeFiles = this.getAttribute('include-files') === 'true';

        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:block;overflow-y:auto}` +
            `[part=container]{display:flex;flex-direction:column}` +
            `[part~=node]{display:flex;align-items:center;cursor:pointer;gap:4px}` +
            `[part~=child]{display:flex;align-items:center;cursor:pointer;gap:4px}` +
            `[part~=file]{display:flex;align-items:center;cursor:pointer;gap:4px}` +
            `</style>` +
            `<div part="container" role="tree"></div>`;
        this._container = this._$<HTMLElement>('[part=container]')!;
    }

    protected override _setupEvents(): void {
        this._listen(this._container, 'click', (e: Event) => {
            const me = e as MouseEvent;
            const target = me.target as HTMLElement | null;
            const node = target?.closest(
                '[data-node-type]',
            ) as HTMLElement | null;
            if (!node) return;
            const nodeType = node.dataset.nodeType || '';
            if (nodeType === 'header') return;

            // Toggle on caret only — preserves selection state.
            if (
                target?.closest('[part=node-toggle]') &&
                (nodeType === 'root' || nodeType === 'directory')
            ) {
                void this._toggleExpand(node);
                return;
            }

            this._selectNode(node, me.ctrlKey || me.metaKey, me.shiftKey);
            this._emitSelect(node);
        });

        this._listen(this._container, 'dblclick', (e: Event) => {
            const target = (e as MouseEvent).target as HTMLElement | null;
            const node = target?.closest(
                '[data-node-type]',
            ) as HTMLElement | null;
            if (!node) return;
            const nodeType = node.dataset.nodeType || '';
            if (nodeType === 'header') return;
            this._emitPlay(node);
        });

        this._listen(this._container, 'contextmenu', (e: Event) => {
            const me = e as MouseEvent;
            const target = me.target as HTMLElement | null;
            const node = target?.closest(
                '[data-node-type]',
            ) as HTMLElement | null;
            if (!node) return;
            const nodeType = node.dataset.nodeType || '';
            if (nodeType === 'header') return;
            me.preventDefault();
            // DUI behaviour: right-click promotes an unselected row
            // before the context menu opens.
            if (!node.hasAttribute('selected')) {
                this._selectNode(node, false, false);
                this._emitSelect(node);
            }
            const detail: FbLibraryFsContextDetail = {
                type: nodeType,
                rootId: node.dataset.rootId || '',
                pathId: node.dataset.pathId || '',
                absolutePath: node.dataset.absolutePath || '',
                x: me.clientX,
                y: me.clientY,
            };
            if (nodeType === 'file' && node.dataset.fileInfo) {
                try {
                    detail.file = JSON.parse(node.dataset.fileInfo);
                } catch {
                    /* drop malformed cached payload */
                }
            }
            this._emit<FbLibraryFsContextDetail>(
                'fb-library-fs-context',
                detail,
            );
        });
    }

    protected override _subscribe(): void {
        const invalidate = (): void => {
            this._nodeCache.clear();
            void this._loadRoots();
        };
        this._sub('library:itemsAdded', invalidate);
        this._sub('library:itemsRemoved', invalidate);
        this._sub('library:itemsModified', invalidate);
        this._sub('library:initialized', invalidate);
        void this._loadRoots();
    }

    private async _loadRoots(): Promise<void> {
        const fb = getFb();
        const gen = this._generation;
        try {
            if (this._rootId) {
                // Single-root mode: skip the multi-root header.
                this._roots = [];
                this._rebuildTree();
                if (gen !== this._generation || !this.isConnected) return;
                await this._loadChildren(this._rootId, '');
                return;
            }
            const result = (await fb.library.getRoots()) as
                | { success?: boolean; roots?: FsRoot[] }
                | null;
            // Generation guard: root-id / include-files changed or the
            // component was detached while we awaited the host call.
            if (gen !== this._generation || !this.isConnected) return;
            if (!result || !result.success) {
                this._roots = [];
            } else {
                this._roots = result.roots || [];
            }
            this._rebuildTree();
        } catch {
            if (gen !== this._generation || !this.isConnected) return;
            this._roots = [];
            this._rebuildTree();
        }
    }

    private _rebuildTree(): void {
        const frag = document.createDocumentFragment();

        if (this._rootId) {
            const cacheKey = this._rootId + '::';
            const cached = this._nodeCache.get(cacheKey);
            if (cached) {
                this._appendDirectories(
                    frag,
                    cached.directories,
                    this._rootId,
                    0,
                );
                if (this._includeFiles && cached.files) {
                    this._appendFiles(frag, cached.files, this._rootId, '');
                }
            }
        } else {
            // Multi-root: top-level header listing every library root.
            const header = document.createElement('div');
            header.setAttribute('part', 'header');
            header.dataset.nodeType = 'header';
            header.setAttribute('role', 'treeitem');
            const totalCount = this._roots.reduce(
                (s, r) => s + (r.trackCount || 0),
                0,
            );
            header.innerHTML =
                `<span part="node-label">All Music</span>` +
                `<span part="node-count">(${totalCount})</span>`;
            frag.appendChild(header);

            for (const root of this._roots) {
                const expanded = this._expanded.has('root::' + root.id);
                const node = document.createElement('div');
                node.setAttribute('part', 'node root');
                node.dataset.nodeType = 'root';
                node.dataset.rootId = root.id;
                node.dataset.pathId = '';
                node.dataset.absolutePath = root.absolutePath;
                node.setAttribute('role', 'treeitem');
                if (expanded) node.setAttribute('expanded', '');
                node.innerHTML =
                    `<span part="node-toggle">${expanded ? '\u25BE' : '\u25B8'}</span>` +
                    `<span part="node-label">${this._escHtml(root.displayName)}</span>` +
                    `<span part="node-count">(${root.trackCount || 0})</span>`;
                frag.appendChild(node);

                const children = document.createElement('div');
                children.setAttribute('part', 'node-children');
                children.style.display = expanded ? '' : 'none';
                if (expanded) {
                    const cacheKey = root.id + '::';
                    const cached = this._nodeCache.get(cacheKey);
                    if (cached) {
                        this._appendDirectories(
                            children,
                            cached.directories,
                            root.id,
                            1,
                        );
                        if (this._includeFiles && cached.files) {
                            this._appendFiles(
                                children,
                                cached.files,
                                root.id,
                                '',
                            );
                        }
                    }
                }
                frag.appendChild(children);
            }
        }

        this._container.innerHTML = '';
        this._container.appendChild(frag);
    }

    private _appendDirectories(
        parent: ParentNode,
        directories: FsDirectory[],
        rootId: string,
        depth: number,
    ): void {
        for (const dir of directories) {
            const expandKey = rootId + '::' + dir.pathId;
            const expanded = this._expanded.has(expandKey);
            const node = document.createElement('div');
            node.setAttribute('part', 'node directory');
            node.dataset.nodeType = 'directory';
            node.dataset.rootId = rootId;
            node.dataset.pathId = dir.pathId;
            node.dataset.absolutePath = dir.absolutePath || '';
            node.setAttribute('role', 'treeitem');
            node.style.paddingLeft = depth * 16 + 'px';
            if (expanded) node.setAttribute('expanded', '');
            node.innerHTML =
                `<span part="node-toggle">${dir.hasChildren ? (expanded ? '\u25BE' : '\u25B8') : '\u00A0'}</span>` +
                `<span part="node-label">${this._escHtml(dir.displayName || dir.name || '')}</span>` +
                `<span part="node-count">(${dir.trackCount || 0})</span>`;
            parent.appendChild(node);

            if (
                dir.hasChildren ||
                (this._includeFiles && (dir.trackCount || 0) > 0)
            ) {
                const children = document.createElement('div');
                children.setAttribute('part', 'node-children');
                children.style.display = expanded ? '' : 'none';
                if (expanded) {
                    const cacheKey = rootId + '::' + dir.pathId;
                    const cached = this._nodeCache.get(cacheKey);
                    if (cached) {
                        this._appendDirectories(
                            children,
                            cached.directories,
                            rootId,
                            depth + 1,
                        );
                        if (this._includeFiles && cached.files) {
                            this._appendFiles(
                                children,
                                cached.files,
                                rootId,
                                dir.pathId,
                            );
                        }
                    }
                }
                parent.appendChild(children);
            }
        }
    }

    private _appendFiles(
        parent: ParentNode,
        files: FsFile[],
        rootId: string,
        pathId: string,
    ): void {
        for (const file of files) {
            const node = document.createElement('div');
            node.setAttribute('part', 'node file');
            node.dataset.nodeType = 'file';
            node.dataset.rootId = rootId;
            node.dataset.pathId = pathId;
            node.dataset.absolutePath = file.absolutePath || file.path || '';
            node.dataset.subsong = String(file.subsong ?? 0);
            try {
                node.dataset.fileInfo = JSON.stringify(file);
            } catch {
                /* serialise failure → skip dataset cache */
            }
            node.setAttribute('role', 'treeitem');
            node.innerHTML =
                `<span part="node-label">${this._escHtml(file.title || file.filename || '(Unknown)')}</span>` +
                `<span part="node-count">${formatTime(file.duration || 0)}</span>`;
            parent.appendChild(node);
        }
    }

    private async _toggleExpand(nodeEl: HTMLElement): Promise<void> {
        const nodeType = nodeEl.dataset.nodeType || '';
        const rootId = nodeEl.dataset.rootId || '';
        const pathId = nodeEl.dataset.pathId || '';

        // Roots use the `'root::'` prefix to disambiguate from the
        // root's "::" directory entry; directory entries use raw
        // `rootId::pathId`.
        const key =
            nodeType === 'root'
                ? 'root::' + rootId
                : rootId + '::' + pathId;

        if (this._expanded.has(key)) {
            this._expanded.delete(key);
            nodeEl.removeAttribute('expanded');
            const toggle = nodeEl.querySelector('[part=node-toggle]');
            if (toggle) toggle.textContent = '\u25B8';
            const children = nodeEl.nextElementSibling as HTMLElement | null;
            if (children?.getAttribute('part') === 'node-children') {
                children.style.display = 'none';
            }
        } else {
            this._expanded.add(key);
            nodeEl.setAttribute('expanded', '');
            const toggle = nodeEl.querySelector('[part=node-toggle]');
            if (toggle) toggle.textContent = '\u25BE';
            const children = nodeEl.nextElementSibling as HTMLElement | null;
            if (children?.getAttribute('part') === 'node-children') {
                children.style.display = '';
                if (!children.hasChildNodes()) {
                    await this._loadChildren(rootId, pathId, children);
                }
            }
        }
    }

    private async _loadChildren(
        rootId: string,
        pathId: string,
        container?: HTMLElement | null,
    ): Promise<void> {
        const cacheKey = rootId + '::' + pathId;
        if (this._loading.has(cacheKey)) return;
        this._loading.add(cacheKey);

        const fb = getFb();
        const gen = this._generation;
        try {
            // Bridge typing returns the strict `LibraryBrowseTreeResponse`
            // (with `TrackInfo[]` files), but `FsFile` is a wider runtime
            // shape that includes the `subsong` / `filename` fields the
            // host exposes. Double-cast preserves bridge types.
            const result = (await fb.library.browseTree({
                rootId,
                pathId: pathId || undefined,
                includeFiles: this._includeFiles,
                recursiveFiles: false,
            })) as unknown as FsBrowseResult | null;

            // Generation guard: drop stale results from a previous
            // root-id / include-files generation or a detached host.
            if (gen !== this._generation || !this.isConnected) return;
            if (!result || !result.success) {
                return;
            }

            this._nodeCache.set(cacheKey, {
                directories: result.directories || [],
                files: result.files || [],
            });

            if (container && container.isConnected) {
                const frag = document.createDocumentFragment();
                // Depth heuristic: empty pathId means we're at depth 1
                // (children of a root). Otherwise count `/` segments.
                const depth = pathId ? pathId.split('/').length + 1 : 1;
                this._appendDirectories(
                    frag,
                    result.directories || [],
                    rootId,
                    depth,
                );
                if (this._includeFiles && result.files) {
                    this._appendFiles(frag, result.files, rootId, pathId);
                }
                container.appendChild(frag);
            } else if (!container) {
                this._rebuildTree();
            }
        } catch {
            /* silent — leave the container empty */
        } finally {
            this._loading.delete(cacheKey);
        }
    }

    private _emitSelect(nodeEl: HTMLElement): void {
        const detail: FbLibraryFsSelectDetail = {
            type: nodeEl.dataset.nodeType || '',
            rootId: nodeEl.dataset.rootId || '',
            pathId: nodeEl.dataset.pathId || '',
            absolutePath: nodeEl.dataset.absolutePath || '',
        };
        if (nodeEl.dataset.nodeType === 'file' && nodeEl.dataset.fileInfo) {
            try {
                detail.file = JSON.parse(nodeEl.dataset.fileInfo);
            } catch {
                /* drop malformed payload */
            }
        }
        this._emit<FbLibraryFsSelectDetail>('fb-library-fs-select', detail);
    }

    private _emitPlay(nodeEl: HTMLElement): void {
        const detail: FbLibraryFsPlayDetail = {
            type: nodeEl.dataset.nodeType || '',
            rootId: nodeEl.dataset.rootId || '',
            pathId: nodeEl.dataset.pathId || '',
            absolutePath: nodeEl.dataset.absolutePath || '',
        };
        if (nodeEl.dataset.nodeType === 'file' && nodeEl.dataset.fileInfo) {
            try {
                detail.file = JSON.parse(nodeEl.dataset.fileInfo);
            } catch {
                /* drop malformed payload */
            }
        }
        this._emit<FbLibraryFsPlayDetail>('fb-library-fs-play', detail);
    }

    private _selectNode(
        node: HTMLElement,
        ctrlKey = false,
        shiftKey = false,
    ): void {
        const sel = (n: HTMLElement): void => {
            n.setAttribute('selected', '');
            const part = n.getAttribute('part') || '';
            if (!part.includes(' selected')) {
                n.setAttribute('part', `${part} selected`);
            }
        };
        const desel = (n: HTMLElement): void => {
            n.removeAttribute('selected');
            const part = n.getAttribute('part') || '';
            n.setAttribute('part', part.replace(' selected', ''));
        };
        if (ctrlKey) {
            if (node.hasAttribute('selected')) desel(node);
            else sel(node);
        } else if (shiftKey && this._lastSelected) {
            const allNodes = Array.from(
                this._container.querySelectorAll<HTMLElement>(
                    '[data-node-type=root],[data-node-type=directory],[data-node-type=file]',
                ),
            );
            const from = allNodes.indexOf(this._lastSelected);
            const to = allNodes.indexOf(node);
            if (from >= 0 && to >= 0) {
                this._container
                    .querySelectorAll<HTMLElement>('[selected]')
                    .forEach((n) => desel(n));
                const [start, end] = from < to ? [from, to] : [to, from];
                for (let i = start; i <= end; i++) sel(allNodes[i]);
            }
        } else {
            this._container
                .querySelectorAll<HTMLElement>('[selected]')
                .forEach((n) => desel(n));
            sel(node);
        }
        this._lastSelected = node;
    }
}
