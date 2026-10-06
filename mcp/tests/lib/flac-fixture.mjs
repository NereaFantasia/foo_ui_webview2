/**
 * Out-of-band FLAC tag fixtures for the real-instance e2e scripts.
 *
 * A regression case for multi-value tags needs a file whose values are known
 * before anything under test reads it, and it must not be tagged through the
 * bridge: using metadata.write to build the input would make the fixture and the
 * assertion share one implementation, so a value-loss bug would be invisible.
 *
 * These helpers therefore edit the VORBIS_COMMENT block directly. The audio
 * frames and the remaining metadata blocks are copied through untouched, so the
 * result is a playable file whose tags are the ones asked for.
 *
 * The source's PICTURE blocks are always dropped, so a fixture never inherits
 * artwork: "this copy starts with no cover" must not depend on which track the
 * library happened to offer as the smallest one. A fixture that needs embedded
 * artwork asks for it by passing `pictures`, which are built here from a
 * synthesized PNG of known dimensions and bytes - so the artwork read endpoints
 * are checked against a picture nothing under test produced.
 *
 * FLAC layout: "fLaC", then metadata blocks of [1 bit last][7 bit type][24 bit
 * big-endian length][payload], then audio frames. VORBIS_COMMENT is type 4,
 * PICTURE is type 6, and the comment payload is little-endian, unlike the block
 * headers and the picture payload.
 */

import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname } from "node:path";
import { crc32, deflateSync } from "node:zlib";

const BLOCK_VORBIS_COMMENT = 4;
const BLOCK_PICTURE = 6;
const MAX_BLOCK_SIZE = 0xffffff;

/** APIC picture types, as FLAC borrows them from ID3v2. */
export const PICTURE_TYPE_FRONT_COVER = 3;
export const PICTURE_TYPE_BACK_COVER = 4;

export function parseFlac(buffer) {
    if (buffer.length < 4 || buffer.toString("latin1", 0, 4) !== "fLaC") {
        throw new Error("not a FLAC stream: missing fLaC marker");
    }

    const blocks = [];
    let offset = 4;
    for (;;) {
        if (offset + 4 > buffer.length) {
            throw new Error("truncated FLAC metadata block header");
        }
        const header = buffer[offset];
        const last = (header & 0x80) !== 0;
        const type = header & 0x7f;
        const length = buffer.readUIntBE(offset + 1, 3);
        const start = offset + 4;
        const end = start + length;
        if (end > buffer.length) {
            throw new Error("truncated FLAC metadata block payload");
        }
        blocks.push({ type, payload: buffer.subarray(start, end) });
        offset = end;
        if (last) break;
    }
    return { blocks, audioOffset: offset };
}

export function decodeVorbisComment(payload) {
    let offset = 0;
    const vendorLength = payload.readUInt32LE(offset);
    offset += 4;
    const vendor = payload.toString("utf8", offset, offset + vendorLength);
    offset += vendorLength;
    const count = payload.readUInt32LE(offset);
    offset += 4;

    const entries = [];
    for (let i = 0; i < count; i += 1) {
        const length = payload.readUInt32LE(offset);
        offset += 4;
        const text = payload.toString("utf8", offset, offset + length);
        offset += length;
        const separator = text.indexOf("=");
        entries.push(
            separator === -1
                ? [text, ""]
                : [text.slice(0, separator), text.slice(separator + 1)],
        );
    }
    return { vendor, entries };
}

export function encodeVorbisComment({ vendor, entries }) {
    const vendorBuffer = Buffer.from(vendor, "utf8");
    const entryBuffers = entries.map(([key, value]) =>
        Buffer.from(`${key}=${value}`, "utf8"),
    );
    const size =
        8 +
        vendorBuffer.length +
        entryBuffers.reduce((total, entry) => total + 4 + entry.length, 0);

    const out = Buffer.alloc(size);
    let offset = out.writeUInt32LE(vendorBuffer.length, 0);
    offset += vendorBuffer.copy(out, offset);
    offset = out.writeUInt32LE(entryBuffers.length, offset);
    for (const entry of entryBuffers) {
        offset = out.writeUInt32LE(entry.length, offset);
        offset += entry.copy(out, offset);
    }
    return out;
}

