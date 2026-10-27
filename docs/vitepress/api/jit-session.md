# JIT Session API

Provides explicit JIT playback sessions with lease control, supporting hybrid queue scheduling, native playback adoption, and dynamic entry prefetching.

::: warning Experimental
The methods of this namespace may still change. `jitQueue` stays as it is; the two share the host's execution, and a `jitQueue` call made during a session takes the session over.
:::

## How a session runs

1. Subscribe to the `jitSession:*` events, then call `jitSession.create`. Keep the `controlToken`; write commands carry it with the `sessionId`, the `revision` they are based on and a `requestId` of your choice.
2. `jitSession.play` starts an entry as a new attempt. The answer only means the command was accepted: `jitSession:operationCompleted` tells whether the entry was added and started, `jitSession:attemptFinished` how the attempt ended.
3. When an attempt starts with no next entry, `jitSession:needNext` asks for one; answer with `jitSession.setNext`. The same method replaces the next entry, withdraws it (without `entryId`) or declares the end of the queue.
4. After a page reload, call `jitSession.discover` and, when `canResume` is true, `jitSession.claimControl` with `mode: 'resume'`. Playback does not restart. Another page takes control with `mode: 'handover'` and the current `ownerEpoch`.
5. `jitSession.release` ends the session: the buffer is emptied and the track playing keeps decoding.

Entries are identified by the caller's `entryId`; each start of an entry, a replay included, gets a new `attemptId` from the host. Events reach only the page holding control; a page that missed some reads `jitSession.getState`.

## jitSession

### jitSession.getCapabilities

<!-- api-schema:begin jitSession.getCapabilities -->
Experimental API; it may change in future releases.

Report what this host's JIT sessions can and cannot do, so a coordinator can plan around the limits instead of discovering them in playback.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `protocolVersion` | `integer` | Version of the session protocol. |
| `adoption` | `boolean` | Whether a session can take over native playback without interrupting it, through `jitSession.prepareAdoption` and `jitSession.adoptNative`. This host supports it; `jitSession.prepareAdoption` decides whether the playback at hand qualifies. |
| `preciseFailure` | `boolean` | Whether attempt failures are classified (network, decoding). Not available: foobar2000 reports no structured playback error. See `failureRecheck` for the best-effort substitute. |
| `failureRecheck` | `boolean` | Whether an attempt that ends as `unknown` within its first second is followed by `jitSession:attemptRechecked`. |
| `backgroundExecution` | `boolean` | Whether the controlling page keeps answering `jitSession:needNext` while its window is minimized, hidden to the tray or the screen is locked. This host supports it: while a session is active, its page is hidden but not suspended. Closing or reloading the page still interrupts the supply until a page claims control again. |
| `externalTakeoverDetection` | `"afterTrackStart"` | When playback started elsewhere is recognized: only once that track has started, so an operation completing before then may still be added. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### jitSession.create

<!-- api-schema:begin jitSession.create -->
Experimental API; it may change in future releases.

Create a session and take control of it. Creating changes nothing in playback. Fails with `CONFLICT` while another session exists (released sessions excepted), with its `sessionId` in `details`; take it over with `jitSession.claimControl` instead. A retry with the same `requestId` from the same page returns the first answer.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `requestId` | `string` | Yes | Caller-chosen key that makes a retry return the first answer. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `sessionId` | `string` | The new session. |
| `controlToken` | `string` | Token that write commands must carry. Keep it in the page; it is never sent in events or state. |
| `ownerEpoch` | `integer` | Control generation; changes whenever control moves. |
| `revision` | `integer` | Current state version. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### jitSession.discover

<!-- api-schema:begin jitSession.discover -->
Experimental API; it may change in future releases.

Find the session a page can resume or take over, without changing playback or granting control. `canResume` is true for a page reloaded in the window whose previous page held control.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `session` | `JitSessionDiscovery \| null` | The session, or null when there is none. |
| `session.sessionId` | `string` | The session. |
| `session.ownerState` | `"connected" \| "disconnected"` | `connected` while the page holding control is still loaded; `disconnected` after it navigated away or closed. |
| `session.ownerEpoch` | `integer` | Current control generation, for `jitSession.claimControl`. |
| `session.canResume` | `boolean` | Whether the calling page may claim it with `mode: 'resume'`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### jitSession.getState

<!-- api-schema:begin jitSession.getState -->
Experimental API; it may change in future releases.

