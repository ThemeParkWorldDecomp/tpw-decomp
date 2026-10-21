"""Unit v51115, the number-window text objects' constructors and destructors, UIMenu::UIMenu, InterfaceDrawEngineHGradient::Show and a few
one-liners, against libtpw_unit_InterfacePasswordTextObject_InterfacePasswordTex.dylib
(tests/unit_InterfacePasswordTextObject_InterfacePasswordTex.cpp), main app ("SimTheme Park"),
src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/UIMenu.cpp, src/game/ui/ui_bogStatus.cpp, src/game/ui/ui_main.cpp, src/game/ui/ui_textmanager.cpp:
  constructors 0x1018dc38 (password), 0x1018e594 (null cash), 0x1018e874 (percent), 0x1018eb38 (visitor number), 0x1018edf4 (visitor time),
  0x1018f93c (rank), 0x1018de60 (cash, unit v5L16); destructors 0x1018df3c (cash), 0x1018e674, 0x1018e950, 0x1018ec18, 0x1018eed4;
  UIMenu::UIMenu 0x1018ac44;
  InterfaceTextBox::ClientDraw 0x1018a8fc; InterfaceBar::DrawFunction 0x1018a8c4; InterfaceDrawEngineHGradient::Show 0x10189df8;
  BogStatusPanel::DeInitViewWindow 0x10199ff4, DeInitStatusWindow 0x1019a4c4, DeInitWindow 0x1019a894, ApplyChanges 0x1019a978;
  UIMain_GetObjectStateTextColour 0x10180fc0.

A case is a world and a call. The world is built by one piece of Python code in the emulator's memory (big-endian, the Mac's offsets) and in the
host library's memory. The callees outside the unit (the sprite constructor, the formatter constructor and destructor, the sprite list, engine
data and draw object destructors, the cursor position text object's constructor, operator delete, the mesh render calls and the ride preview
destroy) are fakes on both sides that record the object they were given as a label (1 the object, 2 its formatter, 3 its sprite list, 4 its
engine data, 5 its mesh set).

What is compared: the event log, the style bytes a constructor sets (justification, wrap, font, colour mode, offsets, the colour), the UIMenu
words, the shown state, the colour answer, the bytes of the objects the one-liners must not touch, and which Change the vtable of a
constructed cash, null cash, percent, visitor number or time object reaches (unit v5L33; 0 on the host is the text sprite's). Not tested: the vtable words the Mac stores
(the host has native vtables), the delete flag value the Mac passes on to the callees (the host's members have no flag), a NULL object (the
compiler's check, not a translated function). No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InterfacePasswordTextObject_InterfacePasswordTex"
MASK = 0xFFFFFFFF
P = "tpw_vi_"

CTORS = {
    "InterfaceNullCashTextObject::InterfaceNullCashTextObject": (1, 0x1018E594),
    "InterfacePercentTextObject::InterfacePercentTextObject": (2, 0x1018E874),
    "InterfaceVisitorNumTextObject::InterfaceVisitorNumTextObject": (3, 0x1018EB38),
    "InterfaceVisitorTimeTextObject::InterfaceVisitorTimeTextObject": (4, 0x1018EDF4),
    "InterfaceRankTextObject::InterfaceRankTextObject": (5, 0x1018F93C),
    "InterfaceCashTextObject::InterfaceCashTextObject": (6, 0x1018DE60),   # unit v5L16
}
PASSWORD = "InterfacePasswordTextObject::InterfacePasswordTextObject"
# Unit v5L33: the classes whose vtable Change slot is checked (their own Change; the host's are fakes answering the id). The password and rank
# objects are left out: the host classes have no Change member yet (see include/ui_DrawObject.h).
CHANGE_IDS = {0x1018E02C: 1, 0x1018E768: 2, 0x1018EA44: 3, 0x1018ED10: 4, 0x1018EFCC: 5}
CHANGE_CLASSES = {n for n in CTORS if "Rank" not in n}
PASSWORD_ADDR = 0x1018DC38
DTORS = {
    "InterfaceCashTextObject::~InterfaceCashTextObject": (0, 0x1018DF3C),
    "InterfaceNullCashTextObject::~InterfaceNullCashTextObject": (1, 0x1018E674),
    "InterfacePercentTextObject::~InterfacePercentTextObject": (2, 0x1018E950),
    "InterfaceVisitorNumTextObject::~InterfaceVisitorNumTextObject": (3, 0x1018EC18),
    "InterfaceVisitorTimeTextObject::~InterfaceVisitorTimeTextObject": (4, 0x1018EED4),
}
NOPS = {
    "InterfaceTextBox::ClientDraw": (0, 0x1018A8FC),
    "InterfaceBar::DrawFunction": (1, 0x1018A8C4),
    "BogStatusPanel::DeInitStatusWindow": (2, 0x1019A4C4),
    "BogStatusPanel::DeInitWindow": (3, 0x1019A894),
    "BogStatusPanel::ApplyChanges": (4, 0x1019A978),
    "BogStatusPanel::DeInitViewWindow": (5, 0x10199FF4),
}
MENU = "UIMenu::UIMenu"
MENU_ADDR = 0x1018AC44
SHOW = "InterfaceDrawEngineHGradient::Show"
SHOW_ADDR = 0x10189DF8
COLOUR = "UIMain_GetObjectStateTextColour"
COLOUR_ADDR = 0x10180FC0
COLOUR_TABLE = 0x102A7780

SIZES = {a: 0x100 for _, a in list(CTORS.values()) + list(DTORS.values()) + list(NOPS.values())}
SIZES.update({PASSWORD_ADDR: 60, MENU_ADDR: 36, SHOW_ADDR: 108, COLOUR_ADDR: 20})
OWN = set(SIZES)

# ---- the callees faked on both sides
SPRITE_CTOR, CURSORPOS_CTOR = 0x1018C978, 0x1018F1D0
FMT_CTOR, FMT_DTOR, LIST_DTOR, ENGINE_DTOR, OBJ_DTOR, OP_DELETE = 0x10238F08, 0x10238E18, 0x10238CF8, 0x10238D10, 0x101CCD9C, 0x10238218
DESTROY_PREVIEW, DM_ENABLE, DM_DISABLE = 0x101815D4, 0x1003088C, 0x100309C8

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def LB(e, a):
    """The label of an address inside the case's object."""
    off = (a & MASK) - e.objbase
    return {0: 1, 0xE8: 2, 0xB4: 3, 0x9C: 4, 0x6C: 5}.get(off, -1)


