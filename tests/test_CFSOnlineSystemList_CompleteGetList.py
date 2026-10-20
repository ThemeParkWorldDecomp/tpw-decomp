"""Unit v5662, the ride instance of the list's completion step, against libtpw_unit_CFSOnlineSystemList_CompleteGetList.dylib
(tests/unit_CFSOnlineSystemList_CompleteGetList.cpp), main app ("SimTheme Park"), src/FS_Online_init_CFSOnlineSystemList_CompleteGetList.cpp:
  CFSOnlineSystemList<TbPtrList<CFSOnlineRide>,ccRideList>::CompleteGetList 0x10174410

A case is a world and a call. The world is built twice: in emulator memory (big-endian, the Mac layout) and in the host library's memory
(the host layout, with a real TbPtrList). The callees outside the unit are fakes on both sides that record the same events: AddUIError,
the CFSOnlineSystem constructor and Logout, the three __ptr_glue calls (the list's virtual StoreReceivedList, a ride's virtual destructor,
the callback), operator new, operator delete and __destroy_new_array. TbTList::RemoveHead is real on the host and emulated on the PowerPC
side (it answers the case's ride pointers in order and counts the list down); the rides' deletions show its order.

The world of a case: the words 0x04..0x28 of the list, the rides in the list (some NULL), the stored answer (count, infos, word 0x08),
gOnlineSystem (NULL or an object). What is compared: the words, the list count, the stored answer's state, gOnlineSystem as 0 / old / new
and the event log.

No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystemList_CompleteGetList"
MASK = 0xFFFFFFFF

FN = 0x10174410
ADDR = dict(gOnlineSystem=0x10380ab0)
CALLEE = dict(AddUIError=0x101f1df0, RemoveHead=0x10177758, PtrGlue=0x1023baa8, DestroyArray=0x10238ad0, OperatorDelete=0x10238218,
              OperatorNew=0x102384b8, Ctor=0x1016f7d0, Logout=0x10171398)
DTOR_VECTOR = 0x10267488
HOST_LAYOUT = [8, 44, 48, 56, 8, 16, 20]
WORDS = list(range(0x04, 0x2C, 4))

PERSIST = {}
STATE = {"rides": []}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.add(FN)


def st_add_ui_error(e):
    L = e.cl
    L.rec("AddUIError", (s32(e.r(3)), L.lab(e.r(4))))


def st_remove_head(e):
    lst = e.r(3)
    count = e.u32(lst + 0xC)
    if count == 0:
        e.set_r(3, 0)
        return
    ride = STATE["rides"].pop(0)
    e.write(lst + 0xC, struct.pack(">I", (count - 1) & MASK))
    e.set_r(3, ride)


def st_ptr_glue(e):
    L = e.cl
    r12 = e.r(12)
    if r12 == PERSIST["D_dtor"]:
        L.rec("RideDelete", (L.lab(e.r(3)), s32(e.r(4))))
    elif r12 == PERSIST["D_store"]:
        L.rec("Store", (L.lab(e.r(3)), L.lab(e.r(4))))
    else:
        L.rec("Callback", (s32(r12), s32(e.r(3)), s32(e.r(4)), L.lab(e.r(5))))


def st_destroy_array(e):
    L = e.cl
    assert e.r(4) == DTOR_VECTOR, hex(e.r(4))
    for i in (1, 0):
        L.rec("InfoDtor", (L.lab(e.r(3) + 0x34 * i),))


def st_delete(e):
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)),))


def st_new(e):
    assert e.r(3) == 0x404, hex(e.r(3))
    e.set_r(3, e.alloc(0x404))


STUBS.internal(CALLEE["AddUIError"], st_add_ui_error)
STUBS.internal(CALLEE["RemoveHead"], st_remove_head)
STUBS.internal(CALLEE["PtrGlue"], st_ptr_glue)
STUBS.internal(CALLEE["DestroyArray"], st_destroy_array)
STUBS.internal(CALLEE["OperatorDelete"], st_delete)
STUBS.internal(CALLEE["OperatorNew"], st_new)
STUBS.internal(CALLEE["Ctor"], CL.recorder("Ctor", "N"))
STUBS.internal(CALLEE["Logout"], CL.recorder("Logout", "L"))


def setup(runner):
    T.install_std_stubs(runner.emu)
    e = runner.emu
    PERSIST["D_dtor"] = T.persist_alloc(e, 8)
    PERSIST["D_store"] = T.persist_alloc(e, 8)
    PERSIST["ride_vt"] = T.persist_alloc(e, 16, struct.pack(">IIII", 0, 0, PERSIST["D_dtor"], 0))
    PERSIST["list_vt"] = T.persist_alloc(e, 32, struct.pack(">IIIIII", 0, 0, 0, 0, 0, PERSIST["D_store"]) + bytes(8))


class Side:
    fmt = ">I"

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def labels(self, A):
        fixed = {A["obj"]: 1, A["list"]: 2}
        if A["recv"]:
            fixed[A["recv"]] = 3
        if A["infos"]:
            fixed[A["infos"]] = 4
            fixed[A["infos"] + 0x34] = 20
        if A["gos_old"]:
            fixed[A["gos_old"]] = 5
        for i, r in enumerate(A["rides"]):
            if r:
                fixed[r] = 10 + i
        return fixed


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        A = self.A = self.make(case)
        STATE["rides"] = list(A["rides"])
        self.L = e.cl = CL.PpcLog(self.labels(A), case["queues"])
        self.saved = STUBS.activate(e)

    def make(self, case):
        e = self.e
        rides = []
        for r in case["rides"]:
            if r:
                rides.append(e.alloc(0x70))
                self.words(rides[-1], [PERSIST["ride_vt"]])
            else:
                rides.append(0)
        lst = e.alloc(0x10)
        self.words(lst + 0xC, [len(rides)])
        recv = infos = 0
        if case["recv"] is not None:
            has_infos, refc, f08 = case["recv"]
            recv = e.alloc(0x10)
            if has_infos:
                infos = e.alloc(0x70)
            self.words(recv, [0, infos, f08, refc])
        obj = e.alloc(0x34)
        self.words(obj, [PERSIST["list_vt"]])
        self.words(obj + 0x2C, [lst, recv])
        old = e.alloc(0x10) if case["gos"] else 0
        return dict(obj=obj, list=lst, recv=recv, infos=infos, rides=rides, gos_old=old)

    def field(self, off):
        return self.A["obj"] + off

    def set_gos(self, v):
        self.words(ADDR["gOnlineSystem"], [v])

    def gos(self):
        v = self.rd_words(ADDR["gOnlineSystem"], 1)[0]
        return 0 if v == 0 else (1 if v == self.A["gos_old"] else 2)

    def state(self, deleted):
        A = self.A
        count = self.rd_words(A["list"] + 0xC, 1)[0]
        recv_now = self.rd_words(A["obj"] + 0x30, 1)[0] != 0
        if A["recv"] and not deleted:
            _, inf, f08, refc = self.rd_words(A["recv"], 4)
            return [s32(count), int(recv_now), s32(refc), int(inf != 0), s32(f08)]
        return [count, int(recv_now), 0, 0, 0]

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, error):
        T.call_ppc(self.e, FN, [self.A["obj"], error & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        n = len(case["rides"])
        rides = []
        for r in case["rides"]:
            rides.append(lib.tpw_cgl_alloc(0x20) if r else 0)
        arr = (C.c_uint64 * max(n, 1))(*rides)
        out = (C.c_uint64 * 4)()
        has = case["recv"] is not None
        hi, refc, f08 = case["recv"] if has else (0, 0, 0)
        lib.tpw_cgl_build(n, arr, int(has), int(hi), s32(refc), s32(f08), out)
        old = lib.tpw_cgl_alloc(0x10) if case["gos"] else 0
        self.A = dict(obj=out[0], list=out[1], recv=out[2], infos=out[3], rides=rides, gos_old=old)
        self.base = HOST_LAYOUT[0]
        CL.host_begin(lib, {v: k for k, v in self.labels(self.A).items()}, case["queues"])

    def field(self, off):
        return self.A["obj"] + self.base + (off - 4)

    def set_gos(self, v):
        self.lib.tpw_cgl_set_gos(v)

    def gos(self):
        v = self.lib.tpw_cgl_get_gos()
        return 0 if v == 0 else (1 if v == self.A["gos_old"] else 2)

    def state(self, deleted):
        out = (C.c_int64 * 5)()
        self.lib.tpw_cgl_state(self.A["obj"], 0 if deleted else self.A["recv"], out)
        return list(out)

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, error):
        self.lib.tpw_cgl_complete(self.A["obj"], s32(error))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cgl_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cgl_build.argtypes = [C.c_int32, C.POINTER(C.c_uint64), C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.POINTER(C.c_uint64)]
    lib.tpw_cgl_state.argtypes = [C.c_uint64, C.c_uint64, C.POINTER(C.c_int64)]
    lib.tpw_cgl_set_gos.argtypes = [C.c_uint64]
    lib.tpw_cgl_get_gos.restype = C.c_uint64
    lib.tpw_cgl_complete.argtypes = [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cgl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    f = case["f"]
    s.words(s.field(0x04), [f[o] for o in WORDS])
    s.set_gos(s.A["gos_old"])


def observe(s, case):
    log = s.log()
    deleted = any(ev[0] == "delete" for ev in log)
    return {"words": s.rd_words(s.field(0x04), 10), "state": s.state(deleted), "gos": s.gos(), "log": log}


def run(s, case):
    try:
        build(s, case)
        s.call(case["error"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


CBS = [0, 0x7001, 0x12345678, 0xFFFFFFFF, 1]


def mk(r, error=None, f18=None, f14=None, f08=None, f28=None, cb=None):
    f = {o: r.choice([0, 1, 5, 0xFFFFFFFF, r.randrange(0, 1 << 32)]) for o in WORDS}
    f[0x18] = r.choice([0, 3, 0x80000000]) if f18 is None else f18
    f[0x14] = r.choice([0, 1, 7]) if f14 is None else f14
    f[0x08] = r.choice([0, 1, 9]) if f08 is None else f08
    f[0x28] = r.choice([0, 1, 5]) if f28 is None else f28
    f[0x1C] = r.choice(CBS) if cb is None else cb
    n = r.choice([0, 1, 2, 3, 4])
    rides = [r.choice([1, 1, 1, 0]) for _ in range(n)]
    recv = None
    if r.random() < 0.75:
        recv = (r.choice([0, 1]), r.choice([1, 1, 2, 0, 0xFFFFFFFF, 3]), r.choice([0, 1, 77, 0xFFFFFFFF]))
    return {"queues": {}, "error": r.choice([0, 0, 1, -1, 5]) if error is None else error, "f": f, "rides": rides, "recv": recv,
            "gos": r.choice([0, 1])}


def edges():
    r = random.Random(5662)
    for error in (0, 1):
        for f18 in (0, 3):
            for f14 in (0, 1):
                for f08 in (0, 1):
                    for f28 in (0, 1):
                        for cb in (0, 0x7001):
                            yield mk(r, error, f18, f14, f08, f28, cb)
    c = mk(r, 0, 0, 1, 1, 0, 0)
    c["rides"], c["recv"], c["gos"] = [1, 0, 1], (1, 1, 5), 0
    yield c
    c = mk(r, 0, 0, 1, 1, 0, 0)
    c["rides"], c["recv"], c["gos"] = [1, 0, 1], (1, 2, 5), 1
    yield c
    c = mk(r, 0, 0, 1, 0, 0, 0)
    c["rides"], c["recv"], c["gos"] = [], None, 0
    yield c


def rnd(r):
    return mk(r)


TESTS = [
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlineRide>,ccRideList>::CompleteGetList", ppc, host, edge=edges, rnd=rnd),
]
