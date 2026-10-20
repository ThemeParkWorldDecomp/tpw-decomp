"""Unit v5760, CMessageCentre::Save, Load and UnsubscribeAll, against libtpw_unit_CMessageCentre_Save.dylib
(tests/unit_CMessageCentre_Save.cpp), main app ("SimTheme Park"), src/ThingLoop_CMessageCentre_Save.cpp:
  Save 0x10152758, Load 0x10152840, UnsubscribeAll 0x10152448

A case is a message centre block (0x1d0 bytes, contents irrelevant: every callee is a fake) and a call. The callees outside the unit,
LbFile_Read, LbFile_Write, CSubscriptionHeader::Save and Load, the subscription tree's clear, find and erase, and CThing::GetThingId, are
fakes on both sides that record their arguments (header or tree index, file label, the four bytes written, the key) and answer from queues.

What is compared: the answer and the event log. No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CMessageCentre_Save"
MASK = 0xFFFFFFFF

FN_SAVE, FN_LOAD, FN_UNSUB = 0x10152758, 0x10152840, 0x10152448
SIZES = {FN_SAVE: 184, FN_LOAD: 184, FN_UNSUB: 228}
OWN = set(SIZES)

LBWRITE, LBREAD = 0x10238878, 0x10238848
HSAVE, HLOAD, CLEAR, FIND, ERASE, GETID = 0x10152928, 0x10152a10, 0x10152e64, 0x10152c58, 0x10152f3c, 0x1012e1f8
HOST_LAYOUT = [0x1d0]
FOUND = 0x30000010      # what the PowerPC find fake answers for "found" (never the end node)

CUR = {"centre": 0}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def idx(a):
    return (a - CUR["centre"]) // 16


def stub_write(e):
    L = e.cl
    b = e.read(e.r(5), 4)
    L.rec("Write", (L.lab(e.r(4)), e.r(6) & MASK, 1 if e.r(7) else 0, b[0], b[1], b[2], b[3]))


def stub_read(e):
    L = e.cl
    L.rec("Read", (L.lab(e.r(4)), e.r(6) & MASK, 1 if e.r(5) else 0, 1 if e.r(7) else 0))


def stub_hsave(e):
    L = e.cl
    L.rec("HSave", (idx(e.r(3)), L.lab(e.u32(e.r(4)))))
    e.set_r(3, L.ret("HSave") & MASK)


def stub_hload(e):
    L = e.cl
    L.rec("HLoad", (idx(e.r(3)), L.lab(e.u32(e.r(4)))))
    e.set_r(3, L.ret("HLoad") & MASK)


def stub_clear(e):
    e.cl.rec("Clear", (idx(e.r(3) - 4),))


def stub_find(e):
    L = e.cl
    tree = e.r(4)
    key = int.from_bytes(e.read(e.r(5), 2), "big")
    L.rec("Find", (idx(tree - 4), key))
    e.write(e.r(3), ((FOUND if L.ret("Find") else tree + 4) & MASK).to_bytes(4, "big"))


def stub_erase(e):
    e.cl.rec("Erase", (idx(e.r(3) - 4), 1 if e.r(4) == FOUND else 0))


def stub_getid(e):
    L = e.cl
    L.rec("GetThingId", (L.lab(e.r(4)),))
    e.write(e.r(3), (L.ret("Id") & 0xFFFF).to_bytes(2, "big"))


STUBS.internal(LBWRITE, stub_write)
STUBS.internal(LBREAD, stub_read)
STUBS.internal(HSAVE, stub_hsave)
STUBS.internal(HLOAD, stub_hload)
STUBS.internal(CLEAR, stub_clear)
STUBS.internal(FIND, stub_find)
STUBS.internal(ERASE, stub_erase)
STUBS.internal(GETID, stub_getid)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def alloc_world(self):
        self.centre = self.alloc(0x1d0)
        self.file = self.alloc(16)
        self.thing = self.alloc(16)
        self.cell = self.alloc(8)


class Ppc(Side):
    def __init__(self, e, case):
        self.e = e
        self.alloc = e.alloc
        self.alloc_world()
        CUR["centre"] = self.centre
        self.L = e.cl = CL.PpcLog({self.file: 1, self.thing: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def setcell(self):
        self.e.write(self.cell, self.file.to_bytes(4, "big"))

    def log(self):
        return self.L.log

    def call(self, op):
        if op == "unsub":
            r3, _, _ = T.call_ppc(self.e, FN_UNSUB, [self.centre, self.thing])
        else:
            r3, _, _ = T.call_ppc(self.e, FN_SAVE if op == "save" else FN_LOAD, [self.centre, self.cell])
        return r3 & MASK


class Host(Side):
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.alloc = lib.tpw_cmc_alloc
        self.alloc_world()
        self.cellv = C.c_uint64(self.file)
        CL.host_begin(lib, {1: self.file, 2: self.thing}, case["queues"])

    def done(self):
        pass

    def setcell(self):
        pass

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        fn = {"save": self.lib.tpw_cmc_save, "load": self.lib.tpw_cmc_load}.get(op)
        if fn:
            return fn(self.centre, self.file) & MASK
        return self.lib.tpw_cmc_unsub(self.centre, self.thing) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cmc_"
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    for n in ("save", "load", "unsub"):
        f = getattr(lib, p + n)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.setcell()
        ans = s.call(case["op"])
        return {"answer": ans, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(op, **q):
    return {"op": op, "queues": {k: v for k, v in q.items()}}


def rnd_case(r, op):
    def seq(vals):
        return [r.choice(vals) for _ in range(29)]
    q = {"HSave": [0] * 29, "HLoad": [0] * 29}
    if r.random() < 0.5:
        k = r.randrange(29)
        q["HSave"][k] = r.choice([-1, 1, 5, 0x80000000, 0x7FFFFFFF])
        q["HLoad"][k] = r.choice([-1, 1, 5, 0x80000000, 0x7FFFFFFF])
    q["Find"] = seq([0, 1])
    q["Id"] = seq([0, 1, 0xFFFF, r.randrange(65536)])
    return {"op": op, "queues": q}


def edges(op):
    def gen():
        z = [0] * 29
        for k in (0, 1, 14, 27, 28):
            for v in (-1, 3):
                h = list(z)
                h[k] = v
                yield {"op": op, "queues": {"HSave": h, "HLoad": h, "Find": [0], "Id": [7]}}
        yield {"op": op, "queues": {"HSave": z, "HLoad": z, "Find": [0], "Id": [0]}}
        yield {"op": op, "queues": {"HSave": z, "HLoad": z, "Find": [1], "Id": [0xFFFF]}}
        for k in range(29):
            f = [0] * 29
            f[k] = 1
            yield {"op": op, "queues": {"HSave": z, "HLoad": z, "Find": f, "Id": list(range(1, 30))}}
    return gen


TESTS = [
    FnTest("CMessageCentre::Save", ppc, host, edge=edges("save"), rnd=lambda r: rnd_case(r, "save")),
    FnTest("CMessageCentre::Load", ppc, host, edge=edges("load"), rnd=lambda r: rnd_case(r, "load")),
    FnTest("CMessageCentre::UnsubscribeAll", ppc, host, edge=edges("unsub"), rnd=lambda r: rnd_case(r, "unsub")),
]
