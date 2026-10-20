"""Unit v5905, the upgrade status panel's InitStatusWindow, UpdateStatusWindow, InitWindow and Show, against
libtpw_unit_UpgradeStatusPanel_InitStatusWindow_2.dylib (tests/unit_UpgradeStatusPanel_InitStatusWindow_2.cpp), main app:
  UpgradeStatusPanel::InitStatusWindow 0x101c5e00, UpdateStatusWindow 0x101c60d4, InitWindow 0x101c6208, Show 0x101c665c

A case is a panel (parent window at +0xc, status window at +0x10) with child windows present or not, built twice by one piece of Python.
Every callee is a fake on both sides that logs its arguments (windows by id, new objects by label, the Mac's TVector and script addresses
as the Mac's numbers). What is compared: the log and the thing global. No fused operations. Not tested: UpdateStatusWindow with no child
0x520e, InitWindow with no child 0x520a or 0x520b (the original reads through NULL).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UpgradeStatusPanel_InitStatusWindow_2"
MASK = 0xFFFFFFFF

FN_INIT, FN_UPD, FN_INITWIN, FN_SHOW = 0x101C5E00, 0x101C60D4, 0x101C6208, 0x101C665C
SIZES = {FN_INIT: 596, FN_UPD: 244, FN_INITWIN: 136, FN_SHOW: 76}
OWN = set(SIZES)

ADDR = dict(world=0x1037EBFC, staff=0x103947A4, upthing=0x10395B48)
HOST_LAYOUT = []

WINS = {}     # id -> (address, root) on the PowerPC side


def s32(v):
    return T.s32(v)


def winid(p):
    for i, (a, root) in WINS.items():
        if a == p:
            return i
    return -1


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def half(e, p):
    return struct.unpack(">H", e.read(p + 0xE, 2))[0]


def st_getwindow(e):
    ident = s32(e.r(4))
    e.cl.rec("GetWindow", (winid(e.r(3)), ident))
    r = 0
    if ident >= 0x5200 and ident in WINS and not WINS[ident][1]:
        r = WINS[ident][0]
    e.set_r(3, r)


def st_setdraw(e):
    e.cl.rec("SetClientDrawObject", (winid(e.r(3)), e.cl.lab(e.r(4))))


def st_setfont(e):
    e.cl.rec("SetFont", (winid(e.r(3)), e.r(4) & MASK))


def st_settext(e):
    e.cl.rec("SetClientDrawText", (winid(e.r(3)), e.r(4) & MASK))


def st_show(e):
    e.cl.rec("Show", (winid(e.r(3)), T.s16(e.r(4))))


def st_font(e):
    e.cl.rec("GetFontTableEntry", (e.r(3) & MASK,))
    e.set_r(3, e.cl.ret("font") & MASK)


def st_str(e):
    e.cl.rec("UIMain_GetString", (s32(e.r(3)),))
    e.set_r(3, e.cl.ret("str") & MASK)


def st_adv(e):
    e.cl.rec("UIMain_AdvisorMessage", (s32(e.r(3)),))


def st_dbinfo(e):
    e.cl.rec("GetDBInfo", (half(e, e.r(3)),))


def st_resid(e):
    e.cl.rec("GetCurrentResidualValue", (half(e, e.r(3)),))
    e.set_r(3, e.cl.ret("resid") & MASK)


def st_park(e):
    e.cl.rec("GetParkAnalyser", (e.r(3) & MASK,))
    e.set_r(3, e.cl.ret("park") & MASK)


def st_num(e):
    e.cl.rec("NumberOfObjectInPark", (e.r(3) & MASK, (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, e.cl.ret("num") & MASK)


def st_fill(e):
    e.cl.rec("FillObjectList", (e.r(4) & MASK,))
    e.set_r(3, e.cl.ret("fill") & MASK)


def st_edit(e):
    e.cl.rec("InitEditBox", (s32(e.r(4)),))


def st_open(e):
    e.cl.rec("OpenWindow", (e.r(4) & MASK, e.r(5) & MASK, s32(e.r(6)), s32(e.r(7)), s32(e.r(8))))


def st_new(e):
    size = e.r(3) & MASK
    e.cl.rec("new", (size,))
    e.set_r(3, e.alloc(size))


def st_setvalue(e):
    e.cl.rec("SetValue", (winid(e.r(3)), s32(e.r(4))))


STUBS.internal(0x101DBE40, st_getwindow)
STUBS.internal(0x101DB624, st_setdraw)
STUBS.internal(0x101DA118, st_setfont)
STUBS.internal(0x101DE130, st_settext)
STUBS.internal(0x101DC1EC, st_show)
STUBS.internal(0x10180270, st_font)
STUBS.internal(0x10180468, st_str)
STUBS.internal(0x101821CC, st_adv)
STUBS.internal(0x10107CD4, st_dbinfo)
STUBS.internal(0x1010FFC8, st_resid)
STUBS.internal(0x1013EBC8, st_park)
STUBS.internal(0x100EA52C, st_num)
STUBS.internal(0x10192A90, st_fill)
STUBS.internal(0x10193DCC, st_edit)
STUBS.internal(0x101935A0, st_open)
STUBS.internal(0x102384B8, st_new)
STUBS.internal(0x1023BAA8, st_setvalue)
STUBS.internal(0x1018D11C, CL.recorder("SpriteCtor", "N"))
STUBS.internal(0x1018DE60, CL.recorder("CashCtor", "N"))
STUBS.internal(0x1018D930, CL.recorder("NumberCtor", "N"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        WINS.clear()

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def ptr(self, a):
        return a

    def setwin(self, ident, a, root):
        WINS[ident] = (a, root)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, args):
        fn = {"init": FN_INIT, "update": FN_UPD, "initwin": FN_INITWIN, "show": FN_SHOW}[op]
        T.call_ppc(self.e, fn, [a & MASK for a in args])

    def wptr(self, a, v):
        self.words(a, [v])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_us_clear()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_us_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_us_alloc(n)

    def ptr(self, a):
        return self.lib.tpw_us_off(a)

    def setwin(self, ident, a, root):
        self.lib.tpw_us_setwin(ident, a, root)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        getattr(self.lib, "tpw_us_" + op)(*args)   # the panel is a full 64-bit host address

    def wptr(self, a, v):
        self.write(a, struct.pack("<Q", v))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_us_"
    lib.tpw_us_addr.restype, lib.tpw_us_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_us_alloc.restype, lib.tpw_us_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_us_off.restype, lib.tpw_us_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_us_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_us_clear.argtypes = []
    lib.tpw_us_setwin.argtypes = [C.c_int32, C.c_uint64, C.c_int32]
    for n in ("init", "update", "initwin"):
        getattr(lib, p + n).argtypes = [C.c_uint64]
    lib.tpw_us_show.argtypes = [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_us_layout(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.bound = True


def mkwin(s):
    w = s.alloc(0x140)
    vt = s.alloc(0x40)
    s.words(w + 0x118, [s.ptr(vt)])
    s.words(vt + 0x24, [0x1000])
    return w


def run(s, case):
    try:
        s.clear = None
        panel = s.alloc(0x20)
        parent, w10 = mkwin(s), mkwin(s)
        s.setwin(2, parent, 1)
        s.setwin(1, w10, 1)
        s.words(panel + 0xC, [s.ptr(parent), s.ptr(w10) if case["has10"] else 0])
        for ident in case["kids"]:
            s.setwin(ident, mkwin(s), 0)
        thing = s.alloc(0x40)
        s.write(thing + 0xE, struct.pack(s.fmt[0] + "H", case["hw"]))
        s.wptr(s.g("staff"), thing)
        s.words(s.g("world"), [case["world"]])
        s.words(s.g("upthing"), [case["upthing"]])
        s.call(case["op"], [panel] + ([case["arg"]] if case["op"] == "show" else []))
        return {"log": s.log(), "upthing": s.rd_word(s.g("upthing"))}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


ALLKIDS = [0x5211, 0x5210, 0x520F, 0x520E, 0x520A, 0x520B]


def mk(r, op, has10=None, kids=None, fill=None):
    q = {
        "font": [r.randrange(1, 0xFFFF) for _ in range(2)],
        "str": [r.randrange(1, 0xFFFF) for _ in range(2)],
        "resid": [r.choice([0, 1, 0x7FFFFFFF, 0x80000000, r.randrange(0, 100000)])],
        "park": [r.randrange(1, 0xFFFFFF)],
        "num": [r.choice([0, 1, -1, r.randrange(0, 1000)]) & MASK],
        "fill": [r.choice([-1, 0, 1, 2, 3, r.randrange(0, 20)]) & MASK if fill is None else fill & MASK],
    }
    return {"op": op, "queues": q, "has10": r.random() < 0.8 if has10 is None else has10,
            "kids": [k for k in ALLKIDS if r.random() < 0.7] if kids is None else kids,
            "hw": r.choice([0, 1, 0x8000, 0xFFFF, r.randrange(0, 0x10000)]), "world": r.randrange(1, 0xFFFFFF),
            "upthing": r.randrange(0, 0xFFFFFF), "arg": r.choice([0, r.randrange(0, 0xFFFFFFFF)])}


def init_case(r):
    return mk(r, "init", kids=[k for k in ALLKIDS if r.random() < 0.7])


def init_edges():
    r = random.Random(11)
    for has in (False, True):
        for mask in range(16):
            yield mk(r, "init", has, [k for i, k in enumerate(ALLKIDS[:4]) if mask >> i & 1])
        yield mk(r, "init", has, list(ALLKIDS))


def upd_case(r):
    c = mk(r, "update")
    c["kids"] = [0x520E] + [k for k in (0x5210,) if r.random() < 0.7]
    return c


def upd_edges():
    r = random.Random(12)
    for has in (False, True):
        for k in ([0x520E], [0x520E, 0x5210]):
            yield mk(r, "update", has, k)


def win_case(r):
    c = mk(r, "initwin")
    c["kids"] = [0x520A, 0x520B]
    return c


def win_edges():
    r = random.Random(13)
    for f in (-1, 0, 1, 2, 3):
        yield mk(r, "initwin", True, [0x520A, 0x520B], f)


def show_case(r):
    return mk(r, "show")


def show_edges():
    r = random.Random(14)
    for a in (0, 1, 0xFFFFFFFF):
        c = mk(r, "show")
        c["arg"] = a
        yield c


TESTS = [
    FnTest("UpgradeStatusPanel::InitStatusWindow", ppc, host, edge=init_edges, rnd=init_case),
    FnTest("UpgradeStatusPanel::UpdateStatusWindow", ppc, host, edge=upd_edges, rnd=upd_case),
    FnTest("UpgradeStatusPanel::InitWindow", ppc, host, edge=win_edges, rnd=win_case),
    FnTest("UpgradeStatusPanel::Show", ppc, host, edge=show_edges, rnd=show_case),
]
