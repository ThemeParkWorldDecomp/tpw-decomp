"""Unit v5717, LbDraw.cpp's static initialiser, against libtpw_unit_sinit_LbDraw_cpp.dylib (tests/unit_sinit_LbDraw_cpp.cpp), bullfrog,
src/bullfrog/LbDraw.cpp:
  __sinit_LbDraw_cpp 0x1002b7dc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. There are no callees.

The world of a case: random words in the viewport (4 words), the clip rectangle (4 words) and the pixel capabilities (8 words). What is
compared: those 16 words after the call (all zero) and the log. The four render contexts: the original stores a vtable address in each;
the host's contexts are placeholder objects (mDepth) whose vtables are not translated, so the host leaves them alone. The PowerPC side's
four words are checked against the expected vtable addresses inside the test (an assertion) and the host side's answer is the same
constant; the host side's context words are checked unchanged by the same assertion.

No fused operations. Not tested: the host's context stores (no vtables exist on the host).
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest
import calllog as CL

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_sinit_LbDraw_cpp"
MASK = 0xFFFFFFFF

FN = 0x1002b7dc
ADDR = dict(view=0x10074870, clip=0x10074860, caps=0x10074838,
            ctx8=0x10074824, ctx16=0x10074820, ctx24=0x1007481c, ctx32=0x10074818)
VTABLES = dict(ctx8=0x10073720, ctx16=0x1007360c, ctx24=0x100736c4, ctx32=0x10073668)
HOST_LAYOUT = [16, 32, 4]

STUBS = CL.Stubs()
STUBS.real.add(FN)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self):
        T.call_ppc(self.e, FN, [])

    def ctx_ok(self, case):
        ok = all(self.rd_words(ADDR[k], 1)[0] == v for k, v in VTABLES.items())
        assert ok, [hex(self.rd_words(ADDR[k], 1)[0]) for k in VTABLES]
        return 1


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        self.before = {k: self.rd_words(self.g(k), 1)[0] for k in VTABLES}

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sl_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_sl_run()

    def ctx_ok(self, case):
        after = {k: self.rd_words(self.g(k), 1)[0] for k in VTABLES}
        assert after == self.before
        return 1


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sl_addr.restype, lib.tpw_sl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sl_alloc.restype, lib.tpw_sl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sl_run.restype, lib.tpw_sl_run.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_sl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("view"), case["view"])
    s.words(s.g("clip"), case["clip"])
    s.words(s.g("caps"), case["caps"])


def observe(s, case):
    s.call()
    return {"view": s.rd_words(s.g("view"), 4), "clip": s.rd_words(s.g("clip"), 4),
            "caps": s.rd_words(s.g("caps"), 8), "ctx": s.ctx_ok(case), "log": s.log()}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, kind):
    if kind == 0:
        w = lambda n: [0] * n
    elif kind == 1:
        w = lambda n: [MASK] * n
    else:
        w = lambda n: [r.getrandbits(32) for _ in range(n)]
    return {"queues": {}, "view": w(4), "clip": w(4), "caps": w(8)}


def edges():
    r = random.Random(5717)
    for k in (0, 1, 2):
        yield mk(r, k)


def rnd(r):
    return mk(r, 2)


TESTS = [
    FnTest("__sinit_LbDraw_cpp", ppc, host, edge=edges, rnd=rnd),
]
