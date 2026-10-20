"""Unit v5193, UpdateSatellite and UI2DMReturnScaleColour, against libtpw_unit_UpdateSatellite.dylib (tests/unit_UpdateSatellite.cpp), main app
("SimTheme Park"), src/UpdateSatellite.cpp:
  UpdateSatellite 0x1022b8d0, UI2DMReturnScaleColour 0x1022bb44

A case is a world and a call. The world is built twice by ONE piece of Python code (PowerPC memory big-endian, host little-endian). The callees
outside the unit are fakes on both sides: InterfaceWindow::GetWindow (answers the window of its id; logs this and id) and the virtual call through
__ptr_glue (the PowerPC stub logs the window and the argument and checks r12 is the vector the vtable holds; the host fake is SetValue).

The world of UpdateSatellite: the parent global (0 or a window) and six windows whose word at +0x118 points to a vtable holding a vector at +0x24.
The world of the colour function: three colours (36 bytes) and an output block with a sentinel; sometimes the output is one of the inputs.
Compared: the event log, the global, the output block and the three inputs. No fused operations. Not tested: nothing excluded (the original
dereferences the GetWindow answers unchecked; the fakes always answer windows).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UpdateSatellite"
MASK = 0xFFFFFFFF

FN_SAT, FN_COL = 0x1022B8D0, 0x1022BB44
SIZES = {FN_SAT: 312, FN_COL: 800}
OWN = set(SIZES)

GET_WINDOW = 0x101DBE40
PTR_GLUE = 0x1023BAA8
VEC = 0x10300040

ADDR = dict(gSat=0x10396FEC)
HOST_LAYOUT = [36, 4]
IDS = [0x16007 + k for k in range(6)]

STUBS = CL.Stubs()
STUBS.real.update(OWN)
WINS = {}


def s32(v):
    return T.s32(v)


def stub_get_window(e):
    L = e.cl
    ident = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), ident))
    e.set_r(3, WINS.get(ident, 0))


def stub_glue(e):
    L = e.cl
    if e.r(12) != VEC:
        L.rec("BADVEC", (e.r(12),))
    L.rec("SetValue", (L.lab(e.r(3)), s32(e.r(4))))


STUBS.internal(GET_WINDOW, stub_get_window)
STUBS.internal(PTR_GLUE, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def wr_ptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_words(a, 1)[0]

    def __init__(self, e, case):
        self.e = e
        self.root = e.alloc(0x138)
        vt = e.alloc(0x40)
        e.write(vt + 0x24, struct.pack(">I", VEC))
        self.win = []
        for k in range(6):
            w = e.alloc(0x140)
            e.write(w + 0x118, struct.pack(">I", vt))
            self.win.append(w)
        WINS.clear()
        WINS.update({IDS[k]: self.win[k] for k in range(6)})
        fixed = {self.root: 1}
        fixed.update({self.win[k]: 2 + k for k in range(6)})
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

    def update(self, which, value):
        T.call_ppc(self.e, FN_SAT, [which & MASK, value & MASK])

    def colour(self, out, pct, a, b, c):
        T.call_ppc(self.e, FN_COL, [out, pct & MASK, a, b, c])


class Host(Side):
    fmt = "<I"

    def wr_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.root = lib.tpw_us_alloc(0x138)
        fixed = {1: self.root}
        for k in range(6):
            fixed[2 + k] = lib.tpw_us_addr(b"w%d" % k)
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_us_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_us_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def update(self, which, value):
        self.lib.tpw_us_update(s32(which), s32(value))

    def colour(self, out, pct, a, b, c):
        self.lib.tpw_us_colour(out, s32(pct), a, b, c)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_us_addr.restype, lib.tpw_us_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_us_alloc.restype, lib.tpw_us_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_us_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_us_update.restype, lib.tpw_us_update.argtypes = None, [C.c_int32, C.c_int32]
    lib.tpw_us_colour.restype, lib.tpw_us_colour.argtypes = None, [C.c_uint64, C.c_int32, C.c_uint64, C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_us_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build_sat(s, case):
    s.wr_ptr(s.g("gSat"), s.root if case["root"] else 0)


def run_sat(s, case):
    try:
        build_sat(s, case)
        s.update(*case["args"])
        return {"log": s.log(), "gSat": s.rd_ptr(s.g("gSat")) == (s.root if case["root"] else 0)}
    finally:
        s.done()


def build_col(s, case):
    blocks = [s.alloc(36) for _ in range(4)]
    for k in range(3):
        s.write(blocks[k], bytes(case["col"][k]))
    s.write(blocks[3], bytes(case["sentinel"]))
    return blocks


def run_col(s, case):
    try:
        blocks = build_col(s, case)
        out = blocks[case["alias"]] if case["alias"] >= 0 else blocks[3]
        s.colour(out, case["pct"], blocks[0], blocks[1], blocks[2])
        return {"log": s.log(), "blocks": [s.read(b, 36) for b in blocks]}
    finally:
        s.done()


def ppc_sat(e, case):
    return run_sat(Ppc(e, case), case)


def host_sat(lib, case):
    return run_sat(Host(lib, case), case)


def ppc_col(e, case):
    return run_col(Ppc(e, case), case)


def host_col(lib, case):
    return run_col(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases


def sat_case(r, which=None, root=None):
    return {"queues": {}, "root": r.choice([1, 1, 1, 0]) if root is None else root,
            "args": [r.choice([0, 1, 2, 3, 4, 5, -1, 6, r.randrange(-100, 100)]) if which is None else which, r.getrandbits(32)]}


def sat_edges():
    r = random.Random(5193)
    for w in (-3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 100, 0x7FFFFFFF, -0x80000000):
        yield sat_case(r, w, 1)
    for w in (0, 2, 4, -1):
        yield sat_case(r, w, 0)


def sat_random(r):
    return sat_case(r)


def colour_bytes(r, mode=None):
    k = r.random() if mode is None else mode
    if k < 0.15:
        head = [r.choice([0, 255]) for _ in range(4)]
    else:
        head = [r.randrange(256) for _ in range(4)]
    return head + [b for _ in range(8) for b in struct.pack(">I", r.getrandbits(32))]


def col_case(r, pct=None):
    return {"queues": {}, "pct": r.choice([-1000, -1, 0, 1, 25, 49, 50, 51, 75, 99, 100, 101, 1000, r.randrange(-5, 106)]) if pct is None else pct,
            "col": [colour_bytes(r) for _ in range(3)], "sentinel": [r.randrange(256) for _ in range(36)],
            "alias": r.choice([-1, -1, -1, 0, 1, 2])}


def col_edges():
    r = random.Random(5194)
    for pct in list(range(-2, 4)) + list(range(46, 54)) + list(range(96, 104)) + [0x7FFFFFFF, -0x80000000, 10, 20, 30, 40, 60, 70, 80, 90]:
        for _ in range(2):
            yield col_case(r, pct)
    for pct in (0, 25, 49, 50, 75, 100):
        c = col_case(r, pct)
        c["col"] = [[0, 0, 0, 0] + [0] * 32, [255, 255, 255, 255] + [1] * 32, [0, 255, 0, 7] + [2] * 32]
        yield c
        c = col_case(r, pct)
        c["col"] = [[255, 0, 255, 0] + [0] * 32, [0, 255, 0, 255] + [1] * 32, [255, 0, 255, 7] + [2] * 32]
        yield c


def col_random(r):
    return col_case(r)


TESTS = [
    FnTest("UpdateSatellite", ppc_sat, host_sat, edge=sat_edges, rnd=sat_random),
    FnTest("UI2DMReturnScaleColour", ppc_col, host_col, edge=col_edges, rnd=col_random),
]
