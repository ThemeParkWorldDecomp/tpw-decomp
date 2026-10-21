"""Unit v5928, the theme list and two park lists' list requests, against libtpw_unit_CFSOnlineSystemThemeList_GetList_2.dylib
(tests/unit_CFSOnlineSystemThemeList_GetList_2.cpp), main app ("SimTheme Park"), src/game/online/FS_OnlineList.cpp:
  CFSOnlineSystemThemeList::GetList 0x1017a1c0, RequestListFromServer 0x1017a354, StoreReceivedList 0x1017a3fc,
  ReturnOnlineThemeID 0x1017a4c0, ThemeListPostLoginCallbackFn 0x10179e8c,
  CFSOnlineSystemParkList_InWorldByParkID::RequestListFromServer 0x10179964,
  CFSOnlineSystemParkList_InCityTop10::RequestListFromServer 0x10179428

The world is built twice by ONE piece of Python code: in emulator memory (big-endian, 32-bit pointers, Mac offsets) and in static blocks of the
host library (little-endian, 64-bit pointers, host offsets from tpw_tl_layout). The callees outside the unit (operator new and delete, the
array destroyer, the online system's constructor, AreWeLoggedIn and Login, the theme list's CompleteGetList and LoggedInGetList, the theme
constructor, City's three list calls, UDebug, _stricmp) are fakes on both sides that log their arguments.

The world of a case: gOnlineSystem (null or the system block), the system's theme list pointer, the list words, the stored answer (count,
refcount, infos), the themes of the list (nodes, ids, names), the answer queues. What is compared: the answer, gOnlineSystem as a label,
the list words, the answer block and the event log.

No fused operations. Not tested: a NULL list or city (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystemThemeList_GetList_2"
MASK = 0xFFFFFFFF

F_GETLIST, F_REQUEST, F_STORE, F_RETID, F_CB, F_PBID, F_TOP10 = 0x1017A1C0, 0x1017A354, 0x1017A3FC, 0x1017A4C0, 0x10179E8C, 0x10179964, 0x10179428
TVEC_CB = 0x102675E0

ADDR = dict(gOnlineSystem=0x10380AB0)
BLOCKS = dict(sys=0x1000, tl=0x200, city=0x100, rcv=0x40, infos=0x200, park=0x100, pool=0x800)
LAB = dict(sys=1, tl=2, city=3, infos=5, rcv=6)

MAC = {"id": 4, "f08": 8, "f0c": 0xC, "f10": 0x10, "f14": 0x14, "f18": 0x18, "f1c": 0x1C, "f20": 0x20, "mlist": 0x2C, "recv": 0x30,
       "pk1": 0x34, "pk2": 0x34, "themelist": 0xF8, "rcount": 0, "rinfos": 4, "rf08": 8, "rref": 0xC, "infosize": 16, "head": 4,
       "ndata": 0, "nnext": 4, "tid": 8, "ttext": 0x14}
HNAMES = ["id", "f08", "f0c", "f10", "f14", "f18", "f1c", "f20", "mlist", "recv", "pk1", "pk2", "themelist", "rcount", "rinfos",
          "rf08", "rref", "infosize", "head", "ndata", "nnext", "tid", "ttext"]
WORDS = ["id", "f08", "f0c", "f10", "f14", "f18", "f1c", "f20"]

NAMES = ["Fantasy", "fantasy", "FANTASY", "Space", "space", "Halloween", "", "Lost World", "lost world", "Sp"]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update({F_GETLIST, F_REQUEST, F_STORE, F_RETID, F_CB, F_PBID, F_TOP10})


def stub_login(e):
    L = e.cl
    L.rec("Login", (L.lab(e.r(3)), 1 if e.r(4) == TVEC_CB else 0, e.r(5) & MASK))


def stub_theme_ct(e):
    L = e.cl
    L.rec("CFSOnlineTheme_ct", (L.lab(e.r(5)), e.u32(e.r(4))))


def stricmp_ret(e, log):
    a, b = e.cstr(e.r(3)), e.cstr(e.r(4))
    a, b = a.lower(), b.lower()
    return 0 if a == b else (MASK if a < b else 1)


STUBS.internal(0x10000E40, CL.recorder("UDebug", "s"))
def stub_new(e):
    """operator new: the host's is the program's own (no event there), so the size is checked here: the online system or a theme."""
    size = e.r(3)
    assert size in (0x404, 0x48), size
    e.set_r(3, e.alloc(size))


def stub_delete(e):
    """operator delete: only the stored answer is ever freed (no event on the host, see the module docstring)."""
    assert e.r(3) == ADDR["rcv"], hex(e.r(3))
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def stub_destroy_array(e):
    """__destroy_new_array(infos, dtor): the host's delete[] shows as the destructor of each of the 32 elements, last first."""
    L = e.cl
    L.rec("destroy_array", (L.lab(e.r(3)),))
    for i in range(31, -1, -1):
        L.rec("~ccThemeInfo", (i,))