Read a consistent snapshot of a session. The snapshot never contains the control token.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | Yes | Session to read. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `state` | `JitSessionSnapshot` | Snapshot of the session. |
| `state.sessionId` | `string` | The session. |
| `state.revision` | `integer` | State version; every change the session reports raises it. |
| `state.lastEventSeq` | `integer` | `eventSeq` of the last event this snapshot reflects. |
| `state.ownerEpoch` | `integer` | Current control generation. |
| `state.ownerState` | `"connected" \| "disconnected" \| "none"` | Whether the controlling page is still loaded. |
| `state.phase` | `"active" \| "stopped" \| "released"` | `active` while the session runs playback, `stopped` after a stop, `released` after `jitSession.release` or a `jitQueue` takeover; a takeover while an adopted native context is open leaves it `stopped`. |
| `state.phaseReason` | `"user" \| "command" \| "external" \| "legacyTakeover" \| null` | Why the session last stopped or was released, or null. |
| `state.current` | `JitSessionAttempt \| null` | The current attempt, or null. |
| `state.current.entryId` | `string` | Caller's entry identifier. |
| `state.current.attemptId` | `string` | Host's identifier of this attempt; a replay of the same entry gets a new one. |
| `state.current.title` | `string` | Display title. |
| `state.current.status` | `"preparing" \| "playing" \| "ended"` | `preparing` until playback starts, then `playing`; `ended` once the attempt finished. |
| `state.current.nativeRef` | `string` | The native reference the entry plays, for an adopted current track or a native supply; absent for a location. |
| `state.next` | `JitSessionNextEntry \| null` | The next entry, or null. |
| `state.next.entryId` | `string` | Caller's entry identifier. |
| `state.next.title` | `string` | Display title. |
| `state.next.operationId` | `string` | Operation adding it to the buffer. |
| `state.next.nativeRef` | `string` | The native reference the entry plays, for a native supply; absent for a location. |
| `state.supplyState` | `"needed" \| "preparing" \| "ready" \| "exhausted"` | `needed`: no next entry and no end declared; `preparing`: the next entry's operation is pending; `ready`: the next entry is buffered; `exhausted`: the end of queue was declared. |
| `state.neededAfterAttemptId` | `string \| null` | The attempt an unanswered `jitSession:needNext` was sent for, or null. |
| `state.neededRequestKey` | `string \| null` | Identifier of the unanswered supply request, or null. |
| `state.pendingOperations` | `string[]` | Pending operations, in the order they were accepted. |
| `state.nativeContext` | `JitNativeContextSummary` | The native context captured with `jitSession.prepareAdoption`; absent when there is none. |
| `state.nativeContext.contextId` | `string` | The context. |
| `state.nativeContext.contextRevision` | `integer` | Version of the occurrences, their bindings and the context status; every change raises it together with the session `revision`. |
| `state.nativeContext.status` | `"prepared" \| "managed" \| "restoring" \| "restored" \| "conflicted" \| "detached"` | `prepared` while only the snapshot exists and native playback runs on its own. |
| `state.nativeContext.reason` | `"snapshotExpired" \| "nativeChanged" \| null` | Why the context stopped applying, or null: `snapshotExpired` after 30 seconds unadopted, `nativeChanged` when playback, the queue, the settings or a captured playlist changed. |
| `state.nativeContext.controlMode` | `"playback" \| "recovery"` | `playback` while the session may schedule playback; `recovery` while only release is allowed. |
| `state.nativeContext.sourceNextOffset` | `integer` | Source row native playback continues from when the context is given back; equals `sourceCount` when the source is exhausted. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### jitSession.claimControl

<!-- api-schema:begin jitSession.claimControl -->
Experimental API; it may change in future releases.

Take control of a session without restarting what is playing. `resume` is for a page reloaded in the window whose previous page held control; `handover` is for any other page and must name the current `ownerEpoch`. The previous controller's token stops working and its pending operations are kept. Fails with `NOT_OWNER` when resuming is not allowed and with `CONFLICT` when `expectedOwnerEpoch` is out of date.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | Yes | Session to claim. Must not be empty. |
| `mode` | `"resume" \| "handover"` | Yes | `resume` after reloading in the controlling window, `handover` from any other page. |
| `expectedOwnerEpoch` | `integer` | No | The `ownerEpoch` being taken over; required for `handover`. At least `0`. |
| `requestId` | `string` | Yes | Caller-chosen key that makes a retry return the first answer. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `controlToken` | `string` | New control token; the previous controller's no longer works. |
| `ownerEpoch` | `integer` | New control generation. |
| `revision` | `integer` | Current state version. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### jitSession.play

<!-- api-schema:begin jitSession.play -->
Experimental API; it may change in future releases.

