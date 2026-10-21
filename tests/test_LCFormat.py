"""Unit v5210, the online park list panel's row functions, message function and fill, against libtpw_unit_LCFormat.dylib
(tests/unit_LCFormat.cpp), main app ("SimTheme Park"), src/game/ui/ui_onlineParkListPanel.cpp:
  LCFormat 0x101ebd58, LCPostFormat 0x101ebef4, OnlineParkListPanelMsg 0x101ebba8, FillParkList 0x101ec1d4
(the C++ names of the first two are OnlineParkListLCFormat and OnlineParkListLCPostFormat: LCFormat and LCPostFormat exist at other addresses).

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (windows, list control, font, draw object constructors, string library, master save, online system, game, info panel, the
virtual calls through __ptr_glue) are fakes on both sides that record their arguments; texts are compared as hashes of their characters.

The world of a case: the globals (panel, close function, lobby, master save, online system, row height and its flag, screen height), the
persistent windows, font, parks and tables, a park list built per case, and the answer queues (font height, GetWindow, default handler,
swear filter on/changes, localised name). What is compared: the answer, the globals, the event log.

No fused operations. Not tested: a screen height of 0 (divw by 0 differs between the emulator and the host), a NULL gOnlineParkListPanel
in FillParkList (the original calls GetWindow on it unconditionally), a NULL list.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LCFormat"
MASK = 0xFFFFFFFF

FN_FMT, FN_POST, FN_MSG, FN_FILL = 0x101ebd58, 0x101ebef4, 0x101ebba8, 0x101ec1d4
SIZES = {FN_FMT: 380, FN_POST: 684, FN_MSG: 364, FN_FILL: 560}
OWN = set(SIZES)

# ---- the callees faked on both sides
GETFONT, WINCTOR, CREATEOBJ = 0x10180270, 0x101d98d8, 0x101dc750
SPRITECTOR, NUMCTOR, RANKCTOR = 0x1018d11c, 0x1018d930, 0x1018f93c
SETCLIENTDRAW, SETFONT, SETCLIENTTEXT = 0x101db624, 0x101da118, 0x101de130
GETCHILD, GETNEXT, SETDRAWOBJ, SETFONTJUST = 0x101dbac4, 0x101dbb38, 0x101db474, 0x101da204
PTRGLUE, NEW, REMOVE, CAPTUREKB, GETWINDOW = 0x1023baa8, 0x102384b8, 0x101ec840, 0x101dccec, 0x101dbe40
GETDATA, INFOPANEL, CALLDEF, REMOVEALL = 0x101d41f0, 0x101eabb4, 0x101de318, 0x101d3cc8
STRCTOR, WCSLEN, STRASSIGN, STRDTOR = 0x101e0b84, 0x102389c8, 0x101b2510, 0x101468f8
MSCTOR, ISSWEAR, FILTER, WIDECTOR, ONLCTOR = 0x101694dc, 0x1016b438, 0x1020d2cc, 0x10238740, 0x1016f7d0
THEMENAME, CHARCTOR, GETLOC, CHARDTOR = 0x1017a588, 0x102382d8, 0x1014a478, 0x102382f0
ADDITEM, WIDEDTOR, UPDATE = 0x101d3000, 0x10238710, 0x101d2220

CALLBACK_VEC = 0x10268590        # the TVector of GeneralParkDownloadCompleteCallback
HEIGHT_VEC, SETVAL_VEC, CLOSE_VEC = 0x7FE00010, 0x7FE00020, 0x7FE00030

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host's come from tpw_pl_addr
ADDR = dict(g_panel=0x10396234, g_close=0x10396230, g_lobby=0x102e4888, g_master=0x10380a8c, g_online=0x10380ab0,
            g_rowh=0x1039622c, g_rowinit=0x10396228, g_screenh=0x10395bba)
GAME = 0x1037f134
LABELS = dict(w0=1, w1=2, w2=3, w3=4, w4=5, w5=6, w6=7, w7=8, park0=10, park1=11, park2=12, park3=13, font=14, master=15,
              onlinesys=16, themelist=17, game=18, lobby=19)
# the Mac's offsets of what the module writes by offset, and the names of the host's (tpw_pl_layout order)
MAC = dict(mWindow=4, pdata=0x14, p4c=0x4c, p50=0x50, p54=0x54, pb8=0xb8, listhead=4, listcount=0xc, listsize=0x10, nnext=4, nprev=8,
           nsize=12, themelist=0xf8)
HOSTKEYS = ["mWindow", "pdata", "p4c", "p50", "p54", "pb8", "psize", "listhead", "listcount", "listsize", "nnext", "nprev", "nsize",
            "themelist"]
NAMES = ["fantasy", "hallow", "jungle", "space"]
LOCAL = ["Fant", "Hall", "Jung", "Spac"]
P = {}                       # the persistent objects of the PowerPC side (set up once)


def s32(v):
    return T.s32(v)


def hash16(chars):
    h = 2166136261
    for c in chars:
        h = ((h ^ c) * 16777619) & MASK
    return h


def u16s(e, a):
    out = []
    while len(out) < 400:
        c = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if c == 0:
            break
        out.append(c)
    return out


def hash_at(e, a):
    return 0x1234 if a == 0 else hash16(u16s(e, a))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
R = CL.recorder


def stub_new(e):
    size = e.r(3) & MASK
    if size not in (0x4c, 0x404):        # the host makes those two with a plain new, which is not logged
        e.cl.rec("new", (size,))
    a = e.alloc(size)
    e.write(a, bytes(size))
    e.set_r(3, a)


def stub_getfont(e):
    e.cl.rec("GetFontTableEntry", (e.r(3) & MASK,))
    e.set_r(3, P["font"])


def stub_ptrglue(e):
    L = e.cl
    r12 = e.r(12) & MASK
    if r12 == HEIGHT_VEC:
        L.rec("GetHeight", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("GetHeight") & MASK)
    elif r12 == SETVAL_VEC:
        L.rec("SetValue", (L.lab(e.r(3)), s32(e.r(4))))
    elif r12 == CLOSE_VEC:
        L.rec("Close", ())
    else:
        raise AssertionError("unknown vector %x" % r12)


def stub_getchild(e):
    e.cl.rec("GetChild", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, P["w3"])


def stub_getnext(e):
    e.cl.rec("GetNext", (e.cl.lab(e.r(3)),))
    chain = {P["w3"]: P["w4"], P["w4"]: P["w5"], P["w5"]: P["w6"]}
    e.set_r(3, chain.get(e.r(3) & MASK, 0))


def stub_getwindow(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, P["w1"] if L.ret("GetWindow") != 0 else 0)


def stub_getdata(e):
    e.cl.rec("GetData", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.write(e.r(5) & MASK, struct.pack(">I", P["park0"]))
    e.set_r(3, 0)


def stub_infopanel(e):
    L = e.cl
    r6 = e.r(6) & MASK
    L.rec("InfoPanel", (L.lab(e.r(3)), L.lab(e.r(4)), 1 if e.r(5) else 0, 1 if r6 == CALLBACK_VEC else (2 if r6 else 0),
                        1 if e.r(7) else 0))


def stub_additem(e):
    L = e.cl
    item = e.r(4) & MASK
    w = struct.unpack(">5I", e.read(item, 20))
    L.rec("AddItem", (L.lab(e.r(3)), hash_at(e, w[0]), hash_at(e, w[1]), s32(w[2]), s32(w[3]), s32(w[4]), L.lab(e.r(5)),
                      s32(e.r(6)), s32(e.r(7))))


def stub_getloc(e):
    L = e.cl
    L.rec("GetLocalisedThemeName", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))
    e.write((e.r(5) & MASK) + 8, struct.pack(">I", P["loc"] + (L.ret("Loc") & 3) * 32))


def stub_themename(e):
    e.cl.rec("ReturnOnlineThemeName", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, P["names"] + (s32(e.r(4)) & 3) * 16)


def stub_strctor(e):
    e.cl.rec("WStringCtor", ())
    rep = e.alloc(16)
    e.write(rep, struct.pack(">IIII", 0, 3, 1, 0))
    e.write(e.r(3) & MASK, struct.pack(">I", rep))


def stub_strassign(e):
    text = e.r(4) & MASK
    e.cl.rec("WStringAssign", (hash_at(e, text), e.r(5) & MASK))
    rep = e.u32(e.r(3) & MASK)
    e.write(rep, struct.pack(">I", e.r(5) & MASK))
    e.write(rep + 0xc, struct.pack(">I", text))


def stub_strdtor(e):
    e.cl.rec("WStringDtor", (s32(e.r(4)),))


def stub_filter(e):
    L = e.cl
    L.rec("Filter", (L.lab(e.r(3)), e.r(5) & 0xff))
    if L.ret("FilterChanges") != 0:
        rep = e.u32(e.r(4) & MASK)
        data = e.u32(rep + 0xc)
        if struct.unpack(">H", e.read(data, 2))[0] != 0:
            e.write(rep + 0xc, struct.pack(">I", data + 2))


def stub_wcslen(e):
    e.set_r(3, len(u16s(e, e.r(3) & MASK)))


GLUE = {NEW, PTRGLUE, WCSLEN, CHARCTOR, CHARDTOR, WIDECTOR, WIDEDTOR}


def with_toc(fn):
    """The import glue saves the caller's TOC at 0x14(r1); the code reloads r2 from there afterwards."""
    def wrapped(e):
        e.write((e.r(1) & MASK) + 0x14, struct.pack(">I", e.r(2) & MASK))
        return fn(e)
    return wrapped


