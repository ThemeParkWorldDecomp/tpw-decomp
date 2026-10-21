"""Unit v51022, the visitor status panel's two refresh functions, against libtpw_unit_VisitorStatusPanel_UpdateStatusWindow_2.dylib
(tests/unit_VisitorStatusPanel_UpdateStatusWindow_2.cpp), main app ("SimTheme Park"),
src/game/ui/ui_visitorStatus.cpp:
  VisitorStatusPanel::UpdateStatusWindow 0x10142ae4, VisitorStatusPanel::UpdateViewWindow 0x10142170

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian at the Mac's addresses,
host memory little-endian). The callees outside the unit are fakes on both sides recording their arguments: __ptr_glue (the window's
SetValue and SetText virtuals and the panel's slot 0x34, told apart by the vtable word they were reached through), TbITable::FormatString
(with the tag it was given), GetConstString, UIMain_SetTitleText, UIMain_ConvertMulti, InterfaceWindow::GetWindow / SetFrame /
GetClientRect, CPerson::GetTimeInPark, CParkTime::GetFunnyTimeForGameTurns, CObject::GetUIName, GAME_ASSERTMSG, the rectangle's
constructor and destructor, UIMain_ShowSpritePreview. The runtime's 64-bit divide (0x10237e1c) is stubbed on the PowerPC side as a signed
divide (the host divides the ticks by 36,000,000,000 as a signed int64; only non-negative spans are tested).

The world of a case: the panel (Mac layout: windows at 0xc, 0x10, 0x14, the table at 0x20, a word at 0x24), the visitor block read by Mac
offset, the globals gStaffRoomPanelThing, gGameWorld, gThingArray (entries 0 and the visitor's index), the three text tables, the answers of
GetWindow per id. What is compared: the event log, the panel's words and the first four characters of the text buffer.

No fused operations. Not tested: a state of 0x1c8 (the original would read a tag that was never passed), a thing index of 0x2800 or more,
windows 0x3733, 0x3734 and 0x3735 answered NULL (the original dereferences them), a negative time span.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_VisitorStatusPanel_UpdateStatusWindow_2"
MASK = 0xFFFFFFFF

FN_STATUS, FN_VIEW = 0x10142ae4, 0x10142170
SIZES = {FN_STATUS: 1056, FN_VIEW: 140}
OWN = set(SIZES)
FN = {"status": FN_STATUS, "view": FN_VIEW}

A_PTR_GLUE, A_FORMAT, A_SETTITLE, A_GETWINDOW, A_TIMEINPARK = 0x1023baa8, 0x10239730, 0x10180794, 0x101dbe40, 0x1011962c
A_FUNNY, A_DIV64, A_ASSERT, A_UINAME, A_SETFRAME = 0x10112928, 0x10237e1c, 0x1000dde8, 0x10110690, 0x101db7dc
A_GETCONST, A_CONVMULTI, A_RECTCTOR, A_RECTDTOR, A_GETCLIENT, A_SHOWSPRITE = 0x10238b78, 0x10180334, 0x101c7448, 0x101c74e0, 0x101dba4c, 0x10181648

MARK_VALUE, MARK_TEXT, MARK_SLOT = 0xFA000001, 0xFA000002, 0xFA000003

ADDR = dict(thing=0x103947a4, world=0x1037ebfc, tarr=0x1034cbfc, buf=0x10380c48,
            res6=0x1037ff18 + 0x564, res8=0x1037ff18 + 0x56c, res9=0x1037ff18 + 0x570)
IDS = list(range(0x3724, 0x3736))
CTX = {}
HOSTL = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


def mk_text(v):
    v &= MASK
    return [0x41 + v % 26, 0x61 + (v >> 5) % 26, 0x30 + (v >> 10) % 10, 0]


def units(e, a, n=4):
    return list(struct.unpack(">%dH" % n, e.read(a, 2 * n)))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def save_toc(e):
    """The glue saves the caller's TOC pointer at 0x14(r1); the caller reloads it after the call."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))


def st_ptr_glue(e):
    save_toc(e)
    m, r3, r4, L = e.r(12), e.r(3), e.r(4), e.cl
    if m == MARK_VALUE:
        L.rec("WindowSetValue", (L.lab(r3), s32(r4)))
    elif m == MARK_TEXT:
        L.rec("WindowSetText", (L.lab(r3), L.lab(r4)) + tuple(units(e, r4)))
    elif m == MARK_SLOT:
        L.rec("Slot34", (L.lab(r3),))
    else:
        raise AssertionError("ptr_glue through %x" % m)


