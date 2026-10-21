"""Unit v5022, the congratulations and bankrupt windows of the main app, against libtpw_unit_UI_Congratulations.dylib
(tests/unit_UI_Congratulations.cpp), main app ("SimTheme Park"), src/game/ui/ui_main.cpp:
  UI_Congratulations 0x1018724c, UI_Bankrupt 0x10186f34, __sinit_ui_main_cpp 0x101877b8, CongratsWindowMsg 0x10186b9c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee
outside the unit is a fake on both sides that records its arguments (pointers as labels) and answers from the case's queues: the first
person and key table calls, the window calls (CreateObject, Build, GetPosition, setPivot, startAnim, SetCallbackFunction, CaptureKb,
CallDefaultWindowHandler), the virtual Enable of the built window (through __ptr_glue on the Mac, a fake table on the host), the
rectangle's constructor and destructor, the light, the cursor table and the string constructors, __register_global_object (the
destructor token and the node), the game type constructor, the string table, the converter and the dialog box (its parameter block
read back as a whole).

The world of a case: the window globals (root, game, first person, panel, bankrupt), the key table flag, the world, the text object's
three resource pointers, the settings byte, the tick word, the game type cell and its guard byte, the two words and the light's flags
word the static initialiser sets. What is compared: the answer, the event log, and every one of those cells read back.

No fused operations (the only floats are the constants 0.0 and 1.0 the initialiser hands to the light). Not tested: a NULL table or
NULL converter (the original calls through them and the fakes only log), a guard byte other than 0 or 1 (the host's guard test looks
at bit 0, the Mac's at the whole byte), a game type outside 0..3 (the host holds it in an enum), the contents of the scripts and the
animations (only their addresses are used).

A last test (unit v5L39), "the window animations": the host's congratulations and bankrupt animations as the library was loaded against
the program image's frames (tests/imagedata.py), one case per frame. Before v5L39 the host had one frame of each, and the animation ran on
into the memory after it.
"""
import ctypes as C
import random
import struct

import calllog as CL
import imagedata
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UI_Congratulations"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
FN_CONG, FN_BANK, FN_SINIT, FN_MSG = 0x1018724C, 0x10186F34, 0x101877B8, 0x10186B9C
SIZES = {FN_CONG: 436, FN_BANK: 428, FN_SINIT: 436, FN_MSG: 860}
OWN = set(SIZES)
FN = {"cong": FN_CONG, "bank": FN_BANK, "sinit": FN_SINIT, "msg": FN_MSG}

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_u5022_addr(name)
ADDR = dict(
    kt=0x1038CC54, bank=0x1038CC58, panel=0x1038CC5C, game=0x103949E4, fp=0x103949E0, root=0x10395C24, world=0x1037EBFC,
    ticks=0x10380B78, gtype=0x102B3AA0, guard=0x102B3AA4, rmb=0x1038071C + 0x39, res=0x1037FF18 + 0x54C,
    w8=0x1038CCF8, wc=0x1038CCFC, light=0x1038CC78, lightflags=0x1038CC78 + 0x60, cursor=0x1038CC64,
    frame=0x10380BF4, slider=0x10380BD4, plain=0x10380BB4, button=0x10380B94,
    nlight=0x1038CCEC, ncursor=0x1038CC6C, nframe=0x10380C08, nslider=0x10380BE8, nplain=0x10380BC8, nbutton=0x10380BA8,
    dtlight=0x102657B8, dtcursor=0x10268138, dtstring=0x103BCD38, cbcong=0x10267688, cbbank=0x10267690,
    cscript=0x102A772C, bscript=0x102A7756, canim=0x102A79D4, banim=0x102A7914, convbuf=0x10380C48,
)
# ---- the host's offsets the module relies on, checked once against tpw_u5022_layout (catches a header that moved):
# the text object's first resource pointer, the settings byte, the rectangle's right and bottom
HOST_LAYOUT = [2288, 57, 12, 14]

