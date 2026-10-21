"""LLCR_StartQueue and LLCR_StartTrack, against libtpw_unit_LLCR_StartQueue.dylib (tests/unit_LLCR_StartQueue.cpp), main app
("SimTheme Park"), src/game/park/LLCellRide.cpp:
  LLCR_StartQueue 0x1008c128, LLCR_StartTrack 0x1008c320

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (CMap::IsXYValid,
OPPOSITE, CCellBase::ModifyNeighbours, CMapCell::ValidTypeChange, CCellBase::ModifyDirection, LLCC_DrawTrackLine) are fakes on both sides
that record their arguments (cell pointers as the offset from the world) and answer from the case. LLCC_DrawTrackLine also adds 1 to the
two integers its last arguments point to, so the shared locals are checked.

The world of a case: a block standing for the CWorld (cells and track cells), the placement record, the rotation global, sentinels in the
written globals. What is compared: the answer (as an offset from the world), the out parameters, the globals written and the event log.

No fused operations. Not tested: coordinates that make the 16 bit cell id wrap outside the world block; LLCR_StartTrack with a track kind
other than 1 or 2 (the first mode is then the placement's address, which differs between the two memories); a rotation other than 0, 0x5a,
0xb4, 0x10e in LLCR_StartTrack (the Mac uses unset callee-saved registers).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCR_StartQueue"
MASK = 0xFFFFFFFF

FN_Q, FN_T = 0x1008C128, 0x1008C320
SIZES = {FN_Q: 452, FN_T: 716}
OWN = set(SIZES)

IS_XY_VALID = 0x101012F4
OPPOSITE = 0x10102500
MOD_NEIGH = 0x10081608
VALID_TYPE = 0x1009B054
MOD_DIR = 0x10081810
DRAW_LINE = 0x100A30F8

ADDR = dict(world=0x1037EBFC, firstx=0x1029E02C, firsty=0x1029E030, db44=0x102DDB44, e0cc=0x102DE0CC, rot=0x102DE0D0)
HOST_LAYOUT = [0x2D8, 0x110000]
WORLD_SIZE = 0x2D8 + 0x110000 + 0xA0000 + 0x100


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def off(e, v):
    return s32(v - e.u32(ADDR["world"]))


def st_is_xy(e):
    e.cl.rec("IsXYValid", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, e.cl.ret("IsXYValid") & MASK)


def st_opposite(e):
    d = e.r(3) & 0xFF
    e.cl.rec("OPPOSITE", (d,))
    e.set_r(3, e.cl.ret("OPPOSITE") & 0xFF)


def st_neigh(e):
    e.cl.rec("ModifyNeighbours", (off(e, e.r(3)), e.r(4) & 0xFF))


def st_valid(e):
    e.cl.rec("ValidTypeChange", (off(e, e.r(3)), s32(e.r(4))))
    e.set_r(3, e.cl.ret("ValidTypeChange") & MASK)


def st_dir(e):
    e.cl.rec("ModifyDirection", (off(e, e.r(3)), e.r(4) & 0xFF))


def st_draw(e):
    a, b = e.r(6), e.r(7)
    va, vb = e.u32(a), e.u32(b)
    e.cl.rec("DrawTrackLine", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(va), s32(vb)))
    e.write(a, struct.pack(">I", (va + 1) & MASK))
    e.write(b, struct.pack(">I", (vb + 1) & MASK))


STUBS.internal(IS_XY_VALID, st_is_xy)
STUBS.internal(OPPOSITE, st_opposite)
STUBS.internal(MOD_NEIGH, st_neigh)
STUBS.internal(VALID_TYPE, st_valid)
STUBS.internal(MOD_DIR, st_dir)
STUBS.internal(DRAW_LINE, st_draw)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, case["queues"])
        self.L = e.cl
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

    def call_q(self, prr, ox, oy):
        r3, _, _ = T.call_ppc(self.e, FN_Q, [prr, ox, oy])
        return r3 & MASK

    def call_t(self, prr, ox, oy, track):
        r3, _, _ = T.call_ppc(self.e, FN_T, [prr, ox, oy, track & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_llq_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_llq_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_q(self, prr, ox, oy):
        return self.lib.tpw_llq_q(prr, ox, oy)

    def call_t(self, prr, ox, oy, track):
        return self.lib.tpw_llq_t(prr, ox, oy, s32(track))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_llq_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "q").restype, getattr(lib, p + "q").argtypes = C.c_uint64, [C.c_uint64] * 3
    getattr(lib, p + "t").restype, getattr(lib, p + "t").argtypes = C.c_uint64, [C.c_uint64] * 3 + [C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.wbase = s.alloc(WORLD_SIZE)
    s.words(s.g("world"), [s.wbase])
    s.words(s.g("world"), [s.wbase])
    if s.fmt == "<I":
        s.words(s.g("world") + 4, [s.wbase >> 32])      # the host pointer is 64 bits (arena base + offset)
    s.prr = s.alloc(0x20)
    s.words(s.prr, [case["px"], case["py"], case["tx"], case["ty"]])
    s.write(s.prr + 0x10, bytes([case["dir"]]))
    s.ox = s.alloc(8)
    s.oy = s.alloc(8)
    s.words(s.ox, [0x1234])
    s.words(s.oy, [0x5678])
    s.words(s.g("rot"), [case["rot"]])
    s.words(s.g("firstx"), [0xFFFFFFFF])
    s.words(s.g("firsty"), [0xFFFFFFFF])
    s.words(s.g("db44"), [0x55])
    s.write(s.g("e0cc"), bytes([0x77]))


def execute(s, case):
    if case["op"] == "q":
        return s.call_q(s.prr, s.ox, s.oy)
    return s.call_t(s.prr, s.ox, s.oy, case["track"])


def rel(s, v, low=False):
    base = s.wbase & MASK if low else s.wbase       # DB44 holds the low 32 bits of the host address
    if v == 0:
        return 0
    if base <= v < base + WORLD_SIZE:
        return v - base
    return v


def observe(s, case, answer):
    return {"answer": rel(s, answer), "log": s.log(), "ox": s.rd_word(s.ox), "oy": s.rd_word(s.oy),
            "firstx": s.rd_word(s.g("firstx")), "firsty": s.rd_word(s.g("firsty")),
            "db44": rel(s, s.rd_word(s.g("db44")), True), "e0cc": s.read(s.g("e0cc"), 1)}


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


def valid_q(r, n=2):
    return [r.choice([0, 1, 1, 1]) for _ in range(n)]


def make(r, op, px, py, direction, rot, track, valid):
    return {"op": op, "px": px & MASK, "py": py & MASK, "tx": px & MASK, "ty": py & MASK, "dir": direction, "rot": rot,
            "track": track,
            "queues": {"IsXYValid": valid, "OPPOSITE": [r.randrange(256)], "ValidTypeChange": [r.choice([0, 1])]}}


DIRS = [1, 4, 0x10, 0x40, 0, 3, 0x80]
ROTS = [0, 0x5A, 0xB4, 0x10E]


def q_case(r, px=None, py=None, d=None, valid=None):
    px = r.randrange(1, 126) if px is None else px
    py = r.randrange(1, 126) if py is None else py
    return make(r, "q", px, py, r.choice(DIRS) if d is None else d, 0, 0, valid_q(r) if valid is None else valid)


def q_edges():
    r = random.Random(5998)
    for d in DIRS:
        for valid in ([1, 1], [0], [1, 0]):
            for vt in (0, 1):
                c = q_case(r, d=d, valid=valid)
                c["queues"]["ValidTypeChange"] = [vt]
                yield c
    for px, py, ds in ((0, 0, (1, 0x40)), (127, 127, (4, 0x10)), (0, 127, (0x10, 0x40)), (127, 0, (1, 4))):
        for d in ds:
            yield q_case(r, px, py, d, [1, 1])


def q_random(r):
    return q_case(r)


def t_case(r, rot=None, track=None, valid=None, px=None, py=None):
    px = r.randrange(6, 120) if px is None else px
    py = r.randrange(6, 120) if py is None else py
    return make(r, "t", px, py, 0, r.choice(ROTS) if rot is None else rot, r.choice([1, 2]) if track is None else track,
                valid_q(r) if valid is None else valid)


def t_edges():
    r = random.Random(5999)
    for rot in ROTS:
        for track in (1, 2):
            for valid in ([1, 1], [0, 1], [1, 0], [0, 0]):
                yield t_case(r, rot, track, valid)
    for px, py in ((6, 6), (120, 120), (125, 5), (5, 125)):
        yield t_case(r, 0, 1, [1, 1], px, py)
        yield t_case(r, 0xB4, 2, [1, 1], px, py)


def t_random(r):
    return t_case(r)


TESTS = [
    FnTest("LLCR_StartQueue", ppc, host, edge=q_edges, rnd=q_random),
    FnTest("LLCR_StartTrack", ppc, host, edge=t_edges, rnd=t_random),
]
