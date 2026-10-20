"""Unit v5929, the visitor status panel, against libtpw_unit_VisitorStatusMsg_2.dylib (tests/unit_VisitorStatusMsg_2.cpp), main app
("SimTheme Park"), src/ActionRecorder_CActionRecorder_CActionRecorder_VisitorStatusMsg_2.cpp:
  VisitorStatusMsg 0x1014320c, __sinit_ui_visitorStatus_cpp 0x101435c4, VisitorStatusPanel::UpdateWindow 0x101430c0,
  VisitorStatusPanel::Show 0x101433f8, VisitorStatusPanel::InitWindow 0x10142f44

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the panel's Remove, Locate and UpdateStatusWindow, the base panel's GetNextObject, GetPreviousObject, FillObjectList, OpenWindow and
constructor, the window methods, the sprite allocation and constructor, the advisor and sound calls, the string formatter, the sub-handler
DefaultStatusPanelMsg, CreateAllKidsPanel) are fakes on both sides that record their arguments (objects as labels) and answer from the case.
The panel's three virtual slots (0x1c, 0x20, 0x34 of the table at +0x20) are fakes that record the slot and the panel.

The world of a case: the panel's ten words (the Mac layout: state, window at 0x0c, table at 0x20, the word at 0x24), the thing globals, the
person word at +0x1d8 of the staff-room thing, the sound bank word, the two sinit words, the string table object of gText. What is compared:
the answer, the event log, the panel words (pointers as labels), the thing globals, the sinit words.

No fused operations. Not tested: a NULL window in the panel (the fakes would accept it, but the original then runs on a NULL object),
operator new answering NULL (the runtime never does). The panel's table pointer after the static initialiser is the Mac's constant on the
original and an arena block on the host: both are compared as the label "the table".
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_VisitorStatusMsg_2"
MASK = 0xFFFFFFFF

FN_MSG, FN_SINIT, FN_UPD, FN_SHOW, FN_INIT = 0x1014320C, 0x101435C4, 0x101430C0, 0x101433F8, 0x10142F44
SIZES = {FN_MSG: 432, FN_SINIT: 100, FN_UPD: 276, FN_SHOW: 112, FN_INIT: 264}
OWN = set(SIZES)

ADDR = dict(panel=0x1037EC14, thing=0x1037EC10, ec3c=0x1037EC3C, ec40=0x1037EC40, staff=0x103947A4, buffer=0x10380C48,
            bank1=0x102F7C98, text6=0x1037FF18 + 0x564)
SCRIPT = 0x102A4758
MSG_TVECTOR = 0x10266E00
TABLE = 0x102A4B5C
HOST_LAYOUT = [0x0C, 0x20, 1]

# objects with labels (the panel and the buffer are globals, the others blocks of the case)
BLOCKS = ["win", "child", "thing", "thing2", "sprite", "font", "tbl", "txt"]
LABELS = dict(panel=1, win=2, child=3, thing=4, thing2=5, sprite=6, font=7, tbl=8, buffer=9, txt=11)
VT_LABEL = 10
SLOTS = (0x1C, 0x20, 0x34)


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk(name, spec, ret=None, extra=None):
    """A recording fake: spec has one letter per argument register from r3: L label, i signed int."""
    def fn(e):
        L = e.cl
        vals = []
        for k, c in enumerate(spec):
            r = e.r(3 + k)
            vals.append(L.lab(r) if c == "L" else s32(r))
        L.rec(name, tuple(vals))
        if ret == "queue":
            e.set_r(3, L.ret(name) & MASK)
        elif callable(ret):
            e.set_r(3, ret(e) & MASK)
    return fn


def stub_open(e):
    L = e.cl
    L.rec("OpenWindow", (L.lab(e.r(3)), int(e.r(4) == SCRIPT), int(e.r(5) == MSG_TVECTOR), s32(e.r(6)), s32(e.r(7)), s32(e.r(8))))


def stub_format(e):
    L = e.cl
    ident = e.r(6)
    magic = tag_id = value = flag = 0
    if ident == 0x1C8:
        t = e.r(7)
        magic, tag_id, value, flag = e.u32(t), e.u32(t + 8), s32(e.u32(t + 0xC)), s32(e.u32(t + 0x10))
    L.rec("FormatString", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK, ident & MASK, magic, tag_id, value, flag))
    e.set_r(3, 0)


def stub_ptr_glue(e):
    L = e.cl
    L.rec("slot", (e.slotmap[e.r(12)], L.lab(e.r(3))))


STUBS.internal(0x1014356C, mk("Remove", "L"))
STUBS.internal(0x1014350C, mk("Locate", "L"))
STUBS.internal(0x10142AE4, mk("UpdateStatusWindow", "L"))
STUBS.internal(0x1018227C, mk("CancelAdvisorMessage", "i"))
STUBS.internal(0x101821CC, mk("AdvisorMessage", "i"))
STUBS.internal(0x101817E4, mk("UpdateSpritePreview", ""))
STUBS.internal(0x101DD5E0, mk("setAnimTimer", "Lii", ret="queue"))
STUBS.internal(0x101DD6B8, mk("startAnimTimer", "Li"))
STUBS.internal(0x101DBE40, mk("GetWindow", "Li", ret=lambda e: e.objs["child"]))
STUBS.internal(0x101DB624, mk("SetClientDrawObject", "LL"))
STUBS.internal(0x101DA204, mk("SetFontJust", "Lii"))
STUBS.internal(0x101DA2D8, mk("SetFontWrap", "Lii"))
STUBS.internal(0x10180270, mk("GetFontTableEntry", "i", ret=lambda e: e.objs["font"]))
STUBS.internal(0x101DA118, mk("SetFont", "LL"))
STUBS.internal(0x101DA3AC, mk("SetFontColour", "Liiii"))
STUBS.internal(0x101DE130, mk("SetClientDrawText", "LL"))
STUBS.internal(0x10192C28, mk("GetNextObject", "L"))
STUBS.internal(0x10192DE0, mk("GetPreviousObject", "L"))
STUBS.internal(0x10192A90, mk("FillObjectList", "LL", ret="queue"))
STUBS.internal(0x101935A0, stub_open)
STUBS.internal(0x101929E8, mk("UIStatusPanelCtor", "L"))
STUBS.internal(0x101952BC, mk("CreateAllKidsPanel", "L"))
STUBS.internal(0x10193C78, mk("DefaultStatusPanelMsg", "Liii", ret="queue"))
STUBS.internal(0x100DD9FC, mk("SOUND_SendEvent", "iiiiii"))
STUBS.internal(0x10180794, mk("SetTitleText", "LiL"))
STUBS.internal(0x102384B8, mk("OperatorNew", "i", ret=lambda e: e.objs["sprite"]))
STUBS.internal(0x1018D11C, mk("SpriteCtor", "L"))
STUBS.internal(0x10239730, stub_format)
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

    def lab_ptr(self, v):
        if v == 0:
            return 0
        for name, a in self.objs.items():
            if v == a:
                return LABELS[name]
        return -1

    def lab_word(self, w):
        if w == 0:
            return 0
        for name, a in self.objs.items():
            if name in BLOCKS and w == self.enc(a):
                return LABELS[name]
        if self.is_table(w):
            return VT_LABEL
        return w


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.objs = {name: e.alloc(0x400) for name in BLOCKS}
        for name in BLOCKS:
            e.write(self.objs[name], bytes(0x400))
        self.objs["panel"] = ADDR["panel"]
        self.objs["buffer"] = ADDR["buffer"]
        self.blocks = {slot: e.alloc(8) for slot in SLOTS}
        e.slotmap = {a: slot for slot, a in self.blocks.items()}
        e.objs = self.objs
        fixed = {a: LABELS[name] for name, a in self.objs.items()}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def enc(self, a):
        return a

    def is_table(self, w):
        return w == TABLE

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def wptr(self, a, v):
        self.words(a, [v])

    def rptr(self, a):
        return self.rd_word(a)

    def slot_block(self, slot):
        return self.blocks[slot]

    def log(self):
        return self.L.log

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.objs = {name: lib.tpw_vsp_alloc(0x400) for name in BLOCKS}
        self.objs["panel"] = self.g("panel")
        self.objs["buffer"] = self.g("buffer")
        self.blocks = {slot: lib.tpw_vsp_alloc(16) for slot in SLOTS}
        lib.tpw_vsp_objs(self.objs["child"], self.objs["sprite"], self.objs["font"])
        CL.host_begin(lib, {LABELS[name]: a for name, a in self.objs.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_vsp_addr(name.encode())

    def enc(self, a):
        return self.lib.tpw_vsp_off(a)

    def is_table(self, w):
        return w != 0 and w == self.lib.tpw_vsp_tableoff()

    def alloc(self, n):
        return self.lib.tpw_vsp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def wptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def slot_block(self, slot):
        a = self.blocks[slot]
        self.write(a, struct.pack("<Q", self.lib.tpw_vsp_slotfn(slot)))
        return a

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        name = {FN_MSG: "msg", FN_SHOW: "show", FN_INIT: "init", FN_UPD: "update", FN_SINIT: "sinit"}[entry]
        fn = getattr(self.lib, "tpw_vsp_" + name)
        if name == "msg":
            return fn(args[0], s32(args[1]), s32(args[2]), s32(args[3])) & MASK
        if name == "show":
            fn(args[1])
            return 0
        fn()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_vsp_addr.restype, lib.tpw_vsp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_vsp_alloc.restype, lib.tpw_vsp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_vsp_off.restype, lib.tpw_vsp_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_vsp_tableoff.restype, lib.tpw_vsp_tableoff.argtypes = C.c_uint32, []
    lib.tpw_vsp_objs.restype, lib.tpw_vsp_objs.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_vsp_slotfn.restype, lib.tpw_vsp_slotfn.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_vsp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_vsp_msg.restype, lib.tpw_vsp_msg.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_vsp_show.restype, lib.tpw_vsp_show.argtypes = None, [C.c_uint64]
    for n in ("init", "update", "sinit"):
        getattr(lib, "tpw_vsp_" + n).restype = None
        getattr(lib, "tpw_vsp_" + n).argtypes = []
    out = (C.c_int64 * 32)()
    n = lib.tpw_vsp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o = s.objs
    s.wptr(s.g("staff"), o[case["staff"]] if case["staff"] else 0)
    s.wptr(s.g("thing"), o[case["vthing"]] if case["vthing"] else 0)
    s.wptr(s.g("text6"), o["txt"])
    s.words(o["thing"] + 0x1D8, [case["w1d8"]])
    s.words(s.g("bank1"), [case["bank"]])
    s.words(s.g("ec3c"), [case["ec3c"]])
    s.words(s.g("ec40"), [case["ec40"]])
    table = o["tbl"]
    for slot in SLOTS:
        s.words(table + slot, [s.enc(s.slot_block(slot))])
    w = list(case["pw"])
    if case["op"] != "sinit":
        w[3] = s.enc(o["win"])
        w[8] = s.enc(table)
    s.words(s.g("panel"), w)


def execute(s, case):
    op = case["op"]
    if op == "msg":
        return s.call(FN_MSG, [s.objs["win"]] + case["args"])
    if op == "show":
        t = case["args"][0]
        return s.call(FN_SHOW, [s.objs["panel"], s.objs[t] if t else 0])
    if op == "init":
        return s.call(FN_INIT, [s.objs["panel"]])
    if op == "update":
        return s.call(FN_UPD, [s.objs["panel"]])
    return s.call(FN_SINIT, [])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(),
            "thing": s.lab_ptr(s.rptr(s.g("thing"))), "staff": s.lab_ptr(s.rptr(s.g("staff"))),
            "ec3c": s.rd_word(s.g("ec3c")), "ec40": s.rd_word(s.g("ec40")), "bank1": s.rd_word(s.g("bank1")),
            "panel": [s.lab_word(w) for w in s.rd_words(s.g("panel"), 10)]}


def run(s, case):
    try:
        build(s, case)
        answer = execute(s, case)
        return observe(s, case, answer if case["op"] == "msg" else 0)   # the others answer nothing (r3 is whatever was left)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

MSGS = [0, 5, 0xF, 0x10, 0x11, 0x13, 0x14, 0x15, 0x16, 0x1E, 0x1F, 0xFF, 0x100, 0x101, -1]
AS = [0x80080, 0x80081, 0x7FF80, 0x80000, 0, -1, -2, -3, 0x371F, 0x3720, 0x3721, 0x3722, 0x3736, 0x3737, 0x3738, 0x3739]
ANIM = [0, 1, 0x10000, 5, 0xFFFF0000, 0xFFFF]
W1D8 = [0, 1, 0xFFFFFFFF, 0x80000000, 7]


def r32(r):
    return r.getrandbits(32)


def base(r, op):
    return {"op": op,
            "queues": {"setAnimTimer": [r.choice(ANIM)], "DefaultStatusPanelMsg": [r32(r)], "FillObjectList": [r.randrange(0, 6)]},
            "staff": r.choice([None, "thing", "thing2"]), "vthing": r.choice([None, "thing", "thing2"]),
            "w1d8": r.choice(W1D8) if r.random() < 0.7 else r32(r), "bank": r32(r), "ec3c": r32(r), "ec40": r32(r),
            "pw": [r32(r) for _ in range(10)], "args": []}


def msg_case(r, m=None, a=None):
    c = base(r, "msg")
    m = (r.choice(MSGS) if r.random() < 0.8 else T.s32(r32(r))) if m is None else m
    a = (r.choice(AS) if r.random() < 0.7 else T.s32(r32(r))) if a is None else a
    c["args"] = [m & MASK, a & MASK, r32(r)]
    return c


def msg_edges():
    r = random.Random(5929)
    for m in MSGS:
        for a in AS:
            yield msg_case(r, m, a)


def msg_random(r):
    return msg_case(r)


def show_case(r, t="x"):
    c = base(r, "show")
    c["args"] = [r.choice([None, "thing", "thing2"]) if t == "x" else t]
    return c


def show_edges():
    r = random.Random(5930)
    for t in (None, "thing", "thing2"):
        for _ in range(3):
            yield show_case(r, t)


def show_random(r):
    return show_case(r)


def init_edges():
    r = random.Random(5931)
    for v in (None, "thing", "thing2"):
        c = base(r, "init")
        c["vthing"] = v
        yield c


def init_random(r):
    return base(r, "init")


def update_edges():
    r = random.Random(5932)
    for staff in (None, "thing"):
        for w in W1D8:
            c = base(r, "update")
            c["staff"], c["w1d8"] = staff, w
            yield c


def update_random(r):
    return base(r, "update")


def sinit_edges():
    r = random.Random(5933)
    for _ in range(4):
        yield base(r, "sinit")


def sinit_random(r):
    return base(r, "sinit")


TESTS = [
    FnTest("VisitorStatusMsg", ppc, host, edge=msg_edges, rnd=msg_random),
    FnTest("__sinit_ui_visitorStatus_cpp", ppc, host, edge=sinit_edges, rnd=sinit_random),
    FnTest("VisitorStatusPanel::UpdateWindow", ppc, host, edge=update_edges, rnd=update_random),
    FnTest("VisitorStatusPanel::Show", ppc, host, edge=show_edges, rnd=show_random),
    FnTest("VisitorStatusPanel::InitWindow", ppc, host, edge=init_edges, rnd=init_random),
]
