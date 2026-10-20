"""Unit v5678, the fly file writer, against libtpw_unit_FLY_Save.dylib (tests/unit_FLY_Save.cpp), main app
("SimTheme Park"), src/FLY_SetNumberOfCars_FLY_Save.cpp:
  FLY_Save 0x10079248

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit are fakes on both
sides that record their arguments: _USwapCopy32 (a little-endian copy of the words), _USwapDelete, LbFile_Write (the file, the buffer as a
label or, for a local buffer, a checksum of its bytes, the length, the count pointer), USwapInplace32<long>, the ThingId Byteswap and
USwapBlockStruct<FLYBOY> (the fakes of the original swap the bytes in place, so the bytes written are the little-endian ones on both sides).

The world of a case: the initialised flag, the four zone limits and the 100 fly base slots (each empty or a block of 0x122c random bytes).
What is compared: the answer and the event log (which holds the bytes written). No fused operations. Not tested: nothing excluded; the raw
table written by the second LbFile_Write is 400 bytes of the table in memory (Mac 32-bit pointers, host 64-bit): only its label and length
are compared.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FLY_Save"
MASK = 0xFFFFFFFF

FN_SAVE = 0x10079248
SIZES = {FN_SAVE: 1240}
OWN = set(SIZES)

COPY, WRITE, DELETE = 0x10239B68, 0x10238878, 0x10239B80
SWAP32, BYTESWAP16, SWAPBLOCK = 0x10079A64, 0x10079A0C, 0x10079740

ADDR = dict(gFlyInitialised=0x102DAAF8, gFlyBases=0x102DA964, gFlyZoneD=0x1029D2AC, gFlyZoneB=0x1029D2B0, gFlyZoneA=0x1029D2B4,
            gFlyZoneC=0x1029D2B8)
LABELS = {"gFlyInitialised": 1, "gFlyBases": 2, "gFlyZoneD": 3, "gFlyZoneB": 4, "gFlyZoneA": 5, "gFlyZoneC": 6}
HOST_LAYOUT = [8, 800]
FILE = 0x7001
BASE = 0x122C


def csum(data):
    c = 0
    for b in data:
        c = (c * 31 + b) & MASK
    return c


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_copy(e):
    L = e.cl
    src, n = e.r(3), e.r(4)
    v = e.u32(src)
    L.rec("USwapCopy32", (L.lab(src), n, v))
    buf = e.alloc(4 * n)
    e.write(buf, struct.pack("<I", v) + e.read(src + 4, 4 * n - 4))
    e.set_r(3, buf)


def stub_delete(e):
    L = e.cl
    L.rec("USwapDelete", (struct.unpack("<I", e.read(e.r(3), 4))[0],))


def stub_write(e):
    L = e.cl
    file, buf, length, tr = e.r(4), e.r(5), e.r(6), e.r(7)
    lb = L.lab(buf)
    s = csum(e.read(buf, length)) if lb == -1 else 0
    L.rec("LbFile_Write", (file, lb, length, L.lab(tr), s))


def stub_swap32(e):
    L = e.cl
    p = e.r(3)
    v = e.u32(p)
    L.rec("Swap32", (v,))
    e.write(p, struct.pack("<I", v))


def stub_byteswap16(e):
    L = e.cl
    p = e.r(3)
    v = struct.unpack(">H", e.read(p, 2))[0]
    L.rec("Byteswap16", (v,))
    e.write(p, struct.pack("<H", v))


def stub_swapblock(e):
    L = e.cl
    p, n = e.r(3), e.r(4)
    words = struct.unpack(">%dI" % (n * 0xE4 // 4), e.read(p, n * 0xE4))
    c = 0
    for w in words:
        c = (c * 31 + w) & MASK
    L.rec("USwapBlockStruct", (n, words[0], c))
    e.write(p, struct.pack("<%dI" % len(words), *words))


STUBS.imp("_USwapCopy32__13SamsUtilitiesFPUll", stub_copy)
STUBS.imp("LbFile_Write__FPvPCvUlPUl", stub_write)
STUBS.imp("_USwapDelete__13SamsUtilitiesFPv", stub_delete)
STUBS.internal(SWAP32, stub_swap32)
STUBS.internal(BYTESWAP16, stub_byteswap16)
STUBS.internal(SWAPBLOCK, stub_swapblock)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR[n]: l for n, l in LABELS.items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptrs(self, a, ps):
        self.words(a, ps)

    def log(self):
        return self.L.log

    def save(self):
        r3, _, _ = T.call_ppc(self.e, FN_SAVE, [FILE])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {l: self.g(n) for n, l in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_flysave_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_flysave_alloc(n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptrs(self, a, ps):
        self.write(a, b"".join(struct.pack("<Q", p) for p in ps))

    def log(self):
        return CL.host_end(self.lib)

    def save(self):
        return self.lib.tpw_flysave_save(FILE) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_flysave_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "save").restype, getattr(lib, p + "save").argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.words(s.g("gFlyInitialised"), [case["init"]])
    s.words(s.g("gFlyZoneD"), [case["zones"][0]])
    s.words(s.g("gFlyZoneB"), [case["zones"][1]])
    s.words(s.g("gFlyZoneA"), [case["zones"][2]])
    s.words(s.g("gFlyZoneC"), [case["zones"][3]])
    ps = []
    for seed in case["bases"]:
        if seed is None:
            ps.append(0)
            continue
        a = s.alloc(BASE)
        r = random.Random(seed)
        ws = [r.getrandbits(32) for _ in range(BASE // 4)]
        ws[0x58 // 4] = (ws[0x58 // 4] & 0xFF) * 0x01010101     # the halfword at +0x58 reads the same big- or little-endian
        s.words(a, ws)
        ps.append(a)
    s.ptrs(s.g("gFlyBases"), ps)


def observe(s, case, answer):
    return {"answer": answer, "log": s.log()}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.save())
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def mk(r, init, density, zones=None):
    bases = [r.randrange(1 << 30) if r.random() < density else None for _ in range(100)]
    return {"queues": {}, "init": init, "zones": [r.getrandbits(32) for _ in range(4)] if zones is None else zones, "bases": bases}


def edges():
    r = random.Random(678)
    yield mk(r, 0, 0.5)
    yield mk(r, 1, 0.0)
    yield mk(r, 1, 1.0)
    yield mk(r, 0x80000000, 0.05)
    yield mk(r, 0xFFFFFFFF, 0.3, [0, MASK, 0x80000000, 1])
    c = mk(r, 1, 0.0)
    c["bases"][0] = 123          # slot 0 is never written
    c["bases"][99] = 5
    c["bases"][1] = 7
    yield c


def rnd(r):
    return mk(r, r.choice([0, 1, 1, 2, r.getrandbits(32)]), r.choice([0.0, 0.02, 0.1, 0.5]))


TESTS = [
    FnTest("FLY_Save", ppc, host, edge=edges, rnd=rnd),
]
