# Versioning and Compatibility

Starting with 2.0.0, foo_ui_webview2 follows [Semantic Versioning](https://semver.org/). Version numbers take the form `MAJOR.MINOR.PATCH`: minor releases add backwards-compatible features, patch releases provide backwards-compatible bug fixes, and breaking changes only occur in major releases.

## Compatibility Commitments for 2.x

- Existing methods, events, parameters, and response fields are never removed, renamed, or changed in type.
- Optional parameters are never changed into required ones, and identical failure scenarios return consistent error codes.
- New capabilities arrive strictly as new methods, events, optional parameters, or response fields.
- Breaking changes are introduced only in the next major release (3.0.0), accompanied by migration guides in the changelog.
- Discrepancies between runtime behavior and documentation contracts are resolved to match documentation facts. Such bug fixes are not treated as breaking changes, but are highlighted in the changelog.

## Deprecation Policy {#deprecation}

APIs marked as deprecated remain functional and supported throughout the entire 2.x lifecycle and are removed only in the next major release. SDK types mark them with `@deprecated`, and the documentation provides recommended replacements.

## Experimental APIs

APIs marked experimental in the documentation and SDK types (`@experimental`) are exempt from these compatibility commitments and may change across minor releases; all changes are recorded in the changelog.

## Version Matching

- The SDK (`foo-webview-sdk`) shares the plugin's major version: use 2.x SDK releases with 2.x plugin builds.
- Newly introduced APIs document their minimum required version. Themes can query `plugin.version` via `config.getVersionInfo()` to verify host runtime capabilities.
- The host rejects undeclared parameters: when an older 2.x host receives unknown parameters, calls fail explicitly with `INVALID_PARAMS` rather than silently ignoring them.
- The MCP server (`foo-ui-webview2-mcp`) uses independent versioning and makes no compatibility commitments during 0.x.

## 1.x Legacy Branch {#_1-x}

The final release of the 1.x series is v1.14.0; it receives no further maintenance or security updates. 1.x lacks complete origin validation (see the known issues for v1.14.0 in the [changelog](/changelog)). Themes still running on 1.x should migrate following the v2.0.0 breaking change notes.
