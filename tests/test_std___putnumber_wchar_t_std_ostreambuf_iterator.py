"""Unit v5996: std::__putnumber<wchar_t, ostreambuf_iterator>, basic_ios<wchar_t>::widen and deque<vector<Argument>>::const_iterator::operator++,
against libtpw_unit_std___putnumber_wchar_t_std_ostreambuf_iterator.dylib (tests/unit_std___putnumber_wchar_t_std_ostreambuf_iterator.cpp), main app,
src/game/MslSupport.cpp:
  __putnumber 0x10226358, widen 0x10226828, const_iterator::operator++ 0x1022571c

A case is a world and a call, built twice by one piece of Python (emulator memory, big-endian, and the host library's memory, little-endian).
Callees outside the unit are fakes on both sides that record the same events: the streambuf overflow (__ptr_glue 0x1023baa8, which is also the
ctype widen call: the case's op says which), ios_base::getloc, use_facet<ctype<wchar_t>> and the locale destructor.

The world: __putnumber has a streambuf (pptr/epptr inside a 300 character buffer), an ios (width, flags), prefix and suffix strings. widen has an
ios whose first word getloc reads and a locale the fake fills from queues. operator++ has a block table and an iterator of six words (the
Mac's 32-bit words; the host reads them as heap addresses). Compared: the answer, the log, the buffer, pptr, width; the iterator, the table
descriptor and the table words (the slot word relative to the table).

No fused operations. Not tested: a NULL ios (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___putnumber_wchar_t_std_ostreambuf_iterator"
MASK = 0xFFFFFFFF

FN_PUT, FN_WIDEN, FN_INC = 0x10226358, 0x10226828, 0x1022571c
PTR_GLUE, GETLOC, USE_FACET, LOC_DTOR = 0x1023baa8, 0x10072a34, 0x1021eb10, 0x10071f34
FACET = 0x12345678

HOST_LAYOUT = [0x14, 0x18, 0x2C, 0x30, 0x18]

STUBS = CL.Stubs()
STUBS.real.update({FN_PUT, FN_WIDEN, FN_INC})


def s32(v):
    return T.s32(v)


def stub_glue(e):
    L = e.cl
    if e.mode == "widen":
        L.rec("widen", (1 if e.r(3) == FACET else 0, s32(e.r(4))))
        e.set_r(3, L.ret("widen") & MASK)
    else:
        L.rec("overflow", (e.r(4) & MASK,))
        e.set_r(3, L.ret("overflow") & MASK)


def stub_getloc(e):
    L = e.cl
    L.rec("getloc", (e.u32(e.r(4)),))
    e.write(e.r(3), struct.pack(">II", L.ret("loc0") & MASK, L.ret("loc1") & MASK))


def stub_use_facet(e):
    L = e.cl
    L.rec("use_facet", (e.u32(e.r(3)), e.u32(e.r(3) + 4)))
    e.set_r(3, FACET)
    # FACET's vtable word (entry 0x2c) is never followed: the __ptr_glue fake takes the call


def stub_loc_dtor(e):
    L = e.cl
    L.rec("locale_dtor", (e.u32(e.r(3)), e.u32(e.r(3) + 4), s32(e.r(4))))


STUBS.internal(PTR_GLUE, stub_glue)
STUBS.internal(GETLOC, stub_getloc)
STUBS.internal(USE_FACET, stub_use_facet)
STUBS.internal(LOC_DTOR, stub_loc_dtor)


def setup(runner):
    global FACET
    T.install_std_stubs(runner.emu)
    vt = T.persist_alloc(runner.emu, 0x80)
    FACET = T.persist_alloc(runner.emu, 0x40, struct.pack(">I", vt) + bytes(0x3C))    # the facet object: its vtable word is read (entry 0x2c is the __ptr_glue fake)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def halves(self, a, vs):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(vs), *vs))

    def rd_halves(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.mode = case["op"]
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def ptr(self, a):        # a Ptr32 word
        return a

    def low(self, a):        # a 32-bit Mac word holding an address
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK

    def put(self, w, a):
        out = self.alloc(8)
        self.call(FN_PUT, [out, w["sb"], w["ios"], a["fill"], w["prefix"], a["plen"], w["suffix"], a["slen"]])
        return self.rd_words(out, 1)[0]

    def widen(self, w, c):
        return self.call(FN_WIDEN, [w["ios"], c])

    def inc(self, w):
        self.call(FN_INC, [w["it"]])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_wp_alloc(n)

    def ptr(self, a):
        return self.lib.tpw_wp_off(a)

    def low(self, a):
        return a & MASK

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def put(self, w, a):
        r = self.lib.tpw_wp_put(w["sb"], w["ios"], a["fill"], w["prefix"], a["plen"], w["suffix"], a["slen"])
        return self.lib.tpw_wp_off(r) if r else 0

    def widen(self, w, c):
        return self.lib.tpw_wp_widen(w["ios"], s32(c)) & MASK

    def inc(self, w):
        self.lib.tpw_wp_inc(w["it"])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_wp_alloc.restype, lib.tpw_wp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_wp_off.restype, lib.tpw_wp_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_wp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_wp_put.restype = C.c_uint64
    lib.tpw_wp_put.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint64, C.c_int32, C.c_uint64, C.c_int32]
    lib.tpw_wp_widen.restype, lib.tpw_wp_widen.argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    lib.tpw_wp_inc.restype, lib.tpw_wp_inc.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_wp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


BUFN = 300


def run_put(s, case):
    a = case["put"]
    buf = s.alloc(2 * BUFN)
    s.halves(buf, [0x7777] * BUFN)
    sb = s.alloc(0x20)
    pp = s.ptr(buf) + 2 * a["start"]
    s.words(sb, [s.ptr(s.alloc(0x40))])      # the vtable the original reads entry 0x38 of
    s.words(sb + 0x14, [pp, pp + 2 * a["room"]])
    ios = s.alloc(0x40)
    s.words(ios + 0x2C, [a["width"], a["flags"]])
    prefix = s.alloc(2 * a["plen"] + 4)
    s.halves(prefix, a["pdata"])
    suffix = s.alloc(2 * a["slen"] + 4)
    s.halves(suffix, a["sdata"])
    w = {"sb": 0 if a["null"] else sb, "ios": ios, "prefix": prefix, "suffix": suffix}
    r = s.put(w, a)
    return {"ok": r != 0, "log": s.log(), "buf": s.rd_halves(buf, BUFN), "pptr": s.rd_words(sb + 0x14, 1)[0] - s.ptr(buf),
            "ioswords": s.rd_words(ios + 0x2C, 2), "same": r in (0, s.ptr(sb))}


def run_widen(s, case):
    ios = s.alloc(0x20)
    s.words(ios, [case["w0"]])
    r = s.widen({"ios": ios}, case["c"])
    return {"answer": r, "log": s.log()}


def run_inc(s, case):
    d = case["inc"]
    cap = d["cap"]
    table = s.alloc(4 * (cap + 6))
    s.words(table, d["table"])
    cd = s.alloc(16)
    s.words(cd, [cap, d["cnt"], d["start"], s.low(table)])
    it = s.alloc(0x18)
    slot = s.low(table) + 4 * d["k"]
    s.words(it, [s.low(cd), d["idx"], slot, d["element"], d["blockEnd"], d["idx2"]])
    s.inc({"it": it})
    iw = s.rd_words(it, 6)
    tbl = s.low(table)
    return {"it": [iw[0] - s.low(cd), iw[1], (iw[2] - tbl) & MASK, iw[3], iw[4], iw[5]], "cd": s.rd_words(cd, 3),
            "table": s.rd_words(table, cap + 6), "tptr": s.rd_words(cd + 12, 1)[0] - tbl}


def run(s, case):
    try:
        return {"put": run_put, "widen": run_widen, "inc": run_inc}[case["op"]](s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases


def rchar(r):
    return r.choice([0xFFFF, 0, 0x41, r.randrange(0x10000), r.randrange(0x10000)])


def put_case(r, **kw):
    plen, slen = r.randrange(0, 5), r.randrange(0, 5)
    a = {"start": r.randrange(0, 50), "room": r.choice([0, 1, 2, 3, 5, 8, 30]), "width": r.choice([0, 1, 3, 6, 10, 14, -2]),
         "flags": r.choice([0x10, 0x20, 0x80, 0, 0x30, 0xB0, 0x40, 0x10 | 0x4, 0x20 | 0x2000, r.randrange(1 << 16)]),
         "fill": r.choice([0x20, 0xFFFF, 0x30, r.randrange(0x10000)]), "plen": plen, "slen": slen, "null": r.random() < 0.08,
         "pdata": [rchar(r) for _ in range(plen + 2)], "sdata": [rchar(r) for _ in range(slen + 2)]}
    a.update(kw)
    n = max(a["plen"], 0)
    a["pdata"] = (a["pdata"] + [0] * 8)[:a["plen"] + 2]
    a["sdata"] = (a["sdata"] + [0] * 8)[:a["slen"] + 2]
    q = {"overflow": [r.choice([0x41, 0xFFFF, 0x1FFFF, 0, -1, 0x12345]) for _ in range(r.randrange(1, 4))]}
    return {"op": "put", "put": a, "queues": q}


def put_edges():
    r = random.Random(11)
    for flags in (0, 0x10, 0x20, 0x80, 0x30, 0xB0):
        for room in (0, 2, 30):
            yield put_case(r, flags=flags, room=room, width=8, plen=2, slen=2, null=False)
    yield put_case(r, null=True, width=6, plen=2, slen=2, flags=0)
    yield put_case(r, width=3, plen=2, slen=3, flags=0x20, room=30)        # width below the text: no padding
    yield put_case(r, width=5, plen=2, slen=3, flags=0x20, room=30)        # width equal to the text
    yield put_case(r, width=6, plen=2, slen=3, flags=0x20, room=30)        # one pad
    yield put_case(r, width=0, plen=0, slen=0, flags=0, room=30)
    yield put_case(r, width=4, plen=1, slen=0, flags=0, room=30, fill=0xFFFF)   # a stored 0xffff fails the iterator
    yield put_case(r, width=0, plen=2, slen=2, flags=0, room=30, pdata=[1, 0xFFFF, 3, 4], sdata=[5, 6, 7, 8])


def put_random(r):
    return put_case(r)


def widen_case(r, c=None):
    return {"op": "widen", "w0": r.randrange(1 << 32), "c": r.choice([0, 1, -1, 127, -128, 0x41, r.randrange(-128, 128)]) if c is None else c,
            "queues": {"loc0": [r.randrange(1 << 32)], "loc1": [r.randrange(1 << 32)],
                       "widen": [r.choice([0, 0x41, 0xFFFF, r.randrange(0x10000)])]}}


def widen_edges():
    r = random.Random(12)
    for c in (0, 1, -1, 127, -128, 0x7F, 0x41):
        yield widen_case(r, c)


def widen_random(r):
    return widen_case(r)


def inc_case(r, early=None, wrap=None):
    cap = r.randrange(1, 8)
    k = r.randrange(0, cap)
    if wrap is True:
        k = cap - 1
    elif wrap is False and cap > 1:
        k = r.randrange(0, cap - 1)
    element = r.randrange(1 << 32)
    if early is None:
        early = r.random() < 0.3
    blockEnd = (element + 12) & MASK if not early else r.randrange(1 << 32)
    if not early and r.random() < 0.85:
        pass
    # when the block is left, the old block start decides the new block length
    length = r.choice([0, 1, 2, 5, 42, -1, -3, r.randrange(-50, 50)])
    table = [r.randrange(1 << 32) for _ in range(cap + 6)]
    if not early:
        table[k] = (blockEnd - length * 12 - r.choice([0, 0, 1, 5, 11, 12, 13])) & MASK
    if r.random() < 0.1:
        table[k] = r.randrange(1 << 32)
    return {"op": "inc", "queues": {}, "inc": {"cap": cap, "k": k, "table": table, "cnt": r.randrange(1 << 32), "start": r.randrange(1 << 32),
                                              "idx": r.choice([0, 7, MASK, r.randrange(1 << 32)]), "idx2": r.choice([0, 3, MASK, r.randrange(1 << 32)]),
                                              "element": element, "blockEnd": blockEnd}}


def inc_edges():
    r = random.Random(13)
    for early in (True, False):
        for wrap in (True, False):
            for _ in range(3):
                yield inc_case(r, early, wrap)


def inc_random(r):
    return inc_case(r)


TESTS = [
    FnTest("std::__putnumber<wchar_t,std::ostreambuf_iterator<wchar_t,std::char_traits<wchar_t>>>", ppc, host, edge=put_edges, rnd=put_random),
    FnTest("std::basic_ios<wchar_t,std::char_traits<wchar_t>>::widen", ppc, host, edge=widen_edges, rnd=widen_random),
    FnTest("std::deque<std::vector<Argument,std::allocator<Argument>>,std::allocator<std::vector<Argument,std::allocator<Argument>>>>::const_iterator::operator++",
           ppc, host, edge=inc_edges, rnd=inc_random),
]