def st_format(e):
    save_toc(e)
    L = e.cl
    table, out, size, ident, tag = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
    args = (L.lab(table), L.lab(out), size & MASK, ident & MASK)
    if ident == 0x1c8:
        args += (e.u32(tag), e.u32(tag + 8), s32(e.u32(tag + 0xc)), s32(e.u32(tag + 0x10)))
    elif ident == 0x10:
        args += (e.u32(tag), e.u32(tag + 8)) + tuple(units(e, e.u32(tag + 0xc)))
    L.rec("FormatString", args)
    e.write(out, struct.pack(">4H", *mk_text(L.ret("FormatString"))))


def st_getconst(e):
    save_toc(e)
    L = e.cl
    L.rec("GetConstString", (L.lab(e.r(3)), e.r(4) & MASK))
    e.set_r(3, CTX["labaddr"][10])


def st_settitle(e):
    L = e.cl
    L.rec("SetTitleText", (L.lab(e.r(3)), s32(e.r(4)), L.lab(e.r(5))) + tuple(units(e, e.r(5))))


def st_convmulti(e):
    L = e.cl
    b = tuple(e.read(e.r(3), 4))
    L.rec("ConvertMulti", (L.lab(e.r(3)),) + b)
    e.set_r(3, CTX["labaddr"][11])


