import { describe, test } from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import fs from 'node:fs';
import {
  isAllowedElement,
  isAllowedAttribute,
  isValidViewBox,
  isValidOpacity,
  isValidFillOrStroke,
  isValidFillRule,
  isValidLinecap,
  isValidLinejoin,
  isValidPathData,
  isValidTransform,
  isSvgNamespace,
  isValidAttributeValue,
  contentLooksUnsafe,
  isOversizedSvgContent,
} from '../../src/window/menu-overlay/svg-allowlist.mjs';
import {
  buildMenuDocumentModel,
  buildItemStableAttrs,
  menuDirectItems,
  menuDirectSeparators,
  menuZones,
  findById,
  hideMenuProtocol,
  showMenuProtocol,
  analyzeProtectedCss,
  analyzeDefaultCss,
  analyzeRichControlCss,
  analyzeHtmlShell,
  normalizePlacedGeometry,
  hiddenSubmenuSlotStyle,
  visibleSubmenuStyle,
  submenuWindowPlacement,
  analyzeIndependentSubmenuWindowContract,
  analyzeRootCloseSubmenuPolicy,
  resolveMenuArrowLeftAction,
  buildMeasurePayload,
  sameMeasurePayload,
  resolveSliderOrientation,
  normalizeSliderRange,
  sliderValueFromPointer,
  sliderPaintStyles,
  sliderKeyAction,
  applySliderKey,
  wheelAdjustStep,
  shouldEmitSliderValue,
  shouldThrottleEmit,
  resolveNavRowRole,
  buildRichNavAriaLabel,
  isExplicitCheckable,
  analyzeReducedMotionCss,
  shouldFocusOnActivate,
  isSegmentEnabled,
  nextEnabledSegmentIndex,
  firstEnabledSegmentIndex,
  planSegmentedEditorRoving,
  planSegmentedEditorAdjust,
  planSegmentedNavigationInternals,
  analyzeMouseenterSetActiveFocus,
  analyzeMenuInteractionCleanup,
  contentSizedGeometryPlan,
  clearContentSizedGeometryPlan,
  analyzeContentSizedGeometryContract,
  MENU_SHOW_DELAY_FALLBACK_MS,
  resolveMenuShowDelayMs,
  evaluateHoverIntent,
  analyzeHoverIntentWiring,
  analyzeContentSizedChromeCss,
  analyzeContentSizedRootClassWiring,
} from '../../src/window/menu-overlay/menu-overlay-layout.mjs';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const REPO = path.resolve(__dirname, '..', '..');
const OVERLAY = path.join(REPO, 'src/window/menu-overlay');

describe('menu overlay SVG allowlist predicates', () => {
  test('allows only the DESIGN 8.4 element set', () => {
    for (const el of ['path', 'circle', 'rect', 'line', 'polyline', 'polygon', 'ellipse', 'g']) {
      assert.equal(isAllowedElement(el), true);
    }
    assert.equal(isAllowedElement('script'), false);
    assert.equal(isAllowedElement('foreignObject'), false);
    assert.equal(isAllowedElement('style'), false);
    assert.equal(isAllowedElement('animate'), false);
    assert.equal(isAllowedElement('svg'), false);
  });

  test('rejects on* / href / unknown attrs', () => {
    assert.equal(isAllowedAttribute('path', 'd'), true);
    assert.equal(isAllowedAttribute('path', 'fill'), true);
    assert.equal(isAllowedAttribute('path', 'onclick'), false);
    assert.equal(isAllowedAttribute('path', 'href'), false);
    assert.equal(isAllowedAttribute('path', 'xlink:href'), false);
    assert.equal(isAllowedAttribute('path', 'unknown'), false);
    assert.equal(isAllowedAttribute('circle', 'd'), false);
  });

  test('viewBox must be four finite numbers with positive size', () => {
    assert.equal(isValidViewBox('0 0 24 24'), true);
    assert.equal(isValidViewBox('0,0,24,24'), true);
    assert.equal(isValidViewBox('0 0 0 24'), false);
    assert.equal(isValidViewBox('0 0 24'), false);
    assert.equal(isValidViewBox('0 0 24 NaN'), false);
    assert.equal(isValidViewBox('0 0 1e7 24'), false);
  });

  test('fill/stroke/opacity/linecap/linejoin/path/transform value gates', () => {
    assert.equal(isValidFillOrStroke('currentColor'), true);
    assert.equal(isValidFillOrStroke('none'), true);
    assert.equal(isValidFillOrStroke('red'), false);
    assert.equal(isValidFillOrStroke('url(#x)'), false);
    assert.equal(isValidOpacity('0.5'), true);
    assert.equal(isValidOpacity('1.1'), false);
    assert.equal(isValidFillRule('evenodd'), true);
    assert.equal(isValidFillRule('inherit'), false);
    assert.equal(isValidLinecap('round'), true);
    assert.equal(isValidLinejoin('bevel'), true);
    assert.equal(isValidPathData('M0 0 L10 10 Z'), true);
    assert.equal(isValidPathData('M0 0 L10 10;alert(1)'), false);
    assert.equal(isValidTransform('translate(1 2) scale(0.5)'), true);
    assert.equal(isValidTransform('matrix(1 0 0 1 0 0)'), true);
    assert.equal(isValidTransform('url(#t)'), false);
    assert.equal(isValidTransform('skewX(10)'), true);
    assert.equal(isValidAttributeValue('path', 'd', 'M0 0h10'), true);
    assert.equal(isValidAttributeValue('path', 'fill', 'currentColor'), true);
    assert.equal(isValidAttributeValue('path', 'fill', '#fff'), false);
  });

  test('transform rejects prefix, inter-function junk/commas, empty args, wrong arity', () => {
    // Prefix / gap junk must not be ignored by a loose global regex.
    assert.equal(isValidTransform('evil translate(1)'), false);
    assert.equal(isValidTransform('translate(1) evil scale(2)'), false);
    assert.equal(isValidTransform('translate(1),scale(2)'), false);
    // Zero-separator between functions is illegal (SVG transform-list requires
    // wsp and/or comma; this allowlist requires wsp and rejects bare adjacency).
    assert.equal(isValidTransform('translate(1)scale(2)'), false);
    assert.equal(isValidTransform('matrix(1 0 0 1 0 0)translate(1)'), false);
    // Empty / wrong arity.
    assert.equal(isValidTransform('matrix()'), false);
    assert.equal(isValidTransform('translate()'), false);
    assert.equal(isValidTransform('scale()'), false);
    assert.equal(isValidTransform('rotate()'), false);
    assert.equal(isValidTransform('skewX()'), false);
    assert.equal(isValidTransform('matrix(1 0 0 1 0)'), false); // need 6
    assert.equal(isValidTransform('matrix(1 0 0 1 0 0 0)'), false);
    assert.equal(isValidTransform('translate(1 2 3)'), false);
    assert.equal(isValidTransform('rotate(10 20)'), false); // 1 or 3 only
    assert.equal(isValidTransform('skewX(1 2)'), false);
    // Legal forms still pass.
    assert.equal(isValidTransform('translate(1)'), true);
    assert.equal(isValidTransform('translate(1,2)'), true);
    assert.equal(isValidTransform('scale(2)'), true);
    assert.equal(isValidTransform('rotate(45)'), true);
    assert.equal(isValidTransform('rotate(45 10 10)'), true);
    assert.equal(isValidTransform('  translate(1)   scale(2)  '), true);
  });

  test('isSvgNamespace requires the exact SVG namespace URI', () => {
    assert.equal(isSvgNamespace('http://www.w3.org/2000/svg'), true);
    assert.equal(isSvgNamespace(''), false);
    assert.equal(isSvgNamespace(null), false);
    assert.equal(isSvgNamespace(undefined), false);
    assert.equal(isSvgNamespace('http://www.w3.org/1999/xhtml'), false);
  });

  test('contentLooksUnsafe catches script/on*/href/url/oversize', () => {
    assert.equal(contentLooksUnsafe('<path d="M0 0h1"/>'), false);
    assert.equal(contentLooksUnsafe('<script>alert(1)</script>'), true);
    assert.equal(contentLooksUnsafe('<path onclick="x" d="M0 0"/>'), true);
    assert.equal(contentLooksUnsafe('<use href="#a"/>'), true);
    assert.equal(contentLooksUnsafe('<path fill="url(#g)" d="M0 0"/>'), true);
    assert.equal(contentLooksUnsafe('a'.repeat(32 * 1024 + 1)), true);
    assert.equal(isOversizedSvgContent('a'.repeat(32 * 1024 + 1)), true);
  });
});