def _install(addr, fn):
    STUBS.internal(addr, with_toc(fn) if addr in GLUE else fn)


for addr, fn in [
    (NEW, stub_new), (GETFONT, stub_getfont), (PTRGLUE, stub_ptrglue), (GETCHILD, stub_getchild), (GETNEXT, stub_getnext),
    (GETWINDOW, stub_getwindow), (GETDATA, stub_getdata), (INFOPANEL, stub_infopanel), (ADDITEM, stub_additem),
    (GETLOC, stub_getloc), (THEMENAME, stub_themename), (STRCTOR, stub_strctor), (STRASSIGN, stub_strassign),
    (STRDTOR, stub_strdtor), (FILTER, stub_filter), (WCSLEN, stub_wcslen),
    (WINCTOR, R("WindowCtor", "NLiihhhh")), (CREATEOBJ, R("CreateObject", "Liiihhhh")),
    (SPRITECTOR, R("SpriteCtor", "Ni")), (NUMCTOR, R("NumberCtor", "Ni")), (RANKCTOR, R("RankCtor", "Ni")),
    (SETCLIENTDRAW, R("SetClientDrawObject", "LL")), (SETDRAWOBJ, R("SetDrawObject", "LL")), (SETFONT, R("SetFont", "LL")),
    (SETCLIENTTEXT, R("SetClientDrawText", "LL")), (SETFONTJUST, R("SetFontJust", "Lii")),
    (CAPTUREKB, R("CaptureKb", "Li")), (REMOVE, R("RemoveOnlineParkListPanel", "")),
    (CALLDEF, R("CallDefault", "Liii", "i")), (REMOVEALL, R("RemoveAllItems", "L")), (UPDATE, R("UpdateDisplay", "L")),
    (MSCTOR, R("MasterSaveCtor", "N")), (ONLCTOR, R("OnlineSystemCtor", "N")), (ISSWEAR, R("IsSwearFilterOn", "L", "b")),
    (WIDECTOR, R("WideCtor", "N")), (CHARCTOR, R("CharCtor", "Ns")), (CHARDTOR, R("CharDtor", "Li")),
    (WIDEDTOR, R("WideDtor", "Li")),
]:
    _install(addr, fn)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    be = lambda *ws: b"".join(struct.pack(">I", w) for w in ws)
    vt_win = T.persist_alloc(e, 0x40, be(*([0] * 9), SETVAL_VEC))        # [0x24]
    vt_font = T.persist_alloc(e, 0x20, be(0, 0, 0, HEIGHT_VEC))          # [0x0c]
    for i in range(8):
        data = bytearray(0x140)
        struct.pack_into(">I", data, 0x118, vt_win)
        P["w%d" % i] = T.persist_alloc(e, 0x140, bytes(data))
    font = bytearray(0x20)
    struct.pack_into(">I", font, 0x10, vt_font)
    P["font"] = T.persist_alloc(e, 0x20, bytes(font))
    for i in range(4):
        P["park%d" % i] = T.persist_alloc(e, 0xc0)
    P["master"] = T.persist_alloc(e, 0x50)
    P["themelist"] = T.persist_alloc(e, 0x20)
    onl = bytearray(0x404)
    struct.pack_into(">I", onl, 0xf8, P["themelist"])
    P["onlinesys"] = T.persist_alloc(e, 0x404, bytes(onl))
    P["lobby"] = T.persist_alloc(e, 0x20)
    P["game"] = GAME
    P["names"] = T.persist_alloc(e, 64, b"".join(n.encode().ljust(16, b"\0") for n in NAMES))
    P["loc"] = T.persist_alloc(e, 128, b"".join(n.encode("utf-16-be").ljust(32, b"\0") for n in LOCAL))


