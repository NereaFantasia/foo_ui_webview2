# fb.cursor 光标控制

`fb.cursor` 用于显式控制调用窗口客户区内的光标。状态与 `cursor:hiddenChanged` 事件都限定在发起调用的窗口，因此各弹窗可以独立管理光标可见性。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### isHidden()

封装 `cursor.isHidden`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.cursor.isHidden(/* 参数见 TypeScript 声明 */);
```

### setHidden()

封装 `cursor.setHidden`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.cursor.setHidden(/* 参数见 TypeScript 声明 */);
```

<!-- END AUTO-GENERATED SDK STUBS -->

## 事件

`cursor:hiddenChanged` 携带 `{ hidden: boolean }`，且只会发送给改变了光标状态的窗口。
