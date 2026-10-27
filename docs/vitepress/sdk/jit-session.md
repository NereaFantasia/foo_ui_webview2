# JIT session client

`JitSessionClient` manages document-bound credentials and serializes commands for the experimental
`jitSession` protocol. Construct it before creating or claiming a session so it can retain events
that arrive before a command response. Check [host capabilities](../api/jit-session.md) before use.

## Start an attempt

```typescript
import { JitSessionClient } from 'foo-webview-sdk/bridge';

const client = new JitSessionClient();
await client.create();
const accepted = await client.play({
  entryId: 'queue-entry-1',
  location: 'https://media.example.com/track.flac',
});
const result = await client.waitForOperation(accepted.operationId);
console.log(result.status, result.reason);
```

An accepted command is not proof that a resource can play. `waitForOperation()` resolves with a
terminal record, including `failed` or `superseded`; inspect its `status`. Playback outcomes arrive
as `attemptFinished`, independently of the operation result. A result can be `unknown` when the
host cannot establish why playback ended.

Stop reasons come from foobar2000 callbacks. A playlist's default action can first report
`user`, ending the attempt as `stopped`; the session then stays stopped even if another track
starts. A track change that leaves the active session's buffer without that stop is reported
as `external` after the new track starts. Neither path relinquishes control.

The script bundle exposes the same class as `fb.JitSessionClient`.

## Supply or withdraw the next entry

```typescript
import { JitSessionClient } from 'foo-webview-sdk/bridge';

const client = new JitSessionClient();
await client.create();
const accepted = await client.play({ entryId: 'one', location: 'C:/Music/one.flac' });
await client.setNext({
  afterAttemptId: accepted.attemptId,
  entryId: 'two',
  location: 'C:/Music/two.flac',
});
// Withdraw the next entry without stopping the current attempt
await client.setNext({ afterAttemptId: accepted.attemptId });
// Declare that the current entry is the last one
await client.setNext({ afterAttemptId: accepted.attemptId, endOfQueue: true });
```

Each update names the attempt it follows. Replaying an entry creates a new `attemptId`; an update
for an older attempt is rejected. A new entry and `endOfQueue: true` cannot be supplied together.

foobar2000 picks the track that follows shortly before the current one ends, and that choice
cannot be taken back. Within the output buffer length plus one second of the end, withdrawing or
replacing a buffered next entry rejects with `CONFLICT` and `details.reason: "nextPreselected"`;
the buffered entry plays next, and you can change what follows it once it starts.

`client.skip(attemptId)` requires a ready next entry. If there is none, or it is still being
prepared, the call rejects with `NO_ACTIVE_ITEM`. The current attempt continues, the revision
does not change, no operation is created, and the native playback queue is untouched.

## Resume after a reload

```typescript
import { jitSession, JitSessionClient, unwrap } from 'foo-webview-sdk/bridge';

const client = new JitSessionClient();
const { session } = unwrap(await jitSession.discover());
if (session?.canResume) {
  await client.claimControl({ sessionId: session.sessionId, mode: 'resume' });
  console.log(client.state);
}
```

Resume claims control and reads the latest snapshot without sending `play`. A different window
uses `mode: 'handover'` with the observed `expectedOwnerEpoch`. Tokens are not portable between
documents and are never part of `client.state`.

## Adopt native playback

```typescript
import { JitSessionClient } from 'foo-webview-sdk/bridge';

const client = new JitSessionClient();
await client.create();
const snapshot = await client.prepareAdoption();
const queue = await client.getNativeContext({
  contextId: snapshot.contextId, expectedContextRevision: snapshot.contextRevision,
  collection: 'manual', offset: 0, limit: 128,
});
const adopted = await client.adoptNative({
  contextId: snapshot.contextId, snapshotToken: snapshot.snapshotToken, currentEntryId: 'native-current',
});
const first = queue.items[0];
if (first) {
  await client.setNativeNext({
    contextId: snapshot.contextId, afterAttemptId: adopted.attemptId,
    nativeRef: first.nativeRef, entryId: 'queued-1',
  });
}
// Later: give the remaining queue occurrences back and continue the playlist
await client.release();
```

`prepareAdoption()` changes nothing; the snapshot is committed by `adoptNative()` within 30 seconds.
The track playing keeps decoding, and the native queue is emptied, its occurrences kept by the host.
`playNative()` and `setNativeNext()` supply a source row or queue occurrence by `nativeRef`;
`setNativeQueue()` keeps and reorders the occurrences still to be given back. `stop()` and
`release()` give the remaining occurrences back; `release(requestId, 'detach')` leaves native
playback as it is. `getCapabilities().adoption` tells whether the host supports adoption;
`prepareAdoption()` decides whether the playback at hand qualifies.

