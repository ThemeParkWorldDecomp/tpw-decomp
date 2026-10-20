"""Unit v5222, CParkTime::Serialise 0x10111fe0 (the park time to or from a save file), against libtpw_unit_CParkTime_Serialise.dylib
(tests/unit_CParkTime_Serialise.cpp), main app, src/ParkTime_update_CParkTime_Serialise.cpp.

A case is an object (nine words) and a call (file, save, version, unused). The six callees (SAVEVAR / LOADVAR of int and unsigned long,
SAVEVARSTRUCT / LOADVARSTRUCT of TbTimeStamp) are fakes on both sides that record the file, the offset of the value in the object, the name
(its index among the five names) and the no-log flag, and answer from a queue per callee.

What is compared: the answer, the nine words of the object, the event log. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CParkTime_Serialise"
MASK = 0xFFFFFFFF

FN = 0x10111FE0
NAMES = {0x102438B5: 0, 0x102438C5: 1, 0x102438D3: 2, 0x102438E6: 3, 0x102438F7: 4}
KEYS = ["SaveStamp", "SaveInt", "SaveUlong", "LoadStamp", "LoadInt", "LoadUlong"]
HOST_LAYOUT = [0x10, 0x14, 0x1C, 0x24]

OBJ = [0]
STUBS = CL.Stubs()
STUBS.real.add(FN)


def make_stub(event):
    def stub(e):
        L = e.cl
        name = NAMES.get(e.r(5) & MASK, -1)
        L.rec(event, ((e.r(3) & MASK), T.s32(e.r(4) - OBJ[0]), name, T.s32(e.r(6))))
        e.set_r(3, L.ret(event) & MASK)
    return stub


for _key, _addr in (("SaveStamp", 0x100F1B1C), ("LoadStamp", 0x100F1468), ("SaveInt", 0x1000F4B0), ("LoadInt", 0x1000EF18),
                    ("SaveUlong", 0x1000F5DC), ("LoadUlong", 0x1000F02C)):
    STUBS.internal(_addr, make_stub(_key))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        a = self.e.alloc(n)
        OBJ[0] = a
        return a

    def words(self, a, ws):
        self.e.write(a, b"".join((w & MASK).to_bytes(4, "big") for w in ws))

    def rd(self, a, n):
        return [int.from_bytes(self.e.read(a + 4 * i, 4), "big") for i in range(n)]

    def log(self):
        return self.L.log

    def call(self, obj, args):
        r3, _, _ = T.call_ppc(self.e, FN, [obj] + [a & MASK for a in args])
        return r3 & MASK


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_cpt_alloc(n)

    def words(self, a, ws):
        C.memmove(a, b"".join((w & MASK).to_bytes(4, "little") for w in ws), 4 * len(ws))

    def rd(self, a, n):
        raw = C.string_at(a, 4 * n)
        return [int.from_bytes(raw[4 * i:4 * i + 4], "little") for i in range(n)]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, obj, args):
        f, save, ver, unused = args
        return self.lib.tpw_cpt_serialise(obj, f & MASK, T.s32(save), T.s32(ver), unused & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cpt_alloc.restype, lib.tpw_cpt_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cpt_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cpt_serialise.restype = C.c_int32
    lib.tpw_cpt_serialise.argtypes = [C.c_uint64, C.c_uint32, C.c_int32, C.c_int32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cpt_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        obj = s.alloc(0x24)
        s.words(obj, case["words"])
        ans = s.call(obj, case["args"])
        return {"answer": ans, "log": s.log(), "obj": s.rd(obj, 9)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, save, version, fails=None):
    q = {k: [0] for k in KEYS}
    if fails is not None:
        key, idx = fails
        q[key] = [0] * idx + [1]
    return {"queues": q, "words": [r.getrandbits(32) for _ in range(9)],
            "args": [r.getrandbits(32), save, version, r.getrandbits(32)]}


def edges():
    r = random.Random(5222)
    for save in (0, 1, -1, 7, 0x80000000):
        for ver in (-0x80000000, -2, -1, 0, 1, 2, 3, 0x7FFFFFFF):
            yield mk(r, save, ver)
    for key in KEYS:
        n = 2 if key.endswith("Stamp") or key.endswith("Int") else 1
        for idx in range(n):
            for ver in (0, 1):
                yield mk(r, 1 if key.startswith("Save") else 0, ver, (key, idx))


def rnd(r):
    save = r.choice([0, 1, r.getrandbits(32)])
    ver = r.choice([-1, 0, 1, 2, r.getrandbits(32)])
    fails = None
    if r.random() < 0.5:
        fails = (r.choice(KEYS), r.randrange(2))
    return mk(r, save, ver, fails)


TESTS = [FnTest("CParkTime::Serialise", ppc, host, edge=edges, rnd=rnd)]
