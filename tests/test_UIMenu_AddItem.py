"""Unit v5163, UIMenu::Init and UIMenu::AddItem, against libtpw_unit_UIMenu_AddItem.dylib (tests/unit_UIMenu_AddItem.cpp), main app
("SimTheme Park"), src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/InterfaceWindow.cpp, src/game/ui/UIMenu.cpp, src/game/ui/UI_Base.cpp:
  UIMenu::Init 0x1018ad20, UIMenu::AddItem 0x1018ae78, with the real InterfaceStringHeap::GetAndCopy 0x101d7b60, GetString 0x101d79ec,
  ReleaseString 0x101d7b10 and UIMenu_DefaultItemCallback 0x1018aa48 (on destroy) under AddItem

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit (UI_MeasureText, the CoolRect constructor and destructor, GetPosition, CreateObject, SetCallbackFunction,
setExtraParam, getExtraParam, the default window handler, operator new, the title text constructor, SetClientDrawObject, GetFontTableEntry, SetFont, SetFontJust, the virtual
SetText, SetFontColour, rand, the window constructor, FindDrawObject, SetDrawObject, Show) are fakes on both sides that record their
arguments and answer from the case's queues.

The string heap is not a fake: wcslen and wcscpy are plain on both sides and the item keeps what the real GetAndCopy answers, so a stand-in
that answers the caller's text fails here. Every row of the heap has a label (10 up for table A, 50 up for table B), so the log shows
which row an item took.

The world of a case: the menu object (the handler word, the counter, two unread words, the shown word, the window), the two screen size
globals, the text, the heap's six globals and two tables, the window and the window CreateObject answers. What is compared: the answer,
the menu's words, the handler (as a number 0 to 3) and the window (as a label) after the call, the screen size globals, the heap's tables
and globals, the caller's text and the event log. The script of a case runs AddItem and the item's handler on destroy in any order, so
the rows run out, come back and the caller's text is seen to survive the release.

No fused operations. Not tested: a screen size of 0 (divw by 0 is undefined on the host), and a handler word that is not one of the
three known handlers.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UIMenu_AddItem"
MASK = 0xFFFFFFFF
TABLE_HALVES, TEXT_HALVES, MAX_ROWS = 0x100, 0x40, 30   # MAX_ROWS keeps the row labels under calllog's first new label

FN_INIT, FN_ADD = 0x1018AD20, 0x1018AE78
SIZES = {FN_INIT: 260, FN_ADD: 572}
OWN = set(SIZES)

MEASURE, RECT_CTOR, RECT_DTOR, GET_POSITION, CREATE_OBJECT = 0x1018B8E4, 0x101C7448, 0x101C74E0, 0x101DB884, 0x101DC750
SET_CALLBACK, SET_EXTRA, NEW, TITLE_CTOR = 0x101DCBA8, 0x101DCFD0, 0x102384B8, 0x1018FBE8
GET_AND_COPY, GET_STRING, RELEASE_STRING, FN_ITEM = 0x101D7B60, 0x101D79EC, 0x101D7B10, 0x1018AA48
GET_EXTRA, CALL_DEFAULT, WCSLEN, WCSCPY = 0x101DD030, 0x101DE318, 0x102389C8, 0x10238C80
SET_CLIENT, GET_FONT, SET_FONT, SET_JUST, PTR_GLUE = 0x101DB624, 0x10180270, 0x101DA118, 0x101DA204, 0x1023BAA8
SET_COLOUR, RAND, WINDOW_CTOR, FIND, SET_DRAW, SHOW = 0x101DA3AC, 0x10239148, 0x101D98D8, 0x101CD2D4, 0x101DB474, 0x101DC1EC

ADDR = dict(width=0x10395BBC, height=0x10395BBA, TableB=0x10395C34, TableA=0x10395C38, RowsB=0x10395C3C, WidthB=0x10395C40,
            RowsA=0x10395C44, WidthA=0x10395C48)
HOST_LAYOUT = [0, 4, 8, 12, 16, 24]
# the Mac's handler addresses for the fake handler numbers 1 to 3
CBADDR = {0: 0, 1: 0x10268608, 2: 0x10268628, 3: 0x10268650}
CBNUM = {v: k for k, v in CBADDR.items()}
LOLIGHT = 0x1024DB68


def pv(L, v):
    """A pointer-sized value: small numbers as they are, objects as labels."""
    v &= MASK
    return v if v < 0x100000 else L.lab(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN | {GET_AND_COPY, GET_STRING, RELEASE_STRING, FN_ITEM})


def st_measure(e):
    L = e.cl
    rect = struct.unpack(">4i", e.read(e.r(4), 16))
    L.rec("MeasureText", (L.lab(e.r(3)),) + rect + (T.s8(e.r(5)), T.s8(e.r(6)), T.s8(e.r(7)), T.s32(e.r(8))))
    e.write(e.r(9), struct.pack(">4i", *[T.s32(L.ret(k)) for k in ("mt0", "mt1", "mt2", "mt3")]))


def st_rect_ctor(e):
    e.cl.rec("RectCtor", (0,))


def st_rect_dtor(e):
    e.cl.rec("RectDtor", (T.s32(e.r(4)),))


def st_get_position(e):
    L = e.cl
    L.rec("GetPosition", (L.lab(e.r(3)), T.s16(e.r(5))))
    e.write(e.r(4) + 4, struct.pack(">4h", *[T.s16(L.ret(k)) for k in ("gp0", "gp1", "gp2", "gp3")]))
    e.set_r(3, 0)


def st_create(e):
    L = e.cl
    L.rec("CreateObject", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), T.s16(e.r(7)), T.s16(e.r(8)),
                           T.s16(e.r(9)), T.s16(e.r(10))))
    e.set_r(3, e.newwin if L.ret("create") != 0 else 0)


def st_set_callback(e):
    L = e.cl
    L.rec("SetCallback", (L.lab(e.r(3)), CBNUM.get(e.r(4), 9)))


def st_set_extra(e):
    L = e.cl
    L.rec("setExtraParam", (L.lab(e.r(3)), T.s16(e.r(4)), pv(L, e.r(5))))
    if T.s16(e.r(4)) == 1:
        e.extra1 = e.r(5) & MASK


def st_get_extra(e):
    L = e.cl
    L.rec("getExtraParam", (L.lab(e.r(3)), T.s16(e.r(4))))
    e.set_r(3, e.extra1 if T.s16(e.r(4)) == 1 else 0)


def st_call_default(e):
    L = e.cl
    L.rec("CallDefault", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))))
    e.set_r(3, L.ret("default") & MASK)


def glue_save(e):
    """The import glue saves the caller's TOC at 0x14(r1) (the caller reloads r2 from there)."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def st_wcslen(e):
    glue_save(e)
    a, n = e.r(3), 0
    while e.read(a + 2 * n, 2) != b"\0\0":
        n += 1
    e.set_r(3, n)


