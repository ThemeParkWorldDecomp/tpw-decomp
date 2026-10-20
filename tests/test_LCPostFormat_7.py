"""Unit v5930, the park find panel's list functions, against libtpw_unit_LCPostFormat_7.dylib (tests/unit_LCPostFormat_7.cpp), main app
("SimTheme Park"), src/OnlineParkFindPanelMsg_LCPostFormat_7.cpp:
  LCFormat 0x101e96b0, LCPostFormat 0x101e9874, FillOnlineParkFindList 0x101e9bf0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, 64 bit pointers, the host's own offsets) for the translation; the same
code reads both back. Every callee outside the unit is a fake on both sides that records its arguments (windows, fonts and parks as labels,
texts as a length-first hash) and answers from the case: the window and draw object makers, the setters, the list control, the master
save and online system makers, the swear filter, the theme and city lookups. The MSL string (constructor, assign, destructor, wcslen)
is modelled by silent fakes on the PowerPC side (a word pointing at an object whose word at 0xc points at the characters); on the host
it is a std::u16string. The two virtual calls through __ptr_glue (the font height, the window's SetValue) are told apart by their TVector.

The world of a case: LCFormat: the guard byte, the row height, the screen height and the font height the virtual answers; LCPostFormat:
six window blocks and the queue of GetChild / GetNext answers; FillOnlineParkFindList: the panel, the master save and the online system
(present or to be made), a list of parks (name, theme, city, three numbers) whose count word may differ from the nodes. What is compared:
the answer (LCFormat: that it is the window made), the guard byte, the row height, the globals the fill function may make (as
none / the existing block / a new one) and the whole event log.

No fused operations. Not tested: a screen height of 0 (the original divides by it, the hardware result is undefined); park names over
80 characters (the fakes hash 80).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LCPostFormat_7"
MASK = 0xFFFFFFFF

FN_FORMAT, FN_POST, FN_FILL = 0x101E96B0, 0x101E9874, 0x101E9BF0
SIZES = {FN_FORMAT: 420, FN_POST: 840, FN_FILL: 748}
OWN = set(SIZES)

ADDR = dict(panel=0x103960C8, game=0x1037F134, master=0x10380A8C, os=0x10380AB0, screen=0x10395BBA,
            rowinit=0x1039609E, rowheight=0x103960A0)
WIDE = 0x1024F01E            # gOnlineParkFindWide
M_FONT, M_SETV = 0x7F001000, 0x7F002000      # the TVectors the two virtual calls go through

MAC = dict(text=0x14, theme=0x4C, city=0x48, fb8=0xB8, f50=0x50, f54=0x54, park_size=0xC0, head=4, count=0xC, list_size=0x10,
           next=4, node_size=0xC, city_name=0x14, city_size=0x68, os_theme=0xF8, os_city=0xE0, os_size=0x404, data=0, ptr=4)
HLAY = []                    # the host's offsets, read once from tpw_opf_layout
HOST_KEYS = ["text", "theme", "city", "fb8", "f50", "f54", "park_size", "head", "count", "list_size", "next", "node_size",
             "city_name", "city_size", "os_theme", "os_city", "os_size", "data"]
CUR = {}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- text helpers

def units_hash(units):
    h = len(units)
    for c in units:
        h = (h * 31 + c) & 0x7FFFFFFF
    return h


def read_units(e, p):
    units = struct.unpack(">81H", e.read(p, 162))
    n = 0
    while n < 80 and units[n]:
        n += 1
    return list(units[:n])


def cell_hash(e, p):
    if p == 0:
        return -1
    if p == WIDE:
        return -2
    return units_hash(read_units(e, p))


def cstr_bytes(e, p):
    data = e.read(p, 81)
    n = 0
    while n < 80 and data[n]:
        n += 1
    return data[:n]


def cstr_hash(b):
    h = len(b)
    for c in b:
        h = (h * 31 + c) & 0x7FFFFFFF
    return h


def put_units(e, addr, units):
    e.write(addr, struct.pack(">%dH" % (len(units) + 1), *(list(units) + [0])))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def W():
    return CUR["w"]


def stub(addr, fn):
    STUBS.internal(addr, fn)


def s_new(e):
    size = e.r(3)
    e.cl.rec("new", (size,))
    e.set_r(3, e.alloc(size))


def s_font(e):
    e.cl.rec("GetFontTableEntry", (e.r(3),))
    e.set_r(3, W().font)


def s_glue(e):
    L = e.cl
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))      # the glue saves the TOC; the caller reloads r2 from there
    r12 = e.r(12)
    if r12 == M_FONT:
        L.rec("FontHeight", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("FontHeight") & MASK)
    elif r12 == M_SETV:
        L.rec("WindowSetValue", (L.lab(e.r(3)), s32(e.r(4))))
    else:
        raise AssertionError("unknown TVector %x" % r12)


def s_getwindow(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    i = s32(e.r(4))
    e.set_r(3, W().list if i == 0xE05B else W().button if i == 0xE059 else 0)


def s_getchild(e):
    L = e.cl
    L.rec("GetChild", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("GetChild") & MASK)


def s_getnext(e):
    L = e.cl
    L.rec("GetNext", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetNext") & MASK)


def s_setcdo(e):
    e.cl.rec("SetClientDrawObject", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def s_setdo(e):
    e.cl.rec("SetDrawObject", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def s_setfont(e):
    e.cl.rec("SetFont", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def s_settext(e):
    e.cl.rec("SetClientDrawText", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def s_fontjust(e):
    e.cl.rec("SetFontJust", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def s_create(e):
    L = e.cl
    L.rec("CreateObject", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), T.s16(e.r(7)), T.s16(e.r(8)), T.s16(e.r(9)),
                           T.s16(e.r(10))))
    e.set_r(3, 0)


def s_removeall(e):
    e.cl.rec("RemoveAllItems", (e.cl.lab(e.r(3)),))


def s_additem(e):
    L = e.cl
    words = struct.unpack(">6I", e.read(e.r(4), 24))
    L.rec("AddItem", (L.lab(e.r(3)), L.lab(e.r(5)), s32(e.r(6)), s32(e.r(7)), cell_hash(e, words[0]), cell_hash(e, words[1]),
                      cell_hash(e, words[2]), s32(words[3]), s32(words[4]), s32(words[5])))
    e.set_r(3, 0)


def s_setstate(e):
    e.cl.rec("SetState", (e.cl.lab(e.r(3)), T.s8(e.r(4)), T.s8(e.r(5))))


def s_getstring(e):
    e.cl.rec("UIMain_GetString", (s32(e.r(3)),))
    e.set_r(3, W().string)


def s_swear(e):
    e.cl.rec("IsSwearFilterOn", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("Swear") & 0xFF)


def s_filter(e):
    L = e.cl
    obj = e.u32(e.r(4))
    chars = e.u32(obj + 0xC)
    units = read_units(e, chars)
    L.rec("Filter", (L.lab(e.r(3)), units_hash(units), e.r(5) & 0xFF))
    mod = L.ret("FilterMod")
    if mod != 0 and units:
        e.write(chars, struct.pack(">H", 0x2A))


def s_strctor(e):
    obj = e.alloc(0x20)
    chars = e.alloc(0x200)
    e.write(obj + 0xC, struct.pack(">I", chars))
    e.write(e.r(3), struct.pack(">I", obj))


def s_strassign(e):
    obj = e.u32(e.r(3))
    chars = e.u32(obj + 0xC)
    n = e.r(5)
    assert n <= 100, n
    e.write(chars, e.read(e.r(4), 2 * n) + b"\0\0")


def s_nothing(e):
    pass


def s_wcslen(e):
    p = e.r(3)
    units = struct.unpack(">120H", e.read(p, 240))
    n = 0
    while n < 120 and units[n]:
        n += 1
    e.set_r(3, n)


def s_themename(e):
    L = e.cl
    L.rec("ReturnOnlineThemeName", (L.lab(e.r(3)), s32(e.r(4))))
    a = e.alloc(0x100)
    e.write(a, b"Th%d\0" % (s32(e.r(4)) & 7))
    e.set_r(3, a)


def s_cstrctor(e):
    text = cstr_bytes(e, e.r(4))
    e.cl.rec("CStrCtor", (cstr_hash(text),))
    e.write(e.r(3), b"\0" * 0x14)
    e.write(e.r(3) + 8, struct.pack(">I", e.r(4)))


def s_cstrdtor(e):
    e.cl.rec("CStrDtor", ())


def s_wstrctor(e):
    e.cl.rec("WStrCtor", ())
    e.write(e.r(3), b"\0" * 0x14)


def s_wstrdtor(e):
    e.cl.rec("WStrDtor", ())


def s_localised(e):
    L = e.cl
    name = cstr_bytes(e, e.u32(e.r(4) + 8))
    L.rec("GetLocalisedThemeName", (L.lab(e.r(3)), cstr_hash(name)))
    a = e.alloc(0x100)
    put_units(e, a, [0x4C] + list(name))
    e.write(e.r(5) + 8, struct.pack(">I", a))


def s_cityinfo(e):
    L = e.cl
    L.rec("ReturnCityInfo", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, W().city1 if (s32(e.r(4)) & 1) else W().city0)


def mk_ctor(name, spec):
    return CL.recorder(name, spec)


_win_ctor = mk_ctor("WindowCtor", "NLiihhhh")
_os_ctor = mk_ctor("NewOnlineSystem", "N")


def s_winctor(e):
    W().lastwin = e.r(3)
    _win_ctor(e)


def s_osctor(e):
    obj = e.r(3)
    _os_ctor(e)
    e.write(obj + 0xF8, struct.pack(">I", W().themelist))
    e.write(obj + 0xE0, struct.pack(">I", W().citylist))
    e.set_r(3, obj)


for _addr, _fn in [
    (0x102384B8, s_new), (0x10180270, s_font), (0x1023BAA8, s_glue), (0x101DBE40, s_getwindow), (0x101DBAC4, s_getchild),
    (0x101DBB38, s_getnext), (0x101DB624, s_setcdo), (0x101DB474, s_setdo), (0x101DA118, s_setfont), (0x101DE130, s_settext),
    (0x101DA204, s_fontjust), (0x101DC750, s_create), (0x101D3CC8, s_removeall), (0x101D3000, s_additem), (0x101CB1D0, s_setstate),
    (0x10180468, s_getstring), (0x1016B438, s_swear), (0x1020D2CC, s_filter), (0x101E0B84, s_strctor), (0x101B2510, s_strassign),
    (0x101468F8, s_nothing), (0x102389C8, s_wcslen), (0x1017A588, s_themename), (0x102382D8, s_cstrctor), (0x102382F0, s_cstrdtor),
    (0x10238740, s_wstrctor), (0x10238710, s_wstrdtor), (0x1014A478, s_localised), (0x10178B3C, s_cityinfo),
    (0x101D98D8, s_winctor), (0x1016F7D0, s_osctor),
    (0x1018D11C, mk_ctor("SpriteCtor", "N")), (0x1018D930, mk_ctor("NumberCtor", "N")), (0x1018F93C, mk_ctor("RankCtor", "N")),
    (0x101694DC, mk_ctor("NewMasterSave", "N")),
]:
    stub(_addr, _fn)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides

class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def put16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "h", v))

    def text(self, units):
        a = self.alloc(max(0x100, 2 * len(units) + 2))
        self.write(a, struct.pack(self.fmt[0] + "%dH" % (len(units) + 1), *(list(units) + [0])))
        return a

    def make_world(self, case):
        """Allocates the blocks every function may name; labels in self.lab (name -> label)."""
        o = self.O
        self.font = self.alloc(0x40)
        fobj = self.alloc(0x20)
        self.words(self.font + 0x10, [fobj])
        self.words(fobj + 0xC, [M_FONT])
        table = self.alloc(0x40)
        self.words(table + 0x24, [M_SETV])
        self.wins = []
        for _ in range(6):
            w = self.alloc(0x130)
            self.words(w + 0x118, [table])
            self.wins.append(w)
        self.panel, self.list, self.button, self.master = (self.alloc(0x40) for _ in range(4))
        self.os = self.alloc(o["os_size"])
        self.themelist, self.citylist = self.alloc(0x20), self.alloc(0x20)
        self.city0, self.city1 = self.alloc(o["city_size"]), self.alloc(o["city_size"])
        self.string = self.text(case.get("nothing", [0x6E, 0x6F]))
        for i, city in enumerate((self.city0, self.city1)):
            self.ptr(city + o["city_name"], self.text(case.get("city%d" % i, [0x43, 0x30 + i])))
        self.ptr(self.os + o["os_theme"], self.themelist)
        self.ptr(self.os + o["os_city"], self.citylist)
        self.game = None
        self.parks = []
        nodes = []
        for p in case.get("parks", []):
            park = self.alloc(o["park_size"])
            self.ptr(park + o["text"], self.text(p["name"]))
            self.words(park + o["theme"], [p["theme"]])
            self.words(park + o["city"], [p["city"]])
            self.words(park + o["fb8"], [p["fb8"]])
            self.words(park + o["f50"], [p["f50"]])
            self.words(park + o["f54"], [p["f54"]])
            self.parks.append(park)
            nodes.append(self.alloc(o["node_size"]))
        for i, node in enumerate(nodes):
            self.ptr(node + o["data"], self.parks[i])
            self.ptr(node + o["next"], nodes[i + 1] if i + 1 < len(nodes) else 0)
        self.plist = self.alloc(o["list_size"])
        self.ptr(self.plist + o["head"], nodes[0] if nodes else 0)
        self.words(self.plist + o["count"], [case.get("count", len(nodes))])

    def labels(self):
        """address -> label for every object the log may name."""
        d = {self.panel: 1, self.list: 2, self.button: 3, self.master: 4, self.os: 5, self.themelist: 6, self.citylist: 7,
             self.city0: 8, self.city1: 9, self.font: 10, self.string: 12}
        for i, w in enumerate(self.wins):
            d[w] = 20 + i
        for i, p in enumerate(self.parks):
            d[p] = 40 + i
        return d

    def queues(self, case):
        q = {k: list(v) for k, v in case["queues"].items()}
        q["GetChild"] = [self.wins[1]]
        q["GetNext"] = self.wins[2:6]
        return q


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"
    O = MAC

    def __init__(self, e, case):
        self.e = e
        self.lastwin = 0
        self.ptr = lambda a, v: self.words(a, [v])
        self.make_world(case)
        fixed = {a: l for a, l in self.labels().items()}
        fixed[ADDR["game"]] = 11
        self.L = e.cl = CL.PpcLog(fixed, self.queues(case))
        CUR["w"] = self
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

    def rd_ptr(self, a):
        return self.rd_word(a)

    def publish(self):
        pass

    def lastnew(self):
        return self.lastwin

    def call_format(self):
        return T.call_ppc(self.e, FN_FORMAT, [])[0] & MASK

    def call_post(self, w):
        T.call_ppc(self.e, FN_POST, [w])
        return 0

    def call_fill(self):
        T.call_ppc(self.e, FN_FILL, [self.plist])
        return 0


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.O = dict(zip(HOST_KEYS, HLAY))
        self.ptr = lambda a, v: self.write(a, struct.pack("<Q", v))
        self.make_world(case)
        fixed = {l: a for a, l in self.labels().items()}
        fixed[11] = self.g("game")
        for i in range(16):
            self.ptr(self.g("parks") + 8 * i, self.parks[i] if i < len(self.parks) else 0)
        CL.host_begin(lib, fixed, self.queues(case))

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_opf_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_opf_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def publish(self):
        for name, v in (("font", self.font), ("list", self.list), ("button", self.button), ("themelist", self.themelist),
                        ("citylist", self.citylist), ("city0", self.city0), ("city1", self.city1), ("string", self.string),
                        ("lastwin", 0)):
            self.ptr(self.g(name), v)

    def lastnew(self):
        return self.rd_ptr(self.g("lastwin"))

    def call_format(self):
        return self.lib.tpw_opf_format() & 0xFFFFFFFFFFFFFFFF

    def call_post(self, w):
        self.lib.tpw_opf_post(w)
        return 0

    def call_fill(self):
        self.lib.tpw_opf_fill(self.plist)
        return 0


def bind(lib):
    """The host library's entry points, typed once; the layout is read here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_opf_addr.restype, lib.tpw_opf_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_opf_alloc.restype, lib.tpw_opf_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_opf_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_opf_format.restype, lib.tpw_opf_format.argtypes = C.c_uint64, []
    lib.tpw_opf_post.restype, lib.tpw_opf_post.argtypes = None, [C.c_uint64]
    lib.tpw_opf_fill.restype, lib.tpw_opf_fill.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_opf_layout(out)
    assert n == len(HOST_KEYS), n
    HLAY[:] = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

