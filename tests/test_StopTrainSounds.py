"""Unit v5728, StopTrainSounds and RestoreCoasterSounds, against libtpw_unit_StopTrainSounds.dylib (tests/unit_StopTrainSounds.cpp), main app
("SimTheme Park"), src/TPConfig_bend_StopTrainSounds.cpp:
  StopTrainSounds 0x100471bc, RestoreCoasterSounds 0x100472c4

A case is a world and a call. The world is built twice by one piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation (the host structs have 64-bit pointers, so offsets and the train
stride come from the library's layout export). SOUND_StopSoundEffect, SCREAM_Stop and RSSE_GetSoundEventCode are fakes on both sides that
record their arguments; RSSE answers from a queue per kind.

The world of a case: gActionRecorder.mRecording, a coaster (script id, train count, trains) and trains with random sound words. What is
compared: the log, every train's sound words 0x54..0x70. No fused operations. Not tested: a NULL train for StopTrainSounds (read
unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_StopTrainSounds"
MASK = 0xFFFFFFFF

FN_STOP, FN_RESTORE = 0x100471BC, 0x100472C4
SIZES = {FN_STOP: 184, FN_RESTORE: 220}
OWN = set(SIZES)

SOUND_STOP, SCREAM_STOP, RSSE = 0x100DE058, 0x100CCD98, 0x100D6D04
ADDR = dict(rec=0x1037EC44 + 0x24)
WORDS = (0x54, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x6C, 0x70)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def stub_stop(e):
    e.cl.rec("SOUND_StopSoundEffect", (s32(e.r(3)),))


def stub_scream(e):
    e.cl.rec("SCREAM_Stop", (s32(e.r(3)),))


def stub_rsse(e):
    L = e.cl
    b = s32(e.r(4))
    L.rec("RSSE_GetSoundEventCode", (s32(e.r(3)), b))
    e.set_r(3, L.ret("RSSE:%d" % b) & MASK)


STUBS.internal(SOUND_STOP, stub_stop)
STUBS.internal(SCREAM_STOP, stub_scream)
STUBS.internal(RSSE, stub_rsse)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def tw(self, train, mac):
        return train + self.toff(mac)


class Ppc(Side):
    fmt = ">I"
    stride = 0x80

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def coff(self, mac):
        return mac

    def toff(self, mac):
        return mac

    def ptr(self, a, v):
        self.words(a, [v])

    def stop(self, inst, train):
        T.call_ppc(self.e, FN_STOP, [inst, train])

    def restore(self, inst):
        T.call_ppc(self.e, FN_RESTORE, [inst])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.lay = lib.lay
        self.stride = self.lay[4]
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_stsn_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_stsn_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def coff(self, mac):
        return {0x24: self.lay[0], 0xF4: self.lay[1], 0x128: self.lay[2]}[mac]

    def toff(self, mac):
        return self.lay[3] + mac - 0x54

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def stop(self, inst, train):
        self.lib.tpw_stsn_stop(inst, train)

    def restore(self, inst):
        self.lib.tpw_stsn_restore(inst)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_stsn_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "stop").argtypes = [C.c_uint64, C.c_uint64]
    getattr(lib, p + "restore").argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    lib.lay = list(out)[:n]
    # the sound words keep the Mac's distances: mF54..mF70 contiguous
    assert lib.lay[5] == 0x10 and lib.lay[6] == 0x1C, lib.lay
    lib.bound = True


def build(s, case):
    s.words(s.g("rec"), [case["rec"]])
    inst = s.alloc(0x400)
    s.words(inst + s.coff(0x24), [case["script"]])
    s.words(inst + s.coff(0xF4), [case["n"]])
    trains = s.alloc(s.stride * max(1, len(case["trains"])))
    s.ptr(inst + s.coff(0x128), trains)
    for i, ws in enumerate(case["trains"]):
        t = trains + i * s.stride
        for m, w in zip(WORDS, ws):
            s.words(t + s.toff(m), [w])
    return inst, trains


def run(s, case):
    try:
        inst, trains = build(s, case)
        if case["op"] == "stop":
            s.stop(inst, trains)
        elif case["null"]:
            s.restore(0)
        else:
            s.restore(inst)
        out = {"log": s.log()}
        for i in range(len(case["trains"])):
            t = trains + i * s.stride
            out["t%d" % i] = [s.rd_word(t + s.toff(m)) for m in WORDS]
        return out
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def word(r):
    return r.choice([0, 0, r.randrange(1, 1000), r.randrange(0x80000000, 0x100000000), 1])


def base(r, op, rec=None, n=None, null=False):
    if n is None:
        n = 1 if op == "stop" else r.choice([0, 1, 2, 3, 5])
    trains = [[word(r) for _ in WORDS] for _ in range(max(n, 1) if op == "stop" else n)]
    return {"op": op, "null": null, "rec": r.choice([0, 0, 1, 5]) if rec is None else rec, "script": r.randrange(-5, 100) & MASK,
            "n": n, "trains": trains,
            "queues": {"RSSE:%d" % k: [r.choice([0, r.randrange(1, 5000)])] for k in (5, 6, 7, 8)}}


def stop_edges():
    r = random.Random(11)
    for rec in (0, 1, 0xFFFFFFFF):
        for mask in range(8):
            c = base(r, "stop", rec=rec)
            for bit, idx in enumerate((4, 5, 7)):
                c["trains"][0][idx] = 0x77 + idx if mask >> bit & 1 else 0
            yield c


def stop_random(r):
    return base(r, "stop")


def restore_edges():
    r = random.Random(12)
    yield base(r, "restore", null=True)
    yield base(r, "restore", n=0)
    for n in (1, 2, 4):
        for mask in range(16):
            c = base(r, "restore", n=n)
            for bit in range(4):
                c["trains"][0][bit] = 0x10 + bit if mask >> bit & 1 else 0
            c["queues"] = {"RSSE:%d" % k: [1000 + k] for k in (5, 6, 7, 8)}
            yield c


def restore_random(r):
    return base(r, "restore", null=r.random() < 0.05)


TESTS = [
    FnTest("StopTrainSounds", ppc, host, edge=stop_edges, rnd=stop_random),
    FnTest("RestoreCoasterSounds", ppc, host, edge=restore_edges, rnd=restore_random),
]