def st_wcscpy(e):
    glue_save(e)
    dst, src, n = e.r(3), e.r(4), 0
    while True:
        ch = e.read(src + 2 * n, 2)
        e.write(dst + 2 * n, ch)
        n += 1
        if ch == b"\0\0":
            break


def st_new(e):
    glue_save(e)
    L = e.cl
    L.rec("new", (e.r(3),))
    e.set_r(3, e.alloc(e.r(3)))


def st_set_client(e):
    L = e.cl
    L.rec("SetClientDrawObject", (L.lab(e.r(3)), pv(L, e.r(4))))


def st_get_font(e):
    L = e.cl
    L.rec("GetFontTableEntry", (e.r(3) & MASK,))
    e.set_r(3, L.ret("font") & MASK)


def st_set_font(e):
    L = e.cl
    L.rec("SetFont", (L.lab(e.r(3)), pv(L, e.r(4))))


def st_set_just(e):
    L = e.cl
    L.rec("SetFontJust", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5))))


def st_ptr_glue(e):
    glue_save(e)
    L = e.cl
    L.rec("SetText", (L.lab(e.r(3)), L.lab(e.r(4))))


def st_set_colour(e):
    L = e.cl
    L.rec("SetFontColour", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), T.s32(e.r(7))))


def st_rand(e):
    glue_save(e)
    L = e.cl
    L.rec("rand", (0,))
    e.set_r(3, L.ret("rand") & MASK)


def st_find(e):
    L = e.cl
    L.rec("FindDrawObject", (1 if e.cstr(e.r(3)) == "lolight" else 0,))
    e.set_r(3, L.ret("find") & MASK)


def st_set_draw(e):
    L = e.cl
    L.rec("SetDrawObject", (L.lab(e.r(3)), pv(L, e.r(4))))


def st_show(e):
    L = e.cl
    L.rec("Show", (L.lab(e.r(3)), T.s16(e.r(4))))
    e.set_r(3, 0)


