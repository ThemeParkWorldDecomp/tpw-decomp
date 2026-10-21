"""Unit v5034, eight key actions, against libtpw_unit_turn_object_counterclockwise.dylib (tests/unit_turn_object_counterclockwise.cpp), main app
("SimTheme Park"), src/game/keyboard.cpp:
  turn_object_counterclockwise 0x1014cafc, turn_object_clockwise 0x1014ca74, quickload_layout 0x1014c97c, quicksave_layout 0x1014c89c,
  place_vomit 0x1014ce2c, quickload 0x1014c7bc, quicksave 0x1014c6dc, TestAdvisor 0x1014c454

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory at the Mac's addresses, host memory). The
callees outside the unit (the ride rotation, the string glue, CGame::GetCurrentThemeName, StringToUniString, the two save and load classes,
CTRACE, DEBUG_MESSAGE, ReplayRecording, PlaceLitter, the advisor) are fakes on both sides that record their arguments and answer from the
case's queues. The string assignment fake gives the string the same text pointer (a label) so that the path handed to the loader is compared.

The world of a case: the queues (rotation, the loaders' answers, ADV_Stat), TestAdvisor's index, the pointer's map cell id and the world
pointer. What is compared: the answer (1), the event log, the index global. No fused operations. Not tested: nothing is left out.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_turn_object_counterclockwise"
MASK = 0xFFFFFFFF

FUNCS = {
    "turn_object_counterclockwise": 0x1014cafc,
    "turn_object_clockwise": 0x1014ca74,
    "quickload_layout": 0x1014c97c,
    "quicksave_layout": 0x1014c89c,
    "place_vomit": 0x1014ce2c,
    "quickload": 0x1014c7bc,
    "quicksave": 0x1014c6dc,
    "TestAdvisor": 0x1014c454,
}
SIZES = {0x1014cafc: 92, 0x1014ca74: 92, 0x1014c97c: 208, 0x1014c89c: 184, 0x1014ce2c: 92, 0x1014c7bc: 192, 0x1014c6dc: 192, 0x1014c454: 104}
OWN = set(SIZES)

NAMEBUF = 0x1037f154            # inside gGame: the text pointer the string assignment fake hands out
ADDR = dict(gGame=0x1037f134, namebuf=NAMEBUF, suffix=0x1037f248, saveload=0x1037fefc, recorder=0x1037ec44, loadsave=0x10396a7c,
            idx=0x102a4e54, f24=0x102dc1ac + 0x24, world=0x1037ebfc)
LABELS = {"gGame": 1, "namebuf": 2, "suffix": 3, "saveload": 4, "recorder": 5, "loadsave": 6}
HOST_LAYOUT = []


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def f_assign(e):
    L = e.cl
    this = e.r(3)
    L.rec("StrAssign", (L.lab(this), L.lab(e.r(4))))
    e.write(this + 8, struct.pack(">I", NAMEBUF))
    e.set_r(3, this)


def f_theme(e):
    L = e.cl
    L.rec("GetCurrentThemeName", (L.lab(e.r(3)),))
    e.set_r(3, ADDR["gGame"])


def f_litter(e):
    L = e.cl
    world = e.u32(ADDR["world"])
    L.rec("PlaceLitter", (s32(e.r(3) - world), s32(e.r(4))))


for addr, name, spec, ret in (
    (0x100057a0, "StringToUniString", "LL", None),
    (0x10157fec, "SaveLoadLoad", "LLLi", "i"),
    (0x10157868, "SaveLoadSave", "LLLi", "i"),
    (0x101fc408, "FSLoad", "LL", "i"),
    (0x101fc4f4, "FSSave", "LL", "i"),
    (0x1006fcbc, "CTRACE", "us", None),
    (0x1006fc90, "DEBUG_MESSAGE", "su", None),
    (0x10144e2c, "ReplayRecording", "Li", None),
    (0x10090de0, "LLCC_GetRideRotation", "", "i"),
    (0x10090d40, "LLCC_SetRideRotation", "ib", None),
    (0x100090dc, "ADV_Stat", "", "i"),
    (0x1000887c, "ADV_Call", "i", None),
    (0x10008dd4, "ADV_Slap", "", None),
):
    STUBS.internal(addr, CL.recorder(name, spec, ret))


def glue(fn):
    """A glue function saves the caller's TOC pointer at 0x14(r1); the listing reloads it after the call."""
    def w(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return w


for addr, name, spec in (
    (0x10238458, "StrCtor", "N"),
    (0x10238308, "StrCopyCtor", "NL"),
    (0x102382f0, "StrDtor", "L"),
    (0x10238380, "StrAppend", "LL"),
    (0x10238740, "WStrCtor", "N"),
    (0x10238710, "WStrDtor", "L"),
):
    STUBS.internal(addr, glue(CL.recorder(name, spec)))
STUBS.internal(0x10238470, glue(f_assign))
STUBS.internal(0x1014a058, f_theme)
STUBS.internal(0x10103010, f_litter)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {ADDR[k]: v for k, v in LABELS.items()}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def call(self, name):
        r3, _, _ = T.call_ppc(self.e, FUNCS[name], [])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {v: self.g(k) for k, v in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_qs_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_qs_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, name):
        return getattr(self.lib, "tpw_qs_" + name)() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_qs_addr.restype, lib.tpw_qs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_qs_alloc.restype, lib.tpw_qs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_qs_layout.argtypes = [C.POINTER(C.c_int64)]
    for name in FUNCS:
        f = getattr(lib, "tpw_qs_" + name)
        f.restype, f.argtypes = C.c_uint32, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_qs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    w = s.alloc(16)
    s.words(s.g("world"), [w])
    s.half(s.g("f24"), case["f24"])
    s.words(s.g("idx"), [case["idx"]])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "idx": s.rd_words(s.g("idx"), 1)}


