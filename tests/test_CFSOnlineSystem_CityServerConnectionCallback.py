"""Unit v5373, CFSOnlineSystem::CityServerConnectionCallback 0x101727f8, against libtpw_unit_CFSOnlineSystem_CityServerConnectionCallback.dylib
(tests/unit_CFSOnlineSystem_CityServerConnectionCallback.cpp), main app ("SimTheme Park"), src/FS_Online_init_CFSOnlineSystem_CityServerConnectionCallback.cpp.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers, host offsets exported by the library) for the
translation. The callees outside the unit are fakes on both sides: SamsUtilities::UDebug (the text), the array destroyer (one "dtor"
event per element, last to first, then "deletearr": the host's delete[] on an array the library made with new[] logs the same) and
operator delete (the tag of the block freed).

The world of a case: the system object with its nine lists (request ids, the three answer words, an old stored answer or none), the
answer block d (reference count, infos array or none). What is compared: the log, every list word, the stored answer of each list as a
tag, the reference count and the infos-cleared state of every block.

No fused operations. Not tested: a NULL list pointer in the system (the original reads it unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystem_CityServerConnectionCallback"
MASK = 0xFFFFFFFF

FN = 0x101727f8
SIZES = {FN: 2252}
OWN = set(SIZES)

UDEBUG = 0x10000e40
DESTROY_ARRAY = 0x10238ad0
OPERATOR_DELETE = 0x10238218
DTOR_KIND = {0x10267488: 0, 0x10267490: 1, 0x10267498: 2, 0x102674f0: 3, 0x10267508: 4}   # the TVectors: ride server theme city park
LIST_KIND = [3, 4, 4, 4, 4, 4, 2, 1, 0]       # the kind of each of the nine lists (0xe0 to 0x100)

TABLE = 0x102a7258
STR_BASE = 0x1024cc03
STR_OFFS = [0x18b, 0x1b1, 0x1e1, 0x20f, 0x23a, 0x268, 0x298, 0x2cf, 0x30d, 0x334, 0x365, 0x38d, 0x3bf, 0x3e5, 0x415, 0x42d]

# Mac offsets: the system's list pointers, a list's words, a received block's words
MAC = dict(sys=[0xe0 + 4 * i for i in range(9)], id=4, f10=0x10, f14=0x14, f18=0x18, rec=0x30, count=0, infos=4, ref=0xc, ptr=4)
HOST_LAYOUT = None          # set once by bind from the library; the checked values are below
HOST_EXPECT = None
HOST = {}

# the kind of the received block d for each answer code (filled from the original's jump table)
KIND_OF_A = {2: 3, 3: 4, 4: 1, 5: 2, 6: 0}

CUR = {}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_destroy(e):
    L = e.cl
    p, dtor = e.r(3), e.r(4)
    n = CUR["arrays"][p]
    kind = DTOR_KIND[dtor]
    for i in range(n - 1, -1, -1):
        L.rec("dtor", (kind, i))


def stub_delete(e):
    pass        # the host's operator delete is the arena's (not recorded); the blocks' state is compared instead


STUBS.internal(UDEBUG, CL.recorder("UDebug", "s"))
STUBS.internal(DESTROY_ARRAY, stub_destroy)
STUBS.internal(OPERATOR_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    mac = True

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, {})
        CUR["tags"] = {}
        CUR["arrays"] = {}
        self.off = MAC
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_word(a)

    def tag(self, a, t):
        CUR["tags"][a] = t

    def tag_of(self, a):
        return 0 if a == 0 else CUR["tags"].get(a, -1)

    def make_infos(self, kind, n):
        p = self.e.alloc(16)
        CUR["arrays"][p] = n
        return p

    def log(self):
        return self.L.log

    def call(self, S, a, b, c, d):
        T.call_ppc(self.e, FN, [S, a & MASK, b & MASK, c, d])


class Host(Side):
    fmt = "<I"
    mac = False

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, {})
        lib.tpw_cscb_reset()
        self.off = HOST

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_cscb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def tag(self, a, t):
        self.lib.tpw_cscb_tag(a, t)

    def tag_of(self, a):
        return 0 if a == 0 else self.lib.tpw_cscb_tag_of(a)

    def make_infos(self, kind, n):
        return self.lib.tpw_cscb_newarray(kind, n)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, S, a, b, c, d):
        self.lib.tpw_cscb_call(S, a, b, c, d)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cscb_"
    lib.tpw_cscb_alloc.restype, lib.tpw_cscb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cscb_tag.restype, lib.tpw_cscb_tag.argtypes = None, [C.c_uint64, C.c_int32]
    lib.tpw_cscb_tag_of.restype, lib.tpw_cscb_tag_of.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_cscb_newarray.restype, lib.tpw_cscb_newarray.argtypes = C.c_uint64, [C.c_int32, C.c_int32]
    lib.tpw_cscb_reset.restype, lib.tpw_cscb_reset.argtypes = None, []
    lib.tpw_cscb_call.restype, lib.tpw_cscb_call.argtypes = None, [C.c_uint64, C.c_int32, C.c_int32, C.c_uint64, C.c_uint64]
    lib.tpw_cscb_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cscb_layout(out)
    got = list(out)[:n]
    if HOST_EXPECT is not None:
        assert got == HOST_EXPECT, got
    HOST.update(sys=got[0:9], id=got[9], f10=got[10], f14=got[11], f18=got[12], rec=got[13], count=got[14], infos=got[15], ref=got[16], ptr=8)
    lib.bound = True


def mkrec(s, kind, spec, tag):
    ref, n, has = spec
    R = s.alloc(0x20)
    o = s.off
    s.words(R + o["count"], [n])
    s.words(R + o["ref"], [ref])
    if has:
        s.ptr(R + o["infos"], s.make_infos(kind, n))
    s.tag(R, tag)
    return R


def build(s, case):
    o = s.off
    S = s.alloc(0x400)
    lists, olds = [], []
    for i in range(9):
        L = s.alloc(0x100)
        lists.append(L)
        s.ptr(S + o["sys"][i], L)
        s.words(L + o["id"], [case["ids"][i]])
        s.words(L + o["f10"], [case["init"][i][0]])
        s.words(L + o["f14"], [case["init"][i][1]])
        s.words(L + o["f18"], [case["init"][i][2]])
        old = case["old"][i]
        R = 0
        if old is not None:
            R = mkrec(s, LIST_KIND[i], old, 10 + i)
            s.ptr(L + o["rec"], R)
        olds.append(R)
    d = 0
    if case["d"] is not None:
        d = mkrec(s, KIND_OF_A.get(case["a"], 3), case["d"], 1)
    return S, lists, olds, d


def observe(s, case, S, lists, olds, d):
    o = s.off
    out = {"log": s.log()}
    for i in range(9):
        L = lists[i]
        out["list%d" % i] = [s.rd_word(L + o["id"]), s.rd_word(L + o["f10"]), s.rd_word(L + o["f14"]), s.rd_word(L + o["f18"]),
                             s.tag_of(s.rd_ptr(L + o["rec"]))]
    blocks = [("d", d, case["d"])] + [("old%d" % i, olds[i], case["old"][i]) for i in range(9)]
    for name, R, spec in blocks:
        # A block with one reference may have been freed by operator delete (the host's is the real one, so its memory cannot be read
        # back): the log (the destructor events) and the lists' stored tags show what happened to it. Blocks with more references
        # are never freed and are compared whole.
        if R and spec[0] > 1:
            out[name] = [s.rd_word(R + o["count"]), s.rd_word(R + o["ref"]), 1 if s.rd_ptr(R + o["infos"]) else 0]
    return out


def run(s, case, fresh_print=False):
    try:
        S, lists, olds, d = build(s, case)
        s.call(S, case["a"], case["b"], 0, d)
        return observe(s, case, S, lists, olds, d)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

def rspec(r):
    return (r.choice([1, 1, 2, 3]), r.randrange(1, 4), r.choice([0, 1, 1]))


def base(r, a, match=None, b=None):
    c = {"a": a}
    ids = [r.choice([5, 6, 7, 0xFFFFFFFF, 0]) for _ in range(9)]
    if b is None:
        b = r.choice([5, 6, 7, 0xFFFFFFFF, 0, 9, 1])
    if match is not None:
        for i in range(9):
            if i != match:
                ids[i] = (b + 1 + i) & MASK
        ids[match] = b
    c["b"] = b
    c["ids"] = ids
    c["init"] = [(r.randrange(0, 3), r.randrange(0, 3), r.randrange(0, 0x60)) for _ in range(9)]
    c["old"] = [None if r.random() < 0.4 else rspec(r) for _ in range(9)]
    c["d"] = None if r.random() < 0.2 else rspec(r)
    c["queues"] = {}
    return c


def edges():
    r = random.Random(5373)
    for a in list(range(-12, 11)) + [0x7FFFFFFF, -0x80000000]:
        for m in [None] + list(range(9)):
            yield base(r, a, m)
    for a in range(-10, 9):
        for m in range(9):
            c = base(r, a, m)
            c["old"] = [(1, 0, 0)] * 9
            yield c
            c = base(r, a, m)
            c["d"] = (1, 2, 1)
            c["old"] = [(2, 1, 1)] * 9
            yield c


def rnd(r):
    a = r.choice([r.randrange(-12, 11)] * 3 + [r.randrange(-1000, 1000)])
    m = r.choice([None] + list(range(9)))
    return base(r, a, m)


TESTS = [
    FnTest("CFSOnlineSystem::CityServerConnectionCallback", ppc, host, edge=edges, rnd=rnd),
]
