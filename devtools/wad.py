#!/usr/bin/env python3
"""Read Bullfrog BFWD ('DWFB') WAD archives, including RefPack entries.

Layout (matches opentpw-docs bfwd.md, verified on the Mac and PC Gold data):
88-byte header, file count u32 at 0x48; 40-byte little-endian entries from
0x58: unknown, name offset, name length, data offset, stored length,
compression (0 stored, 4 RefPack), decompressed size, 12 bytes padding.

usage: wad.py list WAD...           print entries
       wad.py extract WAD OUTDIR    write every entry (decompressed)
"""
import os
import struct
import sys


def refpack_decompress(src):
    """EA RefPack/QFS. Header: flags byte, 0xFB, [compressed size if
    flags&1], uncompressed size (3 bytes, or 4 if flags&0x80), big-endian."""
    flags, magic = src[0], src[1]
    if magic != 0xFB:
        raise ValueError("not RefPack")
    n = 4 if flags & 0x80 else 3
    p = 2 + (n if flags & 0x01 else 0)
    size = int.from_bytes(src[p:p + n], "big")
    p += n
    out = bytearray()
    while p < len(src):
        b0 = src[p]
        if b0 < 0x80:
            b1 = src[p + 1]
            plain = b0 & 3
            copy = ((b0 & 0x1C) >> 2) + 3
            off = ((b0 & 0x60) << 3) + b1 + 1
            p += 2
        elif b0 < 0xC0:
            b1, b2 = src[p + 1], src[p + 2]
            plain = b1 >> 6
            copy = (b0 & 0x3F) + 4
            off = ((b1 & 0x3F) << 8) + b2 + 1
            p += 3
        elif b0 < 0xE0:
            b1, b2, b3 = src[p + 1], src[p + 2], src[p + 3]
            plain = b0 & 3
            copy = ((b0 & 0x0C) << 6) + b3 + 5
            off = ((b0 & 0x10) << 12) + (b1 << 8) + b2 + 1
            p += 4
        elif b0 < 0xFC:
            plain = ((b0 & 0x1F) << 2) + 4
            copy = 0
            off = 0
            p += 1
        else:
            plain = b0 & 3
            out += src[p + 1:p + 1 + plain]
            break
        out += src[p:p + plain]
        p += plain
        for _ in range(copy):
            out.append(out[-off])
    if len(out) != size:
        raise ValueError(f"RefPack size {len(out)} != {size}")
    return bytes(out)


def entries(b):
    """Yield (path, compression, stored length, size, data offset).
    A directory prefix ("textures\\x.wct") is sticky: it also applies to
    the following entries until another prefix appears."""
    if b[:4] != b"DWFB":
        raise ValueError("not a DWFB WAD")
    count = struct.unpack_from("<I", b, 0x48)[0]
    cwd = ""
    for i in range(count):
        (_, name_off, name_len, data_off, length, comp,
         usize) = struct.unpack_from("<7I", b, 0x58 + 40 * i)
        name = b[name_off:name_off + name_len].split(b"\0")[0].decode(
            "latin-1")
        if "\\" in name:
            cwd, name = name.rsplit("\\", 1)
        yield (cwd + "\\" + name if cwd else name), comp, length, usize, \
            data_off


def read_entry(b, comp, length, data_off):
    raw = b[data_off:data_off + length]
    if comp == 0:
        return raw
    if comp == 4:
        return refpack_decompress(raw)
    raise ValueError(f"unknown compression {comp}")


def main(argv):
    if argv[0] == "list":
        for path in argv[1:]:
            b = open(path, "rb").read()
            for name, comp, length, usize, _ in entries(b):
                print(f"{path}\t{name}\t{comp}\t{length}\t{usize}")
    elif argv[0] == "extract":
        b = open(argv[1], "rb").read()
        for name, comp, length, usize, off in entries(b):
            dst = os.path.join(argv[2], *[p for p in name.split("\\") if p])
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(dst, "wb") as f:
                f.write(read_entry(b, comp, length, off))


if __name__ == "__main__":
    main(sys.argv[1:])
