# Test 测试 API

`test` 命名空间的方法。

## test {#test-api}

### test.echo

原样回显请求内容，用于验证 Bridge 双向链路诊断。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `json` | 否 | 任意 JSON 值，原样回显到 `echo`；省略时将整个参数对象回显到 `echo` |

**返回值**: `{"echo":"...","input":"...","success":true}`

```javascript
const result = await fb2k.invoke('test.echo', { message: 'hello' });
console.log(result.echo); // "hello"
```

### test.ping

心跳连通性检测，返回存活标记与宿主当前 Unix 时间戳。

- **参数**: 无

**返回值**: `{ "pong": true, "timestamp": 1707500000 }`

```javascript
const result = await fb2k.invoke('test.ping');
console.log('pong:', result.timestamp);
```

## 行为与限制

- `test.*` 为诊断接口，不属于正式业务功能。
