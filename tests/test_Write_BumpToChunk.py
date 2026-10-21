"""Unit v5247, the advisor chunk writer, against libtpw_unit_Write_BumpToChunk.dylib (tests/unit_Write_BumpToChunk.cpp), main app,
src/game/rides/BumperRide.cpp:
  Write_BumpToChunk 0x10021960, Write_CloseChunk 0x10021e44

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian, host memory little-endian).
Callees outside the unit are fakes on both sides: LbFile_Write (keeps the buffer it was given), LbFile_GetPosition, LbFile_Seek, TRACE,
GlobalFree. The USwap helpers run as the original on the PowerPC side.

The world: a chunk (start, file, tag, sizes), a file object, a bump of 0xac bytes (the last word is read past tagBUMP). Compared: the
answer, the call log, the buffer written (its bytes, as the file gets them), the chunk and bump words. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Write_BumpToChunk"
MASK = 0xFFFFFFFF

FN_BUMP, FN_CLOSE = 0x10021960, 0x10021e44
SIZES = {FN_BUMP: 532, FN_CLOSE: 308}
OWN = set(SIZES)

C_WRITE, C_GETPOS, C_SEEK, C_TRACE, C_FREE = 0x10238878, 0x102398e0, 0x10239b98, 0x1006fcfc, 0x10238b18
HOST_LAYOUT = [4, 0xC, 0x10, 0x14, 0x18]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def stub_write(e):
    """LbFile_Write(result*, file, buffer, length, transferred*) with the TbError returned through r3."""
    L = e.cl
    length = e.r(6) & MASK
    L.rec("LbFile_Write", (L.lab(e.r(4)), length, 1 if e.r(7) else 0))
    buf = e.read(e.r(5), min(length, 0xB0))
    e.wrote = buf + bytes(0xB0 - len(buf))
    if e.r(7):
        e.write(e.r(7), struct.pack(">I", L.ret("xfer") & MASK))
    e.write(e.r(3), struct.pack(">I", L.ret("err") & MASK))


STUBS.imp("LbFile_Write__FPvPCvUlPUl", stub_write)
STUBS.imp("LbFile_GetPosition__FPv", CL.recorder("LbFile_GetPosition", "L", "i"))
STUBS.imp("LbFile_Seek__FPvl14TbFileSeekMode", CL.recorder("LbFile_Seek", "Liu"))
STUBS.internal(C_TRACE, CL.recorder("TRACE", "sii"))
STUBS.imp("GlobalFree__10NS_MacDozeFPQ210NS_MacDoze14InternalHandle", CL.recorder("GlobalFree", "L"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def fileword(self):
        return self.file

    def wordsof(self, b):
        return list(struct.unpack(self.fmt[0] + "%dI" % (len(b) // 4), b))

    def prepare(self):
        self.file = self.alloc(16)
        self.chunk = self.alloc(0x18)
        self.bump = self.alloc(0xB0)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.wrote = bytes(0xB0)
        self.prepare()
        self.L = e.cl = CL.PpcLog({self.file: 1, self.chunk: 2}, case["queues"])
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

    def wrote(self):
        return self.e.wrote

    def call_bump(self):
        r3, _, _ = T.call_ppc(self.e, FN_BUMP, [self.chunk, self.bump])
        return r3 & MASK

    def call_close(self):
        r3, _, _ = T.call_ppc(self.e, FN_CLOSE, [self.chunk])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_wr_clear()
        self.prepare()
        CL.host_begin(lib, {1: self.file, 2: self.chunk}, case["queues"])

    def done(self):
        pass

    def fileword(self):
        return self.lib.tpw_wr_off(self.file)

    def alloc(self, n):
        return self.lib.tpw_wr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def wrote(self):
        return C.string_at(self.lib.tpw_wr_addr(b"wrote"), 0xB0)

    def call_bump(self):
        return self.lib.tpw_wr_bump(self.chunk, self.bump) & MASK

    def call_close(self):
        return self.lib.tpw_wr_close(self.chunk) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_wr_addr.restype, lib.tpw_wr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_wr_alloc.restype, lib.tpw_wr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_wr_off.restype, lib.tpw_wr_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_wr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_wr_bump.restype, lib.tpw_wr_bump.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    lib.tpw_wr_close.restype, lib.tpw_wr_close.argtypes = C.c_int32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_wr_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.chunk, [case["start"], s.fileword(), case["w8"], case["tag"], case["data"], case["phys"]])
    s.words(s.bump, case["bump"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "wrote": list(struct.unpack(">44I", s.wrote())),
            "chunk": s.rd_words(s.chunk, 6)[0:1] + s.rd_words(s.chunk, 6)[2:], "bump": s.rd_words(s.bump, 0x2C)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call_bump() if case["op"] == "bump" else s.call_close())
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    return {"op": op, "start": r.choice([0, 100, 0x7FFFFFFF, 0xFFFFFFF0, r.getrandbits(32)]), "w8": r.getrandbits(32),
            "tag": r.getrandbits(32), "data": r.choice([0, 0xFFFFFFF8, r.randrange(0, 5000)]),
            "phys": r.getrandbits(32), "bump": [r.getrandbits(32) for _ in range(0x2C)],
            "queues": {"xfer": [r.choice([0, 0xAC, r.getrandbits(32)])], "err": [r.choice([0, 1, -1 & MASK, 0x80000000, 5])],
                       "LbFile_GetPosition": [r.choice([0, 1000, -1 & MASK, r.getrandbits(32)])]}}


def edges(op, seed):
    r = random.Random(seed)
    for err in (0, 1, -1 & MASK, 0x80000000, 0x7FFFFFFF):
        c = base(r, op)
        c["queues"]["err"] = [err]
        yield c
    c = base(r, op)
    c["bump"] = [0] * 0x2C
    yield c
    c = base(r, op)
    c["bump"] = [MASK] * 0x2C
    yield c


TESTS = [
    FnTest("Write_BumpToChunk", ppc, host, edge=lambda: edges("bump", 11), rnd=lambda r: base(r, "bump")),
    FnTest("Write_CloseChunk", ppc, host, edge=lambda: edges("close", 12), rnd=lambda r: base(r, "close")),
]
