"""Unit v51030, std::__cdeque<const void*>::insert(iterator pos, unsigned long count, const void* const& value), against
libtpw_unit_std___cdeque_insert_2.dylib (tests/unit_std___cdeque_insert_2.cpp), main app, src/game/MslSupport.cpp:
  insert 0x101f3b10

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory, big-endian, and host memory).
Callees outside the unit: strlen, operator new[], operator new, operator delete, strcpy, __throw (all fakes recording their arguments;
the PowerPC __throw jumps to the epilogue like the original, which never returns) and the iterator copy 0x101f2dd8. The iterator
operator+= 0x101f2bec runs as the original on the PowerPC side and as its translation on the host.

The world of a case: the deque (capacity, size, start, block of random words), the minimum capacity global, the position iterator,
the count and the value word. What is compared: the deque words, the old block, the new block, the call log.

No fused operations. Not tested: counts whose result stays just under 0x3fffffff (a 4 GiB allocation); the throw path is tested.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___cdeque_insert_2"
MASK = 0xFFFFFFFF

INSERT = 0x101F3B10
SIZES = {INSERT: 2564}
ITER_ADD = 0x101F2BEC
ITER_COPY = 0x101F2DD8
EPILOGUE = 0x101F4500
G_STRLEN, G_NEWARR, G_NEW, G_DELETE, G_STRCPY, G_THROW = 0x10238650, 0x10238668, 0x102384B8, 0x10238218, 0x10238410, 0x10238680

ADDR = dict(mincap=0x102B1424)
HOST_LAYOUT = [16, 12]
VTBL_LE, DTOR_LE = 0x10278B74, 0x10265680

STUBS = CL.Stubs()
STUBS.real.update(SIZES)
STUBS.real.add(ITER_ADD)

CUR = {}


def cs(e, a):
    out = bytearray()
    while True:
        b = e.read(a + len(out), 1)[0]
        if b == 0:
            return bytes(out)
        out.append(b)


def h(b):
    v = 0
    for c in b:
        v = (v * 31 + c) & MASK
    return v & 0x7FFFFFFF


def st_strlen(e):
    t = cs(e, e.r(3))
    e.cl.rec("strlen", (len(t), h(t)))
    e.set_r(3, len(t))


def st_new_array(e):
    n = e.r(3)
    e.cl.rec("new_array", (n,))
    e.set_r(3, e.alloc(n))


def st_new(e):
    n = e.r(3)
    e.cl.rec("new", (n,))
    e.set_r(3, e.alloc(n))


def st_delete(e):
    e.cl.rec("delete", (1 if e.r(3) == e.u32(CUR["D"] + 12) else 0,))


def st_strcpy(e):
    t = cs(e, e.r(4))
    e.cl.rec("strcpy", (len(t), h(t)))
    e.write(e.r(3), t + b"\0")


def st_throw(e):
    from unicorn.ppc_const import UC_PPC_REG_LR
    typ = cs(e, e.r(3))
    obj = e.r(4)
    msg = cs(e, e.u32(obj + 4))
    cp = e.u32(obj + 8)
    cnt = e.u32(cp) if cp else -1
    e.cl.rec("throw", (1 if e.u32(obj) == VTBL_LE else 0, 1 if e.r(5) == DTOR_LE else 0, cnt, len(msg), h(msg), len(typ), h(typ)))
    e.uc.reg_write(UC_PPC_REG_LR, EPILOGUE)


def st_itercopy(e):
    d, s = e.r(3), e.r(4)
    e.write(d, e.read(s, 12))
    e.set_r(3, d)


STUBS.imp("strlen", st_strlen)
STUBS.imp("__nwa__FUl", st_new_array)
STUBS.imp("__nw__FUl", st_new)
STUBS.imp("__dl__FPv", st_delete)
STUBS.imp("strcpy", st_strcpy)
STUBS.imp("__throw", st_throw)
STUBS.internal(ITER_COPY, st_itercopy)


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n))) if n > 0 else []


class Ppc(Side):
    fmt = ">I"
    base = 0

    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, case["queues"])
        self.L = e.cl
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

    def call(self, D, itd, idx, itp, count, val):
        T.call_ppc(self.e, INSERT, [D & MASK, itd & MASK, idx & MASK, itp & MASK, count & MASK, val & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        self.base = lib.tpw_cdi_addr(b"arena")

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cdi_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cdi_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, D, itd, idx, itp, count, val):
        self.lib.tpw_cdi_insert(D, itd, idx & MASK, itp, count & MASK, val)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cdi_addr.restype, lib.tpw_cdi_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cdi_alloc.restype, lib.tpw_cdi_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cdi_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cdi_insert.restype = None
    lib.tpw_cdi_insert.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint64, C.c_uint32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cdi_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    cap, size, start = case["cap"], case["size"], case["start"]
    s.words(s.g("mincap"), [case["mincap"]])
    D = s.alloc(16)
    blk = s.alloc(cap * 4) if cap > 0 else s.base
    off = (blk - s.base) & MASK
    if cap > 0:
        s.words(blk, case["words"])
    s.words(D, [cap, size, start, off])
    idx = case["idx"]
    sm = start + idx
    ptr = blk + 4 * (sm if sm < cap else sm - cap) if cap > 0 else s.base
    val = s.alloc(4)
    s.words(val, [case["value"]])
    CUR["D"] = D
    return dict(D=D, blk=blk, off=off, ptr=ptr, val=val)


def run(s, case):
    try:
        w = build(s, case)
        s.call(w["D"], w["D"], case["idx"], w["ptr"], case["count"], w["val"])
        cap, size, start, off = s.rd_words(w["D"], 4)
        out = {"hdr": [cap, size, start], "newblk": off != w["off"], "log": s.log(), "value": s.rd_words(w["val"], 1)}
        if case["cap"] > 0:
            out["old"] = s.rd_words(w["blk"], case["cap"])
        if off != w["off"] and cap < 1000:
            out["new"] = s.rd_words(s.base + off, cap)
        return out
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, cap, size, start, idx, count, mincap=1):
    words = [r.getrandbits(32) for _ in range(cap)]
    return {"queues": {}, "cap": cap, "size": size, "start": start, "idx": idx, "count": count, "mincap": mincap,
            "words": words, "value": r.getrandbits(32)}


def edges():
    r = random.Random(51030)
    for cap in range(0, 6):
        for size in range(0, cap + 1):
            for start in range(0, max(cap, 1)):
                for idx in range(0, size + 1):
                    for count in sorted({0, 1, 2, 3, cap - size, cap - size + 1}):
                        if count < 0:
                            continue
                        yield mk(r, cap, size, start, idx, count, r.choice([1, 4, 8]))
    for big, size in ((0x40000000, 0), (0x40000000, 3), (5, 0x3FFFFFFB), (7, 0x3FFFFFFF), (1, 0x3FFFFFFF), (0xFFFFFFFF, 0), (3, 0x3FFFFFFD)):
        c = mk(r, 0, size, 0, 0, big)
        yield c


def rnd(r):
    cap = r.randrange(0, 13)
    size = r.randrange(0, cap + 1)
    start = r.randrange(0, max(cap, 1))
    idx = r.randrange(0, size + 1)
    return mk(r, cap, size, start, idx, r.randrange(0, 9), r.choice([1, 2, 4, 8, 16]))


TESTS = [
    FnTest("std::__cdeque<const_void*,std::allocator<const_void*>>::insert", ppc, host, edge=edges, rnd=rnd),
]
