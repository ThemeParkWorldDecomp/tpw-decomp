"""Unit v51121, the small functions of the ride and shop status panels, the staff costs and research panel removers, two static
initialisers and StoreTankMsg, against libtpw_unit_RideStatusPanel_UpgradePanelByeBye.dylib
(tests/unit_RideStatusPanel_UpgradePanelByeBye.cpp), main app ("SimTheme Park"), src/LCPostFormat_RideStatusPanel_UpdateStatusWindow_
RideStatusPanel_Upgrade_RideStatusPanel_UpgradePanelByeBye.cpp: the twenty functions of FUNCS.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit is a fake on both sides that records its arguments (a window by the tag word written at +0x3f0 of its block) and answers from the
case: the window setters, the sprite allocation, GetDBInfo, the preview calls, CloseWindow, EnableNameClick, UI_SendMessage, the assert,
slot 5 of the panel's table, CallDefaultWindowHandler, ShopStatusPanel::applytoallcallback.

The world of a case: the panel block (the view windows by their Mac offsets 0x14 and 0x18, the class's own fields by their offsets on each
side), the windows with their z, the ride record, gStaffRoomPanelThing, the thing array rows 0 and the id's, and the globals each
function writes. What is compared: the answer, the log, the globals and the panel's upgrade window pointer.

No fused operations. Not tested: nothing is left out.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RideStatusPanel_UpgradePanelByeBye"
MASK = 0xFFFFFFFF
P = "tpw_RideStatusPanel_UpgradePanelByeBye_"

# name, address, size, kind (the order is the C++ wrapper's switch)
FUNCS = [
    ("RideStatusPanel::UpgradePanelByeBye", 0x101ba930, 48, "panel"),
    ("RideStatusPanel::DeInitWindow", 0x101baa74, 4, "panel"),
    ("RideStatusPanel::DeInitStatusWindow", 0x101b9a84, 4, "panel"),
    ("RideStatusPanel::UpdateViewWindow", 0x101b9264, 164, "view"),
    ("RideStatusPanel::DeInitViewWindow", 0x101b9208, 36, "panel"),
    ("RideStatusPanel::InitViewWindow", 0x101b90e4, 236, "init"),
    ("RideStatusPanel::Remove", 0x101bc1a0, 36, "panel"),
    ("RideStatusPanel::BreakDown", 0x101bc1f4, 156, "break"),
    ("ShopStatusPanel::InitViewWindow", 0x101bc468, 236, "init"),
    ("ShopStatusPanel::DeInitViewWindow", 0x101bc58c, 36, "panel"),
    ("__sinit_ui_ridepanel_cpp", 0x101b8c8c, 16, "free"),
    ("ShopStatusPanel::UpdateViewWindow", 0x101bc5e8, 164, "view"),
    ("ShopStatusPanel::DeInitStatusWindow", 0x101bce60, 4, "panel"),
    ("ShopStatusPanel::DeInitWindow", 0x101bd6f8, 4, "panel"),
    ("_applytoall", 0x101bda5c, 48, "apply"),
    ("ShopStatusPanel::Remove", 0x101bdf64, 36, "panel"),
    ("RemoveStaffCostsPanel", 0x101bf278, 56, "free"),
    ("__sinit_ui_StaffCostsPanel_cpp", 0x101bf2dc, 28, "free"),
    ("RemoveResearchPanel", 0x101b5ae8, 56, "free"),
    ("StoreTankMsg", 0x101b495c, 60, "tank"),
]
OWN = {a for _, a, _, _ in FUNCS}

ADDR = dict(gStaffRoomPanelThing=0x103947a4, gThingArray=0x1034cbfc, gShopStatusPanel=0x103959e0, gStaffCostsPanel=0x10395a20,
            gResearchPanel=0x10395980, gResearchEffortBar=0x1039597c, gRideListF88=0x10395988, gStaffCostsF24=0x10395a24,
            gStaffCostsF28=0x10395a28)
TAG = 0x3f0
FAKE_THING = 0x5000
FAKE_SPRITE = 0x7100
SLOT5 = 0x6666

# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def tag(e, p):
    return e.u32(p + TAG) if p else 0


def mk(name, fmt, ret=None):
    """A recording fake: fmt per argument register: w window tag, i s32, h s16, p raw pointer word."""
    def fn(e):
        vals = []
        for k, c in enumerate(fmt):
            v = e.r(3 + k)
            if c == "w":
                vals.append(tag(e, v))
            elif c == "i":
                vals.append(T.s32(v))
            elif c == "h":
                vals.append(T.s16(v))
            else:
                vals.append(v & MASK)
        e.cl.rec(name, tuple(vals))
        if ret is not None:
            e.set_r(3, ret(e) & MASK)
    return fn


def rect_ctor(e):
    e.cur.rect = e.r(3)
    e.cl.rec("RectCtor", ())


def rect_dtor(e):
    e.cl.rec("RectDtor", (1 if e.r(3) == e.cur.rect else 0,))


def get_client_rect(e):
    e.cl.rec("GetClientRect", (tag(e, e.r(3)), 1 if e.r(4) == e.cur.rect else 0))
    e.set_r(3, 0)


def show_preview(e):
    e.cl.rec("UIMain_ShowRidePreview", (1 if e.r(3) == e.cur.rect else 0, e.r(4) & MASK, T.s32(e.r(5))))


def get_db_info(e):
    e.cl.rec("GetDBInfo", (e.r(3) & MASK,))
    e.set_r(3, e.cur.info)


def panel_call(name, n):
    def fn(e):
        e.cl.rec(name, ((1 if e.r(3) == e.cur.panel else 0),) + tuple(T.s32(e.r(4 + k)) for k in range(n)))
    return fn


def ptr_glue(e):
    e.cl.rec("Slot5", (1 if e.r(12) == SLOT5 else 0,))


def apply_cb(e):
    e.cl.rec("applytoallcallback", (1 if e.r(3) == ADDR["gShopStatusPanel"] else 0, e.r(4) & MASK))


STUBS.internal(0x101dd8bc, mk("setZ", "wh"))
STUBS.internal(0x101dc1ec, mk("Show", "wh", lambda e: 0))
STUBS.internal(0x101db624, mk("SetClientDrawObject", "wp"))
STUBS.internal(0x101da204, mk("SetFontJust", "wii"))
STUBS.internal(0x101da118, mk("SetFont", "wp"))
STUBS.internal(0x101da3ac, mk("SetFontColour", "wiiii"))
STUBS.internal(0x101de130, mk("SetClientDrawText", "wp"))
STUBS.internal(0x101dba4c, get_client_rect)
STUBS.internal(0x101de318, mk("CallDefaultWindowHandler", "wiii", lambda e: e.cl.ret("CallDefaultWindowHandler")))
STUBS.internal(0x10180270, mk("GetFontTableEntry", "p", lambda e: 0x1234))
STUBS.internal(0x102384b8, mk("OperatorNew", "p", lambda e: e.cl.ret("OperatorNew")))
STUBS.internal(0x1018d11c, mk("SpriteCtor", "p"))
STUBS.internal(0x101c7448, rect_ctor)
STUBS.internal(0x101c74e0, rect_dtor)
STUBS.internal(0x10107cd4, get_db_info)
STUBS.internal(0x10181400, show_preview)
STUBS.internal(0x101815d4, mk("UIMain_DestroyRidePreview", ""))
STUBS.internal(0x10193774, panel_call("CloseWindow", 0))
STUBS.internal(0x10194608, panel_call("EnableNameClick", 1))
STUBS.internal(0x101c9518, mk("UI_SendMessage", "wiii", lambda e: 0))
STUBS.internal(0x1000dde8, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(0x1023baa8, ptr_glue)
STUBS.internal(0x101bdab8, apply_cb)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def window(self, z, tg):
        w = self.alloc(0x400)
        self.half(w + self.zoff, z)
        self.words(w + TAG, [tg])
        return w


class Ppc(Side):
    fmt = ">I"
    zoff = 0xdc
    rideoff = 0x78c
    win_off, ops_off, upg_off, stride = 0x0c, 0x20, 0x24, 0x14

    def __init__(self, e, case):
        self.e = e
        e.cur = self
        e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        self.rect = 0

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.e.cl.log

    def p32(self, a):
        return a

    def ptr(self, a, v):
        self.words(a, [v])

    def rptr(self, a):
        return self.rd_words(a, 1)[0]

    def make_ops(self):
        t = self.alloc(0x40)
        self.words(t + 0x1c, [SLOT5])
        return t

    def begin(self):
        self.e.cur.panel = self.panel
        self.e.cur.info = self.info

    def call(self, which, a, b, c, d):
        if FUNCS[which][3] == "break":
            b = (b << 16) & MASK
        with_a = a & MASK
        r3, _, _ = T.call_ppc(self.e, FUNCS[which][1], [with_a, b & MASK, c & MASK, d & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        L = lib.layout
        self.win_off, self.ops_off, self.upg_off, self.stride, self.zoff = L[0], L[1], L[2], L[3], L[4]
        self.rideoff = L[5]
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return getattr(self.lib, P + "addr")(name.encode())

    def alloc(self, n):
        return getattr(self.lib, P + "alloc")(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def p32(self, a):
        return getattr(self.lib, P + "p32")(a)

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def make_ops(self):
        return getattr(self.lib, P + "makeops")()

    def begin(self):
        getattr(self.lib, P + "begin")(self.panel, self.info)

    def call(self, which, a, b, c, d):
        return getattr(self.lib, P + "call")(which, a, b, c, d) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, P + "addr").restype, getattr(lib, P + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, P + "alloc").restype, getattr(lib, P + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, P + "p32").restype, getattr(lib, P + "p32").argtypes = C.c_uint64, [C.c_uint64]
    getattr(lib, P + "makeops").restype, getattr(lib, P + "makeops").argtypes = C.c_uint64, []
    getattr(lib, P + "begin").restype, getattr(lib, P + "begin").argtypes = None, [C.c_uint64, C.c_uint64]
    getattr(lib, P + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, P + "call").restype = C.c_int32
    getattr(lib, P + "call").argtypes = [C.c_int32, C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, P + "layout")(out)
    lib.layout = list(out)[:n]
    L = lib.layout
    assert L[6] < TAG and L[7] > L[5] + 4 and L[7] <= 0x1000, L
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

GLOBAL_WORDS = ("gStaffCostsF24", "gStaffCostsF28")


def build(s, case):
    kind = case["kind"]
    s.panel = s.alloc(0x80)
    s.info = s.alloc(0x1000)
    s.words(s.info + s.rideoff, [case["ride_id"]])
    L = getattr(s, "lib", None)
    s.rect = 0
    # globals read or written by some function, always written so no case sees another's
    s.ptr(s.g("gStaffRoomPanelThing"), FAKE_THING if case["thing"] else 0)
    for name in GLOBAL_WORDS:
        s.words(s.g(name), [case[name]])
    s.half(s.g("gRideListF88"), case["f88"])
    wins = {}
    for name in ("gStaffCostsPanel", "gResearchPanel", "gResearchEffortBar"):
        wins[name] = s.window(5, case["tags"][name]) if case["have"][name] else 0
        s.ptr(s.g(name), wins[name])
    s.wins = wins
    stride = s.stride
    base = s.g("gThingArray")
    s.ptr(base, case["entry0"])
    s.ptr(base + case["idx"] * stride, case["entry"])
    if kind in ("view", "init"):
        w14 = s.window(case["z14"], 0x31) if case["w14"] else 0
        w18 = s.window(case["z18"], 0x32) if case["w18"] else 0
        s.words(s.panel + 0x14, [s.p32(w14), s.p32(w18)])
    elif kind == "break":
        s.ptr(s.panel + s.win_off, s.window(5, 0x33) if case["w0c"] else 0)
        s.ptr(s.panel + s.ops_off, s.make_ops())
        if s.ops_off != 0x20:
            pass
    elif kind == "panel":
        s.ptr(s.panel + s.upg_off, 0x4321)
    s.tankwin = s.window(5, 0x44) if kind == "tank" else 0
    s.begin()


def execute(s, case):
    kind = case["kind"]
    which = case["which"]
    if kind == "tank":
        s.tankwin_bar = None
        return s.call(which, s.tankwin, case["msg"], case["a"], case["b"])
    if kind == "apply":
        return s.call(which, FAKE_THING + case["a"] % 16, 0, 0, 0)
    if kind == "break":
        return s.call(which, s.panel, case["idx"], 0, 0) and None
    return s.call(which, s.panel, 0, 0, 0) and None


def observe(s, case, answer):
    kind = case["kind"]
    out = {"log": s.log()}
    if kind in ("tank", "apply"):
        out["answer"] = answer
    out["f88"] = s.rd_half(s.g("gRideListF88"))
    for name in GLOBAL_WORDS:
        out[name] = s.rd_words(s.g(name), 1)
    for name in ("gStaffCostsPanel", "gResearchPanel", "gResearchEffortBar"):
        v = s.rptr(s.g(name))
        out[name] = 0 if v == 0 else (1 if v == s.wins[name] else 2)
    if kind == "panel":
        out["upg"] = s.rptr(s.panel + s.upg_off)
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the cases

ZS = [0, 1, -1, 5, 0x7ff5, 0x7fff, -0x8000, 0x100]


def make(r, which, k=None):
    """k: a flag word for the edge cases (every combination of the branch flags); None: random."""
    if k is None:
        k = r.getrandbits(8)
    name, addr, size, kind = FUNCS[which]
    c = {"which": which, "kind": kind}
    c["w14"], c["w18"], c["thing"], c["w0c"] = bool(k & 1), bool(k & 2), bool(k & 4), bool(k & 8)
    c["z14"], c["z18"] = r.choice(ZS), r.choice(ZS)
    c["ride_id"] = r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)])
    c["idx"] = r.choice([0, 1, 2, 0xFFFF & r.getrandbits(4)])
    ent = [0, FAKE_THING, 0x6000]
    c["entry0"] = ent[(k >> 4) & 1]
    c["entry"] = ent[1 + ((k >> 5) & 1)] if (k >> 6) & 1 else ent[0 if (k >> 7) & 1 else 1]
    c["f88"] = r.choice([0, 1, 0xFFFF, r.getrandbits(16)])
    for name2 in GLOBAL_WORDS:
        c[name2] = r.choice([0, 0x10000, r.getrandbits(32)])
    c["have"] = {n: bool((k >> j) & 1) for j, n in enumerate(("gStaffCostsPanel", "gResearchPanel", "gResearchEffortBar"))}
    c["tags"] = {n: 0x50 + j for j, n in enumerate(("gStaffCostsPanel", "gResearchPanel", "gResearchEffortBar"))}
    c["msg"] = r.choice([0x14, 0x14, 0, 0x13, 0x15, -1, r.getrandbits(31)])
    c["a"], c["b"] = r.getrandbits(31) - (1 << 30), r.getrandbits(32) - (1 << 31)
    c["queues"] = {"OperatorNew": [FAKE_SPRITE if (k & 16) else 0], "CallDefaultWindowHandler": [r.getrandbits(31) - (1 << 30)]}
    return c


def edges_for(which):
    def gen():
        r = random.Random(100 + which)
        for k in range(256):
            yield make(r, which, k)
    return gen


def rnd_for(which):
    def gen(r):
        return make(r, which)
    return gen


TESTS = [FnTest(name, ppc, host, edge=edges_for(i), rnd=rnd_for(i)) for i, (name, _, _, _) in enumerate(FUNCS)]