Play an entry now as a new attempt. The answer means the command was accepted, not that the resource can play: `jitSession:operationCompleted` reports whether it was added and started, and `jitSession:attemptFinished` how the attempt ended. Tracks in the native playback queue are kept and play first when the attempt ends. Write commands fail with `NOT_OWNER` for a token that is not the current controller's and with `CONFLICT` when `expectedRevision` is out of date or `requestId` was used for a different command.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | Yes | Session to act on. Must not be empty. |
| `controlToken` | `string` | Yes | Control token from `jitSession.create` or `jitSession.claimControl`. Must not be empty. |
| `expectedRevision` | `integer` | Yes | The `revision` the command is based on. At least `0`. |
| `requestId` | `string` | Yes | Caller-chosen key that makes a retry return the first answer instead of running again. Must not be empty. |
| `entryId` | `string` | Yes | Caller's identifier of this occurrence in its queue; repeated resources use different ones. Must not be empty. |
| `title` | `string` | No | Display title. Default: `""`. |
| `location` | `string` | Yes | Resource to play: a file path or `file://` URL, `path\|subsong:N` for one subsong, or an `http(s)://` stream. A playlist file that expands to more than one track fails the operation. CUE locations must name a subsong explicitly. At most 2048 characters. Must not be empty. |
| `expiresAt` | `number` | No | When the location stops being valid, in Unix milliseconds; recorded only. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Operation adding and starting the entry. |
| `attemptId` | `string` | The attempt this command starts. |
| `revision` | `integer` | State version after accepting the command. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### jitSession.setNext

<!-- api-schema:begin jitSession.setNext -->
Experimental API; it may change in future releases.

Set, replace or withdraw the entry after the current attempt. Accepted only while `afterAttemptId` is still the current attempt. A replaced or withdrawn next entry is removed from the buffer and its pending operation ends as `superseded`; the current attempt keeps playing. Without `entryId`, the next entry is withdrawn; `endOfQueue: true` also declares that nothing follows. When the current attempt has already ended and the session is waiting for its next entry, the new entry starts at once. Within the output buffer length plus one second of the current track's end, foobar2000 may already have picked the buffered next entry to follow it, so withdrawing or replacing a buffered entry then fails with `CONFLICT` and `details.reason: "nextPreselected"`: nothing changes and the buffered entry plays next. Once it starts, set what follows it against its attempt. Setting a next entry while none is buffered is still accepted.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | Yes | Session to act on. Must not be empty. |
| `controlToken` | `string` | Yes | Control token from `jitSession.create` or `jitSession.claimControl`. Must not be empty. |
| `expectedRevision` | `integer` | Yes | The `revision` the command is based on. At least `0`. |
| `requestId` | `string` | Yes | Caller-chosen key that makes a retry return the first answer instead of running again. Must not be empty. |
| `afterAttemptId` | `string` | Yes | The current attempt the command is meant for. Must not be empty. |
| `entryId` | `string` | No | Identifier of the next entry; leave out to withdraw the next entry. |
| `title` | `string` | No | Display title. |
| `location` | `string` | No | Resource of the next entry, written as for `jitSession.play`; required with `entryId`. |
| `expiresAt` | `number` | No | When the location stops being valid, in Unix milliseconds; recorded only. |
| `endOfQueue` | `boolean` | No | Declare that nothing follows. Cannot be combined with `entryId`. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Operation adding the next entry; absent when the next entry was withdrawn. |
| `revision` | `integer` | State version after accepting the command. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### jitSession.skip

<!-- api-schema:begin jitSession.skip -->
Experimental API; it may change in future releases.

End the current attempt and play the buffered next entry, leaving the native playback queue untouched. Accepted only while `attemptId` is the current attempt. Without a ready next entry, fails with `NO_ACTIVE_ITEM`: the current attempt keeps playing, the revision does not change and no operation is created.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | Yes | Session to act on. Must not be empty. |
| `controlToken` | `string` | Yes | Control token from `jitSession.create` or `jitSession.claimControl`. Must not be empty. |
| `expectedRevision` | `integer` | Yes | The `revision` the command is based on. At least `0`. |
| `requestId` | `string` | Yes | Caller-chosen key that makes a retry return the first answer instead of running again. Must not be empty. |
| `attemptId` | `string` | Yes | The current attempt to end. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Operation starting the buffered next entry. |
| `revision` | `integer` | State version after accepting the command. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### jitSession.stop

<!-- api-schema:begin jitSession.stop -->
Experimental API; it may change in future releases.

Stop playback and supersede pending operations. The session stays, in `stopped`, for its state and results to be read; a later `jitSession.play` starts a new attempt. After `jitSession.adoptNative`, the remaining native queue occurrences are given back without starting playback; a failure is recorded in `nativeContext`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | Yes | Session to act on. Must not be empty. |
| `controlToken` | `string` | Yes | Control token from `jitSession.create` or `jitSession.claimControl`. Must not be empty. |
| `expectedRevision` | `integer` | Yes | The `revision` the command is based on. At least `0`. |
| `requestId` | `string` | Yes | Caller-chosen key that makes a retry return the first answer instead of running again. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `revision` | `integer` | State version after accepting the command. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### jitSession.getOperation

<!-- api-schema:begin jitSession.getOperation -->
Experimental API; it may change in future releases.

