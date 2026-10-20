"""Unit v5184, the research panel's speed lights and bars, against libtpw_unit_SpeedTimerFudge.dylib (tests/unit_SpeedTimerFudge.cpp), main app
("SimTheme Park"), src/SpeedTimerFudge.cpp:
  SpeedTimerFudge 0x101b35d4, PercentComplete 0x101b3398, ResearchSpeed 0x101b381c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (getExtraParam, setExtraParam, GetWindow, SetFrame, IveGotTicks, GetResearchLab, GetCurrentProject, GetRemainingPoints,
GetResearchSpeed, SOUND_SetParamValue and the virtual SetValue through __ptr_glue) are fakes on both sides that record their arguments and
answer from the case's queues (keyed by window label or category).

The world of a case: the panel, world and sound globals, the "none" project id, four fake windows (labels 11 to 14), the lab, and the answer
queues. What is compared: the log of every call. SpeedTimerFudge, ResearchSpeed's calls into it and the sound call are real code on the
PowerPC side. No fused operations. Not tested: a NULL second window in PercentComplete (the original crashes on it).
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SpeedTimerFudge"
MASK = 0xFFFFFFFF

FN_STF, FN_PC, FN_RS = 0x101B35D4, 0x101B3398, 0x101B381C
SIZES = {FN_STF: 528, FN_PC: 532, FN_RS: 1384}
OWN = set(SIZES)

ADDR = dict(panel=0x10395980, world=0x1037EBFC, sound=0x10395968, none=0x102ACA98)
HOST_LAYOUT = [2, 0]

IDS = (0xD2D01, 0xD2D05, 0xD2D06, 0xD2D07, 0xD2D08, 0xD2D09, 0xD2D0A)
CATS = range(-3, 9)
WLABELS = (11, 12, 13, 14)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def reg(addr, fn):
    STUBS.internal(addr, fn)


def st_getx(e):
    L = e.cl
    lab = L.lab(e.r(3))
    L.rec("getExtraParam", (lab, T.s16(e.r(4))))
    e.set_r(3, L.ret("getExtraParam:%d" % lab) & MASK)


def st_setx(e):
    L = e.cl
    L.rec("setExtraParam", (L.lab(e.r(3)), T.s16(e.r(4)), e.r(5) & MASK))
    e.set_r(3, 0)


def st_getwin(e):
    L = e.cl
    ident = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), ident))
    label = L.ret("GetWindow:%d" % ident)
    e.set_r(3, L.objs.get(label, 0))


def st_setframe(e):
    L = e.cl
    L.rec("SetFrame", (L.lab(e.r(3)), s32(e.r(4))))


def st_ticks(e):
    L = e.cl
    L.rec("IveGotTicks", ())
    e.set_r(3, L.ret("IveGotTicks") & MASK)


def st_getlab(e):
    L = e.cl
    L.rec("GetResearchLab", (L.lab(e.r(3)),))
    e.set_r(3, L.objs[3])


def st_curproj(e):
    L = e.cl
    cat = s32(e.r(5))
    L.rec("GetCurrentProject", (L.lab(e.r(4)), cat))
    e.write(e.r(3), struct.pack(">H", L.ret("GetCurrentProject:%d" % cat) & 0xFFFF))


def st_rempts(e):
    L = e.cl
    cat = s32(e.r(4))
    L.rec("GetRemainingPoints", (L.lab(e.r(3)), cat))
    e.set_r(3, L.ret("GetRemainingPoints:%d" % cat) & MASK)


def st_rspeed(e):
    L = e.cl
    cat = s32(e.r(4))
    L.rec("GetResearchSpeed", (L.lab(e.r(3)), cat))
    e.set_r(3, L.ret("GetResearchSpeed:%d" % cat) & MASK)


def st_sound(e):
    L = e.cl
    L.rec("SetParamValue", (s32(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))
    e.set_r(3, 0)


def st_glue(e):
    L = e.cl
    L.rec("SetValue", (L.lab(e.r(3)), s32(e.r(4))))


reg(0x101DD030, st_getx)
reg(0x101DCFD0, st_setx)
reg(0x101DBE40, st_getwin)
reg(0x101DB7DC, st_setframe)
reg(0x101CAC3C, st_ticks)
reg(0x1013ECE8, st_getlab)
reg(0x10122AF4, st_curproj)
reg(0x10122C94, st_rempts)
reg(0x10122A04, st_rspeed)
reg(0x100DD380, st_sound)
reg(0x1023BAA8, st_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, v):
        """A pointer global: 4 bytes on the Mac, 8 on the host (the host arena is not 32-bit addressable)."""
        self.write(a, struct.pack(self.fmt[0] + ("I" if self.fmt == ">I" else "Q"), v))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def make_objects(self):
        self.objs = {1: self.alloc(16), 2: self.alloc(16), 3: self.alloc(16)}
        for k in WLABELS:
            self.objs[k] = self.win()


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make_objects()
        self.L = e.cl = CL.PpcLog({a: k for k, a in self.objs.items()}, case["queues"])
        self.L.objs = self.objs
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def win(self):
        vt = self.e.alloc(0x40)
        w = self.e.alloc(0x120)
        self.e.write(w + 0x118, struct.pack(">I", vt))
        return w

    def write(self, a, data):
        self.e.write(a, data)

    def read(self, a, n):
        return self.e.read(a, n)

    def log(self):
        return self.L.log

    def call(self, op, args):
        entry = {"stf": FN_STF, "pc": FN_PC, "rs": FN_RS}[op]
        if op == "stf":
            args = [self.objs[args[0]], args[1] & MASK]
        else:
            args = [args[0] & MASK]
        T.call_ppc(self.e, entry, args)
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_objects()
        for k, a in self.objs.items():
            lib.tpw_stf_setlab(k, a)
        CL.host_begin(lib, dict(self.objs), case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_stf_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_stf_alloc(n)

    def win(self):
        return self.lib.tpw_stf_win()

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def read(self, a, n):
        return C.string_at(a, n)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        if op == "stf":
            self.lib.tpw_stf_speedtimerfudge(self.objs[args[0]], s32(args[1]))
        elif op == "pc":
            self.lib.tpw_stf_percentcomplete(s32(args[0]))
        else:
            self.lib.tpw_stf_researchspeed(s32(args[0]))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_stf_addr.restype, lib.tpw_stf_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_stf_alloc.restype, lib.tpw_stf_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_stf_win.restype, lib.tpw_stf_win.argtypes = C.c_uint64, []
    lib.tpw_stf_setlab.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_stf_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_stf_speedtimerfudge.argtypes = [C.c_uint64, C.c_int32]
    lib.tpw_stf_percentcomplete.argtypes = [C.c_int32]
    lib.tpw_stf_researchspeed.argtypes = [C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_stf_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.ptr(s.g("panel"), s.objs[1])
    s.ptr(s.g("world"), s.objs[2])
    s.words(s.g("sound"), [case["sound"]])
    s.half(s.g("none"), case["none"])


def run(s, case):
    try:
        build(s, case)
        ans = s.call(case["op"], case["args"])
        return {"answer": ans, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

PROJ_EDGE = (0, 1, 0xFFFF)
SPEEDS = (0, 1, 5, 20, 100, 1000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFFEC)
POINTS = (0, 1, 50, 99, 100, 101, 200, 1000, 0xFFFFFFFF, 0x80000000)


def rspeed(r):
    k = r.random()
    if k < 0.4:
        return r.choice(SPEEDS)
    if k < 0.8:
        return r.randrange(0, 300)
    return r.randrange(0, 1 << 32)


def rpoints(r):
    k = r.random()
    if k < 0.6:
        return r.randrange(0, 101)
    return r.choice(POINTS)


def base(r, op, args):
    none = r.choice([0, 0, 1, 0x1234, 0xFFFF])
    q = {"IveGotTicks": [r.randrange(0, 1 << 32), r.randrange(0, 1 << 32)]}
    for i in IDS:
        q["GetWindow:%d" % i] = [r.choice(WLABELS) for _ in range(3)]
    for lab in (-1, 0) + WLABELS:
        q["getExtraParam:%d" % lab] = [r.choice([0, 1, 2, 3, 4, -1, 5, 0x10000]) for _ in range(3)]
    for c in CATS:
        q["GetCurrentProject:%d" % c] = [r.choice([none, none, none ^ 1, r.randrange(0, 1 << 16)]) for _ in range(4)]
        q["GetResearchSpeed:%d" % c] = [rspeed(r) for _ in range(4)]
        q["GetRemainingPoints:%d" % c] = [rpoints(r) for _ in range(3)]
    return {"op": op, "args": args, "none": none, "sound": r.randrange(0, 1 << 32), "queues": q}


import random


def stf_case(r, label=None, speed=None, state=None):
    c = base(r, "stf", [r.choice(WLABELS) if label is None else label,
                        (r.choice([0, 1, -1, -20, -19, 5, 80, 0x7FFFFFFF, 0x8000, 100]) if r.random() < 0.7 else r.randrange(-300, 300))
                        if speed is None else speed])
    if state is not None:
        c["queues"]["getExtraParam:%d" % c["args"][0]] = [state]
    return c


def stf_edges():
    r = random.Random(5184)
    for speed in (0, 1, -1, -20, -19, 100, 0x7FFFFFFF, 0x80000000 - (1 << 32), 0x3FFF, 0x4000, 0x8000):
        for state in (0, 1, 2, 3, 4, -1):
            yield stf_case(r, 11, speed, state)


def stf_random(r):
    return stf_case(r)


def pc_case(r, cat=None, points=None):
    c = base(r, "pc", [r.choice([0, 1, 2, 3, 4, 5, -1, 7, -2]) if cat is None else cat])
    if points is not None:
        c["queues"]["GetRemainingPoints:%d" % c["args"][0]] = [points]
    return c


def pc_edges():
    r = random.Random(5185)
    for cat in (-2, -1, 0, 1, 2, 3, 4, 5, 7):
        for points in (0, 1, 50, 99, 100, 101, 0xFFFFFFFF, 0x80000000):
            yield pc_case(r, cat, points)


def pc_random(r):
    return pc_case(r)


def rs_case(r, cat=None, mode=None):
    c = base(r, "rs", [r.choice([-1, -1, 0, 1, 2, 3, 4, 5, 7, -2]) if cat is None else cat])
    if mode == "none":
        for k in c["queues"]:
            if k.startswith("GetCurrentProject"):
                c["queues"][k] = [c["none"]]
    elif mode == "all":
        for k in c["queues"]:
            if k.startswith("GetCurrentProject"):
                c["queues"][k] = [c["none"] ^ 1]
    return c


def rs_edges():
    r = random.Random(5186)
    for cat in (-2, -1, 0, 1, 2, 3, 4, 5, 7):
        for mode in ("none", "all", "all", None):
            yield rs_case(r, cat, mode)


def rs_random(r):
    return rs_case(r, mode=r.choice([None, "all", "all", "none"]))


TESTS = [
    FnTest("SpeedTimerFudge", ppc, host, edge=stf_edges, rnd=stf_random),
    FnTest("PercentComplete", ppc, host, edge=pc_edges, rnd=pc_random),
    FnTest("ResearchSpeed", ppc, host, edge=rs_edges, rnd=rs_random),
]
