"""Unit v5651, STATIC_WhiteOut, against libtpw_unit_STATIC_WhiteOut.dylib (tests/unit_STATIC_WhiteOut.cpp), main app
("SimTheme Park"), src/game/park/DynamicCell.cpp:
  STATIC_WhiteOut 0x100e28b4

A case is a colour table (16 doubles at 0x102a03a4) and a rand seed. The GL calls and rand are fakes on both sides. The state calls
(glPushAttrib, glDisable, glBegin, glEnd, glPopAttrib) are logged as events; the colours (the four doubles each glColor4dv points at, read
from memory) and the vertices (bit patterns of the doubles) and every rand answer are folded in call order into one hash and a count,
which are compared. rand is a fixed LCG on both sides seeded by the case.

No fused operations. Not tested: the real table contents (unknown), so random bit patterns are used.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_STATIC_WhiteOut"
M64 = (1 << 64) - 1
FN = 0x100E28B4
SIZES = {FN: 368}
OWN = set(SIZES)
COLORS = 0x102A03A4
GLUE = dict(push=0x1023B230, disable=0x1023B248, begin=0x1023B260, end=0x1023B2A8, pop=0x1023B2C0,
            color=0x1023B278, vertex=0x1023B290, rand=0x10239148)
HOST_LAYOUT = [128]

STUBS = CL.Stubs()
STUBS.real.update(OWN)
ST = {"h": 0, "n": 0, "seed": 0}


def fold(v):
    ST["h"] = ((ST["h"] * 1000003) ^ v) & M64
    ST["n"] += 1


def dbits(x):
    return struct.unpack(">Q", struct.pack(">d", x))[0]


def s_rec(name, spec):
    def f(e):
        e.cl.rec(name, tuple(e.r(3 + i) & 0xFFFFFFFF for i in range(spec)))
    return f


def s_color(e):
    for b in struct.unpack(">4Q", e.read(e.r(3), 32)):
        fold(b ^ 0x1111)


def s_vertex(e):
    fold(dbits(e.f(1)) ^ 0x2222)
    fold(dbits(e.f(2)) ^ 0x3333)


def s_rand(e):
    ST["seed"] = (ST["seed"] * 1103515245 + 12345) & 0x7FFFFFFF
    fold(ST["seed"])
    e.set_r(3, ST["seed"])


STUBS.imp("glPushAttrib", s_rec("glPushAttrib", 1))
STUBS.imp("glDisable", s_rec("glDisable", 1))
STUBS.imp("glBegin", s_rec("glBegin", 1))
STUBS.imp("glEnd", s_rec("glEnd", 0))
STUBS.imp("glPopAttrib", s_rec("glPopAttrib", 0))
STUBS.imp("glColor4dv", s_color)
STUBS.imp("glVertex2d", s_vertex)
STUBS.imp("rand", s_rand)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ww_addr.restype, lib.tpw_ww_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ww_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ww_run.argtypes = [C.c_uint32]
    lib.tpw_ww_hash.restype = C.c_uint64
    lib.tpw_ww_count.restype = C.c_uint64
    out = (C.c_int64 * 32)()
    n = lib.tpw_ww_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def ppc(e, case):
    e.cl = CL.PpcLog({}, {})
    ST.update(h=0, n=0, seed=case["seed"])
    e.write(COLORS, b"".join(struct.pack(">Q", w) for w in case["colors"]))
    saved = STUBS.activate(e)
    try:
        T.call_ppc(e, FN, [])
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"log": e.cl.log, "hash": ST["h"], "count": ST["n"]}


def host(lib, case):
    bind(lib)
    CL.host_begin(lib, {}, {})
    C.memmove(lib.tpw_ww_addr(b"colors"), b"".join(struct.pack("<Q", w) for w in case["colors"]), 128)
    lib.tpw_ww_run(case["seed"])
    return {"log": CL.host_end(lib), "hash": lib.tpw_ww_hash(), "count": lib.tpw_ww_count()}


def mk(r):
    cols = []
    for _ in range(16):
        cols.append(dbits(r.choice([0.0, 1.0, 0.5, r.uniform(-2, 2)])))
    return {"colors": cols, "seed": r.randrange(1 << 31)}


def edges():
    r = random.Random(77)
    yield {"colors": [0] * 16, "seed": 0}
    yield mk(r)


TESTS = [
    FnTest("STATIC_WhiteOut", ppc, host, edge=edges, rnd=mk),
]
