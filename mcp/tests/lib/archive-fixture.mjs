import { crc32 } from "node:zlib";

/** 将给定字节放进无压缩 ZIP，条目名按 UTF-8 编码。 */
export function storedZip(entries) {
    const local = [];
    const central = [];
    let offset = 0;
    for (const [name, data] of entries) {
        const filename = Buffer.from(name, "utf8");
        const checksum = crc32(data) >>> 0;
        const header = Buffer.alloc(30);
        header.writeUInt32LE(0x04034b50, 0);
        header.writeUInt16LE(20, 4);
        header.writeUInt16LE(0x800, 6);
        header.writeUInt32LE(checksum, 14);
        header.writeUInt32LE(data.length, 18);
        header.writeUInt32LE(data.length, 22);
        header.writeUInt16LE(filename.length, 26);
        local.push(header, filename, data);
        const directory = Buffer.alloc(46);
        directory.writeUInt32LE(0x02014b50, 0);
        directory.writeUInt16LE(20, 4);
        directory.writeUInt16LE(20, 6);
        directory.writeUInt16LE(0x800, 8);
        directory.writeUInt32LE(checksum, 16);
        directory.writeUInt32LE(data.length, 20);
        directory.writeUInt32LE(data.length, 24);
        directory.writeUInt16LE(filename.length, 28);
        directory.writeUInt32LE(offset, 42);
        central.push(directory, filename);
        offset += header.length + filename.length + data.length;
    }
    const directory = Buffer.concat(central);
    const end = Buffer.alloc(22);
    end.writeUInt32LE(0x06054b50, 0);
    end.writeUInt16LE(entries.length, 8);
    end.writeUInt16LE(entries.length, 10);
    end.writeUInt32LE(directory.length, 12);
    end.writeUInt32LE(offset, 16);
    return Buffer.concat([...local, directory, end]);
}

/** 与 fb2k 的 unpack 格式一致，容器长度是 UTF-8 字节数。 */
export function unpackPath(container, entry, type = "zip") {
    return `unpack://${type}|${Buffer.byteLength(container, "utf8")}|${container}|${entry}`;
}
