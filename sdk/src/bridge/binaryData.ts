const BASE64_ALPHABET =
    'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
const BASE64_PATTERN =
    /^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/;
const RESTRICTED_NAME =
    '[A-Za-z0-9](?:[A-Za-z0-9!#$&^_.+-]{0,125}[A-Za-z0-9])?';
const MEDIA_TYPE_PATTERN = new RegExp(`^${RESTRICTED_NAME}/${RESTRICTED_NAME}$`);

/** @internal */
export interface ParsedBase64DataUrl {
    mediaType: string;
    base64: string;
}

function asBytes(data: ArrayBuffer | Uint8Array): Uint8Array {
    return data instanceof Uint8Array ? data : new Uint8Array(data);
}

function validateBase64(value: string): void {
    if (value.length % 4 !== 0 || !BASE64_PATTERN.test(value)) {
        throw new TypeError('Expected canonical padded Base64 data.');
    }
    if (value.endsWith('==')) {
        const second = BASE64_ALPHABET.indexOf(value[value.length - 3]);
        if ((second & 0x0f) !== 0) {
            throw new TypeError('Expected canonical padded Base64 data.');
        }
    } else if (value.endsWith('=')) {
        const third = BASE64_ALPHABET.indexOf(value[value.length - 2]);
        if ((third & 0x03) !== 0) {
            throw new TypeError('Expected canonical padded Base64 data.');
        }
    }
}

/** @internal */
export function bytesToBase64(data: ArrayBuffer | Uint8Array): string {
    const bytes = asBytes(data);
    let result = '';

    for (let index = 0; index < bytes.length; index += 3) {
        const first = bytes[index] ?? 0;
        const second = bytes[index + 1] ?? 0;
        const third = bytes[index + 2] ?? 0;
        const remaining = bytes.length - index;

        result += BASE64_ALPHABET[first >> 2];
        result += BASE64_ALPHABET[((first & 0x03) << 4) | (second >> 4)];
        result += remaining > 1
            ? BASE64_ALPHABET[((second & 0x0f) << 2) | (third >> 6)]
            : '=';
        result += remaining > 2 ? BASE64_ALPHABET[third & 0x3f] : '=';
    }

    return result;
}

/** @internal */
export function base64ToBytes(value: string): Uint8Array {
    validateBase64(value);
    if (value.length === 0) return new Uint8Array();

    let padding = 0;
    if (value.endsWith('==')) {
        padding = 2;
    } else if (value.endsWith('=')) {
        padding = 1;
    }
    const output = new Uint8Array((value.length / 4) * 3 - padding);
    let outputIndex = 0;

    for (let index = 0; index < value.length; index += 4) {
        const first = BASE64_ALPHABET.indexOf(value[index]);
        const second = BASE64_ALPHABET.indexOf(value[index + 1]);
        const third = value[index + 2] === '='
            ? 0
            : BASE64_ALPHABET.indexOf(value[index + 2]);
        const fourth = value[index + 3] === '='
            ? 0
            : BASE64_ALPHABET.indexOf(value[index + 3]);
        const combined =
            (first << 18) | (second << 12) | (third << 6) | fourth;

        if (outputIndex < output.length) output[outputIndex++] = combined >> 16;
        if (outputIndex < output.length) output[outputIndex++] = combined >> 8;
        if (outputIndex < output.length) output[outputIndex++] = combined;
    }

    return output;
}

/** @internal */
export function parseBase64DataUrl(dataUrl: string): ParsedBase64DataUrl {
    if (!dataUrl.startsWith('data:')) {
        throw new TypeError('Expected a Base64 data URL.');
    }

    const commaIndex = dataUrl.indexOf(',');
    if (commaIndex < 0) {
        throw new TypeError('Expected a Base64 data URL with a payload.');
    }

    const descriptor = dataUrl.slice(5, commaIndex);
    const parts = descriptor.split(';');
    const mediaType = parts.shift() ?? '';
    if (!MEDIA_TYPE_PATTERN.test(mediaType)) {
        throw new TypeError('Expected an explicit valid media type.');
    }
    if (parts.length !== 1 || parts[0].toLowerCase() !== 'base64') {
        throw new TypeError('Expected a Base64 data URL.');
    }

    const base64 = dataUrl.slice(commaIndex + 1);
    if (base64.length === 0) {
        throw new TypeError('Expected a Base64 data URL with a payload.');
    }
    validateBase64(base64);
    return { mediaType: mediaType.toLowerCase(), base64 };
}
