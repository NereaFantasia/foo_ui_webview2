# fb.cursor 光标控制

`fb.cursor` 用于显式控制调用窗口客户区内的光标。状态与 `cursor:hiddenChanged` 事件都限定在发起调用的窗口，因此各弹窗可以独立管理光标可见性。

## isHidden()

签名：`fb.cursor.isHidden(): Promise<CursorIsHiddenResponse>`

返回调用窗口的 `{ hidden }`。宿主找不到调用窗口时返回 `{ hidden: false }`。

```javascript
const res = await fb.cursor.isHidden();
if (res.success === false) throw new Error(res.error);
const { hidden } = res;
```

## setHidden(hidden)

签名：`fb.cursor.setHidden(hidden: boolean): Promise<CursorSetHiddenResponse>`

`hidden` 为 `true` 时隐藏光标，为 `false` 时恢复。重复设置当前状态会以 `success: true`、`changed: false` 返回；只有状态真正改变时才发出 `cursor:hiddenChanged`。宿主找不到调用窗口时以 `OPERATION_FAILED` 失败。

```javascript
await fb.cursor.setHidden(true);
```

## 事件

`cursor:hiddenChanged` 携带 `{ hidden: boolean }`，且只会发送给改变了光标状态的窗口。
