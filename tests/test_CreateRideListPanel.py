"""Unit v5010, the ride list panel, against libtpw_unit_CreateRideListPanel.dylib (tests/unit_CreateRideListPanel.cpp), main app
("SimTheme Park"), src/CreateRideListPanel.cpp:
  CreateRideListPanel 0x101b8434

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee is a fake on both sides that records its
arguments (windows and objects as labels, strings as text) and answers from the case: GetWindow from a link table (parent, id) -> child,
Build from the case's result, GetBalance / FindData from queues, GetBankAccount, GetFontTableEntry, FindDrawObject and UIMain_GetString from
named objects. The windows carry a vtable whose SetText slot (the original's call through __ptr_glue, +0x2c of the table at +0x118) records.
The constructors and operator new are fakes too (the host's constructors are written without a vtable, see the unit library).

The world of a case: whether the panel is up (gRideListPanel), the type argument and the selected id (in the high half of r4), the globals
the code reads (the last type, the two "none" ids, the font, the sort column, the four words a new panel zeroes), the window links (the list
and the type buttons and the column buttons may be missing, the build may fail), the z of the two windows that get z + 12, the balance, the
FindData answer. What is compared: the call log of every callee, gRideListPanel (as a label), the four zeroed words, the text the
formatter wrote. The sizes asked of operator new (0x19c, 0xdc, 0x19c) are checked on the PowerPC side.

No fused operations. Not tested: a NULL game window or a missing title or picture window (the original reads +0xdc and +0x118 of them).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateRideListPanel"
MASK = 0xFFFFFFFF

FN = 0x101B8434
SIZES = {FN: 1936}
OWN = set(SIZES)

# ---- the callees (the asm's bl targets) and the glue imports
SETLASTPANEL, FILLRIDELIST, GETWINDOW, CB_SETCURRENT, FINDDATA, LC_SETCURRENT = 0x101A57FC, 0x101B5C14, 0x101DBE40, 0x101CC148, 0x101D4384, 0x101D4C64
REMOVESLIDEOUT, CHECKPANEL, BUILD, SETPANEL, GETBANK, GETBALANCE = 0x101A83D8, 0x10180514, 0x101DEB78, 0x101805A0, 0x1013EDFC, 0x100F32B4
TITLE_CTOR, SETDRAWOBJECT, SETFONTJUST, GETFONT, SETFONT, SETFONTCOLOUR = 0x1018FBE8, 0x101DB474, 0x101DA204, 0x10180270, 0x101DA118, 0x101DA3AC
SPRITE_CTOR, SETCLIENTOBJECT, SETZ, TEXTSPRITE_CTOR, SETFONTWRAP, SETCLIENTTEXT = 0x1018C978, 0x101DB624, 0x101DD8BC, 0x1018D11C, 0x101DA2D8, 0x101DE130
SETCALLBACK, SETCOLFORMATS, SETCOLTYPE, FINDDRAWOBJECT, SETSELOBJECT, SETSORT = 0x101DCBA8, 0x101D2864, 0x101D2614, 0x101CD2D4, 0x101D1EB4, 0x101D4DB8
GETSTRING, MAKEBUTTON, SETTITLETEXT, ADVISOR = 0x10180468, 0x101818E4, 0x10180794, 0x101821CC
PTR_GLUE, OP_NEW, LABS, FORMATSTRING = 0x1023BAA8, 0x102384B8, 0x10239928, 0x10239730

# ---- the Mac's addresses
G_GAMEWINDOW, G_GAMEWORLD, G_TEXT, G_PANEL = 0x103949E4, 0x1037EBFC, 0x1037FF18, 0x10395998
G_TYPE, G_NONEA, G_NONEB, G_FONT, G_SORT = 0x10395994, 0x102ACEEC, 0x102ACEEE, 0x102ACEDC, 0x102ACEE4
G_F90, G_F8C, G_F8A, G_F88 = 0x10395990, 0x1039598C, 0x1039598A, 0x10395988
BUFFER = 0x10380C48
SCRIPT, VT_RIDEPANELMSG, VT_LCHANDLER, VT_LCFORMAT, VT_LCPOST = 0x102ACA9C, 0x10267D80, 0x10267D78, 0x10267D68, 0x10267D60
VTAG_VT = 0x103BCAF0

INTS = {"gRideListType": (G_TYPE, 4), "gRideListNoneA": (G_NONEA, 2), "gRideListNoneB": (G_NONEB, 2), "gRideListButtonFont": (0x102ACEDC, 4),
        "gRideListSortColumn": (G_SORT, 4), "gRideListF90": (G_F90, 4), "gRideListF8C": (G_F8C, 2), "gRideListF8A": (G_F8A, 2),
        "gRideListF88": (G_F88, 2)}
PTRS = {"gGameWindow": G_GAMEWINDOW, "gGameWorld": G_GAMEWORLD, "gRideListPanel": G_PANEL, "table": G_TEXT + 0x564}

# the host's offset of mZ and the host's window size (tpw_crl_layout)
HOST_LAYOUT = [360, 488]

WINDOWS = ["root", "panel", "title", "w1ea", "w1eb", "w1ec", "list", "b10", "b11", "b12", "check"]
BLOCKS = ["bank", "world", "table", "font5", "font6", "selection"]
LABELED = WINDOWS + BLOCKS
STRING_IDS = [0x7B, 0x7C, 0x8A, 0x77, 0x78, 0x79, 0x7A]


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = [None]            # the Ppc side of the running case


def save_toc(e):
    """What the real glue does: the caller's TOC pointer goes to 0x14(r1), where the original reloads r2 from."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))


