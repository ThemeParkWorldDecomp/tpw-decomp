"""Unit v51114: the status panel's small members, the SamsUtilities byte swaps next to them and the rank text object's destructor, against
libtpw_unit_UIStatusPanel_EnableNameClick.dylib (tests/unit_UIStatusPanel_EnableNameClick.cpp), main app ("SimTheme Park"),
src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_StatusPanel.cpp:
  UIStatusPanel::EnableNameClick 0x10194608, EditNamePanelGone 0x101945c4, ApplyChanges 0x10194674, GetName 0x1019387c, ChangeName 0x10193844,
  Remove 0x10193814, Show 0x101937dc, CloseWindow 0x10193774, DeInitViewWindow 0x10193564, DeInitStatusWindow 0x10193528, UpdateViewWindow
  0x101934ec, UpdateStatusWindow 0x101934b0, InitViewWindow 0x10193478, InitStatusWindow 0x1019343c, DeInitWindow 0x10193404, UpdateWindow
  0x101933cc, InitWindow 0x10193398, ZoomTo 0x10193330, CloseObject 0x10193010, ObjectGone 0x10192f9c, GetPreviousObject 0x10192de0,
  UIStatusPanel 0x101929e8; SamsUtilities::USwap32<TagId> 0x101926a0, USwapBlock16<wchar_t> 0x10192644, USwapInplace32<long> 0x10192600,
  USwapInplace16<short> 0x101925bc, USwap32<TagButtonAction> 0x10192544; InterfaceRankTextObject::~InterfaceRankTextObject 0x1018fa18.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for
the original, and in the host library's memory (little-endian) for the translation. The panel is a block with the Mac layout (mode +0, words
+4 +8, windows +0xc +0x10 +0x14 +0x18 +0x1c, table +0x20); the table has three entries (0xc, 0x34, 0x44), each a block the call runs (the
__ptr_glue stub on the PowerPC, a function pointer on the host) and logs; the window at +0x1c answers Enable (a mock on the host, a vtable
at +0x118 on the PowerPC). The callees outside the unit are fakes on both sides that record their arguments and answer from the case's queues.

What is compared: the answer, the event log, gStaffRoomPanelThing, the panel's words and (swaps) the buffer. Pointers are logged as
labels (panel 1, windows 2 and 3, rank object 4, buffer 5, the constructor's table 10); things are plain numbers.
No fused operations. Not tested: the rank destructor's stores of the vtable at +0x38 (the host keeps none); EnableNameClick with a NULL
window and CloseObject/ZoomTo with a NULL thing where the original would read through it (CloseObject and ZoomTo only pass it on).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UIStatusPanel_EnableNameClick"
MASK = 0xFFFFFFFF

# name -> (op number for the host, address, size)
FUNCS = [
    ("UIStatusPanel::EnableNameClick", 0, 0x10194608, 52),
    ("UIStatusPanel::EditNamePanelGone", 1, 0x101945c4, 12),
    ("UIStatusPanel::ApplyChanges", 2, 0x10194674, 4),
    ("UIStatusPanel::GetName", 3, 0x1019387c, 44),
    ("UIStatusPanel::ChangeName", 4, 0x10193844, 4),
    ("UIStatusPanel::Remove", 5, 0x10193814, 4),
    ("UIStatusPanel::Show", 6, 0x101937dc, 4),
    ("UIStatusPanel::CloseWindow", 7, 0x10193774, 52),
    ("UIStatusPanel::DeInitViewWindow", 8, 0x10193564, 4),
    ("UIStatusPanel::DeInitStatusWindow", 9, 0x10193528, 4),
    ("UIStatusPanel::UpdateViewWindow", 10, 0x101934ec, 4),
    ("UIStatusPanel::UpdateStatusWindow", 11, 0x101934b0, 4),
    ("UIStatusPanel::InitViewWindow", 12, 0x10193478, 4),
    ("UIStatusPanel::InitStatusWindow", 13, 0x1019343c, 4),
    ("UIStatusPanel::DeInitWindow", 14, 0x10193404, 4),
    ("UIStatusPanel::UpdateWindow", 15, 0x101933cc, 4),
    ("UIStatusPanel::InitWindow", 16, 0x10193398, 4),
    ("UIStatusPanel::ZoomTo", 17, 0x10193330, 60),
    ("UIStatusPanel::CloseObject", 18, 0x10193010, 64),
    ("UIStatusPanel::ObjectGone", 19, 0x10192f9c, 60),
    ("UIStatusPanel::GetPreviousObject", 20, 0x10192de0, 388),
    ("UIStatusPanel::UIStatusPanel", 21, 0x101929e8, 40),
    ("SamsUtilities::USwap32<TagId>", 22, 0x101926a0, 40),
    ("SamsUtilities::USwapBlock16<wchar_t>", 23, 0x10192644, 36),
    ("SamsUtilities::USwapInplace32<long>", 24, 0x10192600, 12),
    ("SamsUtilities::USwapInplace16<short>", 25, 0x101925bc, 12),
    ("SamsUtilities::USwap32<TagButtonAction>", 26, 0x10192544, 40),
    ("InterfaceRankTextObject::~InterfaceRankTextObject", 27, 0x1018fa18, 188),
]
OPNO = {n: o for n, o, _, _ in FUNCS}
ENTRY = {n: a for n, _, a, _ in FUNCS}
OWN = {a for _, _, a, _ in FUNCS}
VOID = {n for n, o, _, _ in FUNCS if o not in (3, 22, 26, 27)}

ADDR = dict(thing=0x103947A4, table=0x102A8644)
HOST_LAYOUT = [0xC, 0x14, 0x20]
GLUE_ENABLE = 0x1000


def s16(v):
    return T.s16(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ptr_glue(e):
    L = e.cl
    marker = e.u32(e.r(12))
    if marker == GLUE_ENABLE:
        L.rec("Enable", (L.lab(e.r(3)), s16(e.r(4))))
        e.set_r(3, L.ret("EnableAnswer") & 0xFFFF)
    else:
        L.rec("Slot", (marker, L.lab(e.r(3)), (e.r(4) & MASK) if marker == 0x34 else 0))


def stub_get_name(e):
    L = e.cl
    L.rec("GetObjectName", (e.r(3) & MASK,))
    e.set_r(3, L.ret("GetObjectName") & MASK)


def stub_send(e):
    L = e.cl
    L.rec("SendMessage", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("SendMessage") & MASK)


def stub_thing_id(e):
    L = e.cl
    L.rec("GetThingId", (e.r(4) & MASK,))
    e.write(e.r(3), struct.pack(">H", L.ret("GetThingId") & 0xFFFF))


def stub_zoom(e):
    e.cl.rec("ZoomToThing", ((e.r(3) >> 16) & 0xFFFF,))


def stub_close(e):
    e.cl.rec("CloseObject", (e.r(3) & MASK,))


def stub_reopen(e):
    e.cl.rec("Reopen", (e.r(3) & MASK,))


def stub_wrap(e):
    L = e.cl
    L.rec("WrapPrev", (s32(e.r(3)), e.r(4) & MASK))
    e.set_r(3, L.ret("WrapPrev") & MASK)


def stub_block16(e):
    L = e.cl
    L.rec("USwapBlock16", (L.lab(e.r(3)), s32(e.r(4))))


def stub_fmt(e):
    e.cl.rec("FormatterDtor", (s16(e.r(4)),))


def stub_list(e):
    e.cl.rec("SpriteListDtor", (s16(e.r(4)),))


def stub_engine(e):
    e.cl.rec("EngineDataDtor", (s16(e.r(4)),))


def stub_draw(e):
    L = e.cl
    L.rec("DrawObjectDtor", (L.lab(e.r(3)), s16(e.r(4))))


def stub_delete(e):
    L = e.cl
    L.rec("Delete", (L.lab(e.r(3)),))


def s32(v):
    return T.s32(v)


STUBS.internal(0x1023BAA8, stub_ptr_glue)
STUBS.internal(0x101810FC, stub_get_name)
STUBS.internal(0x101C9518, stub_send)
STUBS.internal(0x1012E1F8, stub_thing_id)
STUBS.internal(0x10181B00, stub_zoom)
STUBS.internal(0x1010ABCC, stub_close)
STUBS.internal(0x1010ACDC, stub_reopen)
STUBS.internal(0x101FA800, stub_wrap)
STUBS.internal(0x102398B0, stub_block16)
STUBS.internal(0x10238E18, stub_fmt)
STUBS.internal(0x10238CF8, stub_list)
STUBS.internal(0x10238D10, stub_engine)
STUBS.internal(0x101CCD9C, stub_draw)
STUBS.internal(0x10238218, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def refmap(self):
        return {self.ref(self.panel): 1, self.ref(self.wc): 2, self.ref(self.w1c): 3, self.ref(self.obj): 4,
                self.ref(self.buf): 5, self.ctor_table_ref(): 10, self.ref(self.table): 9}


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.panel = e.alloc(0x40)
        self.wc = e.alloc(0x20)
        self.w1c = e.alloc(0x120)
        self.table = e.alloc(0x50)
        self.obj = e.alloc(0x200)
        self.buf = e.alloc(0x40)
        self.L = e.cl = CL.PpcLog({self.panel: 1, self.wc: 2, self.w1c: 3, self.obj: 4, self.buf: 5}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def ref(self, a):
        return a

    def ctor_table_ref(self):
        return ADDR["table"]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def make_slot(self, off):
        a = self.e.alloc(8)
        self.words(a, [off, 0])
        return a

    def setup_window(self):
        vt = self.e.alloc(0x40)
        self.words(vt + 0x1C, [self.make_slot(GLUE_ENABLE)])
        self.words(self.w1c + 0x118, [vt])

    def set_thing(self, v):
        self.words(ADDR["thing"], [v])

    def get_thing(self):
        return self.rd_words(ADDR["thing"], 1)[0]

    def call(self, name, p, a):
        r3, _, _ = T.call_ppc(self.e, ENTRY[name], [p & MASK, a & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.panel = lib.tpw_us_alloc(0x40)
        self.wc = lib.tpw_us_alloc(0x20)
        self.w1c = lib.tpw_us_make_window()
        self.table = lib.tpw_us_alloc(0x50)
        self.obj = lib.tpw_us_alloc(0x400)
        self.buf = lib.tpw_us_alloc(0x40)
        CL.host_begin(lib, {1: self.panel, 2: self.wc, 3: self.w1c, 4: self.obj, 5: self.buf}, case["queues"])

    def done(self):
        pass

    def ref(self, a):
        return self.lib.tpw_us_offset(a)

    def ctor_table_ref(self):
        return self.lib.tpw_us_offset(self.lib.tpw_us_addr(b"table"))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def make_slot(self, off):
        a = self.lib.tpw_us_alloc(16)
        C.memmove(a, struct.pack("<Q", self.lib.tpw_us_slot_fn(off)), 8)
        return self.lib.tpw_us_offset(a)

    def setup_window(self):
        pass

    def set_thing(self, v):
        self.lib.tpw_us_set_thing(v)

    def get_thing(self):
        return self.lib.tpw_us_get_thing() & MASK

    def call(self, name, p, a):
        r = self.lib.tpw_us_call(OPNO[name], p, a, 0) & MASK
        if name.startswith("InterfaceRank") and r == self.obj & MASK:
            r = self.ref(self.obj)       # the returned pointer as the offset the shared code names it by
        return r


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_us_"
    lib.tpw_us_addr.restype, lib.tpw_us_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_us_alloc.restype, lib.tpw_us_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_us_offset.restype, lib.tpw_us_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_us_make_window.restype, lib.tpw_us_make_window.argtypes = C.c_uint64, []
    lib.tpw_us_slot_fn.restype, lib.tpw_us_slot_fn.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_us_set_thing.restype, lib.tpw_us_set_thing.argtypes = None, [C.c_uint64]
    lib.tpw_us_get_thing.restype, lib.tpw_us_get_thing.argtypes = C.c_uint64, []
    lib.tpw_us_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_us_call.restype, lib.tpw_us_call.argtypes = C.c_uint64, [C.c_int32, C.c_uint64, C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_us_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.setup_window()
    table = s.table
    for off in (0xC, 0x34, 0x44):
        s.words(table + off, [s.make_slot(off)])
    s.words(s.panel, [case["mode"], case["w4"], case["w8"], s.ref(s.wc) if case["wc"] else 0, case["w10"], case["w14"], case["w18"],
                      s.ref(s.w1c), s.ref(s.table)])
    s.set_thing(case["thing"])
    if case["bytes"] is not None:
        s.write(s.buf, case["bytes"])


def execute(s, name, case):
    a = case["arg"]
    if name in SWAP_VALUE:
        return s.call(name, a, 0)
    if name in SWAP_BUF:
        return s.call(name, s.buf, a)
    if name.startswith("InterfaceRank"):
        return s.call(name, 0 if case["null"] else s.obj, a)
    return s.call(name, s.panel, a)


SWAP_VALUE = {"SamsUtilities::USwap32<TagId>", "SamsUtilities::USwap32<TagButtonAction>"}
SWAP_BUF = {"SamsUtilities::USwapBlock16<wchar_t>", "SamsUtilities::USwapInplace32<long>", "SamsUtilities::USwapInplace16<short>"}


def observe(s, name, case, answer):
    refs = s.refmap()
    ws = [refs.get(w, w) for w in s.rd_words(s.panel, 9)]
    if name.startswith("InterfaceRank") or name in SWAP_VALUE:
        answer = refs.get(answer, answer) if name.startswith("InterfaceRank") else answer
    elif name in VOID:
        answer = 0
    return {"answer": answer, "log": s.log(), "thing": s.get_thing(), "panel": ws, "buf": s.read(s.buf, 0x40)}


def make_run(name):
    def run(s, case):
        try:
            build(s, case)
            return observe(s, name, case, execute(s, name, case))
        finally:
            s.done()

    def ppc(e, case):
        return run(Ppc(e, case), case)

    def host(lib, case):
        return run(Host(lib, case), case)

    return ppc, host


# ---------------------------------------------------------------------------------------------------------------- case generators

MODES = [-3, -2, -1, 0, 1, 2, 3, 4, 5, 0x3E8, 0x3E9, 0x3EA, 0x3EB, 0x3EC, 0x3ED, 0x7FFFFFFF, -0x80000000]
THINGS = [0x5000, 0x6000, 0x12345678]


def base(r, name):
    thing = r.choice(THINGS + [0])
    other = r.choice([0x7000, 0x8000, 0xFFFF0000])
    c = {
        "mode": r.choice(MODES) & MASK if r.random() < 0.9 else r.getrandbits(32),
        "w4": r.getrandbits(32), "w8": r.getrandbits(32), "w10": r.getrandbits(32), "w14": r.getrandbits(32), "w18": r.getrandbits(32),
        "wc": r.choice([0, 1]), "thing": thing, "other": other, "null": r.choice([False, False, True]), "bytes": None,
        "arg": r.getrandbits(32),
    }
    c["queues"] = {
        "EnableAnswer": [r.getrandbits(16)], "GetObjectName": [r.getrandbits(32)], "SendMessage": [r.getrandbits(32)],
        "GetThingId": [r.getrandbits(16)], "WrapPrev": [r.choice([0, thing, other, r.getrandbits(32)])],
    }
    if name == "UIStatusPanel::EnableNameClick":
        c["arg"] = r.choice([0, 1, -1, 0x8000, 0x10001, 0xFFFF8000, 0x7FFF, r.getrandbits(32)]) & MASK
    elif name == "UIStatusPanel::CloseObject":
        c["arg"] = r.choice([0, 1, -1, 0x100, 0x80000000, r.getrandbits(32)]) & MASK
    elif name in ("UIStatusPanel::ObjectGone", "UIStatusPanel::Show"):
        c["arg"] = r.choice([thing, other, 0, thing, r.getrandbits(32)])
    elif name == "UIStatusPanel::ChangeName":
        c["arg"] = r.choice([0, 0x10000, r.getrandbits(32)])
    elif name in SWAP_VALUE:
        c["arg"] = r.choice([0, 1, 0x80000000, 0xFFFFFFFF, 0x12345678, 0x000000FF, 0xFF000000, r.getrandbits(32)])
    elif name in SWAP_BUF:
        c["bytes"] = bytes(r.getrandbits(8) for _ in range(0x40))
        c["arg"] = r.choice([0, 1, -1, 7, r.getrandbits(32)]) & MASK
    elif name.startswith("InterfaceRank"):
        c["arg"] = r.choice([0, 1, -1, 2, 0x7FFF, 0x8000, 0xFFFF8000, 0x10001, 0x10000, r.getrandbits(32)]) & MASK
    return c


def make_test(name):
    ppc, host = make_run(name)

    def edges():
        r = random.Random(sum(map(ord, name)))
        for i in range(48):
            c = base(r, name)
            if name == "UIStatusPanel::GetPreviousObject":
                c["mode"] = MODES[i % len(MODES)] & MASK
                c["queues"]["WrapPrev"] = [[0, c["thing"], c["other"]][(i // len(MODES)) % 3]]
            yield c

    def rnd(r):
        return base(r, name)

    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


TESTS = [make_test(n) for n, _, _, _ in FUNCS]
