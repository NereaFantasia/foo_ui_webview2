# E. Window Management

## `<fb-titlebar>` {#fb-titlebar}

Custom title-bar drag region. Double-clicking toggles maximize/restore, and right-clicking opens the system menu.

```html
<fb-titlebar>
  <span slot="left">foobar2000</span>
  <span slot="right"><fb-window-controls></fb-window-controls></span>
</fb-titlebar>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `maximized` | boolean | — | Read-only reflection of whether the window is maximized |

**CSS Parts:** `drag-region`
**Slots:** `left`, `right` (slotted content is excluded from the drag region)

**Events:**

```js
el.addEventListener('fb-titlebar-dblclick', e => {
  console.log('Title bar double-clicked');
});
```

## `<fb-window-controls>` {#fb-window-controls}

Minimize, maximize/restore, and close buttons. The `maximize-button` and `restore-icon` parts are shown mutually exclusively according to the `maximized` state.

```html
<fb-window-controls>
  <span slot="minimize-icon">—</span>
  <span slot="maximize-icon">□</span>
  <span slot="restore-icon">❐</span>
  <span slot="close-icon">✕</span>
</fb-window-controls>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `maximized` | boolean | — | Read-only reflection of whether the window is maximized |

**CSS Parts:** `controls`, `minimize-button`, `maximize-button`, `restore-icon`, `close-button`
**Slots:** `minimize-icon` (default: —), `maximize-icon` (default: □), `restore-icon` (default: ❐), `close-icon` (default: ✕)

**Events:**

```js
el.addEventListener('fb-window-minimize', e => { /* minimize requested */ });
el.addEventListener('fb-window-maximize', e => { /* maximize/restore requested */ });
el.addEventListener('fb-window-close', e => { /* close requested */ });
```

## `<fb-popup-panel>` {#fb-popup-panel}

Declarative popup-window trigger. Removing the element closes its popup automatically.

```html
<fb-popup-panel url="settings.html" width="600" height="400" popup-title="Settings">
  ⚙️ Open settings
</fb-popup-panel>
```

| Attribute | Type | Default | Description |
| --- | --- | --- | --- |
| `url` | string | `''` | Popup page URL |
| `width` | string | `'400'` | Width in pixels |
| `height` | string | `'300'` | Height in pixels |
| `popup-title` | string | `''` | Popup title |
| `resizable` | string | enabled | Set to the exact string `false` to disable resizing |
| `always-on-top` | boolean | false | Enables always-on-top when present |
| `show-in-taskbar` | boolean | false | Shows the popup in the taskbar when present |
| `frame` | string | enabled | Set to the exact string `false` to disable the frame |
| `transparent` | boolean | false | Enables transparency when present |
| `before-close` | boolean | false | Requests confirmation before closing when present |
| `open` | boolean | — | Read-only reflection of whether the popup is open |
| `window-id` | string | — | Read-only reflection of the current popup window ID |

**Slots:** default slot (the clickable popup trigger)

**JavaScript API:**

```js
const windowId = await el.open(); // opens once; returns the window ID or null
await el.close();                 // requests closure; no-op when already closed
console.log(el.windowId);         // current window ID or null
```

**Events:**

```js
el.addEventListener('fb-popup-open', e => {
  console.log('Popup opened:', e.detail.windowId);
});
el.addEventListener('fb-popup-close', e => {
  console.log('Popup closed:', e.detail.windowId);
});
el.addEventListener('fb-popup-message', e => {
  console.log('Popup message:', e.detail.sourceWindowId, e.detail.message);
});
```
