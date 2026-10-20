"""Unit v5666, the user interface's message receiver, against libtpw_unit_CUIMsgReceiver_ReceiveMessage.dylib
(tests/unit_CUIMsgReceiver_ReceiveMessage.cpp), main app, src/vector2d_CUIMsgReceiver_ReceiveMessage.cpp:
  CUIMsgReceiver::ReceiveMessage 0x10130998

A case is a message (its type from GetType, a word at +8 and one at +0xc) and a call. On the PowerPC side the message has a vtable whose
slot 2 is a function descriptor for a stub that answers the case's type; on the host the message is a test subclass. The eight
UIInterface callees are fakes on both sides that record the object (a label) and the arguments. Compared: the call log. No fused operations.
Not tested: nothing (a NULL message is dereferenced unconditionally by the original).
"""
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CUIMsgReceiver_ReceiveMessage"
MASK = 0xFFFFFFFF

FN = 0x10130998
SIZES = {FN: 364}
OWN = set(SIZES)
GETTYPE = 0x10110f34          # an in-binary function used only as the stub address behind the message's vtable slot
PTR_GLUE = 0x1023baa8
UI = 0x10396440
TABLE = 0x102a4420

# message type -> kind: 1 when the word at +8 is a uint16 id (high half), 0 when it is an int; None when nothing happens
KINDS = {7: 1, 8: 1, 10: 1, 15: 1, 27: 1, 28: 1}

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.add(PTR_GLUE)


def stub_gettype(e):
    e.set_r(3, e.cl.ret("GetType") & MASK)


STUBS.internal(GETTYPE, stub_gettype)


def mk_id(name, addr):
    def stub(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    STUBS.internal(addr, stub)


def mk_int(name, addr):
    def stub(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)), T.s32(e.r(4))))
    STUBS.internal(addr, stub)


def stub_enable(e):
    e.cl.rec("EnableResearchButton", (e.cl.lab(e.r(3)), e.r(4) & 0xFF))


def stub_staff(e):
    e.cl.rec("StaffRoomEvent", (e.cl.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF, T.s32(e.r(5))))


STUBS.internal(0x101f10f8, stub_enable)
mk_int("ResearchCategoryComplete", 0x101efc14)
mk_id("RideDeleted", 0x101efc84)
mk_id("RideBrokenDown", 0x101efce4)
mk_id("RideCondemned", 0x101efd74)
mk_id("ThingDeath", 0x101f0c2c)
mk_id("KidArrived", 0x101f0d14)
STUBS.internal(0x101f0dcc, stub_staff)


def setup(runner):
    T.install_std_stubs(runner.emu)
    e = runner.emu


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({UI: 1}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def log(self):
        return self.L.log

    def call(self, case):
        e = self.e
        tv = e.alloc(8)
        e.write(tv, GETTYPE.to_bytes(4, "big") + (e.r(2) & MASK).to_bytes(4, "big"))
        vt = e.alloc(16)
        e.write(vt + 8, tv.to_bytes(4, "big"))
        msg = e.alloc(16)
        e.write(msg, vt.to_bytes(4, "big") + (0).to_bytes(4, "big") + (case["w8"] & MASK).to_bytes(4, "big") + (case["wc"] & MASK).to_bytes(4, "big"))
        this = e.alloc(16)
        T.call_ppc(e, FN, [this, msg])


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: lib.tpw_uimsg_addr(b"ui")}, case["queues"])

    def done(self):
        pass

    def log(self):
        return CL.host_end(self.lib)

    def call(self, case):
        self.lib.tpw_uimsg_receive(KINDS.get(case["type"], 0), case["w8"] & MASK, case["wc"] & MASK)


def bind(lib):
    import ctypes as C
    if getattr(lib, "bound", False):
        return
    lib.tpw_uimsg_addr.restype, lib.tpw_uimsg_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_uimsg_receive.restype, lib.tpw_uimsg_receive.argtypes = None, [C.c_uint32, C.c_uint32, C.c_uint32]
    lib.bound = True


def run(s, case):
    try:
        s.call(case)
        return {"log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk_case(r, t, w8=None, wc=None):
    kind = KINDS.get(t, 0)
    if w8 is None:
        w8 = (r.choice([0, 1, 0xFFFF, r.randrange(0x10000)]) << 16 | r.randrange(0x10000)) if kind else r.choice([0, 1, 5, 6, 7, 0xFFFFFFFF, r.randrange(1 << 32)])
    return {"type": t, "w8": w8, "wc": wc if wc is not None else r.choice([0, 1, 0xFFFFFFFF, r.randrange(1 << 32)]),
            "queues": {"GetType": [t & MASK]}}


def edges():
    r = random.Random(5666)
    for t in list(range(-3, 36)) + [0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]:
        yield mk_case(r, t)
    for w8 in (4, 5, 6, 7, 0x00050000, 0x00060000, 0xFFFFFFFF, 0x80000000):
        for t in range(7, 29):
            yield mk_case(r, t, w8)


def rnd(r):
    return mk_case(r, r.choice(list(range(0, 36)) + [r.randrange(-100, 100)]))


TESTS = [
    FnTest("CUIMsgReceiver::ReceiveMessage", ppc, host, edge=edges, rnd=rnd),
]