## Events, conflicts and cleanup

`client.subscribe(listener)` returns an unsubscribe callback. Its event has a `name`
(`stateChanged`, `needNext`, `operationCompleted`, `attemptFinished`, `attemptRechecked` or
`controlChanged`) and a typed `payload`. `attemptRechecked` follows an attempt that ended as
`unknown` within its first second: the host reopened the resource and reports whether it failed
again and why (`accessDenied`, `notFound`, `unsupportedContent`, `network`). Treat it as a hint
for what to do next, such as resolving the address again or skipping the entry, not as proof of
the original cause. Events are deduplicated by `eventSeq`. An unseen operation terminal is retained
even when it is older than the latest snapshot. Sequence gaps trigger a snapshot refresh;
`synchronizationError` records a failed refresh, and a successful `refresh()` clears it.
Both automatic and explicit refreshes query operations that already have active waiters. A
missing terminal event can therefore be recovered after waiting has begun; a `pending` query
result keeps the wait open. Another event gap during reconciliation schedules another pass.

The client fills `sessionId`, `controlToken` and `expectedRevision`, and serializes its own write
commands. Host playback can still change between an event and a command. On `CONFLICT`, call
`refresh()` and decide whether the command still applies; the client does not replay it. Reuse a
`requestId` only for a retry of the same command. `NOT_OWNER` or a control handover disables local
writes until control is claimed again.

`waitForOperation(id, { timeoutMs, signal })` first checks retained events, then queries the host
and waits for a terminal if needed. The default timeout is 30000 milliseconds. Timeout and abort
reject the local wait; they do not cancel the host operation.

`stop()` stops playback and retains the session for queries. `release()` relinquishes control and
the buffer while the currently decoding resource continues. If it fails, for example because adopted
queue occurrences could only partly be given back, the client keeps its credentials and refreshes
`client.state`. `dispose()` removes subscriptions,
rejects pending waits and prevents further commands; it does not stop or release the host session.
Call it when the owning component or page is disposed.

## Direct namespace methods

`fb.jitSession` forwards the declared parameters and returns response envelopes. Applications using
it directly must subscribe before calling, retain the credentials, and reconcile state themselves.
The [API reference](../api/jit-session.md) defines parameter constraints, retention and event delivery.
The following are separate call examples; replace the sample IDs and revisions with observed values.

```typescript
await fb.jitSession.getCapabilities();
await fb.jitSession.discover();
await fb.jitSession.create({ requestId: 'create-1' });
await fb.jitSession.getState({ sessionId: 'session-id' });
await fb.jitSession.claimControl({
  sessionId: 'session-id', mode: 'handover', expectedOwnerEpoch: 1, requestId: 'claim-1',
});
const guard = {
  sessionId: 'session-id', controlToken: 'document-token', expectedRevision: 1, requestId: 'command-1',
};
await fb.jitSession.play({ ...guard, entryId: 'entry-id', location: 'C:/Music/track.flac' });
await fb.jitSession.setNext({ ...guard, afterAttemptId: 'attempt-id', endOfQueue: true });
await fb.jitSession.skip({ ...guard, attemptId: 'attempt-id' });
await fb.jitSession.stop(guard);
await fb.jitSession.getOperation({ sessionId: 'session-id', operationId: 'operation-id' });
await fb.jitSession.prepareAdoption(guard);
await fb.jitSession.getNativeContext({
  sessionId: 'session-id', contextId: 'context-id', expectedContextRevision: 1,
  collection: 'manual', offset: 0, limit: 128,
});
await fb.jitSession.adoptNative({
  ...guard, contextId: 'context-id', snapshotToken: 'snapshot-token', currentEntryId: 'entry-id',
});
await fb.jitSession.playNative({ ...guard, contextId: 'context-id', nativeRef: 'native-ref', entryId: 'entry-2' });
await fb.jitSession.setNativeNext({
  ...guard, contextId: 'context-id', afterAttemptId: 'attempt-id', nativeRef: 'native-ref', entryId: 'entry-3',
});
await fb.jitSession.setNativeQueue({
  ...guard, contextId: 'context-id', expectedContextRevision: 2, remainingOccurrenceIds: ['occurrence-id'],
});
await fb.jitSession.release(guard);
```