describe('menu overlay page generator', () => {
  test('--check is idempotent after --write', () => {
    const script = path.join(REPO, 'scripts/gen_menu_overlay_page.mjs');
    const check = spawnSync(process.execPath, [script, '--check'], {
      cwd: REPO,
      encoding: 'utf8',
    });
    assert.equal(check.status, 0, check.stderr || check.stdout);
  });
});

describe('menu overlay Phase 2 DOM / CSS layout contracts', () => {
  const sampleFlatItems = [
    { id: 'a', label: 'A', type: 'normal', _zone: 'top', _token: 'tok-a' },
    { type: 'separator', _zone: 'top' },
    { id: 'pb', label: 'Play', type: 'normal', _zone: 'playback', _token: 'tok-pb' },
    { type: 'nowplaying', id: 'np', title: 'T', _zone: 'top', _token: 'tok-np' },
    { type: 'rating', id: 'r', value: 3, _zone: 'playback', _token: 'tok-r' },
    { type: 'slider', id: 'vol', value: 50, min: 0, max: 100, _zone: 'playback', _token: 'tok-s' },
    { type: 'segmented', id: 'mode', value: 0, segments: [{ label: 'A' }], _zone: 'playback', _token: 'tok-g' },
    { label: 'id-less', type: 'normal', _zone: 'bottom', _token: 'tok-x' },
  ];

  test('flat: #menu > .fb-item and root separators stay direct children', () => {
    const doc = buildMenuDocumentModel({
      layoutMode: 'flat',
      model: 'legacyItems',
      items: sampleFlatItems,
    });
    const menu = findById(doc, 'menu');
    assert.ok(menu);
    assert.equal(menuZones(doc).length, 0);
    const items = menuDirectItems(doc);
    assert.ok(items.length >= 2);
    assert.ok(menuDirectSeparators(doc).length >= 1);
    // Every direct child is an item or separator — no zone wrappers.
    for (const c of menu.children) {
      assert.equal(/\bfb-zone\b/.test(c.className || ''), false);
    }
  });

  test('zones: .fb-zone[data-zone] > .fb-item wrappers exist only when opted in', () => {
    const doc = buildMenuDocumentModel({
      layoutMode: 'zones',
      model: 'trayZones',
      zones: [
        { id: 'top', items: [{ id: 'a', label: 'A', _token: 't1' }] },
        { id: 'playback', items: [{ id: 'b', label: 'B', _token: 't2' }] },
        { id: 'bottom', items: [{ id: 'c', label: 'C', _token: 't3' }] },
      ],
    });
    const zones = menuZones(doc);
    assert.equal(zones.length, 3);
    assert.equal(zones[0].attrs['data-zone'], 'top');
    assert.equal(zones[1].attrs['data-zone'], 'playback');
    assert.equal(zones[2].attrs['data-zone'], 'bottom');
    for (const z of zones) {
      assert.ok((z.children || []).some((c) => /\bfb-item\b/.test(c.className || '')));
    }
    // Zone separators sit outside .fb-zone and carry preceding-zone data-zone/data-depth.
    const menu = findById(doc, 'menu');
    const zoneSeps = (menu.children || []).filter((c) =>
      String(c.className || '').split(/\s+/).includes('fb-zone-separator'));
    assert.equal(zoneSeps.length, 2);
    assert.equal(zoneSeps[0].attrs['data-zone'], 'top');
    assert.equal(zoneSeps[0].attrs['data-depth'], '0');
    assert.equal(zoneSeps[0].attrs.role, 'separator');
    assert.equal(zoneSeps[1].attrs['data-zone'], 'playback');
    assert.equal(zoneSeps[1].attrs['data-depth'], '0');
  });

  test('zones separator keeps data-zone when middle zone is absent', () => {
    const doc = buildMenuDocumentModel({
      layoutMode: 'zones',
      model: 'trayZones',
      zones: [
        { id: 'top', items: [{ id: 'a', label: 'A', _token: 't1' }] },
        { id: 'bottom', items: [{ id: 'c', label: 'C', _token: 't3' }] },
      ],
    });
    const menu = findById(doc, 'menu');
    const zoneSeps = (menu.children || []).filter((c) =>
      String(c.className || '').split(/\s+/).includes('fb-zone-separator'));
    assert.equal(zoneSeps.length, 1);
    assert.equal(zoneSeps[0].attrs['data-zone'], 'top');
    assert.equal(zoneSeps[0].attrs['data-depth'], '0');
  });

  test('menu.show legacy model never emits .fb-zone', () => {
    const doc = buildMenuDocumentModel({
      layoutMode: 'flat',
      model: 'legacyItems',
      items: [{ id: 'x', label: 'X', _token: 't' }],
    });
    assert.equal(menuZones(doc).length, 0);
  });

  test('stable attrs cover all kinds; id-less items omit data-item-id', () => {
    const kinds = [
      { it: { id: 'n', type: 'normal', _token: 't' }, kind: 'normal' },
      { it: { id: 'np', type: 'nowplaying', _token: 't' }, kind: 'nowplaying' },
      { it: { id: 'r', type: 'rating', _token: 't' }, kind: 'rating' },
      { it: { id: 's', type: 'slider', _token: 't' }, kind: 'slider' },
      { it: { id: 'g', type: 'segmented', _token: 't' }, kind: 'segmented' },
      { it: { type: 'normal', _token: 't-idless' }, kind: 'normal', noId: true },
    ];
    for (const row of kinds) {
      const attrs = buildItemStableAttrs(row.it, { depth: 0, zone: 'playback' });
      assert.equal(attrs['data-kind'], row.kind);
      assert.equal(attrs['data-depth'], '0');
      assert.equal(attrs['data-zone'], 'playback');
      assert.equal(attrs['data-item-token'], row.it._token);
      if (row.noId) assert.equal('data-item-id' in attrs, false);
      else assert.equal(attrs['data-item-id'], row.it.id);
    }
    const sepDoc = buildMenuDocumentModel({
      layoutMode: 'flat',
      model: 'legacyItems',
      items: [{ type: 'separator', _zone: 'top' }],
    });
    const sep = menuDirectSeparators(sepDoc)[0];
    assert.equal(sep.attrs.role, 'separator');
    assert.equal(sep.attrs['data-zone'], 'top');
    assert.equal(sep.attrs['data-depth'], '0');
  });

  test('hide/show protocol: hidden wins over user display; visible writes no inline display', () => {
    const hide = hideMenuProtocol();
    assert.equal(hide[0].op, 'addClass');
    assert.equal(hide[0].className, 'fb-hidden');
    assert.equal(hide[1].op, 'setProperty');
    assert.equal(hide[1].name, 'display');
    assert.equal(hide[1].value, 'none');
    assert.equal(hide[1].priority, 'important');

    const show = showMenuProtocol();
    assert.equal(show[0].op, 'removeProperty');
    assert.equal(show[0].name, 'display');
    assert.equal(show[1].op, 'removeClass');
    assert.equal(show[1].className, 'fb-hidden');
    // Visible path never sets display (user CSS may use flex/grid).
    assert.equal(show.some((s) => s.op === 'setProperty' && s.name === 'display'), false);
  });

  test('document model always wraps #menu in #viewport', () => {
    const doc = buildMenuDocumentModel({ layoutMode: 'flat', items: [] });
    assert.equal(doc.id, 'viewport');
    assert.ok(findById(doc, 'menu'));
  });

  test('editable sources: viewport shell + protected CSS shrink + no default display:none lock', () => {
    const html = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.html'), 'utf8');
    const protectedCss = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.protected.css'), 'utf8');
    const defaultCss = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.css'), 'utf8');

    const shell = analyzeHtmlShell(html);
    assert.equal(shell.hasViewport, true, 'html must include #viewport');
    assert.equal(shell.menuInsideViewport, true, '#menu must be inside #viewport');

    const prot = analyzeProtectedCss(protectedCss);
    assert.equal(prot.hasViewportRule, true, 'protected CSS must own #viewport');
    assert.equal(prot.forcesVisibleBlock, false, 'must not force #menu visible display:block!important');
    assert.equal(prot.hasHiddenFallback, true, 'hidden fallback display:none!important required');
    assert.equal(prot.hasMenuBoxModel, true);

    const def = analyzeDefaultCss(defaultCss);
    assert.equal(def.locksMenuDisplayNone, false, 'default CSS must not lock .fb-menu{display:none}');

    const rich = analyzeRichControlCss(defaultCss);
    assert.equal(rich.verticalUsesGrid, true, 'vertical slider row must use explicit grid geometry');
    assert.equal(rich.verticalHasTallTrack, true, 'vertical track needs a tall operable hit area');
    assert.equal(rich.verticalVisualTrackIsNarrow, true, 'vertical hit area must expose a narrow visual track');
    assert.equal(rich.verticalFillUsesVisualWidth, true, 'vertical fill must not cover the full hit width');
    assert.equal(rich.verticalThumbCentered, true, 'vertical thumb must remain centered on the visual track');
    assert.equal(rich.segmentedCanWrap, true, 'segmented options must wrap in constrained menus');
    assert.equal(rich.segmentedCanShrink, true, 'segmented row/group must shrink without overlap');
    assert.equal(rich.segmentedButtonsBoundText, true, 'long segmented labels need ellipsis bounds');
  });

  test('content-sized placement separates root panel from transparent submenu slot', () => {
    const geometry = normalizePlacedGeometry({ geometry: {
      viewportW: 600, viewportH: 300, rootSlotW: 240, subSlotW: 360, rootTop: 20,
    } }, 2);
    assert.deepEqual(geometry, {
      viewportW: 300, viewportH: 150, rootSlotW: 120, subSlotW: 180, rootTop: 10,
      virtualViewportH: 150, virtualRootTop: 10,
    });
    assert.equal(normalizePlacedGeometry({ first: true }, 1), null);

    assert.deepEqual(hiddenSubmenuSlotStyle(120, 150), {
      left: '120px', top: '0px', width: '0px', height: '150px', pointerEvents: 'none',
    });
    assert.deepEqual(visibleSubmenuStyle({
      rootSlotW: 120, subSlotW: 180, viewportH: 150, parentTop: 130, submenuHeight: 80,
    }), {
      left: '120px', top: '70px', maxWidth: '180px', maxHeight: '150px',
    });
    assert.deepEqual(submenuWindowPlacement({
      rootSlotW: 120, viewportH: 150, virtualViewportH: 260, virtualRootTop: 110,
      parentTop: 40, submenuHeight: 180,
    }), { x: 120, y: 80, h: 180 });
  });

  test('measurement payload reports actual submenu maxima and requires stable frames', () => {
    const first = buildMeasurePayload({
      menuId: 'menu-7', dpr: 1.5, root: { w: 170, h: 220 },
      submenus: [{ w: 260, h: 180 }, { w: 210, h: 310 }],
    });
    assert.deepEqual(first, {
      menuId: 'menu-7', root: { w: 255, h: 330 }, submenu: { maxW: 390, maxH: 465 },
    });
    assert.equal(sameMeasurePayload(first, structuredClone(first)), true);
    const changed = structuredClone(first);
    changed.submenu.maxW += 1;
    assert.equal(sameMeasurePayload(first, changed), false);
    assert.deepEqual(buildMeasurePayload({ menuId: 'plain', root: { w: 170, h: 90 }, dpr: 2 }), {
      menuId: 'plain', root: { w: 340, h: 180 }, submenu: { maxW: 0, maxH: 0 },
    });
  });

  test('content-sized submenu reports an independent HWND state contract', () => {
    const source = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.js'), 'utf8');
    assert.deepEqual(analyzeIndependentSubmenuWindowContract(source), {
      reportsMonotonicSequence: true,
      reportsParentToken: true,
      computesVirtualWindowPlacement: true,
      treatsSubmenuAsLeafSurface: true,
      reconcilesNativeSubmenuClose: true,
      waitsForNativeVisibilityBeforeFocus: true,
      announcesSurfaceReadiness: true,
      submenuArrowLeftReturnsToRoot: true,
      submenuEscapeRemainsWholeSurfaceDismiss: true,
    });
  });

  test('independent submenu ArrowLeft returns to the root surface at depth zero', () => {
    assert.deepEqual(resolveMenuArrowLeftAction({ windowModel: 'submenu', depth: 0 }), {
      kind: 'dismissSubmenu', reason: 'submenu-left',
    });
    assert.deepEqual(resolveMenuArrowLeftAction({ windowModel: 'contentSized', depth: 1 }), {
      kind: 'closeLayer',
    });
    assert.deepEqual(resolveMenuArrowLeftAction({ windowModel: 'contentSized', depth: 0 }), {
      kind: 'noop',
    });
  });

  test('root close hides and invalidates the child HWND before root animation', () => {
    const hostSource = fs.readFileSync(path.join(REPO, 'src/window/MenuOverlayHost.cpp'), 'utf8');
    assert.deepEqual(analyzeRootCloseSubmenuPolicy(hostSource), {
      resolvesRootClosePolicy: true,
      hidesChildBeforeRootCloseBranch: true,
      childCloseAvoidsFocusOrRootNotification: true,
      rejectsPanelUpdatesWhileClosing: true,
      rejectsReadyWhileClosing: true,
    });
  });
});

