"""Unit v5991, the online contacts panel's receive-string, list handler and buddy helpers, against libtpw_unit_OnlineReceiveString_2.dylib
(tests/unit_OnlineReceiveString_2.cpp), main app ("SimTheme Park"), src/ContactsLCFormat_OnlineReceiveString_2.cpp:
  OnlineReceiveString 0x101e7dd8, ContactListHandler 0x101e7854, _OnlineAddBuddy 0x101e839c, _OnlineRemoveBuddy 0x101e845c,
  _OnlineUpdateBuddy 0x101e8524

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (GetWindow, the font
table and its character width virtual, GetColumnPos, AddItem, RemoveItem, SetCurrent, FindString, SetData, UpdateDisplay, the fills, the
online control, the postcard, CallDefaultWindowHandler) are fakes on both sides that record their arguments (texts as offset, length and
hash) and answer from the case's queues; AddItem and RemoveItem keep the list's item count like the real ones.

The world of a case: the panel pointer (NULL or set), the busy flag, the font index, the screen width, the row limit, the item count, the
control pointer, a text buffer. What is compared: the event log, the globals, the item count, the text buffer (the terminator the code
writes temporarily must be restored), whether the control pointer is NULL, the old one or a new one, the answer of the handler.

No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_OnlineReceiveString_2"
MASK = 0xFFFFFFFF

RECV, HANDLER, ADDB, REMB, UPDB = 0x101E7DD8, 0x101E7854, 0x101E839C, 0x101E845C, 0x101E8524
GETWINDOW, GETFONT, COLPOS, ADDITEM = 0x101DBE40, 0x10180270, 0x101D42EC, 0x101D3000
REMOVEITEM, SETCURRENT, FINDSTRING, SETDATA, UPDATEDISPLAY = 0x101D39F0, 0x101D4C64, 0x101D44B8, 0x101D3FCC, 0x101D2220
FILLCONTACT, FILLBUDDY, CTOR, EARON, EAROFF, VOTE = 0x101E82DC, 0x101E8608, 0x10213870, 0x10219424, 0x1021952C, 0x10218B44
POSTCARD, CALLDEFAULT, PTRGLUE = 0x101B232C, 0x101DE318, 0x1023BAA8
SIZES = {RECV: 612, HANDLER: 452, ADDB: 152, REMB: 156, UPDB: 184}

ADDR = dict(font=0x102B0C4C, maxrows=0x102B0C54, width=0x10395BBC, busy=0x10395E7C, panel=0x10395E98, control=0x10396D28)
COUNT_OFF = 0x150

STUBS = CL.Stubs()
STUBS.real.update(SIZES)

ST = {}   # per case on the PowerPC side: FONT, PANEL, WIN, LIST, BASE


def text_info(e, ptr):
    n, h = 0, 0
    while True:
        c = struct.unpack(">H", e.read(ptr + 2 * n, 2))[0]
        if c == 0:
            return n, h
        h = (h * 31 + c) & 0x7FFFFFFF
        n += 1


def stub_getwindow(e):
    L = e.cl
    this, wid = e.r(3), s32(e.r(4))
    L.rec("GetWindow", (L.lab(this), wid))
    m = {ST["PANEL"]: ST["WIN"], ST["WIN"]: ST["LIST"], ST["LIST"]: ST["PANEL"]}
    e.set_r(3, m.get(this, 0))


def s32(v):
    return T.s32(v)


def stub_getfont(e):
    e.cl.rec("GetFontTableEntry", (e.r(3) & MASK,))
    e.set_r(3, ST["FONT"])


def stub_colpos(e):
    L = e.cl
    L.rec("GetColumnPos", (L.lab(e.r(3)), s32(e.r(4))))
    e.write(e.r(5), struct.pack(">h", T.s16(L.ret("colL"))))
    e.write(e.r(6), struct.pack(">h", T.s16(L.ret("colR"))))
    e.set_r(3, 1)


def bump(e, lst, d):
    a = lst + COUNT_OFF
    e.write(a, struct.pack(">I", (e.u32(a) + d) & MASK))


def stub_additem(e):
    L = e.cl
    ptr = e.u32(e.r(4))
    n, h = text_info(e, ptr)
    L.rec("AddItem", (L.lab(e.r(3)), (ptr - ST["BASE"]) // 2, n, h, s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))
    bump(e, e.r(3), 1)
    e.set_r(3, 0)


def stub_removeitem(e):
    L = e.cl
    L.rec("RemoveItem", (L.lab(e.r(3)), s32(e.r(4))))
    bump(e, e.r(3), -1)
    e.set_r(3, 0)


def stub_findstring(e):
    L = e.cl
    ptr = e.r(4)
    n, h = text_info(e, ptr)
    L.rec("FindString", (L.lab(e.r(3)), (ptr - ST["BASE"]) // 2, n, h, s32(e.r(5))))
    e.set_r(3, L.ret("FindString") & MASK)


def stub_ptrglue(e):
    L = e.cl
    if e.r(3) == ST["FONT"]:
        L.rec("CharWidth", (e.r(4) & 0xFFFF,))
        e.set_r(3, L.ret("CharWidth") & MASK)
    else:
        L.rec("WindowSlot1c", (L.lab(e.r(3)), s32(e.r(4))))


def stub_wcslen(e):
    ptr, n = e.r(3), 0
    while struct.unpack(">H", e.read(ptr + 2 * n, 2))[0] != 0:
        n += 1
    e.set_r(3, n)


STUBS.internal(GETWINDOW, stub_getwindow)
STUBS.internal(GETFONT, stub_getfont)
STUBS.internal(COLPOS, stub_colpos)
STUBS.internal(ADDITEM, stub_additem)
STUBS.internal(REMOVEITEM, stub_removeitem)
STUBS.internal(SETCURRENT, CL.recorder("SetCurrent", "Li"))
STUBS.internal(FINDSTRING, stub_findstring)
STUBS.internal(SETDATA, CL.recorder("SetData", "Lii"))
STUBS.internal(UPDATEDISPLAY, CL.recorder("UpdateDisplay", "L"))
STUBS.internal(FILLCONTACT, CL.recorder("FillContactList", ""))
STUBS.internal(FILLBUDDY, CL.recorder("FillBuddyList", ""))
STUBS.internal(CTOR, CL.recorder("ctor", "N"))
STUBS.internal(EARON, CL.recorder("EarmuffsOn", "L"))
STUBS.internal(EAROFF, CL.recorder("EarmuffsOff", "L"))
STUBS.internal(VOTE, CL.recorder("VoteForThisPark", "L"))
STUBS.internal(POSTCARD, CL.recorder("UI_DoPostcard", ""))
STUBS.internal(CALLDEFAULT, CL.recorder("CallDefault", "Liii", ret="i"))
STUBS.internal(PTRGLUE, stub_ptrglue)
STUBS.imp("wcslen", stub_wcslen)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "h", v))

    def chars(self, a, cs):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(cs), *cs))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.panel, self.win, self.list, self.ctl = (e.alloc(0x800) for _ in range(4))
        font, table = e.alloc(0x40), e.alloc(0x40)
        wtable = e.alloc(0x40)
        e.write(font + 0x10, struct.pack(">I", table))
        for w in (self.panel, self.win, self.list):
            e.write(w + 0x118, struct.pack(">I", wtable))
        ST.update(FONT=font, PANEL=self.panel, WIN=self.win, LIST=self.list, BASE=0)
        self.L = e.cl = CL.PpcLog({self.panel: 1, self.win: 2, self.list: 3, self.ctl: 4}, case["queues"])
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

    def set_base(self, a):
        ST["BASE"] = a

    def set_panel(self, on):
        self.words(ADDR["panel"], [self.panel if on else 0])

    def set_ctl(self, on):
        self.words(ADDR["control"], [self.ctl if on else 0])

    def ctl_state(self):
        v = self.rd_word(ADDR["control"])
        return 0 if v == 0 else (1 if v == self.ctl else 2)

    def set_count(self, n):
        self.words(self.list + COUNT_OFF, [n])

    def get_count(self):
        return s32(self.rd_word(self.list + COUNT_OFF))

    def call(self, entry, args):
        return T.call_ppc(self.e, entry, [a & MASK for a in args])[0] & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.panel, self.win, self.list, self.ctl = (lib.tpw_fx_alloc(0x800) for _ in range(4))
        lib.tpw_fx_setobjs(self.panel, self.win, self.list)
        CL.host_begin(lib, {1: self.panel, 2: self.win, 3: self.list, 4: self.ctl}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fx_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_fx_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_base(self, a):
        self.lib.tpw_fx_setbase(a)

    def set_panel(self, on):
        self.lib.tpw_fx_setptr(0, self.panel if on else 0)

    def set_ctl(self, on):
        self.lib.tpw_fx_setptr(1, self.ctl if on else 0)

    def ctl_state(self):
        v = self.lib.tpw_fx_getptr(1)
        return 0 if v == 0 else (1 if v == self.ctl else 2)

    def set_count(self, n):
        self.words(self.list + self.lib.countoff, [n])

    def get_count(self):
        return s32(self.rd_word(self.list + self.lib.countoff))

    def call(self, entry, args):
        lib, a = self.lib, args
        if entry == RECV:
            lib.tpw_fx_recv(a[0], s32(a[1]))
            return 0
        if entry == HANDLER:
            return lib.tpw_fx_handler(a[0], s32(a[1]), s32(a[2]), s32(a[3])) & MASK
        if entry == ADDB:
            lib.tpw_fx_add(a[0], s32(a[1]))
        elif entry == REMB:
            lib.tpw_fx_remove(a[0])
        else:
            lib.tpw_fx_update(a[0], s32(a[1]))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    U64, I32 = C.c_uint64, C.c_int32
    lib.tpw_fx_addr.restype, lib.tpw_fx_addr.argtypes = U64, [C.c_char_p]
    lib.tpw_fx_alloc.restype, lib.tpw_fx_alloc.argtypes = U64, [C.c_uint32]
    lib.tpw_fx_setptr.argtypes = [I32, U64]
    lib.tpw_fx_getptr.restype, lib.tpw_fx_getptr.argtypes = U64, [I32]
    lib.tpw_fx_setobjs.argtypes = [U64, U64, U64]
    lib.tpw_fx_setbase.argtypes = [U64]
    lib.tpw_fx_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fx_recv.argtypes = [U64, I32]
    lib.tpw_fx_handler.restype, lib.tpw_fx_handler.argtypes = I32, [U64, I32, I32, I32]
    lib.tpw_fx_add.argtypes = [U64, I32]
    lib.tpw_fx_remove.argtypes = [U64]
    lib.tpw_fx_update.argtypes = [U64, I32]
    out = (C.c_int64 * 32)()
    lib.tpw_fx_layout(out)
    lib.countoff = out[0]
    assert out[1] <= 0x800, out[1]
    lib.bound = True


def build(s, case):
    s.words(s.g("font"), [case["font"]])
    s.words(s.g("maxrows"), [case["maxrows"]])
    s.words(s.g("busy"), [case["busy"]])
    s.half(s.g("width"), case["width"])
    s.set_panel(case["panel"])
    s.set_ctl(case["ctl"])
    s.set_count(case["count"])
    cs = case["chars"]
    s.nbuf = len(cs) + 8
    s.buf = s.alloc(2 * s.nbuf)
    s.chars(s.buf, cs + [0] * 8)
    s.set_base(s.buf)


def execute(s, case):
    op = case["op"]
    if op == "recv":
        return s.call(RECV, [s.buf, case["data"]])
    if op == "handler":
        return s.call(HANDLER, [s.panel] + case["args"])
    if op == "add":
        return s.call(ADDB, [s.buf, case["data"]])
    if op == "remove":
        return s.call(REMB, [s.buf])
    return s.call(UPDB, [s.buf, case["data"]])


def observe(s, case, answer):
    return {"answer": answer if case["op"] == "handler" else 0, "log": s.log(), "font": s.rd_word(s.g("font")), "maxrows": s.rd_word(s.g("maxrows")),
            "busy": s.rd_word(s.g("busy")), "count": s.get_count(), "ctl": s.ctl_state(), "text": list(struct.unpack(s.fmt[0] + "%dH" % s.nbuf, s.read(s.buf, 2 * s.nbuf)))}


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


def base(r, op, chars=None, **kw):
    widths = [r.choice([0, 5, 8, 12, 15, 20, r.randrange(0, 40), 0x18000 + r.randrange(0, 30), 0xFFF0]) for _ in range(60)]
    lo = r.randrange(0, 100)
    c = {"op": op, "font": r.choice([0, 7, r.randrange(0, 12)]), "maxrows": r.choice([0, 1, 5, 20, r.randrange(0, 15)]),
         "width": r.choice([640, 800, 1024, r.randrange(0, 1400)]), "busy": r.choice([0, 1, 1, 1, 2, -1]),
         "panel": r.random() < 0.85, "ctl": r.randrange(2), "count": r.randrange(0, 15), "data": r.choice([0, 1, -1, r.randrange(-5000, 5000)]),
         "chars": chars if chars is not None else [r.choice([1, 65, 0x20, r.randrange(1, 0x300)]) for _ in range(r.randrange(0, 41))],
         "args": [0, 0, 0],
         "queues": {"CharWidth": widths, "colL": [lo], "colR": [lo + r.choice([-50, 0, 100, 300, 500, r.randrange(-100, 700)])],
                    "FindString": [r.choice([-1, 0, 3, 7, -5, r.randrange(-3, 20)])], "CallDefault": [r.choice([0, 1, -1, 77])]}}
    c.update(kw)
    return c


def recv_case(r):
    return base(r, "recv")


def small(r):
    return [r.randrange(1, 0x300) for _ in range(r.randrange(1, 9))]


def recv_edges():
    r = random.Random(11)
    for chars in ([], [65], [65, 66], [65] * 5, [0x20] * 30, [r.randrange(1, 300) for _ in range(40)]):
        for widths in ([0], [5], [10], [0x18000 + 7], [300]):
            for colr, width in ((100, 640), (0, 640), (-30, 800), (500, 1024), (2000, 1400)):
                c = base(r, "recv", list(chars), panel=True, width=width)
                c["queues"]["CharWidth"] = widths
                c["queues"]["colL"], c["queues"]["colR"] = [0], [colr]
                yield c
    yield base(r, "recv", [65, 66], panel=False)
    for mr in (0, 1, 3, 20, 100):
        for cnt in (0, 1, 3, 20, 25):
            yield base(r, "recv", [65] * 12, panel=True, maxrows=mr, count=cnt)


def handler_case(r, ctl=None):
    c = base(r, "handler")
    c["args"] = [r.choice([0x100, 0x101, 0xFF, 0x102, 0]), r.choice([0xC3125, 0xC3126, 0xC3127, 0xC3128, 0xC3129, 0xC312A, 0xC312B, 0]),
                 r.choice([0, 1, 2, -1]), 0]
    c["args"][3] = c["args"][2]
    c["args"] = [c["args"][0], c["args"][1], c["args"][2]]
    return c


def handler_edges():
    r = random.Random(12)
    for m in (0x100, 0x101, 0xFF, 0x102, 0, -1):
        for a in (0xC3123, 0xC3125, 0xC3126, 0xC3127, 0xC3128, 0xC3129, 0xC312A, 0xC312B, 0):
            for b in (0, 1, 2, -1):
                for ctl in (0, 1):
                    c = base(r, "handler", ctl=ctl)
                    c["args"] = [m, a, b]
                    yield c


def buddy_case(op):
    def f(r):
        return base(r, op, small(r))
    return f


def buddy_edges(op):
    def f():
        r = random.Random(13)
        for panel in (False, True):
            for busy in (0, 1, 2, -1):
                for found in (-1, 0, 3, -5):
                    c = base(r, op, small(r), panel=panel, busy=busy)
                    c["queues"]["FindString"] = [found]
                    yield c
    return f


TESTS = [
    FnTest("OnlineReceiveString", ppc, host, edge=recv_edges, rnd=recv_case),
    FnTest("ContactListHandler", ppc, host, edge=handler_edges, rnd=handler_case),
    FnTest("_OnlineAddBuddy", ppc, host, edge=buddy_edges("add"), rnd=buddy_case("add")),
    FnTest("_OnlineRemoveBuddy", ppc, host, edge=buddy_edges("remove"), rnd=buddy_case("remove")),
    FnTest("_OnlineUpdateBuddy", ppc, host, edge=buddy_edges("update"), rnd=buddy_case("update")),
]
