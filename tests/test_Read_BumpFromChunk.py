"""Unit v51017, the advisor save's chunk reader, against libtpw_unit_Read_BumpFromChunk.dylib (tests/unit_Read_BumpFromChunk.cpp), main app
("SimTheme Park"), src/host_leaves_Write_BumpToChunk_BMP_Save_Read_BumpFromChunk.cpp:
  Read_OpenChunk 0x10021fac, Read_DataFromChunk 0x100220bc, Read_BumpFromChunk 0x100221d4

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory, big-endian, for the original; the host
library's memory for the translation). The callees outside the unit are fakes on both sides that record their arguments: BMPFile_Read
(answer and byte count from the case's queues, the buffer filled with a pattern of the case's seed), LbFile_GetPosition, GlobalAlloc
(answers the block the case made, or 0) and TRACE. The swap helpers (USwap32 ...) run as the original on the PowerPC side: the file bytes
are the same on both sides and the Mac swaps them where the host's words are already in order.

The world of a case: a chunk block (start, file, read position, tag, data size, physical size), a buffer with guard bytes around it, the file
handle. What is compared: the answer, the log, the chunk's words (the file word as "points at the file"), the whole buffer, the out word.

No fused operations. Not tested: nothing is skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Read_BumpFromChunk"
MASK = 0xFFFFFFFF

FN_OPEN, FN_DATA, FN_BUMP = 0x10021FAC, 0x100220BC, 0x100221D4
SIZES = {FN_OPEN: 216, FN_DATA: 220, FN_BUMP: 592,
         0x10021C6C: 40, 0x10021CE0: 40, 0x10021BF8: 40, 0x10021D48: 40, 0x10021DB8: 12, 0x10021DFC: 12, 0x10021BB4: 12}
OWN = set(SIZES)

BMP_READ, GETPOS, GLOBAL_ALLOC, TRACE = 0x1002FFC4, 0x102398E0, 0x10239610, 0x1006FCFC
HOST_LAYOUT = [0x18, 4, 8, 0xC]
BUFSIZE = 0x100


def s32(v):
    return T.s32(v)


def pattern(seed, n):
    return bytes((seed + i * 13) & 0xFF for i in range(n))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_read(e):
    """BMPFile_Read(TbError* hidden r3, file r4, buffer r5, size r6, long* r7)."""
    L = e.cl
    size = e.r(6) & MASK
    L.rec("BMPFile_Read", (L.lab(e.r(4)), size))
    res = L.ret("BMPFile_Read.res")
    n = min(L.ret("BMPFile_Read.n") & MASK, size)
    if res < 0:
        n = 0       # a failed read leaves the buffer alone
    e.write(e.r(5), pattern(L.ret("seed"), n))
    e.write(e.r(7), struct.pack(">I", n))
    e.write(e.r(3), struct.pack(">I", res & MASK))


def stub_memset(e):
    n = e.r(5) & MASK
    e.write(e.r(3), bytes([e.r(4) & 0xFF]) * n)


def toc(fn):
    """The glue of a real call saves the TOC in 0x14(r1) and the original reloads r2 from there: the fake does the save."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", 0x10267CC0))
        return fn(e)
    return wrapped


STUBS.internal(0x10238560, toc(stub_memset))
STUBS.internal(BMP_READ, toc(stub_read))
STUBS.internal(GETPOS, toc(CL.recorder("LbFile_GetPosition", "L", ret="i")))
STUBS.internal(GLOBAL_ALLOC, toc(CL.recorder("GlobalAlloc", "uu", ret="i")))
STUBS.internal(TRACE, toc(CL.recorder("TRACE", "s")))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def make(self, case):
        """The blocks every case has; labels 1 file, 2 chunk, 3 buffer."""
        self.file = self.alloc(16)
        self.chunk = self.alloc(0x18)
        self.buf = self.alloc(BUFSIZE)
        self.out = self.alloc(4)

    def queues(self, case):
        q = dict(case["queues"])
        q["GlobalAlloc"] = [self.chunk if case["alloc_ok"] else 0]
        return q


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make(case)
        self.L = e.cl = CL.PpcLog({self.file: 1, self.chunk: 2, self.buf: 3}, self.queues(case))
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

    def ptrword(self, a):
        return a

    def call(self, fn, args):
        self.e.set_r(2, 0x10267CC0)         # the TOC base: 0x102626d4 (slot -0x55ec) + 0x55ec
        r3, _, _ = T.call_ppc(self.e, fn, args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make(case)
        CL.host_begin(lib, {1: self.file, 2: self.chunk, 3: self.buf}, self.queues(case))

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_rbc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptrword(self, a):
        return self.lib.tpw_rbc_offset(a)

    def call(self, fn, args):
        f = {FN_OPEN: self.lib.tpw_rbc_open, FN_DATA: self.lib.tpw_rbc_data, FN_BUMP: self.lib.tpw_rbc_bump}[fn]
        return f(*args) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rbc_alloc.restype, lib.tpw_rbc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rbc_offset.restype, lib.tpw_rbc_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_rbc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rbc_open.restype, lib.tpw_rbc_open.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    lib.tpw_rbc_data.restype, lib.tpw_rbc_data.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64, C.c_uint32]
    lib.tpw_rbc_bump.restype, lib.tpw_rbc_bump.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rbc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.chunk, [case["start"], s.ptrword(s.file), case["pos"], case["tag"], case["dsize"], case["psize"]])
    s.write(s.buf, bytes([0xA5]) * BUFSIZE)
    s.write(s.out, b"\0\0\0\0")