Read an operation's status. The pending operations and the latest 64 finished ones of a session can be read. After release they remain readable until the next `jitSession.create` or for ten minutes, whichever comes first; an expired record fails with `NOT_FOUND`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | Yes | Session the operation belongs to. Must not be empty. |
| `operationId` | `string` | Yes | Operation to read. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `operation` | `JitSessionOperation` | The operation. |
| `operation.operationId` | `string` | The operation. |
| `operation.kind` | `"play" \| "setNext" \| "skip"` | Command that created it. |
| `operation.entryId` | `string` | Entry it adds. |
| `operation.attemptId` | `string \| null` | Attempt started by play or skip; null while preparing a next entry. |
| `operation.status` | `"pending" \| "completed" \| "failed" \| "superseded"` | `pending`, or the final status: `completed` (added, and started for `play`), `failed`, or `superseded` by a later command. |
| `operation.reason` | `string \| null` | Why it failed or was superseded, or null. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### jitSession.release

<!-- api-schema:begin jitSession.release -->
Experimental API; it may change in future releases.

End the session and give the native context back: the buffer is emptied, the track playing keeps decoding, and the playback order and active playlist saved when the session first played are restored. After `jitSession.adoptNative`, the remaining native queue occurrences are given back first (see `nativeDisposition`); when that fails the session is kept, the error carries `details.contextId` and `details.recoveryId`, and a release with a new `requestId` continues where it stopped. Giving the context back while foobar2000 may already have picked the buffered next entry (see `jitSession.setNext`) fails with `CONFLICT` and `details.reason: "nextPreselected"` and changes nothing; `nativeDisposition: "detach"` is still accepted. Without an adopted context, a next entry picked that way may still play after the release.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | Yes | Session to act on. Must not be empty. |
| `controlToken` | `string` | Yes | Control token from `jitSession.create` or `jitSession.claimControl`. Must not be empty. |
| `expectedRevision` | `integer` | Yes | The `revision` the command is based on. At least `0`. |
| `requestId` | `string` | Yes | Caller-chosen key that makes a retry return the first answer instead of running again. Must not be empty. |
| `nativeDisposition` | `"restore" \| "detach"` | No | What to do with an adopted native context: `restore` (default) gives the remaining occurrences back to the native queue and continues the playlist; `detach` leaves the native queue and playlists as they are and keeps the occurrences readable. Only for a session that adopted native playback. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### jitSession.prepareAdoption

<!-- api-schema:begin jitSession.prepareAdoption -->
Experimental API; it may change in future releases.

Capture the native playback for adoption: the track playing, the native playback queue one occurrence at a time (repeated tracks stay separate) and the playlist it plays from. Nothing in playback, the queue, the playlists or the settings changes; read the occurrences and source rows with `jitSession.getNativeContext`. The session must be new: `CONFLICT` once it has played, while a `jitQueue` buffer is in use or while an adopted context is still open. A new call replaces an unadopted snapshot. Fails with `NOT_SUPPORTED` unless playback is running and not paused from a row of a normal playlist (not an autoplaylist or another locked one), the playback order is Default, cursor and playback following and stop-after-current are off, and the queue holds at most 1024 tracks whose positions match their playlists; `details.reason` names the first failed condition. `jitSession.adoptNative` commits the snapshot.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | Yes | Session to act on. Must not be empty. |
| `controlToken` | `string` | Yes | Control token from `jitSession.create` or `jitSession.claimControl`. Must not be empty. |
| `expectedRevision` | `integer` | Yes | The `revision` the command is based on. At least `0`. |
| `requestId` | `string` | Yes | Caller-chosen key that makes a retry return the first answer instead of running again. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `contextId` | `string` | The native context. |
| `snapshotToken` | `string` | Authorizes adopting this snapshot once. Keep it in the page; it is never sent in events or state. |
| `expiresAt` | `number` | When the snapshot expires unadopted, in Unix milliseconds; the host times it with its own monotonic clock. |
| `revision` | `integer` | State version after accepting the command. |
| `contextRevision` | `integer` | Version of the context. |
| `current` | `JitNativeTrack` | The track playing; its `nativeRef` is the same as its source row's. |
| `current.nativeRef` | `string` | Host's reference to this occurrence or source row, valid within its context only. |
| `current.path` | `string` | foobar2000 path of the track. |
| `current.subsong` | `integer` | Subsong index within the file. |
| `current.title` | `string` | Title tag, or the file name when there is none. |
| `current.playlistGuid` | `string \| null` | Playlist the track is in, or null for a queue entry without a playlist position. |
| `current.row` | `integer \| null` | Row within that playlist, or null. |
| `manualCount` | `integer` | Occurrences captured from the native playback queue. |
| `sourceCount` | `integer` | Rows of the playlist being played. |
| `sourceNextOffset` | `integer` | Source row after the track playing. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### jitSession.getNativeContext

<!-- api-schema:begin jitSession.getNativeContext -->
Experimental API; it may change in future releases.