def build(s, case):
    s.publish()
    ptr = lambda name, v: s.ptr(s.g(name), v)
    if case["op"] == "format":
        s.write(s.g("rowinit"), bytes([case["rowinit"]]))
        s.words(s.g("rowheight"), [case["rowheight"]])
        s.put16(s.g("screen"), case["screen"])
    elif case["op"] == "fill":
        ptr("panel", s.panel if case["panel"] else 0)
        ptr("master", s.master if case["master"] else 0)
        ptr("os", s.os if case["os"] else 0)


def execute(s, case):
    if case["op"] == "format":
        return s.call_format()
    if case["op"] == "post":
        return s.call_post(s.wins[0])
    return s.call_fill()


def cls(s, v):
    return 0 if v == 0 else 1 if v in (s.master, s.os) else 2


def observe(s, case, answer):
    out = {"log": s.log()}
    if case["op"] == "format":
        out["ok"] = 1 if answer != 0 and (answer & MASK) == (s.lastnew() & MASK) else 0
        out["rowinit"] = s.read(s.g("rowinit"), 1)
        out["rowheight"] = s.rd_word(s.g("rowheight"))
    elif case["op"] == "fill":
        out["master"] = cls(s, s.rd_ptr(s.g("master")))
        out["os"] = cls(s, s.rd_ptr(s.g("os")))
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


