"""Unit v5995: the main panel's date display, the last-panel memory, the loans panel refresh and the slide-out panel's message function,
against libtpw_unit_UI_MainPanelDateChange_2.dylib (tests/unit_UI_MainPanelDateChange_2.cpp), main app ("SimTheme Park"),
src/ui_mainpanel2_msg_UI_MainPanelDateChange_2.cpp:
  UI_MainPanelDateChange 0x101a6234, SetLastPanel 0x101a57fc, UI_LoansPanelDateChange 0x101a577c, RailsCallback 0x101a6f2c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit (the window methods, the rectangle constructor, destructor and GetDims, FormatString, the text converter, the park analyser, the
history, the sound, message and panel calls, the virtual SetValue) is a fake on both sides that records its arguments and answers from the case.

The world of a case: twelve window blocks (labels 1 to 12; label 1 is the panel RailsCallback runs on, 2 the window it is given), the
globals (main panel, game window, slide-out tab, loans panel and mode, the three last-panel words, the sound bank, the text table word, the
world word), the panel's slide counter (+0x130), its state byte (+0x134) and the arriving window's depth (+0xdc). What is compared: the answer,
the event log (every argument of every callee, the rectangles the code read back), the three last-panel words and the panel's counter and
state byte.

No fused operations. Not tested: a NULL slide-out tab or main panel in the paths that read them (the original reads them unconditionally),
an arriving window that is not a window (the original reads its depth).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UI_MainPanelDateChange_2"
MASK = 0xFFFFFFFF

FN_DATE, FN_LAST, FN_LOANS, FN_RAILS = 0x101A6234, 0x101A57FC, 0x101A577C, 0x101A6F2C
SIZES = {FN_DATE: 416, FN_LAST: 72, FN_LOANS: 80, FN_RAILS: 1932}
OWN = set(SIZES)

ADDR = dict(gMainPanel=0x103949D8, gGameWindow=0x103949E4, gSlideoutTab=0x103949C8, gLoansPanel=0x10394980, gLenderListMode=0x1039497C,
            gLastPanelLists=0x103949C0, gLastPanelPark=0x103949BC, gLastPanelFinance=0x103949B8, bank3=0x102F7C94 + 0xC,
            table=0x1037FF18 + 0x564, gGameWorld=0x1037EBFC)
HOST_LAYOUT = [0xDC, 0x130, 0x134, 1]
NWIN = 12
WIN_SIZE = 0x200
WORLD = 0x50000000
ANALYSER = 0x30000000
TABLE = 0x4000


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def win_of(e, label):
    wins = e.cl.win
    return wins[label] if 0 <= label < len(wins) else 0


def rect_at(e, a):
    return struct.unpack(">4h", e.read(a + 4, 8))


def fill_rect(e, a):
    vals = [s16(e.cl.ret("pos")) for _ in range(4)]
    e.write(a + 4, struct.pack(">4h", *vals))


def stub(addr):
    def deco(fn):
        def wrapped(e):
            # a cross-module call is followed by "lwz r2,20(r1)": the glue saves the table of contents there
            e.write((e.r(1) + 20) & MASK, struct.pack(">I", e.r(2) & MASK))
            return fn(e)
        STUBS.internal(addr, wrapped)
        return fn
    return deco


@stub(0x101DBE40)
def st_get_window(e):
    L = e.cl
    ident = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), ident))
    e.set_r(3, win_of(e, L.ret("GetWindow:%d" % ident)))


@stub(0x101DE130)
def st_set_client_text(e):
    L = e.cl
    t = e.r(4)
    first = -1 if t == 0 else struct.unpack(">H", e.read(t, 2))[0]
    L.rec("SetClientDrawText", (L.lab(e.r(3)), first))


@stub(0x101DB7DC)
def st_set_frame(e):
    e.cl.rec("SetFrame", (e.cl.lab(e.r(3)), s32(e.r(4))))


@stub(0x101DCFD0)
def st_set_extra(e):
    L = e.cl
    idx = s16(e.r(4))
    value = e.r(5)
    L.rec("setExtraParam", (L.lab(e.r(3)), idx, L.lab(value) if idx == 1 else value & MASK))
    e.set_r(3, 0)


@stub(0x101DD030)
def st_get_extra(e):
    L = e.cl
    idx = s16(e.r(4))
    L.rec("getExtraParam", (L.lab(e.r(3)), idx))
    v = L.ret("getExtraParam:%d" % idx)
    e.set_r(3, win_of(e, v) if idx == 1 else v & MASK)


@stub(0x101DD5E0)
def st_set_anim(e):
    L = e.cl
    L.rec("setAnimTimer", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, L.ret("setAnimTimer") & MASK)


@stub(0x101DDF20)
def st_link(e):
    L = e.cl
    L.rec("link", (L.lab(e.r(3)), L.lab(e.r(4)), T.s8(e.r(5)), T.s8(e.r(6))))
    e.set_r(3, L.ret("link") & MASK)


@stub(0x101DD8BC)
def st_set_z(e):
    e.cl.rec("setZ", (e.cl.lab(e.r(3)), s16(e.r(4))))


@stub(0x101DB884)
def st_get_pos(e):
    L = e.cl
    L.rec("GetPosition", (L.lab(e.r(3)), s16(e.r(5))))
    fill_rect(e, e.r(4))
    e.set_r(3, 0)


@stub(0x101DB360)
def st_set_pos4(e):
    L = e.cl
    L.rec("SetPosition4", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))
    e.set_r(3, 0)


@stub(0x101DAE98)
def st_set_pos_rect(e):
    L = e.cl
    L.rec("SetPositionR", (L.lab(e.r(3)),) + rect_at(e, e.r(4)))
    e.set_r(3, 0)


@stub(0x101DC1EC)
def st_show(e):
    e.cl.rec("Show", (e.cl.lab(e.r(3)), s16(e.r(4))))
    e.set_r(3, 0)


@stub(0x101DD6B8)
def st_start_anim(e):
    e.cl.rec("startAnimTimer", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, 0)


@stub(0x101DD754)
def st_stop_anim(e):
    e.cl.rec("stopAnimTimer", (e.cl.lab(e.r(3)), s32(e.r(4))))


@stub(0x101DB9D0)
def st_get_orig(e):
    e.cl.rec("GetOriginalPosition", (e.cl.lab(e.r(3)),))
    fill_rect(e, e.r(4))
    e.set_r(3, 0)


@stub(0x101DB954)
def st_get_prev(e):
    e.cl.rec("GetPreviousPosition", (e.cl.lab(e.r(3)),))
    fill_rect(e, e.r(4))
    e.set_r(3, 0)


@stub(0x101DE084)
def st_unlink(e):
    e.cl.rec("unlink", (e.cl.lab(e.r(3)),))


@stub(0x101DBAFC)
def st_get_parent(e):
    L = e.cl
    L.rec("GetParent", (L.lab(e.r(3)),))
    e.set_r(3, win_of(e, L.ret("GetParent")))


@stub(0x101DA9D0)
def st_shift(e):
    e.cl.rec("ShiftPosition", (e.cl.lab(e.r(3)), s16(e.r(4)), s16(e.r(5))))
    e.set_r(3, 0)


@stub(0x101DE318)
def st_default(e):
    L = e.cl
    a = s32(e.r(5))
    if (a & MASK) in L.win[1:]:
        a = -1000 - L.win.index(a & MASK)
    L.rec("CallDefaultWindowHandler", (L.lab(e.r(3)), s32(e.r(4)), a, s32(e.r(6))))
    e.set_r(3, L.ret("default") & MASK)


@stub(0x101C7618)
def st_get_dims(e):
    L = e.cl
    L.rec("GetDims", rect_at(e, e.r(3)))
    e.write(e.r(4), struct.pack(">h", s16(L.ret("dims"))))
    e.write(e.r(5), struct.pack(">h", s16(L.ret("dims"))))


@stub(0x101C7448)
def st_rect_ctor(e):
    e.cl.rec("RectCtor", ())
    e.write(e.r(3), bytes(16))


@stub(0x101C74E0)
def st_rect_dtor(e):
    e.cl.rec("RectDtor", (s32(e.r(4)),))


@stub(0x101A6C44)
def st_hide(e):
    e.cl.rec("HideIfUnder", (e.cl.lab(e.r(3)), s16(e.r(4))))


@stub(0x100DD9FC)
def st_sound(e):
    e.cl.rec("SOUND_SendEvent", tuple(s32(e.r(i)) for i in range(3, 9)))
    e.set_r(3, 0)


@stub(0x101C9518)
def st_send(e):
    L = e.cl
    L.rec("UI_SendMessage", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


@stub(0x101C9400)
def st_post(e):
    L = e.cl
    L.rec("UI_PostMessage", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


@stub(0x101802CC)
def st_ui_sound(e):
    e.cl.rec("UIMain_UISound", (s32(e.r(3)),))


@stub(0x101A83D8)
def st_remove(e):
    e.cl.rec("RemoveSlideoutPanel", (s16(e.r(3)),))


@stub(0x101A4A38)
def st_fill_lender(e):
    e.cl.rec("FillLenderList", (e.cl.lab(e.r(3)),))


@stub(0x1023BAA8)
def st_ptr_glue(e):
    e.cl.rec("SetValue", (e.cl.lab(e.r(3)), s32(e.r(4))))


@stub(0x10180334)
def st_convert(e):
    L = e.cl
    c0, c1 = struct.unpack(">2H", e.read(e.r(3), 4))
    L.rec("ConvertMulti", (c0, c1))
    e.write(L.conv, struct.pack(">2H", L.ret("conv") & 0xFFFF, 0))
    e.set_r(3, L.conv)


@stub(0x10239F70)
def st_format(e):
    L = e.cl
    vals = [e.r(3) & MASK, s32(e.r(5)) & MASK, e.r(6) & MASK]
    for reg in (7, 8, 9):
        magic, _vptr, ident, value, flag = struct.unpack(">5I", e.read(e.r(reg), 20))
        vals += [s32(ident), s32(value), s32(flag ^ (0x100 if magic != 0x5654414D else 0))]
    L.rec("FormatString", tuple(vals))
    e.write(e.r(4), struct.pack(">2H", L.ret("fmt") & 0xFFFF, L.ret("fmt") & 0xFFFF))
    e.set_r(3, 0)


@stub(0x1013EBC8)
def st_analyser(e):
    e.cl.rec("GetParkAnalyser", (e.r(3) & MASK,))
    e.set_r(3, ANALYSER)


@stub(0x1001C2A8)
def st_history(e):
    L = e.cl
    L.rec("GetValue", (e.r(3) & MASK, s32(e.r(5))))
    e.write(e.r(4), struct.pack(">I", L.ret("hist") & MASK))
    e.set_r(3, 1)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    ptr_fmt = ">I"

    def ptr(self, a, v):
        self.write(a, struct.pack(self.ptr_fmt, v))

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "h", v))


class Ppc(Side):
    fmt = ">I"
    off = HOST_LAYOUT[:3]

    def __init__(self, e, case):
        self.e = e
        self.win = [0] + [e.alloc(WIN_SIZE) for _ in range(NWIN)]
        vt = e.alloc(0x40)
        for a in self.win[1:]:
            e.write(a + 0x118, struct.pack(">I", vt))
        e.write(0x102A460C, struct.pack(">I", ADDR["gGameWorld"]))
        self.L = e.cl = CL.PpcLog({a: i for i, a in enumerate(self.win) if i}, case["queues"])
        self.L.win = self.win
        self.L.conv = e.alloc(8)
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, args):
        entry = {"date": FN_DATE, "last": FN_LAST, "loans": FN_LOANS, "rails": FN_RAILS}[op]
        try:
            r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        except Exception:
            print("regs", [hex(self.e.r(i)) for i in range(32)])
            print("log", self.L.log)
            print("code", " ".join("%08x" % w for w in struct.unpack(">24I", self.e.read(0x101A62E0, 96))))
            print("pre", " ".join("%08x" % w for w in struct.unpack(">24I", self.e.read(0x101A6234, 96))))
            raise
        return r3 & MASK if op == "rails" else 0


class Host(Side):
    fmt = "<I"
    ptr_fmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = lib.layout[:3]
        self.win = [0] + [lib.tpw_v5995_alloc(WIN_SIZE) for _ in range(NWIN)]
        for i, a in enumerate(self.win):
            lib.tpw_v5995_setwin(i, a)
        CL.host_begin(lib, {i: a for i, a in enumerate(self.win) if i}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v5995_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        lib = self.lib
        if op == "date":
            lib.tpw_v5995_date(*[s32(a) for a in args])
            return 0
        if op == "last":
            lib.tpw_v5995_last(*[s32(a) for a in args])
            return 0
        if op == "loans":
            lib.tpw_v5995_loans(s32(args[0]))
            return 0
        return lib.tpw_v5995_rails(args[0],s32(args[1]), s32(args[2]), s32(args[3])) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_v5995_"
    lib.tpw_v5995_addr.restype, lib.tpw_v5995_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_v5995_alloc.restype, lib.tpw_v5995_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_v5995_setwin.restype, lib.tpw_v5995_setwin.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_v5995_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_v5995_date.restype, lib.tpw_v5995_date.argtypes = None, [C.c_int32] * 3
    lib.tpw_v5995_last.restype, lib.tpw_v5995_last.argtypes = None, [C.c_int32] * 2
    lib.tpw_v5995_loans.restype, lib.tpw_v5995_loans.argtypes = None, [C.c_int32]
    lib.tpw_v5995_rails.restype, lib.tpw_v5995_rails.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_v5995_layout(out)
    lib.layout = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    g = case["g"]
    for name in ("gMainPanel", "gGameWindow", "gSlideoutTab", "gLoansPanel"):
        s.ptr(s.g(name), s.win[g[name]])
    for name in ("gLenderListMode", "gLastPanelLists", "gLastPanelPark", "gLastPanelFinance", "bank3"):
        s.words(s.g(name), [g[name]])
    s.ptr(s.g("table"), TABLE)
    s.ptr(s.g("gGameWorld"), WORLD)
    w = case["w"]
    s.words(s.win[1] + s.off[1], [w["f130"]])
    s.write(s.win[1] + s.off[2], bytes([w["f134"] & 0xFF]))
    s.half(s.win[2] + s.off[0], w["z"])


def execute(s, case):
    args = list(case["args"])
    if case["op"] == "rails":
        args[2] = s.win[case["a_win"]] if case["a_win"] else args[2]
        args = [s.win[1]] + args[1:]
    return s.call(case["op"], args)


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(),
            "last": s.rd_words(s.g("gLastPanelLists"), 1) + s.rd_words(s.g("gLastPanelPark"), 1) + s.rd_words(s.g("gLastPanelFinance"), 1),
            "f130": s.rd_word(s.win[1] + s.off[1]), "f134": s.read(s.win[1] + s.off[2], 1)}


def run(s, case):
    try:
        build(s, case)
        o = observe(s, case, execute(s, case))
        print(type(s).__name__, o["log"])
        return o
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

EDGE_INTS = [0, 1, -1, 2, 3, 4, 0x24, 100, 0x7FFF, 0x8000, -0x8000, -0x8001, 0x10000, 0x12345, 0x7FFFFFFF, -0x80000000, 0x80080]


def rv(r):
    k = r.random()
    if k < 0.4:
        return r.choice(EDGE_INTS)
    if k < 0.8:
        return r.randrange(-400, 400)
    return r.randrange(-0x80000000, 0x80000000)


def rs(r):
    k = r.random()
    if k < 0.2:
        return r.choice([0, 1, -1, 0x7FFF, -0x8000, 100, -100])
    return r.randrange(-300, 300)


def lbl(r):
    return r.randrange(0, NWIN + 1)


def base(r, op):
    f134 = r.choice([0, 0, 1, 2, 3, 4, 3, 4, 0x80, 0x83, 0xFF, 5])
    q = {}
    for ident in (0x20, 0x2F, 0x31, 0x24, 0x12C4BD):
        q["GetWindow:%d" % ident] = [lbl(r)]
    q["getExtraParam:0"] = [rv(r)]
    q["getExtraParam:1"] = [r.choice([0, lbl(r), lbl(r)])]
    q["getExtraParam:2"] = [r.choice([0, 0, rv(r)])]
    q["setAnimTimer"] = [r.choice([0, 1, 1, 0x10000, 0x10001, -1, 5])]
    q["link"] = [r.choice([0, 1, 1, 2, 0x10000])]
    q["GetParent"] = [lbl(r)]
    q["default"] = [rv(r)]
    q["pos"] = [rs(r) for _ in range(4 * 4)]
    q["dims"] = [rs(r), rs(r)]
    q["conv"] = [r.randrange(0, 0x10000)]
    q["fmt"] = [r.randrange(0, 0x10000), r.randrange(0, 0x10000)]
    q["hist"] = [rv(r), rv(r)]
    return {"op": op, "queues": q, "a_win": 0,
            "g": {"gMainPanel": r.choice([3, 3, 3, 0]), "gGameWindow": r.choice([4, 4, 4, 0]), "gSlideoutTab": 5,
                  "gLoansPanel": r.choice([6, 6, 0]), "gLenderListMode": r.choice([0, 1, 1, 2, -1]),
                  "gLastPanelLists": rv(r), "gLastPanelPark": rv(r), "gLastPanelFinance": rv(r), "bank3": rv(r)},
            "w": {"f130": r.choice([0, 1, 2, 10, 0xFFFFFFFF, 0x80000000, rv(r)]), "f134": f134, "z": rs(r)}}


def date_case(r, args=None, **kw):
    c = base(r, "date")
    c["args"] = args if args is not None else [rv(r), rv(r), rv(r)]
    # the original dereferences the windows it asks for: never NULL
    c["g"]["gGameWindow"] = 4
    for k in ("GetWindow:32", "GetWindow:47", "GetWindow:49"):
        c["queues"][k] = [r.randrange(1, NWIN + 1)]
    apply(c, kw)
    return c


def last_case(r, t=None, p=None):
    c = base(r, "last")
    c["args"] = [rv(r) if t is None else t, rv(r) if p is None else p]
    return c


def loans_case(r, panel=None, mode=None):
    c = base(r, "loans")
    c["args"] = [rv(r)]
    if panel is not None:
        c["g"]["gLoansPanel"] = panel
    if mode is not None:
        c["g"]["gLenderListMode"] = mode
    return c


def apply(c, kw):
    for k, v in kw.items():
        if k in ("f130", "f134", "z"):
            c["w"][k] = v
        elif k in c["g"]:
            c["g"][k] = v
        else:
            c["queues"][k] = v


def rails_case(r, msg=None, a=None, b=None, **kw):
    c = base(r, "rails")
    msg = r.choice([0x12, 0xB, 0xA, 0x10, 0x10, 0x100, 0x15, 0x9, 0x11, 0x13, 0xC, 0, -1, 0x101]) if msg is None else msg
    if a is None:
        a = {0x10: r.choice([0x80080, 0x80080, 0x80080, 0x80081, 0x7FF80, rv(r)]), 0x100: r.choice([0x24, 0x24, 0x25, rv(r)]),
             0xB: r.choice([-1, -1, 0, 1, rv(r)])}.get(msg, rv(r))
    c["args"] = [0, msg, a, rv(r) if b is None else b]
    apply(c, kw)
    if msg == 0xA:
        # the arriving window: needed when the panel is idle and the link works (the original reads its depth)
        if c["w"]["f134"] & 0xFF == 0:
            if a == 0 or kw.get("noarrival"):
                c["queues"]["link"] = [0]
                c["queues"].pop("noarrival", None)
                c["args"][2] = 0
            else:
                c["a_win"] = 2
    c["queues"].pop("noarrival", None)
    return c


def date_edges():
    r = random.Random(995)
    yield date_case(r, [1, 2, 3], gMainPanel=0)
    yield date_case(r, [0, 0, 0])
    yield date_case(r, [31, 12, 99])
    yield date_case(r, [-1, -1, -1])
    yield date_case(r, [0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF])
    yield date_case(r, [5, 6, 0x7FFFFFFF - 0x76C + 1])
    for hist in ([5, 5], [5, 4], [5, 6], [-1, 0], [0, -1], [0x7FFFFFFF, -0x80000000], [-0x80000000, 0x7FFFFFFF]):
        yield date_case(r, hist=hist)


def last_edges():
    r = random.Random(996)
    for t in (-0x80000000, -2, -1, 0, 1, 2, 3, 4, 5, 6, 0x7FFFFFFF):
        for p in (0, 1, -1, 0x7FFFFFFF):
            yield last_case(r, t, p)


def loans_edges():
    r = random.Random(997)
    for panel in (0, 6):
        for mode in (0, 1, 2, -1, 0x10000):
            yield loans_case(r, panel, mode)
    for ident in (0, 3, 12):
        yield loans_case(r, 6, 1)
        yield loans_case(r, 6, 1)


def rails_edges():
    r = random.Random(998)
    # message 0x12 (drag)
    for a in (0, 5, 0x8000, 0x18000, -0x8001, 0x7FFFFFFF):
        yield rails_case(r, 0x12, a)
    yield rails_case(r, 0x12, 7, **{"getExtraParam:1": [0]})
    # message 0xb (close)
    for a in (-1, 0, 1):
        for anim in (0, 1, 0x10000, 0x10001):
            yield rails_case(r, 0xB, a, setAnimTimer=[anim], **{"getExtraParam:1": [2]})
    yield rails_case(r, 0xB, -1, **{"getExtraParam:1": [0]})
    yield rails_case(r, 0xB, -1, f130=0, **{"getExtraParam:1": [2], "setAnimTimer": [1], "GetWindow:36": [0]})
    yield rails_case(r, 0xB, 3, **{"getExtraParam:1": [2], "setAnimTimer": [1], "GetWindow:36": [9], "pos": [0, 0, 0, 0, 0, 0, 0, 0]})
    yield rails_case(r, 0xB, 3, **{"getExtraParam:1": [2], "setAnimTimer": [1], "pos": [-5, 0, 0, 0, 7, 0, 0, 0]})
    yield rails_case(r, 0xB, 3, **{"getExtraParam:1": [2], "setAnimTimer": [1], "pos": [100, 0, 0, 0, -100, 0, 0, 0]})
    # message 0xa (arrival)
    for f134 in (0, 1, 2, 0x80, 0xFF):
        for a in (0, 1, 0x1234, -1):
            for anim in (0, 1, 0x10000):
                yield rails_case(r, 0xA, a, f134=f134, setAnimTimer=[anim])
    for link in (0, 1, 0x10000, 2):
        yield rails_case(r, 0xA, 1, f134=0, setAnimTimer=[1], link=[link])
    for pos in ([0] * 16, [10, 20, 300, 40] * 4, [-5, -7, 9, 11] * 4, [32767, -32768, 32767, -32768] * 4, [1, 2, 3, 5] * 4,
                [0, 1, 0, 2, 0, 3, 0, 4] * 2):
        for dims in ([0, 0], [33, 17], [-5, -7], [1, 1], [32767, 32767], [100, 50]):
            yield rails_case(r, 0xA, 1, f134=0, setAnimTimer=[1], link=[1], pos=pos, dims=dims)
    yield rails_case(r, 0xA, 1, f134=0, setAnimTimer=[1], link=[1], z=-32768)
    yield rails_case(r, 0xA, 1, f134=0, setAnimTimer=[1], link=[1], z=32767)
    yield rails_case(r, 0xA, 1, f134=0, setAnimTimer=[1], link=[1], **{"GetWindow:36": [0]})
    # message 0x10 (animation step)
    for f130 in (0, 1, 2, 0xFFFFFFFF):
        for f134 in (3, 4, 0, 1, 0x83):
            for ex1 in (0, 2):
                for ex2 in (0, 0x77, -1):
                    yield rails_case(r, 0x10, 0x80080, f130=f130, f134=f134, **{"getExtraParam:1": [ex1], "getExtraParam:2": [ex2]})
    for ex0 in (0, 5, -5, 0x8000, 0x18000, -0x8001, 0x7FFFFFFF):
        yield rails_case(r, 0x10, 0x80080, f130=3, **{"getExtraParam:0": [ex0], "pos": [0, 0, 32767, 0]})
        yield rails_case(r, 0x10, 0x80080, f130=3, **{"getExtraParam:0": [ex0], "pos": [0, 0, -32768, 0]})
    for a in (0x80081, 0x7FF80, 0x80000, 0, 0x80080 + 0x10000):
        yield rails_case(r, 0x10, a)
    # other messages
    for msg in (0x100, 0x15, 0x9, 0x11, 0x13, 0xC, 0xF, 0, -1, 0x101, 0x10, 0x14):
        for a in (0x24, 0x25, 0, -1):
            yield rails_case(r, msg, a)


def rails_random(r):
    return rails_case(r)


TESTS = [
    FnTest("UI_MainPanelDateChange", ppc, host, edge=date_edges, rnd=lambda r: date_case(r)),
    FnTest("SetLastPanel", ppc, host, edge=last_edges, rnd=lambda r: last_case(r, r.choice([None, 0, 1, 2, 3, 4, 5, -1]))),
    FnTest("UI_LoansPanelDateChange", ppc, host, edge=loans_edges, rnd=lambda r: loans_case(r)),
    FnTest("RailsCallback", ppc, host, edge=rails_edges, rnd=rails_random),
]