function pngChunk(tag, body) {
    const header = Buffer.alloc(8);
    header.writeUInt32BE(body.length, 0);
    header.write(tag, 4, "latin1");
    const crcInput = Buffer.concat([header.subarray(4), body]);
    const trailer = Buffer.alloc(4);
    trailer.writeUInt32BE(crc32(crcInput) >>> 0, 0);
    return Buffer.concat([header, body, trailer]);
}

/**
 * A valid truecolour PNG of one flat colour, used because the artwork cases need
 * dimensions and a byte count they decided rather than inherited. Kept tiny: the
 * bytes travel back over the bridge base64-encoded inside a data URL.
 */
export function makeSolidPng({ width, height, rgb = [0x40, 0x80, 0xc0] }) {
    const ihdr = Buffer.alloc(13);
    ihdr.writeUInt32BE(width, 0);
    ihdr.writeUInt32BE(height, 4);
    ihdr[8] = 8; // bit depth
    ihdr[9] = 2; // colour type: truecolour
    // compression, filter and interlace all stay 0, the only values PNG defines.

    const stride = width * 3;
    const raw = Buffer.alloc(height * (stride + 1));
    for (let y = 0; y < height; y += 1) {
        const rowStart = y * (stride + 1);
        raw[rowStart] = 0; // per-scanline filter: none
        for (let x = 0; x < width; x += 1) {
            const at = rowStart + 1 + x * 3;
            raw[at] = rgb[0];
            raw[at + 1] = rgb[1];
            raw[at + 2] = rgb[2];
        }
    }

    return Buffer.concat([
        Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
        pngChunk("IHDR", ihdr),
        pngChunk("IDAT", deflateSync(raw)),
        pngChunk("IEND", Buffer.alloc(0)),
    ]);
}

/**
 * Builds a PICTURE block payload. Every field is big-endian here, unlike the
 * VORBIS_COMMENT payload alongside it.
 */
export function encodePicture({
    type = PICTURE_TYPE_FRONT_COVER,
    mimeType = "image/png",
    description = "",
    width,
    height,
    depth = 24,
    colors = 0,
    data,
}) {
    const mime = Buffer.from(mimeType, "latin1");
    const desc = Buffer.from(description, "utf8");
    const out = Buffer.alloc(32 + mime.length + desc.length + data.length);
    let offset = out.writeUInt32BE(type, 0);
    offset = out.writeUInt32BE(mime.length, offset);
    offset += mime.copy(out, offset);
    offset = out.writeUInt32BE(desc.length, offset);
    offset += desc.copy(out, offset);
    offset = out.writeUInt32BE(width, offset);
    offset = out.writeUInt32BE(height, offset);
    offset = out.writeUInt32BE(depth, offset);
    offset = out.writeUInt32BE(colors, offset);
    offset = out.writeUInt32BE(data.length, offset);
    data.copy(out, offset);
    return out;
}

export function decodePicture(payload) {
    let offset = 0;
    const type = payload.readUInt32BE(offset);
    offset += 4;
    const mimeLength = payload.readUInt32BE(offset);
    offset += 4;
    const mimeType = payload.toString("latin1", offset, offset + mimeLength);
    offset += mimeLength;
    const descLength = payload.readUInt32BE(offset);
    offset += 4;
    const description = payload.toString("utf8", offset, offset + descLength);
    offset += descLength;
    const width = payload.readUInt32BE(offset);
    const height = payload.readUInt32BE(offset + 4);
    const depth = payload.readUInt32BE(offset + 8);
    const colors = payload.readUInt32BE(offset + 12);
    const dataLength = payload.readUInt32BE(offset + 16);
    offset += 20;
    return {
        type,
        mimeType,
        description,
        width,
        height,
        depth,
        colors,
        data: payload.subarray(offset, offset + dataLength),
    };
}

