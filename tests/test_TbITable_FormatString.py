"""Unit v5032, TbITable::FormatString (the multibyte overload), against libtpw_unit_TbITable_FormatString.dylib
(tests/unit_TbITable_FormatString.cpp), main app, src/TbITable_TbITable_FormatString.cpp:
  TbITable::FormatString 0x10004e30

A case is a table (only its converter at +0x14 is read), a converter or none, an output pointer and size (never dereferenced), an id and
four variable-argument words. Callees outside the unit are fakes on both sides: DoFormat (records the table, the buffer's cursor, end,
converter, overflow flag, id and the four argument words, answers from the queue), the buffer's Close (records its fields) and the
converter's AddRef / Release (virtual calls through __ptr_glue on the PowerPC side). Compared: the answer and the event log.

No fused operations. Not tested: more than four argument words (they would live on the caller's stack).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_TbITable_FormatString"
MASK = 0xFFFFFFFF
PROGRAM = "ltms symbols shared"

FN = 0x10004E30
OWN = {FN}
DO_FORMAT = 0x10004C6C
PTR_GLUE = 0x1000F91C
MARK_REL, MARK_ADD = 0x7A000008, 0x7A00000C

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_do_format(e):
    L = e.cl
    this, buf, ident, args = e.r(3), e.r(4), e.r(5), e.r(6)
    cur, end, conv = e.u32(buf + 4), e.u32(buf + 8), e.u32(buf + 12)
    ovf = 1 if e.read(buf + 16, 1)[0] else 0
    words = [e.u32(args + 4 * i) for i in range(4)]
    L.rec("DoFormat", (L.lab(this), cur, end, L.lab(conv), ovf, ident, *words))
    e.set_r(3, L.ret("DoFormat") & MASK)


def stub_glue(e):
    L = e.cl
    r12, r3 = e.r(12), e.r(3)
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))       # what the real glue saves
    if r12 == MARK_REL:
        L.rec("Release", (L.lab(r3),))
    elif r12 == MARK_ADD:
        L.rec("AddRef", (L.lab(r3),))
    else:
        cur, end, conv = e.u32(r3 + 4), e.u32(r3 + 8), e.u32(r3 + 12)
        L.rec("Close", (cur, end, L.lab(conv), 1 if e.read(r3 + 16, 1)[0] else 0))


STUBS.internal(DO_FORMAT, stub_do_format)
STUBS.internal(PTR_GLUE, stub_glue)


def setup(runner):
    pass


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.table = e.alloc(0x20)
        self.conv = e.alloc(0x10) if case["conv"] else 0
        fixed = {self.table: 2}
        if self.conv:
            fixed[self.conv] = 1
            vt = e.alloc(0x20)
            e.write(self.conv, struct.pack(">I", vt))
            e.write(vt + 8, struct.pack(">II", MARK_REL, MARK_ADD))
        e.write(self.table + 0x14, struct.pack(">I", self.conv))
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def call(self, c):
        r3, _, _ = T.call_ppc(self.e, FN, [self.table, c["out"], c["size"], c["id"]] + c["args"])
        return r3 & MASK

    def log(self):
        return self.L.log

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.table = lib.tpw_ft_alloc(64)
        self.conv = lib.tpw_ft_newconv() if case["conv"] else 0
        fixed = {2: self.table}
        if self.conv:
            fixed[1] = self.conv
        CL.host_begin(lib, fixed, case["queues"])

    def call(self, c):
        return self.lib.tpw_ft_format(self.table, self.conv, c["out"], c["size"], c["id"], *c["args"]) & MASK

    def log(self):
        return CL.host_end(self.lib)

    def done(self):
        pass


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ft_alloc.restype, lib.tpw_ft_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ft_newconv.restype, lib.tpw_ft_newconv.argtypes = C.c_uint64, []
    lib.tpw_ft_format.restype = C.c_int32
    lib.tpw_ft_format.argtypes = [C.c_uint64, C.c_uint64] + [C.c_uint32] * 7
    lib.tpw_ft_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 4)()
    n = lib.tpw_ft_layout(out)
    assert list(out)[:n] == []
    lib.bound = True


def run(s, case):
    try:
        ans = s.call(case)
        return {"answer": ans, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def word(r):
    return r.choice([0, 1, MASK, 0x80000000, r.getrandbits(32), r.getrandbits(32)])


def make(r, conv=None, out=None, size=None):
    return {"conv": r.choice([0, 1]) if conv is None else conv,
            "out": word(r) if out is None else out,
            "size": word(r) if size is None else size,
            "id": word(r),
            "args": [word(r) for _ in range(4)],
            "queues": {"DoFormat": [r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, r.getrandbits(32) - 0x80000000])]}}


def edges():
    r = random.Random(4321)
    for conv in (0, 1):
        for out in (0, 0x1000, 0xFFFFFFF0):
            for size in (0, 1, 0x100, MASK):
                yield make(r, conv, out, size)


def rnd(r):
    return make(r)


TESTS = [FnTest("TbITable::FormatString", ppc, host, edge=edges, rnd=rnd)]
