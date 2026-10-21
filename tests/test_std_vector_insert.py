"""Unit v5916, the Metrowerks std::vector<const void*> insert / erase / resize and std::__locale_imp<__int128>::~__locale_imp, against
libtpw_unit_std_vector_insert.dylib (tests/unit_std_vector_insert.cpp), main app ("SimTheme Park"),
src/game/MslSupport.cpp:
  insert 0x10073ed4, erase 0x10073e10, resize 0x10073d58, ~__locale_imp 0x100737a0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit are fakes on both sides: operator new / new[] (log the size; the host vector elements are 8 bytes, so a vector
allocation is logged at half size), operator delete / delete[] (log the tag stored 8 bytes before the block), strlen, strcpy, __throw
(logs the vtable, the reference count, the type and the message; then the case ends: the original would go on with a size nobody can
allocate, so the PowerPC stub stops the emulator and the host fake throws a C++ exception) and the facets' virtual destructor
(__ptr_glue; logs the tag at facet + 8 and the flag).

The world of a case: a vector (capacity, size, data, elements as tokens) with its data block tagged, a value block, and for the
destructor an impl with facets (tags, reference counts, null slots) and a reference counted character array. What is compared: the
answer, the vector header and the whole capacity of its data (elements as tokens), the facet and array reference counts, the event
log. No fused operations. Not tested: a length error whose fall-through would run (n = 0x3fffffff - size exactly is not a throw but
needs a 4 GB allocation), a NULL character array in the destructor (the original reads it unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_vector_insert"
MASK = 0xFFFFFFFF

INSERT, ERASE, RESIZE, DTOR = 0x10073ed4, 0x10073e10, 0x10073d58, 0x100737a0
SIZES = {INSERT: 736, ERASE: 116, RESIZE: 108, DTOR: 352}
OWN = set(SIZES)

G_STRLEN, G_NEWA, G_NEW, G_DELA, G_THROW, G_DEL, G_STRCPY, G_PTRGLUE = (
    0x10238650, 0x10238668, 0x102384b8, 0x10238638, 0x10238680, 0x10238218, 0x10238410, 0x1023baa8)
LENGTH_ERROR_VTBL, LENGTH_ERROR_DTOR_TV = 0x10278b74, 0x10265680

HOST_LAYOUT = [4, 8, 16, 16, 8, 16, 4]


def fnv(b):
    if not isinstance(b, bytes):
        b = b.encode("latin1")
    h = 2166136261
    for c in b:
        h = ((h ^ c) * 16777619) & MASK
    return h


def cbytes(e, a):
    s = e.cstr(a)
    return s if isinstance(s, bytes) else s.encode("latin1")


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_strlen(e):
    e.set_r(3, len(cbytes(e, e.r(3))))


def st_strcpy(e):
    e.write(e.r(3), cbytes(e, e.r(4)) + b"\0")


def st_new(e):
    e.cl.rec("new", (e.r(3),))
    e.set_r(3, e.alloc(e.r(3)))


def st_newa(e):
    e.cl.rec("new[]", (e.r(3),))
    e.set_r(3, e.alloc(e.r(3)))


def st_del(e):
    e.cl.rec("delete", (e.u32(e.r(3) - 8),))


def st_dela(e):
    e.cl.rec("delete[]", (e.u32(e.r(3) - 8),))


def st_throw(e):
    obj = e.r(4)
    ptr, cnt = e.u32(obj + 4), e.u32(obj + 8)
    msg = cbytes(e, ptr)
    e.cl.rec("throw", (1 if e.u32(obj) == LENGTH_ERROR_VTBL else 0, T.s32(e.u32(cnt)) if cnt else -1,
                       1 if e.r(5) == LENGTH_ERROR_DTOR_TV else 0, fnv(cbytes(e, e.r(3))), fnv(msg), len(msg)))
    e.aborted = True
    e.uc.emu_stop()


def st_ptrglue(e):
    e.cl.rec("FacetDelete", (e.u32(e.r(3) + 8), T.s32(e.r(4))))


def glue(fn):
    """The import glue saves the TOC pointer in the caller's frame (0x14(r1)), which the code reloads after the call."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return wrapped