/**
 * Replaces the whole tag set, drops the source's embedded pictures and inserts
 * the requested ones, keeping every other block and the audio frames.
 */
export function setVorbisComments(
    buffer,
    entries,
    { vendor = "foo_ui_webview2 e2e fixture", pictures = [] } = {},
) {
    const { blocks, audioOffset } = parseFlac(buffer);
    const payload = encodeVorbisComment({ vendor, entries });
    if (payload.length > MAX_BLOCK_SIZE) {
        throw new Error("VORBIS_COMMENT payload exceeds the 24-bit block length");
    }

    const rewritten = blocks.filter(
        (block) => block.type !== BLOCK_VORBIS_COMMENT && block.type !== BLOCK_PICTURE,
    );
    // After STREAMINFO, which the format requires to come first.
    rewritten.splice(1, 0, { type: BLOCK_VORBIS_COMMENT, payload });
    for (const picture of pictures) {
        const picturePayload = encodePicture(picture);
        if (picturePayload.length > MAX_BLOCK_SIZE) {
            throw new Error("PICTURE payload exceeds the 24-bit block length");
        }
        rewritten.push({ type: BLOCK_PICTURE, payload: picturePayload });
    }

    const parts = [Buffer.from("fLaC", "latin1")];
    rewritten.forEach((block, index) => {
        const header = Buffer.alloc(4);
        header[0] = (index === rewritten.length - 1 ? 0x80 : 0) | block.type;
        header.writeUIntBE(block.payload.length, 1, 3);
        parts.push(header, block.payload);
    });
    parts.push(buffer.subarray(audioOffset));
    return Buffer.concat(parts);
}

/**
 * Writes a retagged copy and reads it back with this same parser, so a caller
 * starts from a verified ground truth rather than from an assumption about what
 * landed on disk. Throws when the tags did not round trip, or when the pictures
 * on disk are not exactly the ones asked for - which for the default of none
 * also means no picture survived from the source.
 */
export function writeTaggedFlacCopy({ sourcePath, targetPath, entries, pictures = [] }) {
    const patched = setVorbisComments(readFileSync(sourcePath), entries, { pictures });
    mkdirSync(dirname(targetPath), { recursive: true });
    writeFileSync(targetPath, patched);

    const readBack = parseFlac(readFileSync(targetPath));
    const commentBlock = readBack.blocks.find(
        (block) => block.type === BLOCK_VORBIS_COMMENT,
    );
    if (!commentBlock) {
        throw new Error("fixture lost its VORBIS_COMMENT block");
    }
    const pictureBlocks = readBack.blocks.filter((block) => block.type === BLOCK_PICTURE);
    if (pictureBlocks.length !== pictures.length) {
        throw new Error(
            `fixture holds ${pictureBlocks.length} picture blocks, asked for ${pictures.length}`,
        );
    }
    const writtenPictures = pictureBlocks.map((block) => decodePicture(block.payload));
    writtenPictures.forEach((written, index) => {
        const asked = pictures[index];
        if (
            written.type !== (asked.type ?? PICTURE_TYPE_FRONT_COVER) ||
            written.width !== asked.width ||
            written.height !== asked.height ||
            written.data.length !== asked.data.length ||
            !written.data.equals(asked.data)
        ) {
            throw new Error(`fixture picture ${index} did not round trip`);
        }
    });
    const decoded = decodeVorbisComment(commentBlock.payload);
    const expected = JSON.stringify(entries);
    const actual = JSON.stringify(decoded.entries);
    if (actual !== expected) {
        throw new Error(`fixture round trip mismatch: ${actual} !== ${expected}`);
    }
    return {
        targetPath,
        entries: decoded.entries,
        vendor: decoded.vendor,
        pictures: writtenPictures,
    };
}

/** Collects the values of one tag key in file order, keeping duplicates. */
export function valuesOf(entries, key) {
    const wanted = key.toLowerCase();
    return entries
        .filter(([name]) => name.toLowerCase() === wanted)
        .map(([, value]) => value);
}
