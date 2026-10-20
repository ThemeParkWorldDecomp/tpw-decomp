"""Unit v51107, the small Sound Sprockets pieces of CSprocketsModule.cpp, against libtpw_unit_SamsUtilities_UCommand_SetImmediate.dylib
(tests/unit_SamsUtilities_UCommand_SetImmediate.cpp), sound symbols shared, src/TbAudioBody_create_SamsUtilities_UCommand_SetImmediate.cpp:
  UCommand::SetImmediate 0x1002a5c4, UHasFlags<unsigned long> ClearFlag 0x1002a63c, SetFlag 0x1002a690, UHasFlags() 0x1002a858,
  UHasFlags(unsigned long) 0x1002c4d8, UCommand() 0x1002a7e8, UCommand(unsigned long) 0x1002c468, UReferenceCounter::operator T* 0x1002a6e0,
  UCommandPlay 0x1002a774, CSprocketsChannel SetPipe 0x1002a94c, GetDescription 0x1002a99c, Is3D 0x1002b1fc, UReferenceCounted::AddReference
  0x1002adfc, UReferenceCounter ctor 0x1002afec, CSprocketsSpeaker 0x1002b2d0, Module 0x1002b338, ~CSprocketsModule 0x1002b374, Stereo
  0x1002b65c, Config 0x1002b6c4, UCommandSoundDistance 0x1002b838, UManager<UAllocator>::operator-> 0x1002bbe4, SetOptions 0x1002bd90,
  Set3DDSPFunctionality 0x1002beb8, UStatus::IfOk 0x1002c144, UCommandStop 0x1002c1c4, TbRect 0x1002879c, UCommandPause 0x1002c3f4.

A case is a world and a call: a block of random words (an object of the Mac's layout) built twice by one piece of Python (big-endian in the
emulator, little-endian on the host), then the function. Callees outside the unit: UManager::Allocate (answers a new 8-byte block or NULL from
the case's queue) and the base destructor TbSoundSystemModule::~TbSoundSystemModule (records whether it got the object, and its flag), fakes
on both sides. What is compared: the answer, the whole block read back (fields of the Mac and host layouts that differ by name), the event log.
The vtable words the constructors store are the TOC plus an offset, written native-endian as a word on the host, so they are compared too.
No fused operations. Not tested: the destructor's NULL this and its delete flag above 0 (the original calls operator delete 0x1002ea94; a C++
destructor leaves the delete to the caller), and the vtable word the destructor stores (a host vtable pointer replaces it).
"""
import ctypes as C
import itertools
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_SamsUtilities_UCommand_SetImmediate"
MASK = 0xFFFFFFFF

