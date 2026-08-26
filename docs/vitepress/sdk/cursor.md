# fb.cursor Cursor Visibility

`fb.cursor` explicitly controls the calling window's client-area cursor. State and the `cursor:hiddenChanged` event are scoped to the originating window, so popups can manage visibility independently.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

### isHidden()

Signature: `fb.cursor.isHidden(): Promise<CursorIsHiddenResponse>`

Returns `{ hidden }` for the calling window. If the host cannot resolve that window, the response falls back to `{ hidden: false }`.

```javascript
const { hidden } = await fb.cursor.isHidden();
```

### setHidden(hidden)

Signature: `fb.cursor.setHidden(hidden: boolean): Promise<CursorSetHiddenResponse>`

Hides the cursor when `hidden` is `true` and restores it when `false`. Repeating the current state resolves with `success: true` and `changed: false`; only actual state changes emit `cursor:hiddenChanged`.

```javascript
await fb.cursor.setHidden(true);
```

## Event

`cursor:hiddenChanged` carries `{ hidden: boolean }` and is delivered only to the window that changed its cursor state.

<!-- END AUTO-GENERATED SDK STUBS -->
