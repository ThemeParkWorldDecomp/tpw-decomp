"""Unit v51019, the hire panel's builder, against libtpw_unit_CreateHireListPanel_2.dylib (tests/unit_CreateHireListPanel_2.cpp), main app
("SimTheme Park"), src/HirePanelLCHandler_CreateHireListPanel_2.cpp:
  CreateHireListPanel 0x101a1c88

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the window lookups, the draw object constructors and the new they follow, the font and string tables, the bank and analyser
getters, the histories, the table's FormatString with the integer tag it is given, the virtual SetValue / SetText calls through the table at
window +0x118) are fakes on both sides that record their arguments and answer from the case: windows, strings and fonts are blocks looked up
by label, the numbers (balance, history values) come from the queues.

The world of a case: the staff type asked for and the panel's current one, the panel window (up or not), whether Build answers, which child
windows exist, the balance, the three history values, the sort column, the column colour and the four summary words. What is compared: the
event log, gHirePanelType, gHirePanelWindow (as a label), the four summary words, the colour and the sort column.

No fused operations. Not tested: the children the original uses without a NULL check and then dereferences (the cash line 0x2496, the
header bar 0x2482, the four cash rows of the summary): they always exist, as the original would fault on the PowerPC side too.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateHireListPanel_2"
MASK = 0xFFFFFFFF

FN = 0x101A1C88
SIZES = {FN: 3316}
OWN = set(SIZES)

ADDR = dict(
    gGameWindow=0x103949E4, gGameWorld=0x1037EBFC, gHirePanelType=0x10394920, gHirePanelWindow=0x10394924,
    gTextTable=0x1037FF18 + 0x564, gUIMainConvertBuffer=0x10380C48, gHirePanelScript=0x102A9EE8,
    gHireColumnColour=0x102AA320, gHireSortColumn=0x102AA324, gHireListWord10=0x10394910, gHireListWord14=0x10394914,
    gHireListWithdrawals=0x10394918, gHireListDeposits=0x1039491C, gHireListStrings=0x1024E0C3,
)
HOST_LAYOUT = [592, 1]

# labels of the named objects and globals
L_GAME, L_BUILT, L_EXISTING, L_BANK, L_ANALYSER, L_DRAWOBJ, L_TABLE, L_BUFFER, L_WORLD = (0x40001, 0x40002, 0x40003, 0x40004, 0x40005,
                                                                                         0x40006, 0x40007, 0x40008, 0x40009)
GLOBAL_LABELS = dict(gUIMainConvertBuffer=L_BUFFER, gHireListDeposits=0x40010, gHireListWord10=0x40011, gHireListWithdrawals=0x40012,
                     gHireListWord14=0x40013)
FUNCTIONS = {0x10267BB8: 1, 0x10267BB0: 2, 0x10267BC0: 3, 0x10267BC8: 4}   # TVectors: row maker, attach, list handler, panel message
DESC_SET_VALUE, DESC_SET_TEXT = 0x70000024, 0x7000002C

WINDOWS = [0x248E, 0x248F, 0x2496, 0x10, 0x11, 0x2481, 0x2483, 0x2482, 0x2485, 0x2488, 0x248D, 0x248A, 0x2486, 0x2489, 0x248C, 0x248B,
           0x2487]
MANDATORY = {0x2496}
STRINGS = [0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x165, 0x166, 0x167, 0x169]
FONTS = [5, 6]
CUR = {}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def obj(label):
    return CUR.get(label, 0)


def reg(name, addr, spec, answer=None):
    """A fake that records the registers per spec (L label, i s32, u u32, h s16, f function id) and answers a label's object."""

    def fn(e):
        L = e.cl
        vals = []
        for k, c in enumerate(spec):
            r = e.r(3 + k)
            if c == "L":
                vals.append(L.lab(r))
            elif c == "i":
                vals.append(s32(r))
            elif c == "u":
                vals.append(r & MASK)
            elif c == "h":
                vals.append(T.s16(r))
            elif c == "f":
                vals.append(FUNCTIONS.get(r, 0 if r == 0 else -1))
            elif c == "b":
                vals.append(1 if (r & 0xFF) else 0)
        L.rec(name, tuple(vals))
        if answer is not None:
            e.set_r(3, answer(e) & MASK)

    STUBS.internal(addr, fn)


