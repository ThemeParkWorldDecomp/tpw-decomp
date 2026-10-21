"""Unit v5114: ResetHLSprites and DrawHLSprites (the script sprites), against libtpw_unit_DrawHLSprites.dylib (tests/unit_DrawHLSprites.cpp), main
app ("SimTheme Park"), src/game/HLSprite.cpp:
  ResetHLSprites 0x1007af24, DrawHLSprites 0x1007b02c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. The callees outside the unit are fakes on both sides:
LLCC_SystemGetPointedPeepThing (answers a word from the queue), CIntrface::SetCurrentThing (records the thing), GetLMatrixRotationAroundYAxis
(records whether the matrix is NULL, answers a float from the queue); fmod is a full C fmod on the PowerPC side and the real one on the host.

The world of a case: the first person flags, the camera matrix pointer, the yaw, the cell counts, the global count, the enabled word, the
sixteen lists' counts, the global list's count, gESPF20 and gESPF88. What is compared: all of those after the call and the event log.

No fused operations. Not tested: nothing is left out (a NULL matrix is only recorded by the fake, never read).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DrawHLSprites"
MASK = 0xFFFFFFFF

FN_RESET, FN_DRAW = 0x1007af24, 0x1007b02c
SIZES = {FN_RESET: 224, FN_DRAW: 196}
OWN = set(SIZES)

CALLEE_PEEP = 0x10096bc8
CALLEE_SETCUR = 0x1007dde8
CALLEE_MATRIX = 0x100c0f24

ADDR = dict(flags=0x102b5a40, cammat=0x102b5af0, yaw=0x102b5bf4, f20=0x102dbd20, f88=0x102dc088, globalcount=0x102dbd40,
            cellcounts=0x102dbd7c, enabled=0x102dbd38, globallist=0x102dbd4c)
HOST_LAYOUT = [64]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_peep(e):
    L = e.cl
    L.rec("GetPointed", (1,))
    e.set_r(3, L.ret("GetPointed") & MASK)


def stub_setcur(e):
    e.cl.rec("SetCurrentThing", (e.r(4) & MASK,))


def stub_matrix(e):
    L = e.cl
    L.rec("GetLMatrix", (1 if e.r(3) else 0,))
    e.set_f1(bitsf(L.ret("GetLMatrix")))


def fmod(e):
    x, y = e.f(1), e.f(2)
    try:
        r = math.fmod(x, y)
    except ValueError:
        r = float("nan")
    e.set_f1(r)


STUBS.internal(CALLEE_PEEP, stub_peep)
STUBS.internal(CALLEE_SETCUR, stub_setcur)
STUBS.internal(CALLEE_MATRIX, stub_matrix)
STUBS.imp("fmod", fmod)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name.startswith("hl"):
            return 0x102dbdfc + int(name[2:]) * 0x28 + 4
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a, v):
        self.words(a, [v])

    def log(self):
        return self.L.log

    def call(self, op):
        with_args = FN_RESET if op == "reset" else FN_DRAW
        T.call_ppc(self.e, with_args, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_hl_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_hl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v & MASK))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        (self.lib.tpw_hl_reset if op == "reset" else self.lib.tpw_hl_draw)()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_hl_"
    lib.tpw_hl_addr.restype, lib.tpw_hl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_hl_alloc.restype, lib.tpw_hl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_hl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_hl_reset.restype, lib.tpw_hl_reset.argtypes = None, []
    lib.tpw_hl_draw.restype, lib.tpw_hl_draw.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_hl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("flags"), [case["flags"]])
    s.ptr(s.g("cammat"), s.alloc(64) if case["mat"] else 0)
    s.words(s.g("yaw"), [case["yaw"]])
    s.ptr(s.g("f20"), case["f20"])     # a word on the Mac, a pointer on the host
    s.words(s.g("f88"), [case["f88"]])
    s.words(s.g("globalcount"), [case["gcount"]])
    s.words(s.g("cellcounts"), case["cells"])
    s.words(s.g("enabled"), [case["enabled"]])
    s.words(s.g("globallist"), [case["glist"]])
    for k in range(16):
        s.words(s.g("hl%d" % k), [case["hl"][k]])


def observe(s, case):
    return {
        "log": s.log(),
        "flags": s.rd_word(s.g("flags")),
        "yaw": canon_f(s.rd_word(s.g("yaw"))),
        "f20": s.rd_word(s.g("f20")),
        "f88": canon_f(s.rd_word(s.g("f88"))),
        "gcount": s.rd_word(s.g("globalcount")),
        "cells": s.rd_words(s.g("cellcounts"), 16),
        "enabled": s.rd_word(s.g("enabled")),
        "glist": s.rd_word(s.g("globallist")),
        "hl": [s.rd_word(s.g("hl%d" % k)) for k in range(16)],
    }


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x80000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000,
           0x40C90FDB, 0xC0C90FDB, 0x40C90FDA, 0x40C90FDC, 0x3FC90FDB, 0xBFC90FDB]


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def base(r, op):
    return {"op": op,
            "queues": {"GetPointed": [r.choice([0, 0x1000, 0x80000000, 0xFFFFFFFF, r.getrandbits(32)])], "GetLMatrix": [rf(r)]},
            "flags": r.choice([0, 1, 2, 4, 8, 0x10, 0x20, 0x16, 0x14, 0x12, 0x06, 0xFFFFFFFF, r.getrandbits(32)]),
            "mat": r.choice([0, 1]),
            "yaw": rf(r),
            "f20": r.choice([0, 0x2000, 0x80000000, 0xFFFFFFFF, r.getrandbits(32)]),
            "f88": rf(r),
            "gcount": r.choice([0, 1, 7, 0x800, 0xFFFFFFFF, r.getrandbits(32)]),
            "cells": [r.choice([0, 1, 0x800, 0xFFFFFFFF, r.getrandbits(32)]) for _ in range(16)],
            "enabled": r.choice([0, 1, 5, 0xFFFFFFFF, 0x80000000]),
            "glist": r.getrandbits(32),
            "hl": [r.getrandbits(32) for _ in range(16)]}


def make(op, seed):
    def edges():
        r = random.Random(seed)
        if op == "reset":
            for flags in (0, 0x16, 4, 2, 0x10, 1, 8, 0x20, 0xFFFFFFE9):
                for yaw in SPECIAL:
                    for mat in (0, 1):
                        c = base(r, op)
                        c["flags"], c["yaw"], c["mat"] = flags, yaw, mat
                        yield c
        else:
            for enabled in (0, 1, 0xFFFFFFFF, 0x80000000):
                for f20 in (0, 0x1234, 0xFFFFFFFF):
                    c = base(r, op)
                    c["enabled"], c["f20"] = enabled, f20
                    yield c

    def rnd(r):
        return base(r, op)
    return edges, rnd


_re, _rr = make("reset", 5114)
_de, _dr = make("draw", 5115)

TESTS = [
    FnTest("ResetHLSprites", ppc, host, edge=_re, rnd=_rr),
    FnTest("DrawHLSprites", ppc, host, edge=_de, rnd=_dr),
]
