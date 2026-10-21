"""Unit v5063, the park list "in the world by player name", against libtpw_unit_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor.dylib
(tests/unit_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor.cpp), main app ("SimTheme Park"),
src/game/online/FS_Online.cpp, src/game/online/FS_OnlineList.cpp:
  CFSOnlineSystemParkList_InWorldByPlayerName::~CFSOnlineSystemParkList_InWorldByPlayerName 0x101709b0,
  CFSOnlineSystemParkList_InWorldByPlayerName::GetList 0x101794e8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation (the object's members at the Mac's offsets on the
Mac, at the host's offsets, from tpw_pl_layout, on the host). The callees outside the unit are fakes on both sides that record the same
events: the string's destructor and virtual assignment, the system's constructor, AreWeLoggedIn and Login, PostLoginCallbackFn_Park,
ccParkInfo's destructor (once for each info of a freed answer), and, on the Mac, the virtual delete of the typed list. The park list
bases' destructors, which the Mac inlines into the derived destructor, are the host test library's fakes (the string member runs first,
then the stored answer is released, the request words 0x24 and 0x28 cleared and the typed list deleted).

The world of a case: the ten request words, the stored answer (none, or a block with a count, an infos array of 0 to 3 elements made with
new[] and a reference count), gOnlineSystem (null or a preset object), the answers of AreWeLoggedIn. What is compared: the answer (this),
the request words, whether the answer pointer and the infos pointer are set, the reference count, gOnlineSystem (null, the preset or a
new one), and the event log.

No fused operations. Not tested: the park records of a non-empty typed list (the host deletes them in the program's list code, not
in this unit), a NULL player name (the string's virtual assignment would read it), the second creation of the online system in GetList (the
first one makes it, nothing in between clears it), the size passed to operator new (the host's records nothing) and operator delete[] of
the infos (the host records nothing; the per-info destructor events are compared).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor"
MASK = 0xFFFFFFFF

FN_DTOR, FN_GET = 0x101709B0, 0x101794E8
SIZES = {FN_DTOR: 316, FN_GET: 364}
OWN = set(SIZES)

# the callees faked on both sides
STRING_DTOR = 0x10238710
DESTROY_ARRAY = 0x10238AD0
OP_DELETE = 0x10238218
OP_NEW = 0x102384B8
PTR_GLUE = 0x1023BAA8
SYS_CTOR = 0x1016F7D0
ARE_LOGGED = 0x1017178C
LOGIN = 0x101710AC
POST_LOGIN = 0x10178C74
TV_POST_LOGIN = 0x10267628       # the TVector of PostLoginCallbackFn_Park
TV_PARK_INFO_DTOR = 0x10267508   # the TVector of ccParkInfo's destructor

ADDR = dict(gOnlineSystem=0x10380AB0)
INTS = ["mRequestID", "mF08", "mF0C", "mF10", "mF14", "mF18", "mF1C", "mF20", "mF24", "mF28"]
MAC_OFF = dict(mRequestID=4, mF08=8, mF0C=0xC, mF10=0x10, mF14=0x14, mF18=0x18, mF1C=0x1C, mF20=0x20, mF24=0x24, mF28=0x28,
               mList=0x2C, mReceived=0x30, mPlayerName=0x34)
MAC_REC = dict(count=0, infos=4, ref=0xC, size=0x10)
HOST_NAMES = INTS + ["mList", "mReceived", "mPlayerName"]
HOST_LAYOUT = [8, 0xC, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x38, 0x40, 0, 8, 0x10, 0x14, 0x18]
HOST_OFF = dict(zip(HOST_NAMES, HOST_LAYOUT[:13]))
HOST_REC = dict(count=0, infos=8, ref=0x14, size=0x18)


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
INFOS = {}                 # Mac address of an infos array -> its element count (the array is made by the case)
VT = {}                    # the persistent vtables and function descriptors of the Mac objects

STRING_ASSIGN = CL.recorder("StringAssign", "LS")


def stub_string_dtor(e):
    L = e.cl
    L.rec("StringDtor", (L.lab(e.r(3)), s32(e.r(4))))


def stub_ptr_glue(e):
    """The call through the descriptor in r12: the string's assignment (slot 0x20) or the typed list's deleting destructor (slot 8)."""
    L = e.cl
    r12 = e.r(12)
    if r12 == VT["tv_assign"]:
        STRING_ASSIGN(e)
    elif r12 == VT["tv_list_dtor"]:
        assert e.r(4) == 1      # the typed list's deleting destructor: its operator delete
        L.rec("delete", (L.lab(e.r(3)),))
    else:
        raise AssertionError("unexpected __ptr_glue target %#x" % r12)


def stub_destroy_array(e):
    L = e.cl
    p = e.r(3)
    assert e.r(4) == TV_PARK_INFO_DTOR
    for _ in range(INFOS[p]):
        L.rec("ccParkInfoDtor", ())


def stub_op_delete(e):
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)),))


def stub_login(e):
    L = e.cl
    L.rec("Login", (L.lab(e.r(3)), 1 if e.r(4) == TV_POST_LOGIN else 0, L.lab(e.r(5))))


def stub_op_new(e):
    e.set_r(3, e.alloc(e.r(3)))


def glue(fn):
    """A call through the linker's glue saves the caller's TOC at 0x14(r1) (the code reloads r2 from there afterwards)."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return wrapped