STUBS.imp("__nw__FUl", stub_new)
STUBS.imp("__dl__FPv", stub_delete)
STUBS.imp("__destroy_new_array", stub_destroy_array)
STUBS.internal(0x1016F7D0, CL.recorder("CFSOnlineSystem_ct", ""))
STUBS.internal(0x1017178C, CL.recorder("AreWeLoggedIn", "L", ret="i"))
STUBS.internal(0x101710AC, stub_login)
STUBS.internal(0x1017488C, CL.recorder("CompleteGetList", "Li"))
STUBS.internal(0x10179F50, CL.recorder("LoggedInGetList", "L"))
STUBS.internal(0x1017C57C, stub_theme_ct)
STUBS.imp("_stricmp__10NS_MacDozeFPCcPCc", CL.recorder("_stricmp", "ss", ret=stricmp_ret))
STUBS.imp("GetThemeList__4CityFPvli", CL.recorder("GetThemeList", "Luui", ret="i"))
STUBS.imp("GetParkByID__4CityFPviUli", CL.recorder("GetParkByID", "Luiui", ret="i"))
STUBS.imp("GetParkInCityListTop10__4CityFPviUli", CL.recorder("GetParkInCityListTop10", "Luiui", ret="i"))


def setup(runner):
    for name, size in BLOCKS.items():
        ADDR[name] = T.persist_alloc(runner.emu, size)


