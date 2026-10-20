"""Unit v5532, the InCity park list's destructor and RequestListFromServer, against
libtpw_unit_CFSOnlineSystemParkList_InCity_Dtor_CFSOnlineSys.dylib (tests/unit_CFSOnlineSystemParkList_InCity_Dtor_CFSOnlineSys.cpp), main app
("SimTheme Park"), src/FS_Online_..._CFSOnlineSystemParkList_InCity_Dtor_CFSOnlineSys.cpp:
  CFSOnlineSystemParkList_InCity::~CFSOnlineSystemParkList_InCity 0x10170b34, RequestListFromServer 0x1017911c

A case is a world and a call. The destructor's world: a list object (words 0x24, 0x28), a list of n parks (some NULL entries), a stored
answer (none, count 1 with infos, count 1 without, count 2). The Mac object is built in emulator memory, the host's by the library (the host
layout differs); the callees are fakes recording events: RemoveHead (popping the case's parks), the virtual destructor call of a park, the
array destroy of the infos. Compared: the answer (this or NULL), words 0x24 and 0x28 after (not read after a deleting call), the kept answer's
count, the event log. The list's own delete and the operator deletes are not logged on either side (inline destructor on the host).
RequestListFromServer: the UDebug text and the GetParkInCityList arguments, the answer.
No fused operations. Not tested: a NULL city (read unconditionally by the callee on the Mac).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystemParkList_InCity_Dtor_CFSOnlineSys"
MASK = 0xFFFFFFFF

DTOR, REQ = 0x10170B34, 0x1017911C
SIZES = {DTOR: 312, REQ: 104}
OWN = set(SIZES)

UDEBUG, REMOVEHEAD, PTR_GLUE, DESTROY_ARRAY, GETPARK = 0x10000E40, 0x10177B98, 0x1023BAA8, 0x10238AD0, 0x102392C8

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(UDEBUG, CL.recorder("UDebug", "s"))
STUBS.internal(GETPARK, CL.recorder("GetParkInCityList", "Liiui", ret="i"))

CITY = [0]


def stub_removehead(e):
    w = e.world
    lst = e.r(3)
    count = e.s32(lst + 0xC)
    if count == 0:
        e.set_r(3, 0)
        return
    park = w["parks"].pop(0)
    e.write(lst + 0xC, (count - 1).to_bytes(4, "big", signed=True))
    e.set_r(3, park)


def stub_ptr_glue(e):
    w = e.world
    if e.r(3) in w["park_set"]:
        e.cl.rec("parkdtor", (0,))


def stub_destroy(e):
    e.cl.rec("infodtor", (0,))


STUBS.internal(REMOVEHEAD, stub_removehead)
STUBS.internal(PTR_GLUE, stub_ptr_glue)
STUBS.internal(DESTROY_ARRAY, stub_destroy)
STUBS.internal(0x10238218, lambda e: None)      # operator delete: not logged (the host's is the real one)


def setup(runner):
    T.install_std_stubs(runner.emu)
    CITY[0] = T.persist_alloc(runner.emu, 16)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ic_addr.restype, lib.tpw_ic_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ic_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ic_dtor.restype = C.c_uint32
    lib.tpw_ic_dtor.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_uint32, C.c_int32, C.c_int32, C.c_int32, C.POINTER(C.c_int32)]
    lib.tpw_ic_request.restype, lib.tpw_ic_request.argtypes = C.c_int32, [C.c_int32, C.c_uint32]
    out = (C.c_int64 * 4)()
    assert list(out)[:lib.tpw_ic_layout(out)] == []
    lib.bound = True


def ppc_dtor(e, case):
    L = e.cl = CL.PpcLog({CITY[0]: 1}, case["queues"])
    saved = CL.Stubs.activate(STUBS, e) if False else STUBS.activate(e)
    try:
        n = case["n"]
        this = e.alloc(0x38)
        vt = e.alloc(16)
        lst = e.alloc(0x10)
        e.write(lst, vt.to_bytes(4, "big"))
        e.write(lst + 0xC, n.to_bytes(4, "big"))
        parks = []
        for i in range(n):
            if (case["nullmask"] >> i) & 1:
                parks.append(0)
            else:
                p = e.alloc(8)
                e.write(p, vt.to_bytes(4, "big"))
                parks.append(p)
        e.world = {"parks": list(parks), "park_set": set(p for p in parks if p)}
        e.write(this + 0x24, (case["f24"] & MASK).to_bytes(4, "big") + (case["f28"] & MASK).to_bytes(4, "big") + lst.to_bytes(4, "big"))
        rm = case["recv"]
        recv = 0
        if rm:
            recv = e.alloc(16)
            info = 0 if rm == 2 else e.alloc(0x70)
            e.write(recv, (1).to_bytes(4, "big") + info.to_bytes(4, "big") + (0).to_bytes(4, "big") + (2 if rm == 3 else 1).to_bytes(4, "big"))
        e.write(this + 0x30, recv.to_bytes(4, "big"))
        r3, _, _ = T.call_ppc(e, DTOR, [0 if case["isnull"] else this, case["flag"] & 0xFFFF])
        out = {"answer": 1 if r3 & MASK else 0, "log": L.log}
        if case["isnull"] or case["flag"] > 0:
            out.update(f24=0, f28=0, ref=0)
        else:
            out.update(f24=e.u32(this + 0x24), f28=e.u32(this + 0x28), ref=e.u32(recv + 0xC) if rm == 3 else 0)
        return out
    finally:
        CL.Stubs.deactivate(e, saved)


def host_dtor(lib, case):
    bind(lib)
    CL.host_begin(lib, {1: lib.tpw_ic_addr(b"city")}, case["queues"])
    out = (C.c_int32 * 3)()
    ans = lib.tpw_ic_dtor(case["flag"], case["isnull"], case["n"], case["nullmask"], case["recv"], T.s32(case["f24"]), T.s32(case["f28"]), out)
    return {"answer": ans, "log": CL.host_end(lib), "f24": out[0] & MASK, "f28": out[1] & MASK, "ref": out[2]}


def ppc_req(e, case):
    L = e.cl = CL.PpcLog({CITY[0]: 1}, case["queues"])
    saved = STUBS.activate(e)
    try:
        this = e.alloc(0x38)
        e.write(this + 0x34, (case["f34"] & MASK).to_bytes(4, "big"))
        r3, _, _ = T.call_ppc(e, REQ, [this, CITY[0], case["ul"] & MASK])
        return {"answer": T.s32(r3), "log": L.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host_req(lib, case):
    bind(lib)
    CL.host_begin(lib, {1: lib.tpw_ic_addr(b"city")}, case["queues"])
    ans = lib.tpw_ic_request(T.s32(case["f34"]), case["ul"] & MASK)
    return {"answer": ans, "log": CL.host_end(lib)}


def dtor_case(r, **kw):
    c = {"flag": r.choice([0, 1, -1, 2, 5, -32768]), "isnull": 0, "n": r.randrange(0, 5), "nullmask": r.randrange(16), "recv": r.randrange(4),
         "f24": r.getrandbits(32), "f28": r.getrandbits(32), "queues": {}}
    c.update(kw)
    return c


def dtor_edges():
    r = random.Random(11)
    yield dtor_case(r, isnull=1, flag=1)
    yield dtor_case(r, isnull=1, flag=0)
    for flag in (0, 1, -1):
        for recv in range(4):
            for n, mask in ((0, 0), (1, 0), (1, 1), (3, 2), (4, 0), (4, 15)):
                yield dtor_case(r, flag=flag, recv=recv, n=n, nullmask=mask)


def dtor_random(r):
    return dtor_case(r)


def req_case(r):
    return {"f34": r.choice([-1, 0, 1, r.getrandbits(32)]), "ul": r.choice([0, 1, r.getrandbits(32)]),
            "queues": {"GetParkInCityList": [r.choice([0, 1, -5, r.getrandbits(31)])]}}


def req_edges():
    r = random.Random(12)
    for f34 in (-1, 0, 7, 0x7FFFFFFF):
        for ul in (0, 0xFFFFFFFF, 5):
            c = req_case(r)
            c["f34"], c["ul"] = f34, ul
            yield c


def req_random(r):
    return req_case(r)


TESTS = [
    FnTest("CFSOnlineSystemParkList_InCity::~CFSOnlineSystemParkList_InCity", ppc_dtor, host_dtor, edge=dtor_edges, rnd=dtor_random),
    FnTest("CFSOnlineSystemParkList_InCity::RequestListFromServer", ppc_req, host_req, edge=req_edges, rnd=req_random),
]
