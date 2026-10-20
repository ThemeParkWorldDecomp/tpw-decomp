"""Unit v5L17, the online list destructors of the online system's teardown, against libtpw_unit_FS_OnlineList_dtors.dylib
(tests/unit_FS_OnlineList_dtors.cpp), main app ("SimTheme Park"), src/FS_OnlineList_dtors.cpp:
  CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>::~CFSOnlineSystemList 0x101701c8,
  CFSOnlineSystemParkList_InWorldByParkID::~ 0x10170834, CFSOnlineSystemParkList_InCityTop10::~ 0x10170ca8
and, for the chain, CFSOnlineSystemCityList::~ 0x10170e20 (unit v51065, whose module tests it with the base faked out).

The host side runs the host's whole destructor chain as the game does (delete, or the destructor for a flag of 0 or below): the park
lists' destructors are empty, their work is CFSOnlineSystemParkList::~ (0x1016fd48) followed by the park instance of the base
(0x1017004c, a no-op on the NULL list it leaves); the city list's is v51065's body followed by the city base (a no-op the same way);
the city base alone does the whole work. Each Mac function has all of it inline. A chain that destroys the list twice (the crash on
leaving the park, v5L17) kills the host side.
A case is a world and a call. The world: a list object (words 0x24, 0x28), a list of n records (some NULL entries), a stored answer (none,
count 1 with infos, count 1 without, count 2). The Mac object is built in emulator memory, the host's by the library (the host layout
differs); the callees are fakes recording events: RemoveHead (city 0x10177e2c and park 0x10177b98, popping the case's records), the
virtual destructor call of a record (its index), the array destroy of the infos. Compared: the answer (this or NULL), words 0x24 and 0x28
after (not read after a deleting call), the kept answer's count, the event log. The list's own delete and the operator deletes are not
logged on either side. No fused operations. Not tested: a NULL list (read unconditionally on the Mac; the host's two bases allow it).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FS_OnlineList_dtors"
MASK = 0xFFFFFFFF

CITYBASE, CITYLIST, BYPARKID, TOP10 = 0x101701C8, 0x10170E20, 0x10170834, 0x10170CA8
SIZES = {CITYBASE: 288, CITYLIST: 300, BYPARKID: 312, TOP10: 312}
OWN = set(SIZES)
KIND = {CITYBASE: 0, CITYLIST: 1, BYPARKID: 2, TOP10: 3}

REMOVEHEAD_CITY, REMOVEHEAD_PARK, PTR_GLUE, DESTROY_ARRAY = 0x10177E2C, 0x10177B98, 0x1023BAA8, 0x10238AD0

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_removehead(e):
    w = e.world
    lst = e.r(3)
    count = e.s32(lst + 0xC)
    if count == 0:
        e.set_r(3, 0)
        return
    rec = w["records"].pop(0)
    e.write(lst + 0xC, (count - 1).to_bytes(4, "big", signed=True))
    e.set_r(3, rec)


def stub_ptr_glue(e):
    w = e.world
    if e.r(3) in w["index"]:
        e.cl.rec("recdtor", (w["index"][e.r(3)],))


def stub_destroy(e):
    e.cl.rec("infodtor", (0,))


STUBS.internal(REMOVEHEAD_CITY, stub_removehead)
STUBS.internal(REMOVEHEAD_PARK, stub_removehead)
STUBS.internal(PTR_GLUE, stub_ptr_glue)
STUBS.internal(DESTROY_ARRAY, stub_destroy)
STUBS.internal(0x10238218, lambda e: None)      # operator delete: not logged (the host's is the real one)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ld_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ld_dtor.restype = C.c_uint32
    lib.tpw_ld_dtor.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_uint32, C.c_int32, C.c_int32, C.c_int32, C.POINTER(C.c_int32)]
    out = (C.c_int64 * 4)()
    assert list(out)[:lib.tpw_ld_layout(out)] == []
    lib.bound = True


def make_ppc(addr):
    def ppc(e, case):
        L = e.cl = CL.PpcLog({}, case["queues"])
        saved = STUBS.activate(e)
        try:
            n = case["n"]
            this = e.alloc(0x38)
            vt = e.alloc(16)
            lst = e.alloc(0x10)
            e.write(lst, vt.to_bytes(4, "big"))
            e.write(lst + 0xC, n.to_bytes(4, "big"))
            recs = []
            for i in range(n):
                if (case["nullmask"] >> i) & 1:
                    recs.append(0)
                else:
                    p = e.alloc(8)
                    e.write(p, vt.to_bytes(4, "big"))
                    recs.append(p)
            e.world = {"records": list(recs), "index": {p: i for i, p in enumerate(recs) if p}}
            e.write(this + 0x24, (case["f24"] & MASK).to_bytes(4, "big") + (case["f28"] & MASK).to_bytes(4, "big") + lst.to_bytes(4, "big"))
            rm = case["recv"]
            recv = 0
            if rm:
                recv = e.alloc(16)
                info = 0 if rm == 2 else e.alloc(0x70)
                e.write(recv, (1).to_bytes(4, "big") + info.to_bytes(4, "big") + (0).to_bytes(4, "big") + (2 if rm == 3 else 1).to_bytes(4, "big"))
            e.write(this + 0x30, recv.to_bytes(4, "big"))
            r3, _, _ = T.call_ppc(e, addr, [0 if case["isnull"] else this, case["flag"] & 0xFFFF])
            out = {"answer": 1 if r3 & MASK else 0, "log": L.log}
            if case["isnull"] or T.s16(case["flag"]) > 0:
                out.update(f24=0, f28=0, ref=0)
            else:
                out.update(f24=e.u32(this + 0x24), f28=e.u32(this + 0x28), ref=e.u32(recv + 0xC) if rm == 3 else 0)
            return out
        finally:
            CL.Stubs.deactivate(e, saved)
    return ppc


def make_host(addr):
    def host(lib, case):
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        out = (C.c_int32 * 3)()
        ans = lib.tpw_ld_dtor(KIND[addr], case["flag"], case["isnull"], case["n"], case["nullmask"], case["recv"], T.s32(case["f24"]),
                              T.s32(case["f28"]), out)
        return {"answer": ans, "log": CL.host_end(lib), "f24": out[0] & MASK, "f28": out[1] & MASK, "ref": out[2]}
    return host


def dtor_case(r, **kw):
    c = {"flag": r.choice([0, 1, -1, 2, 5, -32768]), "isnull": 0, "n": r.randrange(0, 6), "nullmask": r.randrange(64), "recv": r.randrange(4),
         "f24": r.getrandbits(32), "f28": r.getrandbits(32), "queues": {}}
    c.update(kw)
    return c


def dtor_edges():
    r = random.Random(17)
    yield dtor_case(r, isnull=1, flag=1)
    yield dtor_case(r, isnull=1, flag=0)
    for flag in (0, 1, -1):
        for recv in range(4):
            for n, mask in ((0, 0), (1, 0), (1, 1), (3, 2), (4, 0), (4, 15), (5, 0)):
                yield dtor_case(r, flag=flag, recv=recv, n=n, nullmask=mask)


def dtor_random(r):
    return dtor_case(r)


TESTS = [
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>::~CFSOnlineSystemList", make_ppc(CITYBASE), make_host(CITYBASE),
           edge=dtor_edges, rnd=dtor_random),
    FnTest("CFSOnlineSystemCityList::~CFSOnlineSystemCityList", make_ppc(CITYLIST), make_host(CITYLIST), edge=dtor_edges, rnd=dtor_random),
    FnTest("CFSOnlineSystemParkList_InWorldByParkID::~CFSOnlineSystemParkList_InWorldByParkID", make_ppc(BYPARKID), make_host(BYPARKID),
           edge=dtor_edges, rnd=dtor_random),
    FnTest("CFSOnlineSystemParkList_InCityTop10::~CFSOnlineSystemParkList_InCityTop10", make_ppc(TOP10), make_host(TOP10), edge=dtor_edges,
           rnd=dtor_random),
]
