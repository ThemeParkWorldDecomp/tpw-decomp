"""Unit v5519, the status window of the bog (ride) status panel, against libtpw_unit_BogStatusPanel_UpdateStatusWindow.dylib
(tests/unit_BogStatusPanel_UpdateStatusWindow.cpp), main app ("SimTheme Park"),
src/BogStatusPanel_InitViewWindow_BogStatusPanel_UpdateStatusWindow.cpp:
  BogStatusPanel::InitStatusWindow 0x1019a12c, BogStatusPanel::UpdateStatusWindow 0x1019a504

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (GetWindow, the constructors, SetClientDrawObject, SetFont, SetClientDrawText, SetState, the ride's queries, UIMain_*, the
history sum, the double to integer runtime function and the virtual calls through __ptr_glue: Enable, SetValue and the panel's slot 0x2c)
are fakes on both sides that record their arguments and answer from the case's queues.

The world of a case: a ride object (gStaffRoomPanelThing, condition float at +0x40), a panel (state word at +4, the status window at +0x10
or none, the panel window at +0x0c, the table at +0x20), the child windows (the status window's number and bar windows may be missing).
What is compared: the call log, the panel's state word and the whole ride object.

No fused operations; fctiwz only on values well inside the int range. Not tested: a missing panel window or missing child windows other
than the two the original tests for (it reads their tables at once), a condition beyond the int range (Unicorn and the hardware differ).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BogStatusPanel_UpdateStatusWindow"
MASK = 0xFFFFFFFF

FN_INIT, FN_UPDATE = 0x1019A12C, 0x1019A504
SIZES = {FN_INIT: 864, FN_UPDATE: 648}
OWN = set(SIZES)

GLUE = 0x1023BAA8
NEW = 0x102384B8
FLOAT_TO_CELL = 0x10237CD8
GET_DB_INFO, GET_WINDOW, GET_SUM = 0x10107CD4, 0x101DBE40, 0x100F1000
RESIDUAL, IS_OPENABLE, IS_CLOSED = 0x1010FFC8, 0x1010AAAC, 0x1010CAE8
SET_STATE, GET_OBJECT_STATE = 0x101CB1D0, 0x10180B84
SPRITE_CTOR, NUMBER_CTOR, CASH_CTOR, GRADIENT_CTOR = 0x1018D11C, 0x1018D930, 0x1018DE60, 0x101898C8
SET_CLIENT_OBJECT, SET_FONT, SET_TEXT = 0x101DB624, 0x101DA118, 0x101DE130
GET_FONT, GET_STRING, INDENT = 0x10180270, 0x10180468, 0x10180940

ADDR = dict(thing=0x103947A4)
HOST_LAYOUT = [4, 0xC, 0x10, 0x20, 0x40, 0x1A4, 0x448]

IDS = [0x15BBE, 0x15BBC, 0x15BBF, 0x15BBD, 0x15BBB, 0x15BC0, 0x15BC4, 0x15BB4]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def save_toc(e):
    """The glue stores the caller's TOC pointer at 0x14(r1); the original reloads r2 from there after the call."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))


