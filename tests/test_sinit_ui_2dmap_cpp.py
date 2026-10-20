"""Unit v5194, the static initialiser of ui_2dmap.cpp, against libtpw_unit_sinit_ui_2dmap_cpp.dylib (tests/unit_sinit_ui_2dmap_cpp.cpp), main app
("SimTheme Park"), src/sinit_ui_2dmap_cpp.cpp:
  __sinit_ui_2dmap_cpp 0x102319d4

A case is a world and the call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the TbRenderArea and
TbDynamicStringTemplate<char> constructors, __construct_array, __register_global_object) are fakes on both sides that record their arguments;
objects are logged as labels.

The world of a case: random words in the map object at the offsets the function writes (and a few it leaves alone), random bytes in the colour
table, random words in the two counters and the registration nodes. What is compared: those words and bytes after the call, and the event log.
The map's first word is the address of the vtable slot global (a different number on the host), compared as "is that address".

No fused operations. The 16 bytes of every colour entry that the original copies from uninitialised stack are compared too (the emulated stack
is zero, the translation writes zero). Not tested: nothing else is left out.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_ui_2dmap_cpp"
MASK = 0xFFFFFFFF

FN = 0x102319D4
G_TBRENDERAREA = 0x10238E30
G_CONSTRUCT_ARRAY = 0x10238758
G_REGISTER = 0x10239940
G_STRING = 0x102382D8

ADDR = dict(
    map=0x1039742C, map2c=0x1039742C + 0x2C, map60=0x1039742C + 0x60,
    tbl=0x103970F0, ca=0x103B74C4, cb=0x103B74C0, vt=0x102B296C, dvec=0x102689F8, cvec=0x10268A00, dglue=0x103BCD38,
    tga=0x103970D0, tgan=0x103970E4, nodem=0x103B74B4,
)
for _i in range(6):
    ADDR["str%d" % _i] = 0x10397010 + 0x14 * _i
    ADDR["node%d" % _i] = 0x103970C4 - 0xC * _i
LABELS = ["map", "map2c", "map60", "tbl", "ca", "cb", "vt", "dvec", "cvec", "dglue", "tga", "tgan", "nodem"] + \
    ["str%d" % i for i in range(6)] + ["node%d" % i for i in range(6)]
LABEL_NO = {n: 200 + i for i, n in enumerate(LABELS)}

HOST_LAYOUT = [36, 4, 20, 12, 828]
TABLE_BYTES = 23 * 36
MAP_OFFSETS = [0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x5C, 0x20060]
NODE_NAMES = ["tgan", "nodem"] + ["node%d" % i for i in range(6)]

STUBS = CL.Stubs()
STUBS.real.add(FN)


def keep_toc(rec):
    """The glue the original calls restores r2 from 0x14(r1) afterwards; the fake stores it there as the glue does."""
    def fn(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        rec(e)
    return fn


STUBS.internal(G_TBRENDERAREA, keep_toc(CL.recorder("TbRenderArea", "L")))
STUBS.internal(G_CONSTRUCT_ARRAY, keep_toc(CL.recorder("ConstructArray", "LLiuu")))
STUBS.internal(G_REGISTER, keep_toc(CL.recorder("RegisterGlobalObject", "LLL")))
STUBS.internal(G_STRING, keep_toc(CL.recorder("TbString", "Ls")))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR[n]: LABEL_NO[n] for n in LABELS}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def vt(self):
        return ADDR["vt"]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self):
        T.call_ppc(self.e, FN, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {LABEL_NO[n]: self.g(n) for n in LABELS}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sm_addr(name.encode())

    def vt(self):
        return self.lib.tpw_sm_vtlow() & MASK

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_sm_run()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sm_addr.restype, lib.tpw_sm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sm_vtlow.restype = C.c_uint32
    lib.tpw_sm_run.restype = None
    out = (C.c_int64 * 32)()
    n = lib.tpw_sm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    for off, w in zip(MAP_OFFSETS, case["map"]):
        s.words(s.g("map") + off, [w])
    s.write(s.g("tbl"), case["tbl"])
    s.words(s.g("ca"), [case["ca"]])
    s.words(s.g("cb"), [case["cb"]])
    for n, ws in zip(NODE_NAMES, case["nodes"]):
        s.words(s.g(n), ws)


def observe(s):
    m = s.g("map")
    words = {}
    for off in MAP_OFFSETS:
        w = s.rd_word(m + off)
        if off == 0:
            w = "vtable" if w == s.vt() else w
        words[off] = w
    return {
        "log": s.log(),
        "map": words,
        "tbl": s.read(s.g("tbl"), TABLE_BYTES),
        "ca": s.rd_word(s.g("ca")),
        "cb": s.rd_word(s.g("cb")),
        "nodes": [[s.rd_word(s.g(n) + 4 * k) for k in range(3)] for n in NODE_NAMES],
    }


def run(s, case):
    try:
        build(s, case)
        s.call()
        return observe(s)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make_case(r, fixed=None):
    if fixed is None:
        rw = lambda: r.getrandbits(32)
        tbl = bytes(r.getrandbits(8) for _ in range(TABLE_BYTES))
    else:
        rw = lambda: fixed
        tbl = bytes([fixed & 0xFF]) * TABLE_BYTES
    return {
        "queues": {},
        "map": [rw() for _ in MAP_OFFSETS],
        "tbl": tbl,
        "ca": rw(), "cb": rw(),
        "nodes": [[rw(), rw(), rw()] for _ in NODE_NAMES],
    }


def edges():
    r = random.Random(5194)
    yield make_case(r, 0)
    yield make_case(r, MASK)
    yield make_case(r)


def rnd(r):
    return make_case(r)


TESTS = [
    FnTest("__sinit_ui_2dmap_cpp", ppc, host, edge=edges, rnd=rnd),
]
