"""Unit v5154, CFSOnlineSystemParkList::~CFSOnlineSystemParkList 0x1016fd48, against
libtpw_unit_CFSOnlineSystemParkList_Dtor_CFSOnlineSystemPark.dylib (tests/unit_CFSOnlineSystemParkList_Dtor_CFSOnlineSystemPark.cpp), main app,
src/game/online/FS_Online.cpp.

A case is a world and a call. The world is a park list (ten words at 0x04..0x28, the typed list at 0x2c holding 0 to 3 parks, some of them
NULL entries, and the stored answer at 0x30, absent or with a reference count, a count and an info array). The Mac side is built in emulator
memory with the Mac's layout (the typed list's real RemoveHead and the pointer glue run as the original); the host side is built by the
library's wrappers (the host's pointers are wide). The callees outside the unit are fakes on both sides recording the same events: operator
delete (the answer, the list nodes, the object itself when the delete flag is positive), the infos' array destruction (the Mac's
__destroy_new_array with its table argument; one info per array), the parks' and the list's deleting destructors (r4 = 1).

What is compared: the event log, the ten words, the stored answer pointer (as a label) and the answer's four fields, and that the
Mac's destructor returns this. No fused operations. Not tested: this NULL (the host cannot call a member on NULL), a list with a count
but no nodes (the original would loop forever).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystemParkList_Dtor_CFSOnlineSystemPark"
MASK = 0xFFFFFFFF

FN = 0x1016FD48
SIZES = {FN: 300}
OWN = set(SIZES)

REMOVE_HEAD = 0x10177B98
PTR_GLUE = 0x1023BAA8
DESTROY_ARRAY = 0x10238AD0
OP_DELETE = 0x10238218
PARK_DTOR = 0x10170B34      # code addresses of functions this unit never runs, standing for the virtual destructors
LIST_DTOR = 0x10170CA8

LAB_THIS, LAB_LIST, LAB_RECV, LAB_INFOS, LAB_PARK = 1, 2, 3, 4, 10

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.update({REMOVE_HEAD, PTR_GLUE})


def s32(v):
    return T.s32(v)


def stub_delete(e):
    L = e.cl
    # the host's delete is recorded only for objects the case labels (the list nodes are not), so the fake does the same
    if L.lab(e.r(3)) != -1:
        L.rec("delete", (L.lab(e.r(3)),))


def stub_destroy_array(e):
    L = e.cl
    L.rec("destroy_array", (L.lab(e.r(3)), e.r(4) & MASK))


def stub_park_dtor(e):
    L = e.cl
    L.rec("park_dtor", (L.lab(e.r(3)), s32(e.r(4))))


def stub_list_dtor(e):
    L = e.cl
    L.rec("list_dtor", (L.lab(e.r(3)), s32(e.r(4))))


STUBS.internal(OP_DELETE, stub_delete)
STUBS.internal(DESTROY_ARRAY, stub_destroy_array)
STUBS.internal(PARK_DTOR, stub_park_dtor)
STUBS.internal(LIST_DTOR, stub_list_dtor)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def lbl(self, addr):
        return self.labels.get(addr, -1) if addr else 0


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.case = case

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def make(self):
        e, case = self.e, self.case
        toc = e.r(2)
        tvp = e.alloc(8)
        self.words(tvp, [PARK_DTOR, toc])
        tvl = e.alloc(8)
        self.words(tvl, [LIST_DTOR, toc])
        pvt = e.alloc(16)
        self.words(pvt, [0, 0, tvp, 0])
        lvt = e.alloc(16)
        self.words(lvt, [0, 0, tvl, 0])
        self.labels = {}
        parks = []
        for i, isnull in enumerate(case["parks"]):
            if isnull:
                parks.append(0)
            else:
                a = e.alloc(16)
                self.words(a, [pvt, 0, 0, 0])
                parks.append(a)
                self.labels[a] = LAB_PARK + i
        nodes = [e.alloc(12) for _ in parks]
        for i, n in enumerate(nodes):
            nxt = nodes[i + 1] if i + 1 < len(nodes) else 0
            prv = nodes[i - 1] if i > 0 else 0
            self.words(n, [parks[i], nxt, prv])
        lst = e.alloc(16)
        self.words(lst, [lvt, nodes[0] if nodes else 0, nodes[-1] if nodes else 0, len(nodes)])
        self.recv = 0
        infos = 0
        r = case["recv"]
        if r:
            self.recv = e.alloc(16)
            if r["infos"]:
                infos = e.alloc(0x66)
                self.labels[infos] = LAB_INFOS
            self.words(self.recv, [r["count"], infos, r["f08"], r["refcount"]])
            self.labels[self.recv] = LAB_RECV
        self.this = e.alloc(0x34)
        self.words(self.this, [0] + case["words"] + [lst, self.recv])
        self.labels[self.this] = LAB_THIS
        self.labels[lst] = LAB_LIST
        self.L = e.cl = CL.PpcLog(dict(self.labels), {})
        self.saved = STUBS.activate(e)

    def call(self):
        r3, _, _ = T.call_ppc(self.e, FN, [self.this, self.case["flag"] & MASK])
        return (r3 & MASK) == self.this

    def observe(self, ret):
        out = {"ret_this": ret, "log": self.L.log}
        if not this_freed(self.case):
            out["words"] = self.rd_words(self.this + 4, 10)
            out["received"] = self.lbl(self.rd_word(self.this + 0x30))
        if self.recv and not recv_freed(self.case):
            f = self.rd_words(self.recv, 4)
            out["recv"] = [f[0], self.lbl(f[1]), f[2], f[3]]
        return out

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)

    def make(self):
        lib, case = self.lib, self.case
        w = (C.c_int32 * 10)(*[s32(x) for x in case["words"]])
        self.this = lib.tpw_pl_new(w)
        labels = {LAB_THIS: self.this, LAB_LIST: lib.tpw_pl_list(self.this)}
        self.recv = 0
        r = case["recv"]
        if r:
            out = (C.c_uint64 * 2)()
            lib.tpw_pl_recv(self.this, s32(r["refcount"]), 1 if r["infos"] else 0, s32(r["count"]), s32(r["f08"]), out)
            self.recv = out[0]
            labels[LAB_RECV] = out[0]
            if out[1]:
                labels[LAB_INFOS] = out[1]
        for i, isnull in enumerate(case["parks"]):
            p = lib.tpw_pl_park(self.this, 1 if isnull else 0)
            if p:
                labels[LAB_PARK + i] = p
        self.rev = {v: k for k, v in labels.items()}
        self.labels = self.rev
        CL.host_begin(lib, labels, {})

    def call(self):
        f = case_flag16(self.case["flag"])
        self.lib.tpw_pl_run(self.this, f)
        return True

    def observe(self, ret):
        out = {"ret_this": ret}
        if not this_freed(self.case):
            st = (C.c_int64 * 11)()
            self.lib.tpw_pl_state(self.this, st)
            out["words"] = [x & MASK for x in list(st)[:10]]
            out["received"] = self.lbl(st[10] & ((1 << 64) - 1))
        out["log"] = CL.host_end(self.lib)
        if self.recv and not recv_freed(self.case):
            rs = (C.c_int64 * 4)()
            self.lib.tpw_pl_recvstate(self.recv, rs)
            out["recv"] = [rs[0] & MASK, self.lbl(rs[1] & ((1 << 64) - 1)), rs[2] & MASK, rs[3] & MASK]
        return out

    def done(self):
        pass


def this_freed(case):
    """A positive delete flag frees the object (memory the host may reuse): its fields are not read back."""
    return case_flag16(case["flag"]) > 0


def recv_freed(case):
    return case["recv"] is not None and ((case["recv"]["refcount"] - 1) & MASK) == 0


def case_flag16(flag):
    v = flag & 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, i32, vp = C.c_uint64, C.c_int32, C.c_void_p
    lib.tpw_pl_new.restype, lib.tpw_pl_new.argtypes = u64, [vp]
    lib.tpw_pl_recv.restype, lib.tpw_pl_recv.argtypes = None, [u64, i32, i32, i32, i32, vp]
    lib.tpw_pl_park.restype, lib.tpw_pl_park.argtypes = u64, [u64, i32]
    lib.tpw_pl_list.restype, lib.tpw_pl_list.argtypes = u64, [u64]
    lib.tpw_pl_run.restype, lib.tpw_pl_run.argtypes = None, [u64, i32]
    lib.tpw_pl_state.restype, lib.tpw_pl_state.argtypes = None, [u64, vp]
    lib.tpw_pl_recvstate.restype, lib.tpw_pl_recvstate.argtypes = None, [u64, vp]
    lib.bound = True


def run(s, case):
    try:
        s.make()
        return s.observe(s.call())
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ------------------------------------------------------------------------------------------------------------------ cases

FLAGS = [1, 0, 0xFFFF, 5, 0x8000, 0x7FFF, 0xFFFF0001, 0x00010000, 0x12340000, 0xFFFFFFFF, 2]


def make_case(r, parks, recv, flag, words=None):
    return {"queues": {}, "parks": parks, "recv": recv, "flag": flag,
            "words": words if words is not None else [r.getrandbits(32) for _ in range(10)]}


def rrecv(r):
    k = r.random()
    if k < 0.3:
        return None
    return {"refcount": r.choice([1, 1, 2, 0, 0xFFFFFFFF, 5, 0x80000001]), "infos": r.random() < 0.7,
            "count": r.getrandbits(32), "f08": r.getrandbits(32)}


def edges():
    r = random.Random(5154)
    recvs = [None,
             {"refcount": 1, "infos": True, "count": 3, "f08": 9},
             {"refcount": 1, "infos": False, "count": 0, "f08": 0},
             {"refcount": 2, "infos": True, "count": 1, "f08": 7},
             {"refcount": 0, "infos": True, "count": 1, "f08": 7},
             {"refcount": 0x80000001, "infos": True, "count": 1, "f08": 7}]
    plists = [[], [False], [True], [False, False, False], [False, True, False], [True, True]]
    for rc in recvs:
        for pl in plists:
            for fl in (1, 0, 0xFFFF):
                yield make_case(r, pl, rc, fl)
    for fl in FLAGS:
        yield make_case(r, [False], None, fl)
    yield make_case(r, [], None, 1, [0] * 10)
    yield make_case(r, [], None, 1, [MASK] * 10)


def rnd(r):
    n = r.choice([0, 0, 1, 2, 3])
    parks = [r.random() < 0.25 for _ in range(n)]
    return make_case(r, parks, rrecv(r), r.choice(FLAGS) if r.random() < 0.7 else r.getrandbits(32))


TESTS = [
    FnTest("CFSOnlineSystemParkList::~CFSOnlineSystemParkList", ppc, host, edge=edges, rnd=rnd),
]
