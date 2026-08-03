# Test reference

Bridge connectivity helpers registered beside the playback family.


## test.echo

Echoes the provided parameters so themes can verify invoke transport.

```javascript
const result = await fb2k.invoke('test.echo', { message: 'hello' });
// { "success": true, "echo": "hello", "input": { "message": "hello" } }
```

If `message` is omitted, both `echo` and `input` contain the full params object.

## test.ping

Lightweight liveness probe.

```javascript
const result = await fb2k.invoke('test.ping');
// { "pong": true, "timestamp": 1234567890 }
```

`timestamp` is a Unix epoch seconds value from the host.
