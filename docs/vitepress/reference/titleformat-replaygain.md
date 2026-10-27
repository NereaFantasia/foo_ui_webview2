# Titleformat and ReplayGain Reference

Cross-topic reference for title formatting and ReplayGain helpers. Primary endpoint documentation:

- [Titleformat API](/api/titleformat)
- [ReplayGain API](/api/replaygain)

## titleformat.eval

Evaluates a single titleformat expression for one track.

## titleformat.evalBatch

Evaluates a single titleformat expression across multiple tracks.

## titleformat.evalFields

Evaluates multiple named field expressions for one track. Recommended for structured UI components.

## titleformat.evalFieldsBatch

Evaluates multiple named field expressions across multiple tracks, suitable for custom playlist columns.

## titleformat.getBuiltinFields

Returns a dictionary mapping common built-in titleformat field expressions.

Typical use cases:

- Custom list columns (`%play_count%`, `%rating%`, `%added%`)
- Conditional display expressions (such as `$if(%album%,%album%,Unknown)`)
- Optional `foo_playcount` statistics fields when the component is installed

## replaygain.get

Reads ReplayGain gain and peak data for one or more paths (`trackGain`, `trackPeak`, `albumGain`, and `albumPeak`, along with per-item status fields).

## replaygain.scan

Triggers a ReplayGain scan workflow for the specified paths via the host context-menu infrastructure.

## replaygain.clear

Clears ReplayGain metadata for the specified track paths.

## Related configuration APIs

ReplayGain mode and preamp configuration endpoints reside in the Audio API documentation:

- `replaygain.getMode`
- `replaygain.setMode`
- `replaygain.getPreamp`
- `replaygain.setPreamp`
- `replaygain.getSettings`
