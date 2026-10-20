"""Unit v5030, keyboard actions win_ticket_0..6 and enter_area_delete_mode, against libtpw_unit_win_ticket_1.dylib
(tests/unit_win_ticket_1.cpp), main app ("SimTheme Park"), src/ui_Postcard_panel_win_ticket_1.cpp:
  win_ticket_0 0x1014ed40, _1 0x1014edd8, _2 0x1014ee70, _3 0x1014ef08, _4 0x1014efa0, _5 0x1014f038, _6 0x1014f0d0,
  enter_area_delete_mode 0x1014ec48

The world: gMasterSave and gMouse (null or a sentinel pointer) and the two mouse words. The callees (constructors, EarnedTicket,
AssignMouseMode) are recording fakes on both sides. Compared: the answer, the log, the pointer globals (null / sentinel / new object)
and the two mouse words. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_win_ticket_1"
MASK = 0xFFFFFFFF

FNS = {
    "win_ticket_0": (0x1014ed40, 0), "win_ticket_1": (0x1014edd8, 1), "win_ticket_2": (0x1014ee70, 2),
    "win_ticket_3": (0x1014ef08, 3), "win_ticket_4": (0x1014efa0, 4), "win_ticket_5": (0x1014f038, 5),
    "win_ticket_6": (0x1014f0d0, 6), "enter_area_delete_mode": (0x1014ec48, 7),
}
OWN = {a for a, _ in FNS.values()}

ADDR = dict(gMasterSave=0x10380a8c, gMouse=0x102dc1f4, w1E0=0x102dc1e0, w1E4=0x102dc1e4)
SENT_MS, SENT_MOUSE = 0x1000, 0x2000

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(0x101694dc, CL.recorder("MSCtor", "N"))
STUBS.internal(0x1016a200, CL.recorder("EarnedLocal", "Li"))
STUBS.internal(0x1016a2a0, CL.recorder("EarnedGlobal", "Lii"))
STUBS.internal(0x1007e6e4, CL.recorder("PathDrawCtor", "Ni"))
STUBS.internal(0x1007ea78, CL.recorder("DefaultCtor", "N"))
STUBS.internal(0x1007e2e0, CL.recorder("Assign", "LL"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    ptr = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({SENT_MS: 1, SENT_MOUSE: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, d):
        self.e.write(a, d)

    def log(self):
        return self.L.log

    def call(self, idx, addr):
        r3, _, _ = T.call_ppc(self.e, addr, [])
        return r3 & MASK


class Host:
    ptr = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: SENT_MS, 2: SENT_MOUSE}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_wt_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, idx, addr):
        return self.lib.tpw_wt_call(idx) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_wt_addr.restype, lib.tpw_wt_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_wt_call.restype, lib.tpw_wt_call.argtypes = C.c_int32, [C.c_int32]
    lib.bound = True


def wptr(s, name, v):
    s.write(s.g(name), struct.pack(s.ptr, v))


def rptr(s, name):
    return struct.unpack(s.ptr, s.read(s.g(name), struct.calcsize(s.ptr)))[0]


def word(s, name):
    return struct.unpack(s.ptr[0] + "I", s.read(s.g(name), 4))[0]


def cls(v, sent):
    return "null" if v == 0 else ("sentinel" if v == sent else "new")


def run(s, case, name):
    try:
        wptr(s, "gMasterSave", case["ms"])
        wptr(s, "gMouse", case["mouse"])
        for k in ("w1E0", "w1E4"):
            s.write(s.g(k), struct.pack(s.ptr[0] + "I", case[k]))
        addr, idx = FNS[name]
        ans = s.call(idx, addr)
        return {"answer": ans, "log": s.log(), "ms": cls(rptr(s, "gMasterSave"), SENT_MS),
                "mouse": cls(rptr(s, "gMouse"), SENT_MOUSE), "w1E0": word(s, "w1E0"), "w1E4": word(s, "w1E4")}
    finally:
        s.done()


def mk(name):
    return (lambda e, case: run(Ppc(e, case), case, name)), (lambda lib, case: run(Host(lib, case), case, name))


def case(r, ms=None, mouse=None):
    return {"queues": {}, "ms": r.choice([0, SENT_MS]) if ms is None else ms,
            "mouse": r.choice([0, SENT_MOUSE]) if mouse is None else mouse,
            "w1E0": r.choice([0, 5, MASK, r.getrandbits(32)]), "w1E4": r.choice([0, 7, MASK, r.getrandbits(32)])}


def edges():
    r = random.Random(5030)
    for ms in (0, SENT_MS):
        for mouse in (0, SENT_MOUSE):
            yield case(r, ms, mouse)


def rnd(r):
    return case(r)


def test(name):
    p, h = mk(name)
    return FnTest(name, p, h, edge=edges, rnd=rnd)


TESTS = [test(n) for n in FNS]