Read one page of a native context: the `manual` occurrences captured from the native queue, in their order, or the `source` rows of the playlist being played. Reading changes neither revision; a row read twice keeps its `nativeRef`. Fails with `CONFLICT` when `expectedContextRevision` is out of date, and for `source` pages also when the playlist's content changed since the snapshot; a page past the end is empty.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | Yes | Session the context belongs to. Must not be empty. |
| `contextId` | `string` | Yes | Context to read. Must not be empty. |
| `expectedContextRevision` | `integer` | Yes | The `contextRevision` the page is based on. At least `0`. |
| `collection` | `"manual" \| "source"` | Yes | `manual` for the queue occurrences, `source` for the playlist rows. |
| `offset` | `integer` | Yes | First item: an occurrence index for `manual`, a playlist row for `source`. At least `0`. |
| `limit` | `integer` | Yes | Items per page. Between `1` and `128` inclusive. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `context` | `JitNativeContextDetail` | The context, with the track captured as current and the counts. |
| `context.contextId` | `string` | The context. |
| `context.contextRevision` | `integer` | Version of the context. |
| `context.status` | `"prepared" \| "managed" \| "restoring" \| "restored" \| "conflicted" \| "detached"` | Status, as in the state snapshot. |
| `context.reason` | `"snapshotExpired" \| "nativeChanged" \| null` | Why the context stopped applying, as in the state snapshot, or null. |
| `context.controlMode` | `"playback" \| "recovery"` | Control mode, as in the state snapshot. |
| `context.sourceNextOffset` | `integer` | Source row native playback continues from when the context is given back. |
| `context.current` | `JitNativeTrack` | The track playing when the snapshot was taken. |
| `context.current.nativeRef` | `string` | Host's reference to this occurrence or source row, valid within its context only. |
| `context.current.path` | `string` | foobar2000 path of the track. |
| `context.current.subsong` | `integer` | Subsong index within the file. |
| `context.current.title` | `string` | Title tag, or the file name when there is none. |
| `context.current.playlistGuid` | `string \| null` | Playlist the track is in, or null for a queue entry without a playlist position. |
| `context.current.row` | `integer \| null` | Row within that playlist, or null. |
| `context.manualCount` | `integer` | Occurrences captured from the native playback queue. |
| `context.sourceCount` | `integer` | Rows of the playlist being played when the snapshot was taken. |
| `total` | `integer` | Number of items in the collection. |
| `items` | `JitNativeEntry[]` | The page. |
| `items[].nativeRef` | `string` | Host's reference, valid within this context only. |
| `items[].entryId` | `string \| null` | The caller's entry bound to this reference, or null before it is supplied. |
| `items[].path` | `string` | foobar2000 path of the track. |
| `items[].subsong` | `integer` | Subsong index within the file. |
| `items[].title` | `string` | Title tag, or the file name when there is none. |
| `items[].playlistGuid` | `string \| null` | Playlist the track is in, or null for a queue entry without a playlist position. |
| `items[].row` | `integer \| null` | Row within that playlist, or null. |
| `items[].occurrenceId` | `string \| null` | Occurrence identifier for `manual` items; null for source rows. |
| `items[].disposition` | `"pending" \| "queued" \| "submitted" \| "consumed" \| "discarded" \| "indeterminate" \| "restored" \| null` | What happened to a `manual` occurrence: `pending` not handed to playback yet, `queued` in the native queue as the next track, `submitted` taken by playback without a confirmed start, `consumed` started, `discarded` removed by the caller, `indeterminate` handed over without a known outcome, `restored` given back to the native queue. Null for source rows. |
| `items[].restorationRank` | `integer \| null` | Position among the occurrences to give back, from 0; null when it will not be given back. |
| `nextOffset` | `integer \| null` | Offset of the next page, or null on the last page. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### jitSession.adoptNative

<!-- api-schema:begin jitSession.adoptNative -->
Experimental API; it may change in future releases.

Adopt the native playback captured by `jitSession.prepareAdoption` without interrupting it. The track playing becomes the current attempt, bound to `currentEntryId`, and keeps decoding; the native playback queue is emptied, its occurrences kept by the host, and the session schedules what follows (`jitSession:needNext` asks for it). Stopping or releasing the session gives the remaining occurrences back to the native queue and continues the playlist after the last row played from it. When no row is left after it, the last occurrence goes into a single-track playlist the host keeps for this (`__webview_restore__`), so playback stops after it instead of continuing the playlist it came from. A `jitQueue` call made while the context is open keeps its usual behavior; the session then stays `stopped` with reason `legacyTakeover`, keeps its control token and accepts only `jitSession.release`. Fails with `CONFLICT` when the snapshot was replaced, expired or already used, or native playback changed since it was taken, and with `NOT_SUPPORTED` when a setting no longer allows adoption; nothing changes then.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | Yes | Session to act on. Must not be empty. |
| `controlToken` | `string` | Yes | Control token from `jitSession.create` or `jitSession.claimControl`. Must not be empty. |
| `expectedRevision` | `integer` | Yes | The `revision` the command is based on. At least `0`. |
| `requestId` | `string` | Yes | Caller-chosen key that makes a retry return the first answer instead of running again. Must not be empty. |
| `contextId` | `string` | Yes | The context returned by `jitSession.prepareAdoption`. Must not be empty. |
| `snapshotToken` | `string` | Yes | The snapshot token returned with it. Must not be empty. |
| `currentEntryId` | `string` | Yes | Caller's entry identifier for the track playing. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `contextId` | `string` | The context, now managed by the session. |
| `attemptId` | `string` | The attempt given to the track playing. |
| `revision` | `integer` | State version after accepting the command. |
| `contextRevision` | `integer` | Version of the context. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### jitSession.playNative

