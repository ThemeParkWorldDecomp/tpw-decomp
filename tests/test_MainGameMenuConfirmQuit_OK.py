"""Unit v5958, the main menu's quit answer, item callback and fade, and two functions of CFSLoadSave, against
libtpw_unit_MainGameMenuConfirmQuit_OK.dylib (tests/unit_MainGameMenuConfirmQuit_OK.cpp), main app ("SimTheme Park"),
src/game/save/FS_LoadSave.cpp, src/game/ui/ui_mainmenu.cpp:
  MainGameMenuConfirmQuit_OK 0x101fab10, MainMenuItemCB 0x101fa9dc, itemOnFrame 0x101fa8d4,
  CFSLoadSave::SaveInternal 0x101fc790, CFSLoadSave::GetFirstSaveFileDetails 0x101fc97c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, 64-bit pointers) for the translation; the same code reads both back.
The callees outside the unit are fakes on both sides that record their arguments (objects as labels) and answer from the case: the
online system, the user info (constructor, global master save, the users path), the game state, the menu, the window (colour, extra
parameters), the default item callback, CSaveLoad::Save, the string operations of the runtime (constructors, copy, assignment, append,
Length, Left, destructors), the two string conversions, the file storage (constructor, Open, FindFirst, FindNext, destructor) and
FS_IsSubDirectory. The list functions (head, tail, count, nodes) run for real on both sides.

The strings carry a 32-bit token in their text pointer: a constructor from text hashes the text, an append mixes the two tokens in order,
an assignment copies one, Left mixes the count, the conversions map the token; so a wrong string, a wrong order or a wrong field shows
in the next call that reads the pointer (the path given to Save, the name of a new save file details).

The world of a case: gOnlineSystem, gUserInfo and gMainMenu (present or not), the three TPGlobals strings (all files, save extension,
wide separator), the window, the name and extension strings, the list with 0 to 4 details already in it, and the queues (the users path,
the file storage's answers, the entries it fills in, the lengths, which entries are folders, the extra parameter, the answers of Save
and of the default callback). What is compared: the answer, the three global pointers (as labels), the TPGlobals tokens, the whole list
(count, head and tail, both chains, each details' label, name token and stamp words) and the event log.

No fused operations. Not tested: the details already in the list (Find never finds a new object, so the branch that skips AddTail
cannot be reached); the sizes handed to operator new and delete and the free of the removed details (the platform's operator new is
not a hook of the unit library; the destruction of each removed details' name and every constructor are logged).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_MainGameMenuConfirmQuit_OK"
MASK = 0xFFFFFFFF

FN_QUIT, FN_ITEMCB, FN_FRAME, FN_SAVE, FN_FIRST = 0x101FAB10, 0x101FA9DC, 0x101FA8D4, 0x101FC790, 0x101FC97C
SIZES = {FN_QUIT: 148, FN_ITEMCB: 248, FN_FRAME: 212, FN_SAVE: 384, FN_FIRST: 848}
OWN = set(SIZES)

# the Mac's addresses of the globals (the asm's TOC comments)
GTP = 0x103B74C8
ADDR = dict(gOnlineSystem=0x10380AB0, gUserInfo=0x10380B34, gMainMenu=0x10396A58, gSaveLoad=0x1037FEFC,
            allfiles=GTP + 0x40D8, saveext=GTP + 0x440C, wsep=GTP + 0x44C0)
# the host's offsets the module relies on: list head, tail, count, node next, node previous, a string's text pointer, details stamp high, low
HOST_LAYOUT = [8, 16, 24, 8, 16, 16, 32, 36]

NAMES = ["Alpha.TPWS", "beta", "Gamma Park.TPWS", "d.TPWS", "", "Z.TPW"]


def s32(v):
    return T.s32(v)


def th(text):
    h = 0
    for ch in text:
        h = (h * 31 + ch) & MASK
    return h


def mix(h, o):
    return (h * 31 + o + 1) & MASK


def stamp0(i):
    return (0x11110000 + i * 0x01010101) & MASK


def stamp1(i):
    return (0x2000 + i * 0x3F1 + (0x80000000 if i & 1 else 0)) & MASK


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def zs(e, a):
    """The fields after the table pointer of a string, cleared (what the host fakes' constructors do)."""
    e.write(a + 4, bytes(16))


def ss(e, a, tok):
    e.write(a + 8, struct.pack(">I", tok & MASK))


def ts(e, a):
    return e.u32(a + 8)


def cstr(e, a):
    t = e.cstr(a)
    return t.encode("latin-1") if isinstance(t, str) else bytes(t)


def r_wctor(e, log):
    zs(e, e.r(3))
    return e.r(3)


def r_wcopy(e, log):
    n, o = e.r(3), e.r(4)
    zs(e, n)
    ss(e, n, ts(e, o))
    return n


def r_assign(e, log):
    ss(e, e.r(3), ts(e, e.r(4)))
    return e.r(3)


def r_append(e, log):
    ss(e, e.r(3), mix(ts(e, e.r(3)), ts(e, e.r(4))))
    return e.r(3)


def r_cctor(e, log):
    zs(e, e.r(3))
    return e.r(3)


def r_cctor_cstr(e, log):
    zs(e, e.r(3))
    ss(e, e.r(3), th(cstr(e, e.r(4))))
    return e.r(3)


def r_left(e, log):
    out = e.r(3)
    zs(e, out)
    ss(e, out, (ts(e, e.r(4)) + (e.r(5) & MASK) * 7 + 1) & MASK)
    return out


def r_userspath(e, log):
    out = e.r(3)
    zs(e, out)
    ss(e, out, e.cl.ret("userspath") & MASK)
    return out


def hidden_result(key, fill=None):
    """A function that answers a TbError through the hidden pointer in r3 (and fills the entry at the register `fill`)."""
    def fn(e, log):
        v = s32(e.cl.ret(key))
        e.write(e.r(3), struct.pack(">i", v))
        if fill is not None and v >= 0:
            k = s32(e.cl.ret("entry"))
            i = ((k % 6) + 6) % 6
            info = e.r(fill)
            e.write(info + 0x24, NAMES[i].encode() + b"\0")
            e.write(info + 8, struct.pack(">II", stamp0(i), stamp1(i)))
        return e.r(3)
    return fn


def conv(mul, add):
    def fn(e, log):
        ss(e, e.r(3), (ts(e, e.r(4)) * mul + add) & MASK)
        return 0
    return fn


def s_issub(e):
    L = e.cl
    info = e.r(3)
    name = cstr(e, e.u32(info + 0x20))
    L.rec("FS_IsSubDirectory", (th(name), e.u32(info + 8), e.u32(info + 12)))
    e.set_r(3, L.ret("FS_IsSubDirectory") & MASK)


def glued(fn):
    """A glue function saves the caller's TOC pointer at 0x14(r1) (the caller reloads it after the call): do that, then the fake."""
    def wrapper(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapper


R = CL.recorder
for addr, fn in [
    (0x10238740, R("wctor", "N", ret=r_wctor)),
    (0x102387A0, R("wcopyctor", "NL", ret=r_wcopy)),
    (0x10238710, R("wdtor", "L")),
    (0x10239850, R("wassign", "LL", ret=r_assign)),
    (0x10238788, R("wappend", "LL", ret=r_append)),
    (0x10238458, R("cctor", "N", ret=r_cctor)),
    (0x102382D8, R("cctor_cstr", "Ns", ret=r_cctor_cstr)),
    (0x102382F0, R("cdtor", "L")),
    (0x10238470, R("cassign_dyn", "LL", ret=r_assign)),
    (0x102387D0, R("cassign_base", "LL", ret=r_assign)),
    (0x10238380, R("cappend", "LL", ret=r_append)),
    (0x10238320, R("Length", "L", ret="i")),
    (0x10238F50, R("Left", "xLu", ret=r_left)),
    (0x10005670, R("UniStringToString", "LL", ret=conv(3, 7))),
    (0x100057A0, R("StringToUniString", "LL", ret=conv(5, 3))),
    (0x1017DE54, R("CFSUserInfo", "N")),
    (0x1017FA50, R("SaveGlobalMasterSave", "L")),
    (0x1017F858, R("ReturnUsersPathWithTheme", "NL", ret=r_userspath)),
    (0x1016F7D0, R("CFSOnlineSystem", "N")),
    (0x10171404, R("StopProcessing", "L")),
    (0x10236E6C, R("GS_Set", "i")),
    (0x101FC140, R("HideMainMenu", "")),
    (0x101DA3AC, R("SetFontColour", "Liiii")),
    (0x101DCFD0, R("setExtraParam", "Lhu")),
    (0x101DD030, R("getExtraParam", "Lh", ret="i")),
    (0x1018AA48, R("DefaultCB", "Liii", ret="i")),
    (0x10157868, R("Save", "LuLi", ret="i")),
    (0x10238440, R("CFileStorage", "N")),
    (0x10238428, R("CFileStorage_dtor", "L")),
    (0x10238488, R("Open", "xLu", ret=hidden_result("Open"))),
    (0x102384A0, R("FindFirst", "xLux", ret=hidden_result("FindFirst", 6))),
    (0x102384D0, R("FindNext", "xLx", ret=hidden_result("FindNext", 5))),
]:
    if 0x10238000 <= addr < 0x1023A000:
        fn = glued(fn)
    STUBS.internal(addr, fn)
STUBS.internal(0x10004764, s_issub)


def s_new(e):
    n = e.r(3) & MASK
    e.set_r(3, e.alloc(n if n else 4))


STUBS.internal(0x102384B8, glued(s_new))   # operator new: silent (the host's operator new is not a hook)
def s_delete(e):
    # the host logs the delete of the details (known objects); the list nodes' deletes are not logged there
    lb = e.cl.lab(e.r(3))
    if lb != -1:
        e.cl.rec("delete", (lb,))


STUBS.internal(0x10238218, glued(s_delete))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides

OBJECTS = ("window", "list", "name", "ext", "gui", "gos", "menu")
LABELS = dict(window=1, list=2, name=3, ext=4, gui=5, gos=6, menu=7)
GLOBAL_LABELS = dict(gSaveLoad=8, allfiles=9, saveext=10, wsep=11)
DETAILS_LABEL = 20


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def setstr(self, a, tok):
        self.wptr(a + self.SD, tok)

    def tokstr(self, a):
        return self.rptr(a + self.SD) & MASK

    def make_objects(self):
        self.o = {n: self.alloc(0x80) for n in OBJECTS}
        self.o["d"] = [self.alloc_details() for _ in range(4)]

    def labels(self):
        """{label: address} of every object the log names."""
        f = {LABELS[n]: self.o[n] for n in OBJECTS}
        for n, lb in GLOBAL_LABELS.items():
            f[lb] = self.g(n)
        for i, d in enumerate(self.o["d"]):
            f[DETAILS_LABEL + i] = d
        return f


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"
    SD, HI, LO, LH, LT, LC, NN, NP = 8, 0x14, 0x18, 4, 8, 12, 4, 8
    CALLS = dict(quit=FN_QUIT, itemcb=FN_ITEMCB, frame=FN_FRAME, save=FN_SAVE, first=FN_FIRST)

    def __init__(self, e, case):
        self.e = e
        self.make_objects()
        self.L = e.cl = CL.PpcLog({a: lb for lb, a in self.labels().items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def alloc_details(self):
        return self.e.alloc(0x20)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def wptr(self, a, v):
        self.words(a, [v])

    def rptr(self, a):
        return self.rd_word(a)

    def lab(self, a):
        return self.L.lab(a)

    def log(self):
        return self.L.log

    def init_list(self):
        self.words(self.o["list"], [0, 0, 0, 0])

    def prefill(self, dets):
        nodes = [self.alloc(12) for _ in dets]
        for i, d in enumerate(dets):
            self.words(nodes[i], [d, nodes[i + 1] if i + 1 < len(nodes) else 0, nodes[i - 1] if i else 0])
        if nodes:
            self.words(self.o["list"], [0, nodes[0], nodes[-1], len(nodes)])

    def call(self, op, args):
        r3, _, _ = T.call_ppc(self.e, self.CALLS[op], [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    """The same on the host library."""
    fmt = "<I"
    SD = HI = LO = LH = LT = LC = NN = NP = 0

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        (self.LH, self.LT, self.LC, self.NN, self.NP, self.SD, self.HI, self.LO) = lib.host_layout
        self.make_objects()
        CL.host_begin(lib, self.labels(), case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_q_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_q_alloc(n)

    def alloc_details(self):
        return self.lib.tpw_q_new_details()   # from the program's operator new: the function deletes them

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def wptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def lab(self, a):
        if a == 0:
            return 0
        return self.lib.tpw_q_lab(a)

    def log(self):
        return CL.host_end(self.lib)

    def init_list(self):
        self.lib.tpw_q_make_list(self.o["list"])

    def prefill(self, dets):
        for d in dets:
            self.lib.tpw_q_prefill(self.o["list"], d)

    def call(self, op, args):
        fn = getattr(self.lib, "tpw_q_" + {"quit": "MainGameMenuConfirmQuit_OK", "itemcb": "MainMenuItemCB", "frame": "itemOnFrame",
                                            "save": "SaveInternal", "first": "GetFirstSaveFileDetails"}[op])
        r = fn(*args)
        return 0 if r is None else r & (2 ** 64 - 1)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    p = "tpw_q_"
    u64, i32, u32 = C.c_uint64, C.c_int32, C.c_uint32
    sigs = {"addr": (u64, [C.c_char_p]), "alloc": (u64, [u32]), "lab": (C.c_int64, [u64]), "make_list": (None, [u64]),
            "prefill": (None, [u64, u64]), "new_details": (u64, []), "MainGameMenuConfirmQuit_OK": (u32, [u64]),
            "MainMenuItemCB": (u32, [u64, i32, i32, i32]), "itemOnFrame": (None, [u64]),
            "SaveInternal": (u32, [u64, u64, u64]), "GetFirstSaveFileDetails": (u64, [u64])}
    for name, (res, args) in sigs.items():
        f = getattr(lib, p + name)
        f.restype, f.argtypes = res, args
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.host_layout = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o = s.o
    s.wptr(s.g("gOnlineSystem"), o["gos"] if case["gos"] else 0)
    s.wptr(s.g("gUserInfo"), o["gui"] if case["gui"] else 0)
    s.wptr(s.g("gMainMenu"), o["menu"] if case["menu"] else 0)
    op = case["op"]
    if op in ("save", "first"):
        s.setstr(s.g("allfiles"), case["allfiles"])
        s.setstr(s.g("saveext"), case["saveext"])
        s.setstr(s.g("wsep"), case["sep"])
    if op == "save":
        s.setstr(o["name"], case["name"])
        s.setstr(o["ext"], case["ext"])
    if op in ("save", "first"):
        s.init_list()
    if op == "first":
        dets = []
        for i, (tok, hi, lo) in enumerate(case["pre"]):
            d = o["d"][i]
            s.setstr(d, tok)
            s.words(d + s.HI, [hi])
            s.words(d + s.LO, [lo])
            dets.append(d)
        s.prefill(dets)


def execute(s, case):
    op, o = case["op"], s.o
    if op == "quit":
        return s.call("quit", [0])
    if op == "itemcb":
        return s.call("itemcb", [o["window"]] + case["args"])
    if op == "frame":
        s.call("frame", [o["window"]])
        return 0   # a void function: r3 is whatever it was
    if op == "save":
        return s.call("save", [o["list"], o["name"], o["ext"]])
    r = s.call("first", [o["list"]])
    return s.lab(r)


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides; pointers as labels."""
    g = s.g
    d = {"answer": answer,
         "gos": s.lab(s.rptr(g("gOnlineSystem"))), "gui": s.lab(s.rptr(g("gUserInfo"))), "menu": s.lab(s.rptr(g("gMainMenu")))}
    if case["op"] in ("save", "first"):
        d["tokens"] = [s.tokstr(g("allfiles")), s.tokstr(g("saveext")), s.tokstr(g("wsep"))]
        L = s.o["list"]
        head, tail, count = s.rptr(L + s.LH), s.rptr(L + s.LT), s.rd_word(L + s.LC)

        def state(n):
            det = s.rptr(n)
            return (s.lab(det), s.tokstr(det + 0), s.rd_word(det + s.HI), s.rd_word(det + s.LO))

        fwd, n = [], head
        while n and len(fwd) < 20:
            fwd.append(state(n))
            n = s.rptr(n + s.NN)
        back, n = [], tail
        while n and len(back) < 20:
            back.append(state(n))
            n = s.rptr(n + s.NP)
        d["list"] = {"count": count, "head": s.lab(s.rptr(head)) if head else 0, "tail": s.lab(s.rptr(tail)) if tail else 0,
                     "fwd": fwd, "back": back}
    if case["op"] == "save":
        d["strings"] = [s.tokstr(s.o["name"]), s.tokstr(s.o["ext"])]
    d["log"] = s.log()
    return d


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

FADES = [0, 1, 0x1F, 0x20, 0xBF, 0xDF, 0xE0, 0xFF, 0x100, 0x7FFFFFC0, 0x7FFFFFDF, 0x7FFFFFE0, 0x7FFFFFFF,
         -1, -0x1F, -0x20, -0xDE, -0xDF, -0xE0, -0xE1, -0xFF, -0x100, -0x7FFFFFFF, -0x80000000]
MESSAGES = [0x1E, 0x11, 0x10, 0x12, 0x1D, 0x1F, 0, -1, 0x10000, 0x10001, 0x10002, 0x10003, 0xFFFF, 0x20000, 0x7FFFFFFF, -0x80000000,
            0x10002 - 0x10000, 0x1E + 0x10000]


def base(r, op):
    return {"op": op, "queues": {}, "gos": r.random() < 0.5, "gui": r.random() < 0.5, "menu": r.random() < 0.5}


def rtok(r):
    return r.getrandbits(32)


def rfade(r):
    k = r.random()
    if k < 0.5:
        return r.choice(FADES)
    return r.choice([-1, 1]) * r.randrange(0, 0x140)


# ---- MainGameMenuConfirmQuit_OK
def quit_case(r, gos=None, menu=None):
    c = base(r, "quit")
    if gos is not None:
        c["gos"] = gos
    if menu is not None:
        c["menu"] = menu
    return c


def quit_edges():
    r = random.Random(5958)
    for gos in (False, True):
        for menu in (False, True):
            yield quit_case(r, gos, menu)


def quit_random(r):
    return quit_case(r)


# ---- itemOnFrame
def frame_case(r, fade=None):
    c = base(r, "frame")
    c["queues"] = {"getExtraParam": [rfade(r) if fade is None else fade]}
    return c


def frame_edges():
    r = random.Random(5959)
    for f in FADES:
        yield frame_case(r, f)


def frame_random(r):
    return frame_case(r)


# ---- MainMenuItemCB
def itemcb_case(r, msg=None, fade=None):
    c = base(r, "itemcb")
    m = r.choice(MESSAGES + [r.randrange(-0x20000, 0x30000)]) if msg is None else msg
    c["args"] = [m, r.choice([0, 1, -1, r.randrange(-1000, 1000)]), r.choice([0, 1, -1, r.randrange(-1000, 1000)])]
    c["queues"] = {"getExtraParam": [rfade(r) if fade is None else fade], "DefaultCB": [r.choice([0, 1, -1, r.randrange(-5000, 5000)])]}
    return c


def itemcb_edges():
    r = random.Random(5960)
    for m in MESSAGES:
        yield itemcb_case(r, m)
    for f in FADES:
        yield itemcb_case(r, 0x1E, f)


def itemcb_random(r):
    return itemcb_case(r)


# ---- CFSLoadSave::SaveInternal
def save_case(r, gui=None):
    c = base(r, "save")
    if gui is not None:
        c["gui"] = gui
    c.update(name=rtok(r), ext=rtok(r), sep=rtok(r), allfiles=rtok(r), saveext=rtok(r))
    c["queues"] = {"userspath": [rtok(r)], "Save": [r.choice([0, 1, -1, r.randrange(-5000, 5000)])]}
    return c


def save_edges():
    r = random.Random(5961)
    for gui in (False, True):
        yield save_case(r, gui)
    c = save_case(r, True)
    c["name"] = c["ext"] = c["sep"] = 0
    c["queues"]["userspath"] = [0]
    yield c


def save_random(r):
    return save_case(r)


# ---- CFSLoadSave::GetFirstSaveFileDetails
def first_case(r, pre=None, opened=None, found=None, nxt=None, sub=None, lengths=None, gui=None):
    c = base(r, "first")
    if gui is not None:
        c["gui"] = gui
    c.update(allfiles=rtok(r), saveext=rtok(r), sep=rtok(r))
    n = r.randrange(0, 5) if pre is None else pre
    c["pre"] = [(rtok(r), rtok(r), rtok(r)) for _ in range(n)]
    opened = r.choice([-1, 0, 0, 0, 3]) if opened is None else opened
    found = r.choice([-1, 0, 0, 0, 4]) if found is None else found
    nxt = [r.choice([0, 1, 5]) for _ in range(r.randrange(0, 5))] + [r.choice([-1, -3])] if nxt is None else nxt
    sub = [r.choice([0, 0, 1]) for _ in range(6)] if sub is None else sub
    lengths = [r.randrange(0, 24) for _ in range(12)] if lengths is None else lengths
    c["queues"] = {"userspath": [rtok(r)], "Open": [opened], "FindFirst": [found], "FindNext": nxt, "FS_IsSubDirectory": sub,
                   "Length": lengths, "entry": [r.randrange(0, 12) for _ in range(10)]}
    return c


def first_edges():
    r = random.Random(5962)
    yield first_case(r, 0, -1, gui=False)
    yield first_case(r, 0, -1, gui=True)
    yield first_case(r, 3, -1, gui=True)
    yield first_case(r, 4, 0, -1, [-1])
    yield first_case(r, 0, 0, -1, [-1])
    yield first_case(r, 0, 3, 0, [-1], [0], [10, 5])
    yield first_case(r, 0, 0, 0, [-1], [1], [10, 5])
    yield first_case(r, 0, 0, 0, [0, 0, -1], [0], [10, 5, 7, 5, 9, 4])
    yield first_case(r, 2, 0, 5, [2, -1], [0, 1, 0], [3, 3, 8, 5, 5, 5])
    yield first_case(r, 4, 0, 0, [0, 0, 0, 0, -1], [0], [12, 5])
    yield first_case(r, 1, 0, 0, [1, -1], [1, 1, 1], [12, 5])
    yield first_case(r, 0, 0, 0, [-1], [0], [2, 5])
    yield first_case(r, 0, 0, 0, [-1], [0], [5, 5])
    yield first_case(r, 0, 0, 0, [-1], [0], [0, 0])
    yield first_case(r, 3, 0, 0, [-1], [0], [11, 5])


def first_random(r):
    return first_case(r)


TESTS = [
    FnTest("MainGameMenuConfirmQuit_OK", ppc, host, edge=quit_edges, rnd=quit_random),
    FnTest("MainMenuItemCB", ppc, host, edge=itemcb_edges, rnd=itemcb_random),
    FnTest("itemOnFrame", ppc, host, edge=frame_edges, rnd=frame_random),
    FnTest("CFSLoadSave::SaveInternal", ppc, host, edge=save_edges, rnd=save_random),
    FnTest("CFSLoadSave::GetFirstSaveFileDetails", ppc, host, edge=first_edges, rnd=first_random),
]
