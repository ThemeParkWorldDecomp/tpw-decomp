"""Unit v5534, the ride status panel's window handler, against libtpw_unit_RideStatusMsg.dylib (tests/unit_RideStatusMsg.cpp), main app
("SimTheme Park"), src/game/ui/ui_RideStatus.cpp:
  RideStatusMsg 0x101bbd78

A case is a message with its two arguments. The world is built twice by one piece of Python code. Every callee (the panel's methods, the
timers, CreateAllItemsPanel, GoFirstPerson, DefaultStatusPanelMsg, the panel's table call through __ptr_glue) is a fake on both sides
that records its arguments and answers from the case's queues.

The world of a case: the thing global (a block), the panel's table pointer at +0x20 (only the PowerPC side needs the Mac table; the host
library installs its fake table). What is compared: the answer, the event log and the thing global. No fused operations.
Not tested: nothing left out.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RideStatusMsg"
MASK = 0xFFFFFFFF

FN = 0x101BBD78
SIZES = {FN: 780}
OWN = set(SIZES)

ADDR = dict(panel=0x103959A4, thing=0x103947A4)
HOST_LAYOUT = []

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def lab(e, a):
    return e.cl.lab(a)


def mk(name, addr, fields):
    """A fake that records (name, fields(e)); fields get the emulator."""
    def stub(e):
        e.cl.rec(name, tuple(fields(e)))
    STUBS.internal(addr, stub)


mk("Tick", 0x10181570, lambda e: ())
mk("Remove", 0x101BC1A0, lambda e: (lab(e, e.r(3)),))
mk("Cancel", 0x1018227C, lambda e: (s32(e.r(3)),))
mk("StartAnim", 0x101DD6B8, lambda e: (lab(e, e.r(3)), s32(e.r(4))))
mk("UpdateStatus", 0x101B9AC4, lambda e: (lab(e, e.r(3)),))
mk("Edit", 0x101BB71C, lambda e: (lab(e, e.r(3)), s32(e.r(4))))
mk("Move", 0x101938D8, lambda e: (lab(e, e.r(3)),))
mk("Close", 0x10193010, lambda e: (lab(e, e.r(3)), s32(e.r(4))))
mk("Maint", 0x101932A0, lambda e: (lab(e, e.r(3)), e.r(4) & 0xFF))
mk("Erase", 0x10193138, lambda e: (lab(e, e.r(3)),))
mk("FirstPerson", 0x1010ED8C, lambda e: (lab(e, e.r(3)), s32(e.r(4))))
mk("ShowUpgrade", 0x101BA6E8, lambda e: (lab(e, e.r(3)),))
mk("Next", 0x10192C28, lambda e: (lab(e, e.r(3)),))
mk("Prev", 0x10192DE0, lambda e: (lab(e, e.r(3)),))
mk("CreateAll", 0x101983C4, lambda e: (s32(e.r(3)), lab(e, e.r(4))))
mk("DoDial", 0x101BB9B8, lambda e: (lab(e, e.r(3)), s32(e.r(4)), s32(e.r(5))))
mk("PtrGlue", 0x1023BAA8, lambda e: (lab(e, e.r(3)),))


def stub_setanim(e):
    L = e.cl
    L.rec("SetAnim", (lab(e, e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, L.ret("SetAnim") & MASK)


def stub_default(e):
    L = e.cl
    L.rec("Default", (lab(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("Default") & MASK)


STUBS.internal(0x101DD5E0, stub_setanim)
STUBS.internal(0x10193C78, stub_default)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        import struct
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        import struct
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.thing = e.alloc(16)
        self.window = e.alloc(16)
        self.L = e.cl = CL.PpcLog({0x103959A4: 1, self.thing: 2, self.window: 3}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def setops(self):
        ops = self.e.alloc(0x40)
        self.words(0x103959A4 + 0x20, [ops])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, m, a, b):
        r3, _, _ = T.call_ppc(self.e, FN, [self.window, m & MASK, a & MASK, b & MASK])
        return s32(r3)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.thing = lib.tpw_rsm_alloc(16)
        self.window = lib.tpw_rsm_alloc(16)
        CL.host_begin(lib, {1: self.g("panel"), 2: self.thing, 3: self.window}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rsm_addr(name.encode())

    def setops(self):
        self.lib.tpw_rsm_setops()

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, m, a, b):
        return self.lib.tpw_rsm_msg(self.window, s32(m), s32(a), s32(b))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rsm_addr.restype, lib.tpw_rsm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rsm_alloc.restype, lib.tpw_rsm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rsm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rsm_setops.restype, lib.tpw_rsm_setops.argtypes = None, []
    lib.tpw_rsm_msg.restype, lib.tpw_rsm_msg.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rsm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.bound = True


def run(s, case):
    try:
        s.setops()
        if isinstance(s, Host):
            s.write(s.g("thing"), s.thing.to_bytes(8, "little"))
        else:
            s.words(s.g("thing"), [s.thing])
        ans = s.call(*case["args"])
        return {"answer": ans, "log": s.log(), "thing": s.rd_word(s.g("thing")) == (s.thing & MASK)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


MSGS = [5, 0, 1, 0xF, 0x10, 0x11, 0x13, 0x14, 0x15, 0x16, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0xFF, 0x100, 0x101, 0x7FF, 0x800, 0x801, -1,
        0x7FFFFFFF]
AS = [-0x80000000, -4, -3, -2, -1, 0, 1, 0x3E25, 0x3E26, 0x3E27, 0x3E28, 0x3E29, 0x3E2A, 0x3E2B, 0x3E2C, 0x3E2D, 0x3E2E, 0x3E33,
      0x3E34, 0x3E35, 0x3E36, 0x3E37, 0x3E38, 0x3E39, 0x3E3A, 0x80080, 0x80081, 0x7FFFF, 0x7FFFFFFF]
BS = [0, 1, 2, -1]


def case(r, m, a, b):
    return {"args": [m, a, b], "queues": {"SetAnim": [r.choice([0, 1, -1])], "Default": [r.choice([0, 1, 7, -5])]}}


def edges():
    r = random.Random(534)
    for m in MSGS:
        for a in ([0x80080, 0x80081, 0x7FFFF, 0, 3] if m != 0x100 else AS):
            for b in (BS if m == 0x100 else [0, 1]):
                if m in (0x15,):
                    for q in (0, 1):
                        c = case(r, m, a, b)
                        c["queues"]["SetAnim"] = [q]
                        yield c
                else:
                    yield case(r, m, a, b)


def rnd(r):
    m = r.choice([0x100, 0x100, 0x100, r.choice(MSGS)])
    a = r.choice(AS + [r.randrange(-0x8000, 0x8000), r.randrange(0x3E00, 0x3E50)])
    b = r.choice(BS + [r.randrange(-100, 100)])
    return case(r, m, a, b)


TESTS = [
    FnTest("RideStatusMsg", ppc, host, edge=edges, rnd=rnd),
]
