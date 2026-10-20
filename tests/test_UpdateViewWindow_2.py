"""Unit v5984, the hire panel's functions, against libtpw_unit_UpdateViewWindow_2.dylib (tests/unit_UpdateViewWindow_2.cpp), main app
("SimTheme Park"), src/sinit_ui_FinancePanel_cpp_UpdateViewWindow_2.cpp:
  UI_FinancePanelPanelDateChange 0x101a0cf4, FillHireList 0x101a0fc8, UpdateViewWindow 0x101a10f4, HirePanelMsg 0x101a1314,
  LCFormat (HireListLCFormat) 0x101a1798

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit (window lookups, the staff database, the list control, the table's FormatString, the virtual calls through __ptr_glue, the
allocator and the constructors) is a fake on both sides that records the arguments it reads and answers from the case's queues; the
objects the fakes hand out (windows, staff records, the list, the bank account ...) are fixed blocks allocated first, named in the queues
as "@NAME".

The world of a case: the hire panel's globals, four staff records (type, name, pay grade, sub type, salary), the windows' z, the
finance graph globals, and the queues. What is compared: the answer, the globals the functions write (hire type, script, selection,
sort column, row height, the text buffer) and the event log.

No fused operations (the float work is exact int to float conversions, one fdivs and one fmuls). Not tested: UpdateViewWindow with a staff
type outside 0..4 (the original leaves its sprite register as the caller had it); the window results the original dereferences unchecked
(null) and a null gHirePanelWindow where the original reads it unchecked.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UpdateViewWindow_2"
MASK = 0xFFFFFFFF

FN_FIN, FN_FHL, FN_UVW, FN_HPM, FN_LCF = 0x101A0CF4, 0x101A0FC8, 0x101A10F4, 0x101A1314, 0x101A1798
SIZES = {FN_FIN: 144, FN_FHL: 252, FN_UVW: 504, FN_HPM: 1100, FN_LCF: 236}
OWN = set(SIZES)

# ---- callees faked on both sides
C_GETWINDOW, C_GETCLIENTRECT, C_CREATEOBJECT = 0x101DBE40, 0x101DBA4C, 0x101DC750
C_SETANIM, C_STARTANIM = 0x101DD5E0, 0x101DD6B8
C_CALCZ, C_GETSTAFF, C_HIRESTAFF = 0x101879D0, 0x101292F8, 0x10129348
C_GETNAME, C_GETSALARY = 0x10128768, 0x10128E1C
C_KILL, C_SPAWN, C_SETFLOAT = 0x100D9478, 0x100DA18C, 0x100D95A0
C_REMOVEALL, C_ADDITEM, C_UPDATEDISPLAY, C_GETDATA = 0x101D3CC8, 0x101D3000, 0x101D2220, 0x101D41F0
C_BANK, C_BALANCE = 0x1013EDFC, 0x100F32B4
C_FORMAT, C_LABS, C_GLUE = 0x10239730, 0x10239928, 0x1023BAA8
C_FONT, C_NEW, C_WINCTOR = 0x10180270, 0x102384B8, 0x101D98D8
C_RECTCTOR, C_RECTDTOR = 0x101C7448, 0x101C74E0
C_REMOVEPANEL, C_RIDELIST, C_ADVISOR, C_CANCELADV, C_DEFAULT = 0x101A29B0, 0x101B8434, 0x101821CC, 0x1018227C, 0x1018493C
C_MAXY, C_MINY, C_SETYRANGE, C_DRAW = 0x1019EF1C, 0x1019F0BC, 0x101FD7D8, 0x101F66C0

MK_SET, MK_TEXT, MK_HEIGHT = 0x7A000024, 0x7A00002C, 0x7A00000C   # what the fake vtables hold (slot 0x24, 0x2c; the font's slot 0xc)

ADDR = dict(world=0x1037EBFC, hirewin=0x10394924, hiretype=0x10394920, script=0x1039490C, selA=0x102AA328, selB=0x102AA32C,
            sort=0x102AA324, w10=0x10394910, w14=0x10394914, dep=0x1039491C, fmt=0x10394908, fontidx=0x102AA31C,
            screenh=0x10395BBA, table=0x1037FF18 + 0x564, graphpanel=0x10394904, grapher=0x10394894, kiss=0x10267BA0,
            spawn=0x1029FF70, cbuf=0x10380C48)
# staff name 4, pay grade 8, sub type 9, salary 0x10, the window's z 0xdc
HOST_LAYOUT = [4, 8, 9, 0x10, 360]   # the last: the host's offset of a window's mZ (the Mac's is 0xdc; the host class is not laid out like the Mac's)

NAMES = ["W0", "W1", "W2", "W3", "W4", "W5", "W6", "W7", "S0", "S1", "S2", "S3", "LIST", "BANK", "TBL", "FONT", "MSGWIN", "NEW", "VT", "FT",
         "WORLD"]
WINS = ["@W1", "@W2", "@W3", "@W4", "@W5", "@W6"]
OBJ = 0x200


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def rw(e, a):
    return struct.unpack(">I", e.read(a, 4))[0]


def rh(e, a):
    return struct.unpack(">H", e.read(a, 2))[0]


def chars(e, a, n):
    return [-1] * n if a == 0 else [rh(e, a + 2 * i) for i in range(n)]


def save_r2(e):
    """The glue functions store the caller's TOC pointer at 20(r1); the code after the call reloads it."""
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def fake(addr, fn):
    STUBS.internal(addr, fn)


