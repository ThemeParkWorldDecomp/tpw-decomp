"""Unit v5759, GetPylonUnderCursor, against libtpw_unit_GetPylonUnderCursor.dylib (tests/unit_GetPylonUnderCursor.cpp), main app
("SimTheme Park"), src/LinkPreviewBrace_GetPylonUnderCursor.cpp:
  GetPylonUnderCursor 0x1003f848

A case is a world and a call. The world is built by ONE piece of Python code through small per-side primitives: in emulator memory
at the Mac's offsets (big-endian) and in the host library (native structs, built by tpw_gpc_* helpers). There are no callees.

The world of a case: the pointer's world x and z, the height field's row stride, a list of coaster instances each with a list of
pylons (flags, id). What is compared: the answer (as the index of the pylon found, -1 for NULL), the three out words, and which of the
three out pointers were NULL (an untouched word keeps a sentinel).

No fused operations; fctiwz only on values well below 2^31. Not tested: a NULL height field header (read unconditionally).
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_GetPylonUnderCursor"
MASK = 0xFFFFFFFF
FN = 0x1003F848
G_INSTANCES, G_HEADER, G_POSN = 0x102B68B4, 0x102C6C7C, 0x102C6CF8
SENT = 0xDEADBEEF


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e):
        self.e = e

    def w(self, a, ws):
        self.e.write(a, b"".join(struct.pack(">I", x & MASK) for x in ws))

    def inst(self):
        return self.e.alloc(0x100)

    def pylon(self, flags, pid):
        a = self.e.alloc(0x40)
        self.w(a + 4, [flags, pid])
        return a

    def pylon_next(self, p, n):
        self.w(p + 0x28, [n])

    def inst_set(self, i, nxt, pylons):
        self.w(i + 0x18, [nxt])
        self.w(i + 0x4C, [pylons])

    def globals(self, head, stride, xb, zb):
        hf = self.e.alloc(0x40)
        self.w(hf + 0x18, [stride])
        hdr = self.e.alloc(0x100)
        self.w(hdr + 0x6C, [hf])
        self.w(G_HEADER, [hdr])
        self.w(G_INSTANCES, [head])
        self.w(G_POSN, [xb, 0, zb])

    def call(self, mask):
        out = self.e.alloc(12)
        self.w(out, [SENT, SENT, SENT])
        args = [out if mask & 1 else 0, out + 4 if mask & 2 else 0, out + 8 if mask & 4 else 0]
        r3, _, _ = T.call_ppc(self.e, FN, args)
        return r3 & MASK, list(struct.unpack(">3I", self.e.read(out, 12)))


class Host:
    def __init__(self, lib):
        self.lib = lib
        if not getattr(lib, "bound", False):
            for n in ("inst", "pylon", "call"):
                getattr(lib, "tpw_gpc_" + n).restype = C.c_uint64
            lib.tpw_gpc_pylon.argtypes = [C.c_uint32, C.c_uint32]
            lib.tpw_gpc_pylon_next.argtypes = [C.c_uint64, C.c_uint64]
            lib.tpw_gpc_inst_set.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64]
            lib.tpw_gpc_globals.argtypes = [C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint32]
            lib.tpw_gpc_call.argtypes = [C.c_int32, C.POINTER(C.c_uint32)]
            lib.bound = True

    def inst(self):
        return self.lib.tpw_gpc_inst()

    def pylon(self, flags, pid):
        return self.lib.tpw_gpc_pylon(flags, pid)

    def pylon_next(self, p, n):
        self.lib.tpw_gpc_pylon_next(p, n)

    def inst_set(self, i, nxt, pylons):
        self.lib.tpw_gpc_inst_set(i, nxt, pylons)

    def globals(self, head, stride, xb, zb):
        self.lib.tpw_gpc_globals(head, stride, xb, zb)

    def call(self, mask):
        out = (C.c_uint32 * 3)(SENT, SENT, SENT)
        r = self.lib.tpw_gpc_call(mask, out)
        return r & 0xFFFFFFFFFFFFFFFF, list(out)


def run(s, case):
    pylons = []          # addresses in creation order
    insts = []
    for plist in case["insts"]:
        addrs = [s.pylon(f, i) for f, i in plist]
        pylons.extend(addrs)
        for a, b in zip(addrs, addrs[1:]):
            s.pylon_next(a, b)
        insts.append((s.inst(), addrs[0] if addrs else 0))
    for k, (ia, head) in enumerate(insts):
        s.inst_set(ia, insts[k + 1][0] if k + 1 < len(insts) else 0, head)
    s.globals(insts[0][0] if insts else 0, case["stride"], case["x"], case["z"])
    r, out = s.call(case["mask"])
    return {"answer": pylons.index(r) if r else -1, "out": out}


def ppc(e, case):
    return run(Ppc(e), case)


def host(lib, case):
    return run(Host(lib), case)


def make(r, x, z, stride, insts, mask):
    return {"x": fb(x), "z": fb(z), "stride": stride, "insts": insts, "mask": mask, "queues": {}}


def rand_case(r):
    stride = r.choice([0, 1, 7, 64, 256, r.randrange(1, 1000)])
    cx, cz = r.randrange(0, 60), r.randrange(0, 60)
    x = cx * 10 + r.uniform(0, 9.99)
    z = cz * 10 + r.uniform(0, 9.99)
    if r.random() < 0.15:
        x, z = r.uniform(-50, 50), r.uniform(-50, 50)
    cxi, czi = int(x / 10.0), int(z / 10.0)
    key = (cxi + czi * stride) & MASK
    insts = []
    for _ in range(r.choice([0, 1, 2, 3, 4])):
        pl = []
        for _ in range(r.choice([0, 1, 2, 3, 5])):
            pid = key if r.random() < 0.25 else r.choice([0, key + 1, r.randrange(0, 5000)]) & MASK
            pl.append((r.choice([0, 0, 1, 2, 4, 5, 8, 3]), pid))
        insts.append(pl)
    return make(r, x, z, stride, insts, r.randrange(8))


def edges():
    r = random.Random(759)
    # empty list, empty pylon lists, flag skips, match in second instance, the key read as the float conversions truncate
    yield make(r, 15.0, 25.0, 4, [], 7)
    yield make(r, 15.0, 25.0, 4, [[]], 0)
    yield make(r, 15.0, 25.0, 4, [[], []], 7)
    key = 1 + 2 * 4
    for fl in (0, 1, 2, 4, 5, 8):
        yield make(r, 15.0, 25.0, 4, [[(fl, key)]], 7)
    yield make(r, 15.0, 25.0, 4, [[(1, key), (4, key), (0, key + 1), (2, key)], [(0, key)]], 7)
    yield make(r, 15.0, 25.0, 4, [[(0, key + 1)], [(0, key + 2), (0, key)]], 1)
    yield make(r, 9.999, 10.0, 4, [[(0, 0), (0, 4), (0, 1)]], 6)
    yield make(r, -5.0, -15.0, 3, [[(0, (-1 + -1 * 3) & MASK), (0, 0)]], 7)
    yield make(r, 0.0, 0.0, 0, [[(0, 0)]], 4)


TESTS = [FnTest("GetPylonUnderCursor", ppc, host, edge=edges, rnd=rand_case)]
