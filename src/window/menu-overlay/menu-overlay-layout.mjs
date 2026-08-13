// Pure layout / DOM-model helpers for the self-drawn menu overlay.
// Node tests import this module directly. The overlay page inlines a stripped
// browser build via scripts/gen_menu_overlay_page.mjs — keep both paths in sync.

export const ZONE_IDS = Object.freeze(['top', 'playback', 'bottom']);

/** Resolve public data-kind for an item payload. */
export function resolveItemKind(it) {
  if (!it || typeof it !== 'object') return 'normal';
  const t = it.type;
  if (t === 'separator') return 'separator';
  if (t === 'nowplaying') return 'nowplaying';
  if (t === 'rating') return 'rating';
  if (t === 'slider') return 'slider';
  if (t === 'segmented') return 'segmented';
  if (t === 'submenu' || (it.submenu && it.submenu.length)) return 'submenu';
  return 'normal';
}

/**
 * Stable public CSS hooks for an item node. Does not invent public ids.
 * `data-item-token` is internal (single-show opaque identity) — not a public contract.
 */
export function buildItemStableAttrs(it, { depth = 0, zone = undefined } = {}) {
  const kind = resolveItemKind(it);
  const attrs = {
    'data-kind': kind,
    'data-depth': String(depth),
  };
  if (zone != null && zone !== '') attrs['data-zone'] = String(zone);
  if (it && typeof it.id === 'string' && it.id.length > 0) {
    attrs['data-item-id'] = it.id;
  }
  if (it && typeof it._token === 'string' && it._token.length > 0) {
    attrs['data-item-token'] = it._token;
  }
  return attrs;
}

/** Separator attrs (role=separator + zone/depth). */
export function buildSeparatorStableAttrs({ depth = 0, zone = undefined, zoneSeparator = false } = {}) {
  const attrs = {
    role: 'separator',
    'data-kind': 'separator',
    'data-depth': String(depth),
  };
  if (zone != null && zone !== '') attrs['data-zone'] = String(zone);
  if (zoneSeparator) attrs['data-zone-separator'] = '1';
  return attrs;
}

/**
 * Hide protocol: class first, then inline display:none !important.
 * Visible state must NOT write inline display.
 */
export function hideMenuProtocol() {
  return Object.freeze([
    { op: 'addClass', className: 'fb-hidden' },
    { op: 'setProperty', name: 'display', value: 'none', priority: 'important' },
  ]);
}

/** Show protocol: remove inline display first, then clear .fb-hidden. */
export function showMenuProtocol() {
  return Object.freeze([
    { op: 'removeProperty', name: 'display' },
    { op: 'removeClass', className: 'fb-hidden' },
  ]);
}

/**
 * Build a virtual DOM tree for flat / zones / legacy menu.show models.
 * Nodes: { tag, id?, className, attrs, children, text? }
 */
export function buildMenuDocumentModel({
  layoutMode = 'flat',
  model = 'legacyItems',
  items = [],
  zones = [],
  depth = 0,
} = {}) {
  const menuAttrs = { role: 'menu', 'data-depth': String(depth) };
  const menu = {
    tag: 'div',
    id: 'menu',
    className: 'fb-menu fb-hidden',
    attrs: menuAttrs,
    children: [],
  };

  const useZones = model === 'trayZones' && layoutMode === 'zones';

  if (useZones) {
    const present = (zones || []).filter((z) => z && Array.isArray(z.items) && z.items.length > 0);
    for (let i = 0; i < present.length; i++) {
      if (i > 0) {
        const prevZone = present[i - 1].id || ZONE_IDS[i - 1] || 'top';
        menu.children.push({
          tag: 'div',
          className: 'fb-sep fb-zone-separator',
          attrs: buildSeparatorStableAttrs({ depth, zone: prevZone, zoneSeparator: true }),
          children: [],
        });
      }
      const z = present[i];
      const zoneId = z.id || ZONE_IDS[i] || 'top';
      const zoneEl = {
        tag: 'div',
        className: 'fb-zone',
        attrs: { 'data-zone': zoneId },
        children: buildItemNodes(z.items, { depth, zone: zoneId }),
      };
      menu.children.push(zoneEl);
    }
  } else {
    menu.children = buildItemNodes(items || [], { depth, zoneFromItem: true });
  }

  return {
    tag: 'div',
    id: 'viewport',
    className: '',
    attrs: {},
    children: [menu],
  };
}

function buildItemNodes(items, { depth, zone, zoneFromItem = false }) {
  const out = [];
  for (const it of items || []) {
    if (!it) continue;
    const itemZone = zoneFromItem ? (it._zone || zone) : zone;
    if (it.type === 'separator') {
      out.push({
        tag: 'div',
        className: 'fb-sep',
        attrs: buildSeparatorStableAttrs({ depth, zone: itemZone }),
        children: [],
      });
      continue;
    }
    const kind = resolveItemKind(it);
    let className = 'fb-item';
    if (kind === 'nowplaying') className += ' fb-np';
    else if (kind === 'rating') className += ' fb-rating';
    else if (kind === 'slider') className += ' fb-slider';
    else if (kind === 'segmented') className += ' fb-seg';
    else className += ' nrm';
    const node = {
      tag: 'div',
      className,
      attrs: buildItemStableAttrs(it, { depth, zone: itemZone }),
      children: [],
    };
    out.push(node);
  }
  return out;
}

/** Direct-child `.fb-item` nodes under #menu (flat / legacy contract). */
export function menuDirectItems(doc) {
  const menu = findById(doc, 'menu');
  if (!menu) return [];
  return (menu.children || []).filter((c) => hasClass(c, 'fb-item'));
}

/** Direct-child separators under #menu (excludes zone separators). */
export function menuDirectSeparators(doc) {
  const menu = findById(doc, 'menu');
  if (!menu) return [];
  return (menu.children || []).filter((c) => hasClass(c, 'fb-sep') && !hasClass(c, 'fb-zone-separator'));
}

export function menuZones(doc) {
  const menu = findById(doc, 'menu');
  if (!menu) return [];
  return (menu.children || []).filter((c) => hasClass(c, 'fb-zone'));
}

