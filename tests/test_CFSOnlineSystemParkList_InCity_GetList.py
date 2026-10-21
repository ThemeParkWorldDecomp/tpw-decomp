"""Unit v5244, CFSOnlineSystemParkList_InCity::GetList, against libtpw_unit_CFSOnlineSystemParkList_InCity_GetList.dylib
(tests/unit_CFSOnlineSystemParkList_InCity_GetList.cpp), main app ("SimTheme Park"),
src/game/online/FS_OnlineList.cpp:
  CFSOnlineSystemParkList_InCity::GetList 0x10178f54

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory (big-endian) for the original, and in
the host library's memory (little-endian) for the translation. The callees outside the unit are fakes on both sides that record their
arguments: operator new / delete / the array destroy (one park_dtor event per element, then delete_array), the online system's constructor,
AreWeLoggedIn, Login (the callback as "is it the post-login function") and PostLoginCallbackFn_Park.

The world of a case: the list object (every word 0x04..0x28 and 0x34 random), the stored answer (none, or a block with a reference count
and an infos array of n park infos), gOnlineSystem (set or null), the answer of AreWeLoggedIn. What is compared: every word of the object, the
stored answer's reference count and infos pointer, gOnlineSystem (null, the preset or a new object), the event log.

No fused operations. Not tested: the second gOnlineSystem null test after the first (unreachable: the first one always fills it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystemParkList_InCity_GetList"
MASK = 0xFFFFFFFF

FN = 0x10178F54
SIZES = {FN: 352}
OWN = set(SIZES)

CTOR, LOGGED, LOGIN, POST = 0x1016F7D0, 0x1017178C, 0x101710AC, 0x10178C74
NEW, DELETE, DESTROY = 0x102384B8, 0x10238218, 0x10238AD0
POST_TVEC = 0x10267628

ADDR = dict(gos=0x10380AB0)
# the Mac words of the object, in the order of the host layout export
MAC_FIELDS = [0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x30, 0x34]
# host: those offsets, the object size, the received list's infos and refcount offsets and size
HOST_LAYOUT = [8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 56, 64, 72, 8, 20, 24]
PPC_R = dict(count=0, infos=4, f08=8, ref=0xC)
HOST_R = dict(count=0, infos=8, f08=16, ref=20)


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def toc(e):
    """The glue stores r2 at 0x14(r1) and the caller reloads it; the fakes do the same."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def stub_new(e):
    # The host's operator new is the shim's and logs nothing, so the size is checked here instead of logged.
    size = e.r(3) & MASK
    assert size == 0x404, size
    toc(e)
    e.set_r(3, e.alloc(size))


def stub_delete(e):
    # (not logged: the host's operator delete is the shim's; the freed block's effect shows as the cleared infos pointer)
    toc(e)


def stub_destroy(e):
    p = e.r(3) & MASK
    n = e.u32(p - 4)
    for _ in range(n):
        e.cl.rec("park_dtor", ())
    toc(e)


def stub_ctor(e):
    e.cl.rec("ctor", ())


def stub_logged(e):
    L = e.cl
    L.rec("AreWeLoggedIn", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("AreWeLoggedIn") & MASK)


def stub_login(e):
    L = e.cl
    L.rec("Login", (L.lab(e.r(3)), 1 if (e.r(4) & MASK) == POST_TVEC else 0, L.lab(e.r(5))))


def stub_post(e):
    L = e.cl
    L.rec("Post", (s32(e.r(3)), L.lab(e.r(4))))


STUBS.internal(NEW, stub_new)
STUBS.internal(DELETE, stub_delete)
STUBS.internal(DESTROY, stub_destroy)
STUBS.internal(CTOR, stub_ctor)
STUBS.internal(LOGGED, stub_logged)
STUBS.internal(LOGIN, stub_login)
STUBS.internal(POST, stub_post)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def fld(self, mac):
        return self.obj + self.off[mac]

    def cookie(self, p, n):
        raise NotImplementedError


class Ppc(Side):
    fmt = ">I"
    R = PPC_R
    off = {m: m for m in MAC_FIELDS}

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(0x40)
        self.gos = e.alloc(0x404)
        self.L = e.cl = CL.PpcLog({self.obj: 1, self.gos: 2}, case["queues"])
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

    def setptr(self, a, v):
        self.words(a, [v])

    def getptr(self, a):
        return self.rd_word(a)

    def new_rcv(self):
        return self.e.alloc(0x20)

    def new_infos(self, n):
        return self.e.alloc(32 + 0x66 * n) + 16

    def cookie(self, p, n):
        self.words(p - 4, [n])

    def log(self):
        return self.L.log

    def call(self, city, cb, ctx):
        T.call_ppc(self.e, FN, [self.obj, city & MASK, cb & MASK, ctx & MASK])


