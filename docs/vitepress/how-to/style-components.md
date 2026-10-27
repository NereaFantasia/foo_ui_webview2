# Style the components

The `fb-*` Web Components ship unstyled, rendering minimal layout placeholders until the host theme applies styling. Each component's reference page documents its supported CSS parts, slots, and host attributes; this guide illustrates how to use these three customization mechanisms. [How the components work](/concepts/components) details the design philosophy behind this separation.

## Style inner elements with `::part()` {#style-the-inner-elements-with-part}

Target designated Shadow DOM elements from your theme stylesheet using the standard `::part()` pseudo-element:

```css
/* A round play button */
fb-play-button::part(button) {
    width: 48px;
    height: 48px;
    border: none;
    border-radius: 50%;
    background: #1db954;
    color: white;
    cursor: pointer;
}
fb-play-button::part(button):hover {
    background: #1ed760;
}

/* The seek bar has a track, a fill and a thumb */
fb-seek-bar::part(track) {
    background: linear-gradient(#333, #333) center / 100% 4px no-repeat;
}
fb-seek-bar::part(fill) { height: 4px; background: #1db954; }
fb-seek-bar::part(thumb) { width: 12px; height: 12px; border-radius: 50%; background: #fff; }
```

`::part()` targets only the element carrying the matching `part` attribute inside the Shadow DOM; it cannot reach nested child elements. Apply visual properties directly to the exposed part itself.

## React to state with host attributes

Components mirror their internal state as host attributes on the element itself. Combine these attributes with `::part()` selectors:

```css
/* Highlight the play button while playing */
fb-play-button[playing]::part(button) {
    background: #ffffff;
    color: #1db954;
}

/* A dimmed volume control while muted */
fb-volume-control[muted] {
    opacity: 0.5;
}
```

Host attributes can be combined with `::part()` as shown above, alongside interactive pseudo-classes such as `:hover` and `:focus-visible`. Notice that attribute selectors appended after `::part()` (such as `::part(btn)[active]`) are invalid in CSS; state indicators placed on internal DOM nodes (such as `data-filled` on `<fb-rating>` stars) cannot be matched directly from page stylesheets.

## Replace content with slots

Supply custom markup within named slots to override default placeholder icons or controls:

```html
<fb-play-button>
    <svg slot="play-icon" viewBox="0 0 24 24" width="20" height="20"><path d="M8 5v14l11-7z" /></svg>
    <svg slot="pause-icon" viewBox="0 0 24 24" width="20" height="20"><path d="M6 5h4v14H6zm8 0h4v14h-4z" /></svg>
</fb-play-button>
```

The component retains control over which slot renders based on runtime state (here, rendering the pause icon while playing).

## Centralize theme stylesheets {#keep-the-styles-in-one-place}

Component styles are standard CSS declarations, organized within the theme's global stylesheets alongside other view rules. CSS custom properties declared on `:root` (such as accent colors and design tokens) cascade naturally into component parts. [Build your first theme](/tutorials/first-theme) demonstrates complete theme styling following this approach.
