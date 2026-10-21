"""Unit v5713, CFSOnlineSystemServerList, against libtpw_unit_CFSOnlineSystemServerList_RequestListFromServer.dylib
(tests/unit_CFSOnlineSystemServerList_RequestListFromServer.cpp), main app, src/game/online/FS_OnlineList.cpp:
  GetList 0x1017ada8, RequestListFromServer 0x1017af40, StoreReceivedList 0x1017afec

A case is a world and a call, built twice by one piece of Python code (emulator memory big-endian at the Mac's offsets; host memory at
the host class's offsets, found through the library's layout export). The callees (UDebug, City::GetLogicalServerList, operator new and
delete, __destroy_new_array, the CFSOnlineSystem and CFSOnlineServer constructors, AreWeLoggedIn, Login, ServerListPostLoginCallbackFn)
are fakes on both sides that record their arguments; new records one event without its size (the host objects are larger than the
Mac's), the array destroy is compared as one dtor event per element, last to first.

The world: the list's ten words at 0x04..0x28, the stored answer (absent, or an answer with a count, reference count and an info array),
gOnlineSystem (NULL or a preset object), the answers of the fakes. Compared: the answer, all list words, the answer block, gOnlineSystem
(NULL, preset or new), and the event log. No fused operations. Not tested: a NULL city or list (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystemServerList_RequestListFromServer"
MASK = 0xFFFFFFFF

FN_GET, FN_REQ, FN_STORE = 0x1017ADA8, 0x1017AF40, 0x1017AFEC
SIZES = {FN_GET: 348, FN_REQ: 92, FN_STORE: 116}
OWN = set(SIZES)

UDEBUG, GLSL, NEW, DEL, DESTROY = 0x10000E40, 0x10239340, 0x102384B8, 0x10238218, 0x10238AD0
CTOR_SYS, CTOR_SRV, LOGGED, LOGIN, CALLBACK = 0x1016F7D0, 0x1017C7D0, 0x1017178C, 0x101710AC, 0x1017AA6C
CALLBACK_TV = 0x102675A8

ADDR = dict(gOnlineSystem=0x10380AB0)
# host offsets: mRequestID, mF28, mList, mReceived; ccServerList mCount, mInfos, mF08, mRefCount, sizeof; sizeof ccServerInfo
HOST_LAYOUT = [8, 44, 48, 56, 0, 8, 16, 20, 24, 38]

STUBS = CL.Stubs()
STUBS.real.update(OWN)

def glue(fn):
    """A glue import saves the caller's TOC pointer at 0x14(r1); the caller reloads it afterwards."""
    def wrapped(e):
        e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapped


STUBS.internal(UDEBUG, CL.recorder("UDebug", "s"))
STUBS.internal(GLSL, glue(CL.recorder("GetLogicalServerList", "Luiui", ret="i")))
STUBS.internal(DEL, glue(CL.recorder("delete", "L")))      # the host's operator delete records the same event
STUBS.internal(CTOR_SYS, CL.recorder("CFSOnlineSystem", "N"))
STUBS.internal(LOGGED, CL.recorder("AreWeLoggedIn", "L", ret="i"))
STUBS.internal(CALLBACK, CL.recorder("ServerListPostLogin", "iu"))
_rec_login = CL.recorder("Login", "L")
_rec_srv = CL.recorder("CFSOnlineServer", "NxL")


def stub_new(e):
    n = e.r(3) & MASK
    e.set_r(3, e.alloc(max(n, 4)))


def stub_login(e):
    _rec_login(e)
    e.cl.rec("LoginCb", (1 if e.r(4) == CALLBACK_TV else 0, e.r(5) & MASK))


