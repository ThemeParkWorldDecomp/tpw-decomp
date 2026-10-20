"""Unit v5077, CActionRecorder::ReadAndProcessNextAction, against libtpw_unit_CActionRecorder_ReadAndProcessNextAction.dylib
(tests/unit_CActionRecorder_ReadAndProcessNextAction.cpp), main app ("SimTheme Park"),
src/ActionRecorder_CActionRecorder_ReadAndProcessNextAction.cpp:
  CActionRecorder::ReadAndProcessNextAction 0x101453a8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. The callees outside the unit are fakes on both sides: the action
file's Read (through __ptr_glue on the Mac; a byte stream the case supplies, the answer from the queue "Read": 0 the size, 1 size-1,
2 size+1, 3 zero, else -1), the twelve ProcessAction overloads (name, whether this is the recorder, every argument; the text and the
128-byte buffer as a hash of their bytes), CTRACE and SamsUtilities::_USwapBlock16 (swaps the bytes of each word in place).

The world of a case: the recorder object with its 256-word action-kind table and the stream. What is compared: the answer, the event
log and the table. No fused operations. Not tested: a wide-text count above 40 (kind 9) or 72 (kind 10), where the original overruns its
stack buffer into its own locals and caller frame; the host holds 255 characters.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CActionRecorder_ReadAndProcessNextAction"
MASK = 0xFFFFFFFF

FN = 0x101453A8
OWN = {FN: 1856}

GLUE = 0x1023BAA8
SWAP = 0x102398B0
CTRACE = 0x1006FCBC
PA = {
    0x10145B28: ("PA", ""),
    0x10145D34: ("PA_bbh", "bbh"),
    0x10145C24: ("PA_bb", "bb"),
    0x10145E38: ("PA_h", "h"),
    0x10146058: ("PA_hb", "hb"),
    0x10146140: ("PA_fh", "fh"),
    0x10146248: ("PA_ffh", "ffh"),
    0x1014634C: ("PA_hh", "hh"),
    0x1014644C: ("PA_hhh", "hhh"),
    0x10146524: ("PA_bw", "bw"),
    0x101465A8: ("PA_hbw", "hbw"),
    0x10146850: ("PA_P", "P"),
}
MAC_FILE, MAC_TABLE, MAC_SIZE = 0x0C, 0x34, 0x34 + 0x400
HOST_LAYOUT = [0x10, 0x50, 0x450]
HL = {}

# which operand layout each switch index reads (filled from the original's jump table)
KINDS = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


def hsh(data):
    h = 17
    for b in data:
        h = (h * 31 + b) & 0x7FFFFFFF
    return h


def read_ret(v, size):
    if v == 0:
        return size
    if v == 1:
        return size - 1
    if v == 2:
        return size + 1
    if v == 3:
        return 0
    return -1


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def save_toc(e):
    """The glue saves the caller's TOC pointer at 20(r1); the original reloads r2 from there after the call."""
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))


def stub_read(e):
    L = e.cl
    dest, size = e.r(4) & MASK, e.r(5) & MASK
    L.rec("Read", (size,))
    save_toc(e)
    out = bytes((L.stream[L.cur + i] if L.cur + i < len(L.stream) else 0) for i in range(size))
    L.cur += size
    if size:
        e.write(dest, out)
    e.set_r(3, read_ret(s32(L.ret("Read")), size) & MASK)


def stub_swap(e):
    p, n = e.r(3) & MASK, s32(e.r(4))
    save_toc(e)
    e.cl.rec("Swap16", (n,))
    d = bytearray(e.read(p, 2 * n))
    for i in range(n):
        d[2 * i], d[2 * i + 1] = d[2 * i + 1], d[2 * i]
    e.write(p, bytes(d))


def make_pa(name, sig):
    def fn(e):
        L = e.cl
        vals = [1 if (e.r(3) & MASK) == L.obj else 0, e.r(4) & MASK]
        gpr, fpr = 5, 1
        count = 0
        for t in sig:
            if t == "b":
                count = e.r(gpr) & 0xFF
                vals.append(count)
                gpr += 1
            elif t == "h":
                vals.append(e.r(gpr) & 0xFFFF)
                gpr += 1
            elif t == "f":
                vals.append(canon_f(fb(e.f(fpr))))
                fpr += 1
                gpr += 1
            elif t == "w":
                vals.append(hsh(e.read(e.r(gpr) & MASK, count * 2)))
            elif t == "P":
                vals.append(hsh(e.read(e.r(gpr) & MASK, 128)))
        L.rec(name, tuple(vals))
    return fn


