"""Unit v5663, std::basic_ostream<char>::flush, against libtpw_unit_std_basic_ostream_flush.dylib
(tests/unit_std_basic_ostream_flush.cpp), main app, src/std_basic_ostream_flush.cpp:
  std::basic_ostream<char,std::char_traits<char>>::flush 0x1020a604

A case is a world and a call. The world (a stream, its ios block, a stream buffer with a vtable, and a second tied stream) is
built twice by ONE piece of Python code, in emulator memory (big-endian) and in the host library's memory. The callees outside the
unit (good, tie, setstate, the stream buffer virtual through __ptr_glue, __throw) are fakes on both sides that record their
arguments (the thrown object is read back: type text, vtable, message, count, destructor vector) and answer from the case's queues.
new, new[], strlen and strcpy run for real on both sides.

Compared: the returned stream (as 'is self'), both ios blocks whole, and the event log. No fused operations. Not tested: the
catch handlers of the listing (reached only through exception tables), and the final recursive flush, which the listing never
reaches because the sentry flag at 0x99 is set to 1 first.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_ostream_flush"
MASK = 0xFFFFFFFF

FN = 0x1020A604
GOOD, TIE, SETSTATE, PTRGLUE = 0x102079B4, 0x10207964, 0x102077C0, 0x1023BAA8
VTBL, DTORVEC, TYPENAME = 0x1029D084, 0x102661D0, 0x10250098
MARK = 0x12345678
WORLD = {}


def ssum(data):
    s = 0
    for b in data:
        s = (s * 31 + b) & 0xFFFFFFFFFFFFFFFF
    return s & 0x7FFFFFFF


def cstr_bytes(e, a):
    out = bytearray()
    while True:
        b = e.read(a + len(out), 1)[0]
        if b == 0:
            return bytes(out)
        out.append(b)


STUBS = CL.Stubs()
STUBS.real.update({FN})


def stub_good(e):
    L = e.cl
    L.rec("good", (L.lab(e.r(3)),))
    e.set_r(3, 1 if L.ret("good") else 0)


def stub_tie(e):
    L = e.cl
    L.rec("tie", (L.lab(e.r(3)),))
    e.set_r(3, WORLD["s1"] if L.ret("tie") else 0)


def stub_setstate(e):
    L = e.cl
    L.rec("setstate", (L.lab(e.r(3)), T.s32(e.r(4))))


def stub_glue(e):
    L = e.cl
    r12 = e.r(12)
    L.rec("virt", (L.lab(e.r(3)), 0x1C if r12 == MARK else r12))
    e.set_r(3, L.ret("glue") & MASK)


def stub_throw(e):
    L = e.cl
    obj = e.r(4)
    msg = e.u32(obj + 4)
    cnt = e.u32(obj + 8)
    text = cstr_bytes(e, msg)
    L.rec("throw", (ssum(cstr_bytes(e, e.r(3))), 1 if e.u32(obj) == VTBL else 0, len(text), ssum(text), e.u32(cnt),
                    1 if e.r(5) == DTORVEC else 0))


STUBS.internal(GOOD, stub_good)
STUBS.internal(TIE, stub_tie)
STUBS.internal(SETSTATE, stub_setstate)
STUBS.internal(PTRGLUE, stub_glue)
STUBS.internal(0x10238680, stub_throw)


def stub_strlen(e):
    e.set_r(3, len(cstr_bytes(e, e.r(3))))


def stub_strcpy(e):
    d, sa = e.r(3), e.r(4)
    e.write(d, cstr_bytes(e, sa) + b"\0")


def stub_new(e):
    e.set_r(3, e.alloc(max(e.r(3), 4)))


STUBS.internal(0x10238650, stub_strlen)
STUBS.internal(0x10238410, stub_strcpy)
STUBS.internal(0x102384B8, stub_new)
STUBS.internal(0x10238668, stub_new)


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
        self.case = case

    def begin(self, labels):
        self.L = self.e.cl = CL.PpcLog({a: n for a, n in labels}, self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def ref(self, a):
        return a

    def set_tie(self, s1):
        WORLD["s1"] = s1

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, stream):
        r3, _, _ = T.call_ppc(self.e, FN, [stream & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)

    def begin(self, labels):
        CL.host_begin(self.lib, {n: a for a, n in labels}, self.case["queues"])

    def ref(self, a):
        return self.lib.tpw_flush_offset(a)

    def set_tie(self, s1):
        self.write(self.lib.tpw_flush_addr(b"tie"), struct.pack("<Q", s1))

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_flush_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, stream):
        return self.lib.tpw_flush_call(stream)


HOST_LAYOUT = [0x24, 0x34, 0x38, 0x30, 0]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_flush_addr.restype, lib.tpw_flush_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_flush_alloc.restype, lib.tpw_flush_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_flush_offset.restype, lib.tpw_flush_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_flush_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_flush_call.restype, lib.tpw_flush_call.argtypes = C.c_uint64, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_flush_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def mk_stream(s, spec):
    st, exc, flags, sbnull = spec
    S, I, SB, VT = s.alloc(16), s.alloc(0x40), s.alloc(16), s.alloc(0x40)
    s.words(S, [s.ref(I)])
    s.words(I + 0x24, [0 if sbnull else s.ref(SB)])
    s.words(I + 0x30, [flags, st, exc])
    s.words(SB, [s.ref(VT)])
    s.words(VT + 0x1C, [MARK])
    return S, I, SB


def run(s, case):
    try:
        S0, I0, SB0 = mk_stream(s, case["s0"])
        S1, I1, SB1 = mk_stream(s, case["s1"])
        s.set_tie(S1)
        s.begin([(S0, 1), (I0, 2), (S1, 3), (I1, 4), (SB0, 5), (SB1, 6)])
        ans = s.call(S0)
        def ios(i, sb):
            w = s.rd_words(i, 16)
            assert w[9] in (0, s.ref(sb))        # the stream buffer pointer is an address: compared as a flag
            w[9] = 1 if w[9] else 0
            return w
        return {"self": ans == S0, "log": s.log(), "ios0": ios(I0, SB0), "ios1": ios(I1, SB1)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def spec(r):
    return (r.choice([0, 0, 1, 2, 4, 5]), r.choice([0, 1, 2, 4, 5, 7, 1]), r.choice([0, 0x2000, 0x2030, 0x10]), r.random() < 0.15)


def case(r, good=None, tie=None, glue=None, s0=None, s1=None):
    g = good if good is not None else [r.choice([1, 1, 0]) for _ in range(r.randrange(1, 6))]
    x = r.choice([0, 1])
    t = tie if tie is not None else [x, x, 0]
    return {"queues": {"good": g, "tie": t, "glue": [glue if glue is not None else r.choice([-1, -1, 0, 5])]},
            "s0": s0 or spec(r), "s1": s1 or spec(r)}


def edges():
    r = random.Random(663)
    for good in ([0], [1], [1, 0], [1, 1, 0], [1, 1, 1, 0], [1, 1, 1, 1], [1, 0, 1]):
        for tie in ([0], [1, 1, 0]):
            for glue in (-1, 0, 5):
                for s0 in ((0, 1, 0, False), (1, 1, 0, False), (0, 2, 0, False), (4, 5, 0x2000, False), (0, 1, 0, True)):
                    yield case(r, good, tie, glue, s0)


def rnd(r):
    return case(r)


TESTS = [FnTest("std::basic_ostream<char,std::char_traits<char>>::flush", ppc, host, edge=edges, rnd=rnd)]
