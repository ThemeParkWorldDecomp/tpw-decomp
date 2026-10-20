"""Unit v51128: the ltms destructors TbIMBInfo, TbIFont, TbResource, CStaticMBBuffer, CStaticBuffer, CCharacterHandler, the
CCharacterHandler constructor and bfl::FileSystem::CFile::Read, against libtpw_unit_TbIMBInfo_Dtor_TbIMBInfo.dylib
(tests/unit_TbIMBInfo_Dtor_TbIMBInfo.cpp), program "ltms symbols shared", src/TbIMBToUnicode_TbIMBInfo_Dtor_TbIMBInfo.cpp.

A case builds an object (Mac layout in the emulator; the host wrapper builds the native object at the same block), calls the destructor
with a hidden delete flag (a short in r4) or the constructor or Read. Fakes on both sides record events: delete (operator delete),
Close and Release (the virtual calls of CStaticMBBuffer's destructor through __ptr_glue), Read (the file's access object).
Compared: the returned this, the event log; for the constructor the vptr being set and the bytes after it untouched.
No fused operations. Not tested: a NULL file for Read (read unconditionally).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_TbIMBInfo_Dtor_TbIMBInfo"
MASK = 0xFFFFFFFF

DTORS = {
    "TbIMBInfo::~TbIMBInfo": (0, 0x10002F3C),
    "TbIFont::~TbIFont": (1, 0x10002CB8),
    "TbResource::~TbResource": (2, 0x10002C44),
    "CStaticMBBuffer::~CStaticMBBuffer": (3, 0x10000B24),
    "CStaticBuffer::~CStaticBuffer": (4, 0x10000AAC),
    "CCharacterHandler::~CCharacterHandler": (5, 0x10000060),
}
CTOR = 0x10000024
READ = 0x10002EC4
DELETE = 0x1000F694
GLUE = 0x1000F91C

CUR = {}
STUBS = CL.Stubs()
STUBS.real.update([a for _, a in DTORS.values()] + [CTOR, READ])


def stub_delete(e):
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)),))


def stub_glue(e):
    L = e.cl
    if CUR["op"] == "read":
        L.rec("Read", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK))
        e.set_r(3, L.ret("Read") & MASK)
    elif e.r(3) == CUR["obj"]:
        L.rec("Close", (L.lab(e.r(3)),))
    else:
        L.rec("Release", (L.lab(e.r(3)),))


STUBS.internal(DELETE, stub_delete)
STUBS.internal(GLUE, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        import struct
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        import struct
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.case = case

    def begin(self, names):
        fixed = {a: i + 1 for i, a in enumerate(names) if a}
        self.L = self.e.cl = CL.PpcLog(fixed, self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, d):
        self.e.write(a, d)

    def log(self):
        return self.L.log

    def dtor(self, kind, addr, obj, flag, conv):
        r3, _, _ = T.call_ppc(self.e, addr, [obj, flag & MASK])
        return r3

    def ctor(self, obj):
        T.call_ppc(self.e, CTOR, [obj])

    def readf(self, obj, impl, dest, size):
        r3, _, _ = T.call_ppc(self.e, READ, [obj, dest, size])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)

    def begin(self, names):
        CL.host_begin(self.lib, {i + 1: a for i, a in enumerate(names) if a}, self.case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_ch_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def log(self):
        return CL.host_end(self.lib)

    def dtor(self, kind, addr, obj, flag, conv):
        return self.lib.tpw_ch_dtor(kind, obj, flag, conv)

    def ctor(self, obj):
        self.lib.tpw_ch_ctor(obj)

    def readf(self, obj, impl, dest, size):
        return self.lib.tpw_ch_read(obj, impl, dest, size) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ch_alloc.restype, lib.tpw_ch_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ch_dtor.restype, lib.tpw_ch_dtor.argtypes = C.c_uint64, [C.c_uint32, C.c_uint64, C.c_int32, C.c_uint64]
    lib.tpw_ch_ctor.restype, lib.tpw_ch_ctor.argtypes = None, [C.c_uint64]
    lib.tpw_ch_read.restype, lib.tpw_ch_read.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_uint64, C.c_uint32]
    lib.bound = True


PAT = 0xA5A5A5A5


def run(s, case):
    try:
        op = case["op"]
        obj, conv, dest, impl = s.alloc(64), s.alloc(16), s.alloc(16), s.alloc(16)
        vt, tv, ivt = s.alloc(32), s.alloc(8), s.alloc(32)
        s.words(obj, [PAT] * 16)
        s.words(conv, [vt])
        s.words(vt + 8, [tv])
        s.words(impl, [ivt])
        s.words(ivt + 0xC, [tv])
        use_conv = conv if case["conv"] else 0
        use_impl = impl if case["impl"] else 0
        if op == "read":
            s.words(obj, [0, use_impl])
        elif op == "MB":
            s.words(obj + 0xC, [use_conv])
        CUR["op"] = op
        CUR["obj"] = obj
        s.begin([obj, use_conv, dest, use_impl])
        out = {}
        if op == "ctor":
            s.ctor(obj)
            out["vptr"] = s.rd_word(obj) != 0
            out["tail"] = s.read(obj + 8, 56)
        elif op == "read":
            out["answer"] = s.readf(obj, use_impl, dest, case["size"])
        else:
            kind, addr = case["fn"]
            arg = 0 if case["null"] else obj
            ans = s.dtor(kind, addr, arg, case["flag"], use_conv)
            out["answer"] = ans == arg
        out["log"] = s.log()
        return out
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


FLAGS = [0, 1, -1, 2, 0x7FFF, -0x8000, 0x10000, 0x18000, 0xFFFF]


def dtor_case(r, name, null=None, flag=None, conv=None):
    kind, addr = DTORS[name]
    return {"op": "MB" if kind == 3 else "dtor", "fn": (kind, addr),
            "null": r.random() < 0.2 if null is None else null,
            "flag": r.choice(FLAGS) if flag is None else flag,
            "conv": r.random() < 0.6 if conv is None else conv, "impl": False, "queues": {}}


def dtor_edges(name):
    def gen():
        r = random.Random(11)
        for null in (False, True):
            for flag in FLAGS:
                for conv in (False, True):
                    yield dtor_case(r, name, null, flag, conv)
    return gen


def dtor_rnd(name):
    return lambda r: dtor_case(r, name)


def ctor_case(r):
    return {"op": "ctor", "conv": False, "impl": False, "queues": {}}


def read_case(r, impl=None, size=None):
    return {"op": "read", "conv": False, "impl": r.random() < 0.7 if impl is None else impl,
            "size": r.choice([0, 1, 16, 0xFFFFFFFF, r.randrange(0, 1 << 32)]) if size is None else size,
            "queues": {"Read": [r.choice([0, -1, 1, 100, r.randrange(-(1 << 31), 1 << 31)])]}}


def read_edges():
    r = random.Random(5)
    for impl in (False, True):
        for size in (0, 1, 0xFFFFFFFF):
            yield read_case(r, impl, size)


TESTS = [FnTest(n, ppc, host, edge=dtor_edges(n), rnd=dtor_rnd(n)) for n in DTORS]
TESTS.append(FnTest("CCharacterHandler::CCharacterHandler", ppc, host, edge=lambda: iter([ctor_case(None)]), rnd=ctor_case))
TESTS.append(FnTest("bfl::FileSystem::CFile::Read", ppc, host, edge=read_edges, rnd=read_case))