def st_rectctor(e):
    e.cl.rec("RectCtor", (0,))


def st_rectdtor(e):
    e.cl.rec("RectDtor", (-1,))


def st_getwindow(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("GetWindow") & MASK)


def st_getclientrect(e):
    L = e.cl
    L.rec("GetClientRect", (L.lab(e.r(3)),))
    vals = [L.ret("RectL") & 0xFFFF, L.ret("RectT") & 0xFFFF, L.ret("RectR") & 0xFFFF, L.ret("RectB") & 0xFFFF]
    e.write(e.r(4) + 4, struct.pack(">4H", *vals))
    e.set_r(3, 0)


def st_createobject(e):
    L = e.cl
    L.rec("CreateObject", (L.lab(e.r(3)),) + tuple(s32(e.r(n)) for n in range(4, 11)))


def st_setanim(e):
    L = e.cl
    L.rec("setAnimTimer", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, L.ret("setAnim") & MASK)


def st_startanim(e):
    L = e.cl
    L.rec("startAnimTimer", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, 0)


def st_calcz(e):
    L = e.cl
    L.rec("CalcZTrans", (s32(e.r(3)),))
    e.set_f1(bitsf(L.ret("CalcZTrans")))


def st_getstaff(e):
    L = e.cl
    L.rec("GetStaff", (L.lab(e.r(3)), e.r(4) & MASK))
    e.set_r(3, L.ret("GetStaff") & MASK)


def st_hirestaff(e):
    L = e.cl
    L.rec("HireStaff", (L.lab(e.r(3)), e.r(4) & MASK))
    e.set_r(3, 0)


def st_getname(e):
    L = e.cl
    L.rec("GetName", (L.lab(e.r(3)), s32(e.r(5))))
    n = rw(e, e.r(3) + 4)
    e.write(e.r(4), struct.pack(">4H", 0x41 + (n & 15), 0x61 + ((n >> 4) & 15), 0x30 + ((n >> 8) & 15), 0))


def st_getsalary(e):
    L = e.cl
    L.rec("GetSalary", (L.lab(e.r(3)),))
    e.set_r(3, rw(e, e.r(3) + 0x10))


def st_kill(e):
    e.cl.rec("SCR_KillScript", (s32(e.r(3)),))


def st_spawn(e):
    L = e.cl
    L.rec("ESP_SpawnScript", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), canon_f(fb(e.f(1))), canon_f(fb(e.f(2))),
                              canon_f(fb(e.f(3)))))
    e.set_r(3, L.ret("ESP") & MASK)


def st_setfloat(e):
    e.cl.rec("SCR_SetFloatVariable", (s32(e.r(3)), e.r(4) & MASK, canon_f(fb(e.f(1)))))


def st_removeall(e):
    e.cl.rec("RemoveAllItems", (e.cl.lab(e.r(3)),))


