"""Unit v5573, CBankContainer::~CBankContainer, against libtpw_unit_CBankContainer_Dtor_CBankContainer.dylib
(tests/unit_CBankContainer_Dtor_CBankContainer.cpp), sound program, src/TbMapStreamer_TbMapStreamer_CCatFixupTable_Load_CBankContainer_Dtor_CBankContainer.cpp:
  CBankContainer::~CBankContainer 0x10023afc

The callees are fakes on both sides: the CFileStorage destructor (0x1002eeb4, flag -1), the string destructor (0x1002eee4, flag -1),
the TbRefRelease destructor (0x1002eb9c, flag 0) and operator delete (0x1002ea94). Each records whether its argument is the expected
subobject. The host cannot see the hidden flags (the compiler picks them), so the PowerPC stubs assert them. The call order is compared.

The world: a zeroed 0x40 byte object; the call has this (NULL or the object) and the delete flag. Compared: the event log and the
object's words other than the vtable (the vtable is a TOC address on the Mac). No fused operations. Not tested: nothing.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_CBankContainer_Dtor_CBankContainer"
MASK = 0xFFFFFFFF

FN_DTOR = 0x10023AFC
SIZES = {FN_DTOR: 136}
OWN = set(SIZES)
STORAGE_DTOR, STRING_DTOR, BASE_DTOR, DELETE = 0x1002EEB4, 0x1002EEE4, 0x1002EB9C, 0x1002EA94

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def make_stub(name, obj_off, flag):
    def stub(e):
        L = e.cl
        assert s16(e.r(4)) == flag, (name, e.r(4))
        L.rec(name, (1 if e.r(3) == L.obj + obj_off else 0,))
    return stub


def s16(v):
    return T.s16(v)


def stub_delete(e):
    e.cl.rec("delete", (1 if e.r(3) == e.cl.obj else 0,))


STUBS.internal(STORAGE_DTOR, make_stub("StorageDtor", 0x24, -1))
STUBS.internal(STRING_DTOR, make_stub("StringDtor", 0xC, -1))
STUBS.internal(BASE_DTOR, make_stub("BaseDtor", 0, 0))
STUBS.internal(DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, {})
        self.saved = CL.Stubs.activate(STUBS, e) if False else STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def make(self, case):
        self.obj = self.e.alloc(0x40)
        self.L.obj = self.obj
        self.e.write(self.obj + 4, b"".join(struct.pack(">I", w) for w in case["words"]))
        return self.obj if case["this"] else 0

    def call(self, this, flag):
        T.call_ppc(self.e, FN_DTOR, [this, flag & 0xFFFF])

    def words(self):
        return list(struct.unpack(">15I", self.e.read(self.obj + 4, 60)))

    def log(self):
        return self.L.log


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    def make(self, case):
        self.obj = self.lib.tpw_cbd_alloc(self.lib.tpw_cbd_size())
        return self.obj if case["this"] else 0

    def call(self, this, flag):
        self.lib.tpw_cbd_dtor(this, s16(flag))

    def words(self):
        # nothing of the host object is written by the destructor; the Mac words are not mirrored on the host
        return list(struct.unpack(">15I", self_words_zero))

    def log(self):
        return CL.host_end(self.lib)


self_words_zero = b"\0" * 60


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cbd_alloc.restype, lib.tpw_cbd_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cbd_size.restype, lib.tpw_cbd_size.argtypes = C.c_uint32, []
    lib.tpw_cbd_dtor.restype, lib.tpw_cbd_dtor.argtypes = None, [C.c_uint64, C.c_int32]
    lib.bound = True


def run(s, case):
    try:
        this = s.make(case)
        s.call(this, case["flag"])
        return {"log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, this, flag):
    return {"this": this, "flag": flag, "words": [r.getrandbits(32) for _ in range(15)], "queues": {}}


def edges():
    r = random.Random(573)
    for this in (0, 1):
        for flag in (0, 1, -1, 2, 0x7FFF, -0x8000, 0x10000, 0x10001, 0xFFFF):
            yield mk(r, this, flag)


def rnd(r):
    return mk(r, r.choice([0, 1, 1]), r.choice([0, 1, -1, r.randrange(-40000, 40000)]))


TESTS = [
    FnTest("CBankContainer::~CBankContainer", ppc, host, edge=edges, rnd=rnd),
]
