# fb.queue Playback Queue

`fb.queue` manages the foobar2000 playback queue. It is distinct from the just-in-time queue exposed as `fb.jitQueue`.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block records SDK method coverage and may later be expanded with complete examples and best practices.

### flush()

Signature: `fb.queue.flush(): Promise<BaseResponse & { clearedCount?: number }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Clears the playback queue. `flush()` is the SDK wrapper for the `queue.flush` alias; `clear()` provides the equivalent clear operation through `queue.clear`.

```javascript
const result = await fb.queue.flush();
```

### getCount()

Signature: `fb.queue.getCount(): Promise<{ count: number }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the current queue length as `{ count }`.

```javascript
const result = await fb.queue.getCount();
console.log(result.count);
```

<!-- END AUTO-GENERATED SDK STUBS -->