# ---------------------------------------------------------------------------------------------------------------- the two sides

HL = {}


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def i16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def wtext(self, text):
        chars = [ord(c) for c in text] + [0]
        a = self.alloc(2 * len(chars) + 2)
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(chars), *chars))
        return a


class Ppc(Side):
    fmt = ">I"
    psz = 4
    o = MAC

    def __init__(self, e, case):
        self.e = e
        fixed = {P[n]: lab for n, lab in LABELS.items()}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def wptr(self, name):
        return P[name]

    def closefn(self):
        return CLOSE_VEC

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_word(a)

    def label(self, v):
        return self.L.lab(v)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK

    def fmt_call(self):
        return self.L.lab(self.call(FN_FMT, []))

    def post_call(self, w):
        self.call(FN_POST, [w])
        return 0

    def msg_call(self, w, m, a, b):
        return s32(self.call(FN_MSG, [w, m, a, b]))

    def fill_call(self, lst):
        self.call(FN_FILL, [lst])
        return 0


class Host(Side):
    fmt = "<I"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.o = {k: HL[k] for k in MAC}
        fixed = {lab: self.lib.tpw_pl_addr(n.encode()) for n, lab in LABELS.items()}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_pl_addr(name.encode())

    def wptr(self, name):
        return self.lib.tpw_pl_addr(name.encode())

    def closefn(self):
        return self.lib.tpw_pl_addr(b"closefn")

    def alloc(self, n):
        return self.lib.tpw_pl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def label(self, v):
        return self.lib.tpw_pl_lab(v)

    def log(self):
        return CL.host_end(self.lib)

    def fmt_call(self):
        return self.lib.tpw_pl_fmt()

    def post_call(self, w):
        self.lib.tpw_pl_post(w)
        return 0

    def msg_call(self, w, m, a, b):
        return self.lib.tpw_pl_msg(w, s32(m), s32(a), s32(b))

    def fill_call(self, lst):
        self.lib.tpw_pl_fill(lst)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    f = lib.tpw_pl_addr
    f.restype, f.argtypes = C.c_uint64, [C.c_char_p]
    f = lib.tpw_pl_alloc
    f.restype, f.argtypes = C.c_uint64, [C.c_uint32]
    f = lib.tpw_pl_lab
    f.restype, f.argtypes = C.c_int64, [C.c_uint64]
    lib.tpw_pl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pl_fmt.restype, lib.tpw_pl_fmt.argtypes = C.c_int64, []
    lib.tpw_pl_post.restype, lib.tpw_pl_post.argtypes = None, [C.c_uint64]
    f = lib.tpw_pl_msg
    f.restype, f.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_pl_fill.restype, lib.tpw_pl_fill.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_pl_layout(out)
    assert n == len(HOSTKEYS)
    HL.update(zip(HOSTKEYS, list(out)[:n]))
    # the host's pointers are wider: the offsets only grow, the object sizes are what they are
    assert all(v >= 0 for v in HL.values()) and HL["psize"] > 0xc0 - 0x40 and HL["pdata"] >= MAC["pdata"]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    w = s.wptr
    s.ptr(s.g("g_panel"), w("w0") if case["panel"] else 0)
    s.ptr(s.g("g_close"), s.closefn() if case["close"] else 0)
    s.ptr(s.g("g_lobby"), w("lobby"))
    s.ptr(w("lobby") + s.o["mWindow"], w("w7"))
    s.ptr(s.g("g_master"), w("master") if case["master"] else 0)
    s.ptr(s.g("g_online"), w("onlinesys") if case["online"] else 0)
    s.ptr(w("onlinesys") + s.o["themelist"], w("themelist"))
    s.words(s.g("g_rowh"), [case["rowh"]])
    s.write(s.g("g_rowinit"), bytes([case["rowinit"] & 0xFF]))
    s.i16(s.g("g_screenh"), case["screenh"])
    s.lst = 0
    if case["op"] == "fill":
        parks = case["parks"]
        nodes = [s.alloc(max(s.o["nsize"], 12)) for _ in parks]
        for i, p in enumerate(parks):
            pa = w("park%d" % i)
            s.ptr(pa + s.o["pdata"], s.wtext(p["text"]))
            s.words(pa + s.o["p4c"], [p["theme"]])
            s.words(pa + s.o["p50"], [p["f50"]])
            s.words(pa + s.o["p54"], [p["f54"]])
            s.words(pa + s.o["pb8"], [p["fb8"]])
            s.ptr(nodes[i], pa)
            s.ptr(nodes[i] + s.o["nnext"], nodes[i + 1] if i + 1 < len(parks) else 0)
            s.ptr(nodes[i] + s.o["nprev"], nodes[i - 1] if i else 0)
        s.lst = s.alloc(max(s.o["listsize"], 0x10) + 16)
        s.ptr(s.lst + s.o["listhead"], nodes[0] if parks else 0)
        s.words(s.lst + s.o["listcount"], [case["count"]])


