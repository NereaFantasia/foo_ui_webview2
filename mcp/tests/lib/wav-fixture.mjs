/**
 * Builds tiny audio files whose tag chunks are written byte by byte.
 *
 * The WAV tag guard is about what foobar2000's own writer does to RIFF INFO and
 * `id3 ` chunks, so a fixture built through the endpoint under test would prove
 * nothing. These files are made here, with the INFO text encoded the way a
 * Windows tool encodes it (GBK) and the id3 text as UTF-16.
 */

import { mkdirSync, writeFileSync } from "node:fs";
import { dirname } from "node:path";

const FRAMES = 4410;

function chunk(id, body, { bigEndian = false } = {}) {
    const head = Buffer.alloc(8);
    head.write(id, 0, "latin1");
    if (bigEndian) head.writeUInt32BE(body.length, 4);
    else head.writeUInt32LE(body.length, 4);
    const pad = !bigEndian && body.length & 1 ? Buffer.alloc(1) : Buffer.alloc(0);
    return Buffer.concat([head, body, pad]);
}

let gbkTable;

/** Encodes text as GBK; Node has a GBK decoder only, so the table is built from it. */
export function encodeGbk(text) {
    if (!gbkTable) {
        gbkTable = new Map();
        const decoder = new TextDecoder("gbk");
        for (let high = 0x81; high <= 0xfe; high++) {
            for (let low = 0x40; low <= 0xfe; low++) {
                if (low === 0x7f) continue;
                const char = decoder.decode(Uint8Array.of(high, low));
                const replacement = 0xfffd; // what the decoder returns for an unassigned code
                if (char.length === 1 && char.charCodeAt(0) !== replacement && !gbkTable.has(char)) {
                    gbkTable.set(char, [high, low]);
                }
            }
        }
    }
    const bytes = [];
    for (const char of text) {
        const code = char.charCodeAt(0);
        if (code < 0x80) bytes.push(code);
        else if (gbkTable.has(char)) bytes.push(...gbkTable.get(char));
        else throw new Error(`no GBK code for ${char}`);
    }
    return Buffer.from([...bytes, 0]);
}

/** LIST/INFO chunk; `fields` is [[subchunkId, text]] with the text stored as GBK. */
function infoChunk(fields) {
    const subchunks = fields.map(([id, text]) => chunk(id, encodeGbk(text)));
    return chunk("LIST", Buffer.concat([Buffer.from("INFO", "latin1"), ...subchunks]));
}

/** ID3v2.3 text frames in UTF-16 with a byte order mark; `frames` is [[frameId, text]]. */
function id3Body(frames) {
    const encoded = frames.map(([id, text]) => {
        const body = Buffer.concat([Buffer.from([1, 0xff, 0xfe]), Buffer.from(text, "utf16le")]);
        const head = Buffer.alloc(10);
        head.write(id, 0, "latin1");
        head.writeUInt32BE(body.length, 4);
        return Buffer.concat([head, body]);
    });
    const payload = Buffer.concat(encoded);
    const header = Buffer.alloc(10);
    header.write("ID3", 0, "latin1");
    header[3] = 3;
    const size = payload.length;
    header[6] = (size >> 21) & 127;
    header[7] = (size >> 14) & 127;
    header[8] = (size >> 7) & 127;
    header[9] = size & 127;
    return Buffer.concat([header, payload]);
}

/**
 * Writes a 0.1 s silent 16-bit stereo WAV.
 * `info` is [[subchunkId, text]] stored as GBK in a LIST/INFO chunk, `id3` is [[frameId, text]]
 * stored as UTF-16 in an `id3 ` chunk. Either may be omitted.
 */
export function writeWav(path, { info, id3 } = {}) {
    const format = Buffer.alloc(16);
    format.writeUInt16LE(1, 0);
    format.writeUInt16LE(2, 2);
    format.writeUInt32LE(44100, 4);
    format.writeUInt32LE(44100 * 4, 8);
    format.writeUInt16LE(4, 12);
    format.writeUInt16LE(16, 14);
    const body = Buffer.concat([
        Buffer.from("WAVE", "latin1"),
        chunk("fmt ", format),
        chunk("data", Buffer.alloc(FRAMES * 4)),
        ...(info ? [infoChunk(info)] : []),
        ...(id3 ? [chunk("id3 ", id3Body(id3))] : []),
    ]);
    const head = Buffer.alloc(8);
    head.write("RIFF", 0, "latin1");
    head.writeUInt32LE(body.length, 4);
    mkdirSync(dirname(path), { recursive: true });
    writeFileSync(path, Buffer.concat([head, body]));
}

/** Writes a 0.1 s silent 16-bit stereo AIFF with no tags. */
export function writeAiff(path) {
    const common = Buffer.alloc(18);
    common.writeUInt16BE(2, 0);
    common.writeUInt32BE(FRAMES, 2);
    common.writeUInt16BE(16, 6);
    Buffer.from([0x40, 0x0e, 0xac, 0x44, 0, 0, 0, 0, 0, 0]).copy(common, 8);
    const sound = Buffer.concat([Buffer.alloc(8), Buffer.alloc(FRAMES * 4)]);
    const body = Buffer.concat([
        Buffer.from("AIFF", "latin1"),
        chunk("COMM", common, { bigEndian: true }),
        chunk("SSND", sound, { bigEndian: true }),
    ]);
    const head = Buffer.alloc(8);
    head.write("FORM", 0, "latin1");
    head.writeUInt32BE(body.length, 4);
    mkdirSync(dirname(path), { recursive: true });
    writeFileSync(path, Buffer.concat([head, body]));
}