def st_additem(e):
    L = e.cl
    item = e.r(4)
    text = rw(e, item)
    c = chars(e, text, 3)
    L.rec("AddItem", (L.lab(e.r(3)), c[0], c[1], c[2], s32(rw(e, item + 4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))
    e.set_r(3, 0)


def st_updatedisplay(e):
    e.cl.rec("UpdateDisplay", (e.cl.lab(e.r(3)),))


def st_getdata(e):
    L = e.cl
    L.rec("GetData", (L.lab(e.r(3)), s32(e.r(4))))
    e.write(e.r(5), struct.pack(">I", L.ret("GetData") & MASK))
    e.set_r(3, 0)


def st_bank(e):
    L = e.cl
    L.rec("GetBankAccount", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("Bank") & MASK)


def st_balance(e):
    L = e.cl
    L.rec("GetBalance", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("Balance") & MASK)


def st_format(e):
    L = e.cl
    save_r2(e)
    tag = e.r(7)
    value = rw(e, tag + 0xC)
    L.rec("FormatString", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK, e.r(6) & MASK, rw(e, tag), rw(e, tag + 8), s32(value)))
    e.write(e.r(4), struct.pack(">5H", e.r(6) & 0xFFFF, value & 0xFFFF, value >> 16, 0x5A, 0))
    e.set_r(3, 0)


def st_labs(e):
    save_r2(e)
    v = s32(e.r(3))
    e.cl.rec("Labs", (v,))
    e.set_r(3, (-v if v < 0 else v) & MASK)


def st_glue(e):
    L = e.cl
    save_r2(e)
    m = e.r(12) & MASK
    if m == MK_SET:
        L.rec("SetValue", (L.lab(e.r(3)), s32(e.r(4))))
    elif m == MK_TEXT:
        c = chars(e, e.r(4), 4)
        L.rec("SetText", (L.lab(e.r(3)), c[0], c[1], c[2], c[3]))
    elif m == MK_HEIGHT:
        L.rec("FontHeight", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("FontHeight") & MASK)
    else:
        raise AssertionError("unknown vtable slot %08x" % m)


def st_font(e):
    L = e.cl
    L.rec("GetFontTableEntry", (e.r(3) & MASK,))
    e.set_r(3, L.ret("Font") & MASK)


def st_new(e):
    L = e.cl
    L.rec("new", (e.r(3) & MASK,))
    e.set_r(3, L.ret("New") & MASK)


def st_winctor(e):
    L = e.cl
    parent = e.r(4)
    L.rec("WindowCtor", (L.lab(parent) if parent else 0,) + tuple(s32(e.r(n)) for n in range(5, 10)) + (s32(((e.r(10) & 0xFFFF) ^ 0x8000) - 0x8000),))


def st_removepanel(e):
    e.cl.rec("RemoveHireListPanel", (0,))


def st_ridelist(e):
    e.cl.rec("CreateRideListPanel", (s32(e.r(3)), (e.r(4) >> 16) & 0xFFFF))


def st_advisor(e):
    e.cl.rec("Advisor", (s32(e.r(3)),))


def st_canceladv(e):
    e.cl.rec("CancelAdvisor", (s32(e.r(3)),))


def st_default(e):
    L = e.cl
    L.rec("Default", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("Default") & MASK)


def st_maxy(e):
    L = e.cl
    L.rec("GetMaxY", (0,))
    e.set_r(3, L.ret("MaxY") & MASK)


def st_miny(e):
    L = e.cl
    L.rec("GetMinY", (0,))
    e.set_r(3, L.ret("MinY") & MASK)


def st_setyrange(e):
    L = e.cl
    L.rec("SetYRange", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))


def st_draw(e):
    L = e.cl
    L.rec("DrawMeGraph", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))