function hasClass(node, name) {
  const parts = String(node && node.className || '').split(/\s+/).filter(Boolean);
  return parts.includes(name);
}

export function findById(node, id) {
  if (!node) return null;
  if (node.id === id) return node;
  for (const c of node.children || []) {
    const hit = findById(c, id);
    if (hit) return hit;
  }
  return null;
}

/** Source-file contract checks for protected CSS. */
export function analyzeProtectedCss(css) {
  const text = String(css || '');
  return {
    hasViewportRule: /#viewport\s*\{/.test(text),
    forcesVisibleBlock: /#menu:not\(\.fb-hidden\)\s*\{[^}]*display\s*:\s*block\s*!important/.test(text),
    hasHiddenFallback: /\.fb-hidden[^}]*display\s*:\s*none\s*!important|#menu\.fb-hidden[^}]*display\s*:\s*none\s*!important/.test(text),
    hasMenuBoxModel: /\.fb-menu\s*\{[^}]*box-sizing\s*:\s*border-box\s*!important/.test(text),
  };
}

/** Default visual CSS must not lock .fb-menu to display:none. */
export function analyzeDefaultCss(css) {
  const text = String(css || '');
  return {
    locksMenuDisplayNone: /\.fb-menu\s*\{[^}]*display\s*:\s*none/.test(text),
  };
}

