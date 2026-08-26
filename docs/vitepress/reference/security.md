# Security reference

This component is a specialized, vertical foobar2000 UI host. Themes come from the user or other trusted sources; installing a theme has roughly the same trust boundary as installing a foobar2000 component.

Security goals are therefore **fail-safe** (prevent a buggy theme from damaging the system), not a full **sandbox** against untrusted code.

## Threat model

- Primary guardrails are PathSecurity and protocol restrictions.
- `shell.exec` / `shell.spawn` intentionally do **not** maintain an executable whitelist.
- Path-bearing Bridge APIs still go through decorator validation and return `PERMISSION_DENIED` when the path is refused, `INVALID_PARAMS` when the parameter has the wrong shape or type.

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

Write destinations must pass PathSecurity write policy. In practice this means the foobar2000 profile directory and the system temporary directory.

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

**Preferences → Advanced → Tools → WebView UI → Enable DevTools**, then restart foobar2000.

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