def stub_srv(e):
    _rec_srv(e)
    e.cl.rec("serverinfo", ((((e.r(4) & MASK) - e.cl.base) & MASK) // 0x26,))


def stub_destroy(e):
    p = e.r(3) & MASK
    n = e.u32(p - 4)
    for i in reversed(range(n)):
        e.cl.rec("dtor", (i,))


STUBS.internal(NEW, glue(stub_new))
STUBS.internal(LOGIN, stub_login)
STUBS.internal(CTOR_SRV, stub_srv)
STUBS.internal(DESTROY, glue(stub_destroy))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def g(self, name):
        return ADDR[name]


class Ppc(Side):
    fmt = ">I"
    ptrfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.lst, self.sys, self.city, self.recv = (e.alloc(0x80) for _ in range(4))
        self.L = e.cl = CL.PpcLog({self.lst: 1, self.sys: 2, self.city: 3, self.recv: 4}, case["queues"])
        self.L.base = 0
        e.deleted = []
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def lf(self, m):
        return self.lst + m

    def rf(self, m):
        return self.recv + m

    def alloc(self, n):
        return self.e.alloc(n)

    def freed(self):
        return [1 if p == self.recv else 2 for p in self.e.deleted]

    def new_infos(self, n):
        a = self.e.alloc(8 + 0x26 * n)
        self.words(a + 4, [n])
        self.L.base = a + 8
        return a + 8

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, args):
        entry = {"get": FN_GET, "req": FN_REQ, "store": FN_STORE}[op]
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK if op == "req" else 0   # the other two answer nothing


class Host(Side):
    fmt = "<I"
    ptrfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.lst, self.sys, self.city = (lib.tpw_sl_alloc(0x100) for _ in range(3))
        self.recv = lib.tpw_sl_allocnew(0x100)
        CL.host_begin(lib, {1: self.lst, 2: self.sys, 3: self.city, 4: self.recv}, case["queues"])

    def done(self):
        pass

    def lf(self, m):
        if m == 0x2C:
            return self.lst + 48
        if m == 0x30:
            return self.lst + 56
        return self.lst + m + 4

    def rf(self, m):
        return self.recv + {0: 0, 4: 8, 8: 16, 0xC: 20}[m]

    def alloc(self, n):
        return self.lib.tpw_sl_alloc(n)

    def freed(self):
        return [1] if self.lib.tpw_sl_wasfreed(self.recv, 0x100) else []

    def new_infos(self, n):
        p = self.lib.tpw_sl_newinfos(n)
        self.lib.tpw_sl_setbase(p)
        return p

    def g(self, name):
        return self.lib.tpw_sl_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        lib = self.lib
        if op == "get":
            lib.tpw_sl_get(args[0])
            return 0
        if op == "req":
            return lib.tpw_sl_req(args[0], args[1], args[2] & MASK) & MASK
        lib.tpw_sl_store(args[0], args[1])
        return 0


def wptr(s, a, v):
    s.write(a, struct.pack(s.ptrfmt, v))


def rptr(s, a):
    n = struct.calcsize(s.ptrfmt)
    return struct.unpack(s.ptrfmt, s.read(a, n))[0]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for name, res, args in (
        ("addr", C.c_uint64, [C.c_char_p]), ("alloc", C.c_uint64, [C.c_uint32]), ("allocnew", C.c_uint64, [C.c_uint32]),("newinfos", C.c_uint64, [C.c_uint32]),
        ("setbase", None, [C.c_uint64]), ("req", C.c_uint32, [C.c_uint64, C.c_uint64, C.c_uint32]),
        ("store", None, [C.c_uint64, C.c_uint64]), ("get", None, [C.c_uint64]),
    ):
        f = getattr(lib, "tpw_sl_" + name)
        f.restype, f.argtypes = res, args
    lib.tpw_sl_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    for i, v in enumerate(case["ints"]):
        s.words(s.lf(4 + 4 * i), [v])
    rc = case["recv"]
    if rc is None:
        wptr(s, s.lf(0x30), 0)
    else:
        wptr(s, s.lf(0x30), s.recv)
        s.words(s.rf(0), [rc["count"]])
        s.words(s.rf(8), [rc["f08"]])
        s.words(s.rf(0xC), [rc["ref"]])
        wptr(s, s.rf(4), s.new_infos(rc["n"]) if rc["n"] > 0 else 0)
    wptr(s, s.g("gOnlineSystem"), s.sys if case["sys"] else 0)


def execute(s, case):
    op = case["op"]
    if op == "req":
        return s.call("req", [s.lst, s.city, case["arg"]])
    if op == "store":
        return s.call("store", [s.lst, s.recv])
    return s.call("get", [s.lst])


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log()}
    out["ints"] = [s.rd_word(s.lf(4 + 4 * i)) for i in range(10)]
    r = rptr(s, s.lf(0x30))
    out["received"] = 0 if r == 0 else (1 if r == s.recv else 2)
    freed = case["op"] == "get" and case["recv"] is not None and case["recv"]["ref"] == 1   # the log shows the delete; its bytes are the allocator's
    if case["recv"] is not None and not freed:
        out["rc"] = [s.rd_word(s.rf(0)), 1 if rptr(s, s.rf(4)) else 0, s.rd_word(s.rf(8)), s.rd_word(s.rf(0xC))]
    g = rptr(s, s.g("gOnlineSystem"))
    out["sys"] = 0 if g == 0 else (1 if g == s.sys else 2)
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


def rw(r):
    return r.choice([0, 1, MASK, 0x80000000, r.randrange(1 << 32)])


def mk(r, op, recv="rand", sys=None, logged=None, n=None, ref=None, arg=None):
    if recv == "rand":
        recv = r.choice([None, "x"])
    rc = None
    if recv is not None:
        nn = r.randrange(0, 5) if n is None else n
        rc = {"count": nn if op == "store" else rw(r), "n": nn, "f08": rw(r),
              "ref": r.choice([0, 1, 1, 2, 3, MASK]) if ref is None else ref}
    return {"op": op, "ints": [rw(r) for _ in range(10)], "recv": rc,
            "sys": r.choice([0, 1]) if sys is None else sys,
            "arg": rw(r) if arg is None else arg,
            "queues": {"AreWeLoggedIn": [r.choice([0, 1]) if logged is None else logged],
                       "GetLogicalServerList": [r.choice([0, 1, 7, MASK, rw(r)])]}}


def req_edges():
    r = random.Random(11)
    for a in (0, 1, MASK, 0x12345678):
        yield mk(r, "req", arg=a, recv=None)


def store_edges():
    r = random.Random(12)
    for n in (0, 1, 2, 5):
        yield mk(r, "store", recv="x", n=n)


def get_edges():
    r = random.Random(13)
    for recv in (None, "x"):
        for sys in (0, 1):
            for logged in (0, 1):
                for ref in (0, 1, 2):
                    for n in (0, 1, 3):
                        yield mk(r, "get", recv=recv, sys=sys, logged=logged, ref=ref, n=n)


TESTS = [
    FnTest("CFSOnlineSystemServerList::RequestListFromServer", ppc, host, edge=req_edges, rnd=lambda r: mk(r, "req")),
    FnTest("CFSOnlineSystemServerList::StoreReceivedList", ppc, host, edge=store_edges, rnd=lambda r: mk(r, "store", recv="x")),
    FnTest("CFSOnlineSystemServerList::GetList", ppc, host, edge=get_edges, rnd=lambda r: mk(r, "get")),
]