def ident(addr, table):
    return table.get(addr & MASK, 0)


def rec(name, spec="", ret=None):
    STUBS.internal(name[0], CL.recorder(name[1], spec, ret=ret))


def stub(addr, name, spec="", ret=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret=ret))


def ret_get_window(e, log):
    return CUR[0].links.get((e.r(3) & MASK, s32(e.r(4))), 0)


def ret_named(key):
    return lambda e, log: CUR[0].named.get(key, 0)


stub(SETLASTPANEL, "SetLastPanel", "ii")
stub(FILLRIDELIST, "FillRideList", "ii")
stub(GETWINDOW, "GetWindow", "Li", ret=ret_get_window)
stub(CB_SETCURRENT, "CheckButtons_SetCurrent", "Li")
stub(FINDDATA, "FindData", "Li", ret="i")
stub(LC_SETCURRENT, "ListControl_SetCurrent", "Li")
stub(REMOVESLIDEOUT, "RemoveSlideoutPanel", "h")
stub(CHECKPANEL, "UIMain_CheckPanel", "", ret="b")
stub(SETPANEL, "UIMain_SetPanel", "L")
stub(GETBANK, "GetBankAccount", "L", ret=ret_named("bank"))
stub(GETBALANCE, "GetBalance", "L", ret="i")
stub(TITLE_CTOR, "InterfaceTitleTextObject", "Ni")
stub(SETDRAWOBJECT, "SetDrawObject", "LL")
stub(SETFONTJUST, "SetFontJust", "Lii")
stub(GETFONT, "GetFontTableEntry", "u", ret=lambda e, log: CUR[0].named.get("font%d" % e.r(3), 0))
stub(SETFONT, "SetFont", "LL")
stub(SETFONTCOLOUR, "SetFontColour", "Liiii")
stub(SPRITE_CTOR, "InterfaceDrawObjectSprite", "Ni")
stub(SETCLIENTOBJECT, "SetClientDrawObject", "LL")
stub(SETZ, "setZ", "Lh")
stub(TEXTSPRITE_CTOR, "InterfaceTextObjectSprite", "Ni")
stub(SETFONTWRAP, "SetFontWrap", "Lii")
stub(SETCLIENTTEXT, "SetClientDrawText", "Lu")
stub(SETCOLTYPE, "SetColumnType", "Lhi")
stub(FINDDRAWOBJECT, "FindDrawObject", "s", ret=ret_named("selection"))
stub(SETSELOBJECT, "SetSelectionObject", "LL")
stub(SETSORT, "SetSort", "Lib")
stub(GETSTRING, "UIMain_GetString", "i", ret=lambda e, log: CUR[0].named.get("str%x" % s32(e.r(3)), 0))
stub(MAKEBUTTON, "MAKECOLUMNBUTTON", "LSiiiii")
stub(SETTITLETEXT, "UIMain_SetTitleText", "LiS")
stub(ADVISOR, "UIMain_AdvisorMessage", "i")

