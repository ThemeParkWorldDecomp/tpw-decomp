"""Unit v5669, the staff room panel's list fill and window set-up, against libtpw_unit_StaffRoomStatusPanel_FillStaffRoomList.dylib
(tests/unit_StaffRoomStatusPanel_FillStaffRoomList.cpp), main app ("SimTheme Park"),
src/LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom_StaffRoomStatusPanel_FillStaffRoomList.cpp:
  StaffRoomStatusPanel::FillStaffRoomList 0x101c31d4, InitStatusWindow 0x101c2e88, InitWindow 0x101c36f0, UpdateViewSprite 0x101c2b88

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit (the window lookups, the list calls, the constructors, the iterator, the script calls, CoolRect, the virtual calls through the
glue) is a fake on both sides that records its arguments; the windows and objects are arena blocks named by labels.

The world of a case: the panel (window pointers, the virtual table, the script words), the globals (staff type, sort, colour, the things),
the staff words and the view window's Z, the fakes' answers (object count, client rectangle, Z translation, script handle, thing id).
What is compared: the panel's script words, the globals written, the whole event log. FillStaffRoomList runs as the original inside
InitWindow.

No fused operations (fsubs, fdivs, fmuls only; every value is exact). Not tested: a NULL window from GetWindow (the original reads it
unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_StaffRoomStatusPanel_FillStaffRoomList"
MASK = 0xFFFFFFFF

FN_FILL, FN_STATUS, FN_WINDOW, FN_VIEW = 0x101C31D4, 0x101C2E88, 0x101C36F0, 0x101C2B88
SIZES = {FN_FILL: 324, FN_STATUS: 328, FN_WINDOW: 592, FN_VIEW: 404}
OWN = set(SIZES)

GLUE = 0x1023BAA8

ADDR = dict(colour=0x102AED64, msg406=0x102AED68, panelthing=0x103947A4, showthing=0x10395A88, id=0x10395A90, list=0x10395A94,
            stafftype=0x10395A98, spawn=0x1029FF70, tv_filler=0x10267F50, tv_lcformat=0x10267F40, tv_lcpost=0x10267F38,
            tv_handler=0x10267F48)
# labels of the function pointers and the spawn block
FIXED_ADDR = {"tv_filler": 50, "tv_lcformat": 51, "tv_lcpost": 52, "tv_handler": 53, "spawn": 60}
HOST_LAYOUT_N = 1

# labels of the objects: 1 panel, 2 panel window, 3 status window, 4 list, 5 check buttons, 6 view window, 7 and 8 arrows, 9 cash figure,
# 10 text figure, 11 and 12 column buttons, 13 panel thing, 14 shown thing, 15 staff, 16-18 strings, 19 font, 20-21 new blocks, 22 draw object
NOBJ = 22
WINDOW_IDS = {0x1E7B: 4, 0x1E7C: 5, 0x1E84: 6, 0x1E85: 7, 0x1E86: 8, 0x1E8A: 9, 0x1E8B: 10, 0x10: 11, 0x11: 12}
STRINGS = {0x38: 16, 0x39: 17, 0x3A: 18}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub(addr, name, spec, ret=None):
    """A fake that records `name` with the arguments the spec names: L label, i s32, h s16, u u32, b u8, f float bits."""

    def fn(e):
        L = e.cl
        args = []
        r, fr = 3, 1
        for ch in spec:
            if ch == "L":
                args.append(L.lab(e.r(r)))
                r += 1
            elif ch == "i":
                args.append(s32(e.r(r)))
                r += 1
            elif ch == "h":
                args.append(T.s16(e.r(r)))
                r += 1
            elif ch == "u":
                args.append(e.r(r) & MASK)
                r += 1
            elif ch == "b":
                args.append(e.r(r) & 0xFF)
                r += 1
            elif ch == "f":
                args.append(fb(e.f(fr)))
                fr += 1
        L.rec(name, tuple(args))
        if ret:
            ret(e, L)

    STUBS.internal(addr, fn)


def ret_obj(label):
    return lambda e, L: e.set_r(3, e.objs[label])


def ret_queue(key):
    return lambda e, L: e.set_r(3, L.ret(key) & MASK)


def st_getwindow(e):
    L = e.cl
    ident = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), ident))
    e.set_r(3, e.objs[WINDOW_IDS[ident]] if ident in WINDOW_IDS else 0)


def st_thingid(e):
    L = e.cl
    L.rec("GetThingId", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("tid") & 0xFFFF))


def st_getstring(e):
    L = e.cl
    ident = s32(e.r(3))
    L.rec("GetString", (ident,))
    e.set_r(3, e.objs[STRINGS[ident]] if ident in STRINGS else 0)


def st_new(e):
    L = e.cl
    L.rec("OperatorNew", (e.r(3) & MASK,))
    e.set_r(3, e.objs[20 + e.newcount])
    e.newcount += 1


def st_finddraw(e):
    L = e.cl
    w0, w1 = struct.unpack(">II", e.read(e.r(3), 8))
    L.rec("FindDrawObject", (w0, w1))
    e.set_r(3, e.objs[22])


def st_clientrect(e):
    L = e.cl
    L.rec("GetClientRect", (L.lab(e.r(3)),))
    vals = [T.s16(L.ret(k)) for k in ("rl", "rt", "rr", "rb")]
    e.write(e.r(4) + 4, struct.pack(">hhhh", *vals))
    e.set_r(3, 0)


def st_ctz(e):
    L = e.cl
    L.rec("CalcZTrans", (s32(e.r(3)),))
    e.set_f1(bitsf(L.ret("ctz")))


def st_glue(e):
    L = e.cl
    kind = e.tv.get(e.r(12))
    if kind == "slot":
        L.rec("Slot28", (L.lab(e.r(3)),))
    elif kind == "setvalue":
        L.rec("WindowSetValue", (L.lab(e.r(3)), s32(e.r(4))))
    else:
        raise AssertionError("glue call through an unknown vector %x" % e.r(12))


STUBS.internal(0x101DBE40, st_getwindow)
stub(0x101D3CC8, "RemoveAllItems", "L")
STUBS.internal(0x1012E1F8, st_thingid)
stub(0x101F7DB0, "Iterate", "iL")
stub(0x101D2220, "UpdateDisplay", "L")
stub(0x10180270, "GetFont", "u", ret_obj(19))
stub(0x101DB624, "SetClientDrawObject", "LL")
stub(0x101DA118, "SetFont", "LL")
STUBS.internal(0x10180468, st_getstring)
stub(0x101DE130, "SetClientDrawText", "LL")
STUBS.internal(0x102384B8, st_new)
stub(0x1018D11C, "SpriteCtor", "L")
stub(0x1018DE60, "CashCtor", "L")
stub(0x10192A90, "FillObjectList", "LL", ret_queue("count"))
stub(0x101DC1EC, "Show", "Lh")
stub(0x101D2864, "SetColumnFormatsAll", "LLL")
stub(0x101D2614, "SetColumnType", "Lhi")
STUBS.internal(0x101CD2D4, st_finddraw)
stub(0x101D1EB4, "SetSelectionObject", "LL")
stub(0x101DCBA8, "SetCallbackFunction", "LL")
stub(0x101D4DB8, "SetSort", "Lib")
stub(0x101818E4, "MAKECOLUMNBUTTON", "LLiiiii")
stub(0x101CC148, "SetCurrent", "Li")
stub(0x10193DCC, "InitEditBox", "Li")
stub(0x101C7448, "CoolRect", "")
stub(0x101C74E0, "~CoolRect", "")
STUBS.internal(0x101DBA4C, st_clientrect)
STUBS.internal(0x101879D0, st_ctz)
stub(0x100D9478, "SCR_KillScript", "i")
stub(0x100DA18C, "ESP_SpawnScript", "Liiifff", ret_queue("esp"))
stub(0x100D95A0, "SCR_SetFloatVariable", "iuf")
STUBS.internal(GLUE, st_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


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

    def make_objs(self):
        return {l: self.alloc(0x400) for l in range(1, NOBJ + 1)}

    def label_of(self, raw):
        for l, a in self.objs.items():
            if a == raw:
                return l
        return raw


class Ppc(Side):
    fmt = ">I"
    psize = 4
    zoff = 0xDC

    def __init__(self, e, case):
        self.e = e
        self.objs = e.objs = self.make_objs()
        e.tv = {}
        e.newcount = 0
        fixed = {a: l for l, a in self.objs.items()}
        for name, l in FIXED_ADDR.items():
            fixed[ADDR[name]] = l
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ref(self, a):
        return a

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def tvec(self, block, kind):
        self.e.tv[block] = kind
        self.words(block, [0x10000000, 0])

    def cash(self, window):
        vt, tv = self.alloc(0x40), self.alloc(0x10)
        self.words(window + 0x118, [vt])
        self.words(vt + 0x24, [tv])
        self.tvec(tv, "setvalue")

    def call(self, op, panel, staff, case):
        with_args = {"fill": (FN_FILL, [panel, case["type"] & MASK, case["flag"] & MASK]), "status": (FN_STATUS, [panel]),
                     "window": (FN_WINDOW, [panel]), "view": (FN_VIEW, [panel, staff])}
        fn, args = with_args[op]
        T.call_ppc(self.e, fn, args)


class Host(Side):
    fmt = "<I"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.zoff = lib.zoff
        self.objs = self.make_objs()
        self.base = lib.tpw_sr_base()
        lib.tpw_sr_reset()
        for l, a in self.objs.items():
            lib.tpw_sr_setobj(l, a)
        fixed = dict(self.objs)
        for name, l in FIXED_ADDR.items():
            fixed[l] = self.g(name)
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sr_addr(name.encode())

    def ref(self, a):
        return a - self.base

    def alloc(self, n):
        return self.lib.tpw_sr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def tvec(self, block, kind):
        self.write(block, struct.pack("<Q", self.g("slotfn")))

    def cash(self, window):
        self.alloc(0x40)
        self.alloc(0x10)

    def call(self, op, panel, staff, case):
        p = "tpw_sr_"
        if op == "fill":
            self.lib.tpw_sr_fill(panel, s32(case["type"]), s32(case["flag"]))
        elif op == "status":
            self.lib.tpw_sr_status(panel)
        elif op == "window":
            self.lib.tpw_sr_window(panel)
        else:
            self.lib.tpw_sr_view(panel, staff)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sr_addr.restype, lib.tpw_sr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sr_alloc.restype, lib.tpw_sr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sr_base.restype, lib.tpw_sr_base.argtypes = C.c_uint64, []
    lib.tpw_sr_setobj.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_sr_reset.argtypes = []
    lib.tpw_sr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sr_fill.argtypes = [C.c_uint64, C.c_int32, C.c_int32]
    lib.tpw_sr_status.argtypes = [C.c_uint64]
    lib.tpw_sr_window.argtypes = [C.c_uint64]
    lib.tpw_sr_view.argtypes = [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sr_layout(out)
    assert n == HOST_LAYOUT_N
    lib.zoff = out[0]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    O = s.objs
    s.words(O[1] + 0x0C, [s.ref(O[2]), s.ref(O[3])])
    table, tv = s.alloc(0x40), s.alloc(0x10)
    s.words(table + 0x28, [s.ref(tv)])
    s.tvec(tv, "slot")
    s.words(O[1] + 0x20, [s.ref(table), case["script"], case["f28"], case["f2c"]])
    s.cash(O[9])
    s.write(O[6] + s.zoff, struct.pack(s.fmt[0] + "h", case["z"]))
    s.words(O[15] + 0x20, [case["a"], case["sprite"]])
    s.words(s.g("colour"), [case["colour"]])
    s.words(s.g("msg406"), [case["msg406"]])
    s.words(s.g("stafftype"), [case["stype"]])
    s.half(s.g("id"), case["id0"])
    for name, label in (("list", 0), ("panelthing", 13), ("showthing", 14)):
        v = s.objs[label] if label else 0
        s.write(s.g(name), struct.pack(s.fmt[0] + ("I" if s.psize == 4 else "Q"), v))


def execute(s, case):
    s.call(case["op"], s.objs[1], s.objs[15], case)


def observe(s, case):
    raw = s.read(s.g("list"), s.psize)
    raw = struct.unpack(s.fmt[0] + ("I" if s.psize == 4 else "Q"), raw)[0]
    return {"log": s.log(), "script": s.rd_words(s.objs[1] + 0x24, 3), "colour": s.rd_words(s.g("colour"), 1),
            "msg406": s.rd_words(s.g("msg406"), 1), "stype": s.rd_words(s.g("stafftype"), 1), "id": s.rd_half(s.g("id")),
            "list": s.label_of(raw)}


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def base(r, op):
    return {"op": op, "type": 0, "flag": 0, "stype": 0, "colour": 8, "msg406": 1, "id0": 0, "script": 0, "f28": -1, "f2c": -1, "z": 0,
            "a": 0, "sprite": 0,
            "queues": {"tid": [r.randrange(0x10000)], "count": [r.choice([0, 1, 2, 3])], "rl": [0], "rt": [0], "rr": [0], "rb": [0],
                       "ctz": [fb(r.uniform(-4, 4))], "esp": [r.randrange(1, 1000)]}}


def rnd_world(r, c):
    c["stype"] = r.choice([-2, -1, 0, 1, 2, 3, 4, 5, 6, r.randrange(-100, 100)])
    c["colour"] = r.choice([8, 0, -1, r.randrange(-1000, 1000)])
    c["msg406"] = r.choice([1, 0, -1, r.randrange(-100, 100)])
    c["id0"] = r.randrange(0x10000)
    c["script"] = r.choice([0, r.randrange(1, 1000)])


def fill_case(r, t=None, flag=None):
    c = base(r, "fill")
    rnd_world(r, c)
    c["type"] = r.choice([-2, -1, 0, 1, 2, 3, 4, 5, 9, r.randrange(-50, 50)]) if t is None else t
    c["flag"] = r.choice([0, 1, -1, r.randrange(-9, 9)]) if flag is None else flag
    return c


def fill_edges():
    r = random.Random(5669)
    for t in (-2, -1, 0, 1, 2, 3, 4, 5, 6, 0x10000):
        for flag in (0, 1, -5):
            yield fill_case(r, t, flag)


def fill_random(r):
    return fill_case(r)


def status_case(r):
    c = base(r, "status")
    rnd_world(r, c)
    return c


def status_edges():
    r = random.Random(5670)
    for _ in range(3):
        yield status_case(r)


def window_case(r, t=None, count=None):
    c = base(r, "window")
    rnd_world(r, c)
    if t is not None:
        c["stype"] = t
    if count is not None:
        c["queues"]["count"] = [count]
    else:
        c["queues"]["count"] = [r.choice([-1, 0, 1, 2, 3, r.randrange(-5, 50)])]
    return c


def window_edges():
    r = random.Random(5671)
    for t in (-2, -1, 0, 1, 2, 3, 4, 5, 6):
        for count in (0, 1, 2, 3, -1):
            yield window_case(r, t, count)


def window_random(r):
    return window_case(r)


def rs16(r):
    return r.choice([0, 1, -1, 2, 3, -2, -3, 0x3F8, 0x300, 0x7FFF, -0x8000, r.randrange(-0x8000, 0x8000), r.randrange(-1200, 1200),
                     r.randrange(-1000, 1000)])


def view_case(r, same28=None, same2c=None, script=None):
    c = base(r, "view")
    c["z"] = r.choice([0, 1, -1, 0x7FFF, -0x8000, r.randrange(-300, 300)])
    c["a"] = r.choice([0, 1, -1, r.randrange(-5, 50), r.randrange(-(1 << 31), 1 << 31)])
    c["sprite"] = r.choice([0, 1, -1, r.randrange(-5, 50), r.randrange(-(1 << 31), 1 << 31)])
    s28 = r.random() < 0.5 if same28 is None else same28
    s2c = r.random() < 0.5 if same2c is None else same2c
    c["f28"] = c["sprite"] if s28 else c["sprite"] + r.choice([1, -1, 7])
    c["f2c"] = c["a"] if s2c else c["a"] + r.choice([1, -1, 7])
    c["script"] = r.choice([0, r.randrange(1, 1000)]) if script is None else script
    for k in ("rl", "rt", "rr", "rb"):
        c["queues"][k] = [rs16(r) & 0xFFFF]
    return c


def view_edges():
    r = random.Random(5672)
    for s28 in (True, False):
        for s2c in (True, False):
            for script in (0, 77):
                yield view_case(r, s28, s2c, script)
    # shifts of odd and negative widths and heights
    for l, t, rr, b in ((0, 0, 1, 1), (5, 3, 4, 2), (-5, -3, 6, 7), (0, 0, 0x7FFF, 0x7FFF), (-0x8000, -0x8000, 0x7FFF, 0x7FFF),
                        (1, 1, 2, 5), (3, 9, -4, -9), (0x3F8, 0, 0x3F8, 0x300)):
        c = view_case(r, False, False, 5)
        for k, v in zip(("rl", "rt", "rr", "rb"), (l, t, rr, b)):
            c["queues"][k] = [v & 0xFFFF]
        yield c


def view_random(r):
    return view_case(r)


TESTS = [
    FnTest("StaffRoomStatusPanel::FillStaffRoomList", ppc, host, edge=fill_edges, rnd=fill_random),
    FnTest("StaffRoomStatusPanel::InitStatusWindow", ppc, host, edge=status_edges, rnd=status_case),
    FnTest("StaffRoomStatusPanel::InitWindow", ppc, host, edge=window_edges, rnd=window_random),
    FnTest("StaffRoomStatusPanel::UpdateViewSprite", ppc, host, edge=view_edges, rnd=view_random),
]
