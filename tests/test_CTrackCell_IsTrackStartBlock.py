"""Unit v5268, four CTrackCell methods, against libtpw_unit_CTrackCell_IsTrackStartBlock.dylib (tests/unit_CTrackCell_IsTrackStartBlock.cpp), main app
("SimTheme Park"), src/game/park/TrackCell.cpp:
  CTrackCell::IsTrackStartBlock 0x100a36cc, IsBridge 0x100a34c4, GetSegmentNumber 0x100a399c, ValidTypeChange 0x100a149c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. CCellBase::GetMapCellEquiv and CTrackCell::IsFlag are
fakes on both sides that record their arguments; IsFlag answers from the case's queue.

The world of a case: one cell (type, flags, parent id, segment number), gGameWorld pointing at a block whose track cell table (+0x1102d8, 0x28
bytes each) holds a parent entry (type, segment number). What is compared: the answer, the event log, the cell and the parent entry as words.

No fused operations. Not tested: a parent id beyond the small table (the block is kept small), and a table address that wraps to NULL.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CTrackCell_IsTrackStartBlock"
MASK = 0xFFFFFFFF

FN_START, FN_BRIDGE, FN_SEG, FN_VALID = 0x100a36cc, 0x100a34c4, 0x100a399c, 0x100a149c
SIZES = {FN_START: 156, FN_BRIDGE: 132, FN_SEG: 104, FN_VALID: 184}
OWN = set(SIZES)

CALLEE_MAPEQUIV = 0x100f49a0
CALLEE_ISFLAG = 0x100a2918

ADDR = dict(gGameWorld=0x1037ebfc)
HOST_LAYOUT = [8, 0xe, 0x10, 0x24, 0x28]

TABLE = 0x1102d8
NCELLS = 70
WORLD_SIZE = TABLE + 0x28 * NCELLS


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_mapequiv(e):
    e.cl.rec("GetMapCellEquiv", (e.cl.lab(e.r(3)),))
    e.set_r(3, 0)


def stub_isflag(e):
    L = e.cl
    L.rec("IsFlag", (L.lab(e.r(3)), e.r(4) & 0xFFFF))
    e.set_r(3, L.ret("IsFlag") & MASK)


STUBS.internal(CALLEE_MAPEQUIV, stub_mapequiv)
STUBS.internal(CALLEE_ISFLAG, stub_isflag)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.fmt[0] + self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def fields(self, a):
        """The fields as numbers (the two memories differ in byte order): every halfword of the 0x28 bytes, then the type word."""
        h = list(struct.unpack(self.fmt[0] + "20H", self.read(a, 0x28)))
        return h[:4] + h[6:] + [self.rd_word(a + 8)]

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = "I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, cell, mode):
        fn = {"start": FN_START, "bridge": FN_BRIDGE, "seg": FN_SEG, "valid": FN_VALID}[op]
        args = [cell] + ([mode & MASK] if op == "valid" else [])
        r3, _, _ = T.call_ppc(self.e, fn, args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_tsb_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_tsb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, cell, mode):
        L = self.lib
        if op == "start":
            return L.tpw_tsb_start(cell) & MASK
        if op == "bridge":
            return L.tpw_tsb_bridge(cell) & MASK
        if op == "seg":
            return L.tpw_tsb_seg(cell) & MASK
        return L.tpw_tsb_valid(cell, s32(mode)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_tsb_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    for n in ("start", "bridge", "seg"):
        getattr(lib, p + n).restype, getattr(lib, p + n).argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_tsb_valid.restype, lib.tpw_tsb_valid.argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def parent_addr(world, pid):
    return world + TABLE + ((pid - 1) * 0x28)


def build(s, case):
    world = s.alloc(WORLD_SIZE)
    cell = s.alloc(0x28)
    s.ptr(s.g("gGameWorld"), world)
    s.words(cell + 8, [case["type"]])
    s.half(cell + 0xE, case["flags"])
    s.half(cell + 0x10, case["pid"])
    s.half(cell + 0x24, case["seg"])
    p = parent_addr(world, case["pid"])
    s.words(p + 8, [case["ptype"]])
    s.half(p + 0x24, case["pseg"])
    return world, cell, p


def run(s, case):
    try:
        world, cell, p = build(s, case)
        ans = s.call(case["op"], cell, case["mode"])
        return {"answer": ans, "log": s.log(), "cell": s.fields(cell), "parent": s.fields(p),
                "world": s.rd_ptr(s.g("gGameWorld")) == world}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


TYPES = [0, 7, 0xC, 0xD, 0x11, 0x12, 0x19, 1, 2, 3, 0x40, 0x7FFFFFFF, 0xFFFFFFFF]
PIDS = [0, 1, 2, 5, 33, 64]
FLAGS = [0, 1, 2, 4, 8, 0x10, 0x40, 0x80, 0xF, 0xFFF0, 0xFFFF, 0x8000]


def base(r, op):
    return {"op": op, "queues": {"IsFlag": [r.choice([0, 1])]},
            "type": r.choice(TYPES + [r.getrandbits(32)]), "flags": r.choice(FLAGS + [r.getrandbits(16)]),
            "pid": r.choice(PIDS + [r.randrange(0, 65)]), "seg": r.choice([0, 1, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16)]),
            "ptype": r.choice(TYPES + [r.getrandbits(32)]), "pseg": r.choice([0, 1, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16)]),
            "mode": r.choice(TYPES)}


def edges(op, seed):
    def gen():
        r = random.Random(seed)
        for t in TYPES:
            for pt in (0, 0xD, 0x12, 0xC, 5):
                for pid in (0, 1, 7):
                    c = base(r, op)
                    c["type"], c["ptype"], c["pid"] = t, pt, pid
                    c["mode"] = t if r.random() < 0.5 else r.choice(TYPES)
                    c["queues"] = {"IsFlag": [r.choice([0, 1])]}
                    yield c
        for f in FLAGS:
            for q in (0, 1):
                for m in (0, 3, 0xC):
                    c = base(r, op)
                    c["flags"], c["mode"], c["type"] = f, m, r.choice([0, 3, 0xC])
                    c["queues"] = {"IsFlag": [q]}
                    yield c
    return gen


def rnd(op):
    return lambda r: base(r, op)


TESTS = [
    FnTest("CTrackCell::IsTrackStartBlock", ppc, host, edge=edges("start", 11), rnd=rnd("start")),
    FnTest("CTrackCell::IsBridge", ppc, host, edge=edges("bridge", 12), rnd=rnd("bridge")),
    FnTest("CTrackCell::GetSegmentNumber", ppc, host, edge=edges("seg", 13), rnd=rnd("seg")),
    FnTest("CTrackCell::ValidTypeChange", ppc, host, edge=edges("valid", 14), rnd=rnd("valid")),
]
