"""Unit v5307, the city list's three overrides, against libtpw_unit_CFSOnlineSystemCityList_StoreReceivedList.dylib
(tests/unit_CFSOnlineSystemCityList_StoreReceivedList.cpp), main app ("SimTheme Park"), src/FS_Online_..._StoreReceivedList.cpp:
  ChildLocked 0x101788d8, RequestListFromServer 0x101789d8, StoreReceivedList 0x10178a78

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit are fakes on both
sides that record their arguments: operator new (size), the CFSOnlineSystem constructor, CFSOnlineSystem::GetChildLock (answers from the
case's queue), the CFSOnlineCity constructor (this, the list, and the info's byte offset from the info array), City::GetCityList
(answers from the queue) and SamsUtilities::UDebug (the text).

The world of a case: gOnlineSystem (null, or an existing system whose word 0x108 is set), the queue of GetChildLock answers, a received
list (count, infos array, any count including negative), the City and the request id. What is compared: the answer, gOnlineSystem as a
label, the system's word 0x108, the event log.

No fused operations. Not tested: a NULL list or NULL city (read unconditionally by the original).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystemCityList_StoreReceivedList"
MASK = 0xFFFFFFFF

FN_CHILD, FN_REQ, FN_STORE = 0x101788D8, 0x101789D8, 0x10178A78
SIZES = {FN_CHILD: 196, FN_REQ: 84, FN_STORE: 116}
OWN = set(SIZES)

CALLEE_NEW = 0x102384B8
CALLEE_SYSCTOR = 0x1016F7D0
CALLEE_CITYCTOR = 0x1017BCB4
CALLEE_CHILDLOCK = 0x10171980
CALLEE_GETCITYLIST = 0x102392F8
CALLEE_UDEBUG = 0x10000E40

ADDR = dict(gOnlineSystem=0x10380AB0)
HOST_LAYOUT_FIXED = [0, 8, 24]     # ccCityList: mCount, mInfos, sizeof (the host's pointer is 8 bytes)

SYS_LABEL, LIST_LABEL, CITY_LABEL = 1, 2, 3
state = {}          # persistent PowerPC objects: sys, listobj, city
INFO_BASE = [0]
NEWS = []           # the sizes operator new was asked for on the PowerPC side (the host's operator new is the shim's)


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_new(e):
    size = e.r(3) & MASK
    NEWS.append(size)
    e.set_r(3, e.alloc(size if size else 4, bytes(size if size else 4)))     # zeroed like the host's arena


STUBS.internal(CALLEE_NEW, stub_new)
_sysctor = CL.recorder("CFSOnlineSystem", "N")
STUBS.internal(CALLEE_SYSCTOR, _sysctor)
STUBS.internal(CALLEE_CHILDLOCK, CL.recorder("GetChildLock", "L", ret="i"))
STUBS.internal(CALLEE_GETCITYLIST, CL.recorder("GetCityList", "Luui", ret="i"))
STUBS.internal(CALLEE_UDEBUG, CL.recorder("UDebug", "s"))
_cityctor = CL.recorder("CFSOnlineCity", "NxL")


def stub_cityctor(e):
    _cityctor(e)
    e.cl.rec("CFSOnlineCityInfo", ((e.r(4) - INFO_BASE[0]) & MASK,))


STUBS.internal(CALLEE_CITYCTOR, stub_cityctor)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    state["sys"] = T.persist_alloc(e, 0x404)
    state["list"] = T.persist_alloc(e, 0x40)
    state["city"] = T.persist_alloc(e, 0x10)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    off108 = 0x108

    def __init__(self, e, case):
        self.e = e
        self.sys, self.listobj, self.city = state["sys"], state["list"], state["city"]
        del NEWS[:]
        self.L = e.cl = CL.PpcLog({self.sys: SYS_LABEL, self.listobj: LIST_LABEL, self.city: CITY_LABEL}, case["queues"])
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

    def setbase(self, a):
        INFO_BASE[0] = a

    def write_list(self, a, count, infos):
        self.words(a, [count, infos, 0, 1])

    def set_gptr(self, v):
        self.words(self.g("gOnlineSystem"), [v])

    def gsys_label(self):
        return self.L.lab(self.rd_word(self.g("gOnlineSystem")))

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off108 = lib.layout[0]
        self.sys, self.listobj, self.city = self.alloc(0x404), self.alloc(0x40), self.alloc(0x10)
        CL.host_begin(lib, {SYS_LABEL: self.sys, LIST_LABEL: self.listobj, CITY_LABEL: self.city}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cityl_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cityl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setbase(self, a):
        self.lib.tpw_cityl_setbase(a)

    def write_list(self, a, count, infos):
        self.write(a, struct.pack("<iIQii", count, 0, infos, 0, 1))

    def set_gptr(self, v):
        self.write(self.g("gOnlineSystem"), struct.pack("<Q", v))

    def rd_gsys(self):
        return struct.unpack("<Q", self.read(self.g("gOnlineSystem"), 8))[0]

    def gsys_label(self):
        return self.lib.tpw_cityl_gsys_label()

    def call(self, fn, args):
        lib = self.lib
        if fn == FN_CHILD:
            return lib.tpw_cityl_ChildLocked(args[0]) & MASK
        if fn == FN_REQ:
            return lib.tpw_cityl_RequestListFromServer(args[0], args[1], args[2]) & MASK
        lib.tpw_cityl_StoreReceivedList(args[0], args[1])
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cityl_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "setbase").argtypes = [C.c_uint64]
    getattr(lib, p + "gsys_label").restype = C.c_int64
    getattr(lib, p + "ChildLocked").restype, getattr(lib, p + "ChildLocked").argtypes = C.c_int32, [C.c_uint64]
    f = getattr(lib, p + "RequestListFromServer")
    f.restype, f.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_uint32]
    getattr(lib, p + "StoreReceivedList").argtypes = [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    got = list(out)[:n]
    assert got[1:] == HOST_LAYOUT_FIXED, got
    lib.layout = got
    lib.bound = True


def build(s, case):
    if case["op"] == "child" and case["have_sys"]:
        s.set_gptr(s.sys)
        s.words(s.sys + s.off108, [case["f108"]])
    else:
        s.set_gptr(0)


def execute(s, case):
    if case["op"] == "child":
        return s.call(FN_CHILD, [s.listobj])
    if case["op"] == "req":
        return s.call(FN_REQ, [s.listobj, s.city, case["id"] & MASK])
    n = case["count"]
    infos = s.alloc(0x30 * max(n, 1) + 16)
    s.setbase(infos)
    lst = s.alloc(0x20)
    s.write_list(lst, n, infos)
    return s.call(FN_STORE, [s.listobj, lst])


def observe(s, case, answer):
    out = {"answer": answer if case["op"] != "store" else 0, "log": s.log()}     # (StoreReceivedList answers nothing)
    if isinstance(s, Ppc):
        want = [0x68] * max(case["count"], 0) if case["op"] == "store" else (
            [0x404] if case["op"] == "child" and not case["have_sys"] else [])
        assert NEWS == want, (NEWS, want)
    if case["op"] == "child":
        out["gsys"] = s.gsys_label()
        out["f108"] = s.rd_word(s.sys + s.off108) if case["have_sys"] else 0
    return out


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


def base(r, op):
    return {"op": op, "queues": {"GetChildLock": [r.choice([0, 0x10, 0x20, 0x30, 0xFFFFFFEF, r.getrandbits(32)])],
                                 "GetCityList": [r.choice([0, 1, -1, r.randrange(-1000, 1000)]) & MASK]}}


def child_case(r, have_sys=None, f108=None, lock=None):
    c = base(r, "child")
    c["have_sys"] = r.random() < 0.8 if have_sys is None else have_sys
    c["f108"] = r.choice([0, 1, -1, 0x10, r.getrandbits(32)]) if f108 is None else f108
    if lock is not None:
        c["queues"]["GetChildLock"] = [lock]
    return c


def child_edges():
    r = random.Random(5307)
    yield child_case(r, False, 0)
    yield child_case(r, False, 5, 0)
    for f in (0, 1, 0x80000000):
        for lock in (0, 0x10, 0x0F, 0xFFFFFFEF, 0xFFFFFFFF, 0x1F, 0x20):
            yield child_case(r, True, f, lock)


def req_case(r, id_=None):
    c = base(r, "req")
    c["id"] = r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)]) if id_ is None else id_
    return c


def req_edges():
    r = random.Random(5308)
    for i in (0, 1, 0xFFFFFFFF, 0x80000000):
        yield req_case(r, i)


def store_case(r, n=None):
    c = base(r, "store")
    c["count"] = r.choice([0, 1, 2, 3, 7, -1, r.randrange(-3, 12)]) if n is None else n
    return c


def store_edges():
    r = random.Random(5309)
    for n in (0, 1, 2, 5, -1, -100):
        yield store_case(r, n)


TESTS = [
    FnTest("CFSOnlineSystemCityList::ChildLocked", ppc, host, edge=child_edges, rnd=child_case),
    FnTest("CFSOnlineSystemCityList::RequestListFromServer", ppc, host, edge=req_edges, rnd=req_case),
    FnTest("CFSOnlineSystemCityList::StoreReceivedList", ppc, host, edge=store_edges, rnd=store_case),
]
