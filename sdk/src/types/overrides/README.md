# `sdk/src/types/overrides/`

Hand-written value types that sit next to the generated ones:

- `audio.ts` — the frame shapes of the `audio:spectrum` event and the value
  unions they share with the `audio.*` parameters.
- `config.ts` — the ReplayGain source-mode unions and the
  `REPLAYGAIN_SOURCE_MODE` constant.

They are ordinary modules. `sdk/src/types/responses.ts` imports and
re-exports them; the type generator does not read this directory.

The params, response and event payload types of every declared method and
event come only from the declarations under `src/api/schema/`, through
`scripts/gen_sdk_types.mjs`. A narrower or friendlier view of a generated type
(a discriminated union, an alias under an earlier name) is written in
`sdk/src/types/responses.ts` or `sdk/src/types/events.ts` on top of the
generated type, never as a second definition of it.
