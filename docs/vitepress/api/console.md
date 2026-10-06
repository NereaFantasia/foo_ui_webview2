# Console API

Methods of the `console` namespace.

## console

### console.error

<!-- api-schema:begin console.error -->
Write an error line to the foobar2000 console, prefixed with `[WebView][ERROR]`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `any` | No | What to write. A string is written as it is; any other JSON value as its JSON text. Takes precedence over `args`. |
| `args` | `any[]` | No | Values to write, separated by spaces: strings as they are, other values as their JSON text. Used only when `message` is absent. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Provide one of `message` or `args`; a call with neither fails with `INVALID_PARAMS` (`message is required`).

```js
const result = await fb2k.invoke('console.error', { message: 'failed to load artwork' });
```

### console.log

<!-- api-schema:begin console.log -->
Write a line to the foobar2000 console, prefixed with `[WebView]`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `any` | No | What to write. A string is written as it is; any other JSON value as its JSON text. Takes precedence over `args`. |
| `args` | `any[]` | No | Values to write, separated by spaces: strings as they are, other values as their JSON text. Used only when `message` is absent. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Provide one of `message` or `args`; a call with neither fails with `INVALID_PARAMS` (`message is required`).

```js
const result = await fb2k.invoke('console.log', { message: 'track started' });
```

### console.warn

<!-- api-schema:begin console.warn -->
Write a warning line to the foobar2000 console, prefixed with `[WebView][WARN]`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `any` | No | What to write. A string is written as it is; any other JSON value as its JSON text. Takes precedence over `args`. |
| `args` | `any[]` | No | Values to write, separated by spaces: strings as they are, other values as their JSON text. Used only when `message` is absent. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Provide one of `message` or `args`; a call with neither fails with `INVALID_PARAMS` (`message is required`).

```js
const result = await fb2k.invoke('console.warn', { args: ['retry', 3] });
```

## Owner-family behavior and limits

- `console.log`, `console.warn` and `console.error` share their `message` / `args` rule with `log.write`; see [Log API](./log.md#owner-family-behavior-and-limits).