<!-- api-schema:begin jitSession.playNative -->
Experimental API; it may change in future releases.

Play a source row or queue occurrence of the adopted context now, as `jitSession.play` does for a location: the answer means accepted, and `jitSession:operationCompleted` reports the start. The first supply of a `nativeRef` binds it to `entryId`; replaying it keeps that `entryId`, and a different one fails with `CONFLICT`. Starting a queue occurrence consumes it, so it is not given back later; replaying a consumed or removed one does not bring it back. Playing a source row moves the point the playlist continues from when the context is given back.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | Yes | Session to act on. Must not be empty. |
| `controlToken` | `string` | Yes | Control token from `jitSession.create` or `jitSession.claimControl`. Must not be empty. |
| `expectedRevision` | `integer` | Yes | The `revision` the command is based on. At least `0`. |
| `requestId` | `string` | Yes | Caller-chosen key that makes a retry return the first answer instead of running again. Must not be empty. |
| `contextId` | `string` | Yes | The adopted context. Must not be empty. |
| `nativeRef` | `string` | Yes | A `nativeRef` read from `jitSession.getNativeContext`. Must not be empty. |
| `entryId` | `string` | Yes | Caller's identifier for this entry. Must not be empty. |
| `title` | `string` | No | Display title; the track's title tag when left out. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Operation adding and starting the entry. |
| `attemptId` | `string` | The attempt this command starts. |
| `revision` | `integer` | State version after accepting the command. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### jitSession.setNativeNext

<!-- api-schema:begin jitSession.setNativeNext -->
Experimental API; it may change in future releases.

Set a source row or queue occurrence of the adopted context as the entry after the current attempt, as `jitSession.setNext` does for a location. Binding works as for `jitSession.playNative`; use `jitSession.setNext` without `entryId` to withdraw it. Replacing a buffered next entry fails with `nextPreselected` under the same condition as `jitSession.setNext`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | Yes | Session to act on. Must not be empty. |
| `controlToken` | `string` | Yes | Control token from `jitSession.create` or `jitSession.claimControl`. Must not be empty. |
| `expectedRevision` | `integer` | Yes | The `revision` the command is based on. At least `0`. |
| `requestId` | `string` | Yes | Caller-chosen key that makes a retry return the first answer instead of running again. Must not be empty. |
| `contextId` | `string` | Yes | The adopted context. Must not be empty. |
| `afterAttemptId` | `string` | Yes | The current attempt the command is meant for. Must not be empty. |
| `nativeRef` | `string` | Yes | A `nativeRef` read from `jitSession.getNativeContext`. Must not be empty. |
| `entryId` | `string` | Yes | Caller's identifier for this entry. Must not be empty. |
| `title` | `string` | No | Display title; the track's title tag when left out. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Operation adding the next entry; absent when the next entry was withdrawn. |
| `revision` | `integer` | State version after accepting the command. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### jitSession.setNativeQueue

<!-- api-schema:begin jitSession.setNativeQueue -->
Experimental API; it may change in future releases.

Decide which queue occurrences of the adopted context are still to be given back, and in what order: list every pending or queued occurrence to keep; the ones left out are removed and never given back. A removed occurrence that is the next entry is withdrawn from the buffer. Fails with `INVALID_PARAMS` for a repeated or unknown identifier, and with `CONFLICT` when `expectedContextRevision` is out of date or a listed occurrence has meanwhile been handed to playback, and with `CONFLICT` and `details.reason: "nextPreselected"` when removing the buffered next entry under the condition described for `jitSession.setNext`; nothing changes then.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | Yes | Session to act on. Must not be empty. |
| `controlToken` | `string` | Yes | Control token from `jitSession.create` or `jitSession.claimControl`. Must not be empty. |
| `expectedRevision` | `integer` | Yes | The `revision` the command is based on. At least `0`. |
| `requestId` | `string` | Yes | Caller-chosen key that makes a retry return the first answer instead of running again. Must not be empty. |
| `contextId` | `string` | Yes | The adopted context. Must not be empty. |
| `expectedContextRevision` | `integer` | Yes | The `contextRevision` the command is based on. At least `0`. |
| `remainingOccurrenceIds` | `string[]` | Yes | Occurrences to keep, in the order to give them back. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `revision` | `integer` | State version after accepting the command. |
| `contextRevision` | `integer` | Version of the context after the change. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

