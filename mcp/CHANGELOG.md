# Changelog

All notable changes to foo-ui-webview2-mcp will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-06-20

### Added

- Initial public release of the `foo-ui-webview2-mcp` MCP server.
- Connects to the foobar2000 `foo_ui_webview2` WebView2 instance over the Chrome
  DevTools Protocol (CDP, default `localhost:9222`) and exposes the bridge API as
  Model Context Protocol (MCP) tools over stdio.
- Ships the `foo-ui-webview2-mcp` CLI binary, runnable via `npx -y foo-ui-webview2-mcp`.
- Configurable connection via `FB2K_CDP_HOST` / `FB2K_CDP_PORT` environment variables.
