"""Unit v5215: the city list's LoggedInGetList and CompleteGetList, against libtpw_unit_CFSOnlineSystemList_LoggedInGetList.dylib
(tests/unit_CFSOnlineSystemList_LoggedInGetList.cpp), main app, src/game/online/FS_OnlineList.cpp:
  CFSOnlineSystemList<TbPtrList<CFSOnlineCity>,ccCityList>::LoggedInGetList 0x10178414
  CFSOnlineSystemList<TbPtrList<CFSOnlineCity>,ccCityList>::CompleteGetList 0x10173f98

A case is a world and a call. The list object (Mac layout on the PowerPC side, a subclass of the real template with recording virtual
slots on the host) is built from the same case. The callees outside the unit are fakes on both sides that record the same events:
AddUIError, CFSOnlineSystem's constructor, Logout, Instance, ReturnConnectionToCityServer, the three virtual slots (ChildLocked,
RequestListFromServer, StoreReceivedList), the list callback (__ptr_glue on the Mac, a hook on the host), a city record's virtual
destructor (a hook on the host), the array destructor of the infos. TbTList::RemoveHead runs as the original on the PowerPC side
and as the header template on the host.

The world of a case: the ten list words (mRequestID..mF28), the records in the list (some NULL entries), a stored answer with its
reference count, gOnlineSystem set or not. What is compared: the event log, the ten words, whether the answer is still held, its
reference count when it survives, the list count, which kind of object gOnlineSystem holds.

No fused operations. Not tested: the free of the answer block itself (both sides free it; the infos' destruction is compared).
"""
import ctypes as C
import random
import itertools

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystemList_LoggedInGetList"
MASK = 0xFFFFFFFF

FN_LOGGED, FN_COMPLETE = 0x10178414, 0x10173f98
SIZES = {FN_LOGGED: 412, FN_COMPLETE: 468}
OWN = set(SIZES)

PTR_GLUE = 0x1023BAA8
ADDUI, SYS_CTOR, RRC, INSTANCE, LOGOUT = 0x101F1DF0, 0x1016F7D0, 0x1017346C, 0x10178618, 0x10171398
ADDR = dict(gsys=0x10380AB0)
DTOR_TVEC = 0x102674F0

ID_CL, ID_REQ, ID_STORE, ID_DTOR, ID_CDTOR = 0x7F00000C, 0x7F000010, 0x7F000014, 0x7F000008, 0x7F000108
CITY, INST = 0x7F0000A0, 0x7F0000B0
CB_TOKEN = 0x5C000001
HOST_LAYOUT = []

CUR = [0]   # the infos count of the case on the PowerPC side


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_glue(e):
    L = e.cl
    t = e.r(12)
    if t == ID_CL:
        L.rec("ChildLocked", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("ChildLocked") & MASK)
    elif t == ID_REQ:
        L.rec("Request", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK))
        e.set_r(3, L.ret("Request") & MASK)
    elif t == ID_STORE:
        L.rec("Store", (L.lab(e.r(3)), L.lab(e.r(4))))
    elif t == ID_CDTOR:
        L.rec("city_delete", (L.lab(e.r(3)), s32(e.r(4))))
    else:
        L.rec("callback", (s32(t), s32(e.r(3)), s32(e.r(4)), L.lab(e.r(5))))


def stub_addui(e):
    L = e.cl
    L.rec("AddUIError", (s32(e.r(3)), L.lab(e.r(4))))


def stub_rrc(e):
    L = e.cl
    L.rec("RRC", (L.lab(e.r(3)),))
    e.set_r(3, CITY if L.ret("RRC") else 0)


def stub_instance(e):
    e.cl.rec("Instance", ())
    e.set_r(3, INST)


def stub_logout(e):
    L = e.cl
    L.rec("Logout", (L.lab(e.r(3)),))


def stub_destroy_array(e):
    assert e.r(4) == DTOR_TVEC, hex(e.r(4))
    for _ in range(CUR[0]):
        e.cl.rec("info_dtor", ())


STUBS.internal(PTR_GLUE, stub_glue)
STUBS.internal(ADDUI, stub_addui)
STUBS.internal(SYS_CTOR, CL.recorder("sys_ctor", "N"))
STUBS.internal(RRC, stub_rrc)
STUBS.internal(INSTANCE, stub_instance)
STUBS.internal(LOGOUT, stub_logout)
STUBS.internal(0x10238AD0, stub_destroy_array)                       # __destroy_new_array
STUBS.internal(0x102384B8, lambda e: e.set_r(3, e.alloc(max(4, e.r(3)))))   # operator new (never NULL)