## JIT Session events

### jitSession:stateChanged

<!-- api-schema:begin event:jitSession:stateChanged -->
The session's state changed: an attempt started or ended, an operation finished, supply or control changed. Carries the full snapshot. Sent to the controlling page only; a page that missed events reads `jitSession.getState`.

Sent to the page that owns the subscription or task.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `sessionId` | `string` | The session. |
| `eventSeq` | `integer` | Order of this event within the session; consumers drop an event they have already seen. |
| `revision` | `integer` | State version this event reflects. |
| `state` | `JitSessionSnapshot` | Snapshot after the change. |
| `state.sessionId` | `string` | The session. |
| `state.revision` | `integer` | State version; every change the session reports raises it. |
| `state.lastEventSeq` | `integer` | `eventSeq` of the last event this snapshot reflects. |
| `state.ownerEpoch` | `integer` | Current control generation. |
| `state.ownerState` | `"connected" \| "disconnected" \| "none"` | Whether the controlling page is still loaded. |
| `state.phase` | `"active" \| "stopped" \| "released"` | `active` while the session runs playback, `stopped` after a stop, `released` after `jitSession.release` or a `jitQueue` takeover; a takeover while an adopted native context is open leaves it `stopped`. |
| `state.phaseReason` | `"user" \| "command" \| "external" \| "legacyTakeover" \| null` | Why the session last stopped or was released, or null. |
| `state.current` | `JitSessionAttempt \| null` | The current attempt, or null. |
| `state.current.entryId` | `string` | Caller's entry identifier. |
| `state.current.attemptId` | `string` | Host's identifier of this attempt; a replay of the same entry gets a new one. |
| `state.current.title` | `string` | Display title. |
| `state.current.status` | `"preparing" \| "playing" \| "ended"` | `preparing` until playback starts, then `playing`; `ended` once the attempt finished. |
| `state.current.nativeRef` | `string` | The native reference the entry plays, for an adopted current track or a native supply; absent for a location. |
| `state.next` | `JitSessionNextEntry \| null` | The next entry, or null. |
| `state.next.entryId` | `string` | Caller's entry identifier. |
| `state.next.title` | `string` | Display title. |
| `state.next.operationId` | `string` | Operation adding it to the buffer. |
| `state.next.nativeRef` | `string` | The native reference the entry plays, for a native supply; absent for a location. |
| `state.supplyState` | `"needed" \| "preparing" \| "ready" \| "exhausted"` | `needed`: no next entry and no end declared; `preparing`: the next entry's operation is pending; `ready`: the next entry is buffered; `exhausted`: the end of queue was declared. |
| `state.neededAfterAttemptId` | `string \| null` | The attempt an unanswered `jitSession:needNext` was sent for, or null. |
| `state.neededRequestKey` | `string \| null` | Identifier of the unanswered supply request, or null. |
| `state.pendingOperations` | `string[]` | Pending operations, in the order they were accepted. |
| `state.nativeContext` | `JitNativeContextSummary` | The native context captured with `jitSession.prepareAdoption`; absent when there is none. |
| `state.nativeContext.contextId` | `string` | The context. |
| `state.nativeContext.contextRevision` | `integer` | Version of the occurrences, their bindings and the context status; every change raises it together with the session `revision`. |
| `state.nativeContext.status` | `"prepared" \| "managed" \| "restoring" \| "restored" \| "conflicted" \| "detached"` | `prepared` while only the snapshot exists and native playback runs on its own. |
| `state.nativeContext.reason` | `"snapshotExpired" \| "nativeChanged" \| null` | Why the context stopped applying, or null: `snapshotExpired` after 30 seconds unadopted, `nativeChanged` when playback, the queue, the settings or a captured playlist changed. |
| `state.nativeContext.controlMode` | `"playback" \| "recovery"` | `playback` while the session may schedule playback; `recovery` while only release is allowed. |
| `state.nativeContext.sourceNextOffset` | `integer` | Source row native playback continues from when the context is given back; equals `sourceCount` when the source is exhausted. |
<!-- api-schema:end -->

### jitSession:needNext

<!-- api-schema:begin event:jitSession:needNext -->
The session needs its next entry: an attempt started with none buffered and no end of queue declared. Answer with `jitSession.setNext`. The open request stays readable in the snapshot as `neededAfterAttemptId`. Sent like `jitSession:stateChanged`.

Sent to the page that owns the subscription or task.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `sessionId` | `string` | The session. |
| `eventSeq` | `integer` | Order of this event within the session; consumers drop an event they have already seen. |
| `revision` | `integer` | State version this event reflects. |
| `afterAttemptId` | `string` | The attempt that has no next entry. |
| `requestKey` | `string` | Identifier of this supply request; also readable in the snapshot. |
<!-- api-schema:end -->

### jitSession:operationCompleted