reg("CheckPanel", 0x10180514, "")
reg("SetLastPanel", 0x101A57FC, "ii")
reg("FillHireList", 0x101A0FC8, "ii")
reg("GetString", 0x10180468, "i", lambda e: obj(0x20000 + s32(e.r(3))))
reg("SetTitleText", 0x10180794, "LiL")
reg("SetPanel", 0x101805A0, "L")
reg("IndentClient", 0x10180940, "Lhh")
reg("AdvisorMessage", 0x101821CC, "i")
reg("GetFontTableEntry", 0x10180270, "u", lambda e: obj(0x30000 + (e.r(3) & MASK)))
reg("MAKECOLUMNBUTTON", 0x101818E4, "LLiiiii")
reg("GetWindow", 0x101DBE40, "Li", lambda e: obj(0x10000 + s32(e.r(4))))
reg("SetDrawObject", 0x101DB474, "LL")
reg("SetClientDrawObject", 0x101DB624, "LL")
reg("SetClientDrawText", 0x101DE130, "LL")
reg("SetFontJust", 0x101DA204, "Lii")
reg("SetFont", 0x101DA118, "LL")
reg("SetFontColour", 0x101DA3AC, "Liiii")
reg("SetCallbackFunction", 0x101DCBA8, "Lf")
reg("SetCurrent", 0x101CC148, "Li")
reg("SetColumnFormatsAll", 0x101D2864, "Lff")
reg("SetColumnType", 0x101D2614, "Lhi")
reg("SetSelectionObject", 0x101D1EB4, "LL")
reg("SetSort", 0x101D4DB8, "Lib")
reg("GetBankAccount", 0x1013EDFC, "L", lambda e: obj(L_BANK))
reg("GetParkAnalyser", 0x1013EBC8, "L", lambda e: obj(L_ANALYSER))


def stub_build(e):
    L = e.cl
    L.rec("Build", (L.lab(e.r(3)), 1 if e.r(4) == ADDR["gHirePanelScript"] else 0, FUNCTIONS.get(e.r(5), 0 if e.r(5) == 0 else -1)))
    e.set_r(3, obj(L_BUILT))


STUBS.internal(0x101DEB78, stub_build)


def stub_find(e):
    L = e.cl
    L.rec("FindDrawObject", (s32(e.r(3) - ADDR["gHireListStrings"]),))
    e.set_r(3, obj(L_DRAWOBJ))


STUBS.internal(0x101CD2D4, stub_find)


def stub_balance(e):
    L = e.cl
    L.rec("GetBalance", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetBalance") & MASK)


STUBS.internal(0x100F32B4, stub_balance)


def stub_labs(e):
    v = s32(e.cl and e.r(3))
    e.cl.rec("labs", (v,))
    e.set_r(3, (-v if v < 0 else v) & MASK)


def glued(fn):
    """The import glue saves the TOC at 0x14(r1), where the caller restores it from after the call."""

    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)

    return wrapped


STUBS.internal(0x10239928, glued(stub_labs))


def stub_format(e):
    L = e.cl
    tag = e.r(7)
    L.rec("FormatString", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK, e.r(6) & MASK, e.u32(tag), e.u32(tag + 8), s32(e.u32(tag + 12))))
    e.set_r(3, 0)


STUBS.internal(0x10239730, glued(stub_format))


def stub_new(e):
    size = e.r(3) & MASK
    e.cl.rec("new", (size,))
    e.set_r(3, e.alloc(size))


STUBS.internal(0x102384B8, glued(stub_new))
for _name, _addr in (("TitleCtor", 0x1018FBE8), ("SpriteCtor", 0x1018D11C), ("CashCtor", 0x1018DE60)):
    STUBS.internal(_addr, CL.recorder(_name, "N"))

_gradient = CL.recorder("GradientCtor", "N")


def stub_gradient(e):
    mesh = e.r(4)
    _gradient(e)
    e.cl.rec("GradientMesh", (s32(mesh - ADDR["gHireListStrings"]),))


STUBS.internal(0x101898C8, stub_gradient)


