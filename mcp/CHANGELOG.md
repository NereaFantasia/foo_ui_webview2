# Changelog

All notable changes to foo-ui-webview2-mcp will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.1] - 2026-07-25

### Changed

- **Behavior change** — a bridge handler response with `success: false` is now
  reported as an MCP tool error instead of a successful tool result. The host
  `code` and `details` fields are preserved in the returned error content.
  Clients that previously treated every response as success must handle the
  error path.
- Raised the minimum `@modelcontextprotocol/sdk` requirement to 1.23.0, which
  is needed for complete Zod 4 object schema registration.

### Fixed

- Enforce declared tool parameter constraints, including numeric bounds, array
  element schemas, nested required fields, defaults, and open metadata tag
  objects. Invalid arguments are rejected instead of being forwarded to the
  bridge.
- Reject circular or prototype-sensitive tool declarations before the server
  starts, and reject non-JSON, circular, sparse, accessor-backed, or
  prototype-sensitive defaults.
- Reject prototype-sensitive runtime argument keys before MCP SDK normalization
  or bridge invocation.
- Parse stdio JSON through a guarded transport ahead of the MCP SDK schema
  pass, returning `-32602` only for unsafe requests while reporting unsafe
  notifications and responses without generating response-to-response traffic.

## [0.1.0] - 2026-06-20

### Added

- Initial public release of the `foo-ui-webview2-mcp` MCP server.
- Connects to the foobar2000 `foo_ui_webview2` WebView2 instance over the Chrome
  DevTools Protocol (CDP, default `localhost:9222`) and exposes the bridge API as
  Model Context Protocol (MCP) tools over stdio.
- Ships the `foo-ui-webview2-mcp` CLI binary, runnable via `npx -y foo-ui-webview2-mcp`.
- Configurable connection via `FB2K_CDP_HOST` / `FB2K_CDP_PORT` environment variables.
