"""Unit v5768, ui_IME.cpp's static initialiser, against libtpw_unit_sinit_ui_IME_cpp.dylib (tests/unit_sinit_ui_IME_cpp.cpp), main app,
src/ui_iterators_sinit_ui_IME_cpp.cpp:
  __sinit_ui_IME_cpp 0x101f7c74

A case fills gUIIME with one garbage byte on both sides, then calls the initialiser. The callees outside the unit are fakes on both
sides: IME::CIME::CIME (0x101fe934) records the object, __register_global_object records the object, destructor TVector and node.

What is compared: the event log, and the words the initialiser writes (owner, +0x08, the active byte, +0x10, the candidate list's first
word and its vector capacity, size and data), read at the Mac's offsets on the PowerPC side and at the host members' offsets on the host
(pointers are 8 bytes there). No fused operations. Not tested: the bytes the initialiser does not write (the host layout differs).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_ui_IME_cpp"
MASK = 0xFFFFFFFF

FN = 0x101F7C74
CIME_CTOR = 0x101FE934

GUIIME = 0x103966C0
MIME = GUIIME + 0x1C
DTOR = 0x10268600
NODE = 0x10396A40
ADDR = dict(gUIIME=GUIIME, dtor=DTOR, node=NODE)
OBJ_SIZE = 0x180

# (name, Mac offset, size) of what the initialiser writes
MAC_FIELDS = [("owner", 0x00, 4), ("f08", 0x08, 4), ("active", 0x0C, 1), ("f10", 0x10, 4),
              ("cand0", 0x154, 4), ("cap", 0x164, 4), ("size", 0x168, 4), ("data", 0x16C, 4)]

STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_cime(e):
    e.cl.rec("CIME", (e.cl.lab(e.r(3)),))


def stub_register(e):
    L = e.cl
    L.rec("RegisterGlobal", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))


STUBS.internal(CIME_CTOR, stub_cime)
STUBS.imp("__register_global_object", stub_register)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({GUIIME: 1, MIME: 2, DTOR: 3, NODE: 4}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def fill(self, byte):
        self.e.write(GUIIME, bytes([byte]) * OBJ_SIZE)

    def fields(self):
        out = {}
        for name, off, size in MAC_FIELDS:
            out[name] = int.from_bytes(self.e.read(GUIIME + off, size), "big")
        return out

    def log(self):
        return self.L.log

    def call(self):
        T.call_ppc(self.e, FN, [])


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_ime_addr(b"gUIIME")
        CL.host_begin(lib, {1: self.base, 2: self.base + HOST_OFFS["mIME"], 3: lib.tpw_ime_addr(b"dtor"),
                            4: lib.tpw_ime_addr(b"node")}, case["queues"])

    def done(self):
        pass

    def fill(self, byte):
        C.memset(self.base, byte, HOST_OFFS["size"])

    def fields(self):
        out = {}
        for name, off, size in HOST_FIELDS:
            out[name] = int.from_bytes(C.string_at(self.base + off, size), "little")
        return out

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_ime_sinit()


HOST_OFFS = {}
HOST_FIELDS = []


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, "tpw_ime_addr").restype, getattr(lib, "tpw_ime_addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, "tpw_ime_layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ime_sinit.restype = None
    out = (C.c_int64 * 32)()
    n = lib.tpw_ime_layout(out)
    v = list(out)[:n]
    assert n == 10, v
    HOST_OFFS.update(mIME=v[8], size=v[9])
    HOST_FIELDS[:] = [("owner", v[0], 8), ("f08", v[1], 4), ("active", v[2], 1), ("f10", v[3], 4),
                      ("cand0", v[4], 4), ("cap", v[5], 4), ("size", v[6], 4), ("data", v[7], 8)]
    lib.bound = True


def run(s, case):
    try:
        s.fill(case["fill"])
        s.call()
        return {"log": s.log(), "fields": s.fields()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def edges():
    for b in (0x00, 0xFF, 0xA5, 0x01):
        yield {"queues": {}, "fill": b}


def rnd(r):
    return {"queues": {}, "fill": r.randrange(256)}


TESTS = [
    FnTest("__sinit_ui_IME_cpp", ppc, host, edge=edges, rnd=rnd),
]