_settext = CL.recorder("SetText", "LS")


def stub_ptr_glue(e):
    """The call through the table in r12 (the window's SetText): the slot must hold the table the test put there."""
    save_toc(e)
    assert e.r(12) == CUR[0].tvector, hex(e.r(12))
    _settext(e)


STUBS.internal(PTR_GLUE, stub_ptr_glue)


def stub_new(e):
    save_toc(e)
    size = e.r(3) & MASK
    CUR[0].news.append(size)
    e.set_r(3, e.alloc(max(size, 4)))


STUBS.internal(OP_NEW, stub_new)


def stub_labs(e):
    save_toc(e)
    x = s32(e.r(3))
    e.set_r(3, (-x if x < 0 else x) & MASK)


STUBS.internal(LABS, stub_labs)


def stub_build(e):
    L = e.cl
    L.rec("Build", (L.lab(e.r(3)), 5 if e.r(4) & MASK == SCRIPT else 0, 1 if e.r(5) & MASK == VT_RIDEPANELMSG else 0))
    e.set_r(3, CUR[0].named.get("buildresult", 0))


STUBS.internal(BUILD, stub_build)


def stub_setcallback(e):
    L = e.cl
    L.rec("SetCallbackFunction", (L.lab(e.r(3)), 2 if e.r(4) & MASK == VT_LCHANDLER else 0))


STUBS.internal(SETCALLBACK, stub_setcallback)


def stub_setcolformats(e):
    L = e.cl
    L.rec("SetColumnFormatsAll", (L.lab(e.r(3)), 3 if e.r(4) & MASK == VT_LCFORMAT else 0, 4 if e.r(5) & MASK == VT_LCPOST else 0))


STUBS.internal(SETCOLFORMATS, stub_setcolformats)


def stub_formatstring(e):
    """FormatString(table, out, size, id, tag*): the tag is magic, vtable, id, value, 0 (the inline temporary)."""
    save_toc(e)
    L = e.cl
    out, size, fid, tag = e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK, e.r(7) & MASK
    magic, vptr, tid, value, tail = e.u32(tag), e.u32(tag + 4), e.u32(tag + 8), s32(e.u32(tag + 12)), e.u32(tag + 16)
    # the vtable pointer is what the import's TOC slot (0x10261344) holds in the emulator
    ok = 1 if vptr == e.u32(0x10261344) and tail == 0 else 0
    L.rec("FormatString", (L.lab(e.r(3)), 1 if out == BUFFER else 0, size, fid, magic, tid, value, ok))
    text = "F%d:%d" % (fid, value)
    n = min(len(text) + 1, size)
    e.write(out, b"".join(struct.pack(">H", ord(text[i]) if i < len(text) else 0) for i in range(n)))
    e.set_r(3, 0)


STUBS.internal(FORMATSTRING, stub_formatstring)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def newstr(self, text):
        a = self.alloc(2 * (len(text) + 1))
        self.write(a, b"".join(struct.pack(self.fmt16, ord(c)) for c in text + "\0"))
        return a

    def rd16s(self, a, n):
        return list(struct.unpack(self.fmt16[0] + "%dH" % n, self.read(a, 2 * n)))

    def sh(self, a, v):
        self.write(a, struct.pack(self.fmt16, v & 0xFFFF))


class Ppc(Side):
    fmt = ">I"
    fmt16 = ">H"
    zoff = 0xDC

    def __init__(self, e, case):
        self.e = e
        CUR[0] = self
        self.links = {}
        self.named = {}
        self.news = []
        self.vt = e.alloc(0x40)
        self.tvector = e.alloc(8)
        self.words(self.vt + 0x2C, [self.tvector])
        self.words(self.tvector, [0x10000000, 0])
        self.objs = alloc_objs(self)
        fixed = {self.objs[n]: i + 1 for i, n in enumerate(LABELED)}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def newwin(self):
        a = self.e.alloc(0x140)
        self.words(a + 0x118, [self.vt])
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def setnamed(self, name, addr):
        self.named[name] = addr

    def link(self, parent, wid, child):
        self.links[(parent, wid)] = child

    def setint(self, name, value):
        addr, size = INTS[name]
        self.write(addr, (value & ((1 << (8 * size)) - 1)).to_bytes(size, "big"))

    def getint(self, name):
        addr, size = INTS[name]
        return int.from_bytes(self.read(addr, size), "big")

    def setptr(self, name, value):
        self.words(PTRS[name], [value])

    def labelof(self, name):
        return self.L.lab(self.e.u32(PTRS[name]))

    def buffer(self):
        return BUFFER

    def call(self, kind, sel):
        T.call_ppc(self.e, FN, [kind & MASK, (sel & 0xFFFF) << 16])


