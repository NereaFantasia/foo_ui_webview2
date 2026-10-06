# Test 测试 API

`test` 命名空间的方法。

## Test API

### test.echo

回显传入的消息，用于测试 bridge 连接。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `json` | 否 | 任意 JSON 值，原样回显到 `echo`；省略时 `echo` 为整个参数对象。 |

**返回值**: `{"echo":"...","input":"...","success":true}`

```javascript
const result = await fb2k.invoke('test.echo', { message: 'hello' });
console.log(result.echo); // "hello"
```

### test.ping

心跳检测，返回当前服务端时间戳。

- **参数**: 无

**返回值**: `{ "pong": true, "timestamp": 1707500000 }`

```javascript
const result = await fb2k.invoke('test.ping');
console.log('pong:', result.timestamp);
```
