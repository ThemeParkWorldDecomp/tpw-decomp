"""Unit v5251, the ostream sentry destructor, against libtpw_unit_std_basic_ostream_sentry_Dtor_sentry.dylib
(tests/unit_std_basic_ostream_sentry_Dtor_sentry.cpp), main app ("SimTheme Park"), src/std_basic_ostream_sentry_Dtor_sentry.cpp:
  std::basic_ostream<char>::sentry::~sentry 0x1020758c

A case is a world and a call. One block holds the sentry (at +0, done byte at +1, stream pointer at the odd offset +2), the stream
(+0x10, its ios pointer) and the ios (+0x40: format flags at 0x30, state at 0x34). The callees outside the unit, flush (0x1020a604) and
operator delete (0x10238218), are fakes on both sides recording their pointer as an offset from the block.

What is compared: whether this is returned, the event log and the whole block. Fused operations: none. Not tested: nothing skipped
(a NULL this is a case; a non-NULL this always has a valid stream because the original reads it unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_ostream_sentry_Dtor_sentry"
MASK = 0xFFFFFFFF

FN = 0x1020758C
FLUSH = 0x1020A604
DELETE = 0x10238218
BLOCK = 0x80
STREAM, IOS = 0x10, 0x40
HOST_LAYOUT = [1, 2, 0x30, 0x34]

BASE = {"v": 0}

STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_flush(e):
    e.cl.rec("flush", (T.s32(e.r(3) - BASE["v"]),))


def stub_delete(e):
    e.cl.rec("delete", (T.s32(e.r(3) - BASE["v"]),))


STUBS.internal(FLUSH, stub_flush)
STUBS.internal(DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def build(self, case):
        a = self.alloc(BLOCK)
        BASE["v"] = a
        self.base = a
        fmt = self.fmt
        self.write(a, bytes([case["pad"], case["done"]]) + struct.pack(fmt, (self.ptr(a) + STREAM) & MASK))
        self.write(a + STREAM, struct.pack(fmt, (self.ptr(a) + IOS) & MASK))
        self.write(a + IOS + 0x30, struct.pack(fmt, case["flags"]) + struct.pack(fmt, case["state"]))
        self.this = 0 if case["null"] else a


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, {})
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def ptr(self, a):
        return a

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, d):
        self.e.write(a, d)

    def log(self):
        return self.L.log

    def call(self, flag):
        r3, _, _ = T.call_ppc(self.e, FN, [self.this, flag & MASK])
        return 1 if (r3 & MASK) == self.this else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    def ptr(self, a):
        return self.lib.tpw_ostsent_off(a)

    def alloc(self, n):
        return self.lib.tpw_ostsent_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, flag):
        return self.lib.tpw_ostsent_dtor(self.this, T.s16(flag)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ostsent_alloc.restype, lib.tpw_ostsent_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ostsent_off.restype, lib.tpw_ostsent_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_ostsent_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ostsent_dtor.restype, lib.tpw_ostsent_dtor.argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ostsent_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.build(case)
        ans = s.call(case["flag"])
        blk = bytearray(s.read(s.base, BLOCK))
        for off in (2, STREAM):    # the two pointers, as offsets from the block (the sides' addresses differ)
            w = struct.unpack(s.fmt, bytes(blk[off:off + 4]))[0]
            blk[off:off + 4] = struct.pack(">I", (w - s.ptr(s.base)) & MASK)
        for off in (IOS + 0x30, IOS + 0x34):    # plain words: made big-endian for the compare
            w = struct.unpack(s.fmt, bytes(blk[off:off + 4]))[0]
            blk[off:off + 4] = struct.pack(">I", w)
        return {"answer": ans, "log": s.log(), "block": bytes(blk)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


FLAGS = [0, 1, -1, 0x7FFF, 0x8000, 0x10000, 0x10001, 0xFFFF, 5, -32768]
STATES = [0, 1, 2, 4, 5, 3, 6, 7, 0x100, 0x80000000, 0x10, 8]
FLAGWORDS = [0, 0x2000, 0x1FFF, 0x4000, 0xFFFFDFFF, 0xFFFFFFFF, 0x2001, 0x1000, 0x20000]


def mk(null, pad, done, flags, state, flag):
    return {"null": null, "pad": pad, "done": done, "flags": flags, "state": state, "flag": flag & MASK}


def edges():
    for null in (True, False):
        for done in (0, 1, 0xFF):
            for flags in FLAGWORDS:
                for state in STATES:
                    yield mk(null, 0xAA, done, flags, state, 1 if (state + done) % 2 else -3)
    for f in FLAGS:
        yield mk(False, 0, 0, 0x2000, 0, f)
        yield mk(True, 0, 0, 0x2000, 0, f)
        yield mk(False, 0, 1, 0x2000, 0, f)


def rnd(r):
    return mk(r.random() < 0.15, r.randrange(256), r.choice([0, 0, 1, r.randrange(256)]),
              r.choice(FLAGWORDS + [r.getrandbits(32)]), r.choice(STATES + [r.getrandbits(32)]),
              r.choice(FLAGS + [r.getrandbits(32)]))


TESTS = [
    FnTest("std::basic_ostream<char,std::char_traits<char>>::sentry::~sentry", ppc, host, edge=edges, rnd=rnd),
]