def execute(s, case):
    op = case["op"]
    if op == "fmt":
        return s.fmt_call()
    if op == "post":
        return s.post_call(s.wptr("w%d" % case["row"]))
    if op == "msg":
        return s.msg_call(s.wptr("w%d" % case["win"]), *case["args"])
    return s.fill_call(s.lst)


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(),
            "panel": s.label(s.rd_ptr(s.g("g_panel"))), "master": s.label(s.rd_ptr(s.g("g_master"))),
            "online": s.label(s.rd_ptr(s.g("g_online"))), "close": s.rd_ptr(s.g("g_close")) != 0,
            "rowh": s.rd_word(s.g("g_rowh")), "rowinit": s.read(s.g("g_rowinit"), 1)}


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


# ---------------------------------------------------------------------------------------------------------------- case generators


def base(r, op, queues=None):
    q = {"GetWindow": [1], "CallDefault": [r.randrange(-1000, 1000)], "GetHeight": [r.randrange(-40, 80)]}
    q.update(queues or {})
    return {"op": op, "queues": q, "panel": r.choice([0, 1]), "close": r.choice([0, 1]), "master": r.choice([0, 1]),
            "online": r.choice([0, 1]), "rowh": r.choice([0, 1, -1, r.randrange(-100, 200)]),
            "rowinit": r.choice([0, 1, 1, 0x80, 0xFF, 5]), "screenh": r.choice([1, 600, 768, -768, 1080, 0x7FFF, -1])}