STUBS.internal(G_STRLEN, glue(st_strlen))
STUBS.internal(G_STRCPY, glue(st_strcpy))
STUBS.internal(G_NEW, glue(st_new))
STUBS.internal(G_NEWA, glue(st_newa))
STUBS.internal(G_DEL, glue(st_del))
STUBS.internal(G_DELA, glue(st_dela))
STUBS.internal(G_THROW, glue(st_throw))
STUBS.internal(G_PTRGLUE, glue(st_ptrglue))


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def tagged(self, size, tag):
        """A block whose tag word sits 8 bytes before it (the delete fakes log it)."""
        a = self.alloc(size + 16)
        self.words(a + 8, [tag])
        return a + 16

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def ptrs(self, a, vs):
        self.write(a, b"".join(struct.pack(self.pfmt, v) for v in vs))

    def rd_ptrs(self, a, n):
        return list(struct.unpack(self.pfmt[0] + "%d%s" % (n, self.pfmt[1]), self.read(a, self.P * n)))

    def rd_ptr(self, a):
        return self.rd_ptrs(a, 1)[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    P = 4
    O_CHARS, O_CREFS, O_CDATA, SZ_IMP, SZ_CHARS = 0xC, 8, 0xC, 12, 16

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        e.aborted = False
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

    def call(self, entry, args):
        try:
            r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        except Exception:
            if not self.e.aborted:
                raise
            return 0
        return r3 & MASK

    def insert(self, vec, pos, n, val):
        self.call(INSERT, [vec, pos, n, val])
        return 1 if self.e.aborted else 0

    def erase(self, vec, first, last):
        return self.call(ERASE, [vec, first, last])

    def resize(self, vec, n, val):
        self.call(RESIZE, [vec, n, val])
        return 0

    def dtor(self, this, flag):
        return self.call(DTOR, [this, flag])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    P = 8
    O_CHARS, O_CREFS, O_CDATA, SZ_IMP, SZ_CHARS = 16, 8, 16, 24, 24

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_vi_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def insert(self, vec, pos, n, val):
        return self.lib.tpw_vi_insert(vec, pos, n & MASK, val)

    def erase(self, vec, first, last):
        return self.lib.tpw_vi_erase(vec, first, last)

    def resize(self, vec, n, val):
        self.lib.tpw_vi_resize(vec, n & MASK, val)
        return 0

    def dtor(self, this, flag):
        return self.lib.tpw_vi_dtor(this, T.s16(flag))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_vi_addr.restype, lib.tpw_vi_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_vi_alloc.restype, lib.tpw_vi_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_vi_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_vi_insert.restype, lib.tpw_vi_insert.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint64]
    lib.tpw_vi_erase.restype, lib.tpw_vi_erase.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_vi_resize.restype, lib.tpw_vi_resize.argtypes = None, [C.c_uint64, C.c_uint32, C.c_uint64]
    lib.tpw_vi_dtor.restype, lib.tpw_vi_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int16]
    out = (C.c_int64 * 32)()
    n = lib.tpw_vi_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_vec(s, c):
    """The vector header (capacity, size, data) at Mac offsets 0, 4, 8 and the tagged data block; the value block."""
    vec = s.alloc(16)
    data = 0
    if not c["null"]:
        data = s.tagged(max(c["cap"], 1) * s.P, 0x7000)
        s.ptrs(data, c["elems"])
    s.words(vec, [c["cap"], c["size"]])
    s.ptrs(vec + 8, [data])
    val = s.alloc(8)
    s.ptrs(val, [c["val"]])
    if c.get("alias") is not None:
        val = data + c["alias"] * s.P          # the value is an element of the vector itself
    return vec, data, val


def run_vec(s, c):
    vec, data, val = build_vec(s, c)
    op = c["op"]
    if op == "insert":
        ans = s.insert(vec, data + c["idx"] * s.P, c["n"], val)
    elif op == "erase":
        ans = (s.erase(vec, data + c["first"] * s.P, data + c["last"] * s.P) - data) // s.P
    else:
        ans = s.resize(vec, c["n"], c["val"])      # the value itself, not its address
    cap, size = s.rd_words(vec, 2)
    nd = s.rd_ptr(vec + 8)
    return {"answer": ans, "log": s.log(), "cap": cap, "size": size, "same_data": nd == data,
            "elems": s.rd_ptrs(nd, cap) if nd and cap < 5000 else None}