<!-- api-schema:begin event:jitSession:operationCompleted -->
An operation reached its final status. Each operation sends this once. Sent like `jitSession:stateChanged`.

Sent to the page that owns the subscription or task.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `sessionId` | `string` | The session. |
| `eventSeq` | `integer` | Order of this event within the session; consumers drop an event they have already seen. |
| `revision` | `integer` | State version this event reflects. |
| `operation` | `JitSessionOperation` | The operation in its final status. |
| `operation.operationId` | `string` | The operation. |
| `operation.kind` | `"play" \| "setNext" \| "skip"` | Command that created it. |
| `operation.entryId` | `string` | Entry it adds. |
| `operation.attemptId` | `string \| null` | Attempt started by play or skip; null while preparing a next entry. |
| `operation.status` | `"pending" \| "completed" \| "failed" \| "superseded"` | `pending`, or the final status: `completed` (added, and started for `play`), `failed`, or `superseded` by a later command. |
| `operation.reason` | `string \| null` | Why it failed or was superseded, or null. |
<!-- api-schema:end -->

### jitSession:attemptFinished

<!-- api-schema:begin event:jitSession:attemptFinished -->
An attempt ended. `outcome` is `completed` only when playback reached the end of the resource; an ending the host cannot attribute is `unknown`, with the facts it has. Sent like `jitSession:stateChanged`.

Sent to the page that owns the subscription or task.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `sessionId` | `string` | The session. |
| `eventSeq` | `integer` | Order of this event within the session; consumers drop an event they have already seen. |
| `revision` | `integer` | State version this event reflects. |
| `attemptId` | `string` | The attempt that ended. |
| `entryId` | `string` | Its entry. |
| `outcome` | `"completed" \| "stopped" \| "superseded" \| "released" \| "unknown"` | `completed`: playback reached the end; `stopped`: stopped by the user or `jitSession.stop`; `superseded`: another attempt or playback started; `released`: the session ended; `unknown`: the host cannot attribute the ending. |
| `stopReason` | `"user" \| "eof" \| "startingAnother" \| "shuttingDown" \| null` | Native stop reason, when one was observed; null for a command or an inferred transition. |
| `position` | `number \| null` | Last playback position read for the attempt, in seconds, or null. |
| `duration` | `number \| null` | Duration of the resource, in seconds, or null when unknown. |
<!-- api-schema:end -->

### jitSession:attemptRechecked

<!-- api-schema:begin event:jitSession:attemptRechecked -->
The host reopened the resource of an attempt that ended as `unknown` within its first second, and reports what that second try ran into. It describes the resource at the time of the recheck, not necessarily the cause of the original failure: a transient network failure may not reproduce, and a single-use address may fail differently the second time. Follows the attempt's `jitSession:attemptFinished`, up to about ten seconds later for network errors; not sent for other endings or when the host shuts down meanwhile. It changes no session state and is not kept in the snapshot, so a page that misses it cannot read it back. Sent like `jitSession:stateChanged`.

Sent to the page that owns the subscription or task.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `sessionId` | `string` | The session. |
| `eventSeq` | `integer` | Order of this event within the session; consumers drop an event they have already seen. |
| `revision` | `integer` | State version this event reflects. |
| `attemptId` | `string` | The attempt whose resource was rechecked. |
| `entryId` | `string` | Its entry. |
| `result` | `"reproduced" \| "notReproduced"` | `reproduced`: the recheck failed too; `notReproduced`: the resource opened and decoded its first block this time. |
| `reason` | `"accessDenied" \| "notFound" \| "unsupportedContent" \| "network" \| "other" \| null` | Why the recheck failed, or null when it did not: `accessDenied` (for example HTTP 403), `notFound` (HTTP 404 or a missing file), `unsupportedContent` (not audio, or a format no installed decoder reads), `network` (other HTTP errors, refused or timed-out connections), `other`. |
| `stage` | `"open" \| "initialize" \| "decode" \| null` | Where the recheck failed: opening the resource, initializing the decoder, or decoding the first block; null when it did not fail. |
| `engineReported` | `boolean` | Whether foobar2000 logged a playback failure naming this resource within the last minute, for example that it could not open it. |
<!-- api-schema:end -->

### jitSession:controlChanged

<!-- api-schema:begin event:jitSession:controlChanged -->
Control of the session moved: claimed by another page, taken over by a `jitQueue` call, or released. Sent to the new and the previous controlling page.

Sent to the page that owns the subscription or task.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `sessionId` | `string` | The session. |
| `eventSeq` | `integer` | Order of this event within the session; consumers drop an event they have already seen. |
| `revision` | `integer` | State version this event reflects. |
| `ownerEpoch` | `integer` | Control generation after the change. |
| `reason` | `"claimed" \| "legacyTakeover" \| "released"` | `claimed` by another page, taken over by a `jitQueue` call (`legacyTakeover`), or `released`. |
<!-- api-schema:end -->