def stub_delete(e):
    lab = e.cl.lab(e.r(3))
    if lab != -1:
        e.cl.rec("delete", (lab,))


STUBS.internal(0x10238218, stub_delete)                               # operator delete


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(int(w & MASK).to_bytes(4, self.order) for w in ws))

    def rd_words(self, a, n):
        return [int.from_bytes(self.read(a + 4 * i, 4), self.order) for i in range(n)]

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    order = "big"

    def __init__(self, e, case):
        self.e = e
        self.case = case
        CUR[0] = case["ninfos"]
        al = e.alloc
        vt, evt = al(0x18), al(0xC)
        self.words(vt, [0, 0, ID_DTOR, ID_CL, ID_REQ, ID_STORE])
        self.words(evt, [0, 0, ID_CDTOR])
        self.obj = al(0x34)
        lst = al(0x10)
        fixed = {self.obj: 1, lst: 2, CITY: 5, INST: 6}
        head = tail = 0
        prev = 0
        for i, flag in enumerate(case["elems"]):
            ea = 0
            if flag:
                ea = al(8)
                self.words(ea, [evt, 0])
                fixed[ea] = 10 + i
            node = al(12)
            self.words(node, [ea, 0, prev])
            if prev:
                self.words(prev + 4, [node])
            else:
                head = node
            prev = tail = node
        self.words(lst, [0, head, tail, len(case["elems"])])
        recv = 0
        if case["recv"]:
            recv = al(0x10)
            infos = al(16) if case["ninfos"] > 0 else 0
            self.words(recv, [case["ninfos"], infos, case["rf08"], case["rcount"]])
            fixed[recv] = 3
            if infos:
                fixed[infos] = 4
        self.recv = recv
        self.words(self.obj, [vt] + case["words"] + [lst, recv])
        self.lst = lst
        sysaddr = al(8) if case["sys"] else 0
        self.sysaddr = sysaddr
        if sysaddr:
            fixed[sysaddr] = 7
        self.words(ADDR["gsys"], [sysaddr])
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, case):
        if case["op"] == "logged":
            T.call_ppc(self.e, FN_LOGGED, [self.obj])
        else:
            T.call_ppc(self.e, FN_COMPLETE, [self.obj, case["error"] & MASK])

    def obs(self, case):
        w = self.rd_words(self.obj + 4, 10)
        recv_now = self.rd_word(self.obj + 0x30)
        g = self.rd_word(ADDR["gsys"])
        kind = 0 if g == 0 else (1 if g == self.sysaddr else 2)
        rc = s32(self.rd_word(self.recv + 0xC)) if (case["recv"] and case["rcount"] != 1) else 0
        return w + [int(recv_now != 0), rc, self.rd_word(self.lst + 0xC), kind]


