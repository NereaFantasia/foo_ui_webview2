# Test API 

用于测试 Bridge 通信是否正常的工具 API。

## test.echo 

回显传入的参数，用于验证 Bridge 通信。

```javascript
const result = await fb2k.invoke('test.echo', { message: 'hello' });
// { "message": "hello" }
```

## test.ping 

测试连接是否存活。

```javascript
const result = await fb2k.invoke('test.ping');
// { "pong": true, "timestamp": 1234567890 }
```