# ---- LCFormat
def fmt_case(r, height=None, rowh=None, rowinit=None, screenh=None):
    c = base(r, "fmt")
    if height is not None:
        c["queues"]["GetHeight"] = [height]
    if rowh is not None:
        c["rowh"] = rowh
    if rowinit is not None:
        c["rowinit"] = rowinit
    if screenh is not None:
        c["screenh"] = screenh
    return c


def fmt_edges():
    r = random.Random(101)
    for rowinit in (0, 1, 0x80, 0xFF):
        for rowh in (0, 7, -3):
            yield fmt_case(r, rowh=rowh, rowinit=rowinit)
    for height in (0, 1, -1, 0x7FFF, 0x8000, 0xFFFF, 0x12345, -0x8000, 20, 0x1FFFF):
        for screenh in (1, 600, 768, -768, 0x7FFF, -1):
            yield fmt_case(r, height=height, rowh=0, rowinit=1, screenh=screenh)


def fmt_random(r):
    return fmt_case(r, height=r.choice([r.randrange(-70000, 70000), r.randrange(-40, 80)]))


# ---- LCPostFormat
def post_case(r, row=None):
    c = base(r, "post")
    c["row"] = r.randrange(8) if row is None else row
    return c


def post_edges():
    r = random.Random(102)
    for row in range(8):
        yield post_case(r, row)


