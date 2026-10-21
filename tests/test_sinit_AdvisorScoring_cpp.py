"""Unit v5018, the static initialiser of AdvisorScoring.cpp, against libtpw_unit_sinit_AdvisorScoring_cpp.dylib
(tests/unit_sinit_AdvisorScoring_cpp.cpp), main app ("SimTheme Park"), src/game/advisor/AdvisorScoring.cpp:
  __sinit_AdvisorScoring_cpp 0x1001c34c

A case is a world and a call. The world: the 351 scorer sources (3 words each on the Mac, a 16-byte pointer to member on the host) and the
351 message records, both filled with random words. The callees outside the unit are fakes on both sides: the string constructor
(0x10238800, records the object as a new label and the text) and __register_global_object (0x10239940, object, destructor, node).

Compared: the event log, and every record word except the scorer (unchanged) plus the three scorer words (+0x10) of every record, which
must hold the source of the same index. The host's fourth scorer word is not compared (the Mac has no such word); the sources write it 0.
No fused operations. Not tested: nothing (the function takes no input).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_AdvisorScoring_cpp"
MASK = 0xFFFFFFFF

FN = 0x1001c34c
SIZES = {FN: 9900}
OWN = set(SIZES)
CTOR = 0x10238800
REGISTER = 0x10239940
DTOR_ADDR = 0x103bcbb8
NODE_ADDR = 0x102b4124
SRC_BASE = 0x1027df30
TABLE_BASE = 0x1027efa4
N = 351
TOC = 0x10267cc0       # r2 of the main app (the asm's TOC comments: 0x1026076c + 0x7554)
HOST_REC = 0x38
HOST_LAYOUT = [0x38, 0x10, 0x20, 16]

STUBS = CL.Stubs()
STUBS.real.update(OWN)
def glue(fn):
    """What the import glue does around the real call: it saves the caller's TOC pointer in the frame (the caller reloads r2 from there)."""
    def run_glue(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", TOC))
        return fn(e)
    return run_glue


_ctor = CL.recorder("StringCtor", "N")


def ctor(e):
    """TbDynamicStringTemplate<wchar_t>::TbDynamicStringTemplate(const wchar_t*): the object as a new label, then the first three UTF-16 units of the text."""
    units = struct.unpack(">8H", e.read(e.r(4), 16))
    r = _ctor(e)
    e.cl.rec("StringText", units)
    return r


STUBS.imp("__ct__26TbDynamicStringTemplate<w>FPCw", glue(ctor))
STUBS.internal(REGISTER, glue(CL.recorder("RegisterGlobalObject", "LLL")))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({DTOR_ADDR: 1, NODE_ADDR: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def put_src(self, k, w):
        self.e.write(SRC_BASE + 12 * k, struct.pack(">3I", *w))

    def put_recs(self, recs):
        self.e.write(TABLE_BASE, b"".join(struct.pack(">12I", *r) for r in recs))

    def get_recs(self):
        data = self.e.read(TABLE_BASE, 0x30 * N)
        return [list(struct.unpack_from(">12I", data, 0x30 * i)) for i in range(N)]

    def call(self):
        T.call_ppc(self.e, FN, [])

    def log(self):
        return self.e.cl.log

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.src = lib.tpw_sinit_addr(b"sources")
        self.table = lib.tpw_sinit_addr(b"table")
        CL.host_begin(lib, {1: lib.tpw_sinit_addr(b"dtor"), 2: lib.tpw_sinit_addr(b"node")}, case["queues"])

    def put_src(self, k, w):
        C.memmove(self.src + 16 * k, struct.pack("<4I", w[0], w[1], w[2], 0), 16)

    def put_recs(self, recs):
        out = bytearray()
        for r in recs:
            out += struct.pack("<4I", *r[0:4]) + struct.pack("<4I", r[4], r[5], r[6], 0) + struct.pack("<5I", *r[7:12]) + b"\0" * 4
        assert len(out) == HOST_REC * N
        C.memmove(self.table, bytes(out), len(out))

    def get_recs(self):
        data = C.string_at(self.table, HOST_REC * N)
        recs = []
        for i in range(N):
            b = HOST_REC * i
            w = list(struct.unpack_from("<4I", data, b)) + list(struct.unpack_from("<3I", data, b + 0x10)) + list(
                struct.unpack_from("<5I", data, b + 0x20))
            recs.append(w)
        return recs

    def call(self):
        self.lib.tpw_sinit_run()

    def log(self):
        return CL.host_end(self.lib)

    def done(self):
        pass


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sinit_addr.restype, lib.tpw_sinit_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sinit_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sinit_run.restype = None
    out = (C.c_int64 * 32)()
    n = lib.tpw_sinit_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        for k in range(N):
            s.put_src(k, case["src"][k])
        s.put_recs(case["rec"])
        s.call()
        lg = s.log()
        return {"log": lg, "recs": s.get_recs()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make(r, mode):
    if mode == "zero":
        gen = lambda: 0
    elif mode == "ones":
        gen = lambda: MASK
    else:
        gen = lambda: r.getrandbits(32)
    return {"queues": {}, "src": [[gen() for _ in range(3)] for _ in range(N)], "rec": [[gen() for _ in range(12)] for _ in range(N)]}


def edges():
    r = random.Random(5018)
    for mode in ("zero", "ones", "rand"):
        yield make(r, mode)


def rnd(r):
    return make(r, "rand")


TESTS = [
    FnTest("__sinit_AdvisorScoring_cpp", ppc, host, edge=edges, rnd=rnd),
]
