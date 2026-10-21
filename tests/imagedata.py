"""Unit v5L39: image tests of tables of initialised data.

The host side is the table as its test library was loaded (tests/image_copies.h, copied before any case writes it). The original side is the
program image's bytes at the Mac address, read from the data section's dump; the emulator's copy is not used, because the other tests of a
module write over it. One case per element, decoded with the table's struct format (big-endian for the image, little-endian for the host),
plus one case for the table's size. Floats are compared as their bits ("I"), so the test is exact.
"""
import ctypes as C
import struct

from tpwtest import FnTest

_BLOB = {}


def image_bytes(emu, addr, size):
    """size bytes of the program image's initialised data at Mac address addr."""
    data = next(b for b in emu.blocks if b["initialized"] and not b["execute"] and b["name"] != "IMPORTS")
    if "blob" not in _BLOB:
        _BLOB["blob"] = (emu.dir / data["file"]).read_bytes()
    lo = addr - data["start"]
    assert 0 <= lo and lo + size <= len(_BLOB["blob"]), hex(addr)
    return _BLOB["blob"][lo:lo + size]


def _bind(lib):
    for name in ("tpw_image_copy", "tpw_image_size"):
        f = getattr(lib, name)
        f.restype, f.argtypes = C.c_uint64, [C.c_char_p]


def table_test(title, tables):
    """An FnTest over tables, each (label, Mac address, element format without byte order, element count)."""
    spec = {t[0]: t for t in tables}

    def ppc(e, case):
        label, addr, fmt, count = spec[case["t"]]
        size = struct.calcsize(">" + fmt)
        if case["i"] < 0:
            return {"size": size * count}
        return {"v": list(struct.unpack(">" + fmt, image_bytes(e, addr + case["i"] * size, size)))}

    def host(lib, case):
        _bind(lib)
        label, addr, fmt, count = spec[case["t"]]
        size = struct.calcsize("<" + fmt)
        if case["i"] < 0:
            return {"size": lib.tpw_image_size(label.encode())}
        p = lib.tpw_image_copy(label.encode())
        if not p or (case["i"] + 1) * size > lib.tpw_image_size(label.encode()):
            return {"v": None}
        return {"v": list(struct.unpack("<" + fmt, C.string_at(p + case["i"] * size, size)))}

    def edges():
        for label, addr, fmt, count in tables:
            yield dict(t=label, i=-1)
            for i in range(count):
                yield dict(t=label, i=i)

    def rnd(r):
        label, addr, fmt, count = r.choice(tables)
        return dict(t=label, i=r.randrange(count))

    return FnTest(title, ppc, host, edge=edges, rnd=rnd)
