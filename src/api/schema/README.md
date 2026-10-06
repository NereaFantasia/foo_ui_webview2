# API 声明

每个命名空间对应一个 `<namespace>.ts`，以 TypeScript 声明方法的参数、返回值与路径安全级别，以及事件的载荷。声明是这些接口唯一手写的契约，以下产物均由它生成：

- C++ 参数结构体与解析器、返回值结构体与 `ToJson`，位于 `src/api/generated/<Ns>Schema.h`。通过 `node scripts/api-schema/generate.mjs --write` 生成；构建时 MSBuild 也会先运行 `--write-cpp`，仅更新这一项。
- 文档页中的说明、参数表与返回值表，位于 `docs/vitepress/**` 中 `<!-- api-schema:begin ns.method -->` 与 `<!-- api-schema:end -->` 之间。通过 `node scripts/api-schema/generate.mjs --write` 生成。
- MCP 工具定义，位于 `mcp/src/generated/`。通过 `node scripts/api-schema/generate.mjs --write` 生成。
- SDK 的 `XxxParams` 与 `XxxResponse`，位于 `sdk/src/types/generated/`。通过 `npm --prefix sdk run gen:types` 生成；`npm --prefix sdk run build` 也会先运行这一步。

C++ 与 SDK 两侧的类型均来自声明，不分别手写后再比对（Chromium 扩展 API 的 json_schema_compiler 采用同样的做法）。生成器通过 TypeScript 编译器读取声明，读取前先进行类型检查；类型错误或引用了不存在的类型时，生成失败，构建随之失败。

构建时的生成由 `scripts/api-schema/ApiSchema.targets` 接入主工程与测试工程，在编译前运行。`sdk/` 未安装依赖（缺少 TypeScript）时只给出警告，使用已提交的头文件编译。生成器只改写内容有变化的文件。

## 写法

```ts
import type { Int } from './common.js';

export interface Api {
  /**
   * Evaluate one title formatting pattern against one track.
   * @zh 对单个文件求值单个 titleformat 表达式。
   */
  eval(params: EvalParams): EvalResult;

  /** 没有参数的方法不写参数；只返回 success 的方法返回 void。 */
  getBuiltinFields(): GetBuiltinFieldsResult;
}

interface EvalParams {
  /**
   * File path of the track to evaluate against.
   * @zh 要求值的曲目文件路径。
   * @minLength 1
   * @security MediaRead
   */
  path: string;
}
```

### 结构

- 文件导出 `interface Api`，每个方法一个签名，参数至多一个对象。命名空间即文件名。
- 说明写在 JSDoc 中：正文为英文说明，`@zh` 为中文说明；中文缺省时文档使用英文说明。JSDoc 须单独成行，与声明写在同一行的注释不会被 TypeScript 关联到声明上。
- 可选参数以 `?` 标注。以下划线开头的键保留给桥接层注入（如 `_callerHwnd`），不得出现在声明中；调用时这类键不会被视为未知键。
- 键名可以是 C++ 关键字或 `json`（每个生成的头文件都声明了这一别名）。JSON 键、`kFields`、文档与 TS 类型均保持原名，只有 C++ 成员名加尾下划线（`default` 为 `default_`，`json` 为 `json_`），handler 按后者访问。

### 约束标签

- 可用的约束标签：`@minLength`、`@minimum`、`@maximum`、`@minItems`、`@default`（值为 JSON，字符串须带双引号）、`@security Read | Write | MediaRead | MediaWrite | FileWrite`。其余标签均报错。
- 带 `@security` 的路径数组可另加 `@skipInvalid`（不带值）：未通过安全检查的路径被丢弃，而不是拒绝整次调用；桥接层在成功响应中补充 `skippedPaths`（丢弃的条数，未丢弃任何路径时不含该键），生成的 TS 类型与文档表格也包含该字段。
- 对象数组（如 `items: { path: string }[]`）须检查路径时，除 `@security` 外还须以 `@pathKey path` 指明持有路径的必填字符串成员，桥接层在进入 handler 前逐个元素检查该成员。对象数组不支持 `@skipInvalid`。
- 同一对象数组中的多个成员各有不同的安全级别时，写作 `@pathKey source=Read destination=FileWrite`（成员=级别，以空格分隔），桥接层按书写顺序逐个检查，此时数组上不再写 `@security`。
- `@minimum` 与 `@maximum` 写在数字数组（`Int[]`、`number[]`）上时约束每一项：生成的解析器逐项检查，越界时以 `INVALID_PARAMS` 拒绝，错误信息带下标（`items[1] is out of range`）。MCP 工具的 `inputSchema` 与文档表格同样逐项标注该约束。

