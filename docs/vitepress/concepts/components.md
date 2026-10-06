# How the components work

The `fb-*` elements from `foo-webview-sdk/components` are Web Components: custom HTML tags such as `<fb-play-button>` or `<fb-playlist-view>`, each wrapping one control or view of the player. They talk to foobar2000 themselves, so a theme places them in its markup and styles them, without wiring up calls and events for each one.

## They bring behavior, not looks

A component's shadow DOM carries only structural CSS: layout, sizing, positioning and the like. It sets no colours, backgrounds, fonts, borders, margins, shadows or transitions, so an unstyled `fb-seek-bar` draws nothing visible. The theme decides how everything looks, through three hooks:

- **CSS parts**: internal elements carry a `part` name, styled from outside with `::part()`, for example `fb-seek-bar::part(fill)`.
- **Host attributes**: state is mirrored as attributes on the element itself, such as `[playing]` on `<fb-play-button>`, `[active]` on `<fb-shuffle-button>` and `[muted]` on `<fb-volume-control>`, so CSS can react to it.
- **Slots**: replaceable content, such as the play and pause icons of `<fb-play-button>`, can be swapped for the theme's own markup.

Keeping looks out of the components is what lets one set of elements fit any theme; [Style the components](/how-to/style-components) shows the hooks in use.

## They report, the theme decides

User actions leave a component as `CustomEvent`s named `fb-` plus kebab case, such as `fb-play` or `fb-seek`, which bubble and cross the shadow DOM boundary, so a listener on `document` catches them. A right-click does not open a menu: views dispatch a context event with the pointer position and the item under it (`fb-track-context` from `<fb-playlist-view>`, `fb-playlist-context` from `<fb-playlist-tabs>`, `fb-queue-context` from `<fb-queue-view>`, `fb-library-context` from `<fb-library-tree>`), and the theme shows whatever menu it wants, for example with `fb.notification.showCustomMenu()`.

## They use the theme's SDK

Components never create a bridge of their own; they use the SDK instance the theme already loaded. With a bundler, `registerComponents()` from `foo-webview-sdk/components` binds them to the instance that `foo-webview-sdk` exports. With `<script>` tags, `components.global.js` registers them and they use the `window.fb` that `bridge.global.js` installs. A component looks the SDK up when it is first added to the page, so the order of the two script tags does not matter. If neither is there, the component throws an error saying how to load the SDK.

A call that fails, such as a play button clicked with no host, leaves the element as it was rather than throwing into the page.

## Related

- [Components reference](/components/)
- [Load the SDK in a theme](/how-to/load-sdk)
