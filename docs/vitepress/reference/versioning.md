# Versioning and compatibility

Starting with 2.0.0, foo_ui_webview2 follows [Semantic Versioning](https://semver.org/). Version numbers take the form MAJOR.MINOR.PATCH: minor releases add features, patch releases fix problems, and only major releases contain breaking changes.

## Compatibility commitments for 2.x

- Existing methods, events, parameters and response fields will not be removed, renamed or change type.
- Optional parameters will not become required, and a call fails with the same error code in the same situation.
- New features arrive as new methods, events, parameters or fields.
- Breaking changes ship only in the next major release (3.0.0), and the changelog will describe how to migrate.
- Behavior that does not match the documentation is fixed to match it. Such fixes are not treated as breaking changes, but the changelog calls them out separately.

## Deprecation

A deprecated API keeps working throughout 2.x and is removed only in the next major release. The SDK types mark it with `@deprecated`, and the documentation names the replacement.

## Experimental APIs

APIs marked experimental in the documentation and in the SDK types (`@experimental`) are not covered by these commitments and may change in a minor release; any change is recorded in the changelog.

## Version matching

- The SDK (`foo-webview-sdk`) shares the plugin's major version: use a 2.x SDK with a 2.x plugin.
- New APIs are documented with the first version that provides them. A theme can check `plugin.version` from `config.getVersionInfo()` to tell whether the current host supports them.
- The host rejects undeclared parameters: when an older 2.x host receives a parameter it does not know, the call fails with `INVALID_PARAMS` instead of ignoring it.
- The MCP server (`foo-ui-webview2-mcp`) is versioned separately and makes no compatibility commitment while at 0.x.

## 1.x

The final release of the 1.x series is v1.14.0. 1.x receives no further updates, including security fixes. 1.x does not fully check where a page comes from; see the known issues of v1.14.0 in the [changelog](/changelog). Themes still on 1.x should migrate using the breaking changes of v2.0.0 listed there.
