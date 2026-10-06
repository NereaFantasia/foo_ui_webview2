English | [中文](./CONTRIBUTING.zh-CN.md)

# Contributing

This guide describes how to develop the component itself: the C++ host, the TypeScript SDK, the MCP server and the documentation site. Theme development needs only the SDK from npm; see the [documentation site](https://nereafantasia.github.io/foo_ui_webview2/).

All commands run in PowerShell from the repository root. CI runs the same checks as the commands listed here.

## Requirements

- Visual Studio 2026 with the "Desktop development with C++" workload (v145 toolset), or Visual Studio 2022 (v143 toolset; pass `-PlatformToolset v143` when building).
- Windows SDK 10.0.22621 or later.
- Node.js 22.
- Git.

Install the JavaScript dependencies:

```powershell
npm ci --prefix sdk
npm ci --prefix mcp
npm install --prefix docs/vitepress
```

- The documentation site keeps no lock file, so it uses `npm install`.
- The C++ build also depends on `sdk/node_modules`. When it is installed, the build first regenerates the API headers from the declarations; otherwise the build warns and uses the committed headers.
- On the first build, `build.ps1` clones the Columns UI SDK into `lib/columns_ui-sdk` and restores the NuGet packages.

## Repository layout

- `src/`: the C++ component. `src/api/` contains the bridge and one `XxxApi.cpp` per namespace.
- `src/api/schema/`: the API declarations, covering methods, parameters, results and events, one file per namespace.
- `sdk/`: the TypeScript SDK, published as `foo-webview-sdk`.
- `mcp/`: the MCP server, published as `foo-ui-webview2-mcp`. `mcp/tests/` also contains the live end-to-end suites.
- `tests/`: the C++ unit tests (GoogleTest).
- `docs/vitepress/`: the documentation site, with English at the root and Chinese under `zh/`.
- `scripts/`: the generators and checks used by the build, CI and the checks in this guide.

## Generated files

The following files are generated from `src/api/schema/` and must not be edited by hand. To change them, edit the declarations and regenerate:

- `src/api/generated/`
- `sdk/src/types/generated/` and `sdk/src/components/generated/`
- `mcp/src/generated/`, and the tool tables in `mcp/README.md` and `mcp/README.zh-CN.md`
- The blocks between `<!-- api-schema:begin … -->` and `<!-- api-schema:end -->` in the documentation pages

`src/window/MenuOverlayPage.inl` is generated from `src/window/menu-overlay/` by `scripts/gen_menu_overlay_page.mjs` and must not be edited by hand either.

To give a generated SDK type a different shape, build on it in `sdk/src/types/responses.ts` or `events.ts`. Hand-written value types belong in `sdk/src/types/overrides/`.

## Changing an API or event

Complete the steps in order:

1. Add or change the declaration in `src/api/schema/<namespace>.ts`, including parameters, results, the path security level and event payloads. The declaration syntax is described in `src/api/schema/README.md` (in Chinese).
2. Regenerate the C++ headers, documentation blocks, MCP tools and SDK types:

   ```powershell
   node scripts/api-schema/generate.mjs --write
   npm --prefix sdk run gen:types
   ```

3. Write the implementation:
   - Register methods with `api::RegisterApi` or `api::RegisterApiDeferred`. On failure, return `api::Fail(error, code, extra)` from `src/api/ApiResult.h`.
   - Emit events with the functions in `api::emit` (`src/api/EventEmit.h`).
   - Inside the SDK, call methods with `call()` and subscribe to events with `subscribe()` (`_sub()` in components), not with `bridge.invoke` or `bridge.on`.
   - Register a method that cannot be declared, such as one that echoes arbitrary parameters, with `BridgeCore::RegisterUndeclaredApi`, and add it to `UNDECLARED_RAW` in `scripts/api-schema/registrations.mjs`.
4. Update the documentation. The generated blocks cover parameters and results only; write the remaining text in both `docs/vitepress/` and `docs/vitepress/zh/`.
5. Run the [checks](#checks).

## Naming conventions

The following four formats must not be mixed:

- Methods use dots, for example `playback.play`.
- Events sent by the host use colons, for example `playback:stateChanged`.
- Component CustomEvents use the `fb-` prefix with kebab-case, for example `fb-track-context`.
- Host attributes and CSS parts use kebab-case, for example `row-height` and `mute-button`.

A page receives a subscribed event only after it calls the method that starts the data flow.

## C++ conventions

- Build with `build.ps1`, which locates MSBuild through vswhere.
- Do not remove `/utf-8` from the project's compiler options. Source files with non-ASCII comments use UTF-8 with BOM.
- Every `.cpp` file must include `"pch.h"` first. The precompiled header is created by `src/pch.cpp`; do not change these settings.
- Add new files to both `foo_ui_webview2.vcxproj` and `foo_ui_webview2.vcxproj.filters`.
- Verify signatures in `lib/foobar2000_sdk/` before calling the foobar2000 SDK.
- The main and test projects treat compiler warnings as errors.

## Checks

The following commands match the checks in `.github/workflows/ci.yml`.

### Component

```powershell
.\build.ps1 -Config Release -Platform x64
.\build.ps1 -Config Release -Platform x64 -PlatformToolset v143
```

- The x64 build runs the GoogleTest suite after building.
- CI builds with the v143 toolset. When macros, raw string literals or long literals change, some code compiles under v145 but fails under v143; build once with v143 before submitting.

### Generators and SDK

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

`npm --prefix sdk run build` checks the contents of `sdk/dist` after building.

### MCP server

```powershell
npm --prefix mcp run type-check
npm --prefix mcp test
```

### Documentation

```powershell
npm --prefix docs/vitepress run check:examples
npm --prefix docs/vitepress run check:api-reference-facts
npm --prefix docs/vitepress run check:sdk:root
npm --prefix docs/vitepress run check:sdk:zh
node docs/vitepress/.vitepress/published-urls.mjs --check
node scripts/gen_sdk_doc_signatures.mjs
npm --prefix docs/vitepress run build
```

- The examples check compiles the code blocks in the documentation against the SDK types, so build the SDK first.
- A code block that is not meant to run can be marked `@ts-nocheck` in its fence, for example ` ```js @ts-nocheck `.

## Live tests

The checks above cannot confirm that a handler behaves as declared, including its error codes, side effects and the windows that receive its events. These must be verified against a running foobar2000.

`scripts/live-e2e/live.mjs` deploys the newly built DLL to a foobar2000 instance, runs the suites in `mcp/tests/` and restores the instance afterwards. The setup is described in `scripts/live-e2e/README.md` (in Chinese). The instance is restarted during the tests, so use a separate instance from the one used for everyday listening.

## Pull requests

- Each pull request contains one change, with the declaration, generated files and implementation in the same commit.
- The description lists the checks that were run and those that were not (for example, live tests).
- Comments in `sdk/src/` ship with the npm package. They must be in English, must not contain TODOs or internal task references, and every exported symbol needs at least a one-line JSDoc summary.
