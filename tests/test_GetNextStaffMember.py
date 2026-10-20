"""Unit v51001, GetNextStaffMember and ShowStatusPanel::InitWindow, against libtpw_unit_GetNextStaffMember.dylib
(tests/unit_GetNextStaffMember.cpp), main app ("SimTheme Park"), src/ui_staffPanel_msg_hackfollowmode_GetNextStaffMember.cpp:
  GetNextStaffMember 0x101bf32c, ShowStatusPanel::InitWindow 0x101c1c90

A case is a world and a call, built twice by one piece of Python code (emulator memory for the original, the host library for the
translation). Every callee outside the unit is a fake on both sides that records its arguments; the pointers the fakes answer
(things, windows, the font, the string) come from a pool of blocks, chosen by the case's queues (index 0 is NULL).

The world: gStaffPanelLocation, gStaffPanelThing, gShowStatusThing, a panel object whose word at +0xc holds the parent window; the
queues of the fakes. What is compared: the log of every call and the thing global afterwards.

No fused operations. Not tested: nothing skipped (the Show calls need a window, so GetWindow answers non-NULL for 0xa08c and 0xa08d
when FillObjectList answers below 2: the original does not test it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_GetNextStaffMember"
MASK = 0xFFFFFFFF

FN_NEXT, FN_INIT = 0x101BF32C, 0x101C1C90
OWN = {FN_NEXT, FN_INIT}

ADDR = dict(loc=0x10395A34, thing=0x10395A3C, showthing=0x10395A54)
HOST_LAYOUT = [0xC]

ITER = 0x101F9268
STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def lab(e, a):
    return e.cl.lab(a & MASK)


def st_iter(e):
    L = e.cl
    t = s32(e.r(3))
    L.rec("Iter", (t, L.lab(e.r(4))))
    e.set_r(3, e.pool[L.ret("Iter:%d" % t) & 15])


def st_fill(e):
    L = e.cl
    L.rec("Fill", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.set_r(3, L.ret("Fill") & MASK)


def st_edit(e):
    L = e.cl
    L.rec("InitEditBox", (L.lab(e.r(3)), s32(e.r(4))))


def st_getwindow(e):
    L = e.cl
    i = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), i))
    e.set_r(3, e.pool[L.ret("GetWindow:%d" % i) & 15])


def st_show(e):
    e.cl.rec("Show", (e.cl.lab(e.r(3)), T.s16(e.r(4))))


def st_minmax(e):
    e.cl.rec("SetMinMax", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def st_setobj(e):
    e.cl.rec("SetClientDrawObject", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def st_minmaxvalues(e):
    e.cl.rec("SetMinMaxValues", (e.cl.lab(e.r(3)), e.s32(e.r(4)), e.s32(e.r(5))))


def st_setinc(e):
    e.cl.rec("SetInc", (e.cl.lab(e.r(3)), e.s32(e.r(4))))


def st_font(e):
    e.cl.rec("GetFontTableEntry", (e.r(3) & MASK,))
    e.set_r(3, e.pool[7])


def st_setfont(e):
    e.cl.rec("SetFont", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def st_just(e):
    e.cl.rec("SetFontJust", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def st_colour(e):
    e.cl.rec("SetFontColour", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))


def st_getstring(e):
    e.cl.rec("GetString", (s32(e.r(3)),))
    e.set_r(3, e.pool[8])


def st_text(e):
    e.cl.rec("SetClientDrawText", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def st_rect(e):
    e.cl.rec("CoolRect", ())


def st_rectd(e):
    e.cl.rec("~CoolRect", ())


STUBS.internal(ITER, st_iter)
STUBS.internal(0x10192A90, st_fill)
STUBS.internal(0x10193DCC, st_edit)
STUBS.internal(0x101DBE40, st_getwindow)
STUBS.internal(0x101DC1EC, st_show)
STUBS.internal(0x101D6B44, st_minmax)
STUBS.internal(0x101DB624, st_setobj)
STUBS.internal(0x101D8E44, st_minmaxvalues)
STUBS.internal(0x101D8EE8, st_setinc)
STUBS.internal(0x10180270, st_font)
STUBS.internal(0x101DA118, st_setfont)
STUBS.internal(0x101DA204, st_just)
STUBS.internal(0x101DA3AC, st_colour)
STUBS.internal(0x10180468, st_getstring)
STUBS.internal(0x101DE130, st_text)
STUBS.internal(0x101C7448, st_rect)
STUBS.internal(0x101C74E0, st_rectd)
STUBS.internal(0x1018DE60, CL.recorder("CashCtor", "Ni"))
STUBS.internal(0x1018D11C, CL.recorder("SpriteCtor", "Ni"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def make_world(self):
        self.pool = [0] + [self.alloc(0x40) for _ in range(8)]
        self.pool += [0] * 7
        self.panel = self.alloc(0x30)
        self.parent = self.alloc(0x40)
        self.words(self.panel + 0xC, [self.ptr32(self.parent)])

    def ptr32(self, a):
        return a


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make_world()
        e.pool = self.pool
        fixed = {self.pool[k]: k for k in range(1, 9)}
        fixed[self.panel] = 20
        fixed[self.parent] = 21
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def set_globals(self, case):
        self.words(ADDR["loc"], [case["loc"]])
        self.words(ADDR["thing"], [self.pool[case["thing"]]])
        self.words(ADDR["showthing"], [self.pool[case["showthing"]]])

    def thing(self):
        w = struct.unpack(">I", self.read(ADDR["thing"], 4))[0]
        return self.pool.index(w) if w in self.pool else -1

    def call_next(self):
        T.call_ppc(self.e, FN_NEXT, [])

    def call_init(self):
        T.call_ppc(self.e, FN_INIT, [self.panel])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_world()
        for k in range(16):
            lib.tpw_gns_pool(k, self.pool[k])
        fixed = {k: self.pool[k] for k in range(1, 9)}
        fixed[20] = self.panel
        fixed[21] = self.parent
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def ptr32(self, a):
        return self.lib.tpw_gns_offset(a)     # a Ptr32 field holds the offset from the arena base

    def alloc(self, n):
        return self.lib.tpw_gns_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_globals(self, case):
        self.write(self.lib.tpw_gns_addr(b"loc"), struct.pack("<i", case["loc"]))
        self.lib.tpw_gns_set(0, case["thing"])
        self.lib.tpw_gns_set(1, case["showthing"])

    def thing(self):
        return self.lib.tpw_gns_thing()

    def call_next(self):
        self.lib.tpw_gns_next()

    def call_init(self):
        self.lib.tpw_gns_init(self.panel)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gns_addr.restype, lib.tpw_gns_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gns_alloc.restype, lib.tpw_gns_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gns_offset.restype, lib.tpw_gns_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_gns_pool.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_gns_set.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_gns_thing.restype = C.c_int32
    lib.tpw_gns_init.argtypes = [C.c_uint64]
    lib.tpw_gns_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gns_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.set_globals(case)
        if case["op"] == "next":
            s.call_next()
        else:
            s.call_init()
        return {"log": s.log(), "thing": s.thing()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


ITYPES = [2, 4, 8, 0x10, 0x20, 0x40]
WIN_IDS = [0xA08C, 0xA08D, 0xA08E, 0xA090, 0xA08F, 0xA0A7, 0xA0A6, 0xA0A5]


def next_case(loc, cur, a, b):
    q = {}
    for t in ITYPES:
        q["Iter:%d" % t] = [a, b]
    return {"op": "next", "loc": loc, "thing": cur, "showthing": 0, "queues": q}


def next_edges():
    for loc in range(-1, 9):
        for cur in (0, 3):
            for a in (0, 1):
                for b in (0, 2):
                    yield next_case(loc, cur, a, b)


def next_random(r):
    return next_case(r.choice([r.randrange(-2, 10), r.randrange(0, 8)]), r.randrange(0, 5), r.choice([0, r.randrange(1, 6)]),
                     r.choice([0, r.randrange(1, 6)]))


def init_case(r, fill=None, present=None):
    fill = r.choice([0, 1, 2, 5]) if fill is None else fill
    q = {"Fill": [fill]}
    for i in WIN_IDS:
        here = r.random() < 0.7 if present is None else present
        k = r.randrange(1, 7) if here else 0
        if fill < 2 and i in (0xA08C, 0xA08D) and k == 0:
            k = r.randrange(1, 7)
        q["GetWindow:%d" % i] = [k]
    return {"op": "init", "loc": 0, "thing": 0, "showthing": r.randrange(0, 5), "queues": q}


def init_edges():
    r = random.Random(51001)
    for fill in (0, 1, 2, 3):
        for present in (True, False):
            yield init_case(r, fill, present)


def init_random(r):
    return init_case(r)


TESTS = [
    FnTest("GetNextStaffMember", ppc, host, edge=next_edges, rnd=next_random),
    FnTest("ShowStatusPanel::InitWindow", ppc, host, edge=init_edges, rnd=init_random),
]
