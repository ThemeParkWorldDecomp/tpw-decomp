"""Unit v5665, std::__use_facet<F>(const std::locale&) for four facets, against libtpw_unit_std___use_facet.dylib
(tests/unit_std___use_facet.cpp), main app ("SimTheme Park"), src/GetNextWaypoint_HeightAtPos_NAV_std___use_facet.cpp:
  ctype<char> 0x10073b54, codecvt<char,char,int> 0x100739a0, ctype<wchar_t> 0x102261a0, codecvt<wchar_t,char,int> 0x102298e4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (operator new,
the facet constructors, the vector resize/insert/erase, the facet delete through __ptr_glue) are fakes on both sides that record their
arguments; the vector fakes only change the count and leave the slots alone, so stale pointers beyond the count reach the release path.

The world of a case: the five id globals, a locale (one word, the impl), the impl (count at +4, data at +8) and a 64-slot data block
holding an existing facet, a stale facet beyond the count, or nothing. What is compared: the answer (the existing facet, or the words of
the new one), the data slots (as labels), the stale and existing facets' words, the id globals and the event log.

No fused operations. Not tested: a count above the 64 slots (the fakes never reallocate).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___use_facet"
MASK = 0xFFFFFFFF

FNS = {"ctc": 0x10073b54, "cvc": 0x100739a0, "ctw": 0x102261a0, "cvw": 0x102298e4}
IDKEY = {"ctc": "id_ctype_char", "cvc": "id_codecvt_cc", "ctw": "id_ctype_wchar", "cvw": "id_codecvt_wc"}
ADDR = dict(count=0x103bd06c, id_ctype_char=0x103bd074, id_codecvt_cc=0x103bcfb4, id_ctype_wchar=0x103bd0d8, id_codecvt_wc=0x103bd03c)
TOC = dict(id_ctype_char=0x10260c7c, count=0x10260c80, id_codecvt_cc=0x10260c84, id_codecvt_wc=0x10261df0, id_ctype_wchar=0x10261df8)
HOST_LAYOUT =[8, 4, 4, 8, 8]      # sizeof/mRefs of the facet, mCount and mData of the impl, sizeof(void*)
SLOTS = 64
IDS = ["id_ctype_char", "id_codecvt_cc", "id_ctype_wchar", "id_codecvt_wc"]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(FNS.values())


def st_new(e):
    size = e.r(3)
    e.cl.rec("new", (size,))
    e.set_r(3, e.alloc(size, bytes(size)))


def st_resize(e):
    vec, n = e.r(3), e.r(4)
    e.cl.rec("resize", (n, 1 if e.r(5) else 0))
    if n > e.u32(vec + 4):
        e.write(vec + 4, struct.pack(">I", n))


def st_insert(e):
    vec, pos, count, valp = e.r(3), e.r(4), e.r(5), e.r(6)
    data = e.u32(vec + 8)
    e.cl.rec("insert", (((pos - data) & MASK) // 4, count, 1 if e.u32(valp) else 0))
    e.write(vec + 4, struct.pack(">I", (e.u32(vec + 4) + count) & MASK))


def st_erase(e):
    vec, first, last = e.r(3), e.r(4), e.r(5)
    data = e.u32(vec + 8)
    e.cl.rec("erase", (((first - data) & MASK) // 4, ((last - data) & MASK) // 4))
    e.write(vec + 4, struct.pack(">I", ((first - data) & MASK) // 4))


def st_delete(e):
    e.cl.rec("facet_delete", (e.cl.lab(e.r(3)), s32(e.r(4))))


STUBS.internal(0x102384b8, st_new)
STUBS.internal(0x10238980, CL.recorder("ctype_char_ctor", "Niii"))
STUBS.internal(0x102388d8, CL.recorder("codecvt_char_ctor", "Ni"))
STUBS.internal(0x1023b128, CL.recorder("codecvt_wchar_ctor", "Ni"))
STUBS.internal(0x10073d58, st_resize)
STUBS.internal(0x10073ed4, st_insert)
STUBS.internal(0x10073e10, st_erase)
STUBS.internal(0x1023baa8, st_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def ptrs(self, a, ws):
        self.write(a, b"".join(struct.pack(self.pfmt, w) for w in ws))

    def rd_ptrs(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%d%s" % (n, self.pfmt[1]), self.read(a, self.psz * n)))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        # the emulator maps the data imports elsewhere: the TOC slots hold the real addresses
        return self.e.u32(TOC[name])

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, loc):
        r3, _, _ = T.call_ppc(self.e, FNS[op], [loc])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_uf_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_uf_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, loc):
        fn = {"ctc": self.lib.tpw_uf_ctype_char, "cvc": self.lib.tpw_uf_codecvt_cc,
              "ctw": self.lib.tpw_uf_ctype_wchar, "cvw": self.lib.tpw_uf_codecvt_wc}[op]
        return fn(loc)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_uf_addr.restype, lib.tpw_uf_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_uf_alloc.restype, lib.tpw_uf_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_uf_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("ctype_char", "codecvt_cc", "ctype_wchar", "codecvt_wc"):
        f = getattr(lib, "tpw_uf_" + n)
        f.restype, f.argtypes = C.c_uint64, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_uf_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    for name in IDS:
        s.words(s.g(name), [case["ids"][name]])
    s.words(s.g("count"), [case["count"]])
    vt = 0x11110000
    if isinstance(s, Ppc):          # the release path reads vtable +8 (the host fake takes the facet only)
        vt = s.alloc(12)
        s.words(vt, [0, 0, 0x10000000])
    exist = s.alloc(8)
    s.words(exist, [vt, case["erefs"]])
    stale = s.alloc(8)
    s.words(stale, [vt, case["srefs"]])
    data = s.alloc(SLOTS * s.psz)
    ws = [{"0": 0, "E": exist, "S": stale}[c] for c in case["slots"]]
    s.ptrs(data, ws)
    imp = s.alloc(16)
    s.words(imp, [0x33330000, case["size"]])
    s.ptrs(imp + 8, [data])
    loc = s.alloc(8)
    s.ptrs(loc, [imp])
    s.exist, s.stale, s.data, s.imp, s.loc = exist, stale, data, imp, loc


def observe(s, case, ans):
    if ans == s.exist:
        a = "E"
    elif ans == s.stale:
        a = "S"
    elif ans == 0:
        a = "null"
    else:
        a = ("new", s.rd_words(ans, 2))
    tag = {0: "0", s.exist: "E", s.stale: "S"}
    slots = []
    for w in s.rd_ptrs(s.data, SLOTS):
        slots.append("N" if (w == ans and ans != 0) else tag.get(w, ("?", w)))
    out = {"answer": a, "log": s.log(), "slots": slots, "imp": [s.rd_words(s.imp, 4)[1]],
           "exist": s.rd_words(s.exist, 2)[1:], "stale": s.rd_words(s.stale, 2)[1:], "count": s.rd_words(s.g("count"), 1)}
    for name in IDS:
        out[name] = s.rd_words(s.g(name), 1)
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], s.loc))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, op, idv=None, count=None, size=None, at=None, srefs=None):
    """idv: the id global (0 = unassigned); size: the vector count; at: what sits in the slot of the id."""
    if idv is None:
        idv = r.choice([0, 0, r.randrange(1, 30)])
    if count is None:
        count = r.randrange(0, 30)
    myid = idv if idv else count + 1
    if size is None:
        size = r.choice([0, myid - 1, myid, myid + 1, r.randrange(0, 40)])
        size = max(0, min(size, SLOTS))
    ids = {n: r.choice([0, r.randrange(1, 30)]) for n in IDS}
    ids[IDKEY[op]] = idv
    slots = []
    for i in range(SLOTS):
        if i < size:
            slots.append(r.choice(["0", "E"]))
        else:
            slots.append(r.choice(["0", "S"]))
    if at is not None:
        slots[myid] = at
    return {"op": op, "queues": {}, "ids": ids, "count": count, "size": size, "slots": slots,
            "erefs": r.randrange(1, 4), "srefs": srefs if srefs is not None else r.randrange(1, 4)}


def edges_for(op, seed):
    def gen():
        r = random.Random(seed)
        for idv in (0, 3):
            for count in (0, 7):
                myid = idv if idv else count + 1
                for size in (0, myid - 1, myid, myid + 1, myid + 5):
                    for at in ("0", "E", "S"):
                        for srefs in (1, 2):
                            yield mk(r, op, idv, count, max(0, size), at, srefs)
    return gen


def rnd_for(op):
    return lambda r: mk(r, op)


TESTS = [
    FnTest("std::__use_facet<std::ctype<char>>", ppc, host, edge=edges_for("ctc", 11), rnd=rnd_for("ctc")),
    FnTest("std::__use_facet<std::codecvt<char,char,int>>", ppc, host, edge=edges_for("cvc", 12), rnd=rnd_for("cvc")),
    FnTest("std::__use_facet<std::ctype<wchar_t>>", ppc, host, edge=edges_for("ctw", 13), rnd=rnd_for("ctw")),
    FnTest("std::__use_facet<std::codecvt<wchar_t,char,int>>", ppc, host, edge=edges_for("cvw", 14), rnd=rnd_for("cvw")),
]
