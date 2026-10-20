"""Unit v5567, email.cpp's static initialiser, against libtpw_unit_sinit_email_cpp.dylib (tests/unit_sinit_email_cpp.cpp), main app,
src/FTPClient_RecvData_sinit_email_cpp.cpp:
  __sinit_email_cpp 0x1020afc0

The call builds 16 language name strings, the app name, the "Language" key and one wide string (each through the string constructor
and __register_global_object), then a 16 byte blob holding a 4 byte buffer whose first byte is the default byte. The constructors,
__register_global_object and operator new are fakes on both sides recording every argument (objects as labels, texts as texts).

The world of a case: the default byte global (0x102b1798). What is compared: the whole event log, the blob's words (length, flag,
word 0) and the buffer's first byte read through the blob's pointers, the default byte. No fused operations. Not tested: nothing
skipped. The buffer's other 3 bytes are never written by the original.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_email_cpp"
MASK = 0xFFFFFFFF

FN = 0x1020AFC0
OWN = {FN}

CTOR_C = "__ct__26TbDynamicStringTemplate<c>FPCc"
CTOR_W = "__ct__26TbDynamicStringTemplate<w>FPCw"

STRINGS, APP, KEY, WIDE = 0x10396AF8, 0x10396AD8, 0x10396AB8, 0x10396CF8
BLOBVAR, BLOBNODE = 0x10396AA8, 0x10396AAC
BYTE = 0x102B1798
DTOR, WDTOR, BLOBDTOR = 0x103BCD38, 0x103BCBB8, 0x102656B8
NODE0 = 0x10396CEC          # language i: NODE0 - 12 * i
WIDE_NODE, APP_NODE, KEY_NODE = 0x10396D0C, 0x10396AEC, 0x10396ACC
HOST_LAYOUT = [4, 8, 12, 16]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_new(e):
    n = e.r(3)
    e.cl.rec("new", (n,))
    e.set_r(3, e.alloc(n if n else 4))


STUBS.imp(CTOR_C, CL.recorder("TbStr", "Ls"))
STUBS.imp(CTOR_W, CL.recorder("TbWStr", "LS"))
STUBS.imp("__register_global_object", CL.recorder("reg", "LLL"))
STUBS.imp("__nw__FUl", stub_new)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {}
        for i in range(16):
            fixed[STRINGS + 0x14 * i] = 1 + i
            fixed[NODE0 - 12 * i] = 21 + i
        fixed.update({APP: 17, KEY: 18, WIDE: 19, BLOBVAR: 20, WIDE_NODE: 37, APP_NODE: 38, KEY_NODE: 39, BLOBNODE: 40,
                      DTOR: 41, WDTOR: 42, BLOBDTOR: 43})
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def byte_addr(self):
        return BYTE

    def deref(self, word):
        return word

    def call(self):
        T.call_ppc(self.e, FN, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        a = lambda n: lib.tpw_sinit_addr(n.encode())
        stride = lib.tpw_sinit_stride()
        fixed = {}
        for i in range(16):
            fixed[1 + i] = a("strings") + stride * i
            fixed[21 + i] = a("nodes") + 12 * i
        fixed.update({17: a("app"), 18: a("key"), 19: a("wide"), 20: a("blob"), 37: a("nodes") + 12 * 16, 38: a("nodes") + 12 * 17,
                      39: a("nodes") + 12 * 18, 40: a("blobnode"), 41: a("dtor"), 42: a("wdtor"), 43: a("blobdtor")})
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def byte_addr(self):
        return self.lib.tpw_sinit_addr(b"byte")

    def deref(self, word):
        return self.lib.tpw_sinit_deref(word)

    def blob_word(self):
        return self.rd_word(self.lib.tpw_sinit_addr(b"blob"))

    def call(self):
        self.lib.tpw_sinit_run()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sinit_addr.restype, lib.tpw_sinit_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sinit_deref.restype, lib.tpw_sinit_deref.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sinit_stride.restype = C.c_uint64
    lib.tpw_sinit_run.restype = None
    lib.tpw_sinit_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sinit_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.write(s.byte_addr(), bytes([case["byte"]]))
        s.call()
        word = s.rd_word(BLOBVAR) if isinstance(s, Ppc) else s.blob_word()
        blob = s.deref(word)
        length, flag, f0 = (s.rd_word(blob + off) for off in (4, 8, 0))
        data = s.deref(s.rd_word(blob + 12))
        return {"log": s.log(), "set": word != 0, "f0": f0, "length": length, "flag": flag, "first": s.read(data, 1),
                "byte": s.read(s.byte_addr(), 1)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(b):
    return {"queues": {}, "byte": b}


def edges():
    for b in (0, 0x41, 0x7F, 0x80, 0xFF):
        yield mk(b)


def rnd(r):
    return mk(r.randrange(256))


TESTS = [
    FnTest("__sinit_email_cpp", ppc, host, edge=edges, rnd=rnd),
]