def stub(addr, name, nargs, glue=False):
    """A recording fake: nargs 0 none, 1 the label, 2 the label and the data word r4 (unsigned)."""

    def f(e):
        if glue:
            # the glue saves the TOC at 0x14(r1) and the caller reloads it afterwards
            e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        if nargs == 0:
            e.cl.rec(name, ())
        elif nargs == 1:
            e.cl.rec(name, (LB(e, e.r(3)),))
        else:
            e.cl.rec(name, (LB(e, e.r(3)), e.r(4) & MASK))

    STUBS.internal(addr, f)


stub(SPRITE_CTOR, "SpriteCtor", 2)
stub(CURSORPOS_CTOR, "CursorPosCtor", 2)
stub(FMT_CTOR, "FormatterCtor", 1, True)
stub(FMT_DTOR, "FormatterDtor", 1, True)
stub(LIST_DTOR, "SpriteListDtor", 1, True)
stub(ENGINE_DTOR, "EngineDataDtor", 1, True)
stub(OBJ_DTOR, "DrawObjectDtor", 1)
stub(OP_DELETE, "OperatorDelete", 1, True)
stub(DESTROY_PREVIEW, "DestroyRidePreview", 0)
stub(DM_ENABLE, "DMEnable", 1)
stub(DM_DISABLE, "DMDisable", 1)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Ppc:
    fmt = ">"

    def __init__(self, e):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, {})
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        a = self.e.alloc(n)
        self.e.objbase = a
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, addr, args):
        return T.call_ppc(self.e, addr, [x & MASK for x in args])[0] & MASK


