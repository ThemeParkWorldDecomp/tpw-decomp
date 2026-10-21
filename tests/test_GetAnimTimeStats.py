"""Unit v5576, the ride animation time statistics, against libtpw_unit_GetAnimTimeStats.dylib (tests/unit_GetAnimTimeStats.cpp), main app
("SimTheme Park"), src/game/rides/RideAnim.cpp:
  GetAnimTimeStats 0x100c5bcc, RestoreAnimTimeStats 0x100c5c68, GetNumActiveChannels 0x100c5fd4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. No callees.

The world of a case: a time control (14 words), an optional time word, an optional animation header, and a ride set (mesh, mesh header
flags word, time controls) whose pointers are 4 bytes on the Mac and 8 on the host (host offsets from the layout export). What is
compared: the control words, the time word, the animation words, the header flags and the set's count word.

No fused operations. Not tested: a NULL control or set (read unconditionally).
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_GetAnimTimeStats"
MASK = 0xFFFFFFFF

FN_GET, FN_RESTORE, FN_CHAN = 0x100c5bcc, 0x100c5c68, 0x100c5fd4

# host layout: set heads, timeCount, timeControls, count; mesh header; control 0c 10 14 20 size; anim 04 08; header flags
HOST_LAYOUT_EXPECTED_TAIL = [0x0c, 0x10, 0x14, 0x20, 0x38, 0x04, 0x08, 0x30]
HL = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def u16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))


class Ppc(Side):
    fmt = ">I"
    O = dict(heads=0x08, count_n=0x0e, tcs=0x10, count=0x14, header=0x04)

    def __init__(self, e, case):
        self.e = e

    def done(self):
        pass

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a, v):
        self.write(a, struct.pack(">I", v & MASK))

    def call(self, entry, args):
        T.call_ppc(self.e, entry, args)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.O = dict(heads=HL[0], count_n=HL[1], tcs=HL[2], count=HL[3], header=HL[4])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_gat_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def call(self, entry, args):
        name = {FN_GET: "get", FN_RESTORE: "restore", FN_CHAN: "chan"}[entry]
        getattr(self.lib, "tpw_gat_" + name)(*args)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_gat_"
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "get").argtypes = [C.c_uint64, C.c_uint64]
    getattr(lib, p + "restore").argtypes = [C.c_uint64, C.c_uint64]
    getattr(lib, p + "chan").argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    lay = list(out)[:n]
    assert lay[5:] == HOST_LAYOUT_EXPECTED_TAIL, lay
    for i in range(5):
        HL[i] = lay[i]
    lib.bound = True


def build(s, case):
    ctl = s.alloc(0x38)
    s.words(ctl, case["ctl"])
    t = 0
    if case["time"] is not None:
        t = s.alloc(4)
        s.words(t, [case["time"]])
    anim = 0
    if case["anim"] is not None:
        anim = s.alloc(0x30)
        s.words(anim, case["anim"])
    hdr = s.alloc(0x80)
    s.words(hdr + 0x30, [case["flags"]])
    mesh = s.alloc(0x40)
    s.ptr(mesh + s.O["header"], hdr)
    kinds = case["kinds"]
    tcs = s.alloc(0x38 * len(kinds) + 8)
    for i, k in enumerate(kinds):
        s.words(tcs + 0x38 * i, [0, k] + [0] * 12)
    sa = s.alloc(0x100)
    s.ptr(sa + s.O["heads"], mesh)
    s.u16(sa + s.O["count_n"], len(kinds))
    s.ptr(sa + s.O["tcs"], tcs)
    s.ptr(sa + s.O["count"], case["count0"])
    return dict(ctl=ctl, t=t, anim=anim, hdr=hdr, sa=sa)


def execute(s, case, w):
    op = case["op"]
    if op == "get":
        s.call(FN_GET, [w["ctl"], w["t"]])
    elif op == "restore":
        s.call(FN_RESTORE, [w["ctl"], w["anim"]])
    else:
        s.call(FN_CHAN, [w["sa"]])


def observe(s, case, w):
    return {
        "ctl": s.rd_words(w["ctl"], 14),
        "time": s.rd_words(w["t"], 1) if w["t"] else None,
        "anim": s.rd_words(w["anim"], 12) if w["anim"] else None,
        "flags": s.rd_words(w["hdr"] + 0x30, 1),
        "count": s.rd_words(w["sa"] + s.O["count"], 1),
    }


def run(s, case):
    try:
        w = build(s, case)
        execute(s, case, w)
        return observe(s, case, w)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000]
WORDS = [0, 1, 2, MASK, 0x80000000, 0x7FFFFFFF, 0x1000000, 0x1000001, 30, 1000, 0xFFFFFF]


def rw(r):
    k = r.random()
    if k < 0.3:
        return r.choice(WORDS)
    if k < 0.7:
        return r.randrange(0, 5000)
    return r.randrange(0, 1 << 32)


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(0.1, 8))
    return fb(r.uniform(-100, 100))


def base(r, op):
    ctl = [rw(r) for _ in range(14)]
    ctl[3] = rf(r)
    if r.random() < 0.5:
        ctl[5] = (ctl[4] + r.randrange(0, 5000)) & MASK
    return {
        "op": op, "ctl": ctl,
        "time": None if r.random() < 0.3 else rw(r),
        "anim": None if r.random() < 0.3 else [rw(r) for _ in range(12)],
        "flags": r.choice([0, MASK, 0x80000, rw(r)]),
        "kinds": [r.choice([0, 1, 0xb, 0xc, 0xd, 0xffffffff, rw(r)]) for _ in range(r.randrange(0, 7))],
        "count0": r.choice([0, 5, MASK, rw(r)]),
    }


def make(op, seed):
    def edges():
        r = random.Random(seed)
        for t in (None, 0, 5):
            for a in (None, [0, 10, 40] + [0] * 9, [0, 40, 10] + [0] * 9):
                for kinds in ([], [0xc], [0xb], [0xc, 3], [0xc, 0xd, 0xffffffff]):
                    for flags in (0, 0x80000, MASK):
                        c = base(r, op)
                        c["time"], c["anim"], c["kinds"], c["flags"] = t, a, kinds, flags
                        yield c
        for rate in (0, 0x80000000, 0x3F800000):
            for diff in (0, 1, MASK, 0x1000001):
                c = base(r, op)
                c["ctl"][3] = rate
                c["ctl"][4], c["ctl"][5] = 100, (100 + diff) & MASK
                yield c

    def rnd(r):
        return base(r, op)
    return edges, rnd


TESTS = []
for _name, _op, _seed in (("GetAnimTimeStats", "get", 11), ("RestoreAnimTimeStats", "restore", 12), ("GetNumActiveChannels", "chan", 13)):
    _e, _r = make(_op, _seed)
    TESTS.append(FnTest(_name, ppc, host, edge=_e, rnd=_r))
