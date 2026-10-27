# Security model and trust boundaries {#security-reference}

This component functions as a specialized foobar2000 UI host. Themes originate from the user or trusted community sources; installing a theme shares substantially the same trust model as installing a native foobar2000 component.

Security goals are therefore geared toward **fail-safe protection** (preventing unintentional defects in themes from altering protected system assets), rather than full hostile **sandboxing** against untrusted code.

## Threat model

- Primary security guardrails rely on PathSecurity specifications and origin validation rules.
- `shell.exec` and `shell.spawn` intentionally omit an executable file whitelist: interpreters such as `cmd /c` would bypass it, so it cannot serve as a security boundary.
- Path-bearing Bridge APIs enforce decorator-based validation, returning `PERMISSION_DENIED` when paths violate policy and `INVALID_PARAMS` on malformed inputs.

## Which pages can call the API

A page can call the host and receive its events only while the top-level page is:

- on `https://foo-ui-webview2.local`, the virtual host that serves local templates;
- a page the host wrote itself, such as the built-in page and the menu overlay;
- on an origin the host navigated this window or panel to: the development server; a panel's configured URL, when it is on this computer or that site is allowed in the panel configuration; or the `http://` or `https://` URL of a popup whose opener already trusted that origin.

On this computer means `localhost`, a `127.x.x.x` address or `[::1]`. The development server's origin is trusted in full, and a trusted page can change the development server settings with `window.setDevServerConfig`; the change applies the next time a page loads.

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

The foobar2000 profile directory is exempt: a portable installation inside `C:\Program Files\` can still read and write its own profile.

Non-system drives are generally allowed for NAS / portable layouts.

## file.write

`file.write`, `file.delete`, `file.mkdir`, the destination of `file.copy` and both ends of `file.move` are checked at the `FileWrite` level. System directories are refused, except the foobar2000 profile directory. Past that, a path is accepted when it is in the foobar2000 profile or temporary directory, inside a media-library watch folder, anywhere on a non-system drive (a drive letter; a UNC path does not count), or when it is a library or playlist track. This is the widest write channel a theme has; the permissions page lists every level.

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
