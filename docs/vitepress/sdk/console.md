# fb.console Host Console

`fb.console` writes string messages to the foobar2000 console window. It is separate from the browser's global `console` object.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

### error(message)

Signature: `fb.console.error(message: string): Promise<BaseResponse>`

Writes an error-level message.

```javascript
await fb.console.error('Artwork loading failed');
```

### log(message)

Signature: `fb.console.log(message: string): Promise<BaseResponse>`

Writes an informational message.

```javascript
await fb.console.log('Theme initialized');
```

### warn(message)

Signature: `fb.console.warn(message: string): Promise<BaseResponse>`

Writes a warning-level message.

```javascript
await fb.console.warn('Using fallback artwork');
```

<!-- END AUTO-GENERATED SDK STUBS -->