for _addr, _fn in ((MEASURE, st_measure), (RECT_CTOR, st_rect_ctor), (RECT_DTOR, st_rect_dtor), (GET_POSITION, st_get_position),
                   (CREATE_OBJECT, st_create), (SET_CALLBACK, st_set_callback),
                   (SET_EXTRA, st_set_extra), (GET_EXTRA, st_get_extra), (CALL_DEFAULT, st_call_default),
                   (WCSLEN, st_wcslen), (WCSCPY, st_wcscpy), (NEW, st_new), (SET_CLIENT, st_set_client), (GET_FONT, st_get_font),
                   (SET_FONT, st_set_font), (SET_JUST, st_set_just), (PTR_GLUE, st_ptr_glue), (SET_COLOUR, st_set_colour),
                   (RAND, st_rand), (FIND, st_find), (SET_DRAW, st_set_draw), (SHOW, st_show)):
    STUBS.internal(_addr, _fn)
STUBS.internal(WINDOW_CTOR, CL.recorder("WindowCtor", "NLiihhhh"))
STUBS.internal(TITLE_CTOR, CL.recorder("TitleCtor", "Ni"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def halves(self, a, hs):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(hs), *hs))

    def rd_halves(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def rows(self, case):
        """The label of every heap row."""
        labels = {}
        for k in range(case["RowsA"]):
            labels[self.ta + 2 * k * case["WidthA"]] = 10 + k
        for k in range(case["RowsB"]):
            labels[self.tb + 2 * k * case["WidthB"]] = 50 + k
        return labels

    def world(self, case):
        """The blocks every case has: menu, its window, the window CreateObject answers, the text."""
        self.menu = self.alloc(0x20)
        self.win = self.alloc(0x140)
        self.newwin = self.alloc(0x140)
        self.text = self.alloc(2 * TEXT_HALVES)
        self.ta = self.alloc(2 * TABLE_HALVES)
        self.tb = self.alloc(2 * TABLE_HALVES)
        self.begin(case)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.saved = None
        self.world(case)

    def begin(self, case):
        e = self.e
        e.newwin = self.newwin
        vt = e.alloc(0x40)
        self.words(self.newwin + 0x118, [vt])
        self.words(self.win + 0x118, [vt])
        e.extra1 = 0
        self.L = e.cl = CL.PpcLog({self.menu: 1, self.win: 2, self.newwin: 3, self.text: 4, **self.rows(case)}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        if self.saved is not None:
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

    f16 = ">h"

    def handle(self, k):
        return CBADDR[k]

    def handle_index(self, h):
        return CBNUM.get(h, 9)

    def set_window(self, p):
        self.words(self.menu + 0x10, [p])

    def ptr(self, a, p):
        self.words(a, [p])

    def final(self):
        m4 = T.s32(self.rd_word(self.menu + 4))
        self.L.rec("Final", (self.handle_index(self.rd_word(self.menu)), m4, self.L.lab(self.rd_word(self.menu + 0x10))))

    def call_init(self, h, i):
        T.call_ppc(self.e, FN_INIT, [self.menu, CBADDR[h], CBADDR[i]])
        self.final()
        return 0

    def call_add(self, pos, id_, a, b, c, d):
        r3, _, _ = T.call_ppc(self.e, FN_ADD, [self.menu, pos & MASK, self.text, id_ & MASK, a & MASK, b & MASK, c & MASK, d & MASK])
        self.final()
        return r3 & MASK

    def call_destroy(self, a, b):
        r3, _, _ = T.call_ppc(self.e, FN_ITEM, [self.newwin, 0x14, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    f16 = "<h"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.world(case)

    def begin(self, case):
        labels = {1: self.menu, 2: self.win, 3: self.newwin, 4: self.text}
        labels.update({label: a for a, label in self.rows(case).items()})
        CL.host_begin(self.lib, labels, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ua_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ua_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def handle(self, k):
        return self.lib.tpw_ua_handle(k)

    def handle_index(self, h):
        return self.lib.tpw_ua_handle_index(h)

    def set_window(self, p):
        self.write(self.menu + 0x10, struct.pack("<Q", p))

    def ptr(self, a, p):
        self.write(a, struct.pack("<Q", p))

    def call_init(self, h, i):
        self.lib.tpw_ua_init(self.menu, h, i)
        return 0

    def call_add(self, pos, id_, a, b, c, d):
        return self.lib.tpw_ua_additem(self.menu, self.newwin, self.text, s32(pos), s32(id_), s32(a), s32(b), s32(c), s32(d)) & MASK

    def call_destroy(self, a, b):
        return self.lib.tpw_ua_item_destroy(self.newwin, s32(a), s32(b)) & MASK


def s32(v):
    return T.s32(v)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ua_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ua_handle.restype, lib.tpw_ua_handle.argtypes = C.c_uint32, [C.c_int32]
    lib.tpw_ua_handle_index.restype, lib.tpw_ua_handle_index.argtypes = C.c_int32, [C.c_uint32]
    lib.tpw_ua_init.restype, lib.tpw_ua_init.argtypes = None, [C.c_uint64, C.c_int32, C.c_int32]
    lib.tpw_ua_additem.restype = C.c_uint32
    lib.tpw_ua_additem.argtypes = [C.c_uint64] * 3 + [C.c_int32] * 6
    lib.tpw_ua_item_destroy.restype, lib.tpw_ua_item_destroy.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ua_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.menu, [s.handle(case["h"]), case["m4"], case["m8"], case["shown"]])
    s.set_window(s.win if case["has_win"] else 0)
    s.write(s.g("width"), struct.pack(s.f16, case["width"]))
    s.write(s.g("height"), struct.pack(s.f16, case["height"]))
    s.halves(s.ta, case["ta"])
    s.halves(s.tb, case["tb"])
    s.halves(s.text, case["src"] + [0] * (TEXT_HALVES - len(case["src"])))
    for name in ("RowsA", "WidthA", "RowsB", "WidthB"):
        s.words(s.g(name), [case[name]])
    s.ptr(s.g("TableA"), s.ta if case["tableA"] else 0)
    s.ptr(s.g("TableB"), s.tb if case["tableB"] else 0)


def execute(s, case):
    if case["op"] == "init":
        return s.call_init(*case["args"])
    answers = []
    for step in case["script"]:
        answers.append(s.call_add(*case["args"]) if step == "add" else s.call_destroy(case["args"][2], case["args"][3]))
    return answers


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "words": s.rd_words(s.menu + 4, 2), "shown": s.rd_word(s.menu + 0xC),
            "ta": s.rd_halves(s.ta, TABLE_HALVES), "tb": s.rd_halves(s.tb, TABLE_HALVES), "text": s.rd_halves(s.text, TEXT_HALVES),
            "heap": [s.rd_word(s.g(n)) for n in ("RowsA", "WidthA", "RowsB", "WidthB")],
            "dims": struct.unpack(s.f16[0] + "2h",s.read(s.g("width"), 2) + s.read(s.g("height"), 2)),
            "handler": s.handle_index(s.rd_word(s.menu))}


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


DIMS = [640, 480, 1024, 768, 1, -1, 2048, 800, 600, 3, 32767, -32768, 17]
SPOTS = [0, 1, -1, 5, 100, 2000, -3000, 0x7FFF, -0x8000, 0x8000, 0x12345, 0x7FFFFFF0]


def rv(r, wide=False):
    k = r.random()
    if k < 0.25:
        return r.choice(SPOTS)
    return r.randrange(-3000, 3000) if not wide else r.randrange(-(1 << 22), 1 << 22)


def table(r, rows, width, free):
    """A table's halves: a row is free when its first two halves are 0xFFFF; garbage in the rest, one half of 0xFFFF on some used rows."""
    t = [r.randrange(0x20, 0x7F) for _ in range(TABLE_HALVES)]
    for k in range(rows):
        if r.random() < free:
            t[k * width], t[k * width + 1] = 0xFFFF, 0xFFFF
        elif r.random() < 0.3:
            t[k * width] = 0xFFFF
    return t


def heap(r, free=None, tables=(True, True)):
    """The heap's half of a case: widthB < widthA, as Allocate requires, sizes that fit the blocks, and a text of any length."""
    width_b = r.choice([5, 6, 8, 16])
    width_a = width_b + r.choice([1, 4, 16, 32])
    rows_b = r.randrange(0, min(MAX_ROWS, TABLE_HALVES // width_b) + 1) if r.random() < 0.2 else r.randrange(1, 5)
    rows_a = r.randrange(0, min(MAX_ROWS, TABLE_HALVES // width_a) + 1) if r.random() < 0.2 else r.randrange(1, 5)
    f = r.choice([0.0, 0.3, 1.0]) if free is None else free
    length = r.choice([0, 1, width_b - 1, width_b, width_a - 1, width_a, r.randrange(0, TEXT_HALVES)])
    return {"RowsA": rows_a, "WidthA": width_a, "RowsB": rows_b, "WidthB": width_b, "ta": table(r, rows_a, width_a, f),
            "tb": table(r, rows_b, width_b, f), "tableA": tables[0], "tableB": tables[1],
            "src": [r.randrange(0x20, 0x7F) for _ in range(min(length, TEXT_HALVES - 1))]}


def base(r, op):
    c = {"op": op, "h": r.choice([0, 1, 2, 3]), "m4": r.choice([0, 1, -1, r.randrange(0, 50), 0x7FFFFFFF]),
         "m8": r.randrange(0, 1 << 32), "shown": r.choice([0, 1, r.randrange(0, 1 << 32)]),
         "has_win": True, "width": r.choice(DIMS), "height": r.choice(DIMS), "queues": {}}
    c.update(heap(r))
    return c


SCRIPTS = (("add",), ("add", "destroy"), ("add", "destroy", "add", "destroy"), ("add", "add", "add", "add", "add", "add", "destroy"),
           ("add", "add", "destroy", "add", "destroy", "destroy"))


def add_case(r, create=None, h=None, dims=None, script=None, hp=None):
    c = base(r, "add")
    if hp is not None:
        c.update(hp)
    c["script"] = list(script if script is not None else r.choice(SCRIPTS))
    adds = c["script"].count("add")
    if h is not None:
        c["h"] = h
    if dims is not None:
        c["width"], c["height"] = dims
    c["queues"] = {"mt0": [rv(r)] * adds, "mt1": [rv(r)] * adds, "mt2": [rv(r)] * adds, "mt3": [rv(r)] * adds,
                   "gp0": [T.s16(rv(r))] * adds, "gp1": [T.s16(rv(r))] * adds, "gp2": [T.s16(rv(r))] * adds, "gp3": [T.s16(rv(r))] * adds,
                   "create": [r.choice([0, 1, 1]) if create is None else create] * adds,
                   "font": [r.choice([0, 5, 0xFFFFF, r.randrange(0, 0x100000)])] * adds,
                   "default": [r.choice([0, 1, -1, r.randrange(0, 0x100000)])] * c["script"].count("destroy")}
    c["args"] = [rv(r, True) & MASK, rv(r) & MASK, r.choice([0, 1, 6, 0x7F, 0x80, 0xFF, 0x100, 0xFFFFFFFF, r.randrange(0, 1 << 32)]),
                 r.randrange(0, 256), r.randrange(0, 256), r.randrange(0, 256)]
    return c


def add_edges():
    r = random.Random(5163)
    for create in (0, 1):
        for h in (0, 1, 2, 3):
            yield add_case(r, create, h)
    for dims in ((640, 480), (1, 1), (-1, -1), (32767, -32768)):
        yield add_case(r, 1, None, dims)
    for _ in range(6):
        yield add_case(r, 1)
    # the release: the row comes back and the caller's text keeps its first characters, with or without a copy
    for tables in ((True, True), (True, False), (False, True), (False, False)):
        for free in (0.0, 1.0):
            for script in SCRIPTS:
                yield add_case(r, 1, None, None, script, heap(r, free, tables))
    # the text fits table B, only table A, or neither; the rows run out and come back
    fit = dict(RowsA=2, WidthA=9, RowsB=2, WidthB=5, tableA=True, tableB=True)
    for length in (0, 3, 4, 5, 8, 9, 10, 40):
        for script in SCRIPTS:
            hp = heap(r, 1.0)
            hp.update(fit)
            hp["src"] = [0x41 + k % 26 for k in range(length)]
            hp["ta"], hp["tb"] = table(r, 2, 9, 1.0), table(r, 2, 5, 1.0)
            yield add_case(r, 1, None, None, script, hp)


def add_random(r):
    return add_case(r)


def init_case(r, has_win=None):
    c = base(r, "init")
    c["has_win"] = r.random() < 0.2 if has_win is None else has_win
    c["queues"] = {"rand": [r.choice([0, 1, 0x7FFFFFFF, r.randrange(0, 0x80000000)]) for _ in range(3)],
                   "find": [r.choice([0, 7, r.randrange(0, 0x100000)])]}
    c["args"] = [r.choice([0, 1, 2, 3]), r.choice([0, 1, 2, 3])]
    return c


def init_edges():
    r = random.Random(5164)
    for has_win in (False, True):
        for h in (0, 1, 2, 3):
            for i in (0, 1, 3):
                c = init_case(r, has_win)
                c["args"] = [h, i]
                yield c
    c = init_case(r, False)
    c["queues"]["rand"] = [0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF]
    yield c


def init_random(r):
    return init_case(r)


TESTS = [
    FnTest("UIMenu::AddItem", ppc, host, edge=add_edges, rnd=add_random),
    FnTest("UIMenu::Init", ppc, host, edge=init_edges, rnd=init_random),
]