# the objects of a case (a block each, labelled), and the names the log knows
DYN = ["obj", "root", "panel", "game", "fp", "created", "built", "win", "world", "table", "conv", "conv1", "str"]
STATIC = ["light", "cursor", "frame", "slider", "plain", "button", "nlight", "ncursor", "nframe", "nslider", "nplain", "nbutton",
          "dtlight", "dtcursor", "dtstring", "cbcong", "cbbank", "cscript", "bscript", "canim", "banim", "convbuf", "gtype"]
LABN = {name: i + 1 for i, name in enumerate(STATIC + DYN)}
ENABLE_CODE = 0x10EEEEE0        # the code word of the Enable slot's transition vector (the fake __ptr_glue looks for it)


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def glue(fn):
    """A fake at a glue address: the glue saves the caller's TOC at 0x14(r1) and the caller reloads it after the call."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapped


def this_back(e, log):
    return e.r(3)


def stub_getpos(e):
    """InterfaceWindow::GetPosition(CoolRect*, short): fills the rectangle's right and bottom, answers from the queue."""
    L = e.cl
    rect = e.r(4)
    L.rec("GetPosition", (L.lab(e.r(3)), L.lab(rect), s16(e.r(5))))
    e.write(rect + 8, struct.pack(">hh", s16(L.ret("GetPositionRight")), s16(L.ret("GetPositionBottom"))))
    e.set_r(3, L.ret("GetPosition") & MASK)


def stub_create(e):
    L = e.cl
    L.rec("CreateObject", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), s16(e.r(7)), s16(e.r(8)), s16(e.r(9)), s16(e.r(10))))
    e.set_r(3, L.objs["created"])


def stub_build(e):
    L = e.cl
    L.rec("Build", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))
    e.set_r(3, L.objs["built"])


def stub_ptr_glue(e):
    """The virtual call through the Enable slot: r12 holds the transition vector, r3 the window, r4 the argument."""
    L = e.cl
    code = e.u32(e.r(12))
    assert code == ENABLE_CODE, "unexpected virtual call %x" % code
    L.rec("Enable", (L.lab(e.r(3)), s16(e.r(4))))
    e.set_r(3, L.ret("Enable") & MASK)


def stub_setnormal(e):
    L = e.cl
    L.rec("SetNormal", (L.lab(e.r(3)), fb(e.f(1)), fb(e.f(2)), fb(e.f(3))))


def stub_setcolor(e):
    L = e.cl
    L.rec("SetColor", (L.lab(e.r(3)), fb(e.f(1)), fb(e.f(2)), fb(e.f(3)), fb(e.f(4))))


def stub_cgametype(e):
    L = e.cl
    this = e.r(3)
    L.rec("CGameType", (L.lab(this),))
    e.write(this, struct.pack(">I", L.ret("CGameType") & MASK))


def stub_getconst(e):
    L = e.cl
    L.rec("GetConstString", (L.lab(e.r(3)), e.r(4) & MASK))
    e.set_r(3, L.objs["str"] if L.ret("GetConstString") else 0)


def stub_dialog(e):
    """UI_DialogBox(params&, dlgbutton, cb, dlgbutton, cb, dlgbutton, cb, bool): the block is read back as a whole."""
    L = e.cl
    p = e.r(3)
    rgb = e.read(p + 4, 3)
    L.rec("UI_DialogBox", (L.lab(e.u32(p)), (rgb[0] << 16) | (rgb[1] << 8) | rgb[2], s32(e.u32(p + 8)), s32(e.u32(p + 12)),
                           s32(e.r(4)), L.lab(e.r(5)), s32(e.r(6)), L.lab(e.r(7)), s32(e.r(8)), L.lab(e.r(9)), e.r(10) & 0xFF))