for _a, _f in [(C_GETWINDOW, st_getwindow), (C_GETCLIENTRECT, st_getclientrect), (C_CREATEOBJECT, st_createobject), (C_SETANIM, st_setanim),
               (C_STARTANIM, st_startanim), (C_CALCZ, st_calcz), (C_GETSTAFF, st_getstaff), (C_HIRESTAFF, st_hirestaff),
               (C_GETNAME, st_getname), (C_GETSALARY, st_getsalary), (C_KILL, st_kill), (C_SPAWN, st_spawn), (C_SETFLOAT, st_setfloat),
               (C_REMOVEALL, st_removeall), (C_ADDITEM, st_additem), (C_UPDATEDISPLAY, st_updatedisplay), (C_GETDATA, st_getdata),
               (C_BANK, st_bank), (C_BALANCE, st_balance), (C_FORMAT, st_format), (C_LABS, st_labs), (C_GLUE, st_glue),
               (C_FONT, st_font), (C_NEW, st_new), (C_WINCTOR, st_winctor), (C_RECTCTOR, st_rectctor), (C_RECTDTOR, st_rectdtor),
               (C_REMOVEPANEL, st_removepanel), (C_RIDELIST, st_ridelist), (C_ADVISOR, st_advisor), (C_CANCELADV, st_canceladv),
               (C_DEFAULT, st_default), (C_MAXY, st_maxy), (C_MINY, st_miny), (C_SETYRANGE, st_setyrange), (C_DRAW, st_draw)]:
    fake(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def setptr(self, a, v):
        """A pointer variable: a word on the Mac, 8 bytes on the host."""
        self.write(a, struct.pack("<Q", v) if self.fmt == "<I" else struct.pack(">I", v & MASK))

    def getptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0] if self.fmt == "<I" else self.rd_word(a)

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def make_objects(self):
        self.o = {n: self.alloc(OBJ) for n in NAMES}
        for a in self.o.values():
            self.write(a, bytes(OBJ))

    def resolve(self, queues):
        return {k: [self.o[v[1:]] if isinstance(v, str) else v for v in vs] for k, vs in queues.items()}

    def label_map(self):
        d = {self.o[n]: i + 1 for i, n in enumerate(NAMES)}
        for j, k in enumerate(["cbuf", "grapher", "kiss", "spawn"]):
            d[self.g(k)] = 90 + j
        return d


class Ppc(Side):
    fmt = ">I"
    zoff = 0xDC

    def __init__(self, e, case):
        self.e = e
        self.make_objects()
        self.L = e.cl = CL.PpcLog(self.label_map(), self.resolve(case["queues"]))
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

    def call(self, addr, args):
        r3, _, _ = T.call_ppc(self.e, addr, [a & MASK for a in args])
        return r3 & MASK

    def ptr(self, name):
        return self.o[name]


class Host(Side):
    fmt = "<I"
    zoff = 360

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_objects()
        CL.host_begin(lib, {n: a for a, n in self.label_map().items()}, self.resolve(case["queues"]))

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_hmsg_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_hmsg_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, addr, args):
        lib = self.lib
        if addr == FN_FIN:
            lib.tpw_hmsg_fin(s32(args[0]))
            return 0
        if addr == FN_FHL:
            lib.tpw_hmsg_fhl(s32(args[0]), s32(args[1]))
            return 0
        if addr == FN_UVW:
            lib.tpw_hmsg_uvw(s32(args[0]))
            return 0
        if addr == FN_HPM:
            return lib.tpw_hmsg_hpm(args[0], s32(args[1]), s32(args[2]), s32(args[3])) & MASK
        return lib.tpw_hmsg_lcf()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for name, res, arg in [("addr", C.c_uint64, [C.c_char_p]), ("alloc", C.c_uint64, [C.c_uint32]), ("layout", C.c_int32, [C.POINTER(C.c_int64)]),
                           ("fin", None, [C.c_int32]), ("fhl", None, [C.c_int32, C.c_int32]), ("uvw", None, [C.c_int32]),
                           ("hpm", C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]), ("lcf", C.c_uint64, [])]:
        f = getattr(lib, "tpw_hmsg_" + name)
        f.restype, f.argtypes = res, arg
    out = (C.c_int64 * 32)()
    n = lib.tpw_hmsg_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    g = case["g"]
    o = s.o
    for name in ("world", "hirewin", "graphpanel"):
        v = g[name]
        s.setptr(s.g(name), o[v[1:]] if isinstance(v, str) else v)
    for name in ("hiretype", "script", "selA", "selB", "sort", "w10", "w14", "dep", "fmt", "fontidx"):
        s.words(s.g(name), [g[name]])
    s.half(s.g("screenh"), g["screenh"])
    s.setptr(s.g("table"), o["TBL"])
    # the fake vtables: window +0x118 -> VT, VT +0x24 / +0x2c the markers; font +0x10 -> FT, FT +0xc the height marker
    s.words(o["VT"] + 0x24, [MK_SET])
    s.words(o["VT"] + 0x2C, [MK_TEXT])
    s.words(o["FT"] + 0xC, [MK_HEIGHT])
    s.words(o["FONT"] + 0x10, [o["FT"]])
    for n in ["W0", "W1", "W2", "W3", "W4", "W5", "W6", "W7", "MSGWIN"]:
        s.words(o[n] + 0x118, [o["VT"]])
        s.half(o[n] + s.zoff, case["z"])
    for i, (typ, name, grade, sub, timeout) in enumerate(case["staff"]):
        a = o["S%d" % i]
        s.words(a, [typ, name])
        s.write(a + 8, bytes([grade, sub]))
        s.words(a + 0x10, [timeout])


