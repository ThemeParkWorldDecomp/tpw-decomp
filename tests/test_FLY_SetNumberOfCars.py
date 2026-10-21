"""Unit v5277, the FLY base accessors, against libtpw_unit_FLY_SetNumberOfCars.dylib (tests/unit_FLY_SetNumberOfCars.cpp), main app,
src/game/rides/FlyingRide.cpp:
  FLY_SetRunTime 0x10078958, FLY_SetRunSpeed 0x100789ec, FLY_SetOpenClosed 0x10078a58, FLY_SetNumberOfCars 0x10078ee4,
  FLY_GetBrokenStat 0x10078fb0, FLY_LaunchCar 0x10079024, FLY_RideRunning 0x100791c0

No callees. The world of a case: gFlyBases (0x102da964) with one slot (the called index, or a neighbour) pointing at a block of 0x1200
random words (small values, around the run-time constants), all other slots NULL. Compared: the answer (for the functions that return
one; SetNumberOfCars, SetOpenClosed, SetRunSpeed and SetRunTime are void in the original), the whole block read back, and the log.
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FLY_SetNumberOfCars"
MASK = 0xFFFFFFFF

FUNCS = {
    "SetRunTime": 0x10078958, "SetRunSpeed": 0x100789ec, "SetOpenClosed": 0x10078a58, "SetNumberOfCars": 0x10078ee4,
    "GetBrokenStat": 0x10078fb0, "LaunchCar": 0x10079024, "RideRunning": 0x100791c0,
}
SIZES = {0x10078958: 108, 0x100789ec: 68, 0x10078a58: 188, 0x10078ee4: 68, 0x10078fb0: 76, 0x10079024: 120, 0x100791c0: 96}
NARGS = {"SetRunTime": 2, "SetRunSpeed": 2, "SetOpenClosed": 2, "SetNumberOfCars": 2, "GetBrokenStat": 1, "LaunchCar": 1, "RideRunning": 1}
VOID = {"SetRunTime", "SetRunSpeed", "SetOpenClosed", "SetNumberOfCars"}
BASES = 0x102da964
HOST_LAYOUT = [8]
BLOCK = 0x1200

STUBS = CL.Stubs()
STUBS.real.update(SIZES)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

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

    def clear_bases(self):
        self.write(BASES, bytes(400))

    def set_base(self, i, addr):
        self.words(BASES + 4 * i, [addr])

    def call(self, name, args):
        r3, _, _ = T.call_ppc(self.e, FUNCS[name], [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_fly_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def clear_bases(self):
        for i in range(100):
            self.lib.tpw_fly_setbase(i, 0)

    def set_base(self, i, addr):
        self.lib.tpw_fly_setbase(i, addr)

    def call(self, name, args):
        fn = getattr(self.lib, "tpw_fly_" + name)
        r = fn(*[T.s32(a) for a in args])
        return 0 if r is None else r & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_fly_addr.restype, lib.tpw_fly_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fly_alloc.restype, lib.tpw_fly_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fly_setbase.restype, lib.tpw_fly_setbase.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_fly_layout.argtypes = [C.POINTER(C.c_int64)]
    for n, k in NARGS.items():
        f = getattr(lib, "tpw_fly_" + n)
        f.restype = None if n in VOID else C.c_uint32
        f.argtypes = [C.c_int32] * k
    out = (C.c_int64 * 32)()
    n = lib.tpw_fly_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.clear_bases()
        blk = s.alloc(BLOCK)
        s.words(blk, case["block"])
        slot = case["slot"]
        s.set_base(slot, blk if case["present"] else 0)
        ans = s.call(case["op"], case["args"])
        if case["op"] in VOID:
            ans = 0
        return {"answer": ans, "log": s.log(), "block": s.rd_words(blk, BLOCK // 4)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


VALS = [0, 1, 2, 3, -1, 5, 63, 64, 65, -64, -65, 100, 127, 128, 0x7FFFFFFF, -0x80000000, 3000]
ARGS = [0, 1, -1, 63, 64, 65, -63, -64, -65, 9, 10, 11, -9, -10, -11, 100, 6400, -6400, 0x7FFFFFFF, -0x80000000, 3, 999]
IDX = [-1, 0, 1, 2, 50, 98, 99, 100, 101, 1000, -0x80000000]


def mkblock(r, small):
    w = [r.choice(VALS) if r.random() < 0.5 else r.choice([0, 1, 2, 3]) for _ in range(BLOCK // 4)]
    if small:
        for k in (0xc, 0x34, 0x38, 0x3c, 0x48, 0x114, 0xdc):
            w[k // 4] = r.choice([0, 1, 2, 3])
    return [x & MASK for x in w]


def mk(r, op, idx=None, arg=None, present=None, vals=None):
    idx = r.choice(IDX + [r.randrange(1, 100)]) if idx is None else idx
    slot = idx if 0 <= idx < 100 else r.randrange(1, 100)
    c = {"op": op, "queues": {}, "block": mkblock(r, r.random() < 0.7), "slot": slot,
         "present": r.random() < 0.85 if present is None else present}
    a = [idx]
    if NARGS[op] == 2:
        a.append(r.choice(ARGS) if arg is None else arg)
    c["args"] = a
    return c


def edges(op):
    def gen():
        r = random.Random(sum(map(ord, op)))
        for idx in IDX:
            yield mk(r, op, idx=idx, present=True)
        yield mk(r, op, idx=5, present=False)
        for arg in (ARGS if NARGS[op] == 2 else [None] * 6):
            for _ in range(3):
                yield mk(r, op, idx=r.choice([1, 99, 7]), arg=arg, present=True)
    return gen


def rnd(op):
    return lambda r: mk(r, op)


TESTS = [FnTest("FLY_" + op, ppc, host, edge=edges(op), rnd=rnd(op)) for op in FUNCS]
