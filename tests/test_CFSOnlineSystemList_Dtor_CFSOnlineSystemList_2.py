"""Unit v5921, three destructors of the online system, against libtpw_unit_CFSOnlineSystemList_Dtor_CFSOnlineSystemList_2.dylib
(tests/unit_CFSOnlineSystemList_Dtor_CFSOnlineSystemList_2.cpp), main app ("SimTheme Park"),
src/CFSOnlineSystemList_Dtor_CFSOnlineSystemList_2.cpp:
  CFSOnlineSystemList<TbPtrList<CFSOnlinePark>,ccParkList>::~CFSOnlineSystemList 0x1017004c, ccCityInfo::~ccCityInfo 0x101703e4,
  CFSOnlineSystem::~CFSOnlineSystem 0x1017055c

A case is a world and a call. The world is built twice from one description: in emulator memory at the Mac's layout for the original, and
in host objects (the host's pointers are 64 bits, so the objects are built and read by the library's helpers) for the translation. The
callees outside the unit are fakes on both sides that record their arguments: RemoveHead (answers the case's queue), the virtual delete
through __ptr_glue ("vdel"; the lists' and the park's deleting destructors), operator delete, free, __destroy_new_array (the host's
~ccParkInfo), the string and list destructors (named by their offset in the system object), Logout and the three Release functions.

What is compared: the event log, and the words the destructors write (the list's two words and the stored answer's pointer and count; the
three blocks of ccCityInfo). Not tested: this == NULL (the original returns at once; C++ cannot call a destructor on NULL), an answer
with more than one info (the host's array destructor logs one event per element, the Mac's __destroy_new_array one in all), the vtable
words the compiler stores. No fused operations.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystemList_Dtor_CFSOnlineSystemList_2"
MASK = 0xFFFFFFFF

FN_LIST, FN_CITY, FN_SYS = 0x1017004C, 0x101703E4, 0x1017055C
SIZES = {FN_LIST: 288, FN_CITY: 164, FN_SYS: 684}
OWN = set(SIZES)

REMOVE_HEAD = 0x10177B98
TBLIST_DTOR = 0x10177F80
LOGOUT, REL_NEWS, REL_UPLOAD, REL_FTP = 0x10171398, 0x1017249C, 0x1017598C, 0x101770DC
PTR_GLUE = 0x1023BAA8
G_DESTROY_ARRAY, G_DELETE, G_FREE, G_STR_C, G_STR_W = 0x10238AD0, 0x10238218, 0x10238A70, 0x102382F0, 0x10238710
SYS_SIZE = 0x404
HOST_LAYOUT = [4, 8, 0x1C]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def where(e, p):
    """A label for an address: a known object, 1000 + the offset inside the system object, or -1."""
    L = e.cl
    b = getattr(L, "sysbase", 0)
    if b and b < p < b + SYS_SIZE:
        return 1000 + p - b
    return L.lab(p)


def stub_ptr_glue(e):
    L = e.cl
    p, flag = e.r(3), s32(e.r(4))
    is_list = L.lab(p) == getattr(L, "listlabel", -99)   # the host's real list destructor logs nothing (the list is empty by then)
    if not is_list:
        L.rec("vdel", (where(e, p), flag))
    if T.s16(flag) > 0:
        L.rec("delete", (where(e, p),))


def rec(name):
    def fn(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)),))
    return fn


def stub_where(name):
    def fn(e):
        e.cl.rec(name, (where(e, e.r(3)),))
    return fn


STUBS.internal(PTR_GLUE, stub_ptr_glue)
# ~TbTList on the empty list the case builds: the host's real one (RemoveAll of nothing) logs nothing, so the original's stands silent.
STUBS.internal(TBLIST_DTOR, lambda e: None)
STUBS.internal(LOGOUT, rec("Logout"))
STUBS.internal(REL_NEWS, rec("ReleaseNews"))
STUBS.internal(REL_UPLOAD, rec("ReleaseUpload"))
STUBS.internal(REL_FTP, rec("ReleaseFTP"))
STUBS.internal(G_DESTROY_ARRAY, stub_where("destroy_array"))
STUBS.internal(G_DELETE, stub_where("delete"))
STUBS.internal(G_FREE, rec("free"))
STUBS.internal(G_STR_C, stub_where("str_dtor_c"))
STUBS.internal(G_STR_W, stub_where("str_dtor_w"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def words(self, a, ws):
        self.e.write(a, b"".join((w & MASK).to_bytes(4, "big") for w in ws))

    def word(self, a):
        return self.e.u32(a)

    def block(self, n):
        return self.e.alloc(n)

    def fake(self):
        """An object with a vtable the original can read (word 0 -> a block, its slot 2 is read)."""
        vt = self.e.alloc(16)
        o = self.e.alloc(16)
        self.words(o, [vt])
        return o

    def begin(self, fixed, queues, objs=None, sysbase=0):
        self.e.cl = CL.PpcLog({a: l for l, a in fixed.items() if a}, queues)
        self.e.cl.objs = {l: a for l, a in (objs or {}).items()}
        self.e.cl.sysbase = sysbase

    def log(self):
        return self.e.cl.log

    def call(self, entry, a, b):
        T.call_ppc(self.e, entry, [a & MASK, b & MASK])


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_dtors_base()

    def done(self):
        pass

    def words(self, a, ws):
        C.memmove(a, b"".join((w & MASK).to_bytes(4, "little") for w in ws), 4 * len(ws))

    def word(self, a):
        return C.c_uint32.from_address(a).value

    def block(self, n):
        return self.lib.tpw_dtors_alloc(n)

    def begin(self, fixed, queues, objs=None, sysbase=0):
        CL.host_begin(self.lib, {l: a for l, a in fixed.items() if a}, queues)

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_dtors_alloc.restype, lib.tpw_dtors_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_dtors_base.restype = C.c_uint64
    lib.tpw_dtors_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_dtors_mkobj.restype, lib.tpw_dtors_mkobj.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_dtors_city.argtypes = [C.c_uint64, C.c_int32]
    lib.tpw_dtors_t1_build.restype = C.c_uint64
    lib.tpw_dtors_t1_build.argtypes = [C.c_int32] * 6 + [C.POINTER(C.c_uint64)] * 2
    lib.tpw_dtors_t1_run.argtypes = [C.c_uint64, C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_dtors_t3_build.restype = C.c_uint64
    lib.tpw_dtors_t3_build.argtypes = [C.c_uint32, C.POINTER(C.c_uint64)]
    lib.tpw_dtors_t3_run.argtypes = [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_dtors_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the worlds


def off(s, a):
    """A Ptr32 word: the host's is an offset from the arena base; the Mac's is the address."""
    return a - s.base if isinstance(s, Host) and a else a


def run_city(s, case):
    blocks = [s.block(16) if case["has"][i] else 0 for i in range(3)]
    this = s.block(0x20)
    s.words(this, [0x55, off(s, blocks[0]), off(s, blocks[1])])
    s.words(this + 0x1C, [off(s, blocks[2])])
    s.begin({1: this, 2: blocks[0], 3: blocks[1], 4: blocks[2]}, case["queues"])
    if isinstance(s, Ppc):
        s.call(FN_CITY, this, case["flag"])
    else:
        s.lib.tpw_dtors_city(this, T.s32(case["flag"]))
    log = s.log()
    return {"log": log, "words": [s.word(this + 4), s.word(this + 8), s.word(this + 0x1C)]}


def run_list(s, case):
    n = case["count"]
    labels = [5 + i if case["parks"][i] else 0 for i in range(3)]
    queue = labels[:n] if n else [0]
    queues = {"RemoveHead": queue}
    if isinstance(s, Ppc):
        this = s.block(0x40)
        lst = s.block(0x20)
        rcv = s.block(0x10) if case["rcv"] else 0
        infos = s.block(0x80) if case["rcv"] and case["info"] else 0
        parks = {5 + i: s.fake() for i in range(3)}
        nodes = [s.block(16) for _ in range(n)]
        for i, nd in enumerate(nodes):
            data = parks[5 + i] if case["parks"][i] else 0
            s.words(nd, [data, nodes[i + 1] if i + 1 < n else 0, nodes[i - 1] if i else 0])
        s.words(this, [0, 11, 12, 13, 14, 15, 16, 17, 18, case["f24"], case["f28"], lst, rcv])
        s.words(lst, [s.block(16), nodes[0] if n else 0, nodes[-1] if n else 0, n])
        if rcv:
            s.words(rcv, [n, infos, 0, case["refc"]])
        fixed = {1: this, 2: lst, 3: rcv, 4: infos}
        fixed.update(parks)
        fixed.update({20 + i: nd for i, nd in enumerate(nodes)})
        s.begin(fixed, queues, parks)
        s.e.cl.listlabel = 2
        s.call(FN_LIST, this, case["flag"])
        alive = case["rcv"] and s32(case["refc"] - 1) != 0
        snap = [s.word(this + 0x24), s.word(this + 0x28), 1 if s.word(this + 0x30) else 0,
                s.word(rcv + 0xC) if alive else 0]
        return {"log": s.log(), "words": snap}
    out = (C.c_uint64 * 8)()
    parks = {5 + i: s.lib.tpw_dtors_mkobj(5 + i) for i in range(3)}
    arr = (C.c_uint64 * 3)(*[parks[5 + i] if case["parks"][i] else 0 for i in range(3)])
    this = s.lib.tpw_dtors_t1_build(n, case["rcv"], s32(case["refc"]), case["info"], s32(case["f24"]), s32(case["f28"]), arr, out)
    fixed = {1: this, 2: out[1], 3: out[2], 4: out[3]}
    fixed.update(parks)
    fixed.update({20 + i: out[4 + i] for i in range(n)})
    s.begin(fixed, queues)
    snap = (C.c_int64 * 4)()
    s.lib.tpw_dtors_t1_run(this, s32(case["flag"]), snap)
    return {"log": s.log(), "words": [v & MASK for v in snap]}


def run_sys(s, case):
    mask = case["mask"]
    if isinstance(s, Ppc):
        this = s.block(SYS_SIZE + 16)
        objs = {}
        for i in range(10):
            if mask & (1 << i):
                objs[2 + i] = s.fake()
                s.words(this + (0xE0 + 4 * i if i < 9 else 0x3A4), [objs[2 + i]])
        fixed = {1: this}
        fixed.update(objs)
        s.begin(fixed, case["queues"], None, this)
        s.call(FN_SYS, this, case["flag"])
        return {"log": s.log()}
    out = (C.c_uint64 * 10)()
    this = s.lib.tpw_dtors_t3_build(mask, out)
    fixed = {1: this}
    for i in range(10):
        if out[i]:
            fixed[2 + i] = out[i]
    s.begin(fixed, case["queues"])
    s.lib.tpw_dtors_t3_run(this, s32(case["flag"]))
    lg = s.log()
    return {"log": lg}


def make(fn):
    def runner(side, case):
        try:
            return fn(side, case)
        finally:
            side.done()

    return runner


def ppc_of(fn):
    return lambda e, case: make(fn)(Ppc(e, case), case)


def host_of(fn):
    return lambda lib, case: make(fn)(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the cases

FLAGS = [-1, 0, 1, 7, 0x8000, 0x10000, 0x7FFF, -32768 & MASK]


def city_case(r, has=None, flag=None):
    return {"queues": {}, "has": [r.randrange(2) for _ in range(3)] if has is None else has,
            "flag": r.choice(FLAGS) if flag is None else flag}


def city_edges():
    r = random.Random(2101)
    for m in range(8):
        yield city_case(r, [(m >> i) & 1 for i in range(3)], 1)
    for f in FLAGS:
        yield city_case(r, [1, 1, 1], f)
        yield city_case(r, [0, 0, 0], f)


def list_case(r, count=None, rcv=None, refc=None, flag=None, info=None):
    return {"queues": {}, "count": r.randrange(4) if count is None else count,
            "parks": [r.randrange(2) for _ in range(3)],
            "rcv": r.randrange(2) if rcv is None else rcv,
            "refc": r.choice([0, 1, 1, 2, 7, MASK]) if refc is None else refc,
            "info": r.randrange(2) if info is None else info,
            "f24": r.choice([0, r.getrandbits(32)]) , "f28": r.choice([0, r.getrandbits(32)]),
            "flag": r.choice(FLAGS) if flag is None else flag}


def list_edges():
    r = random.Random(2102)
    for count in range(4):
        for rcv, refc, info in ((0, 1, 0), (1, 1, 0), (1, 1, 1), (1, 2, 1), (1, 0, 1), (1, MASK, 0)):
            c = list_case(r, count, rcv, refc, 1, info)
            c["parks"] = [1, 0, 1]
            yield c
    for f in FLAGS:
        yield list_case(r, 2, 1, 1, f, 1)


def sys_case(r, mask=None, flag=None):
    return {"queues": {}, "mask": r.getrandbits(10) if mask is None else mask,
            "flag": r.choice(FLAGS) if flag is None else flag}


def sys_edges():
    r = random.Random(2103)
    yield sys_case(r, 0, 1)
    yield sys_case(r, 0x3FF, 1)
    for i in range(10):
        yield sys_case(r, 1 << i, 1)
    for f in FLAGS:
        yield sys_case(r, 0x3FF, f)
        yield sys_case(r, 0, f)


TESTS = [
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlinePark>,ccParkList>::~CFSOnlineSystemList", ppc_of(run_list), host_of(run_list),
           edge=list_edges, rnd=lambda r: list_case(r)),
    FnTest("ccCityInfo::~ccCityInfo", ppc_of(run_city), host_of(run_city), edge=city_edges, rnd=lambda r: city_case(r)),
    FnTest("CFSOnlineSystem::~CFSOnlineSystem", ppc_of(run_sys), host_of(run_sys), edge=sys_edges, rnd=lambda r: sys_case(r)),
]
