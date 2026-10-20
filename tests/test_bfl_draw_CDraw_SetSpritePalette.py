"""Unit v5336, bfl::draw::CDraw::SetSpritePalette 0x10065a7c, against libtpw_unit_bfl_draw_CDraw_SetSpritePalette.dylib
(tests/unit_bfl_draw_CDraw_SetSpritePalette.cpp), src/Draw_state_bfl_draw_CDraw_SetSpritePalette.cpp.

A case is a CDraw block (0x870 bytes, random fill, current pixel format at +8), a 256-word palette and an optional pixel
format. TbMaskRotates::Init (0x10065e7c) is a fake on both sides that logs both formats and writes the rotates from the
case's queue. Compared: the whole object after the call and the log. No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_bfl_draw_CDraw_SetSpritePalette"
MASK = 0xFFFFFFFF

FN = 0x10065A7C
SIZES = {FN: 940}
OWN = set(SIZES)
CALLEE_INIT = 0x10065E7C
OBJ = 0x870

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_init(e):
    L = e.cl
    src = [e.u32(e.r(4) + 4 * i) for i in range(6)]
    dst = [e.u32(e.r(5) + 4 * i) for i in range(6)]
    L.rec("TbMaskRotates::Init", tuple(src + dst))
    out = e.r(3)
    for i in range(4):
        e.write(out + 4 * i, struct.pack(">I", L.ret("rot") & MASK))


STUBS.internal(CALLEE_INIT, stub_init)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    cur_off, tab_off, size = 8, 0x68, OBJ

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, obj, pal, fm):
        T.call_ppc(self.e, FN, [obj, pal, fm])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.cur_off, self.tab_off, self.size = lib.layout
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_sprpal_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, obj, pal, fm):
        self.lib.tpw_sprpal_set(obj, pal, fm)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sprpal_alloc.restype, lib.tpw_sprpal_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sprpal_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sprpal_set.restype, lib.tpw_sprpal_set.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sprpal_layout(out)
    assert n == 3, n
    lib.layout = (out[0], out[2], out[1])       # current format, table, object size
    lib.bound = True


def run(s, case):
    try:
        obj = s.alloc(s.size)
        s.words(obj + s.cur_off, case["cur"])
        s.words(obj + s.tab_off, case["tab"])
        pal = s.alloc(0x400)
        s.words(pal, case["pal"])
        fm = 0
        if case["fmt"] is not None:
            fm = s.alloc(24)
            s.words(fm, case["fmt"])
        s.call(obj, pal, fm)
        return {"cur": s.rd_words(obj + s.cur_off, 6), "tab": s.rd_words(obj + s.tab_off, 512), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rmask(r):
    return r.choice([0, MASK, 0xFF, 0xFF00, 0xFF0000, 0xF800, 0x7E0, 0x1F, 0xFF000000, r.getrandbits(32)])


def rfmt(r):
    return [rmask(r), rmask(r), rmask(r), rmask(r), r.choice([0, 8, 16, 24, 32]), r.choice([0, 1])]


def rrot(r):
    return r.choice([0, 1, 8, 16, 24, 31, 32, 33, 40, r.randrange(0, 64), (-r.randrange(1, 40)) & MASK])


def mk(r, objfmt=None, fmt="rand", pal=None):
    o = [r.getrandbits(32) for _ in range(512)]
    cur = rfmt(r) if objfmt is None else objfmt
    if fmt == "rand":
        fmt = rfmt(r) if r.random() < 0.6 else None
    p = [r.getrandbits(32) for _ in range(256)] if pal is None else pal
    return {"cur": cur, "tab": o, "pal": p, "fmt": fmt, "queues": {"rot": [rrot(r) for _ in range(4)]}}


def edges():
    r = random.Random(77)
    Z = [0] * 6
    yield mk(r, objfmt=Z, fmt=None)
    yield mk(r, objfmt=Z, fmt=Z)
    yield mk(r, objfmt=[1, 0, 0, 0, 0, 0], fmt=None)
    yield mk(r, objfmt=Z, fmt=[0, 1, 0, 0, 0, 0])
    for k in range(6):
        f = [0] * 6
        f[k] = 1
        yield mk(r, objfmt=f, fmt=None)
        yield mk(r, objfmt=Z, fmt=f)
    yield mk(r, objfmt=[0xF800, 0x7E0, 0x1F, 0, 16, 0], fmt=None, pal=[0xFFFFFFFF] * 256)
    yield mk(r, objfmt=[0xF800, 0x7E0, 0x1F, 0, 16, 0], fmt=None, pal=[0] * 256)
    for rots in ([0, 0, 0, 0], [32, 32, 32, 0], [31, 33, 40, 5], [8, 16, 24, 0]):
        c = mk(r)
        c["queues"] = {"rot": rots}
        yield c


def rnd(r):
    return mk(r)


TESTS = [
    FnTest("bfl::draw::CDraw::SetSpritePalette", ppc, host, edge=edges, rnd=rnd),
]