describe('menu overlay Phase 3 orientation / ARIA / focus helpers', () => {
  test('orientation: default/unknown/non-vertical → horizontal; exact vertical kept', () => {
    assert.equal(resolveSliderOrientation({}), 'horizontal');
    assert.equal(resolveSliderOrientation({ orientation: 'horizontal' }), 'horizontal');
    assert.equal(resolveSliderOrientation({ orientation: 'sideways' }), 'horizontal');
    assert.equal(resolveSliderOrientation({ orientation: 'vertical' }), 'vertical');
  });

  test('normalize range: swap, clamp, constant', () => {
    const swapped = normalizeSliderRange({ min: 80, max: 20, value: 999, orientation: 'vertical' });
    assert.equal(swapped.min, 20);
    assert.equal(swapped.max, 80);
    assert.equal(swapped.value, 80);
    assert.equal(swapped.constant, false);
    assert.equal(swapped.orientation, 'vertical');
    const constant = normalizeSliderRange({ min: 5, max: 5, value: 1 });
    assert.equal(constant.constant, true);
    assert.equal(constant.value, 5);
  });

  test('horizontal pointer/paint/keyboard', () => {
    const rect = { left: 0, right: 100, top: 0, bottom: 10, width: 100, height: 10 };
    assert.equal(sliderValueFromPointer({ orientation: 'horizontal', min: 0, max: 100, clientX: 50, clientY: 5, rect }), 50);
    assert.equal(sliderValueFromPointer({ orientation: 'horizontal', min: 0, max: 100, clientX: -10, clientY: 5, rect }), 0);
    assert.equal(sliderValueFromPointer({ orientation: 'horizontal', min: 0, max: 100, clientX: 200, clientY: 5, rect }), 100);
    const paint = sliderPaintStyles({ orientation: 'horizontal', min: 0, max: 100, value: 25 });
    assert.equal(paint.fill.width, '25%');
    assert.equal(paint.thumb.left, '25%');
    assert.equal(applySliderKey({ key: 'ArrowRight', orientation: 'horizontal', min: 0, max: 100, value: 10, step: 5 }), 15);
    assert.equal(applySliderKey({ key: 'ArrowLeft', orientation: 'horizontal', min: 0, max: 100, value: 10, step: 5 }), 5);
    assert.equal(applySliderKey({ key: 'Home', orientation: 'horizontal', min: 0, max: 100, value: 40 }), 0);
    assert.equal(applySliderKey({ key: 'End', orientation: 'horizontal', min: 0, max: 100, value: 40 }), 100);
  });

  test('vertical pointer/paint/keyboard: min bottom max top', () => {
    const rect = { left: 0, right: 10, top: 0, bottom: 100, width: 10, height: 100 };
    // clientY at bottom → min; at top → max; mid → mid
    assert.equal(sliderValueFromPointer({ orientation: 'vertical', min: 0, max: 100, clientX: 5, clientY: 100, rect }), 0);
    assert.equal(sliderValueFromPointer({ orientation: 'vertical', min: 0, max: 100, clientX: 5, clientY: 0, rect }), 100);
    assert.equal(sliderValueFromPointer({ orientation: 'vertical', min: 0, max: 100, clientX: 5, clientY: 50, rect }), 50);
    const paint = sliderPaintStyles({ orientation: 'vertical', min: 0, max: 100, value: 40 });
    assert.equal(paint.fill.height, '40%');
    assert.equal(paint.fill.bottom, '0');
    assert.equal(paint.fill.width, '');
    assert.equal(paint.fill.left, '');
    assert.equal(paint.thumb.bottom, '40%');
    assert.equal(applySliderKey({ key: 'ArrowUp', orientation: 'vertical', min: 0, max: 100, value: 10, step: 5 }), 15);
    assert.equal(applySliderKey({ key: 'ArrowDown', orientation: 'vertical', min: 0, max: 100, value: 10, step: 5 }), 5);
    assert.equal(applySliderKey({ key: 'ArrowRight', orientation: 'vertical', min: 0, max: 100, value: 10, step: 1 }), 11);
    assert.equal(applySliderKey({ key: 'Home', orientation: 'vertical', min: 0, max: 100, value: 40 }), 0);
    assert.equal(applySliderKey({ key: 'End', orientation: 'vertical', min: 0, max: 100, value: 40 }), 100);
  });

  test('constant slider never emits; throttle/force gates', () => {
    assert.equal(shouldEmitSliderValue({ min: 5, max: 5, value: 5, previous: 4 }), false);
    assert.equal(shouldEmitSliderValue({ min: 0, max: 100, value: 50, previous: 40 }), true);
    assert.equal(shouldEmitSliderValue({ min: 0, max: 100, value: 50, previous: 50 }), false);
    assert.equal(shouldEmitSliderValue({ min: 0, max: 100, value: 101, previous: 50 }), false);
    assert.equal(shouldEmitSliderValue({ min: 0, max: 100, value: NaN, previous: 50 }), false);
    assert.equal(shouldThrottleEmit(1000, 960, false, 50), false);
    assert.equal(shouldThrottleEmit(1000, 950, false, 50), true);
    assert.equal(shouldThrottleEmit(1000, 999, true, 50), true);
  });

  test('ARIA roles and checkable identity', () => {
    assert.equal(resolveNavRowRole({ kind: 'normal' }), 'menuitem');
    assert.equal(resolveNavRowRole({ kind: 'submenu' }), 'menuitem');
    assert.equal(resolveNavRowRole({ kind: 'normal', checkable: true, checked: false }), 'menuitemcheckbox');
    assert.equal(resolveNavRowRole({ kind: 'separator' }), 'separator');
    assert.match(buildRichNavAriaLabel({ kind: 'slider', label: 'Vol', value: 30, min: 0, max: 100 }), /Vol.*30.*Enter/);
    assert.match(buildRichNavAriaLabel({ kind: 'rating', label: 'Rate', value: 3 }), /3 of 5/);
    // zh keeps the physical key name "Enter" untranslated so the hint stays actionable.
    assert.match(buildRichNavAriaLabel({ kind: 'slider', label: 'Vol', value: 30, min: 0, max: 100, locale: 'zh' }), /Vol.*30.*Enter/);
    assert.match(buildRichNavAriaLabel({ kind: 'rating', label: 'Rate', value: 3, locale: 'zh' }), /5 星中的 3 星/);
    assert.equal(isExplicitCheckable({ checked: false }), true);
    assert.equal(isExplicitCheckable({ type: 'checkbox' }), true);
    assert.equal(isExplicitCheckable({ label: 'x' }), false);
  });

  test('sliderKeyAction covers Up/Right increase Down/Left decrease Home/End', () => {
    assert.deepEqual(sliderKeyAction('ArrowUp'), { kind: 'delta', delta: 1 });
    assert.deepEqual(sliderKeyAction('ArrowRight'), { kind: 'delta', delta: 1 });
    assert.deepEqual(sliderKeyAction('ArrowDown'), { kind: 'delta', delta: -1 });
    assert.deepEqual(sliderKeyAction('ArrowLeft'), { kind: 'delta', delta: -1 });
    assert.deepEqual(sliderKeyAction('Home'), { kind: 'edge', edge: 'min' });
    assert.deepEqual(sliderKeyAction('End'), { kind: 'edge', edge: 'max' });
    assert.equal(sliderKeyAction('a'), null);
  });

  test('wheelAdjustStep: up increases, magnitude ignored, horizontal is fallback', () => {
    const cases = [
      [{ deltaY: -100, deltaX: 0 }, 1],
      [{ deltaY: 100, deltaX: 0 }, -1],
      [{ deltaY: -1, deltaX: 0 }, 1],
      [{ deltaY: 240, deltaX: 0 }, -1],
      // Vertical wins even when both axes move (diagonal touchpad flicks).
      [{ deltaY: -100, deltaX: 50 }, 1],
      [{ deltaY: 0, deltaX: 50 }, 1],
      [{ deltaY: 0, deltaX: -50 }, -1],
      [{ deltaY: 0, deltaX: 0 }, 0],
    ];
    for (const [ev, expected] of cases) {
      assert.equal(wheelAdjustStep(ev), expected, JSON.stringify(ev));
    }
    assert.equal(wheelAdjustStep(), 0);
  });

  test('overlay js wires wheel adjust on the three rich rows with an editor-row guard', () => {
    const js = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.js'), 'utf8');
    const helper = js.match(/function attachWheelAdjust\(rowEl, row, isInteractive\)\{([\s\S]*?)\n    \}/);
    assert.ok(helper, 'attachWheelAdjust present');
    // Non-passive is what makes preventDefault able to stop the menu from scrolling.
    assert.match(helper[1], /addEventListener\("wheel",[\s\S]*\{passive:false\}\)/);
    // A wheel over a row that does not own editor mode must not move focus.
    assert.match(helper[1], /interactionMode==="editor"\s*&&\s*editorCtx\s*&&\s*editorCtx\.row\s*!==\s*row/);
    assert.match(helper[1], /row\.adjust\(step\)/);
    // rating / slider / segmented only — nowplaying and plain rows keep native scrolling.
    assert.equal((js.match(/attachWheelAdjust\(d, row,/g) || []).length, 3);
    assert.match(js, /attachWheelAdjust\(d, row, function\(\)\{ return en && !constant; \}\)/);
  });

  test('default CSS includes prefers-reduced-motion reduce for enter/exit', () => {
    const defaultCss = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.css'), 'utf8');
    const rm = analyzeReducedMotionCss(defaultCss);
    assert.equal(rm.hasReducedMotionQuery, true);
    assert.equal(rm.disablesTransformOrTransition, true);
    const protectedCss = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.protected.css'), 'utf8');
    assert.equal(/prefers-reduced-motion/.test(protectedCss), false, 'reduced-motion must not live in protected CSS');
  });

  test('overlay js uses single capture keydown and data-orientation', () => {
    const js = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.js'), 'utf8');
    assert.match(js, /data-orientation/);
    assert.match(js, /interactionMode/);
    assert.match(js, /enterEditor/);
    assert.match(js, /prefers-reduced-motion|setAttribute\("role","slider"\)/);
    assert.match(js, /addEventListener\("keydown",\s*onKey,\s*true\)/);
    // Mode split returns early so navigation does not double-handle.
    assert.match(js, /if\(interactionMode==="editor"\)/);
  });

  test('measureElement keeps un-ceiled CSS floats (single physical ceil)', () => {
    const js = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.js'), 'utf8');
    const body = js.match(/function measureElement\(el\)\{([\s\S]*?)\n    \}/);
    assert.ok(body, 'measureElement present');
    // Physical rounding happens exactly once, in buildMeasurePayload (ceil(css*dpr)).
    // A CSS-space ceil here double-rounds and inflates the tight window by up to
    // ~1css*dpr+1 physical px (visible uncovered DWM band at 125% scaling).
    assert.equal(/Math\.ceil/.test(body[1]), false,
      'measureElement must not pre-round CSS sizes');
    assert.match(body[1], /getBoundingClientRect/);
  });

  test('overlay js tail integrity: render/measure/onKey present once, no merge corruption', () => {
    const js = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.js'), 'utf8');
    // Historical Phase 3 merge corruption markers must never reappear.
    assert.equal(js.includes('fuexitEditor'), false);
    assert.equal(js.includes('pendingRootFocus=trueC'), false);
    assert.equal(js.includes('trueC++'), false);
    assert.match(js, /function render\(st\)\{/);
    assert.match(js, /function measureAndReport\(\)\{/);
    assert.match(js, /function onPlaced\(ev\)\{/);
    assert.match(js, /function onKey\(e\)\{/);
    assert.match(js, /menu\.__ready/);
    assert.match(js, /pendingRootFocus=true;/);
    // Single capture keydown registration; no second legacy navigation switch after Escape.
    assert.equal((js.match(/addEventListener\("keydown",\s*onKey,\s*true\)/g) || []).length, 1);
    assert.equal((js.match(/function onKey\(e\)\{/g) || []).length, 1);
    assert.equal((js.match(/function render\(st\)\{/g) || []).length, 1);
    // Editor mode must return before navigation Home/End handling.
    const editorIdx = js.indexOf('if(interactionMode==="editor")');
    const homeIdx = js.indexOf('case "Home"');
    assert.ok(editorIdx >= 0 && homeIdx > editorIdx);
  });

  test('shouldFocusOnActivate: default/true focus; focus:false suppresses', () => {
    assert.equal(shouldFocusOnActivate(undefined), true);
    assert.equal(shouldFocusOnActivate({}), true);
    assert.equal(shouldFocusOnActivate({ focus: true }), true);
    assert.equal(shouldFocusOnActivate({ focus: false }), false);
  });

  test('segmented next/focus plan skips disabled and keeps nav internals inert', () => {
    const segs = [
      { label: 'A' },
      { label: 'B', enabled: false },
      { label: 'C' },
      { label: 'D', enabled: false },
      { label: 'E' },
    ];
    assert.equal(isSegmentEnabled(segs[0]), true);
    assert.equal(isSegmentEnabled(segs[1]), false);
    assert.equal(nextEnabledSegmentIndex(segs, 0, 1), 2);
    assert.equal(nextEnabledSegmentIndex(segs, 2, 1), 4);
    assert.equal(nextEnabledSegmentIndex(segs, 4, 1), -1);
    assert.equal(nextEnabledSegmentIndex(segs, 2, -1), 0);
    assert.equal(nextEnabledSegmentIndex(segs, 0, -1), -1);
    assert.equal(firstEnabledSegmentIndex(segs), 0);
    assert.equal(firstEnabledSegmentIndex([{ enabled: false }, { enabled: false }]), -1);

    const enter = planSegmentedEditorRoving({ segments: segs, selectedIndex: 0 });
    assert.equal(enter.focusIndex, 0);
    assert.deepEqual(enter.tabIndexes, ['0', '-1', '-1', '-1', '-1']);
    assert.deepEqual(enter.ariaChecked, ['true', 'false', 'false', 'false', 'false']);

    const right = planSegmentedEditorAdjust({ segments: segs, current: 0, delta: 1 });
    assert.ok(right);
    assert.equal(right.nextIndex, 2);
    assert.equal(right.focusIndex, 2);
    assert.deepEqual(right.tabIndexes, ['-1', '-1', '0', '-1', '-1']);
    assert.deepEqual(right.ariaChecked, ['false', 'false', 'true', 'false', 'false']);

    const skipDisabled = planSegmentedEditorAdjust({ segments: segs, current: 2, delta: 1 });
    assert.equal(skipDisabled.nextIndex, 4);
    assert.equal(skipDisabled.focusIndex, 4);
    assert.deepEqual(skipDisabled.tabIndexes, ['-1', '-1', '-1', '-1', '0']);

    const left = planSegmentedEditorAdjust({ segments: segs, current: 4, delta: -1 });
    assert.equal(left.nextIndex, 2);
    assert.equal(left.focusIndex, 2);

    // Disabled never focus/checked even if selectedIndex points there.
    const bad = planSegmentedEditorRoving({ segments: segs, selectedIndex: 1 });
    assert.equal(bad.focusIndex, 0);
    assert.equal(bad.ariaChecked[1], 'false');
    assert.equal(bad.tabIndexes[1], '-1');

    const nav = planSegmentedNavigationInternals({ segments: segs, selectedIndex: 2 });
    assert.equal(nav.ariaHidden, true);
    assert.deepEqual(nav.tabIndexes, ['-1', '-1', '-1', '-1', '-1']);
    assert.equal(nav.ariaChecked[2], 'true');
    assert.equal(planSegmentedEditorAdjust({ segments: segs, current: 4, delta: 1 }), null);
  });

  test('runtime: all mouseenter setActive paths are non-focusing', () => {
    const js = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.js'), 'utf8');
    const analysis = analyzeMouseenterSetActiveFocus(js);
    // nowplaying/rating/slider/segmented/normal(+submenu) row hover must call setActive*
    assert.ok(analysis.setActiveHandlerCount >= 5, `expected >=5 mouseenter setActive handlers, got ${analysis.setActiveHandlerCount}`);
    assert.equal(analysis.allSetActiveNonFocusing, true, 'every mouseenter setActive path must use setActiveFromPointer or {focus:false}');
    // Helper must exist and keyboard paths still allow default focus.
    assert.match(js, /function setActiveFromPointer\s*\(/);
    assert.match(js, /shouldFocusOnActivate\s*\(/);
    assert.match(js, /setActive\(depth\+1,\s*firstNav\(depth\+1\)\)/); // keyboard/click openSub still focuses
    assert.match(js, /setActive\(depth-1,\s*parentIdx,\s*\{\s*focus\s*:\s*true\s*\}\)/);
    // Editor early-return preserved on row hover.
    const withEditorGuard = analysis.handlers.filter((h) => h.hasSetActive && /interactionMode==="editor"/.test(h.body));
    assert.ok(withEditorGuard.length >= 5);
  });

  test('runtime: hide/select/dismiss/render clean editor + pendingRootFocus', () => {
    const js = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.js'), 'utf8');
    const analysis = analyzeMenuInteractionCleanup(js);
    assert.equal(analysis.hasCleanupHelper, true);
    assert.equal(analysis.hideCleans, true, 'menu:__hide must cleanup editor state, not only toggle .in/.out');
    assert.equal(analysis.selectCleans, true);
    assert.equal(analysis.dismissCleans, true);
    assert.equal(analysis.renderCleans, true);
    assert.equal(analysis.allPathsClean, true);
    assert.match(js, /function cleanupMenuInteraction\s*\(/);
    assert.match(js, /pendingRootFocus\s*=\s*false/);
    // Consumes pure segmented helpers (not only local nextEnabled copy).
    assert.match(js, /planSegmentedEditorRoving\s*\(/);
    assert.match(js, /planSegmentedEditorAdjust\s*\(/);
    assert.match(js, /isSegmentEnabled\s*\(/);
  });

  test('ContentSized geometry is important, clearable, and never owns visible display', () => {
    const apply = contentSizedGeometryPlan({
      left: '0px', top: '12px', maxWidth: '240px', maxHeight: '300px',
    });
    assert.deepEqual(apply.map((step) => step.name),
      ['left', 'top', 'width', 'height', 'max-width', 'max-height', 'min-width', 'overflow']);
    assert.ok(apply.every((step) => step.op === 'setProperty' && step.priority === 'important'));
    assert.equal(apply.some((step) => step.name === 'display'), false);
    // width/height default to natural sizing for the measure pass; the placed
    // pass pins them so the panel exactly covers the tight HWND (no right/
    // bottom residue of uncovered DWM material).
    assert.equal(apply.find((step) => step.name === 'width').value, 'auto');
    assert.equal(apply.find((step) => step.name === 'height').value, 'auto');
    const pinned = contentSizedGeometryPlan({
      left: '0px', top: '0px', maxWidth: '240px', maxHeight: '300px', width: '240px', height: '300px',
    });
    assert.equal(pinned.find((step) => step.name === 'width').value, '240px');
    assert.equal(pinned.find((step) => step.name === 'height').value, '300px');

    const clear = clearContentSizedGeometryPlan();
    assert.deepEqual(clear.map((step) => step.name),
      ['left', 'top', 'width', 'height', 'max-width', 'max-height', 'min-width', 'overflow']);
    assert.ok(clear.every((step) => step.op === 'removeProperty'));
    assert.equal(clear.some((step) => step.name === 'display'), false);

    // DOM contract: execute the plans against a CSSStyleDeclaration-like
    // object. Geometry survives as inline important, then clears completely;
    // a theme-owned visible display value is never written or removed.
    const properties = new Map([['display', { value: 'grid', priority: 'important' }]]);
    const style = {
      setProperty(name, value, priority) { properties.set(name, { value, priority }); },
      removeProperty(name) { properties.delete(name); },
    };
    for (const step of apply) style.setProperty(step.name, step.value, step.priority);
    for (const name of ['left', 'top', 'max-width', 'max-height', 'min-width', 'overflow']) {
      assert.equal(properties.get(name).priority, 'important');
    }
    assert.deepEqual(properties.get('display'), { value: 'grid', priority: 'important' });
    for (const step of clear) style.removeProperty(step.name);
    assert.deepEqual([...properties.entries()],
      [['display', { value: 'grid', priority: 'important' }]]);

    const js = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.js'), 'utf8');
    const contract = analyzeContentSizedGeometryContract(js);
    assert.deepEqual(contract, {
      hasApplyHelper: true,
      hasClearHelper: true,
      appliesImportantGeometry: true,
      clearsGeometry: true,
      forcesVisibleDisplay: false,
      renderClearsBeforeModeBranch: true,
      rootUsesHelper: true,
      submenuUsesHelper: true,
    });
  });
});

describe('menu overlay hover intent + compact window chrome', () => {
  test('host MenuShowDelay wins; a missing key falls back to the Windows default', () => {
    assert.equal(MENU_SHOW_DELAY_FALLBACK_MS, 400);
    assert.equal(resolveMenuShowDelayMs({ menuShowDelayMs: 250 }), 250);
    assert.equal(resolveMenuShowDelayMs({ menuShowDelayMs: 0 }), 0, 'MenuShowDelay=0 must stay instantaneous');
    assert.equal(resolveMenuShowDelayMs({ menuShowDelayMs: 249.6 }), 250);
    // Older host: the key does not exist in the state payload at all.
    assert.equal(resolveMenuShowDelayMs({ menuId: 'm1', windowModel: 'fullscreen' }), 400);
    assert.equal(resolveMenuShowDelayMs(undefined), 400);
    assert.equal(resolveMenuShowDelayMs(null), 400);
    // Unusable values never beat the native default.
    assert.equal(resolveMenuShowDelayMs({ menuShowDelayMs: -1 }), 400);
    assert.equal(resolveMenuShowDelayMs({ menuShowDelayMs: NaN }), 400);
    assert.equal(resolveMenuShowDelayMs({ menuShowDelayMs: '250' }), 400);
    assert.equal(resolveMenuShowDelayMs({ menuShowDelayMs: null }), 400);
  });

  test('hover intent: schedule / cancel / none per expanded-parent vs hovered row', () => {
    // Nothing expanded and the row owns no submenu → no layer work at all.
    assert.deepEqual(
      evaluateHoverIntent({ openParentRowIdx: -1, hoverRowIdx: 3, hoverHasSub: false, delayMs: 400 }),
      { action: 'none', delayMs: 0 });
    // Nothing expanded, submenu row → schedule the open.
    assert.deepEqual(
      evaluateHoverIntent({ openParentRowIdx: -1, hoverRowIdx: 3, hoverHasSub: true, delayMs: 400 }),
      { action: 'schedule', delayMs: 400 });
    // Expanded elsewhere → schedule close, plus open when this row owns a submenu.
    assert.deepEqual(
      evaluateHoverIntent({ openParentRowIdx: 2, hoverRowIdx: 5, hoverHasSub: false, delayMs: 300 }),
      { action: 'schedule', delayMs: 300 });
    assert.deepEqual(
      evaluateHoverIntent({ openParentRowIdx: 2, hoverRowIdx: 5, hoverHasSub: true, delayMs: 300 }),
      { action: 'schedule', delayMs: 300 });
    // Idempotent re-hover of the expanded parent row: keep it, drop pending.
    assert.deepEqual(
      evaluateHoverIntent({ openParentRowIdx: 2, hoverRowIdx: 2, hoverHasSub: true, delayMs: 300 }),
      { action: 'cancel', delayMs: 0 });
    assert.deepEqual(
      evaluateHoverIntent({ openParentRowIdx: 0, hoverRowIdx: 0, hoverHasSub: true, delayMs: 300 }),
      { action: 'cancel', delayMs: 0 });
    // MenuShowDelay=0 keeps the pre-delay immediate behavior.
    assert.deepEqual(
      evaluateHoverIntent({ openParentRowIdx: 2, hoverRowIdx: 5, hoverHasSub: true, delayMs: 0 }),
      { action: 'schedule', delayMs: 0 });
    // Missing / unusable inputs degrade to the fallback instead of throwing.
    assert.deepEqual(
      evaluateHoverIntent({ openParentRowIdx: 2, hoverRowIdx: 5, hoverHasSub: true }),
      { action: 'schedule', delayMs: 400 });
    assert.deepEqual(
      evaluateHoverIntent({ openParentRowIdx: 2, hoverRowIdx: 5, hoverHasSub: true, delayMs: -5 }),
      { action: 'schedule', delayMs: 400 });
    assert.deepEqual(
      evaluateHoverIntent({ openParentRowIdx: 2, hoverRowIdx: -1, hoverHasSub: true, delayMs: 400 }),
      { action: 'none', delayMs: 0 });
    // A null "expanded parent" must not be read as row 0.
    assert.deepEqual(
      evaluateHoverIntent({ openParentRowIdx: null, hoverRowIdx: 0, hoverHasSub: false, delayMs: 400 }),
      { action: 'none', delayMs: 0 });
    assert.deepEqual(evaluateHoverIntent(), { action: 'none', delayMs: 0 });
  });

  test('runtime: every row hover site defers layer changes to one guarded timer', () => {
    const js = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.js'), 'utf8');
    const wiring = analyzeHoverIntentWiring(js);
    // nowplaying / rating / slider / segmented / normal+submenu rows.
    assert.deepEqual(wiring.hoverIntentSites, {
      nowplaying: true, rating: true, slider: true, segmented: true, normal: true,
    });
    assert.ok(wiring.rowHoverHandlerCount >= 5, `expected >=5 row hovers, got ${wiring.rowHoverHandlerCount}`);
    assert.equal(wiring.allRowHoversDeferLayerChange, true);
    assert.equal(wiring.noRowHoverChangesLayersInline, true,
      'hover must not call closeLayersFrom/openSub inline');
    assert.equal(wiring.highlightBeforeIntent, true,
      'row highlight stays immediate and precedes the intent scheduler');
    assert.ok(wiring.rowLeaveCancelCount >= 5, 'each row cancels its own pending intent on leave');
    assert.equal(wiring.submenuPanelEnterCancels, true, 'entering the submenu panel cancels pending');
    assert.equal(wiring.singlePendingTimer, true, 'exactly one shared hover-intent timer');
    assert.equal(wiring.readsHostDelayWithFallback, true);
  });

  test('runtime: hover intent is generation-guarded against stale menus', () => {
    const js = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.js'), 'utf8');
    const wiring = analyzeHoverIntentWiring(js);
    assert.equal(wiring.guardsGeneration, true,
      'timer callback must capture menuId+generation and no-op on mismatch');
    assert.equal(wiring.invalidatesGenerationOnCleanup, true);
    // render / hide / select / dismiss all reach cleanupMenuInteraction, which
    // cancels the pending timer and bumps the generation.
    assert.equal(analyzeMenuInteractionCleanup(js).allPathsClean, true);
    assert.match(js, /var hoverIntentGeneration = 0;/);
  });

  test('runtime: click and keyboard layer changes bypass the hover delay', () => {
    const js = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.js'), 'utf8');
    const wiring = analyzeHoverIntentWiring(js);
    assert.equal(wiring.clickPathStaysImmediate, true);
    assert.equal(wiring.clickCancelsPendingIntent, true);
    assert.equal(wiring.keyboardPathStaysImmediate, true);
    assert.equal(wiring.keyboardCancelsPendingIntent, true);
  });

  test('compact HWND surfaces flag <html> and hand the shadow to DWM', () => {
    const js = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.js'), 'utf8');
    assert.deepEqual(analyzeContentSizedRootClassWiring(js), {
      togglesRootClassInRender: true,
      coversContentSized: true,
      coversSubmenuSurface: true,
      keepsBodyContentSizedFlag: true,
    });
    const defaultCss = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.css'), 'utf8');
    assert.deepEqual(analyzeContentSizedChromeCss(defaultCss), {
      hasCompactMenuRule: true,
      compactRadiusIsZero: true,
      compactDropsCssShadow: true,
      fullscreenKeepsRadius: true,
      fullscreenKeepsShadow: true,
    });
    // Compact chrome is themeable default styling, never protected structure.
    const protectedCss = fs.readFileSync(path.join(OVERLAY, 'menu-overlay.protected.css'), 'utf8');
    assert.equal(/fb-content-sized/.test(protectedCss), false);
  });
});