def execute(s, case):
    op = case["op"]
    if op == "open":
        return s.call(FN_OPEN, [s.file, s.out])
    if op == "data":
        return s.call(FN_DATA, [s.chunk, s.buf + 8, case["size"]])
    return s.call(FN_BUMP, [s.chunk, s.buf + 8])


def observe(s, case, answer):
    w = s.rd_words(s.chunk, 6)
    chunk = [w[0]] + w[2:]
    if case["op"] == "open" and case["alloc_ok"] and case["queues"]["BMPFile_Read.res"][0] < 0 and s.fmt == ">I":
        # A failed read leaves the constant 0xc in the data size, which the Mac then byte swaps with the file's words (0x0c000000);
        # the host's swap is the identity, so the Mac's word is swapped back to compare the values.
        chunk[3] = struct.unpack("<I", struct.pack(">I", chunk[3]))[0]
    # The Mac swaps the words it read (the bump's block): compare words, as values, in each side's own order.
    buf = s.rd_words(s.buf, BUFSIZE // 4) if case["op"] == "bump" else s.read(s.buf, BUFSIZE)
    return {"answer": answer, "log": s.log(), "chunk": chunk, "file_ok": w[1] == s.ptrword(s.file),
            "buf": buf, "out_ok": s.rd_words(s.out, 1)[0] == s.ptrword(s.chunk),
            "out_zero": s.rd_words(s.out, 1)[0] == 0}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op, size=None, remaining=None, n=None, res=None):
    size = r.choice([0, 1, 3, 4, 12, 0x10, 0xAC, r.randrange(0, 0x90)]) if size is None else size
    pos = r.randrange(0, 200)
    if remaining is None:
        remaining = r.choice([0, 1, size, size - 1, size + 1, 0xAB, 0xAC, 0xAD, 0x10, 0xC, -1, -5, r.randrange(0, 400)])
    if n is None:
        n = r.choice([0, size, size - 1, 1, 0xAC, r.randrange(0, 0xB0)]) & MASK
    if res is None:
        res = r.choice([0, 0, 0, 1, -1, -7])
    # an opened chunk is a zeroed GlobalAlloc block (the Mac swaps whatever the read leaves in it)
    zero = op == "open"
    return {"op": op, "size": size, "start": r.getrandbits(32), "pos": pos, "tag": 0 if zero else r.getrandbits(32),
            "psize": 0 if zero else r.getrandbits(32),
            "dsize": (pos + remaining) & MASK, "alloc_ok": r.random() < 0.8,
            "queues": {"BMPFile_Read.res": [res], "BMPFile_Read.n": [n], "seed": [r.randrange(256)],
                       "LbFile_GetPosition": [r.choice([0, 5, -1, r.randrange(1 << 20)])]}}


def edges(op):
    def gen():
        r = random.Random({"open": 11, "data": 12, "bump": 13}[op])
        for size in (0, 1, 5, 0x10):
            for remaining in (0, size - 1, size, size + 1, -1):
                for res in (0, -1):
                    for n in (0, size, size - 1):
                        yield base(r, op, size, remaining, n, res)
        for remaining in (0xAB, 0xAC, 0xAD):
            for n in (0xAC, 0x40, 0):
                for res in (0, -1):
                    yield base(r, op, 0xAC, remaining, n, res)
        c = base(r, op, 12, 12, 12, 0)
        c["alloc_ok"] = False
        yield c
    return gen


def rnd(op):
    return lambda r: base(r, op)


TESTS = [
    FnTest("Read_OpenChunk", ppc, host, edge=edges("open"), rnd=rnd("open")),
    FnTest("Read_DataFromChunk", ppc, host, edge=edges("data"), rnd=rnd("data")),
    FnTest("Read_BumpFromChunk", ppc, host, edge=edges("bump"), rnd=rnd("bump")),
]
