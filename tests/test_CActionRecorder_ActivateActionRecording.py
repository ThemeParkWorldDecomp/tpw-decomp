"""Unit v51041, CActionRecorder::ActivateActionRecording and the RecordAction / ProcessAction overloads taking a RecordedAction, against
libtpw_unit_CActionRecorder_ActivateActionRecording.dylib (tests/unit_CActionRecorder_ActivateActionRecording.cpp), main app
("SimTheme Park"), src/game/ActionRecorder.cpp:
  ActivateActionRecording 0x10143cf0, RecordAction 0x10143f48 0x10143ff8 0x101440e0 0x101441fc 0x101445d0 0x10144bec, ProcessAction 0x10146850

A case is a recorder object and a call. The object (state 0x00, mode 0x04, recording 0x24) is built twice by the same Python code: in emulator
memory at the Mac's offsets for the original, and in the host library's CActionRecorder for the translation. The callees outside the unit are
fakes on both sides: CTRACE (a format id, the level and the %d arguments), the file's virtual Write (which file, size, a hash of the bytes)
and CMemFile::Open (which file, size).

What is compared: the three words of the object and the event log. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CActionRecorder_ActivateActionRecording"
MASK = 0xFFFFFFFF

FN_ACT, FN_R0, FN_R2, FN_R3, FN_R1, FN_R1B, FN_RP, FN_PA = (0x10143CF0, 0x10143F48, 0x10143FF8, 0x101440E0, 0x101441FC, 0x101445D0,
                                                             0x10144BEC, 0x10146850)
SIZES = {FN_ACT: 184, FN_R0: 108, FN_R2: 160, FN_R3: 208, FN_R1: 200, FN_R1B: 236, FN_RP: 176, FN_PA: 96}
OWN = set(SIZES)
CTRACE = 0x1006FCBC
PTR_GLUE = 0x1023BAA8
OPEN = 0x102398C8

FORMATS = [
    "AR - %d\n",
    "AR - %d (%d, %d)\n",
    "AR - %d (%d, %d, %d)\n",
    "AR - %d (%d)\n",
    "AR - %d (128 byte array)\n",
    "AR replay - %d (128 bytes of nothing)\n",
    "Erroneous stuff going down\n",
]

OBJ = {"base": 0}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ctrace(e):
    fmt = e.cstr(e.r(4))
    if isinstance(fmt, bytes):
        fmt = fmt.decode("latin-1")
    ident = FORMATS.index(fmt) if fmt in FORMATS else 99
    n = fmt.count("%d")
    vals = [ident, e.r(3) & MASK] + [e.r(5 + i) & MASK for i in range(n)]
    e.cl.rec("CTRACE", tuple(vals) if len(vals) > 2 else (vals[0], vals[1], -1))


def stub_write(e):
    """The file's virtual Write through the glue: r3 file, r4 source, r5 size."""
    e.write(e.r(1) + 0x14, (e.r(2) & MASK).to_bytes(4, "big"))     # the glue saves the TOC where the caller reloads it
    size = e.r(5) & MASK
    data = e.read(e.r(4), size)
    h = 0
    for b in data:
        h = (h * 31 + b) & MASK
    e.cl.rec("Write", ((e.r(3) - OBJ["base"] - 0x0C) // 8, size, h))


def stub_open(e):
    """CMemFile::Open: r3 the returned TbError, r4 the file, r5 the size."""
    e.write(e.r(1) + 0x14, (e.r(2) & MASK).to_bytes(4, "big"))
    e.cl.rec("Open", ((e.r(4) - OBJ["base"] - 0x0C) // 8, e.r(5) & MASK))


STUBS.internal(CTRACE, stub_ctrace)
STUBS.internal(PTR_GLUE, stub_write)
STUBS.internal(OPEN, stub_open)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        import struct
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

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

    def make(self, case):
        o = self.e.alloc(0x40)
        OBJ["base"] = o
        self.words(o, [case["state"], case["mode"], 0])
        self.words(o + 0x24, [case["rec"]])
        vt = self.e.alloc(0x40)                     # the file's vtable: slot 0x14 points at a TVector (the glue is stubbed)
        tv = self.e.alloc(8)
        self.words(vt + 0x14, [tv])
        self.words(o + 0x0C, [vt])
        self.o = o

    def obs(self):
        import struct
        w = struct.unpack(">3I", self.read(self.o, 12))
        return [w[0], w[1], struct.unpack(">I", self.read(self.o + 0x24, 4))[0]]

    def data(self, case):
        a = self.alloc(128)
        self.write(a, bytes(case["data"]))
        return a

    def call(self, fn, args):
        T.call_ppc(self.e, fn, [self.o] + [x & MASK for x in args])


HOSTFN = {
    "act": ([C.c_int32], "act"),
    "r0": ([C.c_int32], "r0"),
    "r2": ([C.c_int32, C.c_uint8, C.c_uint8], "r2"),
    "r3": ([C.c_int32, C.c_uint8, C.c_uint8, C.c_uint16], "r3"),
    "r1": ([C.c_int32, C.c_uint16], "r1"),
    "r1b": ([C.c_int32, C.c_uint16, C.c_uint8], "r1b"),
    "rp": ([C.c_int32, C.c_uint64], "rp"),
    "pa": ([C.c_int32, C.c_uint64], "pa"),
}
ADDRS = {"act": FN_ACT, "r0": FN_R0, "r2": FN_R2, "r3": FN_R3, "r1": FN_R1, "r1b": FN_R1B, "rp": FN_RP, "pa": FN_PA}


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_ar_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def make(self, case):
        self.lib.tpw_ar_set(s32(case["state"]), s32(case["mode"]), s32(case["rec"]))

    def obs(self):
        out = (C.c_uint32 * 3)()
        self.lib.tpw_ar_obs(out)
        return list(out)

    def data(self, case):
        a = self.alloc(128)
        self.write(a, bytes(case["data"]))
        return a

    def call(self, fn, args):
        name = {v: k for k, v in ADDRS.items()}[fn]
        getattr(self.lib, "tpw_ar_" + name)(*[(x if name in ("rp", "pa") and i == 1 else x) for i, x in enumerate(args)])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ar_addr.restype, lib.tpw_ar_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ar_alloc.restype, lib.tpw_ar_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ar_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ar_set.argtypes = [C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_ar_obs.argtypes = [C.POINTER(C.c_uint32)]
    for key, (types, name) in HOSTFN.items():
        f = getattr(lib, "tpw_ar_" + name)
        f.restype, f.argtypes = None, types
    out = (C.c_int64 * 32)()
    n = lib.tpw_ar_layout(out)
    assert list(out)[:n] == [16], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.make(case)
        args = list(case["args"])
        if case["op"] == "rp":
            args = [args[0], s.data(case)]
        if case["op"] == "pa":
            args = [args[0], 0]
        if case["op"] in ("r2", "r3", "r1", "r1b", "act", "r0"):
            args = [x & MASK for x in args]
        s.call(ADDRS[case["op"]], args)
        return {"obj": s.obs(), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


ACTIONS = [0, 1, 0x43, 0xE7, 0xFF, 0x100, 0x12345678, 0xFF000000, 0x80000000, 0x01000000, MASK, 0x7FFFFFFF]
U16 = [0, 1, 0xFF, 0x100, 0x1234, 0x8000, 0xFFFF]
U8 = [0, 1, 0x7F, 0x80, 0xFF, 0x42]
KINDS = {"act": "m", "r0": "", "r2": "88", "r3": "88s", "r1": "s", "r1b": "s8", "rp": "", "pa": ""}


def make_case(r, op, state=None, rec=None):
    c = {"op": op, "queues": {}}
    c["state"] = r.choice([0, 1, 1, 1, 2, -1]) if state is None else state
    c["rec"] = r.choice([0, 0, 0, 1, -5, 0x100]) if rec is None else rec
    c["mode"] = r.choice([0, 1, 7, -3])
    args = [r.choice(ACTIONS) if r.random() < 0.6 else r.getrandbits(32)]
    if op == "act":
        args = [r.choice([0, 0, 1, 5, -1, 0x100]) & MASK]
    for k in KINDS[op]:
        if k == "8":
            args.append(r.choice(U8) if r.random() < 0.5 else r.getrandbits(8))
        elif k == "s":
            args.append(r.choice(U16) if r.random() < 0.5 else r.getrandbits(16))
    c["args"] = args
    c["data"] = [r.getrandbits(8) for _ in range(128)]
    return c


def edges_for(op, seed):
    def gen():
        r = random.Random(seed)
        for state in (0, 1, 2, -1):
            for rec in (0, 1, -1):
                for _ in range(3):
                    yield make_case(r, op, state, rec)
    return gen


def rnd_for(op):
    return lambda r: make_case(r, op)


def mk(name, op, seed):
    return FnTest(name, ppc, host, edge=edges_for(op, seed), rnd=rnd_for(op))


TESTS = [
    mk("CActionRecorder::ActivateActionRecording", "act", 1),
    mk("CActionRecorder::RecordAction@0x10143f48", "r0", 2),
    mk("CActionRecorder::RecordAction@0x10143ff8", "r2", 3),
    mk("CActionRecorder::RecordAction@0x101440e0", "r3", 4),
    mk("CActionRecorder::RecordAction@0x101441fc", "r1", 5),
    mk("CActionRecorder::RecordAction@0x101445d0", "r1b", 6),
    mk("CActionRecorder::RecordAction@0x10144bec", "rp", 7),
    mk("CActionRecorder::ProcessAction", "pa", 8),
]