STUBS.internal(GLUE, stub_read)
STUBS.internal(SWAP, stub_swap)
STUBS.internal(CTRACE, CL.recorder("CTRACE", "us"))
for _a, (_n, _s) in PA.items():
    STUBS.internal(_a, make_pa(_n, _s))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    o_file, o_table, size = MAC_FILE, MAC_TABLE, MAC_SIZE

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.stream = case["data"]
        self.L.cur = 0
        self.L.obj = 0
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

    def prepare(self, obj, case):
        self.L.obj = obj
        self.words(obj + self.o_file, [self.alloc(0x40)])        # the vtable pointer the call through the glue loads

    def call(self, obj, case):
        r3, _, _ = T.call_ppc(self.e, FN, [obj & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.o_file, self.o_table, self.size = HL["file"], HL["table"], HL["size"]
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_rec_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def prepare(self, obj, case):
        pass

    def call(self, obj, case):
        d = case["data"]
        buf = self.alloc(len(d) + 1)
        self.write(buf, d)
        return self.lib.tpw_rec_run(obj, buf, len(d)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rec_alloc.restype, lib.tpw_rec_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rec_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rec_run.restype, lib.tpw_rec_run.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rec_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    HL.update(file=out[0], table=out[1], size=out[2])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.obj = s.alloc(s.size)
    s.words(s.obj + s.o_table, case["table"])
    s.prepare(s.obj, case)


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "table": s.rd_words(s.obj + s.o_table, 256)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(s.obj, case))
    finally:
        s.done()


_printed = []


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000, 0x7F800001]
NOPS = {0: 2, 1: 1, 2: 3, 3: 2, 4: 2, 5: 3, 6: 2, 7: 3, 8: 0, 9: 2, 10: 3, 11: 1}


def mk_data(r, act, kind, count=None):
    d = bytearray(r.randrange(256) for _ in range(300))
    d[0] = act
    if kind in (4, 5):
        for off in (1, 5)[: kind - 3]:
            if r.random() < 0.4:
                d[off:off + 4] = struct.pack("<I", r.choice(SPECIAL))
    if kind == 9:
        d[1] = r.choice([0, 1, 2, 40, r.randrange(0, 41)]) if count is None else count
    if kind == 10:
        d[3] = r.choice([0, 1, 2, 72, r.randrange(0, 73)]) if count is None else count
    return bytes(d)


def mk_table(r, act, kind):
    t = [r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 11, 12, 13, 0x100, 0xFFFFFFFF]) for _ in range(256)]
    t[act] = kind
    return t


def mk_case(r, act=None, kind=None, fail=None, count=None):
    act = r.randrange(256) if act is None else act
    if kind is None:
        kind = r.choice(list(range(12)) * 3 + [12, 13, 0x100, 0xFFFFFFFF, 0x80000000])
    if fail is None:
        reads = [0]
    else:
        pos, how = fail
        reads = [0] * pos + [how]
    return {"queues": {"Read": reads}, "table": mk_table(r, act, kind), "data": mk_data(r, act, kind if kind < 12 else 99, count)}


def edges():
    r = random.Random(5077)
    for kind in range(12):
        yield mk_case(r, kind=kind)
        for pos in range(NOPS[kind] + 1):
            for how in (1, 2, 3, 4):
                yield mk_case(r, kind=kind, fail=(pos, how))
    for kind in (12, 13, 0x100, 0xFFFFFFFF, 0x80000000):
        yield mk_case(r, kind=kind)
        yield mk_case(r, kind=kind, fail=(0, 1))
    for act in (0, 1, 255):
        yield mk_case(r, act=act, kind=7)
    for count in (0, 1, 40, 41 - 1):
        yield mk_case(r, kind=9, count=count)
    for count in (0, 1, 72):
        yield mk_case(r, kind=10, count=count)


def rnd(r):
    fail = None
    if r.random() < 0.3:
        fail = (r.randrange(0, 4), r.choice([1, 2, 3, 4]))
    return mk_case(r, fail=fail)


TESTS = [
    FnTest("CActionRecorder::ReadAndProcessNextAction", ppc, host, edge=edges, rnd=rnd),
]
