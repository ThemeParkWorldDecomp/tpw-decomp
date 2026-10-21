"""Unit v5127, the ride height map loaders, against libtpw_unit_LoadFileIntoMemory.dylib (tests/unit_LoadFileIntoMemory.cpp), main app
("SimTheme Park"), src/game/park/TPHField.cpp:
  LoadFileIntoMemory 0x1005e300, LoadRideHeightMap 0x1005e40c, GetRideHeightMap 0x1005e8e8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory (big-endian) for the original and in
the host library's memory (little-endian) for the translation. The file system (open, size, read, close), the allocator, the three byte swaps,
the box's Byteswap, GenerateRideHeightMap and SaveRideHeightMap are fakes on both sides that record their arguments (pointers as classes: the
file block, the generated map, other) and answer from the case's queues. The swaps leave the words alone, so the case writes the file's words
already in their final form (the two halves at 8 and 10 are written as halves).

The world of a case: the file's bytes, a set with a mesh and its header block, a generated map block. What is compared: the answer, the
loaded map's words (the three offsets made relative to the block), the header words 0x80..0x97, the set's height map pointer (as a class)
and the event log.

No fused operations. Not tested: a negative file size (it would be a 4 GB allocation request), a NULL map out of GenerateRideHeightMap (the
original reads through it without a check).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LoadFileIntoMemory"
MASK = 0xFFFFFFFF

FN_LFM, FN_LRHM, FN_GRHM = 0x1005E300, 0x1005E40C, 0x1005E8E8
SIZES = {FN_LFM: 224, FN_LRHM: 224, FN_GRHM: 200}
OWN = set(SIZES)

OPEN, SIZE, READ, CLOSE = 0x10003FD4, 0x100044E8, 0x10004260, 0x10004354
ALLOC, FREE = 0x10064BD4, 0x10064C54
SW32, SW16, SW32P, BYTESWAP = 0x1005E658, 0x1005E610, 0x1005E5B4, 0x1005E52C
GEN, SAVE = 0x1005DB4C, 0x1005E6A0

HOST_LAYOUT = [0x0C, 0x18, 0x24, 0x30]      # sRideHeightMap: mCells, mMin, mMax, sizeof (the rest of the layout is the host's own)
MAGIC0, MAGIC1 = 0xAB1E0003, 0x640005


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cls(e, p):
    p &= MASK
    if p == 0:
        return 0
    if p == e.tw["last"]:
        return 1
    if p == e.tw["gen"]:
        return 2
    return 3


def off(e, p):
    return (p - e.tw["last"]) & MASK


def stub_alloc(e):
    L = e.cl
    size = e.r(3) & MASK
    L.rec("Alloc", (size, e.r(4) & MASK))
    if L.ret("Alloc"):
        a = e.alloc(size + 0x80)
        e.tw["last"] = a
        e.set_r(3, a)
    else:
        e.set_r(3, 0)


def stub_read(e):
    L = e.cl
    dest, size, count, f = e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK
    L.rec("Read", (size, count, f, cls(e, dest)))
    n = min(size * count, len(e.tw["data"]))
    if dest:
        e.write(dest, e.tw["data"][:n])
    e.set_r(3, L.ret("Read") & MASK)


def stub_close(e):
    e.cl.rec("Close", (e.r(3) & MASK,))
    e.set_r(3, 0)


def stub_free(e):
    e.cl.rec("Free", (cls(e, e.r(3)),))
    e.set_r(3, 0)


def stub_gen(e):
    L = e.cl
    L.rec("Gen", (int((e.r(3) & MASK) == e.tw["set"]), e.r(4) & MASK, e.r(5) & MASK, int((e.r(6) & MASK) == e.tw["shape"]), s32(e.r(7))))
    e.set_r(3, e.tw["gen"] if L.ret("Gen") else 0)


def stub_save(e):
    e.cl.rec("Save", (cls(e, e.r(3)),), (e.cstr(e.r(4)),))


def stub_sw32(e):
    p = e.r(3) & MASK
    e.cl.rec("Swap32", (off(e, p), e.u32(p)))


def stub_sw16(e):
    p = e.r(3) & MASK
    e.cl.rec("Swap16", (off(e, p), struct.unpack(">H", e.read(p, 2))[0]))


def stub_sw32p(e):
    e.cl.rec("Swap32p", (e.r(3) & MASK,))


def stub_byteswap(e):
    e.cl.rec("Byteswap", (off(e, e.r(3) & MASK),))


STUBS.internal(OPEN, CL.recorder("Open", "ss", ret="i"))
STUBS.internal(SIZE, CL.recorder("Size", "u", ret="i"))
STUBS.internal(READ, stub_read)
STUBS.internal(CLOSE, stub_close)
STUBS.internal(ALLOC, stub_alloc)
STUBS.internal(FREE, stub_free)
STUBS.internal(GEN, stub_gen)
STUBS.internal(SAVE, stub_save)
STUBS.internal(SW32, stub_sw32)
STUBS.internal(SW16, stub_sw16)
STUBS.internal(SW32P, stub_sw32p)
STUBS.internal(BYTESWAP, stub_byteswap)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def cstring(self, text):
        a = self.alloc(len(text) + 8)
        self.write(a, text.encode() + b"\0")
        return a

    def file_bytes(self, d):
        """The file image of a case (words and halves in this side's byte order)."""
        b = struct.pack(self.fmt, d["m0"] & MASK) + struct.pack(self.fmt, d["m1"] & MASK)
        b += struct.pack(self.fmt[0] + "HH", d["w"] & 0xFFFF, d["h"] & 0xFFFF)
        b += b"".join(struct.pack(self.fmt, x & MASK) for x in [d["o3"], d["o4"], d["o5"]] + d["box"] + d["extra"])
        return b[:4 * d["len"]]       # "len" counts words, so a cut means the same on both sides


class Ppc(Side):
    fmt = ">I"
    ptr = 4
    OFF_HEADS, OFF_HM, OFF_HEADER = 8, 0xCC, 4

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        e.tw = {"set": 0, "gen": 0, "shape": 0, "data": b"", "last": 0}
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def reloc(self, a):
        """What the loader adds to the map's three offsets: the block's address."""
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def state(self, st, gen, shape, data):
        self.e.tw.update({"set": st, "gen": gen, "shape": shape, "data": data})

    def last(self):
        return self.e.tw["last"]

    def put_ptr(self, a, v):
        self.write(a, struct.pack(">I", v & MASK))

    def get_ptr(self, a):
        return struct.unpack(">I", self.read(a, 4))[0]

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [x & MASK for x in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    ptr = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.OFF_HEADS, self.OFF_HM, self.OFF_HEADER = lib.lay[0], lib.lay[1], lib.lay[2]
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_lfm_alloc(n + 0x40)

    def reloc(self, a):
        """What the loader adds to the map's three offsets: the block's arena offset (Ptr32 words)."""
        return self.lib.tpw_lfm_offset(a)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def state(self, st, gen, shape, data):
        self.keep = C.create_string_buffer(data, len(data) + 1)
        self.lib.tpw_lfm_state(st, gen, shape, C.cast(self.keep, C.c_void_p), len(data))

    def last(self):
        return self.lib.tpw_lfm_last()

    def put_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def get_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def call(self, entry, args):
        lib = self.lib
        if entry == FN_LFM:
            return lib.tpw_lfm_lfm(args[0], s32(args[1]))
        if entry == FN_LRHM:
            return lib.tpw_lfm_lrhm(*[a & MASK if i in (1, 2) else a for i, a in enumerate(args)])
        lib.tpw_lfm_grhm(args[0], args[1] & MASK, args[2] & MASK, args[3], args[4], s32(args[5]))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lfm_alloc.restype, lib.tpw_lfm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lfm_last.restype, lib.tpw_lfm_last.argtypes = C.c_uint64, []
    lib.tpw_lfm_offset.restype, lib.tpw_lfm_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_lfm_state.restype = None
    lib.tpw_lfm_state.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64, C.c_void_p, C.c_uint32]
    lib.tpw_lfm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lfm_lfm.restype, lib.tpw_lfm_lfm.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_lfm_lrhm.restype, lib.tpw_lfm_lrhm.argtypes = C.c_uint64, [C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint64]
    lib.tpw_lfm_grhm.restype = None
    lib.tpw_lfm_grhm.argtypes = [C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lfm_layout(out)
    assert list(out)[:4] == HOST_LAYOUT, list(out)[:n]
    lib.lay = list(out)[4:n]
    lib.bound = True


def build(s, case):
    """Path string, file, set with mesh and header, generated map: the same code on both sides."""
    c = {}
    c["path"] = s.cstring(case["path"])
    c["gen"] = s.alloc(0x80)
    s.words(c["gen"] + 0x18, case["genbox"])
    c["shape"] = s.alloc(16) if case["shape"] else 0
    c["set"] = s.alloc(0x400)
    c["mesh"] = s.alloc(0x100)
    c["header"] = s.alloc(0x200)
    s.words(c["header"] + 0x70, case["hdr"])
    s.put_ptr(c["set"] + s.OFF_HEADS, c["mesh"])
    s.put_ptr(c["mesh"] + s.OFF_HEADER, c["header"])
    s.state(c["set"], c["gen"], c["shape"], s.file_bytes(case["data"]))
    return c


def raw_words(s, base):
    ws = s.rd_words(base, 24)
    ws[2] = struct.unpack(s.fmt[0] + "HH", s.read(base + 8, 4))
    return ws


def blockwords(s, base):
    ws = s.rd_words(base, 12)
    ws[2] = struct.unpack(s.fmt[0] + "HH", s.read(base + 8, 4))      # the halves at 8 and 10 (w, h in memory order)
    for i in (3, 4, 5):
        ws[i] = (ws[i] - s.reloc(base)) & MASK
    return ws


def run(s, case):
    try:
        c = build(s, case)
        op = case["op"]
        w, h = case["wh"]
        if op == "lfm":
            r = s.call(FN_LFM, [c["path"], case["binary"]])
            out = {"ans": int(r != 0), "data": raw_words(s, r) if r else None}
        elif op == "lrhm":
            r = s.call(FN_LRHM, [c["set"], w, h, c["path"]])
            out = {"ans": int(r != 0), "words": blockwords(s, r) if r else None}
        else:
            s.call(FN_GRHM, [c["set"], w, h, c["path"], c["shape"], case["flag"]])
            hm = s.get_ptr(c["set"] + s.OFF_HM)
            k = 0 if hm == 0 else 1 if hm == s.last() else 2 if hm == c["gen"] else 3
            out = {"hm": k, "hdr": s.rd_words(c["header"] + 0x70, 12), "gen": s.rd_words(c["gen"], 12)}
        out["log"] = s.log()
        return out
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

def rw(r):
    return r.choice([0, 1, 0xFFFFFFFF, 0x80000000, r.getrandbits(32), r.getrandbits(32)])


def make(r, op, **kw):
    """A case: every field random unless given."""
    good = kw.get("good", r.random() < 0.6)
    w, h = r.randrange(0, 65536), r.randrange(0, 65536)
    d = {"m0": MAGIC0 if good or r.random() < 0.5 else rw(r), "m1": MAGIC1 if good or r.random() < 0.5 else rw(r),
         "w": w, "h": h, "o3": r.randrange(0x30, 0x200), "o4": r.randrange(0, 0x400), "o5": rw(r),
         "box": [rw(r) for _ in range(6)], "extra": [rw(r) for _ in range(r.randrange(0, 20))], "len": 10 ** 6}
    if "magic" in kw:
        d["m0"], d["m1"] = kw["magic"]
    if r.random() < 0.1:
        d["len"] = r.randrange(0, 16)
    d.update(kw.get("data", {}))
    size = min(len(Ppc.file_bytes(Ppc, d)), 0x3FF) if kw.get("size") is None else kw["size"]
    match = kw.get("match", r.random() < 0.5)
    q = {"Open": [kw.get("open", r.choice([0, 0x7700, 0x7700, 0x7700]))], "Size": [size],
         "Alloc": [kw.get("alloc", r.choice([0, 1, 1, 1]))], "Read": [kw.get("read", r.choice([0, 1, 1, 1, 5]))], "Gen": [1]}
    c = {"op": op, "queues": q, "data": d, "path": "".join(r.choice("abc/._") for _ in range(r.randrange(0, 12))),
         "binary": kw.get("binary", r.choice([0, 1, 1, -1, 7])), "wh": (d["w"], d["h"]) if match else (rw(r) & 0xFFFF, rw(r) & 0xFFFF),
         "flag": r.choice([0, 1, 0x800, -3]), "shape": r.random() < 0.5, "genbox": [rw(r) for _ in range(12)],
         "hdr": [rw(r) for _ in range(12)]}
    if kw.get("matchw"):
        c["wh"] = (d["w"], rw(r) & 0xFFFF)
    if "matchh" in kw:
        c["wh"] = (rw(r) & 0xFFFF, d["h"])
    return c


def Ppc_file_bytes_fix():
    pass


Ppc.file_bytes = Side.file_bytes


def edges(op):
    def gen():
        r = random.Random({"lfm": 11, "lrhm": 12, "grhm": 13}[op])
        for open_ in (0, 0x7700):
            for alloc in (0, 1):
                for read in (0, 1, 5):
                    for binary in (0, 1, -1):
                        yield make(r, op, open=open_, alloc=alloc, read=read, binary=binary, good=True, match=True, size=r.choice([0, 4, 0x60]))
        for magic in ((MAGIC0, MAGIC1), (MAGIC0 ^ 1, MAGIC1), (MAGIC0, MAGIC1 ^ 1), (0, 0), (MAGIC0 + 1, MAGIC1 - 1)):
            for match in (True, False):
                yield make(r, op, magic=magic, match=match, open=0x7700, alloc=1, read=1, size=0x60)
        yield make(r, op, matchw=True, match=False, good=True, open=0x7700, alloc=1, read=1, size=0x60)
        yield make(r, op, matchh=True, match=False, good=True, open=0x7700, alloc=1, read=1, size=0x60)
        yield make(r, op, good=True, match=True, open=0x7700, alloc=1, read=1, size=0x60, data={"w": 0x1FFFF, "h": 0})
    return gen


def rnd(op):
    return lambda r: make(r, op)


TESTS = [FnTest(name, ppc, host, edge=edges(op), rnd=rnd(op))
         for name, op in (("LoadFileIntoMemory", "lfm"), ("LoadRideHeightMap", "lrhm"), ("GetRideHeightMap", "grhm"))]
