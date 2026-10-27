# Test API

用于验证 Bridge 双向链路通信与连通性的内部诊断端点。

## test.echo

原样回显传入的请求参数，用于验证 Bridge 链路传输。

```javascript
const result = await fb2k.invoke('test.echo', { message: 'hello' });
// { "success": true, "echo": "hello", "input": { "message": "hello" } }
```

若请求中省略 `message` 字段，则 `echo` 与 `input` 字段均原样回显完整的 params 参数对象。

## test.ping

轻量级心跳与存活探测接口。

```javascript
const result = await fb2k.invoke('test.ping');
// { "pong": true, "timestamp": 1234567890 }
```

`timestamp` 为宿主端报告的当前 Unix 时间戳（秒）。
