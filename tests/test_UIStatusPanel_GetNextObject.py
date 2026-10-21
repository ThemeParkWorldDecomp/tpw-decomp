"""Unit v5090, the UIStatusPanel members GetNextObject, FillObjectList, EraseObject, OpenWindow and Move, against
libtpw_unit_UIStatusPanel_GetNextObject.dylib (tests/unit_UIStatusPanel_GetNextObject.cpp), main app ("SimTheme Park"),
src/ui_StatusPanel_SaveTags_UIStatusPanel_GetNextObject.cpp:
  GetNextObject 0x10192c28, FillObjectList 0x10192a90, EraseObject 0x10193138, OpenWindow 0x101935a0, Move 0x101938d8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the iterators, the
dialog, the cell system, the mouse and its modes, the window members, operator new, __ptr_glue) are fakes on both sides that record their
arguments and answer from the case's queues. Objects are named by labels (the panel 1, the game window 2, the windows 3 to 6, the things 7 to
9, the ride record 10, the mouse 11, objects the code makes 100 + their order). The panel's virtual calls are logged as the table offset
(the Mac's __ptr_glue gets the entry's value, the host's slot function knows its own offset).

The world of a case: the panel (mode, words, windows, table), gStaffRoomPanelThing, the counters and flags, the mouse globals, the thing
and ride blocks, gPlayerSettings' confirm byte. What is compared: the answer, the panel's words, the globals, the objects made, the log.

No fused operations. Not tested: a NULL gStaffRoomPanelThing in EraseObject and Move, a NULL second window where the original calls Enable
on it (the original reads through a NULL), both are crashes.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UIStatusPanel_GetNextObject"
MASK = 0xFFFFFFFF

FN_GET, FN_FILL, FN_ERASE, FN_OPEN, FN_MOVE = 0x10192C28, 0x10192A90, 0x10193138, 0x101935A0, 0x101938D8
SIZES = {FN_GET: 388, FN_FILL: 348, FN_ERASE: 308, FN_OPEN: 388, FN_MOVE: 496}
OWN = set(SIZES)

ADDR = dict(gThing=0x103947A4, gCount=0x103947A0, gMouse=0x102DC1F4, gWord1E0=0x102DC1E0, gWord1E4=0x102DC1E4, gDir=0x102E47F0,
            gMoveObj=0x102E47F4, gGame=0x103949E4, gConfirm=0x1038071C + 0x37)
TV = 0x6F000000
DELME, ITERCB, VIEWCB = 0x102679E8, 0x102679F0, 0x10267998
# panel fields (offsets 0, 4, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x20), the table size, the confirm byte, the thing's cell bytes
HOST_LAYOUT = [0, 4, 0xC, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x37, 4, 6]

L_PANEL, L_GAME, L_WMAIN, L_W14, L_W18, L_W10, L_T0, L_T1, L_T2, L_RIDE, L_MOUSE = range(1, 12)
WINDOWS = (L_GAME, L_WMAIN, L_W14, L_W18, L_W10)
THINGS = (L_T0, L_T1, L_T2)
MODES = [-100, -3, -2, -1, 0, 1, 2, 3, 4, 5, 0x3E9, 0x3EA, 0x3EB, 0x3EC, 0x3ED, 0x7FFFFFFF]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lab(e, v):
    return e.W.lab(v & MASK)


def rec(e, name, *vals):
    e.cl.rec(name, tuple(vals))


def fnid(v, known):
    v &= MASK
    return 0 if v == 0 else (1 if v == known else -1)


def st_glue(e):
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))   # the glue saves the TOC pointer where the caller reloads it
    tv = (e.r(12) & MASK) - TV
    if tv >> 20 == 2:
        rec(e, "virt", lab(e, e.r(3)), tv, e.r(4) & 0xFFFF)
    else:
        rec(e, "virt", lab(e, e.r(3)), tv)


def st_getstring(e):
    rec(e, "GetString", s32(e.r(3)))
    e.set_r(3, e.cl.ret("GetString") & MASK)


def st_dialog(e):
    p = e.r(3)
    rgb = e.read(p + 4, 3)
    rec(e, "DialogBox", e.u32(p), rgb[0], rgb[1], rgb[2], s32(e.u32(p + 8)), lab(e, e.u32(p + 12)), s32(e.r(4)), fnid(e.r(5), DELME),
        s32(e.r(6)), fnid(e.r(7), DELME), s32(e.r(8)), fnid(e.r(9), DELME))
    rec(e, "DialogFlag", e.r(10) & 0xFF)
    e.set_r(3, 0)


def st_setselected(e):
    rec(e, "SetSelected", lab(e, e.r(3)))


def st_setcurrent(e):
    rec(e, "SetCurrent", e.r(3) & 0xFF)


def st_setmode(e):
    rec(e, "SetMode", s32(e.r(3)), e.r(4) & 0xFF)


def st_buttonup(e):
    rec(e, "ButtonUp", s32(e.r(3)), s32(e.r(4)), e.r(5) & 0xFFFF, s32(e.r(6)))


def st_iterate(e):
    rec(e, "Iterate", s32(e.r(3)), fnid(e.r(4), ITERCB))
    e.write(ADDR["gCount"], struct.pack(">I", e.cl.ret("Iterate") & MASK))
    e.set_r(3, 0)


def st_wrapnext(e):
    rec(e, "WrapNext", s32(e.r(3)), lab(e, e.r(4)))
    e.set_r(3, e.W.addr_of(e.cl.ret("WrapNext")))


def st_checkpanel(e):
    rec(e, "CheckPanel")
    e.set_r(3, 0)


def st_setpanel(e):
    rec(e, "SetPanel", lab(e, e.r(3)))


def st_build(e):
    rec(e, "Build", lab(e, e.r(3)), e.r(4) & MASK, e.r(5) & MASK)
    e.set_r(3, e.W.addr_of(e.cl.ret("Build")))


def st_getwindow(e):
    rec(e, "GetWindow", lab(e, e.r(3)), s32(e.r(4)))
    e.set_r(3, e.W.addr_of(e.cl.ret("GetWindow")))


def st_setcallback(e):
    rec(e, "SetCallback", lab(e, e.r(3)), fnid(e.r(4), VIEWCB))


def st_extraparam(e):
    rec(e, "ExtraParam", lab(e, e.r(3)), T.s16(e.r(4)), lab(e, e.r(5)))
    e.set_r(3, 0)


def st_getdbinfo(e):
    rec(e, "GetDBInfo", lab(e, e.r(3)))
    e.set_r(3, e.W.addr_of(e.cl.ret("GetDBInfo")))


def st_constructq(e):
    rec(e, "ConstructQ", lab(e, e.r(3)))
    e.set_r(3, e.cl.ret("ConstructQ") & MASK)


def st_makecopy(e):
    rec(e, "MakeCopy")


def st_setride(e):
    rec(e, "SetRide", struct.unpack(">H", e.read(e.r(3), 2))[0], s32(e.r(4)))


def st_rotation(e):
    rec(e, "Rotation", s32(e.r(3)), e.r(4) & 0xFF)


def st_new(e):
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
    n = e.r(3) & MASK
    a =e.alloc(max(n, 4))
    e.W.newlist.append(a)
    e.set_r(3, a)


def st_pathdraw(e):
    rec(e, "PathDraw", lab(e, e.r(3)), s32(e.r(4)), (e.r(5) >> 16) & 0xFFFF)


def st_default(e):
    rec(e, "Default", lab(e, e.r(3)))


def st_assign(e):
    rec(e, "Assign", lab(e, e.r(3)), lab(e, e.r(4)))


for _a, _f in [(0x1023BAA8, st_glue), (0x10180468, st_getstring), (0x101F1800, st_dialog), (0x10194768, st_setselected),
               (0x10088590, st_setcurrent), (0x10090E18, st_setmode), (0x10084CA4, st_buttonup), (0x101F7DB0, st_iterate),
               (0x101FA778, st_wrapnext), (0x10180514, st_checkpanel), (0x101805A0, st_setpanel), (0x101DEB78, st_build),
               (0x101DBE40, st_getwindow), (0x101DCBA8, st_setcallback), (0x101DCFD0, st_extraparam), (0x10107CD4, st_getdbinfo),
               (0x10092644, st_constructq), (0x10091ED0, st_makecopy), (0x100915BC, st_setride), (0x10090D40, st_rotation),
               (0x102384B8, st_new), (0x1007E744, st_pathdraw), (0x1007EA78, st_default), (0x1007E2E0, st_assign)]:
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def p32(self, a):
        """The word a Ptr32 field holds for the address (the Mac: the address, the host: the arena offset)."""
        return a

    def unp32(self, w):
        return w

    def wptr(self, a, v):
        """A native pointer global (4 bytes on the Mac, 8 on the host)."""
        self.words(a, [v])

    def rptr(self, a):
        return self.rd_word(a)

    def addr_of(self, label):
        return self.addr.get(label, 0)

    def lab(self, v, assign=True):
        """The world's label; an object the code made is numbered 100, 101 ... in the order the log first names it."""
        if v == 0:
            return 0
        if v in self.rev:
            return self.rev[v]
        if v in self.lazy:
            return self.lazy[v]
        if assign and v in self.newlist:
            self.lazy[v] = 100 + len(self.lazy)
            return self.lazy[v]
        return -1

    def register(self, label, a):
        self.addr[label] = a
        self.rev[a] = label


