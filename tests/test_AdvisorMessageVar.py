"""Unit v5L05, the save-file instances of AdvisorMessage, against libtpw_unit_AdvisorMessageVar.dylib (tests/unit_AdvisorMessageVar.cpp),
main app: LOADVAR<AdvisorMessage> 0x1000f250, SAVEVAR<AdvisorMessage> 0x1000f820

A case is a world and a call, built by one piece of Python code on both sides. The callees outside the unit (WriteSerialiseLog,
GAME_ASSERTMSG, LbFile_Read, LbFile_Write) are fakes on both sides that record their arguments and answer from the case's queues
(read/write status, transferred count, the word the read delivers).

The world of a case: a file handle, the value word, the name, the no-log flag, the fakes' answers. What is compared: the answer, the
value word and the whole event log (the word written included).

No fused operations. Not tested: nothing left out.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_AdvisorMessageVar"
MASK = 0xFFFFFFFF

F_LOAD, F_SAVE = 0x1000f250, 0x1000f820
ENTRY = [F_LOAD, F_SAVE]
SIZES = {F_LOAD: 236, F_SAVE: 260}
OWN = set(SIZES)

ADDR = dict(gSaveLoad=0x1037fefc)


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lb_read(e):
    L = e.cl
    n = e.r(6) & MASK
    L.rec("LbFile_Read", (L.lab(e.r(4)), n))
    status = s32(L.ret("readerr"))
    count = L.ret("readn") & MASK
    data = struct.pack("<I", L.ret("rd0") & MASK)
    e.write(e.r(5), data[:min(n, 4)])
    e.write(e.r(3), struct.pack(">i", status))
    e.write(e.r(7), struct.pack(">I", count))


def lb_write(e):
    L = e.cl
    n = e.r(6) & MASK
    data = e.read(e.r(5), min(n, 4))
    word = struct.unpack("<I", (data + b"\0" * 4)[:4])[0]
    L.rec("LbFile_Write", (L.lab(e.r(4)), n, s32(word)))
    status = s32(L.ret("wrerr"))
    e.write(e.r(3), struct.pack(">i", status))
    e.write(e.r(7), struct.pack(">I", L.ret("wrn") & MASK))


STUBS.internal(0x1015b3dc, CL.recorder("WriteSerialiseLog", "Lisi"))
STUBS.internal(0x1000dde8, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.imp("LbFile_Read__FPvPvUlPUl", lb_read)
STUBS.imp("LbFile_Write__FPvPCvUlPUl", lb_write)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def blocks(self):
        self.file = self.alloc(16)
        self.val = self.alloc(16)
        self.nm = self.alloc(32)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.blocks()
        self.L = e.cl = CL.PpcLog({ADDR["gSaveLoad"]: 1, self.file: 2}, case["queues"])
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

    def call(self, op, d):
        r3, _, _ = T.call_ppc(self.e, ENTRY[op], [self.file, self.val, self.nm, d & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blocks()
        CL.host_begin(lib, {1: self.g("gSaveLoad"), 2: self.file}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_amv_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_amv_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, d):
        return self.lib.tpw_amv_call(op, self.file, self.val, self.nm, s32(d)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_amv_addr.restype, lib.tpw_amv_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_amv_alloc.restype, lib.tpw_amv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_amv_call.restype = C.c_uint32
    lib.tpw_amv_call.argtypes = [C.c_int32, C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32]
    lib.bound = True


def build(s, case):
    s.write(s.nm, case["name"].encode() + b"\0")
    s.words(s.val, [case["val"]])


def observe(s, case, answer):
    if case["op"] == 0 and answer:
        # a failed read leaves the file's raw bytes; a good one has the swapped word as a value
        val = list(struct.unpack("<I", s.read(s.val, 4)))
    else:
        val = s.rd_words(s.val, 1)
    return {"answer": answer, "log": s.log(), "val": val}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], case["nolog"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


NAMES = ["", "x", "mMessage", "a longer name for the log"]
STATUS = [0, -1, 1, 0x7FFFFFFF, -0x80000000]


def make(r, op, nolog=None, err=None, cnt=None):
    val = r.getrandbits(32) if r.random() < 0.7 else r.choice([0, MASK, 0x80000000, 1, 0x01020304, 350])
    return {
        "op": op,
        "name": r.choice(NAMES),
        "nolog": r.choice([0, 1, -1, 7]) if nolog is None else nolog,
        "val": val,
        "queues": {
            "readerr": [r.choice(STATUS) if err is None else err],
            "readn": [r.choice([4, 4, 3, 5, 0, MASK]) if cnt is None else cnt],
            "rd0": [r.getrandbits(32)],
            "wrerr": [r.choice(STATUS) if err is None else err],
            "wrn": [r.choice([4, 4, 3, 5, 0, MASK]) if cnt is None else cnt],
        },
    }


def edges(op):
    def gen():
        r = random.Random(500 + op)
        for err in (0, -1, 1, 0x7FFFFFFF, -0x80000000):
            for cnt in (4, 3, 5, 0):
                for nolog in (0, 1):
                    yield make(r, op, nolog, err, cnt)
    return gen


def rnd(op):
    return lambda r: make(r, op)


TESTS = [FnTest("LOADVAR<AdvisorMessage>", ppc, host, edge=edges(0), rnd=rnd(0)),
         FnTest("SAVEVAR<AdvisorMessage>", ppc, host, edge=edges(1), rnd=rnd(1))]