STUBS.internal(0x10187434, CL.recorder("GetFullyOutOfFirstPerson"))
STUBS.internal(0x1010ED8C, CL.recorder("GoFirstPerson", "Li"))
STUBS.internal(0x1015108C, CL.recorder("DisableAllKeytables"))
STUBS.internal(0x10151010, CL.recorder("EnableAllKeytables"))
STUBS.internal(0x101A8C1C, CL.recorder("GameWindowToggle_Hidden", "", ret="i"))
STUBS.internal(0x101A8AC0, CL.recorder("ToggleGameWindow"))
STUBS.internal(0x101C9518, CL.recorder("UI_SendMessage", "Liii"))
STUBS.internal(0x1018116C, CL.recorder("UIMain_DecideKbCapture"))
STUBS.internal(0x101CAC3C, CL.recorder("IveGotTicks", "", ret="i"))
STUBS.internal(0x1013BE2C, CL.recorder("EndWinningSequence", "L"))
STUBS.internal(0x101DC750, stub_create)
STUBS.internal(0x101DEB78, stub_build)
STUBS.internal(0x1023BAA8, glue(stub_ptr_glue))
STUBS.internal(0x101C7448, CL.recorder("CoolRect", "L"))
STUBS.internal(0x101C74E0, CL.recorder("~CoolRect", "L"))
STUBS.internal(0x101DB884, stub_getpos)
STUBS.internal(0x101DDD34, CL.recorder("setPivot", "Lhh"))
STUBS.internal(0x101DD08C, CL.recorder("startAnim", "LLi", ret="i"))
STUBS.internal(0x101DCBA8, CL.recorder("SetCallbackFunction", "LL"))
STUBS.internal(0x101DCCEC, CL.recorder("CaptureKb", "Li"))
STUBS.internal(0x101DE318, CL.recorder("CallDefaultWindowHandler", "Liii", ret="i"))
STUBS.internal(0x10007B7C, lambda e: None)             # CMapLinkBase::CMapLinkBase: inline on the host, so not logged
STUBS.internal(0x10007A98, CL.recorder("CLight", "L"))
STUBS.internal(0x10007964, stub_setnormal)
STUBS.internal(0x1000784C, stub_setcolor)
STUBS.internal(0x101CC994, CL.recorder("CursorTable", "L", ret=this_back))
STUBS.internal(0x10239940, glue(CL.recorder("register", "LLL")))
STUBS.internal(0x102382D8, glue(CL.recorder("TbDynamicString", "Ls", ret=this_back)))
STUBS.internal(0x1016F5D8, stub_cgametype)
STUBS.internal(0x10238B78, glue(stub_getconst))
STUBS.internal(0x10238B90, glue(CL.recorder("ConvertString", "xLLLH")))
STUBS.internal(0x101F1800, stub_dialog)


