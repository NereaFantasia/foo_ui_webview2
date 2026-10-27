# How the components work

The `fb-*` custom elements from `foo-webview-sdk/components` are standard Web Components (Custom Elements) such as `<fb-play-button>` and `<fb-playlist-view>`. Each element encapsulates a player control or view model, managing native Bridge communications and state subscriptions internally. A theme simply places them in markup and applies CSS styling, without manually binding endpoint calls or event listeners.

## They bring behavior, not looks

A component's shadow DOM enforces strict style encapsulation and structural layout only. It declares no opinionated colors, backgrounds, typography, borders, margins, shadows, or transition animations (an unstyled `fb-seek-bar` renders no visible color blocks). The theme dictates all visual styling through three mechanisms:

- **CSS parts**: internal elements carry a `part` name, styled from outside with `::part()`, for example `fb-seek-bar::part(fill)`.
- **Host attributes**: state is mirrored as attributes on the element itself, such as `[playing]` on `<fb-play-button>`, `[active]` on `<fb-shuffle-button>` and `[muted]` on `<fb-volume-control>`, so CSS can react to it.
- **Slots**: replaceable content, such as the play and pause icons of `<fb-play-button>`, can be swapped for the theme's own markup.

Keeping looks out of the components is what lets one set of elements fit any theme; [Style the components](/how-to/style-components) shows the hooks in use.

## They report, the theme decides

User interactions dispatch outward as `CustomEvent`s named `fb-` followed by kebab-case (for example `fb-play` or `fb-seek`). These events configure `bubbles: true` and `composed: true`, allowing them to escape shadow DOM boundaries and be caught by standard listeners on `document`.

Right-clicking never triggers built-in menus directly. Instead, view components dispatch dedicated context events containing pointer coordinates and target item metadata (`fb-track-context` from `<fb-playlist-view>`, `fb-playlist-context` from `<fb-playlist-tabs>`, `fb-queue-context` from `<fb-queue-view>`, `fb-library-context` from `<fb-library-tree>`), enabling the theme to trigger custom popup menus via `fb.notification.showCustomMenu()`.

## They use the theme's SDK

Web Components never establish an independent Bridge connection; they reuse the SDK instance loaded by the theme:

- In bundler environments, `registerComponents()` from `foo-webview-sdk/components` binds custom elements to the SDK instance exported by `foo-webview-sdk`.
- In script-tag environments, `components.global.js` registers elements and connects them to `window.fb` initialized by `bridge.global.js`.

Components resolve the SDK upon attachment to the DOM, making script inclusion order flexible. If no SDK instance is available, the component raises an informative initialization error.

When host calls fail (such as clicking play outside foobar2000), the element preserves its visual state rather than emitting unhandled exceptions into the host page.

## Related

- [Components reference](/components/)
- [Load the SDK in a theme](/how-to/load-sdk)
