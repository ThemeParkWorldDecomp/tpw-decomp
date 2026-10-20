"""Unit v5580, RideStatusPanel::Condemned and RideStatusPanel::InitStatusWindow, against libtpw_unit_RideStatusPanel_Condemned.dylib
(tests/unit_RideStatusPanel_Condemned.cpp), main app ("SimTheme Park"), src/LCPostFormat_RideStatusPanel_UpdateStatusWindow_RideStatusPanel_Condemned.cpp:
  RideStatusPanel::Condemned 0x101bc2f0, RideStatusPanel::InitStatusWindow 0x101b9340

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. The callees outside the unit (GAME_ASSERTMSG, GetWindow,
SetClientDrawObject, GetFontTableEntry, SetFont, UIMain_GetString, SetClientDrawText, UIMain_IndentClient, operator new, the five
constructors and the two virtual calls through the glue) are fakes on both sides that record their arguments. The windows are 15 blocks
(label n + 1 for block n; block 0 is the status window, block k answers GetWindow(0x3e15 + k)).

The world of a case: the panel's window, status window and ops table, entry 0 and entry idx of the thing array, the staff room thing.
What is compared: the event log. No fused operations. Not tested: an index past the thing array (the original reads past it as well).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RideStatusPanel_Condemned"
MASK = 0xFFFFFFFF

FN_COND, FN_INIT = 0x101bc2f0, 0x101b9340
SIZES = {FN_COND: 156, FN_INIT: 1804}
OWN = set(SIZES)

ADDR = dict(thingarr=0x1034cbfc, staff=0x103947a4)
HOST_LAYOUT = [0x2800]
NWIN = 15
WSTRIDE = 0x140
MARK_SLOT5, MARK_SETVALUE = 0xDEAD0005, 0xDEAD0007

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lab_of(e, a):
    return e.cl.lab(a)


STUBS.internal(0x1000dde8, CL.recorder("assert", "is"))
STUBS.internal(0x101dbe40, CL.recorder("GetWindow", "Li", ret=lambda e, log: ADDR["win%d" % (T.s32(e.r(4)) - 0x3e16 + 1)]))
STUBS.internal(0x101db624, CL.recorder("SetClientDrawObject", "LL"))
STUBS.internal(0x10180270, CL.recorder("GetFontTableEntry", "u", ret=lambda e, log: 0x5000 + e.r(3)))
STUBS.internal(0x101da118, CL.recorder("SetFont", "Lu"))
STUBS.internal(0x10180468, CL.recorder("GetString", "i", ret=lambda e, log: 0x7000 + T.s32(e.r(3))))
STUBS.internal(0x101de130, CL.recorder("SetClientDrawText", "Lu"))
STUBS.internal(0x10180940, CL.recorder("IndentClient", "Lhh"))
STUBS.internal(0x102384b8, CL.recorder("new", "u", ret=lambda e, log: e.alloc(e.r(3))))
STUBS.internal(0x1018d11c, CL.recorder("SpriteCtor", "Nx"))
STUBS.internal(0x1018d930, CL.recorder("NumberCtor", "Nx"))
STUBS.internal(0x1018de60, CL.recorder("CashCtor", "Nx"))
STUBS.internal(0x101898c8, CL.recorder("GradientCtor", "Ns"))


def glue(e):
    """__ptr_glue: r12 is the slot's content, a marker the world put there."""
    L = e.cl
    m = e.r(12) & MASK
    if m == MARK_SLOT5:
        L.rec("Slot5", ())
    elif m == MARK_SETVALUE:
        L.rec("SetValue", (L.lab(e.r(3)), T.s32(e.r(4))))
    else:
        raise AssertionError("unknown glue target %x" % m)


STUBS.internal(0x1023baa8, glue)