def setup(runner):
    T.install_std_stubs(runner.emu)       # the C library's memset, memcpy, strlen, sqrt ... (tpwtest.py 491 on)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, w):
        self.words(a, [w])

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def byte(self, a):
        return self.read(a, 1)[0]

    def setbyte(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def res(self, i):
        return self.g("res") + self.psz * i

    def pl(self, v):
        """A pointer word as the name of the object it points at."""
        for name, a in self.objs.items():
            if (a & MASK) == v:
                return name
        return v


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"
    psz = 4

    def __init__(self, e, case):
        self.e = e
        self.objs = {n: e.alloc(0x200) for n in DYN}
        self.vt = e.alloc(0x40)
        self.tv = e.alloc(8)
        fixed = {ADDR[n]: LABN[n] for n in STATIC}
        fixed[ADDR["light"] + 0x10] = LABN["light"]        # the CLight part of the composite object
        fixed.update({a: LABN[n] for n, a in self.objs.items()})
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.L.objs = self.objs
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

    def mkwin(self, addr):
        """The window's vtable word at 0x118 -> a table whose slot 0x1c -> a transition vector (code, toc)."""
        self.words(addr + 0x118, [self.vt])
        self.words(self.vt + 0x1C, [self.tv])
        self.words(self.tv, [ENABLE_CODE, 0])

    def returns(self):
        pass

    def call(self, which, args):
        r3, _, _ = T.call_ppc(self.e, FN[which], [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    """The same on the host library."""
    fmt = "<I"
    psz = 8

    def words(self, a, ws):
        """A host pointer (above 32 bits: the arena is not 32-bit addressable here) is written whole, other words as 4 bytes."""
        self.write(a, b"".join(struct.pack("<Q", w) if w > MASK else struct.pack("<I", w & MASK) for w in ws))

    def ptr(self, a, w):
        self.write(a, struct.pack("<Q", w))

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.objs = {n: lib.tpw_u5022_alloc(0x200) for n in DYN}
        fixed = {LABN[n]: self.g(n) for n in STATIC}
        fixed.update({LABN[n]: a for n, a in self.objs.items()})
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_u5022_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_u5022_alloc(n)       # zeroed, 32-bit addressable (platform/arena.cpp)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def mkwin(self, addr):
        self.lib.tpw_u5022_initwin(addr)

    def returns(self):
        """What the fake CreateObject, Build and GetConstString answer with (the low words of the pointers)."""
        self.words(self.g("hcreated"), [self.objs["created"]])
        self.words(self.g("hbuilt"), [self.objs["built"]])
        self.words(self.g("hstr"), [self.objs["str"]])

    def call(self, which, args):
        r = getattr(self.lib, "tpw_u5022_" + which)(*args)
        return (r or 0) & MASK


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    p = "tpw_u5022_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "initwin").restype, getattr(lib, p + "initwin").argtypes = None, [C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "cong").restype, getattr(lib, p + "cong").argtypes = None, [C.c_uint64]
    getattr(lib, p + "bank").restype, getattr(lib, p + "bank").argtypes = None, [C.c_uint64]
    getattr(lib, p + "sinit").restype, getattr(lib, p + "sinit").argtypes = None, []
    getattr(lib, p + "msg").restype = C.c_uint32
    getattr(lib, p + "msg").argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's globals and blocks through Side.words: the same code for both memories (offsets are the Mac's)."""
    o = s.objs
    s.returns()
    s.mkwin(o["built"])
    s.ptr(s.g("root"), o["root"])
    s.ptr(s.g("game"), o["game"])
    s.ptr(s.g("fp"), o["fp"])
    s.ptr(s.g("world"), o["world"])
    s.ptr(s.g("panel"), o["panel"] if case["panel"] else 0)
    s.ptr(s.g("bank"), o["obj"] if case["bw"] else 0)
    s.setbyte(s.g("kt"), case["kt"])
    s.words(s.g("ticks"), [case["tick"]])
    s.words(s.g("gtype"), [case["gtype"]])
    s.setbyte(s.g("guard"), case["guard"])
    if isinstance(s, Host):
        s.write(s.g("guard") + 1, bytes(7))        # the host's guard is 8 bytes; the runtime leaves flags in the others
    s.setbyte(s.g("rmb"), case["rmb"])
    s.ptr(s.res(0), o["conv"])
    s.ptr(s.res(1), o["conv1"] if case["res1"] else 0)
    s.ptr(s.res(6), o["table"])
    s.words(s.g("w8"), [case["w8"]])
    s.words(s.g("wc"), [case["wc"]])
    s.words(s.g("lightflags"), [case["lf"]])


def execute(s, case):
    """The call."""
    op = case["op"]
    if op == "cong":
        s.call("cong", [s.objs["obj"]])
    elif op == "bank":
        s.call("bank", [s.objs["obj"]])
    elif op == "sinit":
        s.call("sinit", [])
    else:
        m, a, b = case["args"]
        return s32(s.call("msg", [s.objs["win"], s32(m), s32(a), s32(b)]))
    return 0


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides; pointers as the names of the objects."""
    return {"answer": answer, "log": s.log(),
            "kt": s.byte(s.g("kt")), "ticks": s.rd_word(s.g("ticks")), "panel": s.pl(s.rd_word(s.g("panel"))),
            "bank": s.pl(s.rd_word(s.g("bank"))), "gtype": s.rd_word(s.g("gtype")), "guard": s.byte(s.g("guard")),
            "w8": s.rd_word(s.g("w8")), "wc": s.rd_word(s.g("wc")), "lf": s.rd_word(s.g("lightflags"))}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


LAST = {}


def ppc(e, case):
    LAST["p"] = run(Ppc(e, case), case)
    return LAST["p"]


def host(lib, case):
    h = run(Host(lib, case), case)
    p = LAST.get("p")
    if p is not None and p != h:
        for k in p:
            if p[k] != h[k]:
                if k == "log":
                    print("DBG log tails", p[k][-3:], h[k][-3:])
                else:
                    print("DBG", k, p[k], h[k])
    return h


# ---------------------------------------------------------------------------------------------------------------- case generators

WORDS = [0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x10000, 0x12345678]
OTHER_MSGS = [0, 1, 4, 5, 6, 0x14, 0x16, 0x17, 0x10005, 0x10007, 0x1000A, 0x1000C, 0x1000B + 0x10000, 0x7FFFFFFF, 0x80000000,
              0xFFFFFFFF, 0x10006 | 0x80000000, 0x15 | 0x100000000 & MASK]


def i16(r):
    return r.choice([0, 1, -1, 2, 3, 0x7FFF, -0x8000, 0x7FF, 0x5FF, 1023, r.randrange(-40000, 40000)])


def base(r, op):
    """The world every case starts from: random globals; the fakes' answer queues cover every branch."""
    tick = r.choice(WORDS + [r.randrange(1 << 32)])
    delta = r.choice([2000, 2001, 1999, 0, -1, 0x7FFFFFFF, 0x80000000, r.randrange(1 << 32), r.randrange(0, 5000)])
    return {
        "op": op,
        "queues": {
            "GameWindowToggle_Hidden": [r.choice([0, 1, -1, 2])],
            "IveGotTicks": [(tick + delta) & MASK],
            "GetPositionRight": [i16(r)], "GetPositionBottom": [i16(r)], "GetPosition": [r.choice([0, 1, -1])],
            "startAnim": [r.choice([0, 1, 5])], "Enable": [r.choice([0, 1, -1])],
            "CGameType": [r.choice([0, 1, 2, 3])], "GetConstString": [r.choice([0, 1])],
            "CallDefaultWindowHandler": [r.choice(WORDS) & MASK],
        },
        "panel": r.choice([0, 1]), "bw": r.choice([0, 1]), "kt": r.choice([0, 1, 2, 0x80, 0xFF]), "tick": tick,
        "gtype": r.choice([0, 1, 2, 3, 0xFFFFFFFF, 0x80000000]), "guard": r.choice([0, 1]),
        "rmb": r.choice([0, 1, 2, 0x80, 0xFF]), "res1": r.choice([0, 1]),
        "w8": r.choice(WORDS), "wc": r.choice(WORDS), "lf": r.choice(WORDS),
        "args": [0, 0, 0],
    }


def with_delta(c, delta):
    c["queues"]["IveGotTicks"] = [(c["tick"] + delta) & MASK]


# ---- UI_Congratulations and UI_Bankrupt
def window_case(op, r):
    return base(op=op, r=r)


def window_edges(op, seed):
    r = random.Random(seed)
    for panel in (0, 1):
        for hidden in (0, 1, -1):
            for kt in (0, 1):
                c = base(r, op)
                c["panel"], c["kt"], c["bw"] = panel, kt, kt
                c["queues"]["GameWindowToggle_Hidden"] = [hidden]
                yield c
    for right, bottom in ((0, 0), (1, 1), (-1, -1), (0x7FFF, -0x8000), (0x3FF, 0x2FF), (-3, 3)):
        c = base(r, op)
        c["queues"]["GetPositionRight"], c["queues"]["GetPositionBottom"] = [right], [bottom]
        yield c


def cong_edges():
    return window_edges("cong", 50221)


def cong_random(r):
    return base(r, "cong")


def bank_edges():
    return window_edges("bank", 50222)


def bank_random(r):
    return base(r, "bank")


# ---- __sinit_ui_main_cpp
def sinit_edges():
    r = random.Random(50223)
    for lf in (0, 2, 0xFFFFFFFF):
        for w in (0, 0x10000, 0xFFFFFFFF):
            c = base(r, "sinit")
            c["lf"], c["w8"], c["wc"] = lf, w, w
            yield c


def sinit_random(r):
    return base(r, "sinit")


# ---- CongratsWindowMsg
def msg_case(r, m, a, b, **kw):
    c = base(r, "msg")
    c["args"] = [m & MASK, a & MASK, b & MASK]
    for k, v in kw.items():
        if k == "delta":
            with_delta(c, v)
        else:
            c[k] = v
    return c


def msg_edges():
    r = random.Random(50224)
    deltas = (2000, 2001, 1999, 0, -1, 0x7FFFFFFF, 0x80000000, 0x80000001)
    # the right button message: the argument, the setting and the tick distance
    for a in (1, 0, 2, -1):
        for rmb in (0, 1, 2, 0x80, 0xFF):
            for delta in deltas:
                yield msg_case(r, 0x10006, a, 0, rmb=rmb, delta=delta, gtype=2, guard=1, res1=1, **{"queues": None} if False else {})
    # the end of the window and the dialog: game type, guard, string, converter
    for gtype in (2, 1, 0, 3, 0xFFFFFFFF):
        for guard in (0, 1):
            for string in (0, 1):
                for res1 in (0, 1):
                    for m, a, b in ((0x10006, 1, 0), (0x1000B, 0x1B, 0)):
                        c = msg_case(r, m, a, b, rmb=1, delta=2001, gtype=gtype, guard=guard, res1=res1)
                        c["queues"]["GetConstString"] = [string]
                        c["queues"]["CGameType"] = [r.choice([0, 1, 2, 3])]
                        yield c
    # a game type the constructor makes
    for made in (0, 1, 2, 3):
        c = msg_case(r, 0x1000B, 0x1B, 0, gtype=0xFFFFFFFF, guard=0)
        c["queues"]["CGameType"] = [made]
        c["queues"]["GetConstString"] = [1]
        yield c
    # the key message
    for a in (0x1B, 0x1A, 0, -1, 0x11B):
        for b in (0, 0x10000, 0x1000000, 0xFF0000, 0x1B0000, 0xFFFFFFFF, 0x8000FFFF, 0xFFFF, 0xFF000000, 0x1FFFF):
            yield msg_case(r, 0x1000B, a, b, gtype=2, guard=1, res1=1)
    # the tick message and every other message down to the default handler
    for tick in WORDS:
        yield msg_case(r, 0x15, 1, 0, tick=tick, delta=r.choice([0, 5, 2001]))
    for m in OTHER_MSGS:
        for a in (1, 0x1B):
            yield msg_case(r, m, a, 0)


def msg_random(r):
    m = r.choice([0x15, 0x10006, 0x10006, 0x1000B, 0x1000B, r.randrange(1 << 32), r.choice(OTHER_MSGS)])
    a = r.choice([1, 1, 0x1B, 0x1B, 0, -1, r.randrange(-5, 0x40)])
    b = r.choice([0, 0, 0x10000, 0x1000000, r.randrange(1 << 32), 0xFF0000])
    c = msg_case(r, m, a, b)
    if r.random() < 0.5:
        c["rmb"] = 1
        c["gtype"] = 2
    return c


TESTS = [
    FnTest("UI_Congratulations", ppc, host, edge=cong_edges, rnd=cong_random),
    FnTest("UI_Bankrupt", ppc, host, edge=bank_edges, rnd=bank_random),
    FnTest("__sinit_ui_main_cpp", ppc, host, edge=sinit_edges, rnd=sinit_random),
    FnTest("CongratsWindowMsg", ppc, host, edge=msg_edges, rnd=msg_random),
    imagedata.table_test("the window animations", [("canim", ADDR["canim"], "hhhh", 29), ("banim", ADDR["banim"], "hhhh", 24)]),
]