def st_getwindow(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    label = CTX["answers"].get(e.r(4), 0)
    e.set_r(3, CTX["labaddr"][label] if label else 0)


def st_setframe(e):
    L = e.cl
    L.rec("SetFrame", (L.lab(e.r(3)), s32(e.r(4))))


def st_getclient(e):
    L = e.cl
    L.rec("GetClientRect", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.set_r(3, 0)


def st_timeinpark(e):
    L = e.cl
    L.rec("GetTimeInPark", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("TimeInPark") & MASK)


def st_funny(e):
    L = e.cl
    L.rec("GetFunnyTime", (L.lab(e.r(4)), e.r(5) & MASK))
    e.write(e.r(3), struct.pack(">Q", L.ret("Funny") & 0xFFFFFFFFFFFFFFFF))


def st_div64(e):
    x = (e.r(3) << 32) | e.r(4)
    d = (e.r(5) << 32) | e.r(6)
    if x >= 1 << 63:
        x -= 1 << 64
    q = int(x / d) if abs(x) < (1 << 52) else (x // d if x >= 0 else -((-x) // d))
    q &= 0xFFFFFFFFFFFFFFFF
    e.set_r(3, q >> 32)
    e.set_r(4, q & MASK)


def st_uiname(e):
    L = e.cl
    L.rec("GetUIName", (L.lab(e.r(3)), e.r(5) & MASK))
    e.write(e.r(4), struct.pack(">4H", *mk_text(L.ret("UIName"))))


def st_assert(e):
    L = e.cl
    text = e.cstr(e.r(4))
    data = text.encode("latin1") if isinstance(text, str) else bytes(text)
    L.rec("GAME_ASSERTMSG", (s32(e.r(3)), sum((i + 1) * b for i, b in enumerate(data)) & MASK))


def st_rectctor(e):
    e.cl.rec("RectCtor", (e.cl.lab(e.r(3)),))


def st_rectdtor(e):
    e.cl.rec("RectDtor", (e.cl.lab(e.r(3)), s32(e.r(4))))


def st_showsprite(e):
    L = e.cl
    L.rec("ShowSpritePreview", (L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5)), fb(e.f(1))))


for _a, _f in ((A_PTR_GLUE, st_ptr_glue), (A_FORMAT, st_format), (A_SETTITLE, st_settitle), (A_GETWINDOW, st_getwindow),
               (A_TIMEINPARK, st_timeinpark), (A_FUNNY, st_funny), (A_DIV64, st_div64), (A_ASSERT, st_assert),
               (A_UINAME, st_uiname), (A_SETFRAME, st_setframe), (A_GETCONST, st_getconst), (A_CONVMULTI, st_convmulti),
               (A_RECTCTOR, st_rectctor), (A_RECTDTOR, st_rectdtor), (A_GETCLIENT, st_getclient), (A_SHOWSPRITE, st_showsprite)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_halves(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def block(self, n):
        a = self.alloc(n)
        self.write(a, bytes(n))
        return a


class Ppc(Side):
    fmt = ">I"
    mz_off = 0xDC
    stride = 0x14

    def __init__(self, e, case):
        self.e = e
        self.saved = None
        self.vt = None

    def start(self, case):
        e = self.e
        fixed = {a: l for l, a in self.objs.items()}
        fixed[self.g("buf")] = 12
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        CTX["labaddr"] = self.objs
        CTX["answers"] = {i: (20 + i - 0x3724 if case["ans"][i] else 0) for i in IDS}
        self.saved = STUBS.activate(e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def wptr(self, a, v):
        self.words(a, [v])

    def p32(self, a):
        return a

    def init_window(self, w):
        if self.vt is None:
            self.vt = self.block(0x40)
            self.words(self.vt + 0x24, [MARK_VALUE])
            self.words(self.vt + 0x2C, [MARK_TEXT])
        self.words(w + 0x118, [self.vt])

    def slot_table(self, table):
        self.words(table + 0x34, [MARK_SLOT])

    def log(self):
        return self.L.log

    def call(self, op, panel):
        T.call_ppc(self.e, FN[op], [panel])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.mz_off, self.stride = HOSTL["mz"], HOSTL["stride"]

    def start(self, case):
        for label, a in self.objs.items():
            self.write(self.g("labaddr") + 8 * label, struct.pack("<Q", a))
        for i in IDS:
            self.words(self.g("answers") + 4 * (i - 0x3724), [20 + i - 0x3724 if case["ans"][i] else 0])
        CL.host_begin(self.lib, dict((l, a) for l, a in self.objs.items()) | {12: self.g("buf")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_vsp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_vsp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def wptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def p32(self, a):
        return self.lib.tpw_vsp_off(a)

    def init_window(self, w):
        pass

    def slot_table(self, table):
        blk = self.block(16)
        self.write(blk, struct.pack("<Q", self.lib.tpw_vsp_fn34()))
        self.words(table + 0x34, [self.p32(blk)])

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, panel):
        getattr(self.lib, "tpw_vsp_" + op)(panel)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_vsp_"
    lib.tpw_vsp_addr.restype, lib.tpw_vsp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_vsp_alloc.restype, lib.tpw_vsp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_vsp_off.restype, lib.tpw_vsp_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_vsp_fn34.restype,lib.tpw_vsp_fn34.argtypes = C.c_uint64, []
    lib.tpw_vsp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_vsp_status.argtypes = [C.c_uint64]
    lib.tpw_vsp_view.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_vsp_layout(out)
    got = list(out)[:n]
    assert got[2] == 0x2A0 and got[3] >= 0x2800, got
    HOSTL["mz"], HOSTL["stride"] = got[0], got[1]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o = s.objs = {}
    panel = o[1] = s.block(0x40)
    w0c = o[2] = s.block(0x200)
    w10 = o[3] = s.block(0x200)
    w14 = o[4] = s.block(0x200)
    thing = o[5] = s.block(0x240)
    world = s.block(0x300)
    o[6] = world + 0x2A0
    o[7], o[8], o[9] = s.block(0x10), s.block(0x10), s.block(0x10)
    o[10] = s.block(0x20)
    o[11] = s.block(0x20)
    o[13] = s.block(0x20)
    for k in range(18):
        o[20 + k] = s.block(0x200)
    for l in [2, 3, 4] + list(range(20, 38)):
        s.init_window(o[l])
    s.write(o[10], bytes(case["cs"]))
    ptable = s.block(0x80)
    s.slot_table(ptable)
    s.words(panel + 0x0C, [s.p32(w0c)])
    s.words(panel + 0x10, [s.p32(w10) if case["w10"] else 0])
    s.words(panel + 0x14, [s.p32(w14)])
    s.words(panel + 0x20, [s.p32(ptable)])
    s.words(panel + 0x24, [case["p24"]])
    s.half(w14 + s.mz_off, case["z"])
    # the visitor
    t = case["t"]
    for off in (0x1A0, 0x1C4, 0x1C8, 0x1CC, 0x1D0, 0x1D8, 0x220, 0x30, 0x19C):
        s.words(thing + off, [t[off]])
    s.half(thing + 0x1DC, case["idx"])
    s.wptr(s.g("thing"), thing if case["thing"] else 0)
    s.wptr(s.g("world"), world)
    s.wptr(s.g("res6"), o[7])
    s.wptr(s.g("res8"), o[8])
    s.wptr(s.g("res9"), o[9])
    s.wptr(s.g("tarr"), o[13] if case["e0"] else 0)
    if case["idx"] != 0:
        s.wptr(s.g("tarr") + case["idx"] * s.stride, o[13])


def execute(s, case):
    return s.call(case["op"], s.objs[1])


def observe(s, case, answer):
    w = s.rd_words(s.objs[1], 10)      # the pointer words (0xc to 0x20) are addresses and never written
    return {"log": s.log(), "panel": w[:3] + [w[9]], "buf": s.rd_halves(s.g("buf"), 4)}


def run(s, case):
    try:
        build(s, case)
        s.start(case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

HP = [0.0, -0.0, 0.5, -0.5, 1.0, 99.9999, 100.0, 255.0, 255.9, 256.0, 257.5, -1.0, -1.5, -255.5, 1000.25, 123456.7, -2e9, 2e9]


def rhp(r):
    if r.random() < 0.4:
        return fb(r.choice(HP))
    return fb(r.uniform(-400, 400))


def base(r, op):
    state = r.choice([0x10, 0x10, 0, 1, 0x1C7, 0x1C9, 0x2A, r.randrange(0, 0x1000)])
    if state == 0x1C8:
        state = 0x1C7
    ans = {i: r.random() > 0.2 for i in IDS}
    for i in (0x3733, 0x3734, 0x3735):
        ans[i] = True
    ticks = r.choice([0, 1, 36000000000 - 1, 36000000000, 36000000000 + 1, r.randrange(0, 1 << 50), 36000000000 * r.randrange(0, 5000)])
    t = {0x1A0: r.getrandbits(32), 0x1C4: r.getrandbits(32), 0x1C8: r.getrandbits(32), 0x1CC: r.getrandbits(32),
         0x1D0: r.getrandbits(32), 0x1D8: r.choice([0, r.getrandbits(32)]), 0x220: state, 0x30: r.choice([0, 1, 2, r.randrange(0, 300), r.getrandbits(32)]),
         0x19C: rhp(r)}
    return {"op": op, "thing": r.random() > 0.1, "p24": r.choice([0, 0, r.getrandbits(32)]), "w10": r.random() > 0.2, "ans": ans,
            "cs": [r.randrange(256) for _ in range(4)], "z": r.choice([0, 1, -1, -3, 0x7FFE, 0x7FFF, -0x8000, r.randrange(-0x8000, 0x8000)]),
            "idx": r.choice([0, 1, 5, 0x27FF, r.randrange(0, 0x2800)]), "e0": r.choice([0, 1]), "t": t,
            "queues": {"TimeInPark": [r.randrange(0, 1 << 20), r.randrange(0, 1 << 29)], "Funny": [ticks],
                       "FormatString": [r.getrandbits(32) for _ in range(3)], "UIName": [r.getrandbits(32)]}}


def status_case(r, **over):
    c = base(r, "status")
    for k, v in over.items():
        if k in ("state", "f1d8", "f30"):
            c["t"][{"state": 0x220, "f1d8": 0x1D8, "f30": 0x30}[k]] = v
        elif k.startswith("ans"):
            c["ans"][int(k[3:], 16)] = v
        else:
            c[k] = v
    return c


def status_edges():
    r = random.Random(51022)
    yield status_case(r, thing=False)
    yield status_case(r, thing=False, w10=False)
    for p24, f1d8 in ((0, 0), (0, 7), (5, 7), (5, 0)):
        yield status_case(r, p24=p24, f1d8=f1d8)
    yield status_case(r, w10=False)
    yield status_case(r, w10=False, p24=3)
    for st in (0x10, 0x0, 0x11, 0x1C7):
        for idx in (0, 1, 0x27FF):
            for e0 in (0, 1):
                yield status_case(r, state=st, idx=idx, e0=e0)
    for f30 in (0, 1, 2, 0xFFFFFFFF):
        yield status_case(r, f30=f30)
    for i in IDS[:15]:
        yield status_case(r, **{"ans%x" % i: False})
    for i in IDS[:15]:
        yield status_case(r, **{"ans%x" % i: True})
    for k in range(len(HP)):
        c = status_case(r, **{"ans3732": True})
        c["t"][0x19C] = fb(HP[k])
        yield c


def status_random(r):
    return status_case(r)


def view_case(r, **over):
    c = base(r, "view")
    c.update(over)
    return c


def view_edges():
    r = random.Random(51023)
    yield view_case(r, thing=False)
    for z in (0, 1, -1, -2, -3, 0x7FFF, -0x8000, 0x7FFE):
        yield view_case(r, z=z)


def view_random(r):
    return view_case(r)


TESTS = [
    FnTest("VisitorStatusPanel::UpdateStatusWindow", ppc, host, edge=status_edges, rnd=status_random),
    FnTest("VisitorStatusPanel::UpdateViewWindow", ppc, host, edge=view_edges, rnd=view_random),
]