class Host:
    fmt = "<"

    def __init__(self, lib):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    def alloc(self, n):
        return getattr(self.lib, P + "alloc")(n * 2)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    g = lambda n: getattr(lib, P + n)
    g("alloc").restype, g("alloc").argtypes = C.c_uint64, [C.c_uint32]
    g("addr").restype, g("addr").argtypes = C.c_uint64, [C.c_char_p]
    g("ctor").restype, g("ctor").argtypes = None, [C.c_int32, C.c_uint64, C.c_uint32, C.POINTER(C.c_int64)]
    g("dtor").restype, g("dtor").argtypes = C.c_uint64, [C.c_int32, C.c_uint64, C.c_int32]
    g("show").restype, g("show").argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32]
    g("menu").restype, g("menu").argtypes = None, [C.c_uint64, C.POINTER(C.c_int64)]
    g("colour").restype, g("colour").argtypes = C.c_uint32, [C.c_int32]
    g("nop").restype, g("nop").argtypes = None, [C.c_int32, C.c_uint64]
    g("change").restype, g("change").argtypes = C.c_int32, [C.c_uint64]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the calls

FIELD_OFFS = [(0xDC, 1), (0xDD, 1), (0xDE, 1), (0xDF, 1), (0xE0, 1), (0xE1, 1), (0xE2, 2), (0xE4, 2), (0x48, 1), (0x49, 1), (0x4A, 1), (0x4B, 1)]


def run_ctor(s, case):
    name, data = case["name"], case["data"]
    obj = s.alloc(0x200)
    s.write(obj, b"\xAA" * 0x19C)
    if isinstance(s, Ppc):
        s.call(PASSWORD_ADDR if name == PASSWORD else CTORS[name][1], [obj, data])
        b = s.read(obj, 0x19C)
        fields = [int.from_bytes(b[o:o + n], "big") for o, n in FIELD_OFFS]
    else:
        out = (C.c_int64 * 12)()
        getattr(s.lib, P + "ctor")(0 if name == PASSWORD else CTORS[name][0], obj, data, out)
        fields = list(out)
    r = {"log": s.log()}
    if name != PASSWORD:
        r["fields"] = fields
    if name in CHANGE_CLASSES:
        # which Change the object's vtable reaches (Mac: the code word of the TVector in the slot at +0x14 of the vtable stored at +0x38)
        if isinstance(s, Ppc):
            code = s.e.u32(s.e.u32(s.e.u32(obj + 0x38) + 0x14))
            r["change"] = CHANGE_IDS.get(code, -1)
        else:
            r["change"] = getattr(s.lib, P + "change")(obj)
    return r


def run_dtor(s, case):
    which, addr = DTORS[case["name"]]
    obj = s.alloc(0x200)
    if isinstance(s, Ppc):
        r3 = s.call(addr, [obj, case["flag"]])
        same = r3 == obj
    else:
        same = getattr(s.lib, P + "dtor")(which, obj, case["flag"]) == obj
    return {"log": s.log(), "same": same}


def run_show(s, case):
    obj = s.alloc(0x200)
    if isinstance(s, Ppc):
        s.write(obj + 4, struct.pack(">H", case["initial"] & 0xFFFF))
        s.call(SHOW_ADDR, [obj, case["shown"]])
        state = struct.unpack(">h", s.read(obj + 4, 2))[0]
    else:
        state = getattr(s.lib, P + "show")(obj, T.s16(case["initial"]), case["shown"] & 0xFFFF if False else T.s32(case["shown"]))
    return {"log": s.log(), "state": state}


def run_menu(s, case):
    obj = s.alloc(0x200)
    s.write(obj, b"\xAA" * 0x14)
    if isinstance(s, Ppc):
        s.call(MENU_ADDR, [obj])
        words = list(struct.unpack(">5I", s.read(obj, 0x14)))
    else:
        out = (C.c_int64 * 5)()
        getattr(s.lib, P + "menu")(obj, out)
        words = list(out)
    return {"log": s.log(), "words": words}


