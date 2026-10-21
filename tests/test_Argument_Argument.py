"""Unit v5138, Argument's constructor and destructor, against libtpw_unit_Argument_Argument.dylib (tests/unit_Argument_Argument.cpp),
main app, src/game/online/email.cpp:
  Argument::Argument 0x102011a8, Argument::~Argument 0x1020127c

A case is a world and a call. The block holds the three-word Argument at +0 and the source wide string object at +16, built by ONE piece
of Python code on both sides. The callees outside the unit (string ctor, wide string copy, string and wide string dtors, operator delete)
are fakes on both sides that record their arguments, pointers as offsets from the block.

The world of a case: the Argument words and the text words filled with random values (so a word the function must clear or leave shows).
Compared: the answer (as an offset, 0 for NULL), the Argument words, the text words, the event log.
No fused operations. Not tested: nothing excluded (NULL this is tested for the destructor).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Argument_Argument"
MASK = 0xFFFFFFFF

FN_CT, FN_DT = 0x102011A8, 0x1020127C
SIZES = {FN_CT: 108, FN_DT: 112}
OWN = set(SIZES)

STRING_CTOR, STRING_DTOR, WSTRING_COPY, WSTRING_DTOR, OP_DELETE = 0x1007363C, 0x1000221C, 0x101F7424, 0x101468F8, 0x10238218
HOST_LAYOUT = [12, 4, 8]
TEXT = 16


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rel(e, v):
    return (v - e.cl.base) & MASK


def st_string_ctor(e):
    e.cl.rec("StringCtor", (rel(e, e.r(3)),))


def st_string_dtor(e):
    e.cl.rec("StringDtor", (rel(e, e.r(3)), s32(T.s16(e.r(4)))))


def st_wstring_dtor(e):
    e.cl.rec("WStringDtor", (rel(e, e.r(3)), s32(T.s16(e.r(4)))))


def st_wstring_copy(e):
    e.cl.rec("WStringCopy", (rel(e, e.r(3)), rel(e, e.r(4)), e.r(5) & MASK, e.r(6) & MASK))


def st_delete(e):
    e.cl.rec("OperatorDelete", (rel(e, e.r(3)),))


STUBS.internal(STRING_CTOR, st_string_ctor)
STUBS.internal(STRING_DTOR, st_string_dtor)
STUBS.internal(WSTRING_COPY, st_wstring_copy)
STUBS.internal(WSTRING_DTOR, st_wstring_dtor)
STUBS.internal(OP_DELETE, st_delete)


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
        self.L.base = 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def set_base(self, a):
        self.L.base = a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def ctor(self, a):
        r3, _, _ = T.call_ppc(self.e, FN_CT, [a, a + TEXT])
        return r3 & MASK

    def dtor(self, a, flag):
        r3, _, _ = T.call_ppc(self.e, FN_DT, [a, flag & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_arg_alloc(n)

    def set_base(self, a):
        self.lib.tpw_arg_base(a)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ctor(self, a):
        return self.lib.tpw_arg_ctor(a, a + TEXT)

    def dtor(self, a, flag):
        return self.lib.tpw_arg_dtor(a, T.s16(flag))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_arg_alloc.restype, lib.tpw_arg_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_arg_base.restype, lib.tpw_arg_base.argtypes = None, [C.c_uint64]
    lib.tpw_arg_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_arg_ctor.restype, lib.tpw_arg_ctor.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64]
    lib.tpw_arg_dtor.restype, lib.tpw_arg_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int16]
    out = (C.c_int64 * 32)()
    n = lib.tpw_arg_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        a = s.alloc(64)
        s.set_base(a)
        s.words(a, case["init"])
        s.words(a + TEXT, case["text"])
        if case["op"] == "ct":
            r = s.ctor(a)
        else:
            r = s.dtor(0 if case["null"] else a, case["flag"])
        answer = (r - a) & MASK if r else 0
        return {"answer": answer, "log": s.log(), "obj": s.rd_words(a, 3), "text": s.rd_words(a + TEXT, 8)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, op, null=False, flag=0):
    return {"op": op, "queues": {}, "null": null, "flag": flag & MASK,
            "init": [r.getrandbits(32) for _ in range(3)], "text": [r.getrandbits(32) for _ in range(8)]}


def ct_edges():
    r = random.Random(11)
    for _ in range(3):
        yield mk(r, "ct")


def ct_random(r):
    return mk(r, "ct")


FLAGS = [0, 1, -1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, -32768, 5]


def dt_edges():
    r = random.Random(12)
    for f in FLAGS:
        yield mk(r, "dt", False, f)
        yield mk(r, "dt", True, f)


def dt_random(r):
    return mk(r, "dt", r.random() < 0.2, r.choice(FLAGS + [r.getrandbits(32)]))


TESTS = [
    FnTest("Argument::Argument", ppc, host, edge=ct_edges, rnd=ct_random),
    FnTest("Argument::~Argument", ppc, host, edge=dt_edges, rnd=dt_random),
]
