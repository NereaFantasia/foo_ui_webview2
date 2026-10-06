[English](./CONTRIBUTING.md) | 中文

# 贡献指南

本文说明组件本身的开发流程，涵盖 C++ 宿主、TypeScript SDK、MCP 服务器与文档站。仅开发主题时，从 npm 安装 SDK 即可，详见[文档站](https://nereafantasia.github.io/foo_ui_webview2/zh/)。

文中命令均在仓库根目录的 PowerShell 中执行。CI 运行的检查与本文所列命令一致。

## 环境要求

- Visual Studio 2026，含「使用 C++ 的桌面开发」工作负载（v145 工具集）；或 Visual Studio 2022（v143 工具集，构建时须传 `-PlatformToolset v143`）。
- Windows SDK 10.0.22621 或更高版本。
- Node.js 22。
- Git。

安装 JavaScript 依赖：

```powershell
npm ci --prefix sdk
npm ci --prefix mcp
npm install --prefix docs/vitepress
```

- 文档站不保留 lock 文件，因此使用 `npm install`。
- C++ 构建同样依赖 `sdk/node_modules`：安装后，构建会先按声明重新生成 API 头文件；未安装时，构建给出警告并使用已提交的头文件。
- 首次构建时，`build.ps1` 将 Columns UI SDK 克隆至 `lib/columns_ui-sdk`，并还原 NuGet 包。

## 仓库结构

- `src/`：C++ 组件。`src/api/` 为桥接层，每个命名空间对应一个 `XxxApi.cpp`。
- `src/api/schema/`：API 声明，包括方法、参数、返回值与事件，每个命名空间一个文件。
- `sdk/`：TypeScript SDK，以 `foo-webview-sdk` 发布。
- `mcp/`：MCP 服务器，以 `foo-ui-webview2-mcp` 发布。`mcp/tests/` 另含实机端到端套件。
- `tests/`：C++ 单元测试（GoogleTest）。
- `docs/vitepress/`：文档站，英文位于根目录，中文位于 `zh/`。
- `scripts/`：构建、CI 与本文各项检查所用的生成器与检查脚本。

## 生成文件

以下文件由 `src/api/schema/` 生成，不得手动修改。如需修改，请修改声明后重新生成：

- `src/api/generated/`
- `sdk/src/types/generated/` 与 `sdk/src/components/generated/`
- `mcp/src/generated/`，以及 `mcp/README.md`、`mcp/README.zh-CN.md` 中的工具表
- 文档页中 `<!-- api-schema:begin … -->` 与 `<!-- api-schema:end -->` 之间的区块

`src/window/MenuOverlayPage.inl` 由 `scripts/gen_menu_overlay_page.mjs` 根据 `src/window/menu-overlay/` 生成，同样不得手动修改。

如需调整生成的 SDK 类型的形式，请在 `sdk/src/types/responses.ts` 或 `events.ts` 中基于生成类型编写；手写的值类型位于 `sdk/src/types/overrides/`。

## API 与事件的修改流程

各步骤须按顺序完成：

1. 在 `src/api/schema/<命名空间>.ts` 中新增或修改声明，包括参数、返回值、路径安全级别与事件载荷。声明的写法见 `src/api/schema/README.md`。
2. 重新生成 C++ 头文件、文档区块、MCP 工具与 SDK 类型：

   ```powershell
   node scripts/api-schema/generate.mjs --write
   npm --prefix sdk run gen:types
   ```

3. 编写实现：
   - 方法通过 `api::RegisterApi` 或 `api::RegisterApiDeferred` 注册，失败时返回 `src/api/ApiResult.h` 中的 `api::Fail(error, code, extra)`。
   - 事件通过 `api::emit`（`src/api/EventEmit.h`）中的函数发出。
   - SDK 内部通过 `call()` 调用方法，通过 `subscribe()`（组件中为 `_sub()`）订阅事件，不直接调用 `bridge.invoke` 或 `bridge.on`。
   - 无法声明的方法（例如原样返回任意参数的方法）通过 `BridgeCore::RegisterUndeclaredApi` 注册，并须登记到 `scripts/api-schema/registrations.mjs` 的 `UNDECLARED_RAW`。
4. 补充文档。生成的区块只包含参数与返回值，其余说明须在 `docs/vitepress/` 与 `docs/vitepress/zh/` 中同时编写。
5. 完成[检查](#检查)。

## 命名约定

以下四种格式不得混用：

- 方法使用点号，例如 `playback.play`。
- 宿主发出的事件使用冒号，例如 `playback:stateChanged`。
- 组件的 CustomEvent 使用 `fb-` 前缀加 kebab-case，例如 `fb-track-context`。
- 宿主属性与 CSS Part 使用 kebab-case，例如 `row-height`、`mute-button`。

页面须先调用启动数据流的方法，订阅的事件才会送达。

## C++ 约定

- 使用 `build.ps1` 构建，该脚本通过 vswhere 查找 MSBuild。
- 工程编译选项中的 `/utf-8` 不得移除。含非 ASCII 注释的源文件使用 UTF-8 with BOM 编码。
- 每个 `.cpp` 文件须首先包含 `"pch.h"`。预编译头由 `src/pch.cpp` 创建，相关设置不得修改。
- 新增文件须同时加入 `foo_ui_webview2.vcxproj` 与 `foo_ui_webview2.vcxproj.filters`。
- 调用 foobar2000 SDK 前，须在 `lib/foobar2000_sdk/` 中核对签名。
- 主工程与测试工程均将编译警告视为错误。

## 检查

以下命令与 `.github/workflows/ci.yml` 中的检查一致。

### 组件

```powershell
.\build.ps1 -Config Release -Platform x64
.\build.ps1 -Config Release -Platform x64 -PlatformToolset v143
```

- x64 构建完成后会运行 GoogleTest 测试。
- CI 使用 v143 工具集构建。修改宏、原始字符串字面量或较长的字面量时，部分代码在 v145 下可以编译而在 v143 下失败，提交前须使用 v143 构建一次。

### 生成器与 SDK

```powershell
node scripts/api-schema/generate.mjs --check
node scripts/gen_sdk_types.mjs --all --diff
node scripts/gen_sdk_components_global.mjs --diff --quiet
node scripts/gen_menu_overlay_page.mjs --check
node scripts/audit_invoke_typing.mjs --strict
node scripts/audit_index_signature.mjs --strict
npm --prefix sdk run type-check
npm --prefix sdk test
npm --prefix sdk run build
node --test "scripts/__tests__/*.test.mjs"
```

`npm --prefix sdk run build` 在构建后检查 `sdk/dist` 的内容。

### MCP 服务器

```powershell
npm --prefix mcp run type-check
npm --prefix mcp test
```

### 文档

```powershell
npm --prefix docs/vitepress run check:examples
npm --prefix docs/vitepress run check:api-reference-facts
npm --prefix docs/vitepress run check:sdk:root
npm --prefix docs/vitepress run check:sdk:zh
node docs/vitepress/.vitepress/published-urls.mjs --check
node scripts/gen_sdk_doc_signatures.mjs
npm --prefix docs/vitepress run build
```

- 示例检查依据 SDK 的类型编译文档中的代码块，运行前须先构建 SDK。
- 不需要运行的代码块，可在代码围栏上标注 `@ts-nocheck`，例如 ` ```js @ts-nocheck `。

## 实机测试

上述检查无法确认 handler 的实际行为是否与声明一致，包括失败码、副作用以及事件送达的窗口，这些须在运行中的 foobar2000 上验证。

`scripts/live-e2e/live.mjs` 将刚构建的 DLL 部署到 foobar2000 实例，运行 `mcp/tests/` 中的套件，并在结束后恢复实例状态。准备步骤见 `scripts/live-e2e/README.md`。测试过程中实例会被重启，请勿使用日常播放所用的实例。

## Pull Request

- 每个 Pull Request 只包含一项改动；声明、生成文件与实现须位于同一个提交中。
- 说明中须列出已运行与未运行的检查（例如实机测试）。
- `sdk/src/` 中的注释随 npm 包发布，须使用英文，不得包含 TODO 或内部任务编号，每个导出符号须有至少一行 JSDoc 摘要。
