# Security reference

This component is a specialized, vertical foobar2000 UI host. Themes come from the user or other trusted sources; installing a theme has roughly the same trust boundary as installing a foobar2000 component.

Security goals are therefore **fail-safe** (prevent a buggy theme from damaging the system), not a full **sandbox** against untrusted code.

## Threat model

- Primary guardrails are PathSecurity and protocol restrictions.
- `shell.exec` / `shell.spawn` intentionally do **not** maintain an executable whitelist.
- Path-bearing Bridge APIs still go through decorator validation and return `PERMISSION_DENIED` when the path is refused, `INVALID_PARAMS` when the parameter has the wrong shape or type.

## Which pages can call the API

A page can call the host and receive its events only while the top-level page is:

- on `https://foo-ui-webview2.local`, the virtual host that serves local templates;
- a page the host wrote itself, such as the built-in page and the menu overlay;
- on an origin the host navigated this window or panel to: the development server, a panel's configured URL, or the `http://` or `https://` URL of a popup whose opener already trusted that origin.

Addresses are compared by scheme, host and port after parsing, never as text. A URL that carries a user name or password, such as `https://foo-ui-webview2.local:1@example.com/`, is a page on `example.com` and is not trusted. `file://` pages are never trusted.

A page outside this list still shows, but each call fails at once with `ORIGIN_DENIED`, and neither events nor answers to earlier calls reach it. The same applies the moment a trusted page navigates itself somewhere else.

Real file paths in drag and drop go to fewer pages still: the virtual host above, and, while it is on, the development server at the address entered on the preferences page.

## shell.exec

- No executable command whitelist.
- If `cwd` is provided, it is path-checked and rejected when out of policy.

## shell.spawn

- No executable whitelist.
- Parameterized launch avoids string concatenation into a shell command.
- Optional `waitForExitMs` can detect early process exit.
- Absolute executable paths and `cwd` are path-checked.

## shell.openWith

Blocked extensions (29):

`.exe .com .cmd .bat .ps1 .vbs .vbe .js .jse .wsf .wsh .msc .scr .pif .hta .cpl .msi .msp .msu .dll .ocx .sys .drv .lnk .url .reg .inf .jar .application`

## file.read

Blocked system-drive directories include:

- `C:\\Windows\\`
- `C:\\Program Files\\`
- `C:\\Program Files (x86)\\`
- `C:\\ProgramData\\`

Non-system drives are generally allowed for NAS / portable layouts.

## file.write

`file.write`, `file.delete`, `file.mkdir`, the destination of `file.copy` and both ends of `file.move` are checked at the `FileWrite` level. System directories are always refused. Past that, a path is accepted when it is in the foobar2000 profile or temporary directory, inside a media-library watch folder, anywhere on a non-system drive (a drive letter; a UNC path does not count), or when it is a library or playlist track. This is the widest write channel a theme has; the permissions page lists every level.

## http.get / http.post

SSRF protections reject:

- `localhost` / `127.x.x.x`
- `192.168.x.x`
- `10.x.x.x`
- `172.16-31.x.x`
- `169.254.x.x`
- `::1`

::: tip Enabling local-network access
**Preferences → Advanced → Tools → WebView UI → Allow local network access**
:::

## DevTools

Disabled by default. Enable via:

**Preferences → Display → WebView2 UI → Developer → Enable Developer Tools (F12)**, press **Apply**, then restart foobar2000.

## Related runtime tokens

- `file.read`
- `file.write`
- `http.get`
- `http.post`
- `shell.exec`
- `shell.openWith`
- `shell.spawn`
- `PERMISSION_DENIED`
- `INVALID_PARAMS`
