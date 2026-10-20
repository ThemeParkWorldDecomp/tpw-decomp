"""Unit v5078, the all-kids panel, against libtpw_unit_AllKidsPanelMsg.dylib (tests/unit_AllKidsPanelMsg.cpp), main app
("SimTheme Park"), src/ui_allKidsPanel_AllKidsPanelMsg.cpp:
  FillAllKidsListCallback 0x10194880, UpdateListEntries 0x101949cc, AllKidsPanelMsg 0x10194b4c, LCFormat 0x10194df0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (the string and
panel functions, GetWindow, GetData, AddItem, ChangeItem, GetTimeInPark, GetFunnyTimeForGameTurns, GetThingId, the window constructor,
CreateObject, GetFontTableEntry and the font's call) is a fake on both sides that records its arguments and answers from the case.
The 64-bit division 0x10237e1c runs as the original on the PowerPC side.

The world of a case: gGameWorld, the panel window global, the list global, eight person blocks (fields +0x30, +0x19c float, +0x1a0, +0x1c4,
+0x1d8), the list's item count, the screen height, the three small globals. What is compared: the answer, the globals, the event log.

No fused operations. Not tested: the LCFormat font call is the font's height getter on the host (the Mac calls a virtual through
__ptr_glue; the class is not recovered), the same event on both sides; its result is dead (the row height is set to 0x38 after).
Float percent values stay below 2^31 (Unicorn's fctiwz saturates differently).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_AllKidsPanelMsg"
MASK = 0xFFFFFFFF

FILL, UPDATE, MSG, LCF = 0x10194880, 0x101949cc, 0x10194b4c, 0x10194df0
SIZES = {FILL: 276, UPDATE: 344, MSG: 616, LCF: 408}
OWN = set(SIZES)

ADDR = dict(world=0x1037ebfc, win=0x103947c8, lst=0x103947c4, fmt=0x103947bc, fontidx=0x102a8878, sel=0x102a8880,
            scr=0x10395bba, vsp=0x1037ec14)
HOST_LAYOUT = [8]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)

CUR = [None]


def rec(name, spec, ret=None):
    return CL.recorder(name, spec, ret=ret)


def u32at(e, a):
    return struct.unpack(">I", e.read(a, 4))[0]


def st_get_window(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, CUR[0].lst if L.ret("GetWindow") != 0 else 0)


def st_get_data(e):
    L = e.cl
    pos = s32(e.r(4))
    L.rec("GetData", (L.lab(e.r(3)), pos))
    if e.r(5):
        e.write(e.r(5), struct.pack(">I", CUR[0].persons[pos & 7]))
    e.set_r(3, 1)


def st_thing_id(e):
    L = e.cl
    L.rec("GetThingId", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetThingId") & 0xFFFF))


def st_zoom(e):
    e.cl.rec("Zoom", ((e.r(3) >> 16) & 0xFFFF,))


def st_funny(e):
    L = e.cl
    L.rec("Funny", (L.lab(e.r(4)), e.r(5) & MASK))
    e.write(e.r(3), struct.pack(">II", L.ret("FunnyHi") & MASK, L.ret("FunnyLo") & MASK))


def st_add(e):
    L = e.cl
    cells = [s32(u32at(e, e.r(4) + 4 * i)) for i in range(6)]
    L.rec("AddItem", (L.lab(e.r(3)),) + tuple(cells) + (L.lab(e.r(5)), s32(e.r(6)), s32(e.r(7))))
    e.set_r(3, 0)


def st_change(e):
    L = e.cl
    cells = [s32(u32at(e, e.r(5) + 4 * i)) for i in range(6)]
    L.rec("ChangeItem", (L.lab(e.r(3)), s32(e.r(4))) + tuple(cells) + (s32(e.r(6)), s32(e.r(7)), s32(e.r(8))))


def st_font(e):
    L = e.cl
    L.rec("GetFont", (e.r(3) & MASK,))
    e.set_r(3, CUR[0].font)


def st_ptr_glue(e):
    L = e.cl
    L.rec("FontCall", (L.lab(e.r(3)),))
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
    e.set_r(3, L.ret("FontCall") & MASK)


def st_new(e):
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
    e.set_r(3, e.alloc(e.r(3)))


STUBS.internal(0x10180468, rec("GetString", "i", "i"))
STUBS.internal(0x10180794, rec("SetTitleText", "Liu"))
STUBS.internal(0x101956a4, rec("RemoveAllKidsPanel", ""))
STUBS.internal(0x101dbe40, st_get_window)
STUBS.internal(0x101d41f0, st_get_data)
STUBS.internal(0x101433f8, rec("Show", "LL"))
STUBS.internal(0x1012e1f8, st_thing_id)
STUBS.internal(0x10181b00, st_zoom)
STUBS.internal(0x1018227c, rec("CancelAdvisor", "i"))
STUBS.internal(0x101983c4, rec("CreateAllItemsPanel", "iL"))
STUBS.internal(0x101aca78, rec("CreateParkStatusPanel", ""))
STUBS.internal(0x10199630, rec("CreateAllStaffPanel", "iL"))
STUBS.internal(0x1018493c, rec("DefaultPanelMsg", "Liii", "i"))
STUBS.internal(0x1011962c, rec("TimeInPark", "L", "i"))
STUBS.internal(0x10112928, st_funny)
STUBS.internal(0x101d3660, st_change)
STUBS.internal(0x101d3000, st_add)
STUBS.internal(0x10180270, st_font)
STUBS.internal(0x1023baa8, st_ptr_glue)
STUBS.internal(0x102384b8, st_new)
STUBS.internal(0x101d98d8, rec("InterfaceWindow", "NLiihhhh"))
STUBS.internal(0x101dc750, rec("CreateObject", "Liiihhhh"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def ptr(self, a, v):
        """A native pointer global: 4 bytes on the Mac, 8 on the host."""
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        n = struct.calcsize(self.pfmt)
        return struct.unpack(self.pfmt, self.read(a, n))[0]

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def labels(self):
        d = {self.win: 1, self.lst: 2, self.ptime: 3, self.g("vsp"): 4, self.g("font") if False else self.font: 5}
        for i, p in enumerate(self.persons):
            d[p] = 10 + i
        return d


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.win = e.alloc(16)
        self.lst = e.alloc(0x400)
        self.world = e.alloc(0x400)
        self.persons = [e.alloc(0x240) for _ in range(8)]
        self.font = e.alloc(0x40)
        table = e.alloc(0x40)
        e.write(self.font + 0x10, struct.pack(">I", table))
        self.ptime = self.world + 0x2A0
        CUR[0] = self
        self.L = e.cl = CL.PpcLog(self.labels(), case["queues"])
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

    def count(self, n):
        self.words(self.lst + 0x150, [n])

    def call(self, op, args):
        e = self.e
        if op == "fill":
            return T.call_ppc(e, FILL, [self.persons[args[0]]])[0] & MASK
        if op == "update":
            T.call_ppc(e, UPDATE, [])
            return 0
        if op == "msg":
            w = self.win if args[0] else 0
            return T.call_ppc(e, MSG, [w, args[1] & MASK, args[2] & MASK, args[3] & MASK])[0] & MASK
        r3 = T.call_ppc(e, LCF, [])[0] & MASK
        return self.L.lab(r3) if r3 else 0


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        al = lib.tpw_akp_alloc
        self.win = al(16)
        self.lst = al(0x400)
        self.world = al(0x400)
        self.persons = [al(0x240) for _ in range(8)]
        self.font = self.g("font")
        self.ptime = self.world + 0x2A0
        CL.host_begin(lib, {v: k for k, v in self.labels().items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_akp_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def count(self, n):
        arr = (C.c_uint64 * 8)(*self.persons)
        self.lib.tpw_akp_setup(self.lst, n, arr)

    def call(self, op, args):
        lib = self.lib
        if op == "fill":
            return lib.tpw_akp_fill(self.persons[args[0]]) & MASK
        if op == "update":
            lib.tpw_akp_update()
            return 0
        if op == "msg":
            w = self.win if args[0] else 0
            return lib.tpw_akp_msg(w, s32(args[1]), s32(args[2]), s32(args[3])) & MASK
        r = lib.tpw_akp_lcformat()
        return r


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_akp_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_akp_setup.restype, lib.tpw_akp_setup.argtypes = None, [C.c_uint64, C.c_int32, C.POINTER(C.c_uint64)]
    lib.tpw_akp_fill.restype, lib.tpw_akp_fill.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_akp_update.restype, lib.tpw_akp_update.argtypes = None, []
    lib.tpw_akp_msg.restype, lib.tpw_akp_msg.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_akp_lcformat.restype, lib.tpw_akp_lcformat.argtypes = C.c_int64, []
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.ptr(s.g("world"), s.world)
    s.ptr(s.g("win"), s.win if case["haswin"] else 0)
    s.ptr(s.g("lst"), s.lst)
    s.words(s.g("fmt"), [case["fmt"]])
    s.words(s.g("fontidx"), [case["fontidx"]])
    s.words(s.g("sel"), [case["sel"]])
    s.half(s.g("scr"), case["scr"])
    for p, d in zip(s.persons, case["people"]):
        s.words(p + 0x30, [d[0]])
        s.words(p + 0x19C, [d[1]])
        s.words(p + 0x1A0, [d[2]])
        s.words(p + 0x1C4, [d[3]])
        s.words(p + 0x1D8, [d[4]])
    s.count(case["count"])


def observe(s, case, answer):
    w = s.rd_ptr(s.g("win"))
    return {"answer": answer, "log": s.log(), "win": 0 if w == 0 else (1 if w == s.win else -1), "fmt": s.rd_word(s.g("fmt")),
            "sel": s.rd_word(s.g("sel")), "lst": s.rd_ptr(s.g("lst")) == s.lst,
            "people": [s.rd_words(p, 0x90) for p in s.persons]}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], case["args"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


FLOATS = [0.0, 1.0, 25.5, 99.9, 100.0, 255.0, 255.9, 256.0, 256.5, 300.0, 1000.7, -1.0, -0.5, -300.0, 1e9, 12345.678]


def rfl(r):
    if r.random() < 0.4:
        return fb(r.choice(FLOATS))
    return fb(r.uniform(-400, 3000))


def rperson(r):
    return [r.choice([0, 0, 1, 2, r.randrange(1, 5000), r.getrandbits(32)]), rfl(r), r.getrandbits(32), r.getrandbits(32), r.getrandbits(32)]


def base(r, op, args):
    c = base0(r, op, args)
    if op == "update" or (op == "msg" and args[1] == 0x10 and args[2] == 0x80083):
        c["queues"]["GetWindow"] = [1]       # the panel's list always exists while the window does (the original reads it unchecked)
    return c


def base0(r, op, args):
    return {
        "op": op, "args": args,
        "queues": {
            "TimeInPark": [r.choice([0, 1, r.randrange(100000), r.getrandbits(32)])],
            "FunnyHi": [r.choice([0, 0, 1, r.randrange(0x7FFFFFFF), 0xFFFFFFFF, 0x80000000, r.getrandbits(32)])],
            "FunnyLo": [r.choice([0, r.getrandbits(32), 0x61C46800, 0x61C467FF, 0xFFFFFFFF])],
            "GetWindow": [r.choice([1, 1, 1, 0])],
            "GetThingId": [r.getrandbits(16)],
            "GetString": [r.getrandbits(32)],
            "DefaultPanelMsg": [r.getrandbits(32)],
            "FontCall": [r.getrandbits(16)],
        },
        "haswin": r.choice([True, True, False]), "count": r.randrange(0, 5), "scr": r.choice([0, 480, 768, 1200, -5, 32767]),
        "fmt": r.choice([0, 0, 0x38, 5, -1]), "fontidx": r.choice([7, 0, 3]), "sel": r.choice([1, 0, r.getrandbits(32)]),
        "people": [rperson(r) for _ in range(8)],
    }


def fill_edges():
    r = random.Random(11)
    for i in range(8):
        yield base(r, "fill", [i])
    # percent boundaries and the lives branch
    for fl in FLOATS:
        for lives in (0, 1):
            c = base(r, "fill", [0])
            c["people"][0][1] = fb(fl)
            c["people"][0][0] = lives
            yield c
    for hi, lo in ((0, 0), (0, 0x61C46800), (8, 0x61C46800), (8, 0x61C467FF), (9, 0), (0xFFFFFFFF, 0xFFFFFFFF), (0xFFFFFFF0, 0), (0x80000000, 0), (0x7FFFFFFF, 0xFFFFFFFF)):
        c = base(r, "fill", [1])
        c["queues"]["FunnyHi"], c["queues"]["FunnyLo"] = [hi], [lo]
        yield c


def fill_random(r):
    return base(r, "fill", [r.randrange(8)])


def update_edges():
    r = random.Random(12)
    for n in range(0, 5):
        for w in (True, False):
            c = base(r, "update", [])
            c["count"], c["haswin"] = n, w
            yield c
    for hi, lo in ((0xFFFFFFFF, 0xFFFFFFFF), (0xFFFFFFF0, 0), (8, 0x61C46800), (0x80000000, 5)):
        c = base(r, "update", [])
        c["count"] = 2
        c["queues"]["FunnyHi"], c["queues"]["FunnyLo"] = [hi], [lo]
        yield c


def update_random(r):
    return base(r, "update", [])


MESSAGES = [0x100, 5, 0x10, 0x14, 0x15, 0x400, 0x402, 0x406]
OTHERS = [0, 1, 4, 6, 0xF, 0x11, 0x13, 0x16, 0xFF, 0x101, 0x3FF, 0x401, 0x403, 0x405, 0x407, -1, 0x7FFFFFFF]
AS = [-2, -1, 0, 0x1E497, 0x1E498, 0x1E499, 0x1E49A, 0x1E49B, 0x1E49C, 0x1E496, 0x80083, 0x80082, 0x80084, 0x7FFFFF83, 7]
BS = [-1, 0, 1, 2, 3, 7, 8, -2, 0x12345678, 5]


def msg_edges():
    r = random.Random(13)
    for m in MESSAGES + OTHERS:
        for a in AS:
            c = base(r, "msg", [True, m, a, r.choice(BS)])
            yield c
    for m in (0x400, 0x402):
        for gw in (0, 1):
            for b in (-1, 0, 3):
                for w in (True, False):
                    c = base(r, "msg", [w, m, 0x1E497, b])
                    c["queues"]["GetWindow"] = [gw]
                    yield c
    for b in BS:
        yield base(r, "msg", [True, 0x406, 0, b])
    for w in (True, False):
        for cnt in range(0, 4):
            c = base(r, "msg", [True, 0x10, 0x80083, 0])
            c["haswin"], c["count"] = w, cnt
            yield c


def msg_random(r):
    m = r.choice(MESSAGES + OTHERS) if r.random() < 0.9 else r.getrandbits(32)
    a = r.choice(AS) if r.random() < 0.8 else r.getrandbits(32)
    b = r.choice(BS) if r.random() < 0.8 else r.getrandbits(32)
    return base(r, "msg", [r.random() < 0.8, m, a, b])


def lcf_edges():
    r = random.Random(14)
    for fmt in (0, 5, 0x38):
        for scr in (0, 480, -5):
            c = base(r, "lcformat", [])
            c["fmt"], c["scr"] = fmt, scr
            yield c


def lcf_random(r):
    return base(r, "lcformat", [])


TESTS = [
    FnTest("FillAllKidsListCallback", ppc, host, edge=fill_edges, rnd=fill_random),
    FnTest("UpdateListEntries", ppc, host, edge=update_edges, rnd=update_random),
    FnTest("AllKidsPanelMsg", ppc, host, edge=msg_edges, rnd=msg_random),
    FnTest("LCFormat", ppc, host, edge=lcf_edges, rnd=lcf_random),
]