def post_random(r):
    return post_case(r)


# ---- OnlineParkListPanelMsg
MSGS = [0x100, 0x14, 0x15, 0x13, 0, -1, 0x7FFFFFFF, 0xFF, 0x101, 0x3FF, 0x400, 0x401, 0x1000A, 0x1000B, 0x1000C, 0x10000, 0x16,
        -0x80000000]
ARGS = [-2, 0x1B, 0x14BEB, 0, 1, -1, 0x1C]
BARGS = [-1, 0, 5]


def msg_case(r, msg=None, a=None, b=None, win=None, gw=None, close=None):
    c = base(r, "msg")
    c["win"] = r.randrange(8) if win is None else win
    msg = r.choice(MSGS) if msg is None else msg
    a = r.choice(ARGS) if a is None else a
    b = r.choice(BARGS + [r.randrange(-5, 100)]) if b is None else b
    c["args"] = [msg, a, b]
    if gw is not None:
        c["queues"]["GetWindow"] = [gw]
    if close is not None:
        c["close"] = close
    return c


def msg_edges():
    r = random.Random(103)
    for msg in MSGS:
        for a in ARGS:
            for b in BARGS:
                yield msg_case(r, msg, a, b)
    for msg, a in ((0x100, -2), (0x1000B, 0x1B), (0x400, 0x14BEB), (0x14, 0)):
        for close in (0, 1):
            for gw in (0, 1):
                for b in (-1, 3):
                    yield msg_case(r, msg, a, b, gw=gw, close=close)


def msg_random(r):
    return msg_case(r)


# ---- FillParkList
TEXTS = ["", "A", "Park", "Wonder Land", "été", "中文", "x" * 12, "耀a", "Swear"]


def fill_case(r, n=None, count=None, master=None, online=None, sw=None, ch=None):
    n = r.randrange(5) if n is None else n
    c = base(r, "fill")
    c["panel"] = 1
    c["parks"] = [{"text": r.choice(TEXTS), "theme": r.choice([0, 1, 2, 3, -1, -2, 7, r.randrange(-100, 100)]),
                   "f50": r.randrange(-5, 1000), "f54": r.choice([0, -1, r.randrange(-50000, 50000)]),
                   "fb8": r.choice([0, 1, r.randrange(-300, 300)])} for _ in range(n)]
    c["count"] = r.choice([n, n, n, n + 1, n - 1, 0, 1, -1]) if count is None else count
    if master is not None:
        c["master"] = master
    if online is not None:
        c["online"] = online
    c["queues"]["IsSwearFilterOn"] = [r.choice([0, 1])] if sw is None else sw
    c["queues"]["FilterChanges"] = [r.choice([0, 1])] if ch is None else ch
    c["queues"]["Loc"] = [r.randrange(0, 12) for _ in range(max(n, 1))]
    return c


def fill_edges():
    r = random.Random(104)
    yield fill_case(r, 0, 0)
    yield fill_case(r, 0, 3)
    for master in (0, 1):
        for online in (0, 1):
            for sw in ([0], [1], [1, 0], [0, 1]):
                for ch in ([0], [1]):
                    yield fill_case(r, 2, 2, master, online, sw, ch)
    for n in range(5):
        for count in (n, n + 1, n - 1, 1, 0, -1):
            yield fill_case(r, n, count)
    yield fill_case(r, 4, 4, 1, 1, [1], [1])


def fill_random(r):
    return fill_case(r)


TESTS = [
    FnTest("LCFormat", ppc, host, edge=fmt_edges, rnd=fmt_random),
    FnTest("LCPostFormat", ppc, host, edge=post_edges, rnd=post_random),
    FnTest("OnlineParkListPanelMsg", ppc, host, edge=msg_edges, rnd=msg_random),
    FnTest("FillParkList", ppc, host, edge=fill_edges, rnd=fill_random),
]