def make_run(name):
    def run(s, case):
        try:
            build(s, case)
            return observe(s, case, s.call(name))
        finally:
            s.done()
    return run


def make_tests(name, queue_fn, seed, edge_vals):
    run = make_run(name)

    def case(r, v=None):
        c = {"queues": queue_fn(r, v), "f24": r.choice([0, 1, 2, 0x4000, 0xFFFF, r.randrange(0, 0x10000)]),
             "idx": r.choice([0, 1, 0x103, 0x264, 0x265, 0x266, -1, r.randrange(0, 0x400), r.randrange(-5, 2000)]) & MASK}
        if name == "place_vomit" and v is not None:
            c["f24"] = v
        return c

    def edges():
        r = random.Random(seed)
        for v in edge_vals:
            yield case(r, v)
        if name == "TestAdvisor":
            for i in (0, 1, 0x264, 0x265, 0x266, -1):
                c = case(r, 0)
                c["idx"] = i & MASK
                yield c

    def rnd(r):
        return case(r)

    return FnTest(name, lambda e, c: run(Ppc(e, c), c), lambda lib, c: run(Host(lib, c), c), edge=edges, rnd=rnd)


def rot_queues(r, v):
    if v is None:
        v = r.choice([r.randrange(-1000, 1000), r.randrange(-2 ** 31, 2 ** 31), 0, 90, 270, 359, 360])
    return {"LLCC_GetRideRotation": [v]}


def load_queues(key):
    def f(r, v):
        return {key: [r.choice([0, 1, 2, -1]) if v is None else v]}
    return f


def adv_queues(r, v):
    return {"ADV_Stat": [r.choice([0, 1, -1, 50]) if v is None else v]}


ROT_EDGES = [0, 1, 89, 90, 269, 270, 359, 360, -1, -90, -270, -360, -1000, 0x7FFFFFFF, -0x80000000, 0x7FFFFEF0, 100000]
BOOL_EDGES = [0, 1, -1, 2]

TESTS = [
    make_tests("turn_object_counterclockwise", rot_queues, 11, ROT_EDGES),
    make_tests("turn_object_clockwise", rot_queues, 12, ROT_EDGES),
    make_tests("quickload_layout", load_queues("SaveLoadLoad"), 13, BOOL_EDGES),
    make_tests("quicksave_layout", load_queues("SaveLoadSave"), 14, BOOL_EDGES),
    make_tests("place_vomit", lambda r, v: {"x": [0]}, 15, [0, 1, 2, 0xFFFF, 0x4000]),
    make_tests("quickload", load_queues("FSLoad"), 16, BOOL_EDGES),
    make_tests("quicksave", load_queues("FSSave"), 17, BOOL_EDGES),
    make_tests("TestAdvisor", adv_queues, 18, [0, 1, -1, 50]),
]
