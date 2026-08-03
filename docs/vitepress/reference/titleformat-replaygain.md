# Titleformat and ReplayGain reference

Cross-page reference for title formatting and ReplayGain helpers. Primary owners remain:

- [Titleformat API](/api/titleformat)
- [Audio / ReplayGain API](/api/audio)

## titleformat.eval

Evaluate one titleformat expression for one file.


## titleformat.evalBatch

Evaluate the same expression across many files.


## titleformat.evalFields

Evaluate multiple named fields for one file. Preferred for structured UIs.


## titleformat.evalFieldsBatch

Evaluate multiple named fields across many files. Useful for custom list columns.


## titleformat.getBuiltinFields

Returns a convenience map of built-in field expressions.

Typical uses:

- custom list columns (`%play_count%`, `%rating%`, `%added%`)
- conditional display such as `$if(%album%,%album%,Unknown)`
- optional foo_playcount fields when the component is installed


## replaygain.get

Read ReplayGain data for one or more paths (`trackGain`, `trackPeak`, `albumGain`, `albumPeak`, plus per-item status fields from the handler).


## replaygain.scan

Trigger ReplayGain scanning for the provided paths (via the context-menu workflow exposed by the handler).


## replaygain.clear

Clear ReplayGain data for the provided paths.


## Related configuration APIs

Mode/preamp helpers live on the Audio owner page:

- `replaygain.getMode`
- `replaygain.setMode`
- `replaygain.getPreamp`
- `replaygain.setPreamp`
- `replaygain.getSettings`