# (op id, name, address, size)
FUNCS = [
    (0, "SamsUtilities::UCommand::SetImmediate", 0x1002a5c4, 56),
    (1, "SamsUtilities::UHasFlags<unsigned_long>::ClearFlag", 0x1002a63c, 16),
    (2, "SamsUtilities::UHasFlags<unsigned_long>::SetFlag", 0x1002a690, 16),
    (3, "SamsUtilities::UHasFlags<unsigned_long>::UHasFlags@0x1002a858", 0x1002a858, 12),
    (4, "SamsUtilities::UHasFlags<unsigned_long>::UHasFlags@0x1002c4d8", 0x1002c4d8, 8),
    (5, "SamsUtilities::UCommand::UCommand@0x1002a7e8", 0x1002a7e8, 56),
    (6, "SamsUtilities::UCommand::UCommand@0x1002c468", 0x1002c468, 56),
    (7, "SamsUtilities::UReferenceCounter<SamsUtilities::USoundDescription>::operator_SamsUtilities::USoundDescription*", 0x1002a6e0, 8),
    (8, "SamsUtilities::UCommandPlay::UCommandPlay", 0x1002a774, 56),
    (9, "CSprocketsChannel::SetPipe", 0x1002a94c, 8),
    (10, "CSprocketsChannel::GetDescription", 0x1002a99c, 36),
    (11, "SamsUtilities::UReferenceCounted::AddReference", 0x1002adfc, 20),
    (12, "SamsUtilities::UReferenceCounter<SamsUtilities::USoundDescription>::UReferenceCounter", 0x1002afec, 64),
    (13, "CSprocketsChannel::Is3D", 0x1002b1fc, 8),
    (14, "CSprocketsSpeaker::CSprocketsSpeaker", 0x1002b2d0, 56),
    (15, "TbSpeaker::Module::Module", 0x1002b338, 12),
    (16, "CSprocketsModule::~CSprocketsModule", 0x1002b374, 100),
    (17, "TbSpeaker::Stereo::Stereo", 0x1002b65c, 56),
    (18, "TbSpeaker::Config::Config", 0x1002b6c4, 12),
    (19, "SamsUtilities::UCommandSoundDistance::UCommandSoundDistance", 0x1002b838, 72),
    (20, "SamsUtilities::UManager<SamsUtilities::UAllocator>::operator->", 0x1002bbe4, 64),
    (21, "CSprocketsModule::SetOptions", 0x1002bd90, 36),
    (22, "CSprocketsModule::Set3DDSPFunctionality", 0x1002beb8, 36),
    (23, "SamsUtilities::UStatus::IfOk<TbLegacyError>", 0x1002c144, 28),
    (24, "SamsUtilities::UCommandStop::UCommandStop", 0x1002c1c4, 56),
    (25, "TbRect::TbRect", 0x1002879c, 20),
    (26, "SamsUtilities::UCommandPause::UCommandPause", 0x1002c3f4, 56),
]
ADDR_OF = {op: addr for op, _, addr, _ in FUNCS}
OWN = set(ADDR_OF.values())

CALLEE_ALLOCATE = 0x1002bc7c
CALLEE_BASE_DTOR = 0x100071d4
CALLEE_TBERROR = 0x100015b8      # runs as the original: stores r4 in the buffer at r3

MAC_OFF = {"mPipe": 0x3c, "m3D": 0x48, "mFlag49": 0x49, "mDescription": 0x18, "mRest": 4, "mCount": 4}
HOST_OFF = {}
HOST_LAYOUT = [0x48, 0x5c, 0x5d, 0x20, 8, 40, 8]

NARGS = {0: 1, 1: 1, 2: 1, 4: 1, 6: 1, 9: 1, 16: 1, 21: 1, 22: 2, 23: 2, 25: 4, 26: 1}
SPEC = [0, 1, 0x80000000, 0xFFFFFFFF, 0x12345678]
DTOR_FLAGS = [0, 0xFFFFFFFF, 0x80000000, 0x8000, 0x10000, 0x12348000, 0xFFFF8000, 0x7FFF0000]

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.add(CALLEE_TBERROR)


def stub_allocate(e):
    """UManager<UAllocator>::Allocate(this): stores a new 8-byte block (or NULL, from the queue) in mObject."""
    L = e.cl
    L.rec("Allocate", (0,))
    block = e.alloc(8) if (L.ret("Allocate") & 1) else 0
    e.write(e.r(3), struct.pack(">I", block))


def stub_base_dtor(e):
    """TbSoundSystemModule::~TbSoundSystemModule(this, flag): records whether it got the case's object and the flag."""
    L = e.cl
    L.rec("BaseDtor", (int(e.r(3) == L.obj), T.s32(e.r(4))))


STUBS.internal(CALLEE_ALLOCATE, stub_allocate)
STUBS.internal(CALLEE_BASE_DTOR, stub_base_dtor)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_f64bits(self, a):
        return struct.unpack(self.fmt[0] + "Q", self.read(a, 8))[0]

    def wr_ptr(self, a, v):
        self.write(a, struct.pack(self.fmt_ptr, v))

    def rd_ptr(self, a):
        return struct.unpack(self.fmt_ptr, self.read(a, struct.calcsize(self.fmt_ptr)))[0]

    def byte(self, a, v):
        self.write(a, bytes([v & 0xFF]))


