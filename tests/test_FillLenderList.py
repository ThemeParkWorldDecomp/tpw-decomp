"""Unit v5145, the lenders list of the bank panel, against libtpw_unit_FillLenderList.dylib (tests/unit_FillLenderList.cpp), main app
("SimTheme Park"), src/FillLenderList.cpp:
  FillLenderList 0x101a4a38, LCPostFormat 0x101a4660 (host name LenderLCPostFormat), LCFormat 0x101a44b8 (host name LenderLCFormat)

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit are fakes on both
sides that record their arguments and answer from the case: the list control (RemoveAllItems, AddItem), the bank account (GetBankAccount,
GetLoanInfo), the string table (GetConstString), UIMain_ConvertMulti, GetFontTableEntry, the window members (SetClientDrawObject,
SetFont, SetCallbackFunction, SetFontJust, GetChild, GetNext, CreateObject, the constructor), the draw object constructors, operator
new, and the two virtual calls the original makes through __ptr_glue (the font's height and the window's SetValue; the stub tells them
by the table word in r12). Text pointers are small tokens (the string and the converted text), so the log names them by number.

The world of a case: the mode, the row height, the font index and the screen height globals, the eight loans (a queue of eight fields
each), the string and conversion answers, the font's height. What is compared: the answer (a label), the three globals, the event log.

No fused operations. Not tested: a screen height of 0 (divw by zero is undefined on the hardware); the draw objects' and the window's
own constructors (callees, faked); the host's different object sizes (the fakes never touch the objects).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FillLenderList"
MASK = 0xFFFFFFFF

FN_FILL, FN_POST, FN_FORMAT = 0x101A4A38, 0x101A4660, 0x101A44B8
SIZES = {FN_FILL: 376, FN_POST: 932, FN_FORMAT: 392}
OWN = set(SIZES)

ADDR = dict(mode=0x1039497C, rowh=0x10394978, fontidx=0x102AA838, screenh=0x10395BBA, world=0x1037EBFC, text=0x1037FF18)
CALLBACK_TVECTOR = 0x10267C10
FONT_FN, WIN_FN = 0x77770001, 0x77770002          # the table words the two virtual calls read (told apart in the __ptr_glue stub)
STR_BASE, TOK_BASE = 0x60000, 0x50000
HOST_LAYOUT = [32, 0x1C, 8]

LABELS = dict(list=1, font=2, worldobj=3, acct=4, table=5, win0=10, win1=11, win2=12, win3=13, win4=14, win5=15)
OBJ = {}                                           # the PowerPC addresses of the case objects (set in setup)


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def ret_of(name):
    return lambda e, log: OBJ[name]


STUBS.internal(0x101D3CC8, CL.recorder("RemoveAllItems", "L"))
STUBS.internal(0x1013EDFC, CL.recorder("GetBankAccount", "L", ret=ret_of("acct")))
STUBS.internal(0x10180270, CL.recorder("GetFontTableEntry", "u", ret=ret_of("font")))
STUBS.internal(0x101DB624, CL.recorder("SetClientDrawObject", "LL"))
STUBS.internal(0x101DA118, CL.recorder("SetFont", "LL"))
STUBS.internal(0x101DA204, CL.recorder("SetFontJust", "Lii"))
STUBS.internal(0x1018D11C, CL.recorder("SpriteCtor", "Nu"))
STUBS.internal(0x1018DE60, CL.recorder("CashCtor", "Nu"))
STUBS.internal(0x1018D930, CL.recorder("NumberCtor", "Nu"))
STUBS.internal(0x1018E874, CL.recorder("PercentCtor", "Nu"))
STUBS.internal(0x101D98D8, CL.recorder("WindowCtor", "Nuiihhhh"))
STUBS.internal(0x101DC750, CL.recorder("CreateObject", "Liiihhhh"))


def stub_loan(e):
    """CLoan CBankAccount::GetLoanInfo(uint32_t): r3 the result, r4 the account, r5 the loan; the eight fields come from the queue."""
    L = e.cl
    out, i = e.r(3), e.r(5)
    L.rec("GetLoanInfo", (L.lab(e.r(4)), i))
    e.write(out, b"".join(struct.pack(">I", L.ret("loan:%d" % (i * 8 + f)) & MASK) for f in range(8)))


def stub_const_string(e):
    L = e.cl
    L.rec("GetConstString", (L.lab(e.r(3)), e.r(4)))
    e.set_r(3, STR_BASE + (L.ret("cs:%d" % e.r(4)) & 15))


def stub_convert(e):
    L = e.cl
    k = s32(e.r(3) - STR_BASE)
    L.rec("ConvertMulti", (k,))
    e.set_r(3, TOK_BASE + (L.ret("cm:%d" % k) & 15))


def stub_add_item(e):
    L = e.cl
    w = struct.unpack(">6I", e.read(e.r(4), 24))
    L.rec("AddItem", (L.lab(e.r(3)), s32(w[0] - TOK_BASE), s32(w[1]), s32(w[2]), s32(w[3]), s32(w[4]), s32(w[5]), s32(e.r(5)),
                      s32(e.r(6)), s32(e.r(7))))


def stub_callback(e):
    L = e.cl
    L.rec("SetCallbackFunction", (L.lab(e.r(3)), 1 if e.r(4) == CALLBACK_TVECTOR else 0))


def stub_child(e):
    L = e.cl
    L.rec("GetChild", (L.lab(e.r(3)),))
    e.set_r(3, OBJ["win1"])


def stub_next(e):
    L = e.cl
    L.rec("GetNext", (L.lab(e.r(3)),))
    here = e.r(3)
    for i in range(5):
        if OBJ["win%d" % i] == here:
            e.set_r(3, OBJ["win%d" % (i + 1)])
            return
    e.set_r(3, 0)


def stub_glue(e):
    """__ptr_glue: the font's height (table word FONT_FN) or the window's SetValue (WIN_FN, r4 the value)."""
    L = e.cl
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))      # the glue saves the TOC where the caller reloads it from
    if e.r(12) == FONT_FN:
        L.rec("GetHeight", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("height:0") & MASK)
    else:
        assert e.r(12) == WIN_FN
        L.rec("SetValue", (L.lab(e.r(3)), s32(e.r(4))))


def stub_new(e):
    L = e.cl
    size = e.r(3)
    L.rec("new", (size,))
    e.set_r(3, e.alloc(max(size, 8)))


STUBS.internal(0x100F3C44, stub_loan)
STUBS.imp("GetConstString__8TbITableCFUl", stub_const_string)
STUBS.internal(0x101803CC, stub_convert)
STUBS.internal(0x101D3000, stub_add_item)
STUBS.internal(0x101DCBA8, stub_callback)
STUBS.internal(0x101DBAC4, stub_child)
STUBS.internal(0x101DBB38, stub_next)
STUBS.internal(0x1023BAA8, stub_glue)
STUBS.imp("__nw__FUl", stub_new)


def setup(runner):
    T.install_std_stubs(runner.emu)
    emu = runner.emu
    for name in ("list", "worldobj", "acct", "table"):
        OBJ[name] = T.persist_alloc(emu, 0x400)
    vt_font = T.persist_alloc(emu, 0x40, b"\0" * 0xC + struct.pack(">I", FONT_FN) + b"\0" * 0x30)
    OBJ["font"] = T.persist_alloc(emu, 0x40, b"\0" * 0x10 + struct.pack(">I", vt_font) + b"\0" * 0x2C)
    vt_win = T.persist_alloc(emu, 0x40, b"\0" * 0x24 + struct.pack(">I", WIN_FN) + b"\0" * 0x18)
    for i in range(6):
        OBJ["win%d" % i] = T.persist_alloc(emu, 0x140, b"\0" * 0x118 + struct.pack(">I", vt_win) + b"\0" * 0x24)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "h", T.s16(v)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({OBJ[n]: v for n, v in LABELS.items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name] if name in ADDR else OBJ[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_world_and_table(self):
        self.words(ADDR["world"], [OBJ["worldobj"]])
        self.words(ADDR["text"] + 0x594, [OBJ["table"]])

    def call(self, fn, args):
        r3 = T.call_ppc(self.e, fn, args)[0] & MASK
        return self.L.lab(r3) if fn == FN_FORMAT else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {v: self.g(n) for n, v in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lender_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_world_and_table(self):
        self.lib.tpw_lender_set_world(self.g("worldobj"))
        self.lib.tpw_lender_set_table(self.g("table"))

    def call(self, fn, args):
        if fn == FN_FILL:
            self.lib.tpw_lender_fill(self.g("list"))
        elif fn == FN_POST:
            self.lib.tpw_lender_post(self.g("win0"))
        else:
            return self.lib.tpw_lender_format()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    f = lib.tpw_lender_addr
    f.restype, f.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lender_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lender_set_table.argtypes = [C.c_uint64]
    lib.tpw_lender_set_world.argtypes = [C.c_uint64]
    lib.tpw_lender_fill.argtypes = [C.c_uint64]
    lib.tpw_lender_post.argtypes = [C.c_uint64]
    lib.tpw_lender_format.restype = C.c_int32
    out = (C.c_int64 * 32)()
    n = lib.tpw_lender_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("mode"), [case["mode"]])
    s.words(s.g("rowh"), [case["rowh"]])
    s.words(s.g("fontidx"), [case["fontidx"]])
    s.half(s.g("screenh"), case["screenh"])
    s.set_world_and_table()


def execute(s, case):
    if case["op"] == "fill":
        return s.call(FN_FILL, [s.g("list") & MASK])
    if case["op"] == "post":
        return s.call(FN_POST, [s.g("win0") & MASK])
    return s.call(FN_FORMAT, [])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "mode": s.rd_words(s.g("mode"), 1), "rowh": s.rd_words(s.g("rowh"), 1),
            "fontidx": s.rd_words(s.g("fontidx"), 1)}


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


# ---------------------------------------------------------------------------------------------------------------- cases

U32_EDGES = [0, 1, 2, 7, 100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 12345, 0x10001]


def ru(r):
    return r.choice(U32_EDGES) if r.random() < 0.3 else r.randrange(0, 100000)


def base(r, op):
    q = {}
    for k in range(4):
        q["cs:%d" % k] = [r.randrange(0, 16)]
    for k in range(16):
        q["cm:%d" % k] = [r.randrange(0, 16)]
    q["height:0"] = [r.choice([0, 1, 12, 0x7FFF, 0x8000, 0xFFFF, 0xFFFF8000, 0x12345, r.randrange(0, 60), r.randrange(0, 1 << 32)])]
    return {"op": op, "queues": q, "mode": r.choice([0, 1, 2]), "rowh": 0, "fontidx": r.choice([7, 0, 3, 12, 0xFFFFFFFF]),
            "screenh": r.choice([768, 1536, 1, -1, 0x7FFF, -0x8000, r.randrange(1, 4000)])}


def loan(r, avail, bought):
    return [avail & MASK, ru(r), ru(r), ru(r), ru(r), bought & MASK, ru(r), r.randrange(0, 4)]


def fill_case(r, mode=None, pairs=None):
    c = base(r, "fill")
    if mode is not None:
        c["mode"] = mode
    for i in range(8):
        if pairs is not None:
            avail, bought = pairs[i]
        else:
            avail, bought = r.choice([0, 1, 5, -1]), r.choice([0, 0, 1, 3])
        for f, v in enumerate(loan(r, avail, bought)):
            c["queues"]["loan:%d" % (i * 8 + f)] = [v]
    return c


PAIRS = [(1, 0), (0, 0), (1, 1), (0, 1), (2, 0), (1, 5), (0, 3), (-1, 0)]


def fill_edges():
    r = random.Random(5145)
    for mode in (0, 1, 2, -1, 3):
        yield fill_case(r, mode, PAIRS)
        yield fill_case(r, mode, PAIRS[::-1])
    for mode in (0, 1):
        yield fill_case(r, mode, [(1, 0)] * 8)
        yield fill_case(r, mode, [(0, 1)] * 8)
        yield fill_case(r, mode, [(0, 0)] * 8)


def fill_random(r):
    return fill_case(r)


def post_case(r):
    return base(r, "post")


def post_edges():
    r = random.Random(5146)
    for idx in (7, 0, 0xFFFFFFFF):
        c = post_case(r)
        c["fontidx"] = idx
        yield c


def format_case(r, rowh=None, screenh=None, height=None):
    c = base(r, "format")
    c["rowh"] = r.choice([0, 0, 0, 1, 14, 0x12345678, 0xFFFFFFFF]) if rowh is None else rowh
    if screenh is not None:
        c["screenh"] = screenh
    if height is not None:
        c["queues"]["height:0"] = [height]
    return c


def format_edges():
    r = random.Random(5147)
    for screenh in (768, 1536, 1, -1, 0x7FFF, -0x8000, 1000):
        for height in (0, 1, 12, 0x7FFF, 0x8000, 0xFFFF, 0xFFFF8000, 0x12345, 0xFFFFFFFF, 0x7FFFFFFF):
            yield format_case(r, 0, screenh, height)
    for rowh in (1, 14, 0x12345678, 0xFFFFFFFF, 0x10000):
        yield format_case(r, rowh)


TESTS = [
    FnTest("FillLenderList", ppc, host, edge=fill_edges, rnd=fill_random),
    FnTest("LCPostFormat", ppc, host, edge=post_edges, rnd=post_case),
    FnTest("LCFormat", ppc, host, edge=format_edges, rnd=format_case),
]