def execute(s, case):
    op, args = case["op"], case["args"]
    if op in ("fin", "fhl", "uvw"):
        s.call({"fin": FN_FIN, "fhl": FN_FHL, "uvw": FN_UVW}[op], args)
        return None
    if op == "hpm":
        a = list(args)
        a[0] = s.o["MSGWIN"]
        return s32(s.call(FN_HPM, a))
    r = s.call(FN_LCF, [])
    return "NEW" if r == s.o["NEW"] else r


def observe(s, case, answer):
    names = ("hiretype", "script", "selA", "selB", "sort", "fmt")
    return {"answer": answer, "log": s.log(), "g": [s.rd_word(s.g(n)) for n in names],
            "hirewin_null": s.getptr(s.g("hirewin")) == 0,
            "cbuf": list(struct.unpack(s.fmt[0] + "5H", s.read(s.g("cbuf"), 10)))}


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

SPECIAL_F = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL_F)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def r32(r):
    return r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x10000, 0xFFFF, r.randrange(-100000, 100000), r.getrandbits(32) - 0x80000000])


def r16(r):
    return r.choice([0, 1, -1, 0x7FFF, -0x8000, r.randrange(-1000, 1000), r.randrange(-0x8000, 0x8000)])


def staff_rec(r, hi=4):
    return (r.randrange(0, hi + 1), r.getrandbits(32), r.getrandbits(8), r.choice([0, 1, 2, 255, r.getrandbits(8)]), r.getrandbits(32))


def pick_staff(r):
    return r.choice(["@S0", "@S1", "@S2", "@S3", "@S0", "@S1", 0])


def pick_win(r):
    return r.choice(WINS)


def base(r, op, hi=4):
    sel_staff = None
    c = {"op": op, "z": r16(r), "staff": [staff_rec(r, hi) for _ in range(4)], "args": [0, 0, 0, 0],
         "g": {"world": "@WORLD", "hirewin": "@W0" if r.random() < 0.9 else 0, "graphpanel": "@W7" if r.random() < 0.7 else 0,
               "hiretype": r32(r), "script": r.choice([0, 0, 1, 77, r32(r)]), "selA": r.choice([-1, 0, 1, 2, 3, 4, r32(r)]),
               "selB": r.choice([-1, 0, 1, 2, 255, r32(r)]), "sort": r32(r), "w10": r32(r), "w14": r32(r), "dep": r32(r),
               "fmt": r.choice([0, 0, 0, 30, r32(r)]), "fontidx": r.choice([7, 0, r.getrandbits(32)]), "screenh": r.choice([0, 768, 600, r16(r), 1])},
         "queues": {"Default": [r32(r)], "RectL": [r16(r)], "RectT": [r16(r)], "RectR": [r16(r)], "RectB": [r16(r)],
                    "CalcZTrans": [rf(r)], "ESP": [r32(r)], "GetWindow": [pick_win(r)], "GetStaff": [pick_staff(r)]}}
    return c


# ---- UI_FinancePanelPanelDateChange
def fin_case(r, panel=None, month=None):
    c = base(r, "fin")
    if panel is not None:
        c["g"]["graphpanel"] = "@W7" if panel else 0
    c["args"] = [r32(r) if month is None else month]
    c["queues"].update({"MaxY": [r32(r)], "MinY": [r32(r)], "GetWindow": [r.choice(WINS + [0])]})
    return c


def fin_edges():
    r = random.Random(11)
    for panel in (0, 1, 1):
        yield fin_case(r, panel)
    yield fin_case(r, 1, 0)


def fin_random(r):
    return fin_case(r)