def run_colour(s, case):
    table = struct.pack(s.fmt + "27I", *case["table"])
    if isinstance(s, Ppc):
        s.write(COLOUR_TABLE, table)
        out = s.alloc(16)
        s.call(COLOUR_ADDR, [out, case["state"]])
        answer = struct.unpack(">I", s.read(out, 4))[0]
        after = list(struct.unpack(">27I", s.read(COLOUR_TABLE, 108)))
    else:
        base = getattr(s.lib, P + "addr")(b"colours")
        s.write(base, table)
        answer = getattr(s.lib, P + "colour")(case["state"])
        after = list(struct.unpack("<27I", s.read(base, 108)))
    return {"log": s.log(), "answer": answer, "table": after}


def run_nop(s, case):
    which, addr = NOPS[case["name"]]
    obj = s.alloc(0x200)
    s.write(obj, case["bytes"])
    if isinstance(s, Ppc):
        s.call(addr, [obj])
    else:
        getattr(s.lib, P + "nop")(which, obj)
    return {"log": s.log(), "bytes": s.read(obj, 0x200)}


def run(s, case):
    try:
        n = case["name"]
        if n == PASSWORD or n in CTORS:
            return run_ctor(s, case)
        if n in DTORS:
            return run_dtor(s, case)
        if n == SHOW:
            return run_show(s, case)
        if n == MENU:
            return run_menu(s, case)
        if n == COLOUR:
            return run_colour(s, case)
        return run_nop(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e), case)


def host(lib, case):
    return run(Host(lib), case)


# ---------------------------------------------------------------------------------------------------------------- the cases

DATAS = [0, 1, 0x1234, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
FLAGS = [0, 1, -1, 2, 0x7FFF, 0x8000, -0x8000, 0x10000, 0x10001, -0x10000, 100]
STATES = [0, 1, -1, 2, 0x7FFF, 0x8000, -0x8000, 0x10000, 0x10001, 0x12345, 0xFFFF, -0x7FFF]


def ctor_case(name, data):
    return {"name": name, "queues": {}, "data": data & MASK}


def dtor_case(name, flag):
    return {"name": name, "queues": {}, "flag": flag}


def edges_for(name):
    def gen():
        if name == PASSWORD or name in CTORS:
            for d in DATAS:
                yield ctor_case(name, d)
        elif name in DTORS:
            for f in FLAGS:
                yield dtor_case(name, f)
        elif name == SHOW:
            for i in STATES:
                for sh in STATES:
                    yield {"name": name, "queues": {}, "initial": i, "shown": sh}
        elif name == MENU:
            yield {"name": name, "queues": {}}
        elif name == COLOUR:
            r = random.Random(77)
            for st in range(27):
                yield {"name": name, "queues": {}, "state": st, "table": [r.getrandbits(32) for _ in range(27)]}
        else:
            r = random.Random(99)
            yield {"name": name, "queues": {}, "bytes": bytes(r.getrandbits(8) for _ in range(0x200))}
    return gen


def rnd_for(name):
    def gen(r):
        if name == PASSWORD or name in CTORS:
            return ctor_case(name, r.getrandbits(32) if r.random() < 0.5 else r.choice(DATAS))
        if name in DTORS:
            return dtor_case(name, r.choice(FLAGS) if r.random() < 0.5 else r.randrange(-70000, 70000))
        if name == SHOW:
            pick = lambda: r.choice(STATES) if r.random() < 0.6 else r.randrange(-70000, 70000)
            c = {"name": name, "queues": {}, "initial": pick(), "shown": pick()}
            if r.random() < 0.3:
                c["initial"] = c["shown"]
            return c
        if name == MENU:
            return {"name": name, "queues": {}}
        if name == COLOUR:
            return {"name": name, "queues": {}, "state": r.randrange(27), "table": [r.getrandbits(32) for _ in range(27)]}
        return {"name": name, "queues": {}, "bytes": bytes(r.getrandbits(8) for _ in range(0x200))}
    return gen


NAMES = [PASSWORD] + list(CTORS) + list(DTORS) + [MENU] + list(NOPS) + [SHOW, COLOUR]
TESTS = [FnTest(n, ppc, host, edge=edges_for(n), rnd=rnd_for(n)) for n in NAMES]