def run_dtor(s, c):
    nf = len(c["facets"])
    impl = s.tagged(s.SZ_IMP, 0x7100)
    fobjs = []
    for i, f in enumerate(c["facets"]):
        if f is None:
            fobjs.append(0)
        else:
            a = s.alloc(16)
            s.words(a, [0x10270000])
            s.words(a + 4, [f & MASK])
            s.words(a + 8, [0x500 + i])
            fobjs.append(a)
    ref = {}
    for i, j in enumerate(c["alias"]):
        if j is not None:
            fobjs[i] = fobjs[j]
    data = s.tagged(max(nf, 1) * s.P, 0x7200) if nf else 0
    if nf:
        s.ptrs(data, fobjs)
    chars = s.tagged(s.SZ_CHARS, 0x7300)
    cdata = s.tagged(8, 0x7400)
    s.words(chars + 8, [c["crefs"]])
    s.ptrs(chars + s.O_CDATA, [cdata])
    s.words(impl, [nf, nf])
    s.ptrs(impl + 8, [data])
    s.ptrs(impl + s.O_CHARS, [chars])
    this = 0 if c["null_this"] else impl
    ans = s.dtor(this, c["flag"])
    out = {"same_this": ans == this, "log": s.log(), "crefs": s.rd_word(chars + 8)}
    if not c["null_this"]:
        out["facet_refs"] = [None if a == 0 else T.s32(s.rd_word(a + 4)) for a in fobjs]
        out["chars_ptr"] = ("same" if s.rd_ptr(impl + s.O_CHARS) == chars else "null" if s.rd_ptr(impl + s.O_CHARS) == 0 else "other")
        out["vec"] = s.rd_words(impl, 2)
    return out


def run(s, c):
    try:
        return run_dtor(s, c) if c["op"] == "dtor" else run_vec(s, c)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the cases


def vec_base(r, op, cap=None, size=None, null=False):
    if size is None:
        size = r.choice([0, 1, 2, 3, 5, 8, 13])
    if cap is None:
        cap = size + r.choice([0, 0, 1, 2, 5, 9])
    if cap == 0 and size == 0 and r.random() < 0.5:
        null = True
    if null:
        cap = size = 0
    return {"op": op, "queues": {}, "cap": cap, "size": size, "null": null,
            "elems": [r.randrange(1, 1 << 31) for _ in range(cap)], "val": r.randrange(1, 1 << 31)}


def ins_case(r, size=None, cap=None, idx=None, n=None, null=False, alias=False):
    c = vec_base(r, "insert", cap, size, null)
    size = c["size"]
    c["idx"] = r.randrange(0, size + 1) if idx is None else min(idx, size)
    c["n"] = r.choice([1, 1, 2, 3, 7, 20]) if n is None else n
    c["alias"] = r.randrange(0, size) if alias and size else None
    return c


def ins_edges():
    r = random.Random(5001)
    yield ins_case(r, n=0)
    yield ins_case(r, size=0, cap=0, null=True, n=1)
    yield ins_case(r, size=0, cap=0, n=1)
    yield ins_case(r, size=0, cap=4, n=4, idx=0)                 # exact fit, nothing to shift
    yield ins_case(r, size=0, cap=4, n=5, idx=0)                 # grows
    yield ins_case(r, size=3, cap=8, n=2, idx=3)                 # append in place
    yield ins_case(r, size=3, cap=8, n=2, idx=0)                 # shift all
    yield ins_case(r, size=3, cap=8, n=2, idx=1)
    yield ins_case(r, size=3, cap=5, n=2, idx=1)                 # exact fit with a shift
    yield ins_case(r, size=3, cap=3, n=1, idx=3)                 # realloc, append
    yield ins_case(r, size=3, cap=3, n=1, idx=0)                 # realloc, front
    yield ins_case(r, size=3, cap=3, n=1, idx=1)
    yield ins_case(r, size=5, cap=5, n=40, idx=2)                # several doublings
    yield ins_case(r, size=5, cap=5, n=5, idx=5)                 # 10 exactly
    yield ins_case(r, size=5, cap=5, n=6, idx=5)
    yield ins_case(r, size=4, cap=8, n=3, idx=1, alias=True)
    yield ins_case(r, size=4, cap=4, n=3, idx=1, alias=True)
    yield ins_case(r, size=4, cap=8, n=3, idx=4, alias=True)
    # length errors: the fakes end the case at the throw
    yield ins_case(r, size=0, cap=0, n=0x40000000)
    yield ins_case(r, size=2, cap=2, n=0xFFFFFFFF)
    yield ins_case(r, size=3, cap=3, n=0x3FFFFFFF)
    yield ins_case(r, size=2, cap=4, n=0x3FFFFFFE)
    yield ins_case(r, size=1, cap=1, n=0x3FFFFFFF)