class Ppc(Side):
    fmt = ">I"
    fmt_ptr = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.obj = 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def off(self, name):
        return MAC_OFF[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def expect(self, obj):
        self.L.obj = obj

    def call(self, op, c):
        a = c["args"]
        obj = c["obj"]
        if op == 0:
            args = [obj, a[0] & 0xFF]
        elif op in (1, 2, 4, 6, 9, 26):
            args = [obj, a[0] & MASK]
        elif op == 12:
            args = [obj, c["B"]]
        elif op == 16:
            args = [obj, a[0] & MASK]
        elif op == 19:
            args = [obj, T.D(c["dbl"])]
        elif op == 21:
            args = [c["buf"], obj, a[0] & MASK]
        elif op == 22:
            args = [c["buf"], obj, a[0] & MASK, a[1] & MASK]
        elif op == 23:
            args = [obj, a[0] & MASK, a[1] & MASK]
        elif op == 25:
            args = [obj] + [x & MASK for x in a]
        else:
            args = [obj]
        r3, _, _ = T.call_ppc(self.e, ADDR_OF[op], args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    fmt_ptr = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def off(self, name):
        return HOST_OFF[name]

    def alloc(self, n):
        return self.lib.tpw_v51107_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def expect(self, obj):
        pass

    def call(self, op, c):
        a = [x & MASK for x in c["args"]] + [0, 0, 0, 0]
        obj = c["obj"]
        if op == 12:
            B = c["B"]
            a = [B & MASK, B >> 32, 0, 0]
        if op == 0:
            a[0] &= 0xFF
        return self.lib.tpw_v51107_call(op, obj, a[0], a[1], a[2], a[3], c.get("dbl", 0.0))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    f = lib.tpw_v51107_alloc
    f.restype, f.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_v51107_layout.argtypes = [C.POINTER(C.c_int64)]
    f = lib.tpw_v51107_call
    f.restype = C.c_uint64
    f.argtypes = [C.c_int32, C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_double]
    out = (C.c_int64 * 32)()
    n = lib.tpw_v51107_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    HOST_OFF.update(mPipe=out[0], m3D=out[1], mFlag49=out[2], mDescription=out[3], mRest=out[4], mCount=out[6])
    lib.bound = True


def norm(v, ref):
    v &= MASK
    return 0 if v == 0 else (1 if v == ref & MASK else 2)


def run(s, case):
    try:
        op = case["op"]
        a = case["args"]
        obj = case["obj"] = s.alloc(0x200)
        s.words(obj, case["w"])
        if op == 7:
            s.wr_ptr(obj, case["tok"])
        elif op == 10:
            s.wr_ptr(obj + s.off("mDescription"), case["tok"])
        elif op == 9:
            s.wr_ptr(obj + s.off("mPipe"), case["tok"])
            s.byte(obj + s.off("m3D"), case["w"][0])
            s.byte(obj + s.off("mFlag49"), case["w"][1])
        elif op == 13:
            s.byte(obj + s.off("m3D"), a[0])
        elif op == 11:
            s.words(obj + s.off("mCount"), [a[0]])
        elif op == 12:
            B = s.alloc(64)
            s.words(B, case["w"][:16])
            s.words(B + s.off("mCount"), [a[0]])
            case["B"] = 0 if case["null"] else B
            case["Bobj"] = B
            s.wr_ptr(obj, case["tok"])
            s.words(obj + s.off("mRest"), case["w"][20:27])
        elif op == 20:
            S = case["S"] = s.alloc(16)
            s.wr_ptr(obj, [0, S, case["tok"]][case["mode"]])
        elif op in (21, 22):
            case["buf"] = s.alloc(4)
            s.words(case["buf"], [0xDEADBEEF])
        elif op == 16:
            s.expect(obj)
        r = s.call(op, case)
        out = {"answer": None}
        if op in (7, 10, 11, 23):
            out["answer"] = r
        elif op == 13:
            out["answer"] = r & 0xFF
        elif op == 20:
            out["answer"] = norm(r, case["S"])
        elif op in (21, 22):
            out["answer"] = s.rd_word(case["buf"]) if isinstance(s, Ppc) else r
        if op == 19:
            out["w01"] = s.rd_words(obj, 2)
            out["dbl"] = s.rd_f64bits(obj + 8)
            out["rest"] = s.rd_words(obj + 0x10, 30)
        elif op == 9:
            out["pipe"] = s.rd_ptr(obj + s.off("mPipe")) & MASK
            out["b"] = (s.read(obj + s.off("m3D"), 1), s.read(obj + s.off("mFlag49"), 1))
        elif op == 11:
            out["count"] = s.rd_word(obj + s.off("mCount"))
        elif op == 12:
            pm = s.rd_ptr(obj)
            out["pmatch"] = 0 if pm == 0 else (1 if pm == case["Bobj"] else 2)
            out["rest"] = s.rd_words(obj + s.off("mRest"), 7)
            out["count"] = s.rd_word(case["Bobj"] + s.off("mCount"))
        elif op == 20:
            out["mobj"] = norm(s.rd_ptr(obj), case["S"])
            out["rest"] = s.rd_words(obj + 8, 8)
        elif op == 16:
            out["rest"] = s.rd_words(obj + 8, 30)
        elif op not in (7, 10, 13, 21, 22):
            out["blk"] = s.rd_words(obj, 40)
        out["log"] = s.log()
        return out
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rv(r):
    return r.choice(SPEC) if r.random() < 0.4 else r.getrandbits(32)


def gen(op, r, args=None):
    n = NARGS.get(op, 0)
    if args is None:
        args = [r.choice(DTOR_FLAGS)] if op == 16 else [rv(r) for _ in range(n)]
    if op == 0:
        args = [args[0] & 0xFF]
    if op in (11, 12, 13) and not args:
        args = [r.choice([0, 1, 0xFFFFFFFF, 0x7FFFFFFF, r.getrandbits(32)])]
    w = [r.getrandbits(32) for _ in range(40)]
    if r.random() < 0.3:
        w[0] = 0
    dbl = r.choice([0.0, -0.0, 1.0, 400.0, float("inf"), r.uniform(-1e6, 1e6), r.uniform(-1, 1) * 10 ** r.uniform(-20, 20)])
    return {"op": op, "args": args, "w": w, "dbl": dbl, "tok": r.randrange(1, 1 << 32),
            "null": r.random() < 0.3, "mode": r.randrange(3), "queues": {"Allocate": [r.choice([0, 1])]}}


def edges(op):
    r = random.Random(5000 + op)
    if op == 16:
        for f in DTOR_FLAGS:
            yield gen(op, r, [f])
        return
    n = NARGS.get(op, 0)
    if op == 0:
        for v in (0, 1, 2, 0x7F, 0x80, 0xFF):
            for w0 in (0, 1, 0xFFFFFFFF, 0xFFFFFFFE):
                c = gen(op, r, [v])
                c["w"][0] = w0
                yield c
        return
    if op in (11, 13):
        for v in (0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFE, 0x100, 0xFF):
            yield gen(op, r, [v])
        return
    if op == 12:
        for null in (False, True):
            for cnt in (0, 1, 0xFFFFFFFF):
                c = gen(op, r, [cnt])
                c["null"] = null
                yield c
        return
    if op == 20:
        for mode in range(3):
            for al in (0, 1):
                c = gen(op, r)
                c["mode"] = mode
                c["queues"] = {"Allocate": [al]}
                yield c
        return
    if op == 23:
        for v in (0, 1, 0xFFFFFFFF, 0x80000000):
            for pair in ((1, 2), (0xFFFFFFFF, 0x80000000), (0, 0)):
                c = gen(op, r, list(pair))
                c["w"][0] = v
                yield c
        return
    if n == 0:
        for _ in range(4):
            yield gen(op, r)
        return
    for combo in itertools.product(SPEC, repeat=n):
        yield gen(op, r, list(combo))


TESTS = [FnTest(name, ppc, host, edge=(lambda op=op: edges(op)), rnd=(lambda r, op=op: gen(op, r))) for op, name, _, _ in FUNCS]
