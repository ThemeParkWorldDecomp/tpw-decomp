"""Unit v5100, the online city info overlay and the login panel's message function, against libtpw_unit_UpdateOnlineCityInfoOverlay.dylib
(tests/unit_UpdateOnlineCityInfoOverlay.cpp), main app, src/UI_IslandLobby_UpdateOnlineCityInfoOverlay.cpp:
  UpdateOnlineCityInfoOverlay 0x101e1f64, CreateOnlineCityInfoOverlay 0x101e20f0, OnlineLoginPanelDlgMsg 0x101e24c8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. Every callee outside the unit (GetWindow, the window setters,
the virtual SetText through the pointer glue, FormatString, operator new, the constructors, the messages, DoLogin ...) is a fake on both
sides that records its arguments (objects as labels). New blocks come from a pool of labelled blocks.

The world of a case: the overlay global, the shown-city global, the online system global, the flag, two city records, the lobby with its
window. Compared: the answer, those globals (as labels), the whole event log. Not tested: a NULL gLobby or a missing window (the original
reads them unconditionally); the second word at 0x10 of the tag the Mac builds (the host tag has no such member).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UpdateOnlineCityInfoOverlay"
MASK = 0xFFFFFFFF

FN_UPDATE, FN_CREATE, FN_DLG = 0x101E1F64, 0x101E20F0, 0x101E24C8
SIZES = {FN_UPDATE: 328, FN_CREATE: 496, FN_DLG: 496}
OWN = set(SIZES)

GETWINDOW, PTRGLUE, FORMAT, NEW = 0x101DBE40, 0x1023BAA8, 0x10239730, 0x102384B8
BUILD, SETOBJ, FJUST, FWRAP = 0x101DEB78, 0x101DB624, 0x101DA204, 0x101DA2D8
SPRITE, FONTENTRY, SETFONT, FCOLOUR, SETTEXT = 0x1018D11C, 0x10180270, 0x101DA118, 0x101DA3AC, 0x101DE130
SENDMSG, POSTMSG, DISABLE, ENABLE, DECIDE = 0x101C9518, 0x101C9400, 0x10182364, 0x10182304, 0x1018116C
CAPTURE, REMOVE, SYSCTOR, DOLOGIN = 0x101DCCEC, 0x101E2BE4, 0x1016F7D0, 0x10171234

ADDR = dict(overlay=0x10395D04, shown=0x10395D00, flag=0x10395D2C, password=0x10395D30, name=0x10395D72, script=0x102B0288,
            cb=0x102684B0, notext=0x1024EE5E, buf=0x10380C48, sys=0x10380AB0, lobby=0x102E4888, text6=0x1037FF18 + 0x564)
# the host's offsets: CLBLobby.mWindow, CFSOnlineCity.mName, .mValue, sizeof
HOST_LAYOUT = [8, 0, 8, 168]

IDS = [0x16C11, 0x16C12, 0x4B, 0x49]
NAMES = ["A", "B", "C", "D", "OVL", "FONT", "P0", "P1", "P2", "P3", "LW", "PW", "TABLE", "NAME1", "NAME2", "CITY1", "CITY2", "OLDSYS", "LOBBY"]
WINDOWS = ["A", "B", "C", "D", "OVL", "LW", "PW"]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lab(e, v):
    return e.cl.lab(v)


def stub(addr, fn):
    STUBS.internal(addr, fn)


def rec(name, *vals):
    def f(e):
        e.cl.rec(name, tuple(v(e) for v in vals))
    return f


def r(n, signed=True):
    return (lambda e: s32(e.r(n))) if signed else (lambda e: e.r(n) & MASK)


def lb(n):
    return lambda e: e.cl.lab(e.r(n))


def with_ret(fn, getter):
    def f(e):
        fn(e)
        e.set_r(3, getter(e) & MASK)
    return f


def glued(fn):
    """A glue call: the caller reloads its TOC from 0x14(r1) afterwards, so keep r2 there (the real glue does)."""
    def f(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return f


def s_getwin(e):
    rec("GetWindow", lb(3), r(4))(e)
    e.set_r(3, e.ovside.byid.get(s32(e.r(4)), 0))


def s_new(e):
    e.cl.rec("new", (e.r(3) & MASK,))
    e.set_r(3, e.ovside.pool_next())


def s_format(e):
    tag = e.r(7)
    e.cl.rec("FormatString", (lab(e, e.r(3)), lab(e, e.r(4)), e.r(5) & MASK, e.r(6) & MASK, e.u32(tag), e.u32(tag + 8), e.s32(tag + 12)))


stub(GETWINDOW, s_getwin)
stub(PTRGLUE, glued(rec("SetText", lb(3), lb(4))))
stub(FORMAT, glued(s_format))
stub(NEW, glued(s_new))
stub(BUILD, with_ret(rec("Build", lb(3), lb(4), lb(5)), lambda e: e.ovside.objs["OVL"]))
stub(SETOBJ, rec("SetClientDrawObject", lb(3), lb(4)))
stub(FJUST, rec("SetFontJust", lb(3), r(4), r(5)))
stub(FWRAP, rec("SetFontWrap", lb(3), r(4), r(5)))
stub(SPRITE, rec("SpriteCtor", lb(3), r(4)))
stub(FONTENTRY, with_ret(rec("GetFontTableEntry", r(3, False)), lambda e: e.ovside.objs["FONT"]))
stub(SETFONT, rec("SetFont", lb(3), lb(4)))
stub(FCOLOUR, rec("SetFontColour", lb(3), r(4), r(5), r(6), r(7)))
stub(SETTEXT, rec("SetClientDrawText", lb(3), lb(4)))
stub(SENDMSG, rec("UI_SendMessage", lb(3), r(4), r(5), r(6)))
stub(POSTMSG, rec("UI_PostMessage", lb(3), r(4), r(5), r(6)))
stub(DISABLE, rec("UIMain_DisableAllKeyTables"))
stub(ENABLE, rec("UIMain_EnableAllKeyTables"))
stub(DECIDE, rec("UIMain_DecideKbCapture"))
stub(CAPTURE, rec("CaptureKb", lb(3), r(4)))
stub(REMOVE, rec("RemoveOnlineLoginPanel", r(3)))
stub(SYSCTOR, rec("CFSOnlineSystem", lb(3)))
stub(DOLOGIN, rec("DoLogin", lb(3), lb(4), lb(5)))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def wr_ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def make_world(self):
        """Allocates every object the cases use (labelled), the windows with their vtables, the lobby with its window."""
        self.objs = {}
        for n in NAMES:
            self.objs[n] = self.alloc(0x200)
        for n in WINDOWS:
            self.mkwin(self.objs[n])
        self.byid = {IDS[i]: self.objs[WINDOWS[i]] for i in range(4)}
        self.wr_ptr(self.objs["LOBBY"] + self.LOBBY_WIN, self.objs["LW"])
        self.pool = 0
        self.labels = {}
        for i, n in enumerate(NAMES):
            self.labels[self.objs[n]] = i + 1
        for j, n in enumerate(["script", "cb", "name", "password", "buf", "notext"]):
            self.labels[self.g(n)] = 30 + j

    def pool_next(self):
        a = self.objs["P%d" % (self.pool & 3)]
        self.pool += 1
        return a

    def ident(self, v):
        return self.labels.get(v, 0 if v == 0 else -1)


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    LOBBY_WIN, CITY_NAME, CITY_VAL = 4, 0x14, 0x40

    def __init__(self, e, case):
        self.e = e
        e.ovside = self
        self.make_world()
        self.L = e.cl = CL.PpcLog(self.labels, case["queues"])
        self.saved = STUBS.activate(e)

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
        return self.L.log

    def mkwin(self, w):
        vt = self.alloc(0x40)
        self.words(vt + 0x2C, [0x1234])
        self.wr_ptr(w + 0x118, vt)

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    LOBBY_WIN, CITY_NAME, CITY_VAL = 8, 0, 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_world()
        slots = ["A", "B", "C", "D", "OVL", "FONT", "P0", "P1", "P2", "P3"]
        for i, n in enumerate(slots):
            lib.tpw_ov_set(i, self.objs[n])
        CL.host_begin(lib, {lbl: a for a, lbl in self.labels.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ov_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ov_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def mkwin(self, w):
        self.wr_ptr(w, self.lib.tpw_ov_vtable())

    def call(self, fn, args):
        if fn == FN_UPDATE:
            self.lib.tpw_ov_update(args[0])
            return 0
        if fn == FN_CREATE:
            self.lib.tpw_ov_create()
            return 0
        return self.lib.tpw_ov_dlg(args[0], s32(args[1]), s32(args[2]), s32(args[3])) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ov_addr.restype, lib.tpw_ov_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ov_alloc.restype, lib.tpw_ov_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ov_vtable.restype, lib.tpw_ov_vtable.argtypes = C.c_uint64, []
    lib.tpw_ov_set.restype, lib.tpw_ov_set.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_ov_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ov_update.restype, lib.tpw_ov_update.argtypes = None, [C.c_uint64]
    lib.tpw_ov_create.restype, lib.tpw_ov_create.argtypes = None, []
    lib.tpw_ov_dlg.restype, lib.tpw_ov_dlg.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ov_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    o = s.objs
    val = {"none": 0, "ovl": o["OVL"], "city1": o["CITY1"], "city2": o["CITY2"], "sys": o["OLDSYS"], "other": 0x5555}
    s.wr_ptr(s.g("overlay"), val[case["ovl"]])
    s.wr_ptr(s.g("shown"), val[case["shown"]])
    s.wr_ptr(s.g("sys"), val[case["sys"]])
    s.words(s.g("flag"), [case["flag"]])
    s.wr_ptr(s.g("text6"), o["TABLE"])
    s.wr_ptr(s.g("lobby"), o["LOBBY"])
    for k, c in enumerate(("CITY1", "CITY2")):
        s.wr_ptr(o[c] + s.CITY_NAME, [o["NAME1"], o["NAME2"], 0][case["names"][k]])
        s.words(o[c] + s.CITY_VAL, [case["vals"][k]])
    s.pending = val


def execute(s, case):
    o = s.objs
    op = case["op"]
    if op == "update":
        s.call(FN_UPDATE, [{"none": 0, "city1": o["CITY1"], "city2": o["CITY2"]}[case["arg"]]])
        return None
    if op == "create":
        s.call(FN_CREATE, [])
        return None
    return s.call(FN_DLG, [o["PW"]] + case["args"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "overlay": s.ident(s.rd_ptr(s.g("overlay"))),
            "shown": s.ident(s.rd_ptr(s.g("shown"))), "sys": s.ident(s.rd_ptr(s.g("sys"))),
            "flag": s.rd_words(s.g("flag"), 1)}


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


# ---------------------------------------------------------------------------------------------------------------- cases

VALS = [0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x1CE, 1234567]


def base(r, op):
    return {"op": op, "queues": {}, "ovl": r.choice(["none", "ovl"]), "shown": r.choice(["none", "city1", "city2", "other"]),
            "sys": r.choice(["none", "sys"]), "flag": r.choice([0, 1, 0x1234567, -1]) & MASK,
            "names": [r.randrange(3), r.randrange(3)], "vals": [r.choice(VALS) & MASK, r.choice(VALS) & MASK], "arg": "none", "args": [0, 0, 0]}


def update_case(r, ovl=None, shown=None, arg=None):
    c = base(r, "update")
    if ovl is not None:
        c["ovl"] = ovl
    if shown is not None:
        c["shown"] = shown
    c["arg"] = r.choice(["none", "city1", "city2"]) if arg is None else arg
    return c


def update_edges():
    r = random.Random(5100)
    for ovl in ("none", "ovl"):
        for shown in ("none", "city1", "city2", "other"):
            for arg in ("none", "city1", "city2"):
                for _ in range(3):
                    yield update_case(r, ovl, shown, arg)


def update_random(r):
    return update_case(r)


def create_edges():
    r = random.Random(5101)
    for ovl in ("none", "ovl"):
        for shown in ("none", "city1", "other"):
            c = base(r, "create")
            c["ovl"], c["shown"] = ovl, shown
            yield c


def create_random(r):
    return base(r, "create")


MSGS = [0x100, 0x15, 0x14, 0x13, 0x16, 0xFF, 0x101, 0x800, 0x801, 0x802, 0x803, 0x804, 0x805, 0x806, 0, -1, 0x10, 0x7FFFFFFF, -0x80000000]
AS = [-1, -2, -3, 0, 1, 0x49, 0x4B, 0x48, 0x100, -0x7FFFFFFF]


def dlg_case(r, msg=None, a=None):
    c = base(r, "dlg")
    c["args"] = [r.choice(MSGS) if msg is None else msg, r.choice(AS) if a is None else a, r.choice([0, 1, -1, 0x49])]
    return c


def dlg_edges():
    r = random.Random(5102)
    for msg in MSGS:
        for a in AS:
            yield dlg_case(r, msg, a)


def dlg_random(r):
    return dlg_case(r)


TESTS = [
    FnTest("UpdateOnlineCityInfoOverlay", ppc, host, edge=update_edges, rnd=update_random),
    FnTest("CreateOnlineCityInfoOverlay", ppc, host, edge=create_edges, rnd=create_random),
    FnTest("OnlineLoginPanelDlgMsg", ppc, host, edge=dlg_edges, rnd=dlg_random),
]