def ins_random(r):
    return ins_case(r, alias=r.random() < 0.3, n=r.choice([None, None, r.randrange(1, 70)]))


def erase_case(r, size=None, first=None, last=None):
    c = vec_base(r, "erase", None, size)
    size = c["size"]
    f = r.randrange(0, size + 1) if first is None else min(first, size)
    l = r.randrange(f, size + 1) if last is None else min(last, size)
    c["first"], c["last"] = f, max(f, l)
    return c


def erase_edges():
    r = random.Random(5002)
    yield erase_case(r, size=0, first=0, last=0)
    yield erase_case(r, size=5, first=2, last=2)
    yield erase_case(r, size=5, first=0, last=5)                # all
    yield erase_case(r, size=5, first=2, last=5)                # tail: nothing to move
    yield erase_case(r, size=5, first=0, last=1)
    yield erase_case(r, size=5, first=1, last=4)
    yield erase_case(r, size=1, first=0, last=1)
    yield erase_case(r, size=6, first=5, last=6)


def erase_random(r):
    return erase_case(r)


def resize_case(r, size=None, n=None):
    c = vec_base(r, "resize", None, size)
    c["n"] = r.randrange(0, c["size"] + 20) if n is None else n
    c["alias"] = None
    return c


def resize_edges():
    r = random.Random(5003)
    yield resize_case(r, size=4, n=4)
    yield resize_case(r, size=0, n=0)
    yield resize_case(r, size=4, n=0)
    yield resize_case(r, size=4, n=3)
    yield resize_case(r, size=4, n=5)
    yield resize_case(r, size=0, n=3)
    yield resize_case(r, size=4, n=40)
    c = resize_case(r, size=3, n=6)
    c["cap"] = 10
    c["elems"] = [r.randrange(1, 1 << 31) for _ in range(10)]
    yield c


def resize_random(r):
    return resize_case(r)


def dtor_case(r, nf=None, flag=None, crefs=None, null_this=False):
    if nf is None:
        nf = r.choice([0, 1, 2, 3, 6])
    facets = [r.choice([None, 1, 1, 2, 0, -1, 3]) for _ in range(nf)]
    alias = [None] * nf
    for i in range(1, nf):
        if r.random() < 0.2:
            alias[i] = r.randrange(0, i)
    if flag is None:
        flag = r.choice([0, 1, -1, 0x7FFF, 0x8000, 0xFFFF, 2, r.randrange(0, 0x10000)]) | r.choice([0, 0, 0x12340000])
    return {"op": "dtor", "queues": {}, "facets": facets, "alias": alias,
            "crefs": r.choice([0, 1, 2, 3]) if crefs is None else crefs, "flag": flag, "null_this": null_this}


def dtor_edges():
    r = random.Random(5004)
    yield dtor_case(r, null_this=True)
    for nf in (0, 1, 3):
        for crefs in (0, 1, 2):
            for flag in (1, 0, -1 & 0xFFFF):
                yield dtor_case(r, nf=nf, crefs=crefs, flag=flag)
    c = dtor_case(r, nf=3, flag=1, crefs=1)
    c["facets"] = [1, 1, None]
    c["alias"] = [None, 0, None]
    yield c
    c = dtor_case(r, nf=2, flag=0x8000, crefs=1)
    c["facets"] = [1, 2]
    yield c
    c = dtor_case(r, nf=2, flag=0x10001, crefs=3)
    c["facets"] = [0, -1]
    yield c


def dtor_random(r):
    return dtor_case(r)


TESTS = [
    FnTest("std::vector<const_void*,std::allocator<const_void*>>::insert", ppc, host, edge=ins_edges, rnd=ins_random),
    FnTest("std::vector<const_void*,std::allocator<const_void*>>::erase", ppc, host, edge=erase_edges, rnd=erase_random),
    FnTest("std::vector<const_void*,std::allocator<const_void*>>::resize", ppc, host, edge=resize_edges, rnd=resize_random),
    FnTest("std::__locale_imp<__int128>::~__locale_imp", ppc, host, edge=dtor_edges, rnd=dtor_random),
]