STUBS.internal(OP_NEW, glue(stub_op_new))
STUBS.internal(STRING_DTOR, glue(stub_string_dtor))
STUBS.internal(PTR_GLUE, glue(stub_ptr_glue))
STUBS.internal(DESTROY_ARRAY, glue(stub_destroy_array))
STUBS.internal(OP_DELETE, glue(stub_op_delete))
STUBS.internal(SYS_CTOR, CL.recorder("CFSOnlineSystem", "N"))
STUBS.internal(ARE_LOGGED, CL.recorder("AreWeLoggedIn", "L", ret="i"))
STUBS.internal(LOGIN, stub_login)
STUBS.internal(POST_LOGIN, CL.recorder("PostLoginCallbackFn_Park", "iL"))


def setup(runner):
    e = runner.emu
    # descriptors: any distinct addresses; vtables with the slots the code reads
    VT["tv_assign"] = T.persist_alloc(e, 8)
    VT["tv_list_dtor"] = T.persist_alloc(e, 8)
    VT["str"] = T.persist_alloc(e, 0x40, b"".join(struct.pack(">I", VT["tv_assign"] if i == 8 else 0) for i in range(16)))
    VT["list"] = T.persist_alloc(e, 0x20, b"".join(struct.pack(">I", VT["tv_list_dtor"] if i == 2 else 0) for i in range(8)))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def fa(self, name):
        return self.this_ + self.OFF[name]

    def g(self, name):
        return self.addr_of(name)

    def world(self, case):
        """The case's words, the answer block and the preset system; the same code on both sides."""
        for name, value in case["ints"].items():
            self.words(self.fa(name), [value])
        self.recv = 0
        if case["recv"] is not None:
            ref, n = case["recv"]
            self.recv = self.alloc(self.REC["size"])
            infos = self.new_infos(n) if n else 0
            self.words(self.recv + self.REC["count"], [n])
            self.setptr(self.recv + self.REC["infos"], infos)
            self.words(self.recv + self.REC["ref"], [ref])
            self.setptr(self.fa("mReceived"), self.recv)
        self.sys_ = self.alloc(0x10) if case["sys"] else 0
        self.setptr(self.g("gOnlineSystem"), self.sys_)
        data = case["text"].encode(self.enc) + b"\0\0"
        self.text_ = self.alloc(len(data))
        self.write(self.text_, data)

    def labels(self):
        d = {1: self.this_, 2: self.list_, 5: self.this_ + self.OFF["mPlayerName"]}
        if self.recv:
            d[3] = self.recv
        if self.sys_:
            d[4] = self.sys_
        return d


