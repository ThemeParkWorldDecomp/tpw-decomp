"""Unit v5969, the load and save panels' openers and list fills, against libtpw_unit_CreateLoadPanel_2.dylib
(tests/unit_CreateLoadPanel_2.cpp), main app ("SimTheme Park"), src/ui_allKidsPanel_AllKidsPanelMsg_LoansPanelMsg_CreateLoadPanel_2.cpp:
  CreateLoadPanel 0x101a37ac, CreateSavePanel 0x101a3994, FillSaveList 0x101a35a8, FillLoadList 0x101a33c0, LCPostFormat 0x101a3258.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (the pause, the
dialog, the windows' methods, the list control, the draw objects, the save file list, the time conversions, the table's FormatString,
swprintf, operator new and the constructors) is a fake on both sides that records its arguments and answers from the case.

The world of a case: gGame's word 0x3c, the panels' two words, the convert buffer, the time format, the string table pointer in gText, the
save file entries (name, two stamp words), and the queues (Dialog and list answers, the calendar fields per entry, the formatted text).
What is compared: the event log, gGame's word, the two panel words (as labels of the fake windows) and the convert buffer's start.

No fused operations. Not tested: nothing is left out; GetWindow(-1) and GetWindow(0x23bce2) always answer a window (the original does not
test them), the list answer may be NULL.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateLoadPanel_2"
MASK = 0xFFFFFFFF

F_LOAD, F_SAVE, F_FILLSAVE, F_FILLLOAD, F_POST = 0x101A37AC, 0x101A3994, 0x101A35A8, 0x101A33C0, 0x101A3258
SIZES = {F_LOAD: 352, F_SAVE: 552, F_FILLSAVE: 456, F_FILLLOAD: 428, F_POST: 308}
OWN = set(SIZES)

ADDR = dict(game=0x1037F134, text=0x1037FF18, buf=0x10380C48, loadsave=0x10396A7C, panel_open=0x10394974, save_open=0x10394970,
            savename=0x1039492C, fmt=0x1024E0E4, script=0x102AA330, hilight=0x1024E0DB)
HOST_LAYOUT = [0x3C]

# fake objects: 0 panel, 1 window -1, 2 name box, 3 list, 4 draw object, 5 font, 6/7/8 strings 0xca/0xc9/0xce, 9 child, 10 table
NOBJ = 11
OBJ_PPC = []

FORMAT = [ord(c) for c in "%ls %02d:%02d"] + [0] * 5 + [ord(c) for c in "\\/:*?\"<>|"] + [0]
FORMAT += [0] * (0x40 - len(FORMAT))

CUR = [None]


def s32(v):
    return T.s32(v)


def hash16(units):
    h = 0
    for c in units:
        h = (h * 31 + c) & MASK
    return h


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
_orig_internal = STUBS.internal


def _checked(addr, fn):
    def wrapped(e):
        before = e.r(2)
        fn(e)
        print("DEBUG2 r2", hex(addr), hex(before), hex(e.r(2)))

    _orig_internal(addr, wrapped)




def u16str(e, a):
    """The UTF-16 units of the string at a (big-endian), without the terminator; None for NULL."""
    out = []
    while True:
        c = struct.unpack(">H", e.read(a, 2))[0]
        if c == 0 or len(out) > 200:
            return out
        out.append(c)
        a += 2


def hash_at(e, a):
    return -1 if a == 0 else hash16(u16str(e, a))


def obj_ret(index):
    return lambda e, log: OBJ_PPC[index]


def stub(addr, name, spec, ret=None):
    inner = CL.recorder(name, spec, ret)

    def fn(e):
        before = e.r(2)
        inner(e)

    STUBS.internal(addr, fn)


stub(0x10149904, "Pause", "Lib")
stub(0x10180514, "CheckPanel", "")
stub(0x101DCCEC, "CaptureKb", "Li")
stub(0x10180794, "SetTitleText", "LiL")
stub(0x101DC1EC, "Show", "Lh")
stub(0x101D2614, "SetColumnType", "Lhi")
stub(0x101D1EB4, "SetSelectionObject", "LL")
stub(0x101DB624, "SetClientDrawObject", "LL")
stub(0x101DA118, "SetFont", "LL")
stub(0x101DA3AC, "SetFontColour", "Liiii")
stub(0x101DA204, "SetFontJust", "Lii")
stub(0x101DBAC4, "GetChild", "Li", obj_ret(9))
stub(0x101CDAC4, "SetCursorDrawObject", "LL")
stub(0x10182364, "DisableAllKeyTables", "")
stub(0x1018D11C, "SpriteCtor", "N")
stub(0x1018F1D0, "CursorPosCtor", "N")
stub(0x1023BAA8, "WindowSetText", "LL")            # the virtual at slot 0x2c, through __ptr_glue
stub(0x10238740, "WStrCtor", "")
stub(0x10238710, "WStrDtor", "xh")
stub(0x10180270, "GetFontTableEntry", "u", obj_ret(5))


def st_find(e):
    L = e.cl
    L.rec("FindDrawObject", (1 if e.r(3) == ADDR["hilight"] else 0,))
    e.set_r(3, OBJ_PPC[4])


def st_columns(e):
    L = e.cl
    L.rec("SetColumnFormatsAll", (L.lab(e.r(3)), 1 if e.r(4) == 0x10267BD8 else 0, 1 if e.r(5) == 0x10267BD0 else 0))


def st_dialog(e):
    L = e.cl
    handler = {0x10267BF0: 1, 0x10267BE0: 2}.get(e.r(4), 0)
    L.rec("Dialog", (1 if e.r(3) == ADDR["script"] else 0, handler, s32(e.r(5))))
    e.set_r(3, OBJ_PPC[0] if L.ret("Dialog") else 0)


def st_getstring(e):
    L = e.cl
    i = s32(e.r(3))
    L.rec("GetString", (i,))
    e.set_r(3, OBJ_PPC[6 if i == 0xCA else 7 if i == 0xC9 else 8])


def st_getwindow(e):
    L = e.cl
    i = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), i))
    r = 0
    if i == -1:
        r = OBJ_PPC[1]
    elif i == 0x23BCE2:
        r = OBJ_PPC[2]
    elif i == 0x23BCE0:
        r = OBJ_PPC[3] if L.ret("list") else 0
    e.set_r(3, r)


def st_statictext(e):
    L = e.cl
    L.rec("SetStaticTextArea", (L.lab(e.r(3)), 1 if e.r(4) == ADDR["savename"] else 0, s32(e.r(5))))


def st_unaccepted(e):
    L = e.cl
    L.rec("SetUnAcceptedChars", (L.lab(e.r(3)), (e.r(4) - ADDR["fmt"]) // 2))


def st_new(e):
    size = e.r(3)
    e.cl.rec("new", (size,))
    e.write(e.r(1) + 20, struct.pack(">I", e.r(2)))   # the import glue saves the caller's TOC pointer, which the caller reloads
    e.set_r(3, e.alloc(size))


def st_additem_real(e):
    L = e.cl
    this, item = e.r(3), e.r(4)
    c0, c1 = e.u32(item), e.u32(item + 4)
    L.rec("AddItem", (L.lab(this), hash_at(e, c0), 1 if c1 == ADDR["buf"] else 0, hash_at(e, c1), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))


def st_first(e):
    L = e.cl
    L.rec("GetFirst", (L.lab(e.r(3)),))
    ents = CUR[0].entries
    e.set_r(3, ents[0] if ents else 0)


def st_next(e):
    L = e.cl
    ents = CUR[0].entries
    i = ents.index(e.r(4)) if e.r(4) in ents else -1
    L.rec("GetNext", (L.lab(e.r(3)), i))
    e.set_r(3, ents[i + 1] if 0 <= i + 1 < len(ents) else 0)


def st_filetime(e):
    L = e.cl
    a = e.r(3)
    w0, w1 = e.u32(a), e.u32(a + 4)
    L.rec("FileTimeToLocal", (w0, w1))
    e.write(e.r(4), struct.pack(">II", (w0 ^ 0x1234) & MASK, (w1 + 7) & MASK))


def st_stampset(e):
    L = e.cl
    a = e.r(4)
    lo, hi = e.u32(a), e.u32(a + 4)
    L.rec("StampSet", (lo, hi))
    e.write(e.r(3), struct.pack(">II", hi, lo))


def st_gettime(e):
    L = e.cl
    t = e.r(4)
    hi, lo = e.u32(t), e.u32(t + 4)
    sp = e.r(1)
    L.rec("GetTime", (hi, lo, 1 if e.r(8) == 0 else 0, 1 if e.u32(sp + 0x38) == 0 else 0, 1 if e.u32(sp + 0x3C) == 0 else 0,
                      1 if e.r(5) else 0, 1 if e.r(6) else 0, 1 if e.r(7) else 0, 1 if e.r(9) else 0, 1 if e.r(10) else 0))
    for reg, key in ((5, "year"), (6, "month"), (7, "day"), (9, "hour"), (10, "minute")):
        if e.r(reg):
            e.write(e.r(reg), struct.pack(">I", L.ret(key) & MASK))


def st_format(e):
    L = e.cl
    tags = []
    for reg in (7, 8, 9):
        a = e.r(reg)
        tags += [e.u32(a), e.u32(a + 8), s32(e.u32(a + 0xC))]
    L.rec("FormatString", (L.lab(e.r(3)), e.r(5), e.r(6)) + tuple(tags))
    e.write(e.r(4), struct.pack(">HH", 0x61 + (L.ret("fmtc") & 15), 0))


def st_swprintf(e):
    L = e.cl
    hour, minute = s32(e.r(7)), s32(e.r(8))
    L.rec("swprintf", (1 if e.r(3) == ADDR["buf"] else 0, e.r(4), hash_at(e, e.r(5)), hash_at(e, e.r(6)), hour, minute))
    e.write(e.r(3), struct.pack(">HHH", 0x41 + (hour & 15), 0x30 + (minute & 7), 0))


STUBS.internal(0x101D2864, st_columns)
STUBS.internal(0x101CD2D4, st_find)
STUBS.internal(0x101F12C8, st_dialog)
STUBS.internal(0x10180468, st_getstring)
STUBS.internal(0x101DBE40, st_getwindow)
STUBS.internal(0x101CD7E0, st_statictext)
STUBS.internal(0x101CE94C, st_unaccepted)
STUBS.internal(0x102384B8, st_new)
STUBS.internal(0x101D3000, st_additem_real)
STUBS.internal(0x101FC97C, st_first)
STUBS.internal(0x101FCD08, st_next)
STUBS.internal(0x10239FE8, st_filetime)
STUBS.internal(0x1023A000, st_stampset)
STUBS.internal(0x10239970, st_gettime)
STUBS.internal(0x10239730, st_format)
STUBS.internal(0x10239160, st_swprintf)


def setup(runner):
    T.install_std_stubs(runner.emu)
    OBJ_PPC[:] = [T.persist_alloc(runner.emu, 0x200) for _ in range(NOBJ)]
    for o in OBJ_PPC:   # the window's table at +0x118 (its slot 0x2c is only loaded, then the faked glue answers)
        runner.emu.write(o + 0x118, struct.pack(">I", o))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def units(self, a, us):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", u & 0xFFFF) for u in us))

    def rd_units(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def label_of(self, value):
        if value == 0:
            return 0
        # the words are 32 bits: the host's pointers are compared by their low halves
        low = [o & 0xFFFFFFFF for o in self.objs]
        return low.index(value) + 1 if value in low else -1


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.objs = list(OBJ_PPC)
        fixed = {OBJ_PPC[i]: i + 1 for i in range(NOBJ)}
        fixed[ADDR["game"]] = 20
        fixed[ADDR["loadsave"]] = 21
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.entries = []
        CUR[0] = self
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "game_f3c":
            return ADDR["game"] + 0x3C
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_table(self):
        self.words(ADDR["text"] + 0x564, [OBJ_PPC[10]])

    def set_entry(self, i, w0, w1, name):
        a = self.alloc(0x40)
        nb = self.alloc(2 * 34)
        self.units(nb, name + [0])
        self.words(a + 8, [nb])
        self.words(a + 0x14, [w0, w1])
        self.entries.append(a)

    def obj(self, i):
        return OBJ_PPC[i]

    def call(self, op, arg):
        entry = {"load": F_LOAD, "save": F_SAVE, "fillload": F_FILLLOAD, "fillsave": F_FILLSAVE, "post": F_POST}[op]
        try:
            T.call_ppc(self.e, entry, [arg & MASK])
        except RuntimeError:
            print("DEBUG log", [x[0] for x in self.L.log][-6:])
            print("DEBUG words", [hex(self.e.u32(0x101A3A90 + 4 * k)) for k in range(0, 8)], "regs", [hex(self.e.r(k)) for k in range(0, 14)])
            raise


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.objs = [lib.tpw_cl_obj(i) for i in range(NOBJ)]
        fixed = {i + 1: self.objs[i] for i in range(NOBJ)}
        fixed[20] = self.g("game")
        fixed[21] = self.g("loadsave")
        CL.host_begin(lib, fixed, case["queues"])
        self.n = 0
        lib.tpw_cl_count(0)

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cl_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_table(self):
        self.lib.tpw_cl_set_table(self.objs[10])

    def set_entry(self, i, w0, w1, name):
        nb = self.lib.tpw_cl_entry(i, w0 & MASK, w1 & MASK)
        self.units(nb, name + [0])
        self.n = i + 1
        self.lib.tpw_cl_count(self.n)

    def obj(self, i):
        return self.objs[i]

    def call(self, op, arg):
        lib = self.lib
        if op == "load":
            lib.tpw_cl_load(s32(arg))
        elif op == "save":
            lib.tpw_cl_save(s32(arg))
        elif op == "fillload":
            lib.tpw_cl_fillload(arg)
        elif op == "fillsave":
            lib.tpw_cl_fillsave(arg)
        else:
            lib.tpw_cl_post(arg)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cl_"
    for name, res, args in (("addr", C.c_uint64, [C.c_char_p]), ("alloc", C.c_uint64, [C.c_uint32]), ("obj", C.c_uint64, [C.c_int32]),
                            ("entry", C.c_uint64, [C.c_int32, C.c_uint32, C.c_uint32]), ("count", None, [C.c_int32]),
                            ("set_table", None, [C.c_uint64]), ("load", None, [C.c_int32]), ("save", None, [C.c_int32]),
                            ("fillload", None, [C.c_uint64]), ("fillsave", None, [C.c_uint64]), ("post", None, [C.c_uint64])):
        f = getattr(lib, p + name)
        f.restype, f.argtypes = res, args
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert n == len(HOST_LAYOUT), list(out)[:n]   # the host's CGame has 8-byte pointers: mF3C is reached through "game_f3c"
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.words(s.g("game_f3c"), [case["f3c"]])
    s.words(s.g("panel_open"), [0x1234])
    s.words(s.g("save_open"), [0x5678])
    s.units(s.g("buf"), case["buf"])
    s.units(s.g("fmt"), FORMAT)
    s.units(s.g("savename"), [0x41, 0x42, 0])
    s.set_table()
    for i, (w0, w1, name) in enumerate(case["entries"]):
        s.set_entry(i, w0, w1, name)


def observe(s, case):
    return {"log": s.log(), "f3c": s.rd_word(s.g("game_f3c")), "panel": s.label_of(s.rd_word(s.g("panel_open"))),
            "save": s.label_of(s.rd_word(s.g("save_open"))), "buf": s.rd_units(s.g("buf"), 8)}


def run(s, case):
    try:
        build(s, case)
        arg = {"load": case["mode"], "save": case["mode"], "fillload": s.obj(3), "fillsave": s.obj(3), "post": s.obj(2)}[case["op"]]
        s.call(case["op"], arg)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def make(r, op, dialog=None, lst=None, count=None):
    count = r.choice([0, 1, 2, 3, 3]) if count is None else count
    entries = []
    for _ in range(count):
        w0 = r.choice([0, MASK, r.getrandbits(32)])
        w1 = r.choice([0, 1, r.getrandbits(32)])
        name = [r.choice([0x41, 0x5A, 0x61, 0x7A, 0x30, 0x3A]) for _ in range(r.randrange(0, 6))]
        entries.append((w0, w1, name))
    n = max(count, 1)

    def vals(lo, hi):
        return [r.choice([lo, hi, r.randrange(lo, hi + 1)]) & MASK for _ in range(n)]

    queues = {"Dialog": [r.choice([0, 1, 1]) if dialog is None else dialog], "list": [r.choice([0, 1, 1]) if lst is None else lst],
              "year": vals(1990, 2040), "month": vals(1, 12), "day": vals(1, 31), "hour": vals(0, 23), "minute": vals(0, 59),
              "fmtc": [r.getrandbits(8) for _ in range(n)]}
    return {"op": op, "mode": r.choice([0, 1, -1, 7]), "f3c": r.choice([0, 1, r.getrandbits(32)]), "queues": queues,
            "entries": entries, "buf": [r.getrandbits(16) for _ in range(8)]}


def edges(op, seed):
    def gen():
        r = random.Random(seed)
        if op in ("load", "save"):
            for dialog in (0, 1):
                for lst in (0, 1):
                    for count in (0, 1, 3):
                        yield make(r, op, dialog, lst, count)
        elif op in ("fillload", "fillsave"):
            for count in (0, 1, 2, 3, 4):
                yield make(r, op, None, None, count)
        else:
            yield make(r, op)
    return gen


def rnd(op):
    return lambda r: make(r, op)


TESTS = [
    FnTest("CreateLoadPanel", ppc, host, edge=edges("load", 5969), rnd=rnd("load")),
    FnTest("CreateSavePanel", ppc, host, edge=edges("save", 5970), rnd=rnd("save")),
    FnTest("FillLoadList", ppc, host, edge=edges("fillload", 5971), rnd=rnd("fillload")),
    FnTest("FillSaveList", ppc, host, edge=edges("fillsave", 5972), rnd=rnd("fillsave")),
    FnTest("LCPostFormat", ppc, host, edge=edges("post", 5973), rnd=rnd("post")),
]