class Host(Side):
    fmt = "<I"
    R = HOST_R

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = lib.off
        self.obj = lib.tpw_ilgl_alloc(80)
        self.gos = lib.tpw_ilgl_alloc(0x404)
        self.rcv_block = lib.tpw_ilgl_new(0x20)
        self.infos_block = lib.tpw_ilgl_new_array(16 + 0x66 * 3)
        CL.host_begin(lib, {1: self.obj, 2: self.gos}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ilgl_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ilgl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def getptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def new_rcv(self):
        return self.rcv_block

    def new_infos(self, n):
        return self.infos_block + 16     # the ARM64 array cookie is two words: element size, count

    def cookie(self, p, n):
        self.write(p - 8, struct.pack("<Q", n))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, city, cb, ctx):
        self.lib.tpw_ilgl_getlist(self.obj, s32(city), cb & MASK, ctx & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ilgl_"
    lib.tpw_ilgl_addr.restype, lib.tpw_ilgl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ilgl_alloc.restype, lib.tpw_ilgl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    for nm in ("tpw_ilgl_new", "tpw_ilgl_new_array"):
        getattr(lib, nm).restype, getattr(lib, nm).argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ilgl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ilgl_getlist.restype = None
    lib.tpw_ilgl_getlist.argtypes = [C.c_uint64, C.c_int32, C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ilgl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.off = {m: HOST_LAYOUT[i] for i, m in enumerate(MAC_FIELDS)}
    lib.bound = True


def build(s, case):
    for m, w in zip(MAC_FIELDS[:10], case["init"]):
        s.words(s.fld(m), [w])
    s.words(s.fld(0x34), [case["init34"]])
    s.rcv = None
    recv = case["recv"]
    if recv is None:
        s.setptr(s.fld(0x30), 0)
    else:
        R = s.R
        s.rcv = s.new_rcv()
        s.setptr(s.fld(0x30), s.rcv)
        s.words(s.rcv + R["count"], [recv["n"]])
        s.words(s.rcv + R["f08"], [recv["f08"]])
        s.words(s.rcv + R["ref"], [recv["ref"]])
        if recv["n"]:
            p = s.new_infos(recv["n"])
            s.cookie(p, recv["n"])
            s.setptr(s.rcv + R["infos"], p)
        else:
            s.setptr(s.rcv + R["infos"], 0)
    s.setptr(s.g("gos"), s.gos if case["gos"] else 0)


def observe(s, case):
    d = {"log": s.log()}
    for m in MAC_FIELDS[:10]:
        d["w%02x" % m] = s.rd_word(s.fld(m))
    d["w34"] = s.rd_word(s.fld(0x34))
    d["received"] = 1 if s.getptr(s.fld(0x30)) else 0
    if s.rcv is not None and case["recv"]["ref"] != 1:      # (a block that reached zero is freed: the host really frees it)
        d["ref"] = s.rd_word(s.rcv + s.R["ref"])
        d["count"] = s.rd_word(s.rcv + s.R["count"])
        d["infos"] = 1 if s.getptr(s.rcv + s.R["infos"]) else 0
    g = s.getptr(s.g("gos"))
    d["gos"] = 0 if g == 0 else (1 if g == s.gos else 2)
    return d


def run(s, case):
    try:
        build(s, case)
        s.call(*case["args"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rword(r):
    return r.choice([0, 1, 0xFFFFFFFF, 0x80000000, r.getrandbits(32), r.getrandbits(32)])


def mk(r, logged, gos, recv, args=None):
    return {
        "queues": {"AreWeLoggedIn": [logged]},
        "init": [rword(r) for _ in range(10)],
        "init34": rword(r),
        "gos": gos,
        "recv": recv,
        "args": args if args is not None else [rword(r), rword(r), rword(r)],
    }


def rrecv(r):
    k = r.random()
    if k < 0.3:
        return None
    return {"n": r.choice([0, 1, 2, 3]), "f08": rword(r), "ref": r.choice([1, 1, 2, 3, 0, -1, 0x7FFFFFFF, 0x80000000, r.randrange(-5, 6)]) & MASK}


def edges():
    r = random.Random(5244)
    recvs = [None] + [{"n": n, "f08": 7, "ref": ref} for n in (0, 1, 3) for ref in (1, 2, 0, 0xFFFFFFFF, 0x80000001)]
    for logged in (0, 1, -1, 0x100):
        for gos in (False, True):
            for recv in recvs:
                yield mk(r, logged, gos, recv)
    yield mk(r, 0, False, None, [0, 0, 0])
    yield mk(r, 1, True, None, [0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF])


def rnd(r):
    return mk(r, r.choice([0, 1, -1, 5]), r.random() < 0.5, rrecv(r))


TESTS = [
    FnTest("CFSOnlineSystemParkList_InCity::GetList", ppc, host, edge=edges, rnd=rnd),
]