class Ppc(Side):
    fmt = ">I"
    enc = "utf-16-be"
    OFF = MAC_OFF
    REC = MAC_REC

    def __init__(self, e, case):
        self.e = e
        INFOS.clear()
        self.this_ = e.alloc(0x48)
        self.list_ = e.alloc(0x10)
        self.words(self.list_, [VT["list"]])
        self.words(self.this_ + 0x2C, [self.list_])
        self.words(self.this_ + 0x34, [VT["str"]])
        self.world(case)
        self.L = e.cl = CL.PpcLog({a: n for n, a in self.labels().items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def addr_of(self, name):
        return ADDR[name]

    def setptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_word(a)

    def new_infos(self, n):
        a = self.e.alloc(0x10)
        INFOS[a] = n
        return a

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

    def call_dtor(self, this, flag):
        r3, _, _ = T.call_ppc(self.e, FN_DTOR, [this, flag & MASK])
        return r3 & MASK

    def call_get(self, text, cb, ctx):
        T.call_ppc(self.e, FN_GET, [self.this_, text, cb & MASK, ctx & MASK])


class Host(Side):
    fmt = "<I"
    enc = "utf-16-le"
    OFF = HOST_OFF
    REC = HOST_REC

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.this_ = lib.tpw_pl_make()
        self.list_ = self.rd_ptr(self.this_ + HOST_OFF["mList"])
        self.world(case)
        CL.host_begin(lib, self.labels(), case["queues"])

    def addr_of(self, name):
        return self.lib.tpw_pl_addr(name.encode())

    def setptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def new_infos(self, n):
        return self.lib.tpw_pl_newinfos(n)

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_pl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_dtor(self, this, flag):
        return self.lib.tpw_pl_dtor(this, T.s16(flag)) & 0xFFFFFFFFFFFFFFFF

    def call_get(self, text, cb, ctx):
        self.lib.tpw_pl_getlist(self.this_, text, cb & MASK, ctx & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_pl_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "make").restype, getattr(lib, p + "make").argtypes = C.c_uint64, []
    getattr(lib, p + "newinfos").restype, getattr(lib, p + "newinfos").argtypes = C.c_uint64, [C.c_int32]
    getattr(lib, p + "dtor").restype, getattr(lib, p + "dtor").argtypes = C.c_uint64, [C.c_uint64, C.c_int16]
    getattr(lib, p + "getlist").restype, getattr(lib, p + "getlist").argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def execute(s, case):
    if case["op"] == "dtor":
        this = 0 if case["null"] else s.this_
        return int(s.call_dtor(this, case["flag"]) == this)
    s.call_get(s.text_, case["cb"], case["ctx"])
    return 0


def observe(s, case, answer):
    d = {"answer": answer, "log": s.log()}
    # the host frees the object (a positive flag) and the answer block (reference count 1) for real, so those are not read back
    object_freed = case["op"] == "dtor" and not case["null"] and T.s16(case["flag"]) > 0
    answer_freed = case["recv"] is not None and case["recv"][0] == 1
    if not object_freed:
        for name in INTS:
            d[name] = s.rd_word(s.fa(name))
        d["received_set"] = int(s.rd_ptr(s.fa("mReceived")) != 0)
        d["list_same"] = int(s.rd_ptr(s.fa("mList")) == s.list_)
    if s.recv and not answer_freed:
        d["rec_count"] = s.rd_word(s.recv + s.REC["count"])
        d["rec_infos_set"] = int(s.rd_ptr(s.recv + s.REC["infos"]) != 0)
        d["rec_ref"] = s.rd_word(s.recv + s.REC["ref"])
    w = s.rd_ptr(s.g("gOnlineSystem"))
    d["sys"] = 0 if w == 0 else (1 if w == s.sys_ else 2)
    return d


def run(s, case):
    try:
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

RECVS = [None, (1, 2), (1, 0), (2, 1), (0, 1), (1, 1), (1, 3), (-1 & MASK, 1)]
LETTERS = "abcXYZ019 _"


def rword(r):
    return r.choice([0, 1, MASK, r.getrandbits(32)])


def base(r, op):
    return {"op": op, "null": False, "flag": 0, "cb": 0, "ctx": 0,
            "ints": {n: rword(r) for n in INTS},
            "recv": r.choice(RECVS), "sys": r.choice([False, True]),
            "text": "".join(r.choice(LETTERS) for _ in range(r.choice([0, 1, 3, 8]))),
            "queues": {"AreWeLoggedIn": [r.choice([0, 1, 2])]}}


def dtor_case(r, flag=None, recv="r", null=None):
    c = base(r, "dtor")
    c["flag"] = r.choice([0, 1, 2, -1 & MASK, 0x7FFF, 0x8000, 0x10001, 0xFFFF, 0x1FFFF, r.getrandbits(32)]) if flag is None else flag
    if recv != "r":
        c["recv"] = recv
    c["null"] = (r.random() < 0.1) if null is None else null
    return c


def dtor_edges():
    r = random.Random(5063)
    for flag in (0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10001, 0x10000, 0xFFFFFFFF):
        for recv in RECVS:
            yield dtor_case(r, flag, recv, False)
    yield dtor_case(r, 1, None, True)
    yield dtor_case(r, 0, (1, 2), True)


def dtor_random(r):
    return dtor_case(r)


def get_case(r, recv="r", sys=None, logged=None):
    c = base(r, "get")
    c["cb"] = rword(r)
    c["ctx"] = rword(r)
    if recv != "r":
        c["recv"] = recv
    if sys is not None:
        c["sys"] = sys
    if logged is not None:
        c["queues"] = {"AreWeLoggedIn": [logged]}
    return c


def get_edges():
    r = random.Random(5064)
    for recv in RECVS:
        for sys in (False, True):
            for logged in (0, 1, 2, 0x80000000):
                yield get_case(r, recv, sys, logged)


def get_random(r):
    return get_case(r)


TESTS = [
    FnTest("CFSOnlineSystemParkList_InWorldByPlayerName::~CFSOnlineSystemParkList_InWorldByPlayerName", ppc, host, edge=dtor_edges,
           rnd=dtor_random),
    FnTest("CFSOnlineSystemParkList_InWorldByPlayerName::GetList", ppc, host, edge=get_edges, rnd=get_random),
]
