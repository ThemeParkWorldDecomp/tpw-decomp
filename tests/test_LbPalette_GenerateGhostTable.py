"""Unit v5650, LbPalette_GenerateGhostTable 0x100474cc (bullfrog), against libtpw_unit_LbPalette_GenerateGhostTable.dylib.

The five callees (LbMem_Alloc, LbFile_Load, LbFile_Save, LbMem_Free, LbPalette_FindColour) are fakes on both sides. Load and Save
write their TbError through the hidden return pointer and answer from the case's queues; FindColour is a pure function of its
bytes whose calls are counted and folded into a checksum (65536 calls are too many for the event log). A case gives a table slot
(empty or holding a buffer), a random palette, a percentage and a name. Compared: the returned error, the slot (as a label),
both 64 KB buffers, the call count and checksum, and the event log. No fused operations. Not tested: a NULL slot or palette.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbPalette_GenerateGhostTable"
MASK = 0xFFFFFFFF

FN = 0x100474CC
SIZES = {FN: 556}
OWN = set(SIZES)
ALLOC, LOAD, SAVE, FREE, FIND = 0x10013EA4, 0x1000180C, 0x10001B78, 0x10013EFC, 0x1001A5C8


def s32(v):
    return T.s32(v)


def lab(L, p):
    if p == 0:
        return 0
    if p == L.abuf:
        return 1
    if p == L.pre:
        return 2
    if p == L.pal:
        return 3
    return 4


def namehash(e, a):
    h = 7
    for ch in e.cstr(a) if isinstance(e.cstr(a), (bytes, bytearray)) else e.cstr(a).encode():
        h = (h * 31 + ch) & MASK
    return h


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def put_err(e, v):
    e.write(e.r(3), struct.pack(">i", s32(v)))


def st_alloc(e):
    L = e.cl
    L.rec("Alloc", (e.r(3),))
    e.set_r(3, L.abuf)


def st_load(e):
    L = e.cl
    L.rec("Load", (namehash(e, e.r(4)), lab(L, e.r(5)), e.r(6), 1 if e.r(7) else 0))
    put_err(e, L.ret("load"))


def st_save(e):
    L = e.cl
    L.rec("Save", (namehash(e, e.r(4)), lab(L, e.r(5)), e.r(6)))
    put_err(e, L.ret("save"))


def st_free(e):
    L = e.cl
    L.rec("Free", (lab(L, e.r(3)),))


def st_find(e):
    L = e.cl
    r, g, b = e.r(4) & 255, e.r(5) & 255, e.r(6) & 255
    L.fc += 1
    L.fa = (L.fa * 31 + ((r << 16) | (g << 8) | b) + 1 + lab(L, e.r(3))) & MASK
    e.set_r(3, (r * 131 + g * 17 + b * 53 + (r ^ g)) & 255)


STUBS.internal(ALLOC, st_alloc)
STUBS.internal(LOAD, st_load)
STUBS.internal(SAVE, st_save)
STUBS.internal(FREE, st_free)
STUBS.internal(FIND, st_find)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def put_ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def get_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]


class Ppc(Side):
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.fc = 0
        self.L.fa = 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, d):
        self.e.write(a, d)

    def log(self):
        return self.L.log

    def setp(self, abuf, pre, pal):
        self.L.abuf, self.L.pre, self.L.pal = abuf, pre, pal

    def state(self):
        return [self.L.fc, self.L.fa]

    def lab(self, p):
        return lab(self.L, p)

    def call(self, slot, pal, pct, name):
        ret = self.e.alloc(4)
        T.call_ppc(self.e, FN, [ret, slot, pal, pct & MASK, name])
        return struct.unpack(">i", self.e.read(ret, 4))[0]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gg_alloc.restype, lib.tpw_gg_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gg_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_gg_set.argtypes = [C.c_uint64] * 3
    lib.tpw_gg_state.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_gg_run.restype, lib.tpw_gg_run.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint64]
    out = (C.c_int64 * 32)()
    assert lib.tpw_gg_layout(out) == 0
    lib.bound = True


class Host(Side):
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.abuf = self.pre = self.pal = 0
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_gg_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def log(self):
        return CL.host_end(self.lib)

    def setp(self, abuf, pre, pal):
        self.abuf, self.pre, self.pal = abuf, pre, pal
        self.lib.tpw_gg_set(abuf, pre, pal)

    def state(self):
        out = (C.c_int64 * 2)()
        self.lib.tpw_gg_state(out)
        return list(out)

    def lab(self, p):
        if p == 0:
            return 0
        return 1 if p == self.abuf else 2 if p == self.pre else 3 if p == self.pal else 4

    def call(self, slot, pal, pct, name):
        return s32(self.lib.tpw_gg_run(slot, pal, pct & MASK, name))


def run(s, case):
    try:
        pal = s.alloc(1024)
        s.write(pal, case["pal"])
        name = s.alloc(32)
        s.write(name, case["name"] + b"\0")
        pre = s.alloc(0x10000)
        s.write(pre, case["fill"])
        abuf = s.alloc(0x10000) if case["abuf"] else 0
        if abuf:
            s.write(abuf, case["fill"][::-1])
        slot = s.alloc(8)
        s.put_ptr(slot, pre if case["hasslot"] else 0)
        s.setp(abuf, pre, pal)
        err = s.call(slot, pal, case["pct"], name)
        return {
            "err": err,
            "slot": s.lab(s.get_ptr(slot)),
            "pre": s.read(pre, 0x10000),
            "abuf": s.read(abuf, 0x10000) if abuf else b"",
            "state": s.state(),
            "log": s.log(),
        }
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, hasslot, abuf, load, save, pct):
    return {
        "queues": {"load": [load], "save": [save]},
        "pal": r.getrandbits(8 * 1024).to_bytes(1024, "big"),
        "fill": r.getrandbits(8 * 0x10000).to_bytes(0x10000, "big"),
        "name": bytes(r.choice(b"abcxyz/:._") for _ in range(r.randrange(0, 12))),
        "hasslot": hasslot,
        "abuf": abuf,
        "pct": pct,
    }


def edges():
    r = random.Random(4711)
    for hasslot in (0, 1):
        for abuf in (0, 1):
            for load in (0, -1, 5):
                for save in (0, -1):
                    yield mk(r, hasslot, abuf, load, save, r.choice([0, 100, 50, -50, 255, 1, 99]))
    for pct in (0, 100, 1, -1, 37, 0x7FFFFFFF, 0x80000000, 255, 1000):
        yield mk(r, 0, 1, 0, 0, pct)


def rnd(r):
    return mk(r, r.choice([0, 1]), r.choice([0, 1, 1]), r.choice([0, 0, -1, 3]), r.choice([0, 0, -1]),
              r.choice([r.randrange(-300, 400), r.randrange(0, 101), r.getrandbits(32)]))


TESTS = [FnTest("LbPalette_GenerateGhostTable", ppc, host, edge=edges, rnd=rnd)]
