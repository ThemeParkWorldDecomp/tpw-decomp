"""Unit v5649, LOADVARSTRUCT<AdvisorHistory> 0x1000ecb4, against libtpw_unit_LOADVARSTRUCT_2.dylib (tests/unit_LOADVARSTRUCT_2.cpp), main app
("SimTheme Park"), src/game/advisor/Advisor.cpp.

A case is a world and a call. The callees outside the unit are fakes on both sides that record their arguments: WriteSerialiseLog
(this, size, name, state), LbFile_Read (file, length; it writes 16 file bytes, answers a TbError and a count from the case's queues) and
GAME_ASSERTMSG (condition, text).

The world of a case: the 16 bytes the read delivers, the error code, the count read, noLog. What is compared: the answer, the 16 bytes of
the record (words 0, 1 and 3 as numbers after the swap; word 2 and the failure case as raw bytes), and the event log.

No fused operations. Not tested: the TbError constant word (0x1027dee4, a zero nothing writes).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LOADVARSTRUCT_2"
MASK = 0xFFFFFFFF

FN = 0x1000ECB4
OWN = {FN}
GSAVELOAD = 0x1037FEFC
FILE = 0x5000

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_read(e):
    L = e.cl
    L.rec("LbFile_Read", (L.lab(e.r(4)), e.r(6) & MASK))
    data = b"".join((L.ret("w%d" % i) & MASK).to_bytes(4, "big") for i in range(4))
    e.write(e.r(5), data)
    e.write(e.r(7), (L.ret("cnt") & MASK).to_bytes(4, "big"))
    e.write(e.r(3), (L.ret("err") & MASK).to_bytes(4, "big"))


STUBS.imp("LbFile_Read__FPvPvUlPUl", stub_read)
STUBS.internal(0x1015B3DC, CL.recorder("WriteSerialiseLog", "Lisi"))
STUBS.internal(0x1000DDE8, CL.recorder("assert", "is"))


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
        self.L = e.cl = CL.PpcLog({GSAVELOAD: 1}, case["queues"])
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

    def call(self, value, name, nolog):
        r3, _, _ = T.call_ppc(self.e, FN, [FILE, value, name, nolog & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: lib.tpw_lvs_addr(b"gSaveLoad")}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_lvs_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, value, name, nolog):
        return self.lib.tpw_lvs_load(FILE, value, name, T.s32(nolog)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lvs_addr.restype, lib.tpw_lvs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lvs_alloc.restype, lib.tpw_lvs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lvs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lvs_load.restype = C.c_int32
    lib.tpw_lvs_load.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lvs_layout(out)
    assert list(out)[:n] == [16, 4, 12], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        value = s.alloc(16)
        s.words(value, case["init"])
        name = s.alloc(16)
        s.write(name, b"mSoundId\0")
        ans = s.call(value, name, case["nolog"])
        if ans == 0:
            w = s.rd_words(value, 4)
            rec = [w[0], w[1], s.read(value + 8, 4), w[3]]
        else:
            rec = s.read(value, 16)
        return {"answer": ans, "log": s.log(), "rec": rec}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, err=None, cnt=None, nolog=None):
    err = r.choice([0, 1, 0x7FFFFFFF, MASK, 0x80000000, r.randrange(0, 1 << 32)]) if err is None else err
    cnt = r.choice([16, 16, 16, 0, 15, 17, r.randrange(0, 1 << 32)]) if cnt is None else cnt
    q = {"w%d" % i: [r.randrange(0, 1 << 32)] for i in range(4)}
    q["err"] = [err & MASK]
    q["cnt"] = [cnt & MASK]
    return {"queues": q, "init": [r.randrange(0, 1 << 32) for _ in range(4)],
            "nolog": r.choice([0, 1, 2, MASK]) if nolog is None else nolog}


def edges():
    r = random.Random(5649)
    for err in (0, 1, 0x7FFFFFFF, MASK, 0x80000000):
        for cnt in (16, 0, 15, 17, MASK):
            for nolog in (0, 1):
                yield mk(r, err, cnt, nolog)


def rnd(r):
    return mk(r)


TESTS = [
    FnTest("LOADVARSTRUCT<AdvisorHistory>", ppc, host, edge=edges, rnd=rnd),
]
