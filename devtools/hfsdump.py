#!/usr/bin/env python3
"""Dump an HFS (classic) partition from a disk image with machfs.

usage: hfsdump.py IMAGE BYTE_OFFSET BYTE_LENGTH OUTDIR
Writes data forks as files, resource forks as FILE.rsrc, and type/creator
to FILE.finfo. Prints a manifest line per file.
"""
import os
import sys

import machfs


def safe(part):
    return part.replace("/", ":") or "_"


def main(image, offset, length, outdir):
    with open(image, "rb") as f:
        f.seek(offset)
        raw = f.read(length)
    vol = machfs.Volume()
    vol.read(raw)
    print(f"volume: {vol.name!r}")
    nfiles = 0
    for path, obj in vol.iter_paths():
        if not isinstance(obj, machfs.File):
            continue
        rel = os.path.join(*[safe(p) for p in path])
        dst = os.path.join(outdir, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, "wb") as fh:
            fh.write(obj.data)
        if obj.rsrc:
            with open(dst + ".rsrc", "wb") as fh:
                fh.write(obj.rsrc)
        tc = obj.type.decode("mac_roman") + "/" + obj.creator.decode("mac_roman")
        with open(dst + ".finfo", "w") as fh:
            fh.write(tc.replace("/", "\t") + "\n")
        print(f"{tc}\t{len(obj.data)}\t{len(obj.rsrc)}\t{rel}")
        nfiles += 1
    print(f"files: {nfiles}")


if __name__ == "__main__":
    main(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4])
