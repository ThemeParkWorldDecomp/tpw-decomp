"""Unit v51002, the save-file instances and the price per use, against libtpw_unit_SAVEVARSTRUCT_3.dylib (tests/unit_SAVEVARSTRUCT_3.cpp), main app:
  SAVEVARSTRUCT<CStaffHQ::StrikeType> 0x1012c3d4, LOADVARSTRUCT<CStaffHQ::StrikeType> 0x1012c27c, LOADVAR<ResearchCat> 0x10124554,
  SAVEVAR<CPerson::PersonState> 0x1011b90c, CPerson::PayPricePerUseForObject 0x1011a4ac

A case is a world and a call, built by one piece of Python code on both sides. The callees outside the unit (WriteSerialiseLog,
GAME_ASSERTMSG, LbFile_Read, LbFile_Write, GetPricePerUse, RecordNewTakings, PlaySound) are fakes on both sides that record their arguments
and answer from the case's queues (read/write status, transferred count, the bytes the read delivers).

The world of a case: a file handle, the value block (three words), the name, the no-log flag, the fakes' answers; for the payment the
person's +0x1a0 word, the price and the sound category at 0x102f7c94 + 4. What is compared: the answer, the value block, the person's
word and the whole event log (the bytes written included).

No fused operations. Not tested: nothing left out.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SAVEVARSTRUCT_3"
MASK = 0xFFFFFFFF

F_SAVE_STRIKE, F_LOAD_STRIKE, F_LOAD_CAT, F_SAVE_STATE, F_PAY = 0x1012c3d4, 0x1012c27c, 0x10124554, 0x1011b90c, 0x1011a4ac
ENTRY = [F_SAVE_STRIKE, F_LOAD_STRIKE, F_LOAD_CAT, F_SAVE_STATE, F_PAY]
SIZES = {F_SAVE_STRIKE: 264, F_LOAD_STRIKE: 252, F_LOAD_CAT: 236, F_SAVE_STATE: 260, F_PAY: 132}
OWN = set(SIZES)
BYTES = [12, 12, 4, 4]

ADDR = dict(gSaveLoad=0x1037fefc, banks=0x102f7c94)
HOST_LAYOUT = [12, 8]


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
    data = b"".join(struct.pack("<I", L.ret("rd%d" % i) & MASK) for i in range(3))
    e.write(e.r(5), data[:min(n, 12)])
    e.write(e.r(3), struct.pack(">i", status))
    e.write(e.r(7), struct.pack(">I", count))


def lb_write(e):
    L = e.cl
    n = e.r(6) & MASK
    data = e.read(e.r(5), min(n, 12))
    words = struct.unpack("<3I", (data + b"\0" * 12)[:12])
    L.rec("LbFile_Write", (L.lab(e.r(4)), n) + tuple(s32(w) for w in words))
    status = s32(L.ret("wrerr"))
    e.write(e.r(3), struct.pack(">i", status))
    e.write(e.r(7), struct.pack(">I", L.ret("wrn") & MASK))


STUBS.internal(0x1015b3dc, CL.recorder("WriteSerialiseLog", "Lisi"))
STUBS.internal(0x1000dde8, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.imp("LbFile_Read__FPvPvUlPUl", lb_read)
STUBS.imp("LbFile_Write__FPvPCvUlPUl", lb_write)
STUBS.internal(0x1010f4f4, CL.recorder("GetPricePerUse", "L", "i"))
STUBS.internal(0x1010ef54, CL.recorder("RecordNewTakings", "Lu"))
STUBS.internal(0x10115b80, CL.recorder("PlaySound", "Lii"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def blocks(self):
        self.file = self.alloc(16)
        self.val = self.alloc(32)
        self.nm = self.alloc(32)
        self.person = self.alloc(0x240)
        self.obj = self.alloc(0x40)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.blocks()
        self.L = e.cl = CL.PpcLog({ADDR["gSaveLoad"]: 1, self.file: 2, self.person: 3, self.obj: 4}, case["queues"])
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
        if op == 4:
            args = [self.person, self.obj]
        else:
            args = [self.file, self.val, self.nm, d & MASK]
        r3, _, _ = T.call_ppc(self.e, ENTRY[op], args)
        return r3 & MASK if op != 4 else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blocks()
        CL.host_begin(lib, {1: self.g("gSaveLoad"), 2: self.file, 3: self.person, 4: self.obj}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sv3_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sv3_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, d):
        if op == 4:
            return self.lib.tpw_sv3_call(op, self.person, self.obj, 0, 0) & MASK
        return self.lib.tpw_sv3_call(op, self.file, self.val, self.nm, s32(d)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_sv3_"
    lib.tpw_sv3_addr.restype, lib.tpw_sv3_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sv3_alloc.restype, lib.tpw_sv3_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sv3_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sv3_call.restype = C.c_uint32
    lib.tpw_sv3_call.argtypes = [C.c_int32, C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sv3_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.write(s.nm, case["name"].encode() + b"\0")
    if case["op"] < 2:
        # the first word of a strike record is moved as raw bytes by the original (no swap): the same bytes on both sides
        s.write(s.val, struct.pack("<I", case["val"][0] & MASK))
        s.words(s.val + 4, case["val"][1:])
    else:
        s.words(s.val, case["val"])
    s.words(s.person + 0x1a0, [case["money"]])
    s.words(s.g("banks"), [case["bank0"], case["bank1"]])


def observe(s, case, answer):
    op = case["op"]
    if op == 0:
        val = list(struct.unpack("<I", s.read(s.val, 4))) + s.rd_words(s.val + 4, 2)
    elif op == 1:
        # a failed read leaves the file's raw bytes; a good one has the two swapped words as values
        val = list(struct.unpack("<3I", s.read(s.val, 12))) if answer else list(struct.unpack("<I", s.read(s.val, 4))) + s.rd_words(s.val + 4, 2)
    elif op == 2 and answer:
        val = list(struct.unpack("<I", s.read(s.val, 4)))
    else:
        val = s.rd_words(s.val, 3)
    return {"answer": answer, "log": s.log(), "val": val, "money": s.rd_word(s.person + 0x1a0)}


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


NAMES = ["", "x", "mNumProjects", "a longer name for the log"]
STATUS = [0, -1, 1, 0x7FFFFFFF, -0x80000000]


def make(r, op, nolog=None, err=None, cnt=None, val=None):
    n = BYTES[op] if op < 4 else 0
    words = [r.getrandbits(32) for _ in range(3)]
    if r.random() < 0.2:
        words = [r.choice([0, MASK, 0x80000000, 1, 0x01020304]) for _ in range(3)]
    return {
        "op": op,
        "name": r.choice(NAMES),
        "nolog": r.choice([0, 1, -1, 7]) if nolog is None else nolog,
        "val": words if val is None else val,
        "money": r.choice([0, 5, MASK, 0x80000000, r.getrandbits(32)]),
        "bank0": r.getrandbits(32), "bank1": r.choice([0, 3, MASK, r.getrandbits(32)]),
        "queues": {
            "readerr": [r.choice(STATUS) if err is None else err],
            "readn": [r.choice([n, n, n - 1, n + 1, 0, MASK]) if cnt is None else cnt],
            "rd0": [r.getrandbits(32)], "rd1": [r.getrandbits(32)], "rd2": [r.getrandbits(32)],
            "wrerr": [r.choice(STATUS) if err is None else err],
            "wrn": [r.choice([n, n, n - 1, n + 1, 0, MASK]) if cnt is None else cnt],
            "GetPricePerUse": [r.choice([0, 1, 7, 1000, 0x80000000, MASK, r.getrandbits(32)])],
        },
    }


def edges(op):
    def gen():
        r = random.Random(100 + op)
        n = BYTES[op] if op < 4 else 0
        if op == 4:
            for price in (0, 1, 5, 0x7FFFFFFF, 0x80000000, MASK):
                c = make(r, op)
                c["queues"]["GetPricePerUse"] = [price]
                yield c
            return
        for err in (0, -1, 1, 0x7FFFFFFF, -0x80000000):
            for cnt in (n, n - 1, n + 1, 0):
                for nolog in (0, 1):
                    yield make(r, op, nolog, err, cnt)
    return gen


def rnd(op):
    return lambda r: make(r, op)


NAMES_T = ["SAVEVARSTRUCT<CStaffHQ::StrikeType>", "LOADVARSTRUCT<CStaffHQ::StrikeType>", "LOADVAR<ResearchCat>",
           "SAVEVAR<CPerson::PersonState>", "CPerson::PayPricePerUseForObject"]
TESTS = [FnTest(NAMES_T[i], ppc, host, edge=edges(i), rnd=rnd(i)) for i in range(5)]