def setup(runner):
    em = runner.emu
    T.install_std_stubs(em)
    vt = T.persist_alloc(em, 0x40, struct.pack(">I", 0) * 16)
    em.write(vt + 0x24, struct.pack(">I", MARK_SETVALUE))
    for k in range(NWIN):
        w = T.persist_alloc(em, WSTRIDE, b"\0" * WSTRIDE)
        em.write(w + 0x118, struct.pack(">I", vt))
        ADDR["win%d" % k] = w
    ops = T.persist_alloc(em, 0x40, b"\0" * 0x40)
    em.write(ops + 0x1c, struct.pack(">I", MARK_SLOT5))
    ADDR["ops"] = ops
    ADDR["panel"] = T.persist_alloc(em, 0x40, b"\0" * 0x40)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {ADDR["win%d" % k]: k + 1 for k in range(NWIN)}
        fixed[ADDR["panel"]] = 16
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_panel(self, window, status):
        wa = ADDR["win%d" % window] if window is not None else 0
        sa = ADDR["win%d" % status] if status is not None else 0
        self.words(ADDR["panel"], [0, 0, 0, wa, sa, 0, 0, 0, ADDR["ops"]])

    def set_thing(self, idx, v):
        self.words(ADDR["thingarr"] + idx * 0x14, [v])

    def set_staff(self, v):
        self.words(ADDR["staff"], [v])

    def cond(self, idx):
        T.call_ppc(self.e, FN_COND, [ADDR["panel"], (idx << 16) & MASK])

    def init(self):
        T.call_ppc(self.e, FN_INIT, [ADDR["panel"]])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = {k + 1: self.lib.tpw_ride_addr(b"win%d" % k) for k in range(NWIN)}
        fixed[16] = self.lib.tpw_ride_addr(b"panel")
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_panel(self, window, status):
        wa = self.lib.tpw_ride_addr(b"win%d" % window) if window is not None else 0
        sa = self.lib.tpw_ride_addr(b"win%d" % status) if status is not None else 0
        self.lib.tpw_ride_panel_set(wa, sa)

    def set_thing(self, idx, v):
        self.lib.tpw_ride_set_thing(idx, v)

    def set_staff(self, v):
        self.lib.tpw_ride_set_staff(v)

    def cond(self, idx):
        self.lib.tpw_ride_Condemned(idx)

    def init(self):
        self.lib.tpw_ride_InitStatusWindow()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ride_addr.restype, lib.tpw_ride_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ride_alloc.restype, lib.tpw_ride_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ride_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ride_panel_set.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_ride_set_thing.argtypes = [C.c_uint32, C.c_uint64]
    lib.tpw_ride_set_staff.argtypes = [C.c_uint64]
    lib.tpw_ride_Condemned.argtypes = [C.c_uint32]
    lib.tpw_ride_InitStatusWindow.argtypes = []
    out = (C.c_int64 * 32)()
    n = lib.tpw_ride_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.set_panel(case["win"], case["status"])
    s.set_thing(0, case["t0"])
    s.set_thing(case["idx"], case["ti"])
    s.set_staff(case["staff"])


def execute(s, case):
    if case["op"] == "cond":
        s.cond(case["idx"])
    else:
        s.init()


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return {"log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


THINGS = [0, 0x1111, 0x2222]


def base(r, op):
    return {"op": op, "queues": {}, "win": r.choice([None, 0, 1]), "status": r.choice([None, 0, 0]),
            "idx": r.choice([0, 1, 7, 0x27ff, r.randrange(0, 0x2800)]), "t0": r.choice(THINGS), "ti": r.choice(THINGS),
            "staff": r.choice(THINGS)}


def cond_edges():
    r = random.Random(5)
    for win in (None, 0):
        for idx in (0, 3):
            for t0 in THINGS:
                for ti in THINGS:
                    for staff in THINGS:
                        c = base(r, "cond")
                        c.update(win=win, idx=idx, t0=t0, ti=ti, staff=staff)
                        yield c


def cond_random(r):
    c = base(r, "cond")
    if r.random() < 0.5:
        c["ti"] = c["staff"]
    return c


def init_edges():
    r = random.Random(6)
    for status in (None, 0):
        c = base(r, "init")
        c["status"] = status
        yield c


def init_random(r):
    c = base(r, "init")
    c["status"] = r.choice([None, 0])
    return c


TESTS = [
    FnTest("RideStatusPanel::Condemned", ppc, host, edge=cond_edges, rnd=cond_random),
    FnTest("RideStatusPanel::InitStatusWindow", ppc, host, edge=init_edges, rnd=init_random),
]