# ---- FillHireList
def fhl_case(r, typ=None, flag=None, hirewin=None, list_=None, nstaff=None):
    c = base(r, "fhl", 4)
    if typ is None:
        typ = r.choice([0, 1, 2, 3, 4, 4, 2, 9, -1])
    if flag is None:
        flag = r.choice([0, 1, 5, -1])
    if hirewin is not None:
        c["g"]["hirewin"] = "@W0" if hirewin else 0
    lst = r.choice(["@LIST", "@LIST", 0]) if list_ is None else ("@LIST" if list_ else 0)
    c["args"] = [typ, flag]
    c["queues"]["GetWindow"] = [lst]
    c["queues"]["GetStaff"] = [r.choice(["@S0", "@S1", "@S2", "@S3", 0]) for _ in range(32)]
    return c


def fhl_edges():
    r = random.Random(21)
    yield fhl_case(r, 2, 1, hirewin=0)
    yield fhl_case(r, 2, 1, hirewin=1, list_=0)
    for flag in (0, 1):
        for typ in range(5):
            yield fhl_case(r, typ, flag, hirewin=1, list_=1)


def fhl_random(r):
    return fhl_case(r)


# ---- UpdateViewWindow
def uvw_case(r, slot=None, same=None, script=None, nullstaff=False):
    c = base(r, "uvw", 4)
    slot = r.choice([0, 1, 5, 31, -1, r.getrandbits(32) - 0x80000000]) if slot is None else slot
    st = None if nullstaff else r.choice(["@S0", "@S1", "@S2", "@S3"])
    c["queues"]["GetWindow"] = [pick_win(r)]
    c["queues"]["GetStaff"] = [0 if nullstaff else st]
    if st is not None:
        t, _, _, sub, _ = c["staff"][int(st[2:])]
        if same is True:
            c["g"]["selA"], c["g"]["selB"] = t, sub
        elif same == "typeonly":
            c["g"]["selA"], c["g"]["selB"] = t, (sub + 1) & 0xFF
        elif same == "subonly":
            c["g"]["selA"], c["g"]["selB"] = (t + 1) & 7, sub
        elif r.random() < 0.25:
            c["g"]["selA"], c["g"]["selB"] = t, sub
    if script is not None:
        c["g"]["script"] = script
    c["args"] = [slot]
    return c


def uvw_edges():
    r = random.Random(31)
    yield uvw_case(r, 3, nullstaff=True)
    for same in (True, "typeonly", "subonly", False):
        for script in (0, 9):
            yield uvw_case(r, 2, same, script)
    for _ in range(10):
        yield uvw_case(r, 1, False, 0)


def uvw_random(r):
    return uvw_case(r)


# ---- HirePanelMsg
def hpm_case(r, message, a=None, b=None):
    c = base(r, "hpm", 4)
    q = c["queues"]
    if message in (0x400, 0x401):
        a = r.choice([0x248E, 0x2491, r32(r)]) if a is None else a
        b = r.choice([-1, 0, 1, 7, r.randrange(0, 40)]) if b is None else b
    else:
        a = r.choice([0, 1, -2, 0x2495, 0x80080, 0x80081, 0x2494, r32(r)]) if a is None else a
        b = r.choice([0, 1, -1, 7, r32(r)]) if b is None else b
    if message == 0x10 and a == 0x80080:
        q["GetWindow"] = [pick_win(r)]
        q["Bank"] = ["@BANK"]
        q["Balance"] = [r.choice([-5, 0, 7, -0x80000000, 0x7FFFFFFF, r32(r), -123456, 98765])]
    elif message == 0x15:
        q["setAnim"] = [r.choice([0, 1, 0x10000, -1, 0x7FFF0000, 5, r32(r)])]
    elif message == 0x400:
        q["GetWindow"] = [r.choice(["@LIST", "@LIST", 0])]
        q["GetData"] = [r.randrange(0, 40) if r.random() < 0.7 else r32(r)]
    elif message == 0x401:
        lst = r.choice(["@LIST", "@LIST", "@LIST", 0])
        gw = [lst]
        sq = []
        if lst != 0 and b != -1:
            q["GetData"] = [r.randrange(0, 40) if r.random() < 0.7 else r32(r)]
            s1 = pick_staff(r)
            sq.append(s1)
            header = r.choice(WINS + [0])
            gw.append(header)
            if header != 0 and s1 != 0:
                gw.append(pick_win(r))
            s2 = pick_staff(r)
            sq.append(s2)
            summary = r.choice(WINS + [0])
            gw.append(summary)
            if summary != 0 and s2 != 0:
                gw += [pick_win(r), pick_win(r)]
            gw.append(pick_win(r))
            sq.append(pick_staff(r))
            c["g"]["hirewin"] = "@W0"
            # the view window shows another type half of the time, the same one the other half
            if r.random() < 0.3 and sq[2] != 0:
                t, _, _, sub, _ = c["staff"][int(sq[2][2:])]
                c["g"]["selA"], c["g"]["selB"] = t, sub
        q["GetWindow"] = gw
        q["GetStaff"] = sq
    c["args"] = [0, message, a, b]
    return c