# ---------------------------------------------------------------------------------------------------------------- case generators

def units(r, n=None):
    n = r.choice([0, 1, 2, 5, 12, 30]) if n is None else n
    return [r.choice([0x41, 0x62, 0x20, 0x2A, 0xE9, 0x3042, 0xFFFF, r.randrange(1, 0x10000)]) for _ in range(n)]


def base(r, op):
    return {"op": op, "queues": {"FontHeight": [r.choice([8, 12, 16, 0x1000A, 0xFFFF, 1, 100])],
                                 "Swear": [r.choice([0, 1, 0x100, 0x101])], "FilterMod": [r.choice([0, 1])]}}


def format_case(r, rowinit=None, rowheight=None, screen=None, height=None):
    c = base(r, "format")
    c["rowinit"] = r.choice([0, 1, 1, 0x80, 0xFF, 2]) if rowinit is None else rowinit
    c["rowheight"] = r.choice([0, 0, 40, -1, 1000, 7]) if rowheight is None else rowheight
    c["screen"] = r.choice([768, 1536, 480, 1, -1, 7, 2000, -300]) if screen is None else screen
    if height is not None:
        c["queues"]["FontHeight"] = [height]
    return c


def format_edges():
    r = random.Random(5930)
    for init in (0, 1, 0x80, 0xFF):
        for rh in (0, 40, -1):
            yield format_case(r, init, rh)
    for screen in (1, -1, 1536, 768, 32767, -32768):
        for h in (1, 12, 0x1000A, 0xFFFF, 0x8000, 0, 0x7FFF):
            yield format_case(r, 1, 0, screen, h)