class Side:
    def ptr_w(self, a, p):
        self.write(a, struct.pack(self.pfmt, p))

    def ptr_r(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psize))[0]

    def w32(self, a, v):
        self.write(a, struct.pack(self.fmt, v & MASK))

    def r32(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def lab(self, p):
        if p == 0:
            return 0
        for k, v in LAB.items():
            if p == self.g(k):
                return v
        return -1


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psize = 4

    def __init__(self, e, case):
        self.e = e
        fixed = {ADDR[k]: v for k, v in LAB.items()}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def off(self, k):
        return MAC[k]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def cookie(self, n):
        pass

    def log(self):
        return self.L.log

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_tl_fresh()
        CL.host_begin(lib, {v: self.g(k) for k, v in LAB.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_tl_addr(name.encode())

    def off(self, k):
        return self.lib.offs[k]

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def cookie(self, n):
        pass

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        l = self.lib
        name = {F_GETLIST: "getlist", F_REQUEST: "request", F_STORE: "store", F_RETID: "retid", F_CB: "cb", F_PBID: "pbid", F_TOP10: "top10"}[entry]
        r = getattr(l, "tpw_tl_" + name)(*args)
        return 0 if r is None else r & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_tl_addr.restype, lib.tpw_tl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_tl_layout.argtypes = [C.POINTER(C.c_int64)]
    u64, u32, i32 = C.c_uint64, C.c_uint32, C.c_int32
    for n, res, args in (("getlist", None, [u64]), ("request", i32, [u64, u64, u32]), ("store", None, [u64, u64]),
                         ("retid", u32, [u64, u64]), ("cb", None, [i32]), ("pbid", i32, [u64, u64, u32]), ("top10", i32, [u64, u64, u32])):
        f = getattr(lib, "tpw_tl_" + n)
        f.restype, f.argtypes = res, args
    out = (C.c_int64 * 64)()
    n = lib.tpw_tl_layout(out)
    assert n == len(HNAMES), n
    lib.offs = dict(zip(HNAMES, list(out)[:n]))
    assert lib.offs["infosize"] == 16
    lib.bound = True


# ------------------------------------------------------------------------------------------------------------- the world

def build(s, case):
    for name, size in BLOCKS.items():
        s.write(s.g(name), bytes(size))
    sys_, tl, city, rcv, infos, pool = (s.g(k) for k in ("sys", "tl", "city", "rcv", "infos", "pool"))
    s.cookie(8)
    s.ptr_w(s.g("gOnlineSystem"), sys_ if case["gos"] else 0)
    s.ptr_w(sys_ + s.off("themelist"), tl)
    for k, v in zip(WORDS, case["words"]):
        s.w32(tl + s.off(k), v)
    # the stored answer
    s.w32(rcv + s.off("rcount"), case["rcount"])
    s.w32(rcv + s.off("rf08"), case["rf08"])
    s.w32(rcv + s.off("rref"), case["rref"])
    s.ptr_w(rcv + s.off("rinfos"), infos if case["rinfos"] else 0)
    if case["attach"]:
        s.ptr_w(tl + s.off("recv"), rcv)
    for i in range(8):
        s.w32(infos + 16 * i, 0x1000 + i)
    s.w32(s.g("park") + s.off("pk1"), case["pk"])
    # the themes: list object at pool+0, nodes at +0x40, themes at +0x140, strings at +0x600, the asked name at +0x700
    s.ptr_w(tl + s.off("mlist"), pool)
    n = len(case["themes"])
    s.ptr_w(pool + s.off("head"), pool + 0x40 if n else 0)
    for i, (tid, name) in enumerate(case["themes"]):
        node, theme, text = pool + 0x40 + i * 0x20, pool + 0x140 + i * 0x80, pool + 0x600 + i * 0x20
        s.ptr_w(node + s.off("ndata"), theme)
        s.ptr_w(node + s.off("nnext"), node + 0x20 if i + 1 < n else 0)
        s.w32(theme + s.off("tid"), tid)
        s.ptr_w(theme + s.off("ttext"), text)
        s.write(text, name.encode() + b"\0")
    s.write(pool + 0x700, case["ask"].encode() + b"\0")


def execute(s, case):
    tl, city, rcv, pool = s.g("tl"), s.g("city"), s.g("rcv"), s.g("pool")
    op = case["op"]
    if op == "getlist":
        s.call(F_GETLIST, [tl])
        return 0
    if op == "request":
        return s.call(F_REQUEST, [tl, city, case["arg"]])
    if op == "store":
        s.call(F_STORE, [tl, rcv])
        return 0
    if op == "retid":
        return s.call(F_RETID, [tl, pool + 0x700]) & 0xFF
    if op == "cb":
        s.call(F_CB, [case["err"]])
        return 0
    if op == "pbid":
        return s.call(F_PBID, [s.g("park"), city, case["arg"]])
    return s.call(F_TOP10, [s.g("park"), city, case["arg"]])


def observe(s, case, answer):
    tl, rcv = s.g("tl"), s.g("rcv")
    return {"answer": answer, "log": s.log(),
            "gos": s.lab(s.ptr_r(s.g("gOnlineSystem"))),
            "words": [s.r32(tl + s.off(k)) for k in WORDS],
            "recv": s.lab(s.ptr_r(tl + s.off("recv"))),
            # the answer block is freed (and on the host reused) when the last reference goes
            "rcv": None if case["op"] == "getlist" and case["attach"] and case["rref"] == 1 else
            [s.r32(rcv + s.off("rcount")), s.r32(rcv + s.off("rf08")), s.r32(rcv + s.off("rref")),
             s.lab(s.ptr_r(rcv + s.off("rinfos")))]}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ------------------------------------------------------------------------------------------------------------- cases

def rw(r):
    return r.choice([0, 1, MASK, 0x80000000, r.getrandbits(32), r.randrange(0, 50)])


def base(r, op, **kw):
    c = {"op": op, "gos": r.choice([0, 1]), "words": [rw(r) for _ in range(8)], "rcount": r.randrange(0, 9), "rf08": rw(r),
         "rref": r.choice([0, 1, 1, 2, 3]), "rinfos": r.choice([0, 1]), "attach": r.choice([0, 1]), "pk": s32(rw(r)),
         "themes": [], "ask": r.choice(NAMES), "arg": rw(r), "err": r.choice([0, 1, -1, 7]),
         "queues": {"AreWeLoggedIn": [r.choice([0, 1])], "GetThemeList": [s32(rw(r))], "GetParkByID": [s32(rw(r))],
                    "GetParkInCityListTop10": [s32(rw(r))]}}
    c.update(kw)
    return c


def themes(r, n):
    return [(s32(r.choice([0, 1, 2, 0xFF, 0x100, 0x1FF, -1, r.randrange(0, 300)])), r.choice(NAMES)) for _ in range(n)]


def getlist_edges():
    r = random.Random(11)
    for gos in (0, 1):
        for logged in (0, 1):
            for attach, rref, rinfos in ((0, 1, 1), (1, 1, 1), (1, 1, 0), (1, 2, 1), (1, 0, 1), (1, 3, 0)):
                c = base(r, "getlist", gos=gos, attach=attach, rref=rref, rinfos=rinfos)
                c["queues"]["AreWeLoggedIn"] = [logged]
                yield c


def getlist_random(r):
    return base(r, "getlist")


def request_edges():
    r = random.Random(12)
    for arg in (0, 1, MASK, 0x80000000):
        for ans in (0, 5, -1):
            c = base(r, "request", arg=arg)
            c["queues"]["GetThemeList"] = [ans]
            yield c


def request_random(r):
    return base(r, "request")


def store_edges():
    r = random.Random(13)
    for n in (0, 1, 2, 3, 8):
        yield base(r, "store", rcount=n, rinfos=1)


def store_random(r):
    return base(r, "store", rinfos=1, rcount=r.randrange(0, 9))


def retid_edges():
    r = random.Random(14)
    yield base(r, "retid", themes=[], ask="Fantasy")
    yield base(r, "retid", themes=[(5, "Fantasy")], ask="fantasy")
    yield base(r, "retid", themes=[(5, "Fantasy")], ask="Space")
    yield base(r, "retid", themes=[(5, "Fantasy"), (9, "FANTASY"), (7, "Space")], ask="fantasy")
    yield base(r, "retid", themes=[(0x1FF, "Space")], ask="space")
    yield base(r, "retid", themes=[(-1, "")], ask="")
    yield base(r, "retid", themes=[(3, "Sp"), (4, "Space")], ask="Sp")


def retid_random(r):
    return base(r, "retid", themes=themes(r, r.randrange(0, 7)))


def cb_edges():
    r = random.Random(15)
    for gos in (0, 1):
        for err in (0, 1, -1, 7):
            yield base(r, "cb", gos=gos, err=err)


def cb_random(r):
    return base(r, "cb")


def pk_edges(op):
    def gen():
        r = random.Random(16)
        for pk in (-1, 0, 1, 0x7FFFFFFF, -0x80000000):
            for arg in (0, MASK):
                yield base(r, op, pk=pk, arg=arg)
    return gen


def pk_random(op):
    return lambda r: base(r, op)


TESTS = [
    FnTest("CFSOnlineSystemThemeList::GetList", ppc, host, edge=getlist_edges, rnd=getlist_random),
    FnTest("CFSOnlineSystemThemeList::RequestListFromServer", ppc, host, edge=request_edges, rnd=request_random),
    FnTest("CFSOnlineSystemThemeList::StoreReceivedList", ppc, host, edge=store_edges, rnd=store_random),
    FnTest("CFSOnlineSystemThemeList::ReturnOnlineThemeID", ppc, host, edge=retid_edges, rnd=retid_random),
    FnTest("ThemeListPostLoginCallbackFn", ppc, host, edge=cb_edges, rnd=cb_random),
    FnTest("CFSOnlineSystemParkList_InWorldByParkID::RequestListFromServer", ppc, host, edge=pk_edges("pbid"), rnd=pk_random("pbid")),
    FnTest("CFSOnlineSystemParkList_InCityTop10::RequestListFromServer", ppc, host, edge=pk_edges("top10"), rnd=pk_random("top10")),
]
