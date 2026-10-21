"""Unit v5951: the staff room / show status static initialisers, the staff room list filler and row maker, the staff status view
window callback and the staff name edit panel, against libtpw_unit_sinit_ui_staffRoomStatus_cpp.dylib
(tests/unit_sinit_ui_staffRoomStatus_cpp.cpp), main app ("SimTheme Park"),
src/game/ui/ui_showStatus.cpp, src/game/ui/ui_staffRoomStatus.cpp, src/game/ui/ui_staffStatus.cpp:
  __sinit_ui_staffRoomStatus_cpp 0x101c40f0, LCFormat 0x101c3360, StaffStatusViewWindowCallback 0x101c4ec0, StaffRoomListFiller 0x101c30fc,
  __sinit_ui_showStatus_cpp 0x101c2b10, StaffStatusPanel::ShowEditNamePanel 0x101c5878

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (window and edit box
members, font table, strings, draw objects, the staff members' name and rest test, the list's AddItem, the panel constructor, ZoomTo, the
default window handler, and the virtual calls through __ptr_glue) is a fake on both sides that records its arguments.

The world of a case: the globals and blocks (panel, parent window, thing, windows, font) written before the call, the answer queues. What is
compared: the answer, the globals and blocks read back after the call, the event log. The parent window's z is written at the Mac's
+0xdc on the PPC side and at the host window's mZ (from tpw_sr_layout) on the host side: the host window is laid out differently.

No fused operations (fctiwz only on values below 2^31). Not tested: a zero screen height (divw by zero is undefined on the hardware), a
NULL parent window with the edit window clear (the original reads through it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_ui_staffRoomStatus_cpp"
MASK = 0xFFFFFFFF

FN_SINIT_ROOM, FN_LCFORMAT, FN_CALLBACK, FN_FILLER, FN_SINIT_SHOW, FN_SHOW = (0x101C40F0, 0x101C3360, 0x101C4EC0, 0x101C30FC, 0x101C2B10,
                                                                              0x101C5878)
SIZES = {FN_SINIT_ROOM: 124, FN_LCFORMAT: 236, FN_CALLBACK: 132, FN_FILLER: 164, FN_SINIT_SHOW: 76, FN_SHOW: 824}
OWN = set(SIZES)

GLUE = 0x1023BAA8
ADDR = dict(thing=0x103947A4, room=0x10395A9C, show=0x10395A58, staffpanel=0x10395B1C, roomid=0x10395A90, list=0x10395A94,
            screen=0x10395BBA, font=0x102AED60, fmt=0x10395A8C, valueA=0x10395ACC, valueB=0x10395AD0, editname=0x10395AD4,
            script=0x102AF16C, callback=0x10267F90, table_room=0x102AED88, table_show=0x102AE980)
HOST_LAYOUT = [0x24]
PPC_WINDOW = (0xDC, 0x140)   # the parent window: its z offset and size

# the blocks of a case: name -> size; labels of the objects the log names
BLOCKS = [("built", 0x140), ("title", 0x140), ("edit", 0x140), ("thing", 0x240), ("parent", 0x140), ("panel", 0x40), ("tstr", 16),
          ("nstr", 16), ("font", 0x40), ("drawobj", 0x20), ("fvt", 0x40), ("wvt", 0x40), ("win", 0x140), ("list", 0x40)]
LABELS = dict(built=1, title=2, edit=3, thing=4, parent=5, panel=6, tstr=7, nstr=8, font=9, drawobj=10, win=15, list=16)
GLOBAL_LABELS = dict(script=11, callback=12, editname=13, staffpanel=14)
FONT_SLOT = 0x1111
WIN_SLOT = 0x2222


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rec(name, kinds, ret=None):
    """A fake that records the argument registers (L label, i s32, h s16, u u32) and answers ret(e, L)."""
    def fn(e):
        L = e.cl
        args = []
        for i, k in enumerate(kinds):
            v = e.r(3 + i)
            args.append(L.lab(v) if k == "L" else s32(v) if k == "i" else T.s16(v) if k == "h" else v & MASK)
        L.rec(name, tuple(args))
        if ret is not None:
            e.set_r(3, ret(e, L) & MASK)
    return fn


def stub_find(e):
    L = e.cl
    a = e.r(3)
    L.rec("FindDrawObject", (e.u32(a), e.u32(a + 4)))
    e.set_r(3, L.b["drawobj"])


def stub_isresting(e):
    L = e.cl
    L.rec("IsResting", (L.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, L.ret("IsResting") & MASK)


def stub_additem(e):
    L = e.cl
    item = e.r(4)
    L.rec("AddItem", (L.lab(e.r(3)), L.lab(e.u32(item)), s32(e.u32(item + 4)), L.lab(e.r(5)), s32(e.r(6)), s32(e.r(7))))
    e.set_r(3, 0)


def stub_glue(e):
    L = e.cl
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))      # the glue saves the TOC register where the caller reloads it from
    if e.r(12) == FONT_SLOT:
        L.rec("FontHeight", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("FontHeight") & MASK)
    else:
        L.rec("SetText", (L.lab(e.r(3)), L.lab(e.r(4))))


def stub_getwindow(e):
    L = e.cl
    e.cl.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    i = e.r(4)
    e.set_r(3, L.b["title"] if i == 0x5DAA else L.b["edit"] if i == 0x5DAB else 0)


STUBS.internal(0x101DEB78, rec("Build", "LLL", lambda e, L: L.b["built"]))
STUBS.internal(0x101DD8BC, rec("setZ", "Lh"))
STUBS.internal(0x101DCFD0, rec("setExtraParam", "LhL", lambda e, L: 0))
STUBS.internal(0x1018FBE8, CL.recorder("TitleCtor", "N"))
STUBS.internal(0x1018F1D0, CL.recorder("CursorCtor", "N"))
STUBS.internal(0x101DBE40, stub_getwindow)
STUBS.internal(0x101DB624, rec("SetClientDrawObject", "LL"))
STUBS.internal(0x10180270, rec("GetFont", "u", lambda e, L: L.b["font"]))
STUBS.internal(0x101DA118, rec("SetFont", "LL"))
STUBS.internal(0x101DA204, rec("SetFontJust", "Lii"))
STUBS.internal(0x10180468, rec("GetString", "i", lambda e, L: L.b["tstr"]))
STUBS.internal(0x1018BD3C, rec("TitleColour", "L"))
STUBS.internal(0x101DA3AC, rec("SetFontColour", "Liiii"))
STUBS.internal(0x101CD7E0, rec("SetStaticTextArea", "LLi", lambda e, L: 0))
STUBS.internal(0x101CD2D4, stub_find)
STUBS.internal(0x101CDAC4, rec("SetCursorDrawObject", "LL"))
STUBS.internal(0x10126F98, rec("GetName", "L", lambda e, L: L.b["nstr"]))
STUBS.internal(0x101CE8A4, rec("SelectAll", "L"))
STUBS.internal(0x10182364, rec("DisableAllKeyTables", ""))
STUBS.internal(0x101DCCEC, rec("CaptureKb", "Li"))
STUBS.internal(0x1023BAA8, stub_glue)
STUBS.internal(0x101D98D8, CL.recorder("WindowCtor", "NLiihhhh"))
STUBS.internal(0x101DC750, rec("CreateObject", "Liiihhhh", lambda e, L: 0))
STUBS.internal(0x10126C1C, stub_isresting)
STUBS.internal(0x101D3000, stub_additem)
STUBS.internal(0x101929E8, rec("UIStatusPanel", "L"))
STUBS.internal(0x10193330, rec("ZoomTo", "L"))
STUBS.internal(0x101DE318, rec("Default", "Liii", lambda e, L: L.ret("Default")))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def alloc_blocks(self):
        sizes = dict(BLOCKS, parent=self.window_layout()[1])
        self.b = {name: self.alloc(sizes[name]) for name, size in BLOCKS}


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.alloc_blocks()
        fixed = {self.b[n]: LABELS[n] for n in LABELS}
        fixed.update({ADDR[n]: GLOBAL_LABELS[n] for n in GLOBAL_LABELS})
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.L.b = self.b
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def p32(self, v):
        return v

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a, v):
        self.words(a, [v])

    def window_layout(self):
        return PPC_WINDOW

    def log(self):
        return self.L.log

    def lab(self, v):
        return self.L.lab(v)

    def call(self, fn, args):
        return T.call_ppc(self.e, fn, [a & MASK for a in args])[0] & MASK

    def objects(self):
        pass


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.alloc_blocks()
        fixed = {LABELS[n]: self.b[n] for n in LABELS}
        fixed.update({GLOBAL_LABELS[n]: self.g(n) for n in GLOBAL_LABELS})
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sr_addr(name.encode())

    def p32(self, v):
        return self.lib.tpw_sr_off(v)

    def alloc(self, n):
        return self.lib.tpw_sr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def window_layout(self):
        return self.lib.window_layout

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        name = {FN_SINIT_ROOM: "sinit_room", FN_LCFORMAT: "lcformat", FN_CALLBACK: "callback", FN_FILLER: "filler",
                FN_SINIT_SHOW: "sinit_show", FN_SHOW: "show"}[fn]
        f = getattr(self.lib, "tpw_sr_" + name)
        if fn == FN_CALLBACK:
            return f(args[0], s32(args[1]), s32(args[2]), s32(args[3])) & MASK
        if fn in (FN_FILLER, FN_SHOW):
            return f(args[0]) or 0
        return (f() or 0) & MASK

    def objects(self):
        arr = (C.c_uint64 * 8)(self.b["built"], self.b["title"], self.b["edit"], self.b["font"], self.b["drawobj"], self.b["tstr"],
                               self.b["nstr"], self.b["thing"])
        self.lib.tpw_sr_setobjs(arr)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sr_addr.restype, lib.tpw_sr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sr_alloc.restype, lib.tpw_sr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sr_off.restype, lib.tpw_sr_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_sr_setobjs.argtypes = [C.POINTER(C.c_uint64)]
    lib.tpw_sr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sr_lcformat.restype, lib.tpw_sr_lcformat.argtypes = C.c_int64, []
    lib.tpw_sr_filler.restype, lib.tpw_sr_filler.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_sr_callback.restype, lib.tpw_sr_callback.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_sr_show.restype, lib.tpw_sr_show.argtypes = None, [C.c_uint64]
    lib.tpw_sr_sinit_room.restype, lib.tpw_sr_sinit_room.argtypes = None, []
    lib.tpw_sr_sinit_show.restype, lib.tpw_sr_sinit_show.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_sr_layout(out)
    assert list(out)[:len(HOST_LAYOUT)] == HOST_LAYOUT, list(out)[:n]
    lib.window_layout = tuple(out[len(HOST_LAYOUT):n])
    lib.bound = True


def build(s, case):
    b = s.b
    s.objects()
    s.words(b["font"] + 0x10, [b["fvt"]])
    s.words(b["fvt"] + 0xC, [FONT_SLOT])
    for w in ("title", "edit"):
        s.words(b[w] + 0x118, [b["wvt"]])
    s.words(b["wvt"] + 0x2C, [WIN_SLOT])
    s.half(b["parent"] + s.window_layout()[0], case["z"])
    s.words(b["panel"] + 8, [case["win8"]])
    s.words(b["panel"] + 0xC, [s.p32(b["parent"])])
    s.write(b["thing"] + 2, bytes([case["type"]]))
    s.words(b["thing"] + 0x1F8, [fb(case["energy"])])
    s.ptr(s.g("thing"), 0 if case["nothing"] else b["thing"])
    s.ptr(s.g("list"), b["list"])
    s.half(s.g("roomid"), case["roomid"])
    s.half(s.g("screen"), case["screen"])
    s.words(s.g("font"), [case["fontidx"]])
    s.words(s.g("fmt"), [case["fmt"]])
    s.words(s.g("valueA"), [case["junk"][0]])
    s.words(s.g("valueB"), [case["junk"][1]])
    s.words(s.g("room"), case["junk"][2:14])
    s.words(s.g("show"), case["junk"][14:23])


def table_word(s, name, word):
    if isinstance(s, Host):
        ptr = struct.unpack("<Q", s.read(s.g(name), 8))[0]
        expected = s.p32(ptr) if ptr else -1
    else:
        expected = s.g(name)
    return 1 if word == expected else word


def observe(s, case, answer):
    b = s.b
    w8 = s.rd_word(b["panel"] + 8)
    room = s.rd_words(s.g("room"), 12)
    room[8] = table_word(s, "table_room", room[8])
    show = s.rd_words(s.g("show"), 9)
    show[8] = table_word(s, "table_show", show[8])
    if case["op"] == "lcformat":
        answer = s.lab(answer) if isinstance(s, Ppc) else answer
    elif case["op"] in ("sinit_room", "sinit_show", "show"):
        answer = 0          # void: r3 is whatever the last callee left
    return {"answer": answer, "log": s.log(), "w8": 1 if w8 == s.p32(b["built"]) else w8, "fmt": s.rd_words(s.g("fmt"), 1),
            "valueA": s.rd_words(s.g("valueA"), 1), "valueB": s.rd_words(s.g("valueB"), 1), "room": room, "show": show}


def execute(s, case):
    b = s.b
    op = case["op"]
    if op == "lcformat":
        return s.call(FN_LCFORMAT, [])
    if op == "filler":
        return s.call(FN_FILLER, [b["thing"]])
    if op == "callback":
        return s.call(FN_CALLBACK, [b["win"], case["msg"], case["a"], case["b"]])
    if op == "show":
        return s.call(FN_SHOW, [b["panel"]])
    return s.call(FN_SINIT_ROOM if op == "sinit_room" else FN_SINIT_SHOW, [])


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


def base(r, op):
    return {"op": op, "queues": {"IsResting": [r.choice([0, 1])], "FontHeight": [r.choice([0, 1, 8, 16, 24, -3, 0x7FFF, -0x8000])],
                                 "Default": [r.choice([0, 1, -1, 77])]},
            "z": r.choice([0, 1, -1, 100, -300, 0x7FFF, -0x8000]), "win8": r.choice([0, 0, 0, 0x1234]),
            "type": r.choice([0, 3, 4, 5, 6, 7, 8, 9, r.randrange(256)]), "nothing": r.random() < 0.1,
            "energy": r.choice([0.0, 255.0, 256.0, 100.0, 300.7, -5.5, 1e5, -1e5, 0.99, 1023.5, r.uniform(-500, 500)]),
            "roomid": r.randrange(0x10000), "screen": r.choice([768, 1, -1, 600, 1024, 3, r.randrange(1, 2000)]),
            "fontidx": r.choice([7, 0, 5, r.randrange(20)]), "fmt": r.choice([0, 0, 0, 38, r.randrange(1, 100), -7]),
            "junk": [r.randrange(1 << 32) for _ in range(23)],
            "msg": r.choice([0x10006, 0x10006, 0x10005, 0, 0x10007, r.randrange(1 << 20)]), "a": r.randrange(-5, 5), "b": r.randrange(-5, 5)}


def make(op):
    def one(r, **over):
        c = base(r, op)
        c.update(over)
        return c

    def edges():
        r = random.Random(op.__hash__() & 0xFFFF if False else sum(map(ord, op)))
        if op == "show":
            for t in range(10):
                for nothing in (False, True):
                    for win8 in (0, 0x77):
                        yield one(r, type=t, nothing=nothing, win8=win8)
        elif op == "filler":
            for resting in (0, 1):
                for en in (0.0, 255.0, 256.0, 1023.0, 100.0, -1.0, 0.5):
                    c = one(r, energy=en)
                    c["queues"]["IsResting"] = [resting]
                    yield c
        elif op == "lcformat":
            for fmt in (0, 5):
                for hh in (0, 16, -16, 1):
                    c = one(r, fmt=fmt, screen=768)
                    c["queues"]["FontHeight"] = [hh]
                    yield c
        elif op == "callback":
            for m in (0x10006, 0x10005, 0x10007, 0, -1):
                yield one(r, msg=m)
        else:
            yield one(r)
            yield one(r)

    def rnd(r):
        return one(r)

    return edges, rnd


TESTS = []
for _name, _op in (("__sinit_ui_staffRoomStatus_cpp", "sinit_room"), ("LCFormat", "lcformat"),
                   ("StaffStatusViewWindowCallback", "callback"), ("StaffRoomListFiller", "filler"),
                   ("__sinit_ui_showStatus_cpp", "sinit_show"), ("StaffStatusPanel::ShowEditNamePanel", "show")):
    _edges, _rnd = make(_op)
    TESTS.append(FnTest(_name, ppc, host, edge=_edges, rnd=_rnd))