class Ppc(Side):
    fmt = ">I"
    has_queue = 0x40

    def __init__(self, e, case):
        self.e = e
        self.addr, self.rev, self.newlist, self.lazy = {}, {}, [], {}
        e.W = self
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    def init_window(self, a):
        table = self.alloc(0x40)
        self.words(table + 0x1C, [TV + 0x20001C])
        self.words(a + 0x118, [table])

    def slot_entry(self, off):
        return TV + 0x100000 + off

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_ui_begin()
        self.base = lib.tpw_ui_base()
        self.has_queue = lib.has_queue
        self.addr, self.rev, self.newlist, self.lazy = {}, {}, [], {}
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ui_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ui_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def lab(self, v, assign=True):
        return 0 if v == 0 else self.lib.tpw_ui_label_of(v)

    def p32(self, a):
        return 0 if a == 0 else a - self.lib.tpw_ui_base()

    def unp32(self, w):
        return 0 if w == 0 else w + self.lib.tpw_ui_base()

    def wptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def register(self, label, a):
        Side.register(self, label, a)
        self.lib.tpw_ui_label(label, a)

    def init_window(self, a):
        self.lib.tpw_ui_init_window(a)

    def slot_entry(self, off):
        return self.lib.tpw_ui_slot_block(off)

    def call(self, fn, args):
        lib = self.lib
        if fn == FN_GET:
            lib.tpw_ui_get_next(args[0])
        elif fn == FN_FILL:
            return lib.tpw_ui_fill(args[0], args[1]) & MASK
        elif fn == FN_ERASE:
            lib.tpw_ui_erase(args[0])
        elif fn == FN_OPEN:
            lib.tpw_ui_open(args[0], args[1] & MASK, args[2] & MASK, s32(args[3]), s32(args[4]), s32(args[5]))
        else:
            lib.tpw_ui_move(args[0])
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ui_"
    u64, u32, i32 = C.c_uint64, C.c_uint32, C.c_int32
    f = getattr(lib, p + "addr"); f.restype, f.argtypes = u64, [C.c_char_p]
    f = getattr(lib, p + "alloc"); f.restype, f.argtypes = u64, [u32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    f = getattr(lib, p + "begin"); f.restype, f.argtypes = None, []
    f = getattr(lib, p + "label"); f.restype, f.argtypes = None, [i32, u64]
    f = getattr(lib, p + "init_window"); f.restype, f.argtypes = None, [u64]
    f = getattr(lib, p + "slot_block"); f.restype, f.argtypes = u64, [u32]
    f = getattr(lib, p + "base"); f.restype, f.argtypes = u64, []
    f = getattr(lib, p + "label_of"); f.restype, f.argtypes = i32, [u64]
    f = getattr(lib, p + "get_next"); f.restype, f.argtypes = None, [u64]
    f = getattr(lib, p + "fill"); f.restype, f.argtypes = i32, [u64, u64]
    f = getattr(lib, p + "erase"); f.restype, f.argtypes = None, [u64]
    f = getattr(lib, p + "open"); f.restype, f.argtypes = None, [u64, u32, u32, i32, i32, i32]
    f = getattr(lib, p + "move"); f.restype, f.argtypes = None, [u64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n - 1] == [0, 4, 0xC, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x37, 4, 6], list(out)[:n]
    lib.has_queue = out[n - 1]   # the ride record's HasQueue slot on the host (the Mac's +0x40; the host's balance slots are wider)
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    pack = lambda f, v: struct.pack(s.fmt[0] + f, v)
    panel = s.alloc(0x40)
    s.register(L_PANEL, panel)
    for lb in WINDOWS + (L_W10,):
        if lb not in s.addr:
            a = s.alloc(0x130)
            s.register(lb, a)
            s.init_window(a)
    for i, lb in enumerate(THINGS):
        a = s.alloc(0x40)
        s.register(lb, a)
        x, y, rid, rot = case["things"][i]
        s.write(a + 4, bytes([x]))
        s.write(a + 6, bytes([y]))
        s.write(a + 0xE, pack("H", rid))
        s.words(a + 0x10, [rot])
    ride = s.alloc(max(0x60, s.has_queue + 8))
    s.register(L_RIDE, ride)
    s.words(ride + s.has_queue, [case["rinfo"]])
    mouse = s.alloc(0x10)
    s.register(L_MOUSE, mouse)
    table = s.alloc(0x50)
    for off in range(8, 0x48, 4):
        s.words(table + off, [s.p32(s.slot_entry(off))])
    s.words(panel, [case["mode"], case["f4"], case["f8"], s.p32(s.addr_of(case["window"])), s.p32(s.addr_of(case["f10"])),
                    s.p32(s.addr_of(case["f14"])), s.p32(s.addr_of(case["f18"])), case["f1c"], s.p32(table)])
    s.wptr(s.g("gThing"), s.addr_of(case["thing"]))
    s.words(s.g("gCount"), [case["count0"]])
    s.wptr(s.g("gMouse"), s.addr_of(case["mouse"]))
    s.words(s.g("gWord1E0"), [case["w1e0"]])
    s.words(s.g("gWord1E4"), [case["w1e4"]])
    s.write(s.g("gDir"), bytes([case["dir0"]]))
    s.wptr(s.g("gMoveObj"), s.addr_of(case["moveobj0"]))
    s.wptr(s.g("gGame"), s.addr_of(L_GAME))
    s.write(s.g("gConfirm"), bytes([case["confirm"]]))


def execute(s, case):
    p = s.addr[L_PANEL]
    op = case["op"]
    if op == "fill":
        return s.call(FN_FILL, [p, s.addr_of(case["arg"])])
    if op == "get":
        s.call(FN_GET, [p])
    elif op == "erase":
        s.call(FN_ERASE, [p])
    elif op == "open":
        s.call(FN_OPEN, [p, case["desc"], case["cb"]] + [v & MASK for v in case["ids"]])
    else:
        s.call(FN_MOVE, [p])
    return None   # void functions: r3 is whatever the last call left


def observe(s, case, answer):
    panel = s.rd_words(s.addr[L_PANEL], 9)
    out = {"answer": answer, "log": s.log(),
           "mode": panel[0], "f4": panel[1], "f8": panel[2], "f1c": panel[7], "vt": panel[8] != 0}
    for name, i in (("window", 3), ("f10", 4), ("f14", 5), ("f18", 6)):
        out[name] = s.lab(s.unp32(panel[i]), False)
    for name in ("gThing", "gMouse", "gMoveObj", "gGame"):
        out[name] = s.lab(s.rptr(s.g(name)), False)
    for name in ("gCount", "gWord1E0", "gWord1E4"):
        out[name] = s.rd_word(s.g(name))
    out["gDir"] = s.read(s.g("gDir"), 1)
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


def base(r, op, mode=None):
    w = r.choice
    c = {"op": op, "mode": r.choice(MODES) if mode is None else mode, "f4": r.getrandbits(32), "f8": r.getrandbits(32),
         "window": 0, "f10": w([0, L_W10]), "f14": w([0, L_W14]), "f18": w([0, L_W18]), "f1c": r.getrandbits(32),
         "thing": w([L_T0, L_T1, L_T2]) if op in ("erase", "move") else w([0, L_T0, L_T1, L_T2]),
         "mouse": w([0, L_MOUSE]), "w1e0": r.getrandbits(32), "w1e4": r.getrandbits(32), "count0": r.getrandbits(32),
         "dir0": r.getrandbits(8), "moveobj0": w([0, L_T2]), "confirm": w([0, 1, 2, 0x80, 0xFF]),
         "things": [(w([0, 1, 5, 0x7F, 0x80, 0xFF, r.getrandbits(8)]), w([0, 1, 3, 0x80, 0xFF, r.getrandbits(8)]),
                     w([0, 1, 0x7F, 0x8000, 0xFFFF, r.getrandbits(16)]), w([0, 1, -1, r.getrandbits(32)])) for _ in range(3)],
         "rinfo": w([0, 1, 0x80000000, r.getrandbits(32)]), "arg": w([0, L_T0, L_T1, L_T2]),
         "desc": r.getrandbits(32), "cb": r.getrandbits(32), "ids": [w([0, 1, 2, -1, r.getrandbits(31)]) for _ in range(3)]}
    c["queues"] = {"GetString": [r.getrandbits(31)], "Iterate": [w([0, 1, 7, r.getrandbits(32)])],
                   "WrapNext": [w([0, L_T0, L_T1, L_T2])], "Build": [w([0, L_WMAIN])],
                   "GetWindow": [w([0, L_W14]), L_W18, w([0, L_W10])], "GetDBInfo": [L_RIDE],
                   "ConstructQ": [w([0, 1, 3, 0x1FF, r.getrandbits(32)])]}
    if op == "open":
        c["window"] = w([0, 0, L_WMAIN, L_W14])
        c["queues"]["Build"] = [w([0, L_WMAIN, L_WMAIN])]
    return c


def cases(op, seed):
    def gen(r):
        return base(r, op)
    return gen


def edges(op, seed):
    def gen():
        r = random.Random(seed)
        for mode in MODES:
            for _ in range(4):
                yield base(r, op, mode)
        for _ in range(60):
            c = base(r, op)
            if op == "get":
                c["thing"] = r.choice([0, L_T0, L_T1])
                c["queues"]["WrapNext"] = [c["thing"] if r.random() < 0.4 else r.choice([0, L_T0, L_T1, L_T2])]
            elif op == "erase":
                c["confirm"] = r.choice([0, 1, 0x80])
            elif op == "move":
                c["mouse"] = r.choice([0, L_MOUSE])
                c["rinfo"] = r.choice([0, 1])
            elif op == "open":
                c["window"] = r.choice([0, L_WMAIN])
                c["queues"]["Build"] = [r.choice([0, L_WMAIN])]
                c["queues"]["GetWindow"] = [r.choice([0, L_W14]), L_W18, r.choice([0, L_W10])]
                c["ids"] = [r.choice([0, 1, -1]) for _ in range(3)]
            yield c
        if op in ("erase", "move"):
            for x in (0, 1, 0x7F, 0x80, 0xFF):
                for y in (0, 1, 0xFF):
                    c = base(r, op)
                    c["things"][0] = (x, y, 0xFFFF, -1)
                    c["thing"] = L_T0
                    c["confirm"] = 0
                    yield c
    return gen


TESTS = [
    FnTest("UIStatusPanel::GetNextObject", ppc, host, edge=edges("get", 11), rnd=cases("get", 1)),
    FnTest("UIStatusPanel::FillObjectList", ppc, host, edge=edges("fill", 12), rnd=cases("fill", 2)),
    FnTest("UIStatusPanel::EraseObject", ppc, host, edge=edges("erase", 13), rnd=cases("erase", 3)),
    FnTest("UIStatusPanel::OpenWindow", ppc, host, edge=edges("open", 14), rnd=cases("open", 4)),
    FnTest("UIStatusPanel::Move", ppc, host, edge=edges("move", 15), rnd=cases("move", 5)),
]