class Host(Side):
    fmt = "<I"
    fmt16 = "<H"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_crl_reset()
        self.zoff = HOST_LAYOUT[0]
        self.objs = alloc_objs(self)
        CL.host_begin(lib, {i + 1: self.objs[n] for i, n in enumerate(LABELED)}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_crl_alloc(n)

    def newwin(self):
        return self.lib.tpw_crl_newwin()

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setnamed(self, name, addr):
        self.lib.tpw_crl_setnamed(name.encode(), addr)

    def link(self, parent, wid, child):
        self.lib.tpw_crl_link(parent, wid, child)

    def setint(self, name, value):
        self.lib.tpw_crl_setint(name.encode(), value)

    def getint(self, name):
        return self.lib.tpw_crl_getint(name.encode()) & ((1 << (8 * INTS[name][1])) - 1)

    def setptr(self, name, value):
        self.lib.tpw_crl_setptr(name.encode(), value)

    def labelof(self, name):
        return self.lib.tpw_crl_lab(self.lib.tpw_crl_getptr(name.encode()))

    def buffer(self):
        return self.lib.tpw_crl_addr(b"buffer")

    def call(self, kind, sel):
        self.lib.tpw_crl_create(s32(kind), sel & 0xFFFF)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for name, res, args in (("addr", C.c_uint64, [C.c_char_p]), ("alloc", C.c_uint64, [C.c_uint32]), ("newwin", C.c_uint64, []),
                            ("reset", None, []), ("setnamed", None, [C.c_char_p, C.c_uint64]),
                            ("link", None, [C.c_uint64, C.c_int32, C.c_uint64]), ("setptr", None, [C.c_char_p, C.c_uint64]),
                            ("getptr", C.c_uint64, [C.c_char_p]), ("setint", None, [C.c_char_p, C.c_int64]),
                            ("getint", C.c_int64, [C.c_char_p]), ("lab", C.c_int64, [C.c_uint64]),
                            ("create", None, [C.c_int32, C.c_uint32])):
        f = getattr(lib, "tpw_crl_" + name)
        f.restype, f.argtypes = res, args
    lib.tpw_crl_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_crl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def alloc_objs(s):
    objs = {n: s.newwin() for n in WINDOWS}
    for n in BLOCKS:
        objs[n] = s.alloc(32)
    return objs


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    o = s.objs
    for n in ("bank", "font5", "font6", "selection"):
        s.setnamed(n, o[n])
    s.setnamed("font5", o["font5"])
    s.setnamed("font6", o["font6"])
    for sid in STRING_IDS:
        s.setnamed("str%x" % sid, s.newstr("T%x" % sid))
    s.setnamed("buildresult", o["panel"] if (c["mode"] == "new" and c["buildok"]) else 0)
    s.link(o["panel"], 0x200, o["title"])
    s.link(o["panel"], 0x1EA, o["w1ea"])
    s.link(o["w1ea"], 0x1EB, o["w1eb"])
    s.link(o["w1ea"], 0x1EC, o["w1ec"])
    if c["haslist"]:
        s.link(o["panel"], 0x1F8, o["list"])
    for i, n in enumerate(("b10", "b11", "b12")):
        if c["hasb"][i]:
            s.link(o["list"], 0x10 + i, o[n])
    if c["hascheck"]:
        s.link(o["list"], 0x1F9, o["check"])
    s.sh(o["w1eb"] + s.zoff, c["z"][0])
    s.sh(o["w1ec"] + s.zoff, c["z"][1])
    s.setptr("gGameWindow", o["root"])
    s.setptr("gGameWorld", o["world"])
    s.setptr("table", o["table"])
    s.setptr("gRideListPanel", o["panel"] if c["mode"] == "up" else 0)
    s.setint("gRideListType", c["gtype"])
    s.setint("gRideListNoneA", c["nonea"])
    s.setint("gRideListNoneB", c["noneb"])
    s.setint("gRideListButtonFont", c["font"])
    s.setint("gRideListSortColumn", c["sort"])
    for name, v in zip(("gRideListF90", "gRideListF8C", "gRideListF8A", "gRideListF88"), c["g"]):
        s.setint(name, v)
    s.write(s.buffer(), b"\0" * 64)


def observe(s, c):
    return {"log": s.log(), "panel": s.labelof("gRideListPanel"),
            "g": [s.getint(n) for n in ("gRideListF90", "gRideListF8C", "gRideListF8A", "gRideListF88")],
            "buffer": s.rd16s(s.buffer(), 24)}


def run(s, c):
    try:
        build(s, c)
        s.call(c["type"], c["selected"])
        return observe(s, c)
    finally:
        s.done()


def ppc(e, case):
    s = Ppc(e, case)
    s.log = lambda: s.L.log
    r = run(s, case)
    expected = [0x19C, 0xDC, 0x19C] if (case["mode"] == "new" and case["buildok"]) else []
    assert s.news == expected, (s.news, expected)
    return r


def host(lib, case):
    s = Host(lib, case)
    s.log = lambda: CL.host_end(lib)
    return run(s, case)


# ---------------------------------------------------------------------------------------------------------------- case generators

TYPES = [-1, 0, 1, 2, 3, 4, 5, -2, -7, 9]
BALANCES = [0, 1, -1, 12345, -12345, 0x7FFFFFFF, -0x80000000, -0x7FFFFFFF]


def base(r, mode=None):
    selected = r.choice([0, 0xFFFF, 0x1234, r.randrange(65536)])
    c = {"mode": mode or r.choice(["new", "up"]), "type": r.choice(TYPES), "selected": selected,
         "gtype": r.choice(TYPES), "nonea": r.choice([selected, selected, 0, 1, r.randrange(65536)]),
         "noneb": r.choice([selected, selected, 0, 1, r.randrange(65536)]), "font": r.choice([8, 0, -3, r.randrange(1000)]),
         "sort": r.choice([1, 0, 2, -1, r.randrange(100)]),
         "g": [r.choice([0, MASK, r.randrange(1 << 32)]), r.randrange(65536), r.randrange(65536), r.randrange(65536)],
         "buildok": r.random() < 0.85, "haslist": r.random() < 0.85, "hascheck": r.random() < 0.8,
         "hasb": [r.random() < 0.8 for _ in range(3)],
         "z": [r.choice([0, 1, 0x100, 0x7FF4, 0x7FFF, -1, -0x8000, r.randrange(-0x8000, 0x8000)]) for _ in range(2)]}
    c["queues"] = {"GetBalance": [r.choice(BALANCES + [r.randrange(-1000000, 1000000)])],
                   "FindData": [r.choice([-1, 0, 5, -5, r.randrange(0, 1000), r.randrange(-1000, 1000)])],
                   "UIMain_CheckPanel": [r.choice([0, 1])]}
    return c


def rnd(r):
    return base(r)


def edges():
    r = random.Random(5010)
    for mode in ("new", "up"):
        for t in TYPES:
            c = base(r, mode)
            c.update(type=t, buildok=True, haslist=True, hascheck=True, hasb=[True] * 3)
            yield c
            c = base(r, mode)
            c.update(type=t, gtype=t, buildok=True, haslist=True, hascheck=True, hasb=[True] * 3)
            yield c
    for mode in ("new", "up"):
        for key in ("buildok", "haslist", "hascheck"):
            c = base(r, mode)
            c[key] = False
            yield c
        for sel_equal in (True, False):
            for pos in (-1, 0, 7):
                c = base(r, mode)
                c.update(buildok=True, haslist=True)
                c["nonea"] = c["noneb"] = c["selected"] if sel_equal else (c["selected"] ^ 1)
                c["queues"]["FindData"] = [pos]
                yield c
    for bal in BALANCES:
        c = base(r, "new")
        c.update(buildok=True, haslist=True)
        c["queues"]["GetBalance"] = [bal]
        yield c


TESTS = [
    FnTest("CreateRideListPanel", ppc, host, edge=edges, rnd=rnd),
]