/** Default rich-control CSS must provide usable geometry without user CSS. */
export function analyzeRichControlCss(css) {
  const text = String(css || '');
  const verticalRow = text.match(/\.fb-slider\[data-orientation=["']vertical["']\]\s*\{([^}]*)\}/);
  const verticalTrack = text.match(/\.fb-slider\[data-orientation=["']vertical["']\]\s+\.fb-slider-track\s*\{([^}]*)\}/);
  const verticalFill = text.match(/\.fb-slider\[data-orientation=["']vertical["']\]\s+\.fb-slider-fill\s*\{([^}]*)\}/);
  const verticalThumb = text.match(/\.fb-slider\[data-orientation=["']vertical["']\]\s+\.fb-slider-thumb\s*\{([^}]*)\}/);
  const segmentedRow = text.match(/\.fb-seg\s*\{([^}]*)\}/);
  const segmentedGroup = text.match(/\.fb-seg-group\s*\{([^}]*)\}/);
  const segmentedButton = text.match(/\.fb-seg-btn\s*\{([^}]*)\}/);
  return {
    verticalUsesGrid: !!verticalRow && /display\s*:\s*grid/.test(verticalRow[1]),
    verticalHasTallTrack: !!verticalTrack
      && /min-height\s*:\s*(?:[6-9]\d|\d{3,})px/.test(verticalTrack[1])
      && /min-width\s*:\s*(?:1[2-9]|[2-9]\d|\d{3,})px/.test(verticalTrack[1]),
    verticalVisualTrackIsNarrow: !!verticalTrack
      && /--fb-slider-visual-width\s*:\s*(?:[2-8](?:\.\d+)?)px/.test(verticalTrack[1]),
    verticalFillUsesVisualWidth: !!verticalFill
      && /width\s*:\s*var\(--fb-slider-visual-width\)/.test(verticalFill[1])
      && /left\s*:\s*50%/.test(verticalFill[1])
      && /translateX\(-50%\)/.test(verticalFill[1]),
    verticalThumbCentered: !!verticalThumb
      && /left\s*:\s*50%/.test(verticalThumb[1])
      && /translateX\(-50%\)/.test(verticalThumb[1]),
    segmentedCanWrap: !!segmentedGroup && /flex-wrap\s*:\s*wrap/.test(segmentedGroup[1]),
    segmentedCanShrink: !!segmentedRow && /min-width\s*:\s*0/.test(segmentedRow[1])
      && !!segmentedGroup && /min-width\s*:\s*0/.test(segmentedGroup[1]),
    segmentedButtonsBoundText: !!segmentedButton
      && /max-width\s*:/.test(segmentedButton[1])
      && /text-overflow\s*:\s*ellipsis/.test(segmentedButton[1]),
  };
}

/**
 * Convert physical placement geometry from C++ to CSS-pixel constraints.
 * Missing geometry keeps the legacy root-at-origin fallback.
 */
export function normalizePlacedGeometry(ev, dpr = 1) {
  const scale = Number.isFinite(+dpr) && +dpr > 0 ? +dpr : 1;
  const g = ev && ev.geometry;
  if (!g || typeof g !== 'object') return null;
  const read = (key) => Number.isFinite(+g[key]) && +g[key] >= 0 ? +g[key] / scale : null;
  const viewportW = read('viewportW');
  const viewportH = read('viewportH');
  const rootSlotW = read('rootSlotW');
  const subSlotW = read('subSlotW');
  const rootTop = read('rootTop');
  if (viewportW == null || viewportH == null || rootSlotW == null
      || subSlotW == null || rootTop == null) return null;
  const virtualViewportH = read('virtualViewportH');
  const virtualRootTop = read('virtualRootTop');
  return {
    viewportW,
    viewportH,
    rootSlotW,
    subSlotW,
    rootTop,
    virtualViewportH: virtualViewportH == null ? viewportH : virtualViewportH,
    virtualRootTop: virtualRootTop == null ? rootTop : virtualRootTop,
  };
}

/**
 * Place an independently hosted first-level submenu in the original virtual
 * content-sized canvas. The root HWND is tight, so its screen origin is
 * virtualRootTop pixels below that canvas origin.
 */
export function submenuWindowPlacement({ rootSlotW, viewportH, virtualViewportH, virtualRootTop, parentTop, submenuHeight }) {
  const canvasH = Math.max(0, Number(virtualViewportH ?? viewportH) || 0);
  const height = Math.max(0, Math.min(Math.max(0, Number(submenuHeight) || 0), canvasH));
  const desiredTop = Math.max(0, Number(virtualRootTop) || 0) + Math.max(0, Number(parentTop) || 0);
  const top = Math.max(0, Math.min(desiredTop, Math.max(0, canvasH - height)));
  return {
    x: Math.max(0, Number(rootSlotW) || 0),
    y: top,
    h: height,
  };
}

/**
 * ArrowLeft navigation policy. An independently hosted submenu begins at
 * depth zero, unlike an in-DOM submenu layer, so it must explicitly return to
 * its root surface instead of becoming a no-op at depth zero.
 */
export function resolveMenuArrowLeftAction({ windowModel, depth } = {}) {
  if (windowModel === 'submenu') return { kind: 'dismissSubmenu', reason: 'submenu-left' };
  return Number.isFinite(+depth) && +depth > 0
    ? { kind: 'closeLayer' }
    : { kind: 'noop' };
}

/** Source-level contract for the independent first-level submenu surface. */
export function analyzeIndependentSubmenuWindowContract(source) {
  const text = String(source || '');
  const arrowLeftMatch = text.match(/case "ArrowLeft":\s*\{([\s\S]*?)case "Enter"/);
  const arrowLeftBody = arrowLeftMatch ? arrowLeftMatch[1] : '';
  const escapeMatch = text.match(/case "Escape":\s*([\s\S]*?)default:/);
  const escapeBody = escapeMatch ? escapeMatch[1] : '';
  return {
    reportsMonotonicSequence: /submenuPanelSequence\s*=\s*0/.test(text)
      && /sequence\s*:\s*\+\+submenuPanelSequence/.test(text),
    reportsParentToken: /parentToken\s*:\s*\(L\.rows\[rowIdx\]\.item&&L\.rows\[rowIdx\]\.item\._token\)/.test(text),
    computesVirtualWindowPlacement: /submenuWindowPlacement\(\{[\s\S]*?virtualRootTop:geom\.virtualRootTop/.test(text),
    treatsSubmenuAsLeafSurface: /cur\.windowModel==="submenu"\) return/.test(text),
    reconcilesNativeSubmenuClose: /fb2k\.on\("menu:__submenuClosed"/.test(text),
    waitsForNativeVisibilityBeforeFocus: /fb2k\.on\("menu:__submenuVisible"[\s\S]*?setActive\(0,firstNav\(0\),\{focus:true\}\)/.test(text),
    announcesSurfaceReadiness: /fb2k\.invoke\("menu\.__submenuPanel"[\s\S]*?ready:true/.test(text),
    submenuArrowLeftReturnsToRoot: /resolveMenuArrowLeftAction\(\{windowModel:cur&&cur\.windowModel,depth:depth\}\)/.test(arrowLeftBody)
      && /leftAction\.kind==="dismissSubmenu"[\s\S]*?dismiss\(leftAction\.reason\)[\s\S]*?e\.preventDefault\(\)/.test(arrowLeftBody),
    submenuEscapeRemainsWholeSurfaceDismiss: /else\s*\{\s*dismiss\("escape"\);\s*\}/.test(escapeBody),
  };
}

/**
 * Source structure contract for root close: the child HWND must be hidden and
 * invalidated before the root chooses its animated or immediate branch, and
 * stale child callbacks must be blocked while the root is closing.
 */
export function analyzeRootCloseSubmenuPolicy(source) {
  const text = String(source || '');
  const hideMatch = text.match(/void MenuOverlayHost::Hide\(const std::string& reason\)\s*\{([\s\S]*?)\n\}/);
  const hideBody = hideMatch ? hideMatch[1] : '';
  const policyIdx = hideBody.indexOf('ResolveRootClosePolicy(');
  const hideChildIdx = hideBody.indexOf('HideSubmenuWindow(closePolicy.restoreRootFocus');
  const animatedBranchIdx = hideBody.indexOf('if (closePolicy.animateRoot)');
  return {
    resolvesRootClosePolicy: policyIdx >= 0,
    hidesChildBeforeRootCloseBranch: hideChildIdx >= 0 && animatedBranchIdx > hideChildIdx,
    childCloseAvoidsFocusOrRootNotification: /HideSubmenuWindow\(closePolicy\.restoreRootFocus, closePolicy\.notifyRootRenderer,\s*closePolicy\.clearSubmenuStateImmediately\)/.test(hideBody),
    rejectsPanelUpdatesWhileClosing: /bool MenuOverlayHost::OnSubmenuPanelChanged[\s\S]*?if \(!visible_ \|\| closing_\)/.test(text),
    rejectsReadyWhileClosing: /bool MenuOverlayHost::OnSubmenuSurfaceReady[\s\S]*?!visible_ \|\| closing_/.test(text),
  };
}

/** Default style for the reserved submenu HWND slot: non-visible, non-hit-test. */
export function hiddenSubmenuSlotStyle(rootSlotW, viewportH) {
  return {
    left: `${Math.max(0, rootSlotW)}px`,
    top: '0px',
    width: '0px',
    height: `${Math.max(0, viewportH)}px`,
    pointerEvents: 'none',
  };
}

/** Visible submenu panel constraints inside the reserved slot. */
export function visibleSubmenuStyle({ rootSlotW, subSlotW, viewportH, parentTop, submenuHeight }) {
  const maxTop = Math.max(0, viewportH - Math.min(Math.max(0, submenuHeight), viewportH));
  const top = Math.max(0, Math.min(Math.max(0, parentTop), maxTop));
  return {
    left: `${Math.max(0, rootSlotW)}px`,
    top: `${top}px`,
    maxWidth: `${Math.max(0, subSlotW)}px`,
    maxHeight: `${Math.max(0, viewportH)}px`,
  };
}

/** Inline-important geometry owned by the ContentSized host. */
export function contentSizedGeometryPlan({ left, top, maxWidth, maxHeight }) {
  return Object.freeze([
    { op: 'setProperty', name: 'left', value: String(left), priority: 'important' },
    { op: 'setProperty', name: 'top', value: String(top), priority: 'important' },
    { op: 'setProperty', name: 'max-width', value: String(maxWidth), priority: 'important' },
    { op: 'setProperty', name: 'max-height', value: String(maxHeight), priority: 'important' },
    { op: 'setProperty', name: 'min-width', value: '0px', priority: 'important' },
    { op: 'setProperty', name: 'overflow', value: 'auto', priority: 'important' },
  ]);
}

/** Remove every ContentSized-owned property without touching display/theme layout. */
export function clearContentSizedGeometryPlan() {
  return Object.freeze([
    { op: 'removeProperty', name: 'left' },
    { op: 'removeProperty', name: 'top' },
    { op: 'removeProperty', name: 'max-width' },
    { op: 'removeProperty', name: 'max-height' },
    { op: 'removeProperty', name: 'min-width' },
    { op: 'removeProperty', name: 'overflow' },
  ]);
}

/** Source contract: apply/clear helpers must own geometry, never visible display. */
export function analyzeContentSizedGeometryContract(source) {
  const text = String(source || '');
  const applyMatch = text.match(/function applyContentSizedGeometry\s*\([^)]*\)\s*\{([\s\S]*?)\n\s*\}/);
  const clearMatch = text.match(/function clearContentSizedGeometry\s*\([^)]*\)\s*\{([\s\S]*?)\n\s*\}/);
  const applyBody = applyMatch ? applyMatch[1] : '';
  const clearBody = clearMatch ? clearMatch[1] : '';
  const required = ['left', 'top', 'max-width', 'max-height', 'min-width', 'overflow'];
  return {
    hasApplyHelper: !!applyMatch,
    hasClearHelper: !!clearMatch,
    appliesImportantGeometry: required.every((name) =>
      new RegExp(`setProperty\\(\\s*["']${name}["'][^\\n]*["']important["']`).test(applyBody)),
    clearsGeometry: required.every((name) =>
      new RegExp(`removeProperty\\(\\s*["']${name}["']\\s*\\)`).test(clearBody)),
    forcesVisibleDisplay: /setProperty\(\s*["']display["']/.test(applyBody),
    renderClearsBeforeModeBranch: /function render\(st\)\{[\s\S]*?clearContentSizedGeometry\(root\)[\s\S]*?var content\s*=/.test(text),
    rootUsesHelper: /function applyPlacedGeometry\(ev\)[\s\S]*?applyContentSizedGeometry\(root\s*,/.test(text),
    submenuUsesHelper: /function openSub\(parentDepth, rowIdx\)[\s\S]*?if\(content\)[\s\S]*?applyContentSizedGeometry\(sub\s*,/.test(text),
  };
}

/** HTML shell must wrap #menu in #viewport. */
export function analyzeHtmlShell(html) {
  const text = String(html || '');
  const hasViewport = /id\s*=\s*["']viewport["']/.test(text);
  const menuInsideViewport = /id\s*=\s*["']viewport["'][\s\S]*id\s*=\s*["']menu["']/.test(text);
  return { hasViewport, menuInsideViewport };
}

// ── Slider orientation / range / paint / pointer / keyboard ─────────────────

/**
 * Resolve slider orientation. Only exact "vertical" is vertical; missing /
 * "horizontal" / unknown → "horizontal". Non-slider kinds still return
 * horizontal so callers can safely stamp data-orientation only on sliders.
 */
export function resolveSliderOrientation(it) {
  if (it && it.orientation === 'vertical') return 'vertical';
  return 'horizontal';
}

/**
 * Normalize slider range: swap max<min; clamp value; detect constant.
 * Returns { min, max, value, constant, orientation }.
 */
export function normalizeSliderRange(it = {}) {
  let min = Number.isFinite(+it.min) ? Math.round(+it.min) : 0;
  let max = Number.isFinite(+it.max) ? Math.round(+it.max) : 100;
  if (max < min) {
    const t = min; min = max; max = t;
  }
  let value = Number.isFinite(+it.value) ? Math.round(+it.value) : min;
  if (value < min) value = min;
  if (value > max) value = max;
  return {
    min,
    max,
    value,
    constant: max === min,
    orientation: resolveSliderOrientation(it),
  };
}

/**
 * Map a pointer client coordinate to a slider value.
 * Horizontal: min left, max right (clientX / width).
 * Vertical: min bottom, max top (clientY / height).
 */
export function sliderValueFromPointer({ orientation, min, max, clientX, clientY, rect }) {
  if (!rect || max === min) return min;
  let p;
  if (orientation === 'vertical') {
    const h = rect.height || 1;
    // bottom = min, top = max
    p = (rect.bottom - clientY) / h;
  } else {
    const w = rect.width || 1;
    p = (clientX - rect.left) / w;
  }
  if (!Number.isFinite(p)) p = 0;
  if (p < 0) p = 0;
  if (p > 1) p = 1;
  return Math.round(min + p * (max - min));
}

/**
 * Paint properties for a slider fill/thumb.
 * Horizontal: fill width %, thumb left %.
 * Vertical: fill height %, thumb bottom %.
 */
export function sliderPaintStyles({ orientation, min, max, value }) {
  const span = max - min;
  let p = span === 0 ? 1 : (value - min) / span;
  if (!Number.isFinite(p)) p = 0;
  if (p < 0) p = 0;
  if (p > 1) p = 1;
  const pct = (p * 100) + '%';
  if (orientation === 'vertical') {
    return {
      fill: { width: '', height: pct, left: '', bottom: '0', top: 'auto' },
      thumb: { left: '50%', bottom: pct, top: 'auto' },
    };
  }
  return {
    fill: { width: pct, height: '100%', left: '0', top: '0', bottom: 'auto' },
    thumb: { left: pct, top: '50%', bottom: 'auto' },
  };
}

/** Build the physical-pixel menu.__ready payload from stable CSS measurements. */
export function buildMeasurePayload({ menuId = '', root, submenus = [], dpr = 1 } = {}) {
  const scale = Number.isFinite(+dpr) && +dpr > 0 ? +dpr : 1;
  const edge = (v) => Math.ceil(Math.max(0, Number.isFinite(+v) ? +v : 0) * scale);
  const rw = edge(root && root.w);
  const rh = edge(root && root.h);
  let maxW = 0;
  let maxH = 0;
  for (const sub of submenus || []) {
    maxW = Math.max(maxW, edge(sub && sub.w));
    maxH = Math.max(maxH, edge(sub && sub.h));
  }
  return {
    menuId: String(menuId || ''),
    root: { w: rw, h: rh },
    submenu: { maxW, maxH },
  };
}

/** Exact equality used by the two-consecutive-frame measurement stabilizer. */
export function sameMeasurePayload(a, b) {
  return !!a && !!b
    && a.root && b.root && a.submenu && b.submenu
    && a.root.w === b.root.w && a.root.h === b.root.h
    && a.submenu.maxW === b.submenu.maxW && a.submenu.maxH === b.submenu.maxH;
}

/**
 * Keyboard delta for slider keys. Positive = increase.
 * Up/Right increase; Down/Left decrease; Home → min; End → max.
 * Returns { kind:'delta', delta } | { kind:'edge', edge:'min'|'max' } | null.
 */
export function sliderKeyAction(key, orientation) {
  void orientation; // both orientations share the same key semantics
  switch (key) {
    case 'ArrowUp':
    case 'ArrowRight':
      return { kind: 'delta', delta: 1 };
    case 'ArrowDown':
    case 'ArrowLeft':
      return { kind: 'delta', delta: -1 };
    case 'Home':
      return { kind: 'edge', edge: 'min' };
    case 'End':
      return { kind: 'edge', edge: 'max' };
    default:
      return null;
  }
}

/** Apply a slider key action to a current value. Constant → unchanged. */
export function applySliderKey({ key, orientation, min, max, value, step = 1 }) {
  if (max === min) return min;
  const action = sliderKeyAction(key, orientation);
  if (!action) return value;
  let next = value;
  if (action.kind === 'edge') next = action.edge === 'min' ? min : max;
  else next = value + action.delta * step;
  if (next < min) next = min;
  if (next > max) next = max;
  return next;
}

/**
 * Whether a value change should be emitted for a slider.
 * Constant sliders never emit; NaN / out-of-range rejected.
 */
export function shouldEmitSliderValue({ min, max, value, previous }) {
  if (max === min) return false;
  if (!Number.isFinite(value)) return false;
  if (value < min || value > max) return false;
  if (value === previous) return false;
  return true;
}

/** Throttle gate for pointermove (50ms). force always passes. */
export function shouldThrottleEmit(now, lastSent, force, throttleMs = 50) {
  if (force) return true;
  return (now - lastSent) >= throttleMs;
}

// ── ARIA / focus helpers ────────────────────────────────────────────────────

/**
 * Row role for navigation mode.
 * checkable → menuitemcheckbox; rich → menuitem; separator → separator; else menuitem.
 */
export function resolveNavRowRole({ kind, checkable, checked }) {
  if (kind === 'separator') return 'separator';
  if (checkable || kind === 'checkbox') return 'menuitemcheckbox';
  void checked;
  return 'menuitem';
}

/**
 * Build aria-label for a rich control row in navigation mode.
 * `locale` selects the text language: 'zh' for Chinese, anything else English.
 * It arrives via the overlay state JSON, where the host derives it from the
 * Windows user UI language, so it matches the rest of the native chrome instead
 * of the WebView2 browser language.
 * The literal key name "Enter" stays untranslated in both languages because it
 * names a physical key, not a word.
 */
export function buildRichNavAriaLabel({ kind, label, value, min, max, locale }) {
  const zh = locale === 'zh';
  const name = label || kind || (zh ? '控件' : 'control');
  const hint = zh ? '按 Enter 键调整' : 'Press Enter to adjust';
  if (kind === 'slider') {
    return zh
      ? `${name}，${value}，范围 ${min} 到 ${max}。${hint}`
      : `${name}, ${value}, range ${min} to ${max}. ${hint}`;
  }
  if (kind === 'rating') {
    return zh
      ? `${name}，5 星中的 ${value} 星。${hint}`
      : `${name}, ${value} of 5 stars. ${hint}`;
  }
  if (kind === 'segmented') {
    return zh
      ? `${name}，选项 ${value}。${hint}`
      : `${name}, option ${value}. ${hint}`;
  }
  return zh ? `${name}。${hint}` : `${name}. ${hint}`;
}

/** Detect explicit checkable identity (field present, including false). */
export function isExplicitCheckable(it) {
  if (!it || typeof it !== 'object') return false;
  if (it.type === 'checkbox') return true;
  if (Object.prototype.hasOwnProperty.call(it, 'checked')) return true;
  if (it.checkable === true) return true;
  return false;
}

/** Default CSS must honor prefers-reduced-motion for entrance/exit. */
export function analyzeReducedMotionCss(css) {
  const text = String(css || '');
  return {
    hasReducedMotionQuery: /@media\s*\(\s*prefers-reduced-motion\s*:\s*reduce\s*\)/.test(text),
    disablesTransformOrTransition: /prefers-reduced-motion\s*:\s*reduce[\s\S]{0,400}(transition\s*:\s*none|transform\s*:\s*none)/.test(text),
  };
}

// ── Pointer focus + segmented roving + hide cleanup ─────────────────────────

/**
 * setActive focus gate. Pointer hover may update active/roving, but must not
 * steal real keyboard/editor focus unless an explicit path opts in.
 * Default (undefined / true) focuses; only focus:false suppresses.
 */
export function shouldFocusOnActivate(opts) {
  const o = opts || {};
  return o.focus !== false;
}

/** Segment enabled? Missing/undefined enabled means enabled. */
export function isSegmentEnabled(seg) {
  return !!(seg && seg.enabled !== false);
}

/** Next enabled segment index from `from` stepping by `dir` (±1). Bounds clamp (no wrap). */
export function nextEnabledSegmentIndex(segments, from, dir) {
  const segs = segments || [];
  const n = segs.length;
  const step = dir >= 0 ? 1 : -1;
  for (let k = 1; k <= n; k++) {
    const i2 = from + step * k;
    if (i2 < 0 || i2 >= n) return -1;
    if (isSegmentEnabled(segs[i2])) return i2;
  }
  return -1;
}

/** First enabled segment index, or -1. */
export function firstEnabledSegmentIndex(segments) {
  const segs = segments || [];
  for (let i = 0; i < segs.length; i++) {
    if (isSegmentEnabled(segs[i])) return i;
  }
  return -1;
}

/**
 * Editor-mode segmented roving plan for a selected index.
 * Disabled options never receive tabindex 0 / focus.
 * @returns {{ focusIndex: number, tabIndexes: string[], ariaChecked: string[] }}
 */
export function planSegmentedEditorRoving({ segments, selectedIndex } = {}) {
  const segs = segments || [];
  const n = segs.length;
  const tabIndexes = Array(n).fill('-1');
  const ariaChecked = Array(n).fill('false');
  let focusIndex = -1;
  const sel = Number.isFinite(selectedIndex) ? selectedIndex : -1;
  if (sel >= 0 && sel < n && isSegmentEnabled(segs[sel])) {
    focusIndex = sel;
  } else {
    focusIndex = firstEnabledSegmentIndex(segs);
  }
  if (sel >= 0 && sel < n && isSegmentEnabled(segs[sel])) {
    ariaChecked[sel] = 'true';
  }
  if (focusIndex >= 0) tabIndexes[focusIndex] = '0';
  return { focusIndex, tabIndexes, ariaChecked };
}

/**
 * Left/Right plan in segmented editor: next enabled selection + focus/tabindex.
 * @returns {null | { nextIndex: number, focusIndex: number, tabIndexes: string[], ariaChecked: string[] }}
 */
export function planSegmentedEditorAdjust({ segments, current, delta } = {}) {
  const segs = segments || [];
  const cur = Number.isFinite(current) ? current : -1;
  const d = delta >= 0 ? 1 : -1;
  const base = cur < 0 ? (d > 0 ? -1 : segs.length) : cur;
  const nextIndex = nextEnabledSegmentIndex(segs, base, d);
  if (nextIndex < 0) return null;
  const plan = planSegmentedEditorRoving({ segments: segs, selectedIndex: nextIndex });
  return {
    nextIndex,
    focusIndex: plan.focusIndex,
    tabIndexes: plan.tabIndexes,
    ariaChecked: plan.ariaChecked,
  };
}

/**
 * Navigation-mode segmented internals: all radios tabindex -1 (subtree inert).
 * Does not invent checked state beyond selectedIndex.
 */
export function planSegmentedNavigationInternals({ segments, selectedIndex } = {}) {
  const segs = segments || [];
  const n = segs.length;
  const tabIndexes = Array(n).fill('-1');
  const ariaChecked = Array(n).fill('false');
  const sel = Number.isFinite(selectedIndex) ? selectedIndex : -1;
  if (sel >= 0 && sel < n && isSegmentEnabled(segs[sel])) ariaChecked[sel] = 'true';
  return { tabIndexes, ariaChecked, ariaHidden: true };
}

/**
 * Static contract: every mouseenter callback that calls setActive must use a
 * non-stealing path (setActiveFromPointer, or setActive(..., {focus:false})).
 * Rating star hover preview (paint only) is allowed without setActive.
 */
export function analyzeMouseenterSetActiveFocus(jsSource) {
  const text = String(jsSource || '');
  const re = /addEventListener\(\s*["']mouseenter["']\s*,\s*function\s*\([^)]*\)\s*\{/g;
  const handlers = [];
  let m;
  while ((m = re.exec(text))) {
    const start = m.index + m[0].length;
    let depth = 1;
    let i = start;
    for (; i < text.length && depth > 0; i++) {
      const ch = text[i];
      if (ch === '{') depth++;
      else if (ch === '}') depth--;
    }
    const body = text.slice(start, i - 1);
    const setActiveCalls = [];
    const callRe = /\b(setActiveFromPointer|setActive)\s*\(/g;
    let cm;
    while ((cm = callRe.exec(body))) {
      // Capture argument list with nested braces/parens for {focus:false}.
      let p = cm.index + cm[0].length;
      let pd = 1;
      let argsStart = p;
      for (; p < body.length && pd > 0; p++) {
        if (body[p] === '(') pd++;
        else if (body[p] === ')') pd--;
        else if (body[p] === '{') {
          // still inside args
        }
      }
      const args = body.slice(argsStart, p - 1);
      const name = cm[1];
      const nonFocusing =
        name === 'setActiveFromPointer' ||
        /focus\s*:\s*false/.test(args);
      setActiveCalls.push({ name, args, nonFocusing });
    }
    handlers.push({
      body,
      setActiveCalls,
      hasSetActive: setActiveCalls.length > 0,
      allNonFocusing: setActiveCalls.every((c) => c.nonFocusing),
    });
  }
  return {
    handlers,
    setActiveHandlerCount: handlers.filter((h) => h.hasSetActive).length,
    allSetActiveNonFocusing: handlers.filter((h) => h.hasSetActive).every((h) => h.allNonFocusing),
  };
}

// ── Hover intent: native MenuShowDelay for submenu layer changes ────────────

/**
 * Fallback hover delay in milliseconds, matching the Windows MenuShowDelay
 * default. Used when the host state carries no `menuShowDelayMs` key (older
 * host) or reports an unusable value.
 */
export const MENU_SHOW_DELAY_FALLBACK_MS = 400;

/** Hover delay in milliseconds from the overlay state; 0 stays instantaneous. */
export function resolveMenuShowDelayMs(state) {
  const raw = state ? state.menuShowDelayMs : undefined;
  if (typeof raw !== 'number' || !Number.isFinite(raw) || raw < 0) return MENU_SHOW_DELAY_FALLBACK_MS;
  return Math.round(raw);
}

/**
 * Decide what one pointer row entry does to the submenu layer stack. Row
 * highlight is always immediate; only the layer change is deferred, so
 * diagonal travel toward an open submenu no longer closes it.
 *
 * openParentRowIdx: parent row index of the currently expanded child layer,
 * or -1 when nothing is expanded.
 *
 * @returns {{ action: 'none'|'cancel'|'schedule', delayMs: number }}
 *   cancel   — pointer is back on the row owning the open submenu: keep it,
 *              drop any pending change (idempotent re-hover).
 *   none     — nothing is expanded and this row has no submenu.
 *   schedule — after delayMs close the expanded layer, then open this row's
 *              submenu when it has one.
 */
export function evaluateHoverIntent({ openParentRowIdx, hoverRowIdx, hoverHasSub, delayMs } = {}) {
  const index = (v) => (typeof v === 'number' && Number.isFinite(v) ? Math.trunc(v) : -1);
  const open = index(openParentRowIdx);
  const hover = index(hoverRowIdx);
  const wait = (typeof delayMs === 'number' && Number.isFinite(delayMs) && delayMs >= 0)
    ? Math.round(delayMs)
    : MENU_SHOW_DELAY_FALLBACK_MS;
  if (hover < 0) return { action: 'none', delayMs: 0 };
  if (open >= 0 && open === hover) return { action: 'cancel', delayMs: 0 };
  if (open < 0 && !hoverHasSub) return { action: 'none', delayMs: 0 };
  return { action: 'schedule', delayMs: wait };
}

/** Bodies of every `addEventListener("<type>", function(){...})` in a source. */
function collectHandlerBodies(text, type) {
  const re = new RegExp(`addEventListener\\(\\s*["']${type}["']\\s*,\\s*function\\s*\\([^)]*\\)\\s*\\{`, 'g');
  const out = [];
  let m;
  while ((m = re.exec(text))) {
    const start = m.index + m[0].length;
    let depth = 1;
    let i = start;
    for (; i < text.length && depth > 0; i++) {
      const ch = text[i];
      if (ch === '{') depth++;
      else if (ch === '}') depth--;
    }
    out.push(text.slice(start, i - 1));
  }
  return out;
}

/** Source slice between two top-level function declarations. */
function sliceBetween(text, fromMarker, toMarker) {
  const a = text.indexOf(fromMarker);
  if (a < 0) return '';
  const b = text.indexOf(toMarker, a + fromMarker.length);
  return b > a ? text.slice(a, b) : text.slice(a);
}

/**
 * Source contract for hover intent: pointer highlight stays immediate while
 * every layer change funnels through one generation-guarded pending timer.
 * Click / keyboard paths must keep changing layers immediately.
 */
export function analyzeHoverIntentWiring(jsSource) {
  const text = String(jsSource || '');
  const rowHovers = collectHandlerBodies(text, 'mouseenter')
    .filter((b) => /setActiveFromPointer\s*\(/.test(b));
  // Per row-builder verification: one delegating hover + its own leave cancel.
  const siteBounds = [
    ['nowplaying', 'function buildNowPlaying(', 'function buildRating('],
    ['rating', 'function buildRating(', 'function buildSlider('],
    ['slider', 'function buildSlider(', 'function buildSegmented('],
    ['segmented', 'function buildSegmented(', 'function appendItems('],
    ['normal', 'function appendItems(', 'function itemsHaveIcon('],
  ];
  const hoverIntentSites = {};
  for (const [name, from, to] of siteBounds) {
    const region = sliceBetween(text, from, to);
    const hovers = collectHandlerBodies(region, 'mouseenter')
      .filter((b) => /setActiveFromPointer\s*\(/.test(b));
    hoverIntentSites[name] = hovers.length === 1
      && /applyHoverIntent\s*\(/.test(hovers[0])
      && !/closeLayersFrom\s*\(/.test(hovers[0])
      && !/openSub\s*\(/.test(hovers[0])
      && collectHandlerBodies(region, 'mouseleave').some((b) => /cancelHoverIntentFor\s*\(/.test(b));
  }
  const rowLeaves = collectHandlerBodies(text, 'mouseleave')
    .filter((b) => /cancelHoverIntentFor\s*\(/.test(b));
  const openSubBody = sliceBetween(text, 'function openSub(', 'function placeRoot(');
  const appendItemsBody = sliceBetween(text, 'function appendItems(', 'function itemsHaveIcon(');
  const clickBodies = collectHandlerBodies(appendItemsBody, 'click');
  const onKeyBody = sliceBetween(text, 'function onKey(e){', 'function pull()');
  const cleanupMatch = text.match(/function cleanupMenuInteraction\(\)\{([\s\S]*?)\n\s*\}/);
  const cleanupBody = cleanupMatch ? cleanupMatch[1] : '';
  const timerMatch = text.match(/hoverIntentTimer\s*=\s*setTimeout\(function\(\)\{([\s\S]*?)\n\s*\}\s*,\s*plan\.delayMs\)/);
  const timerBody = timerMatch ? timerMatch[1] : '';
  return {
    hoverIntentSites,
    rowHoverHandlerCount: rowHovers.length,
    allRowHoversDeferLayerChange: rowHovers.length > 0
      && rowHovers.every((b) => /applyHoverIntent\s*\(/.test(b)),
    noRowHoverChangesLayersInline: rowHovers.every((b) =>
      !/closeLayersFrom\s*\(/.test(b) && !/openSub\s*\(/.test(b)),
    highlightBeforeIntent: rowHovers.every((b) =>
      b.indexOf('setActiveFromPointer(') >= 0
      && b.indexOf('applyHoverIntent(') > b.indexOf('setActiveFromPointer(')),
    rowLeaveCancelCount: rowLeaves.length,
    submenuPanelEnterCancels: /sub\.addEventListener\("mouseenter",\s*function\(\)\{\s*cancelHoverIntent\(\)/.test(openSubBody),
    singlePendingTimer: (text.match(/setTimeout\(/g) || []).length === 1
      && (text.match(/hoverIntentTimer\s*=\s*setTimeout\(/g) || []).length === 1,
    guardsGeneration: /var token\s*=\s*hoverIntentToken\(\)/.test(text)
      && /token!==hoverIntentToken\(\)\)\s*return/.test(timerBody)
      && /function hoverIntentToken\(\)\{[^}]*menuId[^}]*hoverIntentGeneration/.test(text),
    invalidatesGenerationOnCleanup: /invalidateHoverIntent\(\)/.test(cleanupBody)
      && /function invalidateHoverIntent\(\)\{[^}]*cancelHoverIntent\(\);\s*hoverIntentGeneration\+\+/.test(text),
    clickPathStaysImmediate: clickBodies.some((b) =>
      /closeLayersFrom\(depth\+1\);\s*openSub\(depth, ridx\)/.test(b))
      && clickBodies.every((b) => !/applyHoverIntent\s*\(/.test(b)),
    clickCancelsPendingIntent: clickBodies.some((b) => /cancelHoverIntent\(\)/.test(b)),
    keyboardPathStaysImmediate: onKeyBody.length > 0 && !/applyHoverIntent\s*\(/.test(onKeyBody),
    keyboardCancelsPendingIntent: /cancelHoverIntent\(\)/.test(onKeyBody),
    readsHostDelayWithFallback: /function hoverIntentDelayMs\(\)\{\s*return resolveMenuShowDelayMs\(cur\)/.test(text)
      && /delayMs:hoverIntentDelayMs\(\)/.test(text),
  };
}

/**
 * Static contract: hide / select / dismiss / render must clear editor interaction state.
 */
export function analyzeMenuInteractionCleanup(jsSource) {
  const text = String(jsSource || '');
  const hasCleanupHelper = /function\s+cleanupMenuInteraction\s*\(/.test(text);
  // menu:__hide must invoke cleanup (helper or explicit exitEditor + pendingRootFocus clear).
  const hideMatch = text.match(/fb2k\.on\(\s*["']menu:__hide["']\s*,\s*function\s*\([^)]*\)\s*\{([\s\S]*?)\}\s*\)/);
  const hideBody = hideMatch ? hideMatch[1] : '';
  const hideCleans =
    /cleanupMenuInteraction\s*\(/.test(hideBody) ||
    (/exitEditor\s*\(/.test(hideBody) && /pendingRootFocus\s*=\s*false/.test(hideBody) && /editorCtx\s*=\s*null/.test(hideBody));
  const selectMatch = text.match(/function\s+select\s*\([^)]*\)\s*\{([\s\S]*?)\}/);
  const selectBody = selectMatch ? selectMatch[1] : '';
  const selectCleans =
    /cleanupMenuInteraction\s*\(/.test(selectBody) ||
    /exitEditor\s*\(/.test(selectBody);
  const dismissMatch = text.match(/function\s+dismiss\s*\([^)]*\)\s*\{([\s\S]*?)\}/);
  const dismissBody = dismissMatch ? dismissMatch[1] : '';
  const dismissCleans =
    /cleanupMenuInteraction\s*\(/.test(dismissBody) ||
    /exitEditor\s*\(/.test(dismissBody);
  const renderMatch = text.match(/function\s+render\s*\([^)]*\)\s*\{([\s\S]*?)\n\s*function\s+/);
  // render is large; fall back to a windowed search from "function render"
  let renderCleans = false;
  const renderIdx = text.indexOf('function render(st)');
  if (renderIdx >= 0) {
    const window = text.slice(renderIdx, renderIdx + 900);
    renderCleans =
      /cleanupMenuInteraction\s*\(/.test(window) ||
      (/exitEditor\s*\(/.test(window) && /pendingRootFocus\s*=\s*false/.test(window) && /editorCtx\s*=\s*null/.test(window));
  }
  return {
    hasCleanupHelper,
    hideCleans,
    selectCleans,
    dismissCleans,
    renderCleans,
    allPathsClean: !!(hasCleanupHelper && hideCleans && selectCleans && dismissCleans && renderCleans),
  };
}

// ── Compact-HWND (contentSized / independent submenu) chrome ────────────────

/** Declaration block of the rule whose selector list contains `selector`. */
function findCssRuleBody(css, selector) {
  const clean = String(css || '').replace(/\/\*[\s\S]*?\*\//g, '');
  for (const chunk of clean.split('}')) {
    const brace = chunk.indexOf('{');
    if (brace < 0) continue;
    const selectors = chunk.slice(0, brace).split(',').map((s) => s.trim());
    if (selectors.includes(selector)) return chunk.slice(brace + 1);
  }
  return null;
}

/**
 * Default CSS contract for compact surfaces: a tight HWND clips CSS shadows,
 * so the DWM system shadow takes over and the radius shrinks to the native
 * 4px. The fullscreen sheet keeps its original radius and shadow.
 */
export function analyzeContentSizedChromeCss(css) {
  const compact = findCssRuleBody(css, '.fb-content-sized .fb-menu');
  const base = findCssRuleBody(css, '.fb-menu');
  return {
    hasCompactMenuRule: compact != null,
    compactRadiusIs4px: /border-radius\s*:\s*4px/.test(compact || ''),
    compactDropsCssShadow: /box-shadow\s*:\s*none/.test(compact || ''),
    fullscreenKeepsRadius: /border-radius\s*:\s*6px/.test(base || ''),
    fullscreenKeepsShadow: /box-shadow\s*:\s*[^;]*rgba/.test(base || ''),
  };
}

/**
 * Source contract: the renderer must flag `<html>` for both compact window
 * models so the compact chrome rules apply to root and submenu surfaces.
 */
export function analyzeContentSizedRootClassWiring(jsSource) {
  const text = String(jsSource || '');
  const renderBody = sliceBetween(text, 'function render(st){', 'function reportSubmenuPanel(');
  const match = renderBody.match(/document\.documentElement\.classList\.toggle\(\s*["']fb-content-sized["']\s*,\s*([^)]*)\)/);
  const condition = match ? match[1] : '';
  return {
    togglesRootClassInRender: !!match,
    coversContentSized: /\bcontent\b/.test(condition),
    coversSubmenuSurface: /\bsubmenuSurface\b/.test(condition),
    keepsBodyContentSizedFlag: /document\.body\.classList\.toggle\(\s*["']content-sized["']\s*,\s*content\s*\)/.test(renderBody),
  };
}
