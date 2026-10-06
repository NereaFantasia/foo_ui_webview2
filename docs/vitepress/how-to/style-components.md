# Style the components

The `fb-*` elements draw almost nothing until a theme styles them. Each component's reference entry lists its parts, slots and host attributes; this page shows how to use the three kinds. [How the components work](/concepts/components) explains why they come unstyled.

## Style the inner elements with `::part()`

Select a part of the element from your stylesheet:

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

`::part()` reaches only the named element; descendants of a part are out of reach, so style what the part itself shows.

## React to state with host attributes

Components mirror their state as attributes on the element. Combine them with `::part()`:

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

`::part()` combines with host attributes, as above, and with user-action pseudo-classes such as `:hover` and `:focus-visible`. An attribute selector after `::part()` is not valid CSS, so state that a component marks on an inner element, such as `data-filled` on the stars of `<fb-rating>`, cannot be selected from the page.

## Replace content with slots

Put your own markup in a named slot to replace the default content:

```html
<fb-play-button>
    <svg slot="play-icon" viewBox="0 0 24 24" width="20" height="20"><path d="M8 5v14l11-7z" /></svg>
    <svg slot="pause-icon" viewBox="0 0 24 24" width="20" height="20"><path d="M6 5h4v14H6zm8 0h4v14h-4z" /></svg>
</fb-play-button>
```

The component still decides which slot is visible; here, the pause icon while playing.

## Keep the styles in one place

Component styles are ordinary page CSS, so they live in the theme's stylesheet next to everything else, and a CSS custom property set on `:root` (an accent colour, say) reaches them like any other rule. [Build your first theme](/tutorials/first-theme) styles a full player this way.