def format_random(r):
    return format_case(r)


def post_edges():
    r = random.Random(5931)
    for _ in range(3):
        yield base(r, "post")


def post_random(r):
    return base(r, "post")


def park(r, name=None):
    return {"name": units(r) if name is None else name, "theme": r.choice([0, 1, 2, 3, 4, 7, 8, -1, 0x1234, r.randrange(0, 100)]),
            "city": r.choice([0, 1, 2, 3, -1, r.randrange(0, 100)]), "fb8": r.choice([0, 1, -1, 0x7FFFFFFF, r.randrange(0, 100000)]),
            "f50": r.choice([0, 5, -5, r.randrange(-1000, 1000)]), "f54": r.choice([0, 9, -9, r.randrange(-1000, 1000)])}


def fill_case(r, nparks=None, panel=True, master=None, os=None, count=None):
    c = base(r, "fill")
    n = r.choice([0, 1, 1, 2, 3, 5]) if nparks is None else nparks
    c["parks"] = [park(r) for _ in range(n)]
    c["panel"] = panel
    c["master"] = r.random() < 0.5 if master is None else master
    c["os"] = r.random() < 0.5 if os is None else os
    if count is not None:
        c["count"] = count
    c["queues"]["Swear"] = [r.choice([0, 1, 0x100, 0x101]) for _ in range(max(n, 1))]
    c["queues"]["FilterMod"] = [r.choice([0, 1]) for _ in range(max(n, 1))]
    c["nothing"] = units(r, r.choice([0, 3, 9]))
    return c


def fill_edges():
    r = random.Random(5932)
    yield fill_case(r, 2, panel=False)
    for n in (0, 1, 2, 4):
        for master in (False, True):
            for os in (False, True):
                yield fill_case(r, n, True, master, os)
    yield fill_case(r, 2, True, True, True, count=0)
    yield fill_case(r, 2, True, True, True, count=1)
    yield fill_case(r, 2, True, True, True, count=5)
    yield fill_case(r, 0, True, True, True, count=3)
    c = fill_case(r, 2, True, True, True)
    c["queues"]["Swear"] = [1]
    c["queues"]["FilterMod"] = [1]
    c["parks"][0]["name"] = []
    yield c
    c = fill_case(r, 3, True, False, False)
    c["queues"]["Swear"] = [0x100, 1, 0x101]
    yield c


def fill_random(r):
    return fill_case(r)


TESTS = [
    FnTest("LCFormat", ppc, host, edge=format_edges, rnd=format_random),
    FnTest("LCPostFormat", ppc, host, edge=post_edges, rnd=post_random),
    FnTest("FillOnlineParkFindList", ppc, host, edge=fill_edges, rnd=fill_random),
]
