"""Unit v51129: six functions against libtpw_unit_std_basic_string_assign_2.dylib (tests/unit_std_basic_string_assign_2.cpp), main app
("SimTheme Park"), src/game/MslSupport.cpp, src/game/online/FS_OnlineList.cpp, src/game/ui/InterfaceDrawEngine.cpp:
  std::basic_string<wchar_t>::assign 0x101b2510, InterfaceDrawEngineHGradient::~ 0x10189c0c, CFSOnlineSystemRideList::GetList 0x1017b938,
  CFSOnlineSystemList<..CFSOnlineServer..>::~ 0x1017a6e4, CFSOnlineSystemList<..CFSOnlineTheme..>::LoggedInGetList 0x10179f50,
  CFSOnlineSystemThemeList::~ 0x10179d28.

A case is a world and a call. The world is built twice by one piece of Python code: in emulator memory (big-endian, the Mac's offsets) and in
the host library (native objects, reached through the shim's field accessors). The callees outside the unit are fakes on both sides that
record their arguments (pointers as labels) and answer from the case's queues: change_size and wmemcpy, DMFreeTextures and DMFreeMesh, the
base destructor of the draw object, operator delete, __destroy_new_array (with a kind: ride, server or theme infos), RemoveHead, the virtual
calls made through __ptr_glue (the destructor of a record or list, ChildLocked, RequestListFromServer, the callback), the online system's
constructor, AreWeLoggedIn, Login, Logout, Instance, ReturnConnectionToCityServer, AddUIError and RideListPostLoginCallbackFn.

What is compared: the event log, every word of the list object (0x04 to 0x28), the list's count, the stored answer's pointer and words, the
gOnlineSystem global (as a label), the string's representation pointer and words, the gradient's mData.
No fused operations. Not tested: the hidden delete flag above 0 (the host's C++ destructor has no flag: the PowerPC side is called with 0) and a
NULL this (the C++ destructor cannot be called on NULL). On the host the base destructor of the server list is called as an explicit destructor
call, and the theme list's destructor through its derived test class.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_string_assign_2"
MASK = 0xFFFFFFFF

FN_ASSIGN, FN_HG, FN_RIDE, FN_SRV, FN_LOGGED, FN_THEME = 0x101B2510, 0x10189C0C, 0x1017B938, 0x1017A6E4, 0x10179F50, 0x10179D28
SIZES = {FN_ASSIGN: 108, FN_HG: 140, FN_RIDE: 348, FN_SRV: 288, FN_LOGGED: 412, FN_THEME: 300}
OWN = set(SIZES)

CHANGE_SIZE, WMEMCPY = 0x101B2CD8, 0x102389E0
DM_FREE_TEX, DM_FREE_MESH, OBJ_DTOR = 0x10030178, 0x10030AE8, 0x101CCD9C
OP_DELETE, DESTROY_ARRAY, PTR_GLUE = 0x10238218, 0x10238AD0, 0x1023BAA8
SYS_CTOR, ARE_LOGGED, LOGIN, LOGOUT, INSTANCE, RETURN_CONN, ADD_UI_ERROR, RIDE_CB = (
    0x1016F7D0, 0x1017178C, 0x101710AC, 0x10171398, 0x10178618, 0x1017346C, 0x101F1DF0, 0x1017B604)
REMOVE_HEAD_SRV, REMOVE_HEAD_THEME = 0x1017782C, 0x10177904
TV_RIDE_CB = 0x10267570
TV_INFOS = {0x10267488: 0, 0x10267490: 1, 0x10267498: 2}
SENT_DTOR, SENT_CHILD, SENT_REQ = 0x00700008, 0x0070000C, 0x00700010
SYS_GLOBAL = 0x10380AB0
CALLBACK_WORD = 0x12345678

HOST_LAYOUT = [16]

# labels
L_OBJ, L_LIST, L_RECV, L_INFOS, L_EL1, L_EL2, L_EL3, L_CITY, L_INST, L_SYS = range(1, 11)
L_STR, L_REP, L_ALT, L_DATA, L_DATA2, L_SRC = range(1, 7)
L_TEX, L_MESH = 2, 3


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)

# the PowerPC side's addresses of the case's blocks, set by Ppc.__init__ for the stubs
CUR = {}


def tag(a):
    return a


def st_change_size(e):
    L = e.cl
    L.rec("changeSize", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & 0xFF))
    if L.ret("swap") != 0:
        e.write(e.r(3), (CUR["alt"] & MASK).to_bytes(4, "big"))


def st_wmemcpy(e):
    L = e.cl
    L.rec("wmemcpy", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK))
    # answers dst (r3 unchanged)


def st_dm_tex(e):
    e.cl.rec("DMFreeTextures", (e.cl.lab(e.r(3)),))


def st_dm_mesh(e):
    e.cl.rec("DMFreeMesh", (e.cl.lab(e.r(3)),))
    e.set_r(3, 0)


def st_obj_dtor(e):
    e.cl.rec("ObjDtor", (e.cl.lab(e.r(3)), s32(e.r(4))))


def st_delete(e):
    e.cl.rec("FreeBlock", (e.cl.lab(e.r(3)),))


def st_destroy_array(e):
    e.cl.rec("DestroyInfos", (e.cl.lab(e.r(3)), TV_INFOS[e.r(4)]))


def st_ptr_glue(e):
    L = e.cl
    t = e.r(12) & MASK
    if t == SENT_DTOR:
        L.rec("vdtor", (L.lab(e.r(3)), s32(e.r(4))))
    elif t == SENT_CHILD:
        L.rec("ChildLocked", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("ChildLocked") & MASK)
    elif t == SENT_REQ:
        L.rec("RequestListFromServer", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK))
        e.set_r(3, L.ret("RequestListFromServer") & MASK)
    else:
        L.rec("callback", (t, s32(e.r(3)), s32(e.r(4)), L.lab(e.r(5))))


def st_sys_ctor(e):
    e.cl.rec("SysCtor", (e.cl.lab(e.r(3)),))


def st_are_logged(e):
    L = e.cl
    L.rec("AreWeLoggedIn", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("AreWeLoggedIn") & MASK)


def st_login(e):
    L = e.cl
    L.rec("Login", (L.lab(e.r(3)), 1 if (e.r(4) & MASK) == TV_RIDE_CB else 0, L.lab(e.r(5))))


def st_logout(e):
    e.cl.rec("Logout", (e.cl.lab(e.r(3)),))


def st_instance(e):
    e.cl.rec("Instance", ())
    e.set_r(3, CUR["inst"] & MASK)


def st_return_conn(e):
    L = e.cl
    L.rec("ReturnConn", (L.lab(e.r(3)),))
    e.set_r(3, (CUR["city"] if L.ret("ReturnConn") != 0 else 0) & MASK)


def st_add_error(e):
    e.cl.rec("AddUIError", (s32(e.r(3)), e.cl.lab(e.r(4))))


def st_ride_cb(e):
    e.cl.rec("RideCb", (s32(e.r(3)), e.cl.lab(e.r(4))))


def st_remove_head(e):
    L = e.cl
    lst = e.r(3)
    L.rec("RemoveHead", (L.lab(lst),))
    k = L.ret("RemoveHead")
    e.write(lst + 0xC, ((e.u32(lst + 0xC) - 1) & MASK).to_bytes(4, "big"))
    e.set_r(3, CUR["els"][k - 1] if k > 0 else 0)


for _a, _f in ((CHANGE_SIZE, st_change_size), (WMEMCPY, st_wmemcpy), (DM_FREE_TEX, st_dm_tex), (DM_FREE_MESH, st_dm_mesh),
               (OBJ_DTOR, st_obj_dtor), (OP_DELETE, st_delete), (DESTROY_ARRAY, st_destroy_array), (PTR_GLUE, st_ptr_glue),
               (SYS_CTOR, st_sys_ctor), (ARE_LOGGED, st_are_logged), (LOGIN, st_login), (LOGOUT, st_logout), (INSTANCE, st_instance),
               (RETURN_CONN, st_return_conn), (ADD_UI_ERROR, st_add_error), (RIDE_CB, st_ride_cb),
               (REMOVE_HEAD_SRV, st_remove_head), (REMOVE_HEAD_THEME, st_remove_head)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def lbl(self, addr):
        if addr == 0:
            return 0
        return self.rev.get(addr, -1)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case, blocks):
        """blocks: {name: (size, label)}"""
        self.e = e
        self.b = {}
        for name, (size, label) in blocks.items():
            self.b[name] = e.alloc(size)
        self.fixed = {self.b[n]: blocks[n][1] for n in blocks if blocks[n][1]}
        for off, label in case.get("extra_labels", {}).items():
            self.fixed[self.b[off[0]] + off[1]] = label
        self.rev = dict(self.fixed)
        self.L = e.cl = CL.PpcLog(self.fixed, case["queues"])
        CUR.clear()
        CUR.update(alt=self.b.get("alt", 0), inst=self.b.get("inst", 0), city=self.b.get("city", 0),
                   els=[self.b.get("el1", 0), self.b.get("el2", 0), self.b.get("el3", 0)])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def w(self, a, *words):
        self.e.write(a, b"".join((x & MASK).to_bytes(4, "big") for x in words))

    def u32(self, a):
        return self.e.u32(a)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        return T.call_ppc(self.e, entry, args)[0] & MASK

    # list objects
    def make_list(self, kind):
        b = self.b
        self.w(b["obj"], b["vt_obj"])
        self.w(b["vt_obj"], 0, 0, SENT_DTOR, SENT_CHILD, SENT_REQ)
        self.w(b["list"], b["vt_list"], 0, 0, 0)
        self.w(b["vt_list"], 0, 0, SENT_DTOR)
        self.w(b["vt_elem"], 0, 0, SENT_DTOR)
        for n in ("el1", "el2", "el3"):
            self.w(b[n], b["vt_elem"])
        self.w(b["obj"] + 0x2C, b["list"])

    def set_field(self, off, v):
        self.w(self.b["obj"] + off, v)

    def get_field(self, off):
        return self.u32(self.b["obj"] + off)

    def set_count(self, n):
        self.w(self.b["list"] + 0xC, n)

    def get_count(self):
        return s32(self.u32(self.b["list"] + 0xC))

    def set_recv(self, count, infos, f08, ref):
        b = self.b
        self.w(b["recv"], count, b["infos"] if infos else 0, f08, ref)
        self.w(b["obj"] + 0x30, b["recv"])

    def get_recv(self):
        return self.u32(self.b["obj"] + 0x30)

    def recv_get(self, i):
        v = self.u32(self.b["recv"] + 4 * i)
        return self.lbl(v) if i == 1 else v

    def set_sys(self, present):
        self.w(SYS_GLOBAL, self.b["sys"] if present else 0)

    def get_sys(self):
        return self.lbl(self.u32(SYS_GLOBAL))

    def finish_world(self):
        pass

    def run_list(self, kind, op):
        o = self.b["obj"]
        if op == "dtor":
            self.call(FN_SRV if kind == 0 else FN_THEME, [o, 0])
        elif op == "getlist":
            self.call(FN_RIDE, [o])
        else:
            self.call(FN_LOGGED, [o])


class Host(Side):
    def __init__(self, lib, case, blocks):
        self.lib = lib
        bind(lib)
        self.b = {}
        for name, (size, label) in blocks.items():
            self.b[name] = 0
        self.blocks = blocks
        self.case = case

    def start(self, kind=None):
        lib = self.lib
        for name, (size, label) in self.blocks.items():
            if name in ("obj", "list") and kind is not None:
                continue
            if name.startswith("vt_"):
                continue
            self.b[name] = lib.tpw_sa2_alloc(size)
        if kind is not None:
            self.b["obj"] = lib.tpw_sa2_make(kind)
            self.b["list"] = lib.tpw_sa2_get_list(self.b["obj"])
        fixed = {blocks_label: self.b[n] for n, (size, blocks_label) in self.blocks.items() if blocks_label and n in self.b}
        for off, label in self.case.get("extra_labels", {}).items():
            fixed[label] = self.b[off[0]] + off[1]
        self.rev = {v: k for k, v in fixed.items()}
        self.fixed = fixed
        CL.host_begin(lib, fixed, self.case["queues"])

    def done(self):
        pass

    def log(self):
        return CL.host_end(self.lib)

    # list objects
    def make_list(self, kind):
        lib = self.lib
        for i, n in enumerate(("el1", "el2", "el3")):
            lib.tpw_sa2_set_elem(i, self.b[n])
        lib.tpw_sa2_set_city(self.b["city"])
        lib.tpw_sa2_set_inst(self.b["inst"])

    def set_field(self, off, v):
        self.lib.tpw_sa2_set_field(self.b["obj"], off, s32(v))

    def get_field(self, off):
        return self.lib.tpw_sa2_get_field(self.b["obj"], off) & MASK

    def set_count(self, n):
        self.lib.tpw_sa2_set_count(self.b["list"], s32(n))

    def get_count(self):
        return self.lib.tpw_sa2_get_count(self.b["list"])

    def set_recv(self, count, infos, f08, ref):
        b = self.b
        self.lib.tpw_sa2_recv_set(b["recv"], s32(count), b["infos"] if infos else 0, s32(f08), s32(ref))
        self.lib.tpw_sa2_set_recv(b["obj"], b["recv"])

    def get_recv(self):
        return self.lib.tpw_sa2_get_recv(self.b["obj"])

    def recv_get(self, i):
        v = self.lib.tpw_sa2_recv_get(self.b["recv"], i)
        return self.lbl(v) if i == 1 else v & MASK

    def set_sys(self, present):
        self.lib.tpw_sa2_set_sys(self.b["sys"] if present else 0)

    def get_sys(self):
        return self.lbl(self.lib.tpw_sa2_get_sys())

    def run_list(self, kind, op):
        self.lib.tpw_sa2_call(kind, {"dtor": 0, "getlist": 1, "logged": 2}[op], self.b["obj"])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, i32, u32 = C.c_uint64, C.c_int32, C.c_uint32
    sig = {
        "alloc": (u64, [u32]), "layout": (i32, [C.POINTER(C.c_int64)]),
        "str_set": (None, [u64, u64]), "str_get": (u64, [u64]), "rep_set": (None, [u64, i32, i32, i32, u64]),
        "rep_get": (u64, [u64, i32]), "set_alt": (None, [u64]), "assign": (u32, [u64, u64, u32]),
        "hg_make": (u64, []), "hg_info": (None, [u64, C.POINTER(C.c_int64)]), "hg_set_data": (None, [u64, u64]),
        "hg_get_data": (u64, [u64]), "hg_dtor": (None, [u64]),
        "make": (u64, [i32]), "set_field": (None, [u64, i32, i32]), "get_field": (i32, [u64, i32]), "get_list": (u64, [u64]),
        "set_count": (None, [u64, i32]), "get_count": (i32, [u64]), "set_recv": (None, [u64, u64]), "get_recv": (u64, [u64]),
        "recv_set": (None, [u64, i32, u64, i32, i32]), "recv_get": (u64, [u64, i32]), "set_elem": (None, [i32, u64]),
        "set_city": (None, [u64]), "set_inst": (None, [u64]), "set_sys": (None, [u64]), "get_sys": (u64, []),
        "call": (None, [i32, i32, u64]),
    }
    for name, (res, args) in sig.items():
        f = getattr(lib, "tpw_sa2_" + name)
        f.restype, f.argtypes = res, args
    out = (C.c_int64 * 32)()
    n = lib.tpw_sa2_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- common case parts


def rint(r):
    return r.choice([0, 1, -1, 2, 0x4E, 0x4F, r.randrange(0, 0x7FFFFFFF), -r.randrange(1, 0x7FFFFFFF)]) & MASK


def queues(r, **kw):
    q = {"swap": [0], "ChildLocked": [0], "RemoveHead": [0], "RequestListFromServer": [0], "AreWeLoggedIn": [0], "ReturnConn": [0]}
    q.update(kw)
    return q


# ---------------------------------------------------------------------------------------------------------------- assign

ASSIGN_BLOCKS = {"str": (8, L_STR), "rep": (0x20, L_REP), "alt": (0x20, L_ALT), "data": (0x40, L_DATA), "data2": (0x40, L_DATA2),
                 "src": (0x40, L_SRC)}


def build_assign(s, case):
    b = s.b
    if isinstance(s, Ppc):
        s.w(b["str"], b["rep"])
        s.w(b["rep"], case["len"], case["cap"], case["ref"], b["data"])
        s.w(b["alt"], case["len2"], case["cap2"], case["ref2"], b["data2"])
    else:
        lib = s.lib
        lib.tpw_sa2_str_set(b["str"], b["rep"])
        lib.tpw_sa2_rep_set(b["rep"], s32(case["len"]), s32(case["cap"]), s32(case["ref"]), b["data"])
        lib.tpw_sa2_rep_set(b["alt"], s32(case["len2"]), s32(case["cap2"]), s32(case["ref2"]), b["data2"])
        lib.tpw_sa2_set_alt(b["alt"])


def observe_assign(s, case):
    b = s.b
    d = {}
    if isinstance(s, Ppc):
        d["rep"] = s.lbl(s.u32(b["str"]))
        d["r"] = [s.u32(b["rep"] + 4 * i) for i in range(3)] + [s.lbl(s.u32(b["rep"] + 12))]
        d["a"] = [s.u32(b["alt"] + 4 * i) for i in range(3)] + [s.lbl(s.u32(b["alt"] + 12))]
    else:
        lib = s.lib
        d["rep"] = s.lbl(lib.tpw_sa2_str_get(b["str"]))
        d["r"] = [lib.tpw_sa2_rep_get(b["rep"], i) & MASK for i in range(3)] + [s.lbl(lib.tpw_sa2_rep_get(b["rep"], 3))]
        d["a"] = [lib.tpw_sa2_rep_get(b["alt"], i) & MASK for i in range(3)] + [s.lbl(lib.tpw_sa2_rep_get(b["alt"], 3))]
    return d


def run_assign(side, case):
    try:
        if isinstance(side, Host):
            side.start()
        build_assign(side, case)
        b = side.b
        if isinstance(side, Ppc):
            r3 = side.call(FN_ASSIGN, [b["str"], b["src"], case["n"]])
            answer = 1 if r3 == b["str"] else 0
        else:
            side.lib.tpw_sa2_assign(b["str"], b["src"], case["n"])
            answer = 1
        d = observe_assign(side, case)
        d["answer"] = answer
        d["log"] = side.log()
        return d
    finally:
        side.done()


def assign_case(r, n=None, swap=None):
    return {"n": r.choice([0, 1, 5, 100, 0x7FFFFFFF]) if n is None else n,
            "len": r.randrange(0, 50), "cap": r.randrange(0, 50), "ref": r.choice([0, 1, 2]),
            "len2": r.randrange(0, 50), "cap2": r.randrange(0, 50), "ref2": r.choice([0, 1, 2]),
            "queues": queues(r, swap=[r.choice([0, 1]) if swap is None else swap])}


def assign_edges():
    r = random.Random(101)
    for n in (0, 1, 7, 0xFFFFFFFF):
        for swap in (0, 1):
            yield assign_case(r, n, swap)


def assign_random(r):
    return assign_case(r)


# ---------------------------------------------------------------------------------------------------------------- the gradient

HG_BLOCKS = {"obj": (0x130, L_OBJ)}


def run_hg(side, case):
    try:
        if isinstance(side, Ppc):
            o = side.b["obj"]
            side.w(o, case["data"])
            side.call(FN_HG, [o, 0])
            data = side.u32(o)
        else:
            lib = side.lib
            o = lib.tpw_sa2_hg_make()
            side.b["obj"] = o
            info = (C.c_int64 * 4)()
            lib.tpw_sa2_hg_info(o, info)
            side.case = dict(case, extra_labels={})
            side.rev = {o: L_OBJ, o + info[0]: L_TEX, o + info[1]: L_MESH}
            CL.host_begin(lib, {L_OBJ: o, L_TEX: o + info[0], L_MESH: o + info[1]}, case["queues"])
            lib.tpw_sa2_hg_set_data(o, case["data"])
            lib.tpw_sa2_hg_dtor(o)
            data = lib.tpw_sa2_hg_get_data(o) & MASK
        return {"data": data, "log": side.log()}
    finally:
        side.done()


def hg_case(r, data=None):
    return {"data": r.choice([0, 1, 0x1234, r.randrange(1, 0x7FFFFFFF)]) if data is None else data, "queues": queues(r),
            "extra_labels": {("obj", 0xBC): L_TEX, ("obj", 0x6C): L_MESH}}


def hg_edges():
    r = random.Random(102)
    for d in (0, 1, 0x1234):
        yield hg_case(r, d)


def hg_random(r):
    return hg_case(r)


# ---------------------------------------------------------------------------------------------------------------- the lists

LIST_BLOCKS = {"obj": (0x40, L_OBJ), "vt_obj": (0x20, 0), "list": (0x10, L_LIST), "vt_list": (0x20, 0), "vt_elem": (0x20, 0),
               "recv": (0x10, L_RECV), "infos": (0x10, L_INFOS), "el1": (0x10, L_EL1), "el2": (0x10, L_EL2), "el3": (0x10, L_EL3),
               "city": (0x10, L_CITY), "inst": (0x10, L_INST), "sys": (0x10, L_SYS)}


def run_list(side, case):
    kind, op = case["kind"], case["op"]
    try:
        if isinstance(side, Host):
            side.start(kind)
        side.make_list(kind)
        for off, v in case["fields"].items():
            side.set_field(off, v)
        side.set_count(case["count"])
        if case["recv"] is not None:
            side.set_recv(*case["recv"])
        side.set_sys(case["sys"])
        side.run_list(kind, op)
        d = {"fields": [side.get_field(o) for o in range(4, 0x2C, 4)], "count": side.get_count(),
             "recv_ptr": side.lbl(side.get_recv()), "sys": side.get_sys()}
        if case["recv"] is not None:
            d["recv"] = [side.recv_get(i) for i in range(4)]
        d["log"] = side.log()
        return d
    finally:
        side.done()


def fields(r, **kw):
    f = {off: rint(r) for off in range(4, 0x2C, 4)}
    f.update(kw)
    return f


def recv_case(r, kind=None):
    k = r.choice([None, (3, 1, 7, 1), (3, 0, 7, 1), (5, 1, 0, 2), (0, 1, 0, 0), (2, 1, 9, 1)]) if kind is None else kind
    return k


def list_case(r, kind, op, **kw):
    c = {"kind": kind, "op": op, "fields": fields(r), "count": r.randrange(0, 4), "recv": recv_case(r), "sys": r.choice([0, 1]),
         "queues": queues(r, RemoveHead=[r.randrange(0, 4) for _ in range(3)], ChildLocked=[r.choice([0, 0, 1, 2])],
                          ReturnConn=[r.choice([0, 1])], AreWeLoggedIn=[r.choice([0, 1])],
                          RequestListFromServer=[r.choice([0, -1, 5, r.randrange(1, 0x7FFFFFFF)]) & MASK])}
    c.update(kw)
    return c


def dtor_edges(kind, seed):
    r = random.Random(seed)
    for recv in (None, (3, 1, 7, 1), (3, 0, 7, 1), (5, 1, 0, 2), (0, 1, 0, 0), (2, 1, 9, 1)):
        for count in (0, 1, 2, 3):
            for heads in ([1, 2, 3], [0, 0, 0], [3, 0, 2]):
                yield list_case(r, kind, "dtor", recv=recv, count=count, queues=queues(r, RemoveHead=heads))


def dtor_random(kind):
    return lambda r: list_case(r, kind, "dtor")


def ride_edges():
    r = random.Random(103)
    for recv in (None, (3, 1, 7, 1), (3, 0, 7, 1), (5, 1, 0, 2), (0, 1, 0, 0), (2, 1, 9, 1)):
        for sys in (0, 1):
            for logged in (0, 1):
                yield list_case(r, 2, "getlist", recv=recv, sys=sys, queues=queues(r, AreWeLoggedIn=[logged]))


def logged_edges():
    r = random.Random(104)
    for child in (0, 1, 2):
        for conn in (0, 1):
            for f08 in (0, 1):
                for cb in (0, CALLBACK_WORD):
                    for sys in (0, 1):
                        c = list_case(r, 1, "logged", sys=sys, recv=None,
                                      queues=queues(r, ChildLocked=[child], ReturnConn=[conn], RequestListFromServer=[0x1234]))
                        c["fields"][8] = f08
                        c["fields"][0x1C] = cb
                        yield c


def logged_random(r):
    c = list_case(r, 1, "logged", recv=None)
    c["fields"][8] = r.choice([0, 1, 5])
    c["fields"][0x1C] = r.choice([0, CALLBACK_WORD])
    c["fields"][0x20] = r.randrange(0, 0x7FFFFFFF)
    return c


# ---------------------------------------------------------------------------------------------------------------- entry points


def ppc(e, case):
    if "len" in case:
        return run_assign(Ppc(e, case, ASSIGN_BLOCKS), case)
    if "data" in case:
        return run_hg(Ppc(e, case, HG_BLOCKS), case)
    return run_list(Ppc(e, case, LIST_BLOCKS), case)


def host(lib, case):
    if "len" in case:
        return run_assign(Host(lib, case, ASSIGN_BLOCKS), case)
    if "data" in case:
        return run_hg(Host(lib, case, HG_BLOCKS), case)
    return run_list(Host(lib, case, LIST_BLOCKS), case)


TESTS = [
    FnTest("std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>::assign", ppc, host,
           edge=assign_edges, rnd=assign_random),
    FnTest("InterfaceDrawEngineHGradient::~InterfaceDrawEngineHGradient", ppc, host, edge=hg_edges, rnd=hg_random),
    FnTest("CFSOnlineSystemRideList::GetList", ppc, host, edge=ride_edges, rnd=lambda r: list_case(r, 2, "getlist")),
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlineServer>,ccServerList>::~CFSOnlineSystemList", ppc, host,
           edge=lambda: dtor_edges(0, 105), rnd=dtor_random(0)),
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>,ccThemeList>::LoggedInGetList", ppc, host,
           edge=logged_edges, rnd=logged_random),
    FnTest("CFSOnlineSystemThemeList::~CFSOnlineSystemThemeList", ppc, host, edge=lambda: dtor_edges(1, 106), rnd=dtor_random(1)),
]