MESSAGES = [0x100, 5, 0x10, 0x14, 0x15, 0x400, 0x401, 0x406]
OTHER = [0, 1, 4, 6, 0xF, 0x11, 0x13, 0x16, 0x101, 0x3FF, 0x402, 0x405, 0x407, -1, 0x7FFFFFFF, -0x80000000, 0xFF, 0x1FF]


def hpm_edges():
    r = random.Random(41)
    for a in (-2, 0x2495, 0, 0x2496, -1):
        yield hpm_case(r, 0x100, a)
    yield hpm_case(r, 5)
    for a in (0x80080, 0x80081, 0x80000, 0x180080, 0):
        yield hpm_case(r, 0x10, a)
    for _ in range(6):
        yield hpm_case(r, 0x10, 0x80080)
    yield hpm_case(r, 0x14)
    yield hpm_case(r, 0x14)
    yield hpm_case(r, 0x15)
    yield hpm_case(r, 0x15)
    yield hpm_case(r, 0x15)
    for b in (-1, 0, 1, 33):
        yield hpm_case(r, 0x400, b=b)
        yield hpm_case(r, 0x400, b=b)
    for b in (-1, 0, 5):
        for _ in range(8):
            yield hpm_case(r, 0x401, b=b)
    yield hpm_case(r, 0x406, b=3)
    yield hpm_case(r, 0x406, b=-1)
    for m in OTHER:
        yield hpm_case(r, m)


def hpm_random(r):
    m = r.choice(MESSAGES + MESSAGES + OTHER)
    if m == 0x10 and r.random() < 0.6:
        return hpm_case(r, m, 0x80080)
    return hpm_case(r, m)


# ---- LCFormat
def lcf_case(r, fmt=None, screenh=None, new=True):
    c = base(r, "lcf")
    if fmt is not None:
        c["g"]["fmt"] = fmt
    if screenh is not None:
        c["g"]["screenh"] = screenh
    c["queues"]["Font"] = ["@FONT"]
    c["queues"]["FontHeight"] = [r.choice([0, 1, 12, 16, -1, 0x7FFF, 0x8000, 0xFFFF, 0x12345, r32(r)])]
    c["queues"]["New"] = ["@NEW" if new else 0]
    return c


def lcf_edges():
    r = random.Random(51)
    yield lcf_case(r, 0)
    yield lcf_case(r, 0, 768)
    yield lcf_case(r, 0, 0)
    yield lcf_case(r, 0, -1)
    yield lcf_case(r, 0, 1)
    yield lcf_case(r, 0, 768, new=False)
    yield lcf_case(r, 30)
    yield lcf_case(r, -4, new=False)
    yield lcf_case(r, 99, 600)


def lcf_random(r):
    return lcf_case(r, r.choice([0, 0, 0, 0, 30, r32(r)]), new=r.random() < 0.85)


TESTS = [
    FnTest("UI_FinancePanelPanelDateChange", ppc, host, edge=fin_edges, rnd=fin_random),
    FnTest("FillHireList", ppc, host, edge=fhl_edges, rnd=fhl_random),
    FnTest("UpdateViewWindow", ppc, host, edge=uvw_edges, rnd=uvw_random),
    FnTest("HirePanelMsg", ppc, host, edge=hpm_edges, rnd=hpm_random),
    FnTest("LCFormat", ppc, host, edge=lcf_edges, rnd=lcf_random),
]
