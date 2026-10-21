"""Unit v5654, CResearchProject::Serialise 0x10123af0, against libtpw_unit_CResearchProject_Serialise.dylib
(tests/unit_CResearchProject_Serialise.cpp), main app, src/game/buildings/ResearchLab.cpp.

A case is a project object (20 bytes, random contents) and a call (save flag, version). The eight SAVEVAR / LOADVAR callees are fakes on both
sides that record the event name, the field offset in the object, a hash of the name string and the noLog flag; they answer from a queue
(0 or 1 per call), so a failure at every step is covered. Compared: the answer, the object's words and the event log.
No fused operations. Not tested: none.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CResearchProject_Serialise"
MASK = 0xFFFFFFFF

FN = 0x10123AF0
CALLEES = {
    0x100F1188: ("SAVEVARSTRUCT", 0), 0x10124680: ("SAVEVARCat", 4), 0x100F1A04: ("SAVEVARByte", 8),
    0x100F18D8: ("SAVEVARFloat", 0), 0x100F1300: ("LOADVARSTRUCT", 0), 0x10124554: ("LOADVARCat", 4),
    0x100F17C8: ("LOADVARByte", 8), 0x100F16B4: ("LOADVARFloat", 0),
}
HOST_LAYOUT = [4, 8, 9, 12, 16]

STUBS = CL.Stubs()
STUBS.real.add(FN)
CUR = {"obj": 0}


def name_hash(s):
    h = 0
    for ch in s.encode():
        h = (h * 31 + ch) & 0xFFFFFF
    return h


def make_stub(name):
    def stub(e):
        L = e.cl
        L.rec(name, (s32(e.r(4) - CUR["obj"]), name_hash(e.cstr(e.r(5))), s32(e.r(6))))
        e.set_r(3, L.ret("answer") & MASK)
    return stub


def s32(v):
    return T.s32(v)


for _addr, (_name, _x) in CALLEES.items():
    STUBS.internal(_addr, make_stub(_name))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        a = self.e.alloc(n)
        CUR["obj"] = a
        return a

    def write(self, a, d):
        self.e.write(a, d)

    def read(self, a, n):
        return self.e.read(a, n)

    def log(self):
        return self.e.cl.log

    def call(self, obj, save, version, unused):
        r3, _, _ = T.call_ppc(self.e, FN, [obj, 0, save & MASK, version & MASK, unused & MASK])
        return r3 & MASK

    fmt = ">20B"


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_crs_alloc(n)

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def read(self, a, n):
        return C.string_at(a, n)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, obj, save, version, unused):
        return self.lib.tpw_crs_serialise(obj, s32(save), s32(version), unused & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_crs_alloc.restype, lib.tpw_crs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_crs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_crs_serialise.restype = C.c_uint32
    lib.tpw_crs_serialise.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_crs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        obj = s.alloc(20)
        s.write(obj, bytes(case["obj"]))
        ans = s.call(obj, case["save"], case["version"], case["unused"])
        return {"answer": ans, "obj": s.read(obj, 20), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, save, version, answers):
    return {"save": save, "version": version, "unused": r.choice([0, 1, 0xDEADBEEF]),
            "obj": [r.randrange(256) for _ in range(20)], "queues": {"answer": answers}}


def edges():
    r = random.Random(5654)
    for save in (0, 1, 7):
        for version in (-1, 0, 1, 2, 3, -100):
            yield mk(r, save, version, [0])
    for save in (0, 1):
        for k in range(6):
            yield mk(r, save, 1, [0] * k + [1])
            yield mk(r, save, 0, [0] * k + [2])


def rnd(r):
    return mk(r, r.choice([0, 1, 2, -1]), r.choice([-1, 0, 1, 2, 5]), [r.choice([0, 0, 0, 1]) for _ in range(6)])


TESTS = [FnTest("CResearchProject::Serialise", ppc, host, edge=edges, rnd=rnd)]