class Host(Side):
    order = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        w = (C.c_int32 * 10)(*[s32(x) for x in case["words"]])
        el = (C.c_int32 * 8)(*[int(f) for f in case["elems"]] + [0] * (8 - len(case["elems"])))
        self.obj = lib.tpw_cg_make(w, el, len(case["elems"]), int(case["recv"]), case["rcount"], case["rf08"], case["ninfos"],
                                   int(case["sys"]))
        fixed = {1: self.obj, 5: lib.tpw_cg_addr(b"city"), 6: lib.tpw_cg_addr(b"inst")}
        fixed[2] = lib.tpw_cg_ptr(self.obj, 0)
        if case["recv"]:
            fixed[3] = lib.tpw_cg_ptr(self.obj, 1)
            if case["ninfos"] > 0:
                fixed[4] = lib.tpw_cg_ptr(self.obj, 2)
        if case["sys"]:
            fixed[7] = lib.tpw_cg_addr(b"sys")
        for i, flag in enumerate(case["elems"]):
            if flag:
                fixed[10 + i] = lib.tpw_cg_ptr(self.obj, 3 + i)
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def log(self):
        return CL.host_end(self.lib)

    def call(self, case):
        if case["op"] == "logged":
            self.lib.tpw_cg_logged(self.obj)
        else:
            self.lib.tpw_cg_complete(self.obj, s32(case["error"]))

    def obs(self, case):
        out = (C.c_int32 * 16)()
        self.lib.tpw_cg_obs(self.obj, int(bool(case["recv"] and case["rcount"] != 1)), out)
        return [x & MASK if i < 10 else x for i, x in enumerate(list(out)[:14])]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cg_addr.restype, lib.tpw_cg_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cg_alloc.restype, lib.tpw_cg_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cg_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cg_make.restype = C.c_uint64
    lib.tpw_cg_make.argtypes = [C.c_void_p, C.c_void_p, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_cg_ptr.restype, lib.tpw_cg_ptr.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_cg_obs.restype, lib.tpw_cg_obs.argtypes = None, [C.c_uint64, C.c_int32, C.c_void_p]
    lib.tpw_cg_logged.restype, lib.tpw_cg_logged.argtypes = None, [C.c_uint64]
    lib.tpw_cg_complete.restype, lib.tpw_cg_complete.argtypes = None, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 4)()
    n = lib.tpw_cg_layout(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.bound = True


def run(s, case):
    try:
        s.call(case)
        lg = s.log()
        return {"log": lg, "txt": "".join(str(x[0])[0] for x in lg), "obs": s.obs(case)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


class Case(dict):
    def __repr__(self):
        return "%s w=%s e=%s r=%s/%s/%s s=%s err=%s" % (self["op"][:1], [x & 0xFFFF for x in self["words"]], len(self["elems"]),
                                                      int(self["recv"]), self["rcount"], self["ninfos"], self["sys"], self["error"])


def mk(op, r, **kw):
    c = Case({
        "op": op,
        "words": [r.choice([0, 1, -1, 77]), r.choice([0, 1, 5]), r.choice([0, 1]), r.choice([0, 1]), r.choice([0, 1, 3]),
                  r.choice([0, 0x33]), r.choice([0, CB_TOKEN, 0x1234]), r.choice([0, 9, 0x80000001]), r.choice([0, 5, 0xFFFFFFFF]),
                  r.choice([0, 1, 2])],
        "elems": [],
        "recv": False, "rcount": 1, "rf08": r.choice([0, 7, 123]), "ninfos": 0,
        "sys": r.choice([0, 1]), "error": 0,
        "queues": {"ChildLocked": [r.choice([0, 1])], "Request": [r.choice([0, 5, 99, -3])], "RRC": [r.choice([0, 1])]},
    })
    c.update(kw)
    return c


def setw(c, **kw):
    idx = dict(f08=1, f0c=2, f10=3, f14=4, f18=5, cb=6, ctx=7, f24=8, f28=9)
    for k, v in kw.items():
        c["words"][idx[k]] = v & MASK
    return c


def logged_edges():
    r = random.Random(11)
    for cl, sysx, rrc, f08, cb, f18 in itertools.product((0, 1), (0, 1), (0, 1), (0, 1), (0, CB_TOKEN), (0, 0x33)):
        c = mk("logged", r, sys=sysx)
        c["queues"] = {"ChildLocked": [cl], "Request": [42], "RRC": [rrc]}
        setw(c, f08=f08, cb=cb, f18=f18)
        yield c


def logged_random(r):
    c = mk("logged", r)
    c["words"][6] = r.choice([0, CB_TOKEN])
    return c


RECVS = [(False, 1, 0), (True, 1, 2), (True, 3, 1), (True, 0, 3), (True, 1, 0)]
ELEMS = [[], [True], [True, False, True], [False]]


def complete_edges():
    r = random.Random(12)
    for err, f18, f14, el, rv, f08, sysx, cb in itertools.product(
            (0, 1), (0, 5), (0, 1), range(len(ELEMS)), range(len(RECVS)), (0, 1), (0, 1), (0, CB_TOKEN)):
        has, rc, ni = RECVS[rv]
        c = mk("complete", r, sys=sysx, error=err, elems=list(ELEMS[el]), recv=has, rcount=rc, ninfos=ni)
        setw(c, f18=f18, f14=f14, f08=f08, cb=cb)
        yield c


def complete_random(r):
    has, rc, ni = r.choice(RECVS)
    c = mk("complete", r, error=r.choice([0, 0, 1, -1, 4]), elems=list(r.choice(ELEMS)), recv=has, rcount=rc, ninfos=ni)
    c["words"][4] = r.choice([0, 1, 1])
    c["words"][5] = r.choice([0, 0, 0x33])
    c["words"][6] = r.choice([0, CB_TOKEN])
    return c


TESTS = [
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlineCity>,ccCityList>::LoggedInGetList", ppc, host, edge=logged_edges, rnd=logged_random),
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlineCity>,ccCityList>::CompleteGetList", ppc, host, edge=complete_edges,
           rnd=complete_random),
]
