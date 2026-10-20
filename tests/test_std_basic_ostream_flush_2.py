"""Unit v5898, std::basic_ostream<wchar_t>::flush and its constructor, against libtpw_unit_std_basic_ostream_flush_2.dylib
(tests/unit_std_basic_ostream_flush_2.cpp), main app, src/std_basic_ostream_flush_2.cpp:
  flush 0x102268f8, basic_ostream 0x10226c94

A case is a world and a call. The world is built twice by one piece of Python code (emulator memory big-endian, host memory
little-endian). The callees (good, tie, setstate, the stream buffer's virtual through __ptr_glue, __throw, operator new[] and new,
ios_base::init, widen) are fakes on both sides that record their arguments and answer from the case's queues. Tie answers either
NULL or a second stream (itself flushed recursively by the real code).

Compared: the answer, the event log, the words of both streams and ios blocks (pointers and globals normalised to names), and for
the constructor the whole object including the halfword the fill is stored in. No fused operations. Not tested: the exception
handler code of flush (unreachable) and the final unitbuf flush (the done flag is always set by then).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_ostream_flush_2"
MASK = 0xFFFFFFFF

FN_FLUSH, FN_CTOR = 0x102268F8, 0x10226C94
OWN = {FN_FLUSH: 848, FN_CTOR: 188}

ADDR = dict(gIosFailureVtbl=0x1029D084, gIosFailureDtorVec=0x102661D0, gIosBaseVt=0x103BD034, gIfsWVtblE=0x102B1F20,
            gWostreamVtbl=0x102B1F08)
HOST_LAYOUT = [0x24, 0x34, 0x38, 4]

BLOCKS = [("S", 8), ("I", 0x50), ("SB", 0x10), ("VT", 0x40), ("S2", 8), ("I2", 0x50), ("SB2", 0x10), ("VT2", 0x40),
          ("C", 0x60), ("IO", 0x50)]
LABEL = {n: i + 1 for i, (n, _) in enumerate(BLOCKS)}
SIZE = dict(BLOCKS)
POOL = {}          # PowerPC persistent blocks
HPOOL = {}         # host blocks by library


def cs(e, a):
    out = b""
    while True:
        b = e.read(a + len(out), 1)
        if b == b"\0":
            return out
        out += b


def hsh(b):
    h = 0
    for c in b:
        h = (h * 31 + c) & MASK
    return h


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s_good(e):
    L = e.cl
    L.rec("Good", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("Good") & 1)


def s_tie(e):
    L = e.cl
    L.rec("Tie", (L.lab(e.r(3)),))
    e.set_r(3, POOL["S2"] if L.ret("Tie") else 0)


def s_setstate(e):
    L = e.cl
    L.rec("Setstate", (L.lab(e.r(3)), T.s32(e.r(4))))


def s_virt(e):
    L = e.cl
    L.rec("Virt", (L.lab(e.r(3)), e.r(12) & MASK))
    e.set_r(3, L.ret("Virt") & MASK)


def s_throw(e):
    L = e.cl
    o = e.r(4)
    msg, cnt = e.u32(o + 4), e.u32(o + 8)
    L.rec("Throw", (hsh(cs(e, e.r(3))), 1 if e.u32(o) == ADDR["gIosFailureVtbl"] else 0, hsh(cs(e, msg)) if msg else MASK - 1,
                    e.u32(cnt) if cnt else -1, 1 if e.r(5) == ADDR["gIosFailureDtorVec"] else 0))


def s_newarr(e):
    L = e.cl
    L.rec("NewArray", (e.r(3) & MASK,))
    e.set_r(3, e.alloc(e.r(3)))


def s_new(e):
    L = e.cl
    L.rec("New", (4,))
    e.set_r(3, e.alloc(4))


def s_strlen(e):
    e.set_r(3, len(cs(e, e.r(3))))


def s_strcpy(e):
    e.write(e.r(3), cs(e, e.r(4)) + b"\0")


def s_init(e):
    L = e.cl
    L.rec("Init", (L.lab(e.r(3)), e.r(4) & MASK))


def s_widen(e):
    L = e.cl
    L.rec("Widen", (L.lab(e.r(3)), e.r(4) & 0xFF))
    e.set_r(3, L.ret("Widen") & 0xFFFF)


for _a, _f in ((0x102079B4, s_good), (0x1021F888, s_tie), (0x102077C0, s_setstate), (0x1023BAA8, s_virt), (0x10238680, s_throw),
               (0x10238668, s_newarr), (0x102384B8, s_new), (0x10238650, s_strlen), (0x10238410, s_strcpy),
               (0x10238968, s_init), (0x10226828, s_widen)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)
    for n, sz in BLOCKS:
        POOL[n] = T.persist_alloc(runner.emu, sz)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({POOL[n]: LABEL[n] for n, _ in BLOCKS}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def nat(self, n):
        return POOL[n]

    def ref(self, n):
        return POOL[n]

    def gbase(self, name):
        return ADDR[name]

    def block_bases(self):
        return {n: POOL[n] for n, _ in BLOCKS}

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def flush(self):
        return T.call_ppc(self.e, FN_FLUSH, [POOL["S"]])[0] & MASK

    def ctor(self, flag, sb):
        return T.call_ppc(self.e, FN_CTOR, [POOL["C"], flag & MASK, sb & MASK])[0] & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        if lib not in HPOOL:
            HPOOL[lib] = {n: lib.tpw_wf_alloc(sz) for n, sz in BLOCKS}
        self.pool = HPOOL[lib]
        self.base = lib.tpw_wf_base()
        lib.tpw_wf_settie(self.pool["S2"])
        CL.host_begin(lib, {LABEL[n]: self.pool[n] for n, _ in BLOCKS}, case["queues"])

    def done(self):
        pass

    def nat(self, n):
        return self.pool[n]

    def ref(self, n):
        return self.pool[n] - self.base

    def gbase(self, name):
        return self.lib.tpw_wf_addr(name.encode()) & MASK

    def block_bases(self):
        return {n: self.pool[n] - self.base for n, _ in BLOCKS}

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def flush(self):
        return (self.lib.tpw_wf_flush(self.pool["S"]) - self.base) & MASK

    def ctor(self, flag, sb):
        return (self.lib.tpw_wf_ctor(self.pool["C"], T.s32(flag), sb & MASK) - self.base) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_wf_addr.restype, lib.tpw_wf_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_wf_alloc.restype, lib.tpw_wf_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_wf_base.restype, lib.tpw_wf_base.argtypes = C.c_uint64, []
    lib.tpw_wf_settie.argtypes = [C.c_uint64]
    lib.tpw_wf_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_wf_flush.restype, lib.tpw_wf_flush.argtypes = C.c_uint64, [C.c_uint64]
    lib.tpw_wf_ctor.restype, lib.tpw_wf_ctor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_wf_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def norm(s, w, gb, bb):
    for name, b in gb.items():
        if 0 <= w - b < 0x20:
            return ("g", name, w - b)
    for name, b in bb.items():
        if 0 <= w - b < SIZE[name]:
            return ("b", name, w - b)
    return w


def dump(s, name, half_off=None):
    gb = {n: s.gbase(n) for n in ADDR}
    bb = s.block_bases()
    n = SIZE[name] // 4
    ws = s.rd_words(s.nat(name), n)
    out = [norm(s, w, gb, bb) for w in ws]
    if name == "IO" and ws[0x44 // 4]:    # ios + 0x44 holds (this + 8) - ios: the distance between two blocks, which differs per side
        out[0x44 // 4] = ("d", (ws[0x44 // 4] + bb["IO"] - bb["C"]) & MASK)
    if half_off is not None:
        raw = s.read(s.nat(name) + half_off, 4)
        out[half_off // 4] = (struct.unpack(s.fmt[0] + "H", raw[:2])[0], raw[2:])
    return out


def build(s, case):
    for n, sz in BLOCKS:
        s.write(s.nat(n), bytes(sz))
    if case["op"] == "flush":
        for st, io, sb, vt, k in (("S", "I", "SB", "VT", ""), ("S2", "I2", "SB2", "VT2", "2")):
            s.words(s.nat(st), [s.ref(io)])
            s.words(s.nat(io) + 0x24, [s.ref(sb) if case["sb" + k] else 0])
            s.words(s.nat(io) + 0x30, [case["flags" + k], case["state" + k], case["exc" + k]])
            s.words(s.nat(sb), [s.ref(vt)])
            s.words(s.nat(vt) + 0x1C, [case["entry" + k]])
    elif case["flag"] & 0xFFFF == 0:
        s.words(s.nat("C"), [s.ref("IO")])


def run(s, case):
    try:
        build(s, case)
        if case["op"] == "flush":
            ans = s.flush()
            out = {"ans": ans - s.block_bases()["S"], "log": s.log()}
            for n in ("S", "I", "SB", "VT", "S2", "I2", "SB2", "VT2"):
                out[n] = dump(s, n)
        else:
            ans = s.ctor(case["flag"], case["sb"])
            out = {"ans": ans - s.block_bases()["C"], "log": s.log(), "C": dump(s, "C", 0x48 if case["flag"] & 0xFFFF else None),
                   "IO": dump(s, "IO", None if case["flag"] & 0xFFFF else 0x40)}
        return out
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def flush_case(r, good=None, tie=None, virt=None, sb=None, exc=None):
    c = {"op": "flush"}
    for k in ("", "2"):
        c["sb" + k] = r.choice([0, 1, 1]) if sb is None else sb
        c["state" + k] = r.choice([0, 1, 2, 4, 5, 8])
        c["exc" + k] = r.choice([0, 1, 5, 0xFF, 2]) if exc is None else exc
        c["flags" + k] = 0x80000000 | r.randrange(1 << 31)       # far from any pool offset the normaliser maps
        c["entry" + k] = 0x80005000 + r.randrange(0x100)
    c["queues"] = {
        "Good": [r.choice([0, 1]) for _ in range(r.randrange(1, 4))] if good is None else good,
        # Tie is asked twice per level (test, then the flush argument): the answers come in equal pairs
        "Tie": [v for v in [r.choice([0, 1]) for _ in range(r.randrange(1, 3))] + [0] for _ in (0, 1)] if tie is None else tie,
        "Virt": [r.choice([-1, -1, 0, 7]) for _ in range(r.randrange(1, 3))] if virt is None else virt,
    }
    return c


def flush_edges():
    r = random.Random(898)
    for good in ([0], [1, 0], [1, 1], [1, 1, 0, 1]):
        for tie in ([0, 0], [1, 1, 0, 0], [1, 1, 1, 1, 0, 0]):
            for virt in ([-1], [0]):
                for sb in (0, 1):
                    for exc in (0, 1, 5, 0xFF):
                        yield flush_case(r, good, tie, virt, sb, exc)


def flush_random(r):
    return flush_case(r)


def ctor_case(r, flag=None):
    return {"op": "ctor", "flag": r.choice([0, 1, -1, 0x10000, 0x8000, 0x7FFF, 0x12345]) & MASK if flag is None else flag,
            "sb": r.choice([0, 1, 0x10261000, r.randrange(1 << 32)]),
            "queues": {"Widen": [r.randrange(0x10000)]}}


def ctor_edges():
    r = random.Random(899)
    for f in (0, 1, -1, 0x10000, 0x8000, 0x7FFF, 0xFFFF0000):
        yield ctor_case(r, f & MASK)


def ctor_random(r):
    return ctor_case(r)


TESTS = [
    FnTest("std::basic_ostream<wchar_t,std::char_traits<wchar_t>>::flush", ppc, host, edge=flush_edges, rnd=flush_random),
    FnTest("std::basic_ostream<wchar_t,std::char_traits<wchar_t>>::basic_ostream", ppc, host, edge=ctor_edges, rnd=ctor_random),
]