def stub_glue(e):
    save_toc(e)
    L = e.cl
    name = L.blocks.get(e.r(12))
    assert name is not None, "glue call through an unknown block %x" % e.r(12)
    if name == "PanelSlot2c":
        L.rec(name, (L.lab(e.r(3)),))
    else:
        L.rec(name, (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, 0)


def stub_new(e):
    save_toc(e)
    size = e.r(3)
    e.cl.rec("new", (size,))
    e.set_r(3, e.alloc(size))


def stub_float_to_cell(e):
    L = e.cl
    bits = T.f64_bits(e.f(1)) & 0xFFFFFFFFFFFFFFFF
    L.rec("FloatToCell", (bits - (1 << 64) if bits >= 1 << 63 else bits,))
    e.set_r(3, L.ret("cell") & MASK)


def stub_get_window(e):
    L = e.cl
    parent, ident = e.r(3), s32(e.r(4))
    L.rec("GetWindow", (L.lab(parent), ident))
    e.set_r(3, L.kids.get((parent, ident), 0))


def stub_get_sum(e):
    L = e.cl
    L.rec("GetSumOfLast", (e.r(3) - L.obj, s32(e.r(5))))
    e.write(e.r(4), struct.pack(">d", s32(L.ret("sum")) / 4.0))
    e.set_r(3, 0)


def stub_get_font(e):
    e.cl.rec("GetFontTableEntry", (e.r(3),))
    e.set_r(3, 0x7000 + e.r(3))


def stub_get_string(e):
    e.cl.rec("UIMain_GetString", (s32(e.r(3)),))
    e.set_r(3, 0x9000 + e.r(3))


STUBS.internal(GLUE, stub_glue)
STUBS.internal(NEW, stub_new)
STUBS.internal(FLOAT_TO_CELL, stub_float_to_cell)
STUBS.internal(GET_WINDOW, stub_get_window)
STUBS.internal(GET_SUM, stub_get_sum)
STUBS.internal(GET_FONT, stub_get_font)
STUBS.internal(GET_STRING, stub_get_string)
STUBS.internal(GET_DB_INFO, CL.recorder("GetDBInfo", "L"))
STUBS.internal(RESIDUAL, CL.recorder("GetCurrentResidualValue", "L", ret="i"))
STUBS.internal(IS_OPENABLE, CL.recorder("IsOpenable", "L", ret="i"))
STUBS.internal(IS_CLOSED, CL.recorder("IsClosed", "L", ret="i"))
STUBS.internal(SET_STATE, CL.recorder("SetState", "Lbb"))
STUBS.internal(GET_OBJECT_STATE, CL.recorder("UIMain_GetObjectState", "Li", ret="i"))
STUBS.internal(SPRITE_CTOR, CL.recorder("SpriteCtor", "N"))
STUBS.internal(NUMBER_CTOR, CL.recorder("NumberCtor", "N"))
STUBS.internal(CASH_CTOR, CL.recorder("CashCtor", "N"))
STUBS.internal(GRADIENT_CTOR, CL.recorder("HGradientCtor", "Ns"))
STUBS.internal(SET_CLIENT_OBJECT, CL.recorder("SetClientDrawObject", "LL"))
STUBS.internal(SET_FONT, CL.recorder("SetFont", "Lu"))
STUBS.internal(SET_TEXT, CL.recorder("SetClientDrawText", "Lu"))
STUBS.internal(INDENT, CL.recorder("UIMain_IndentClient", "Lhh"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def world(self, case):
        """Builds the objects (same code, both memories) and answers their addresses."""
        w = {}
        w["obj"] = self.alloc(0x448)
        w["panel"] = self.alloc(0x40)
        w["f10"] = self.mkwin() if case["f10"] else 0
        w["win"] = self.mkwin()
        w["kids"] = [self.mkwin() for _ in IDS]
        w["block_panel"] = self.mkblock("PanelSlot2c")
        w["table"] = self.alloc(0x40)
        self.words(w["table"] + 0x2C, [self.enc(w["block_panel"])])
        self.words(w["obj"] + 0x40, [case["f40"]])
        self.words(w["panel"] + 4, [case["mstate"]])
        self.words(w["panel"] + 0xC, [self.enc(w["win"])])
        self.words(w["panel"] + 0x10, [self.enc(w["f10"])])
        self.words(w["panel"] + 0x20, [self.enc(w["table"])])
        self.putptr(self.g("thing"), w["obj"])
        for parent in (w["f10"], w["win"]):
            if parent == 0:
                continue
            for k, ident in enumerate(IDS):
                if parent == w["f10"] and ((ident == 0x15BBD and case["no_bbd"]) or (ident == 0x15BBB and case["no_bbb"])):
                    continue
                self.link(parent, ident, w["kids"][k])
        return w

    def labels(self, w):
        d = {w["obj"]: 1, w["panel"]: 2, w["win"]: 4}
        if w["f10"]:
            d[w["f10"]] = 3
        for k, a in enumerate(w["kids"]):
            d[a] = 10 + k
        return d


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.kids = {}
        self.blocks = {}
        self.win_table = None
        self.w = self.world(case)
        self.L = e.cl = CL.PpcLog(self.labels(self.w), case["queues"])
        self.L.kids = self.kids
        self.L.blocks = self.blocks
        self.L.obj = self.w["obj"]
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def enc(self, a):
        return a

    def putptr(self, g, a):
        self.words(g, [a])

    def mkblock(self, name):
        a = self.e.alloc(16)
        self.blocks[a] = name
        return a

    def mkwin(self):
        if self.win_table is None:
            self.win_table = self.e.alloc(0x40)
            self.words(self.win_table + 0x1C, [self.mkblock("Enable")])
            self.words(self.win_table + 0x24, [self.mkblock("SetValue")])
        a = self.e.alloc(0x200)
        self.words(a + 0x118, [self.win_table])
        return a

    def link(self, parent, ident, child):
        self.kids[(parent, ident)] = child

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, entry):
        T.call_ppc(self.e, entry, [self.w["panel"] & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_bsp_unlink_all()
        self.w = self.world(case)
        fixed = {label: a for a, label in self.labels(self.w).items()}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bsp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_bsp_alloc(n)

    def enc(self, a):
        return self.lib.tpw_bsp_off(a)

    def putptr(self, g, a):
        self.write(g, struct.pack("<Q", a))

    def mkblock(self, name):
        a = self.alloc(16)
        self.write(a, struct.pack("<Q", self.g("panelslot")))
        return a

    def mkwin(self):
        return self.lib.tpw_bsp_mkwin(self.alloc(0x400))

    def link(self, parent, ident, child):
        self.lib.tpw_bsp_link(parent, ident, child)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry):
        (self.lib.tpw_bsp_init if entry == FN_INIT else self.lib.tpw_bsp_update)(self.w["panel"])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_bsp_"
    lib.tpw_bsp_addr.restype, lib.tpw_bsp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bsp_alloc.restype, lib.tpw_bsp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bsp_off.restype, lib.tpw_bsp_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_bsp_mkwin.restype, lib.tpw_bsp_mkwin.argtypes = C.c_uint64, [C.c_uint64]
    lib.tpw_bsp_link.restype, lib.tpw_bsp_link.argtypes = None, [C.c_uint64, C.c_int32, C.c_uint64]
    lib.tpw_bsp_unlink_all.restype, lib.tpw_bsp_unlink_all.argtypes = None, []
    lib.tpw_bsp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bsp_init.restype, lib.tpw_bsp_init.argtypes = None, [C.c_uint64]
    lib.tpw_bsp_update.restype, lib.tpw_bsp_update.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_bsp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def run(s, case, entry):
    try:
        s.call(entry)
        w = s.w
        return {"log": s.log(), "mstate": s.rd_word(w["panel"] + 4), "obj": s.rd_words(w["obj"], 0x448 // 4)}
    finally:
        s.done()


def ppc_update(e, case):
    return run(Ppc(e, case), case, FN_UPDATE)


def host_update(lib, case):
    return run(Host(lib, case), case, FN_UPDATE)


def ppc_init(e, case):
    return run(Ppc(e, case), case, FN_INIT)


def host_init(lib, case):
    return run(Host(lib, case), case, FN_INIT)


# ---------------------------------------------------------------------------------------------------------------- case generators

F40S = [0.0, 1.0, 25.0, 50.0, 99.9, 100.0, 100.5, 255.0, 255.9, 256.0, 257.0, 300.0, -1.0, -0.5, -255.0, -256.0, 12345.0, -12345.0, 0.4]


def rf40(r):
    if r.random() < 0.3:
        return fb(r.choice(F40S))
    return fb(r.uniform(-400, 400))


def base(r, f10=None):
    state = r.choice([0, 1, 5, 10, 11, 12, 13, 14, 15, 16, 17, -1, r.randrange(0, 30)])
    mstate = state if r.random() < 0.3 else r.choice([0, 1, 11, 99, r.randrange(0, 30)])
    return {
        "f10": r.random() < 0.8 if f10 is None else f10,
        "no_bbd": r.random() < 0.25,
        "no_bbb": r.random() < 0.25,
        "f40": rf40(r),
        "mstate": mstate,
        "queues": {
            "sum": [r.choice([0, 1, -1, 7, -9, 1000, r.randrange(-100000, 100000)])],
            "cell": [r.choice([0, 1, -1, 5, r.randrange(-1000, 1000)])],
            "GetCurrentResidualValue": [r.choice([0, 1, 12345, 0x7FFFFFFF, r.randrange(0, 1 << 31)])],
            "IsOpenable": [r.choice([0, 1, 2])],
            "IsClosed": [r.choice([0, 1])],
            "UIMain_GetObjectState": [state],
        },
    }


def update_edges():
    r = random.Random(5519)
    for f10 in (True, False):
        for no_bbd in (False, True):
            for no_bbb in (False, True):
                c = base(r, f10)
                c["no_bbd"], c["no_bbb"] = no_bbd, no_bbb
                yield c
    for openable in (0, 1):
        for closed in (0, 1):
            c = base(r, True)
            c["no_bbd"] = c["no_bbb"] = False
            c["queues"]["IsOpenable"], c["queues"]["IsClosed"] = [openable], [closed]
            yield c
    for state in (-1, 10, 11, 12, 13, 14, 15, 16):
        for same in (True, False):
            c = base(r, True)
            c["queues"]["UIMain_GetObjectState"] = [state]
            c["mstate"] = state if same else state + 1
            yield c
    for f in F40S:
        c = base(r, True)
        c["no_bbd"] = c["no_bbb"] = False
        c["f40"] = fb(f)
        yield c


def update_random(r):
    return base(r)


def init_case(r, f10=None):
    c = base(r, f10)
    c["no_bbd"] = c["no_bbb"] = False       # the original reads these windows' tables at once
    return c


def init_edges():
    r = random.Random(1919)
    for f10 in (True, False):
        yield init_case(r, f10)


def init_random(r):
    return init_case(r)


TESTS = [
    FnTest("BogStatusPanel::UpdateStatusWindow", ppc_update, host_update, edge=update_edges, rnd=update_random),
    FnTest("BogStatusPanel::InitStatusWindow", ppc_init, host_init, edge=init_edges, rnd=init_random),
]