### 对环境的影响

方法的 JSDoc 可标注其对环境的影响，MCP 工具的注解据此计算：

- `@effect read`：不修改任何状态。
- `@effect write`：修改状态或只增加数据，例如播放控制、音量、选中项、新建播放列表。
- `@effect destructive`：删除或覆盖用户保留的数据，包括播放列表及其行、队列、标签、封面与文件。
- `@idempotent`（不带值）：以相同参数再次调用不产生额外效果，只能与 `write` 或 `destructive` 同用。
- `@openWorld`（不带值）：方法会访问 foobar2000 实例以外的资源。

MCP 暴露的方法须标注 `@effect`，缺少时 `generate.mjs` 报错。这三个标签写在参数、类型或事件上同样报错。

### 类型

- 支持的类型：`string`、`boolean`、`number`（浮点）、`Int`（整数，来自 `common.ts`）、`Json`（任意 JSON 值，来自 `common.ts`）、字符串字面量联合（枚举，如 `'info' | 'warning'`）、数组、`Record<string, T>`（映射）、接口与对象字面量。其余写法（其他泛型、`string | number` 这类非字面量联合）均报错。
- `A & Record<string, T>` 表示具名字段加任意数量的 `T` 类型额外键，只用于返回值。
- `T | null` 表示值可以为 `null`。返回值中按声明生成；参数中只能用于可选键，桥接层将值为 `null` 的键视为未传入。数组元素与映射值不能为 `null`。
- 接口的 `extends`、`keyof`、`Partial<>` 与数组中的自引用见[共享类型](#共享类型)。
- `Int` 在 C++ 侧为 `std::int64_t`，在 JavaScript 中仍为 `number`，只有 2^53 以内的整数能精确表示。可能超出这一范围的值（哈希、64 位 ID）须声明为 `string`。
- `Json` 原样传递，不做任何检查：C++ 侧为 `nlohmann::json`，SDK 侧为 `JsonValue`，文档中标为 `any`。只在方法确实不解析内容时使用，例如需要记录或原样保存的值；能写出形状的值须写出形状。`Json` 不能带 `| null`、`@default` 或约束。顶层参数键的值为 `null` 时视为未传入，因此 `Json` 参数收不到顶层的 `null`；数组元素与嵌套值中的 `null` 照常保留。
- 嵌套对象以具名接口声明时，C++ 侧按接口名生成一个共享的结构体（如 dialog 的 `FileFilter` 同时用于 `openFile` 与 `saveFile`），SDK 侧在 `sdk/src/types/generated/schema-types.ts` 中生成同名的 `interface`，文档表格中也写接口名。接口名在整个 SDK 中平铺导出，两个命名空间不能以同一名称声明不同形状。
- 带 JSDoc 的类型别名会将说明带到所有用到它的位置，适合复用同一段说明（如 dialog 的 `InitialFolder`）；使用处自带说明时以使用处为准。

### 返回值

- 返回值描述正常返回的形状，只写方法自身的字段。`success`、`error`、`code`、`skippedPaths` 由桥接层写入，出现在声明中即报错（嵌套对象不受此限）。
- 成功响应为 `{ success: true, …声明的字段 }`，失败响应为 `{ success: false, error, code }`，两者均由生成器补充到 TS 类型与文档中。
- 返回字段可以是可选（`?`，无值时不写该键）或可空（`| null`，无值时写 `null`），不能同时使用；C++ 侧两者均为 `std::optional<T>`。
- 每个方法生成 `<Method>Result` 结构体与 `ToJson`，`<Method>Params::Result` 指向它；返回 `void` 的方法为 `void`。

## 实验性与弃用标记

生命周期标记只写在导出的 `interface Api` 中方法签名的 JSDoc 上：

```ts
export interface Api {
  /**
   * Read a preview of the next item.
   * @zh 读取下一项的预览。
   * @experimental
   * @deprecated Use getNextItem instead.
   * @deprecatedZh 请改用 getNextItem。
   */
  previewNextItem(): NextItemResult;
}
```

- `@experimental` 不带值，表示接口后续可能变化。
- `@deprecated` 须带非空的英文替代或迁移说明，并与非空的中文 `@deprecatedZh` 成对出现。生成器检查非空与成对，说明的语言与内容须人工复核。
- 实验性与弃用标记可以分别使用，也可以同时使用；同一标签不能重复。
- 同一文件中的 `Api` 与 `Events` 容器各只能声明一次，生成器不接受 TypeScript 的接口声明合并。
- `Api` 与 `Events` 容器本身、事件成员、具名接口、类型别名、参数与返回字段等其余位置均不能使用这些标签，未被引用的声明同样会检查。

SDK 为方法生成的 `Params`、`Success`、`Response` 顶层 JSDoc，以及 `ApiParamsMap`、`ApiResponseMap`、`ApiMethodMap` 中的方法成员，会带上英文的 `@experimental` 与 `@deprecated`。共享类型与字段的说明不受影响；手写的 SDK 门面须在自身的 JSDoc 上标注。

中英文 API 文档区块在原说明之前依次显示实验性提示与对应语言的弃用说明。MCP 工具的 action 简短说明与中英文 action 表也显示这些标记，但不改变工具注解、参数或调用行为。

JSON 声明使用方法节点上的 `x-experimental: true`、`x-deprecated: "Use … instead."` 与 `x-deprecated-zh: "请改用……。"`，遵守同样的范围与成对规则。`x-experimental: false`、空说明与单一语言的弃用说明均报错。未标注的方法保持原有生成文本。

## 共享类型

- **跨命名空间共享**：`common.ts` 中的接口为共享类型（如曲目行 `Track`）。C++ 侧只在 `src/api/generated/CommonSchema.h` 的 `api::common` 命名空间中生成一份，只带 `ToJson`；引用它的命名空间头文件会自动包含它，类型写作 `api::common::Track`。SDK 侧在 `schema-types.ts` 中只导出一份。文档中用到它的表格只写一个指向 `reference/types.md` 的链接，类型本身在中英文的 `reference/types.md` 中各以一对 `<!-- api-schema:begin type:Track -->` 标记生成一次；缺少标记时 `generate.mjs` 报错。
- **共享类型只能用于返回值**：在参数中直接使用、嵌套使用或通过 `extends` 继承均被拒绝。`x-security` 只对顶层参数生效，`Track` 这类含路径字段的类型用于参数时会绕过路径安全检查；参数所需的形状须在命名空间文件中另行声明接口。
- **角色按可达性传递**：具名接口被某个方法的参数用到时，其下嵌套的每个结构体都生成 `FromJson`，即使先在返回值中出现过。参数规则（必填不能可空、数组元素与映射值不能为 `null`、除数字数组的逐项取值范围外数组元素不带约束）对参数可达的每一层都检查，包括通过 `extends` 继承的成员。无法满足的形状报错，不会静默地只生成 `ToJson`。
- **继承**：`interface PlaylistRow extends Track { index: Int }` 只写自身的成员。C++ 侧生成 `struct PlaylistRow : api::common::Track`，`FromJson`、`ToJson` 与 `kFields` 均包含基类成员；SDK 侧为 `interface PlaylistRow extends Track`；文档中公共基类占一行链接，本地基类的成员逐行列出。只能继承一个接口，不能重复声明基类已有的键；方法自身的 Params 与 Result 接口不能继承其他接口。
- **键名枚举**：`keyof Track` 展开为 `Track` 全部键的字符串字面量联合，可用作单个字符串参数的取值范围（`sortBy?: keyof Track`）。字符串数组的元素尚不支持约束，因此暂时无法声明 `fields?: (keyof Track)[]` 这类投影参数。
- **全可选副本**：`Partial<Track>` 是 `Track` 全部成员改为可选的具名类型 `TrackPartial`。共享类型的 `Partial` 同样是共享类型，须在 `common.ts` 中写 `export type TrackPartial = Partial<Track>;` 才会生成，别名自身的 JSDoc 即其说明；命名空间中可直接对本地接口使用 `Partial<Row>`，得到 `RowPartial`。含 `| null` 成员的类型经 `Partial` 后成员既可选又可空，不能用于返回值，因此这类类型不得使用 `Partial`。
- **递归**：接口可以在数组中引用自身（如托盘菜单的 `submenu?: MenuItem[]`），也可以经数组相互引用，或经嵌套对象间接引用自身。C++ 侧为 `std::optional<std::vector<MenuItem>>` 等形式，头文件预先声明全部结构体与函数，定义顺序不受限制。直接引用（`parent?: Node`）或在映射值中引用不受支持；方法自身的 Params 与 Result 接口不能递归引用，须将递归部分提取为单独的接口。
- **额外键**：`PlaylistRow & Record<string, string>` 仍以接口名生成结构体，因此同一命名空间中不能同时使用原接口与带额外键的版本。两者都需要时，先声明 `interface PlaylistRowColumns extends PlaylistRow {}`，再对其取交叉类型。`common.ts` 中的接口不能直接取交叉类型，须先在命名空间中继承。
- **`kFields`**：每个返回值结构体、共享结构体及其派生结构体都带 `static constexpr std::array<std::string_view, N> kFields`，按声明顺序列出全部键（基类在前）。手写 JSON 的输出路径（如媒体库查询的 `WriteTrackJson`）以它为准编写键集一致性的 gtest；比对前须先排除容器自身的键（如 `index`）。

## 修改已声明的方法

1. 修改 `<namespace>.ts`。
2. 运行 `node scripts/api-schema/generate.mjs --write`，重新生成头文件、文档区块与 MCP 工具。
3. 运行 `npm --prefix sdk run gen:types`，重新生成 SDK 类型。
4. 运行 `.\build.ps1` 构建。handler 用到变化的字段时，编译器会指出须修改的位置；构建结束后运行 gtest。

`node scripts/api-schema/generate.mjs --check` 与 `node scripts/gen_sdk_types.mjs --all --diff` 会发现未重新生成的产物，CI 同样运行这两项检查。`--check` 同时核对每个已声明的方法都通过 `api::RegisterApi("<ns.method>"` 或 `api::RegisterApiDeferred("<ns.method>"` 注册，并指出已声明却通过 `BridgeCore::RegisterUndeclaredApi` 注册的方法，以及 `UNDECLARED_RAW` 中无人注册的名称。`BridgeCore::RegisterApi` 为私有成员，只能经 `api::RegisterApi` 调用，未声明的方法名会导致编译失败。

## 设计新方法的原则

宿主方法供所有主题与插件使用，不为某个特定主题或页面设计。编写声明前，须先确认以下三点。

### 参照先例

按以下顺序查找先例，找到后沿用其语义，并在方法说明中写明不同之处：

1. Web 平台标准。页面已具备的能力不再做成宿主方法，例如保持屏幕常亮使用 Screen Wake Lock API（`navigator.wakeLock`），它在 WebView2 中可用。
2. 桌面框架 Electron 与 Tauri。例如窗口按比例缩放对应 Electron 的 `setAspectRatio`；向 `<video>` 等元素提供本地文件参照 Tauri 的 asset 协议：按文件头确定 MIME，每个 206 响应限定长度，请求的范围更长时只返回前一段，由页面继续请求。
3. 播放器标准：MPRIS、Windows 的 SMTC 与 Web 的 Media Session。例如播放位置不逐帧推送，由客户端按播放速率推算，这是 MPRIS 的做法；位置附带读取时刻，对应 SMTC 时间轴信息中的 `LastUpdatedTime`。

### 沿用语义，不沿用前提

上述项目的前提与本组件不同：

- Electron 默认应用代码可信，而本组件的页面是第三方主题。路径参数须标注 `@security`；交给页面的文件访问须限定在发起调用的页面，页面中的其他来源（iframe、跳转到的外部站点）不得取用。
- 宿主运行在 foobar2000 的主线程上，读取文件、解析数据等耗时工作不能在 handler 中同步完成。面板模式没有独立窗口，窗口类方法须写明在面板中的失败方式。
- 本组件只支持 Windows，仅在其他平台上有意义的参数不予采用。
- 命名遵循本仓库的约定：方法使用点号（`playback.play`），事件使用冒号（`playback:stateChanged`）。

### 至少两个用例

能写出两个不同场景的调用方时，能力才放入宿主；只有一个页面需要的能力，先在主题或 SDK 中实现。名称、参数与返回值按能力命名，不按第一个使用它的页面命名。方法返回事实，不替页面下结论，例如报告文件中有哪些轨道，是否可以播放由页面判断。

## 新增方法

先按[设计新方法的原则](#设计新方法的原则)确认该能力应由宿主提供，再编写声明，最后编写 handler：

1. 在 `<namespace>.ts` 的 `Api` 中添加方法签名，以及参数与返回值接口。
2. 在中英文文档页中各添加该方法的标题与一对空的 `api-schema` 标记，然后运行 `generate.mjs --write` 生成头文件与文档区块。示例写在标记之外。
3. 编写 handler。handler 接收生成的参数结构体，返回生成的结果类型，例如 `api::Result<api::titleformat::EvalResult> TitleformatEval(const api::titleformat::EvalParams& p)`：
   - 成功时返回结果结构体；声明返回 `void` 的方法返回 `api::Ok()`。
   - 失败时返回 `api::Fail("原因", ApiErrorCode::…)`。
   - 只在独立窗口中有意义的方法，从面板调用时返回 `api::PanelModeUnsupported("ns.method")`。
   - 必填与类型检查由生成的解析器完成，handler 中不再手写。缺省值随界面语言变化的参数声明为可选且不写 `@default`，由 handler 通过 `value_or(TRU(...))` 补充。
4. 通过 `api::RegisterApi("ns.method", Handler)` 注册；deferred 方法使用 `api::RegisterApiDeferred`，含路径参数时在进入 handler 前执行与同步方法相同的权限检查，成功响应保留 `skippedPaths` 计数。未声明而直接通过 `BridgeCore::RegisterApi` 注册新方法时，`generate.mjs --check` 报错；例外只有 `registrations.mjs` 中 `UNDECLARED_RAW` 列出的 `menu.__*`、`test.echo` 与 `test.ping`。
5. 重新生成 SDK 类型，并在 `mcp/tests/` 中补充端到端测试。

### 事件

- 事件写在同一文件的 `export interface Events` 中：键为事件名冒号之后的部分，值为载荷类型；`@delivery` 标注投递范围（`broadcast`、`caller`、`owner`、`target`、`window`、`main`）。只有事件的命名空间（如 `app`、`metadb`）的文件只包含 `Events`。
- C++ 发射处使用 `src/api/EventEmit.h` 中的函数，只接受生成的载荷结构体，事件名取自生成的 `api::<ns>::events::<Event>`。
- `generate.mjs --check` 中的 `emits.mjs` 会拦下以下情况：已声明事件的名称以字面量出现在 `src/` 中、声明的事件没有任何发射处、以字面量名称发出却未声明的事件（菜单覆盖层私用、列在 `INTERNAL` 中的几个除外）。
- 文档页中每个事件对应一个 `### ns:key` 标题与一对空的 `<!-- api-schema:begin event:ns:key -->` 标记。
- SDK 的事件类型只由声明生成，`sdk/src/types/overrides/` 不包含事件。
