"""Unit v5203, the list control's Sort and FindString, against libtpw_unit_InterfaceListControl2_Sort.dylib
(tests/unit_InterfaceListControl2_Sort.cpp), main app ("SimTheme Park"), src/InterfaceListControl2_items_InterfaceListControl2_Sort.cpp:
  InterfaceListControl2::Sort 0x101d1958, InterfaceListControl2::FindString 0x101d44b8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory (big-endian, 4-byte words) and in
the host library's memory (little-endian, 8-byte words and pointers; the offsets come from the library's layout export). The callees outside
the unit are fakes on both sides recording their arguments: the wide string compare 0x101d5864 (really compares, answers -1/0/1),
wcslen 0x102389c8, AdvanceIndex 0x101d2a84 (the next link) and GetNonHierIndexedEntry 0x101d2c30 (the entry `position` steps along the next
links).

The world of a case: the object (storage, count, first entry, columns, sort column and direction), the column types, the item block
(entries of columns + 4 words, a chain of used entries then free ones with parent -2) with number and string cells. What is compared: the
answer, the call log, the object's fields, the links and number cells of every entry.

No fused operations. Not tested: sort columns outside the columns, a chain that ends on a used entry (the original reads before the block).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InterfaceListControl2_Sort"
MASK = 0xFFFFFFFF

SORT, FIND = 0x101D1958, 0x101D44B8
SIZES = {SORT: 1320, FIND: 348}
OWN = set(SIZES)

COMPARE, GETNH, ADVANCE, WCSLEN = 0x101D5864, 0x101D2C30, 0x101D2A84, 0x102389C8

# Mac layout: storage, count, first, column count, sort column, descending, columns, sizeof object, sizeof column, bottom, word, rep, rep data
MAC_LAYOUT = [0x144, 0x150, 0x160, 0x164, 0x168, 0x16A, 0x16C, 0x180, 0xC, 0xA, 4, 16, 0xC]


def s32(v):
    return T.s32(v)


def chars_at(e, data, n):
    return struct.unpack(">%dH" % n, e.read(data, 2 * n)) if n else ()


def order(a, b):
    n = min(len(a), len(b))
    for i in range(n):
        if a[i] != b[i]:
            return -1 if a[i] < b[i] else 1
    return -1 if len(a) < len(b) else (1 if len(a) > len(b) else 0)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_compare(e):
    L = e.cl
    selfp, pos, n1, sp, n2 = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
    rep = e.u32(selfp)
    a = chars_at(e, e.u32(rep + 0xC) + 2 * pos, n1)
    b = chars_at(e, sp, n2)
    L.rec("compare", (pos, n1, n2, sum(a), sum(b)))
    e.set_r(3, order(a, b) & MASK)


def stub_getnh(e):
    L = e.cl
    this, pos, parent = e.r(3), s32(e.r(4)), s32(e.r(5))
    L.rec("GetNonHier", (pos, parent))
    storage = e.u32(this + 0x144)
    entry = e.s32(this + 0x160)
    i = 0
    while i < pos and entry != -1:
        entry = e.s32(storage + entry * 4)
        i += 1
    e.set_r(3, entry & MASK)


def stub_advance(e):
    L = e.cl
    this, idx, ch = e.r(3), s32(e.r(4)), e.r(5)
    L.rec("AdvanceIndex", (idx, 1 if ch else 0))
    r = -1 if idx < 0 else e.s32(e.u32(this + 0x144) + idx * 4)
    e.set_r(3, r & MASK)


def stub_wcslen(e):
    L = e.cl
    p = e.r(3)
    n = 0
    while e.read(p + 2 * n, 2) != b"\0\0":
        n += 1
    L.rec("wcslen", (n,))
    e.set_r(3, n)


STUBS.internal(COMPARE, stub_compare)
STUBS.internal(GETNH, stub_getnh)
STUBS.internal(ADVANCE, stub_advance)
STUBS.internal(WCSLEN, stub_wcslen)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def put(self, a, v, size):
        self.write(a, (v & ((1 << (8 * size)) - 1)).to_bytes(size, self.order))

    def get(self, a, size, signed=True):
        return int.from_bytes(self.read(a, size), self.order, signed=signed)

    def unpack_layout(self, lay):
        (self.o_storage, self.o_count, self.o_first, self.o_ncols, self.o_scol, self.o_desc, self.o_cols, self.objsz, self.colsz,
         self.o_bottom, self.wsz, self.repsz, self.o_data) = lay

    def mkchars(self, chars):
        data = self.alloc(2 * len(chars) + 2)
        for i, c in enumerate(chars):
            self.put(data + 2 * i, c, 2)
        return data

    def mkstr(self, chars):
        data = self.mkchars(chars)
        rep = self.alloc(self.repsz)
        self.put(rep, len(chars), 4)
        self.put(rep + 4, 3, 4)
        self.put(rep + 8, 1, 4)
        self.put(rep + self.o_data, data, self.ptrsz)
        obj = self.alloc(self.ptrsz)
        self.put(obj, rep, self.ptrsz)
        return obj


class Ppc(Side):
    order = "big"
    ptrsz = 4

    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, case["queues"])
        self.L = e.cl
        self.saved = STUBS.activate(e)
        self.unpack_layout(MAC_LAYOUT)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n + 8)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def sort(self, obj, arg):
        T.call_ppc(self.e, SORT, [obj & MASK, arg & MASK])
        return 0

    def find(self, obj, text, col):
        r3, _, _ = T.call_ppc(self.e, FIND, [obj & MASK, text & MASK, col & MASK])
        return s32(r3)


class Host(Side):
    order = "little"
    ptrsz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        self.unpack_layout(lib.layout)

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_icl_alloc(n + 8)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def sort(self, obj, arg):
        self.lib.tpw_icl_sort(obj, s32(arg))
        return 0

    def find(self, obj, text, col):
        return self.lib.tpw_icl_find(obj, text, s32(col))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_icl_alloc.restype, lib.tpw_icl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_icl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_icl_sort.restype, lib.tpw_icl_sort.argtypes = None, [C.c_uint64, C.c_int32]
    lib.tpw_icl_find.restype, lib.tpw_icl_find.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_icl_layout(out)
    lib.layout = list(out)[:n]
    assert lib.layout[10] == 8 and lib.layout[0] > 0, lib.layout
    lib.bound = True


def build(s, case):
    ncols = case["ncols"]
    stride = ncols + 4
    ents = case["entries"]
    w = {"stride": stride, "nent": len(ents), "storage": 0}
    if not case["nostorage"]:
        storage = s.alloc(len(ents) * stride * s.wsz + 16)
        w["storage"] = storage
        for k, en in enumerate(ents):
            base = storage + k * stride * s.wsz
            for i, v in enumerate((en["next"], en["data"], en["child"], en["parent"])):
                s.put(base + i * s.wsz, v, 4)
            for c in range(ncols):
                a = base + (4 + c) * s.wsz
                if case["ctypes"][c] == 1:
                    s.put(a, en["cells"][c], 4)
                else:
                    s.put(a, s.mkstr(en["cells"][c]), s.ptrsz)
    cols = 0
    if not case["nocols"]:
        cols = s.alloc(max(ncols, 1) * s.colsz)
        for c in range(ncols):
            s.put(cols + c * s.colsz + s.o_bottom, case["ctypes"][c], 2)
    obj = s.alloc(s.objsz)
    w["obj"] = obj
    s.put(obj + s.o_storage, w["storage"], s.ptrsz)
    s.put(obj + s.o_count, case["count"], 4)
    s.put(obj + s.o_first, case["first"], 4)
    s.put(obj + s.o_ncols, ncols, 2)
    s.put(obj + s.o_scol, case["sortcol"], 2)
    s.put(obj + s.o_desc, case["desc"], 2)
    s.put(obj + s.o_cols, cols, s.ptrsz)
    return w


def observe(s, case, w, answer):
    obj = w["obj"]
    out = {"answer": answer, "log": s.log(), "count": s.get(obj + s.o_count, 4), "first": s.get(obj + s.o_first, 4),
           "ncols": s.get(obj + s.o_ncols, 2), "scol": s.get(obj + s.o_scol, 2), "desc": s.get(obj + s.o_desc, 2)}
    if w["storage"]:
        links = []
        for k in range(w["nent"]):
            base = w["storage"] + k * w["stride"] * s.wsz
            row = [s.get(base + i * s.wsz, 4) for i in range(4)]
            for c in range(case["ncols"]):
                if case["ctypes"][c] == 1:
                    row.append(s.get(base + (4 + c) * s.wsz, 4))
            links.append(row)
        out["links"] = links
    return out


def run_with(s, case):
    try:
        w = build(s, case)
        if case["op"] == "sort":
            ans = s.sort(w["obj"], case["arg"])
        else:
            text = s.mkchars(case["text"])
            ans = s.find(w["obj"], text, case["col"])
        return observe(s, case, w, ans)
    finally:
        s.done()


def ppc(e, case):
    return run_with(Ppc(e, case), case)


def host(lib, case):
    return run_with(Host(lib, case), case)


ALPHA = [0x61, 0x62, 0x63]


def rstr(r):
    n = r.choice([0, 1, 1, 2, 2, 3, 4])
    return [r.choice(ALPHA + ([0x8000, 0xFFFF] if r.random() < 0.1 else [])) for _ in range(n)]


def rnum(r):
    return r.choice([0, 1, -1, 2, -2, 3, r.randrange(-5, 5), 0x7FFFFFFF, -0x80000000, r.randrange(-1000, 1000)])


def make(r, op, **kw):
    ncols = kw.get("ncols", r.randint(1, 3))
    ctypes = kw.get("ctypes") or [r.choice([0, 1]) for _ in range(ncols)]
    used = kw.get("used", r.randint(0, 7))
    nfree = kw.get("nfree", r.randint(1, 3))
    nent = used + nfree
    stride = ncols + 4
    order_ = list(range(nent))
    r.shuffle(order_)
    ents = [None] * nent
    for pos, k in enumerate(order_):
        nxt = order_[pos + 1] * stride if pos + 1 < nent else -1
        is_used = pos < used
        cells = [rnum(r) if ctypes[c] == 1 else rstr(r) for c in range(ncols)]
        ents[k] = {"next": nxt, "data": r.randrange(-100, 100), "child": r.choice([-1, -1, 0, stride]),
                   "parent": r.choice([-1, -1, 0, stride]) if is_used else -2, "cells": cells}
    c = {"op": op, "queues": {}, "ncols": ncols, "ctypes": ctypes, "entries": ents,
         "first": kw.get("first", order_[0] * stride), "count": kw.get("count", used),
         "sortcol": kw.get("sortcol", r.randrange(ncols)),
         "desc": kw.get("desc", r.choice([0, 0, 1, 1, 0, 1, 2, -1])),
         "nocols": kw.get("nocols", r.random() < 0.04), "nostorage": kw.get("nostorage", False), "arg": r.randrange(-3, 3)}
    if op == "find":
        c["col"] = kw.get("col", r.choice([-1, 0, 1, 1, 2, 2, 3, ncols, ncols + 1, 100]))
        pool = [en["cells"][i] for en in ents for i in range(ncols) if ctypes[i] == 0]
        c["text"] = kw["text"] if "text" in kw else (r.choice(pool) if pool and r.random() < 0.75 else rstr(r))
    return c


def sort_edges():
    r = random.Random(5203)
    yield make(r, "sort", nocols=True, used=5, count=5)
    yield make(r, "sort", nostorage=True, nocols=False, used=0, count=0, ncols=1, ctypes=[1])
    yield make(r, "sort", first=-1, nocols=False, used=3, count=3)
    for ct in (0, 1):
        for desc in (0, 1, 2, -1):
            for used in (0, 1, 2, 3, 4, 6):
                for count in (None, 2, 3):
                    for _ in range(2):
                        kw = dict(ncols=2, ctypes=[ct, 1 - ct], sortcol=0, desc=desc, used=used, nocols=False)
                        if count is not None:
                            kw["count"] = min(count, used)
                        yield make(r, "sort", **kw)
    c = make(r, "sort", ncols=1, ctypes=[1], used=4, count=4, desc=0, nocols=False)
    for en in c["entries"]:
        en["cells"] = [7]
    yield c
    c = make(r, "sort", ncols=1, ctypes=[1], used=4, count=4, desc=1, nocols=False)
    for en in c["entries"]:
        en["cells"] = [-0x80000000 if en["next"] % 2 else 0x7FFFFFFF]
    yield c


def sort_random(r):
    return make(r, "sort")


def find_edges():
    r = random.Random(5204)
    yield make(r, "find", nocols=True, used=3)
    yield make(r, "find", nostorage=True, first=-1, used=0, count=3, ncols=1, ctypes=[0], nocols=False)
    yield make(r, "find", first=-1, used=3, nocols=False)
    for col in (-1, 0, 1, 2, 3, 4):
        for ct in ([0, 0, 1], [1, 0, 0], [0, 1, 0]):
            for used in (0, 1, 3, 5):
                for count in (None, 0, 1, 2):
                    kw = dict(ncols=3, ctypes=ct, col=col, used=used, nocols=False)
                    if count is not None:
                        kw["count"] = min(count, used)
                    yield make(r, "find", **kw)
    yield make(r, "find", ncols=1, ctypes=[0], used=4, text=[], nocols=False)


def find_random(r):
    return make(r, "find")


TESTS = [
    FnTest("InterfaceListControl2::Sort", ppc, host, edge=sort_edges, rnd=sort_random),
    FnTest("InterfaceListControl2::FindString", ppc, host, edge=find_edges, rnd=find_random),
]