def stub_history(e):
    L = e.cl
    index = (e.r(3) - obj(L_ANALYSER) - 0x1F100) // 0x250
    L.rec("GetValue", (index, L.lab(e.r(4)), s32(e.r(5))))
    v = L.ret("GetValue:%d" % index) & MASK
    e.write(e.r(4), struct.pack(">I", v))
    e.set_r(3, 1)


STUBS.internal(0x1001C2A8, stub_history)


def stub_ptr_glue(e):
    L = e.cl
    d = e.r(12)
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))   # the glue saves the TOC where the caller restores it from
    if d == DESC_SET_VALUE:
        L.rec("WindowSetValue", (L.lab(e.r(3)), s32(e.r(4))))
    elif d == DESC_SET_TEXT:
        L.rec("WindowSetText", (L.lab(e.r(3)), L.lab(e.r(4))))
    else:
        raise AssertionError("unknown virtual %x" % d)


STUBS.internal(0x1023BAA8, stub_ptr_glue)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def fixed(self):
        out = {label: self.g(name) for name, label in GLOBAL_LABELS.items()}
        out.update({label: addr for label, addr in self.objs.items()})
        return out

    def label_of(self, p):
        if p == 0:
            return 0
        for label, addr in self.objs.items():
            if addr == p:
                return label
        return -1


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.objs = make_objects(self, case)
        CUR.clear()
        CUR.update(self.objs)
        self.L = e.cl = CL.PpcLog({addr: label for label, addr in self.fixed().items()}, case["queues"])
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

    def set_ptr(self, name, v):
        self.words(self.g(name), [v])

    def get_ptr(self, name):
        return self.rd_word(self.g(name))

    def call(self, type_):
        try:
            T.call_ppc(self.e, FN, [type_ & MASK])
        except RuntimeError:
            pc = 0x101A2054
            print("insns", [hex(struct.unpack(">I", self.e.read(pc + 4 * k, 4))[0]) for k in range(-12, 4)])
            print("regs", [hex(self.e.r(k)) for k in range(0, 32)])
            raise


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_hl_clearobj()
        self.objs = make_objects(self, case)
        for label, addr in self.objs.items():
            lib.tpw_hl_setobj(label, addr)
        for name, label in GLOBAL_LABELS.items():
            lib.tpw_hl_setobj(label, self.g(name))
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_hl_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_hl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_ptr(self, name, v):
        C.c_uint64.from_address(self.g(name)).value = v

    def get_ptr(self, name):
        return C.c_uint64.from_address(self.g(name)).value

    def call(self, type_):
        self.lib.tpw_hl_create(s32(type_))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_hl_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "setobj").argtypes = [C.c_uint32, C.c_uint64]
    getattr(lib, p + "create").argtypes = [C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def make_objects(s, case):
    """The blocks the fakes hand out, by label (windows carry the virtual table at +0x118)."""
    table = s.alloc(0x40)
    s.words(table + 0x24, [DESC_SET_VALUE])
    s.words(table + 0x2C, [DESC_SET_TEXT])
    objs = {}

    def make(label, size=0x140, window=False):
        a = s.alloc(size)
        if window:
            s.words(a + 0x118, [table])
        objs[label] = a

    for wid in case["windows"]:
        make(0x10000 + wid, window=True)
    for sid in STRINGS:
        make(0x20000 + sid, 0x40)
    for f in FONTS:
        make(0x30000 + f, 0x40)
    make(L_GAME)
    make(L_BUILT)
    if case["existing"]:
        make(L_EXISTING)
    make(L_BANK, 0x40)
    make(L_ANALYSER, 0x40000)
    if case["dobj"]:
        make(L_DRAWOBJ, 0x40)
    make(L_TABLE, 0x40)
    make(L_WORLD, 0x40)
    return objs


def build(s, case):
    s.set_ptr("gGameWindow", s.objs[L_GAME])
    s.set_ptr("gGameWorld", s.objs[L_WORLD])
    s.set_ptr("gTextTable", s.objs[L_TABLE])
    s.set_ptr("gHirePanelWindow", s.objs.get(L_EXISTING, 0))
    s.words(s.g("gHirePanelType"), [case["gtype"]])
    s.words(s.g("gHirePanelScript"), [0x00000001])
    s.words(s.g("gHireColumnColour"), [case["colour"]])
    s.words(s.g("gHireSortColumn"), [case["sort"]])
    s.words(s.g("gHireListWord10"), [case["init"][0]])
    s.words(s.g("gHireListWord14"), [case["init"][1]])
    s.words(s.g("gHireListWithdrawals"), [case["init"][2]])
    s.words(s.g("gHireListDeposits"), [case["init"][3]])


def observe(s, case):
    return {
        "log": s.log(),
        "type": s32(s.rd_word(s.g("gHirePanelType"))),
        "window": s.label_of(s.get_ptr("gHirePanelWindow")),
        "words": [s32(s.rd_word(s.g(n))) for n in ("gHireListWord10", "gHireListWord14", "gHireListWithdrawals", "gHireListDeposits",
                                                  "gHireColumnColour", "gHireSortColumn")],
    }


def run(s, case):
    try:
        build(s, case)
        s.call(case["type"])
        o = observe(s, case)
        return o
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

TYPES = [-1, 0, 1, 2, 3, 4, 5, -2, 6, 100, -100, 0x7FFFFFFF, -0x80000000]
BALANCES = [0, 1, -1, 12345, -12345, 0x7FFFFFFF, -0x80000000, -0x7FFFFFFF]


def windows_for(present):
    w = set(present) | MANDATORY
    if 0x2481 in w:
        w.add(0x2482)
    if 0x2485 in w:
        w.update((0x2489, 0x248C, 0x248B, 0x2487))
    return sorted(w)


def make_case(r, type_=None, gtype=None, existing=None, built=None, absent=(), balance=None):
    c = {}
    c["type"] = r.choice(TYPES) if type_ is None else type_
    if gtype is None:
        gtype = c["type"] if r.random() < 0.3 else r.choice(TYPES)
    c["gtype"] = gtype
    c["existing"] = r.random() < 0.6 if existing is None else existing
    c["built"] = r.random() < 0.8 if built is None else built
    c["dobj"] = r.random() < 0.8
    present = [w for w in WINDOWS if w not in absent and (absent or r.random() < 0.85)]
    c["windows"] = windows_for(present)
    c["balance"] = r.choice(BALANCES + [r.randrange(-100000, 100000), r.randrange(-(1 << 31), 1 << 31)]) if balance is None else balance
    c["hist"] = [r.choice([0, 5, -5, r.randrange(-(1 << 31), 1 << 31), r.randrange(-1000, 1000)]) for _ in range(3)]
    c["sort"] = r.choice([0, 1, 2, -1, r.randrange(-100, 100)])
    c["colour"] = r.choice([8, 0, -1, r.randrange(-(1 << 31), 1 << 31)])
    c["init"] = [r.randrange(-(1 << 31), 1 << 31) for _ in range(4)]
    c["queues"] = {"GetBalance": [c["balance"]], "GetValue:5": [c["hist"][0]], "GetValue:3": [c["hist"][1]], "GetValue:2": [c["hist"][2]]}
    if not c["built"]:
        # Build answers NULL: the label is simply not in the table
        c["windows"] = c["windows"]
    return c


def fix_built(c):
    return c


def edges():
    r = random.Random(51019)
    for t in (-1, 0, 1, 2, 3, 4, 5, -2):
        for g in (-1, 0, 1, 2, 3, 4, 5, 9):
            for existing in (False, True):
                yield make_case(r, t, g, existing, True, absent=[w for w in WINDOWS if w in ()] or (), balance=r.choice(BALANCES))
    for existing in (False, True):
        for t in (-1, 3):
            yield make_case(r, t, 3, existing, False)
    for w in WINDOWS:
        for t in (0, 3):
            yield make_case(r, t, 1, False, True, absent=[w])
    for b in BALANCES:
        yield make_case(r, 2, 4, False, True, balance=b)
    for t in TYPES:
        yield make_case(r, t, 3, True, True)


def rnd(r):
    return make_case(r)


TESTS = [
    FnTest("CreateHireListPanel", ppc, host, edge=edges, rnd=rnd),
]
