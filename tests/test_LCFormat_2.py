"""Unit v5153, the all-staff panel's row functions and button handler, against libtpw_unit_LCFormat_2.dylib (tests/unit_LCFormat_2.cpp), main app
("SimTheme Park"), src/CreateAllStaffPanel_LCFormat.cpp:
  LCFormat 0x101991e8 (AllStaffLCFormat), LCPostFormat 0x10199370 (AllStaffLCPostFormat), AllStaffPanelLCHandler 0x10198d54

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the window's
members, the draw object constructors, the font table, the analyser, the strings) are fakes on both sides that record their arguments; the
virtual calls (the window's SetValue and SetText, the font's GetHeight) go through vtables the test builds on the PowerPC side at the
Mac's offsets (vptr at +0x118 of a window, +0x10 of a font; slots 0x24 SetValue, 0x2c SetText, 0xc GetHeight) to fakes that record the same events.

The world of a case: gStaffListFormat, gStaffListFont, gUIRealScreenHeight, the five window pool, the staff panel and world pointers,
the answer queues (GetHeight, AverageHappiness, CallDefaultWindowHandler). What is compared: the answer (for LCFormat the label of the new
window), the three globals and the event log.

No fused operations. Not tested: gUIRealScreenHeight of 0 (the original divides by it; division by zero is undefined on the hardware).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LCFormat_2"
MASK = 0xFFFFFFFF

FN_FORMAT, FN_POST, FN_HANDLER = 0x101991E8, 0x10199370, 0x10198D54
SIZES = {FN_FORMAT: 360, FN_POST: 652, FN_HANDLER: 1104}
OWN = set(SIZES)

# stand-in code addresses for the virtual calls (unused functions of the neighbourhood; the stubs replace them)
A_SETVALUE, A_SETTEXT, A_GETHEIGHT = 0x10198944, 0x10194B4C, 0x10194DF0

ADDR = dict(gStaffListFormat=0x103947E4, gStaffListFont=0x102A9080, gUIRealScreenHeight=0x10395BBA, gStaffPanel=0x103947F0,
            gGameWorld=0x1037EBFC)

STRING_IDS = [0x6B, 0x6C, 0x6D, 0x6E, 0x6F]
WINDOW_IDS = {0x329: 1, 0x32A: 2, 0x32D: 3}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def R(addr, name, spec, ret=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret))


def ret_get_child(e, log):
    return e.lc["pool"][1]


def ret_get_next(e, log):
    p = e.r(3) & MASK
    pool = e.lc["pool"]
    return pool[pool.index(p) + 1] if p in pool and pool.index(p) < 4 else 0


def ret_get_window(e, log):
    slot = WINDOW_IDS.get(e.r(4) & MASK)
    return e.lc["pool"][slot] if slot else 0


def ret_font(e, log):
    return e.lc["font"]


def ret_string(e, log):
    i = e.r(3) & MASK
    return e.lc["strs"][i - 0x6B] if 0x6B <= i <= 0x6F else 0


def ret_analyser(e, log):
    return e.lc["analyser"]


R(0x101D98D8, "IWCtor", "NLiihhhh")
R(0x101DC750, "CreateObject", "Liiihhhh")
R(0x1018D11C, "TextSpriteCtor", "NL")
R(0x1018DE60, "CashTextCtor", "NL")
R(0x101898C8, "GradientCtor", "Ns")
R(0x101DB624, "SetClientDrawObject", "LL")
R(0x101DA118, "SetFont", "LL")
R(0x101DE130, "SetClientDrawText", "LL")
R(0x101DA204, "SetFontJust", "Lii")
R(0x101DBAC4, "GetChild", "Li", ret_get_child)
R(0x101DBB38, "GetNext", "L", ret_get_next)
R(0x101DBE40, "GetWindow", "Li", ret_get_window)
R(0x101DE318, "CallDefaultWindowHandler", "Liii", "i")
R(0x10180270, "GetFontTableEntry", "u", ret_font)
R(0x10180468, "GetString", "i", ret_string)
R(0x10180940, "IndentClient", "Lhh")
R(0x101987FC, "FillAllStaffList", "ii")
R(0x1013EBC8, "GetParkAnalyser", "L", ret_analyser)
R(0x100E89FC, "AverageHappiness", "Li", "i")
R(0x101C7448, "CoolRectCtor", "N")
R(0x101C74E0, "CoolRectDtor", "Li")
R(A_SETVALUE, "SetValue", "Li")
R(A_SETTEXT, "SetText", "LL")
R(A_GETHEIGHT, "GetHeight", "L", "i")


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


def labels(o):
    d = {}
    for i, a in enumerate(o["pool"]):
        d[a] = 1 + i
    d[o["world"]] = 6
    d[o["analyser"]] = 7
    d[o["font"]] = 8
    for i, a in enumerate(o["strs"]):
        d[a] = 9 + i
    return d


def make_objects(s):
    return dict(pool=[s.alloc(0x400) for _ in range(5)], world=s.alloc(0x40), analyser=s.alloc(0x40), font=s.alloc(0x40),
                strs=[s.alloc(0x10) for _ in range(5)])


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        o = self.o = make_objects(self)
        # the vtables at the Mac's offsets: each slot holds the address of a TVector {code, toc}
        def tvec(code):
            a = e.alloc(8)
            e.write(a, struct.pack(">II", code, 0))
            return a
        wvt = e.alloc(0x40)
        e.write(wvt + 0x24, struct.pack(">I", tvec(A_SETVALUE)))
        e.write(wvt + 0x2C, struct.pack(">I", tvec(A_SETTEXT)))
        for p in o["pool"]:
            e.write(p + 0x118, struct.pack(">I", wvt))
        fvt = e.alloc(0x40)
        e.write(fvt + 0xC, struct.pack(">I", tvec(A_GETHEIGHT)))
        e.write(o["font"] + 0x10, struct.pack(">I", fvt))
        e.lc = o
        self.L = e.cl = CL.PpcLog(labels(o), case["queues"])
        self.saved = STUBS.activate(e)

    def alloc(self, n):
        return self.e.alloc(n)

    def g(self, name):
        return ADDR[name]

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, args)
        return r3 & MASK

    def format(self):
        r3, _, _ = T.call_ppc(self.e, FN_FORMAT, [])
        return self.L.lab(r3 & MASK) & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        o = self.o = make_objects(self)
        slots = o["pool"] + [o["world"], o["analyser"], o["font"]] + o["strs"]
        for i, a in enumerate(slots):
            lib.tpw_lc_setobj(i, a)
        fixed = {v: k for k, v in labels(o).items()}
        CL.host_begin(lib, fixed, case["queues"])

    def alloc(self, n):
        return self.lib.tpw_lc_alloc(n)

    def g(self, name):
        return self.lib.tpw_lc_addr(name.encode())

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        if fn == FN_POST:
            self.lib.tpw_lc_post(args[0])
            return 0
        return self.lib.tpw_lc_handler(args[0], T.s32(args[1]), T.s32(args[2]), T.s32(args[3])) & MASK

    def format(self):
        return self.lib.tpw_lc_format() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lc_addr.restype, lib.tpw_lc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lc_alloc.restype, lib.tpw_lc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lc_setobj.restype, lib.tpw_lc_setobj.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_lc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lc_format.restype, lib.tpw_lc_format.argtypes = C.c_int64, []
    lib.tpw_lc_post.restype, lib.tpw_lc_post.argtypes = None, [C.c_uint64]
    lib.tpw_lc_handler.restype = C.c_int32
    lib.tpw_lc_handler.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lc_layout(out)
    assert list(out)[:n] == [1], list(out)[:n]
    lib.bound = True


def build(s, case):
    o = s.o
    s.words(s.g("gStaffListFormat"), [case["fmt"]])
    s.words(s.g("gStaffListFont"), [case["font"]])
    s.write(s.g("gUIRealScreenHeight"), struct.pack(s.fmt[0] + "h", case["height"]))
    s.write(s.g("gStaffPanel"), struct.pack(s.pfmt, o["pool"][0]))
    s.write(s.g("gGameWorld"), struct.pack(s.pfmt, o["world"]))


def execute(s, case):
    o = s.o
    if case["op"] == "format":
        return s.format()
    if case["op"] == "post":
        s.call(FN_POST, [o["pool"][0]])      # void: r3 is whatever the last call left
        return 0
    return s.call(FN_HANDLER, [o["pool"][4], case["msg"] & MASK, case["a"] & MASK, case["b"] & MASK])


def observe(s, case, answer):
    lg = s.log()
    return {"answer": answer, "log": lg, "fmt": s.rd_words(s.g("gStaffListFormat"), 1), "font": s.rd_words(s.g("gStaffListFont"), 1),
            "height": struct.unpack(s.fmt[0] + "h", s.read(s.g("gUIRealScreenHeight"), 2))[0]}


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


def rint(r):
    return r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x18000, 0x8000, -0x8000, 0x12345, r.randrange(-1000, 1000), r.randrange(-2 ** 31, 2 ** 31)])


def base(r, op):
    c = {"op": op, "fmt": r.choice([0, 0, 1, -5, 22, r.randrange(-100, 100)]), "font": r.choice([7, 0, 1, 12, r.randrange(0, 2 ** 32)]),
         "height": r.choice([480, 600, 768, 1, -1, 2, r.randrange(1, 2000), -r.randrange(1, 2000), 0x7FFF, -0x7FFF]),
         "msg": 0x101, "a": 0x323, "b": 1}
    c["queues"] = {"GetHeight": [rint(r)], "AverageHappiness": [r.choice([0, 1, 50, 99, 100, 101, 255, -1, -100, r.randrange(-300, 300),
                                                                         r.randrange(-2 ** 20, 2 ** 20), 0x7FFFFFFF])],
                   "CallDefaultWindowHandler": [rint(r)]}
    return c


def f_case(r):
    return base(r, "format")


def p_case(r):
    return base(r, "post")


def h_case(r, msg=None, a=None, b=None):
    c = base(r, "handler")
    c["msg"] = r.choice([0x101, 0x101, 0x100, 0x102, 0, -1, r.randrange(-1000, 1000)]) if msg is None else msg
    c["a"] = r.choice([0x323, 0x324, 0x325, 0x326, 0x327, 0x322, 0x328, 0x321, 0x32A, r.randrange(-1000, 2000)]) if a is None else a
    c["b"] = r.choice([1, 1, 1, 0, 2, -1, r.randrange(-5, 5)]) if b is None else b
    return c


def f_edges():
    r = random.Random(101)
    for fmt in (0, 0, 5, -1):
        for hh in (480, 1, -1, 0x7FFF, -0x7FFF):
            c = f_case(r)
            c["fmt"], c["height"] = fmt, hh
            yield c
    for gh in (0, 16, 0x7FFF, 0x8000, 0x18000, -1, 0x12345, 0xFFFF8000, 0x7FFFFFFF):
        c = f_case(r)
        c["fmt"], c["queues"]["GetHeight"] = 0, [gh]
        yield c


def p_edges():
    r = random.Random(102)
    for font in (7, 0, 0xFFFFFFFF):
        c = p_case(r)
        c["font"] = font
        yield c


def h_edges():
    r = random.Random(103)
    for a in range(0x31F, 0x32B):
        yield h_case(r, 0x101, a, 1)
    for msg in (0x100, 0x102, 0, -1):
        yield h_case(r, msg, 0x323, 1)
    for b in (0, 2, -1):
        yield h_case(r, 0x101, 0x323, b)
    for avg in (0, 1, 99, 100, 101, 5000, -1, -99, -100, -101, 0x7FFFFFFF, -0x80000000, 0x1FFFFF, 0x200000, -0x200000):
        for a in (0x323, 0x327):
            c = h_case(r, 0x101, a, 1)
            c["queues"]["AverageHappiness"] = [avg]
            yield c


TESTS = [
    FnTest("LCFormat", ppc, host, edge=f_edges, rnd=f_case),
    FnTest("LCPostFormat", ppc, host, edge=p_edges, rnd=p_case),
    FnTest("AllStaffPanelLCHandler", ppc, host, edge=h_edges, rnd=h_case),
]
