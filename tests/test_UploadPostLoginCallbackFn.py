"""Unit v5961, seven small members of the online system, against libtpw_unit_UploadPostLoginCallbackFn.dylib
(tests/unit_UploadPostLoginCallbackFn.cpp), main app ("SimTheme Park"), src/game/online/FS_Online.cpp, src/game/online/FS_OnlineList.cpp:
  UploadPostLoginCallbackFn 0x10174f2c, GotoPlayerFTPPostGetParkInfoCallbackFn 0x1017620c, CFSOnlineSystem::ReturnConnectionToCityServer
  0x1017346c, ccThemeInfo::~ccThemeInfo 0x101732e4, ccRideInfo::~ccRideInfo 0x10173110, CFSOnlineFTPControl::CallbackFn 0x1017769c,
  CFSOnlineSystemCityList::GetList 0x10178718

A case is a world and a call. The world is built twice by ONE piece of Python code (Side methods hide the Mac's offsets and the host's
layout): in emulator memory at the Mac's addresses for the original, in the host library's memory for the translation. The callees outside
the unit (operator new and delete, free, __destroy_new_array, the online system's constructor and members, City's constructor, Connect and
SetSessionInfo, LbTime_GetClock, GetLanguageId, UDebug, the post login callback) are fakes on both sides that record their arguments
(texts as a hash, objects as labels); their answers come from per case queues.

The world of a case: gOnlineSystem (null or an object), the online system's words, the config's city server text and port, the city list
words and its stored answer, the info structures with their blocks. What is compared: the answer, the globals, the words read back, and
the event log.

No fused operations. Not tested: a NULL list in GotoPlayerFTPPostGetParkInfoCallbackFn and an empty one (the original reads the head
unconditionally); the array destruction runs no element destructor (the arrays hold no element: the Mac side is a fake and the host's
element destructor is a stand-in).
"""
import ctypes as C
import itertools
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UploadPostLoginCallbackFn"
MASK = 0xFFFFFFFF

F_UPLOAD, F_GOTO, F_CONN, F_THEME, F_RIDE, F_FTP, F_GETLIST = (0x10174f2c, 0x1017620c, 0x1017346c, 0x101732e4, 0x10173110, 0x1017769c, 0x10178718)
SIZES = {F_UPLOAD: 136, F_GOTO: 124, F_CONN: 284, F_THEME: 164, F_RIDE: 164, F_FTP: 132, F_GETLIST: 352}
OWN = set(SIZES)

# in-binary callees (faked) and glue imports (faked; the glue's r2 save is done by the wrapper)
UDEBUG, RELEASE, CTOR_SYS, ARE_LOGGED, REQ_UPLOAD, GOTO_CB, FTP_CB, LANG, POSTCITY, LOGIN = (
    0x10000e40, 0x10174ac8, 0x1016f7d0, 0x1017178c, 0x10175154, 0x101762e8, 0x101771a4, 0x1015e9d8, 0x101783a4, 0x101710ac)
G_NEW, G_FREE, G_DELETE, G_DESTROY, G_CITY_CTOR, G_CONNECT, G_SETSESS, G_CLOCK = (
    0x102384b8, 0x10238a70, 0x10238218, 0x10238ad0, 0x1023a678, 0x1023a690, 0x1023a6a8, 0x10238a88)
# the values the TOC slots hold
TOC_CITY_CB, TOC_POSTCITY, TOC_DTOR, GTEXT = 0x10267480, 0x10267658, 0x102674f0, 0x1037ff18
ADDR = dict(gsys=0x10380ab0, cfg=0x102b2b70)
HOST_LAYOUT = [16, 0x34, 4, 0xc, 0x24]

# the pool of blocks every case allocates first (labels 1..): indexes
P_SYS, P_CITY, P_RCV, P_INFOS, P_PARK, P_LIST, P_PW, P_A, P_B, P_C, P_NODE = range(11)
POOL = [0x1800, 0x100, 0x100, 0x800, 0x100, 0x400, 0x100, 0x100, 0x100, 0x100, 0x100]

SYS_OFF = [0xd4, 0xd8, 0xdc, 0x28, 0x34, 0x38]      # mCity, mCityClock, mCityListsWanted, password data, mF34, mF38
SYS_PTR = (0, 3)


def fnv(text):
    if isinstance(text, str):
        text = text.encode("latin1")
    h = 2166136261
    for c in text:
        h = ((h ^ c) * 16777619) & MASK
    return h


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def glue(fn):
    """A glue import: the glue saves r2 where the caller reloads it from."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapped


def rec(e, name, *vals):
    e.cl.rec(name, tuple(v & MASK for v in vals))


def lab(e, a):
    return e.cl.lab(a & MASK)


def ret(e, key):
    e.set_r(3, e.cl.ret(key) & MASK)


def s_udebug(e):
    rec(e, "UDebug", fnv(e.cstr(e.r(3))))


def s_release(e):
    rec(e, "ReleaseCity", lab(e, e.r(3)))


NEWS = {}   # address -> size of the blocks operator new made in the case (the host's operator new is the real one: not logged)


def s_new(e):
    n = e.r(3)
    a = e.alloc(n if n else 4, bytes(n if n else 4))
    NEWS[a] = n
    e.set_r(3, a)


def s_free(e):
    rec(e, "free", lab(e, e.r(3)))


def s_delete(e):
    rec(e, "delete", lab(e, e.r(3)))


def s_destroy(e):
    # one element: the host's array holds one, and its destructor fake records the same event
    rec(e, "destroy_array", 1 if e.r(4) == TOC_DTOR else 0)


def s_logged(e):
    rec(e, "AreWeLoggedIn", lab(e, e.r(3)))
    ret(e, "AreWeLoggedIn")


def s_req(e):
    rec(e, "RequestUploadPostLoginCallback", lab(e, e.r(3)), e.r(4), e.r(5))


def s_goto(e):
    rec(e, "GotoCallback", lab(e, e.r(3)), e.r(4), e.r(5), lab(e, e.r(6)))


def s_ftp(e):
    rec(e, "FTPDownloadCallbackFn", lab(e, e.r(3)), e.r(4), e.r(5))


def s_login(e):
    rec(e, "Login", lab(e, e.r(3)), 1 if e.r(4) == TOC_POSTCITY else 0, lab(e, e.r(5)))


def s_postcity(e):
    rec(e, "PostLoginCallbackFn_City", e.r(3), lab(e, e.r(4)))


def s_connect(e):
    rec(e, "Connect", lab(e, e.r(3)), fnv(e.cstr(e.r(4))), e.r(5), e.r(6), 1 if e.r(7) == TOC_CITY_CB else 0)
    ret(e, "Connect")


def s_setsess(e):
    rec(e, "SetSessionInfo", lab(e, e.r(3)), e.r(4), e.r(5), e.r(6), lab(e, e.r(7)))


def s_clock(e):
    rec(e, "LbTime_GetClock", 0)
    ret(e, "LbTime_GetClock")


def s_lang(e):
    rec(e, "GetLanguageId", 1 if e.r(3) == GTEXT else 0)
    ret(e, "GetLanguageId")


STUBS.internal(UDEBUG, s_udebug)
STUBS.internal(RELEASE, s_release)
STUBS.internal(CTOR_SYS, CL.recorder("ctor_sys", "N"))
STUBS.internal(ARE_LOGGED, s_logged)
STUBS.internal(REQ_UPLOAD, s_req)
STUBS.internal(GOTO_CB, s_goto)
STUBS.internal(FTP_CB, s_ftp)
STUBS.internal(LANG, s_lang)
STUBS.internal(POSTCITY, s_postcity)
STUBS.internal(LOGIN, s_login)
STUBS.internal(G_NEW, glue(s_new))
STUBS.internal(G_FREE, glue(s_free))
STUBS.internal(G_DELETE, glue(s_delete))
STUBS.internal(G_DESTROY, glue(s_destroy))
STUBS.internal(G_CITY_CTOR, glue(CL.recorder("ctor_city", "N")))
STUBS.internal(G_CONNECT, glue(s_connect))
STUBS.internal(G_SETSESS, glue(s_setsess))
STUBS.internal(G_CLOCK, glue(s_clock))


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def p(self, i):
        return self.pool[i]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.pool = [e.alloc(sz, bytes(sz)) for sz in POOL]
        NEWS.clear()
        NEWS.update({a: sz for a, sz in zip(self.pool, POOL)})
        self.L = e.cl = CL.PpcLog({a: i + 1 for i, a in enumerate(self.pool)}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def ptr32(self, a):
        return a

    def set_gsys(self, v):
        self.words(ADDR["gsys"], [v])

    def get_gsys(self):
        return self.L.lab(self.rd_word(ADDR["gsys"]))

    def gsize(self):
        return 1 if NEWS.get(self.rd_word(ADDR["gsys"]), 0) == 0x404 else 0

    def set_cfg(self, text, port):
        rep = self.e.alloc(0x40, bytes(0x40))
        buf = self.e.alloc(0x80, bytes(0x80))
        self.write(buf, text.encode() + b"\0")
        self.words(rep + 0xc, [buf])
        self.words(ADDR["cfg"] + 0xc, [rep])
        self.write(ADDR["cfg"] + 0x1e, struct.pack(">H", port & 0xFFFF))

    def sys_set(self, i, v):
        self.words(self.p(P_SYS) + SYS_OFF[i], [v])

    def sys_get(self, i):
        w = self.rd_word(self.p(P_SYS) + SYS_OFF[i])
        return self.L.lab(w) if i in SYS_PTR else w

    def list_set(self, i, v):
        self.words(self.p(P_LIST) + (4 + 4 * i if i < 10 else 0x30), [v])

    def list_get(self, i):
        w = self.rd_word(self.p(P_LIST) + (4 + 4 * i if i < 10 else 0x30))
        return self.L.lab(w) if i == 10 else w

    def rcv_set(self, i, v):
        self.words(self.p(P_RCV) + (4 if i == 0 else 0xc), [v])

    def rcv_get(self, i):
        w = self.rd_word(self.p(P_RCV) + (4 if i == 0 else 0xc))
        return 1 if i == 0 and w else w

    def mklist(self, park):
        self.words(self.p(P_LIST) + 4, [self.p(P_NODE)])
        self.words(self.p(P_NODE), [park])

    def call(self, fn, args):
        return T.call_ppc(self.e, fn, [a & MASK for a in args])[0] & MASK

    def conn(self):
        return self.L.lab(self.call(F_CONN, [self.p(P_SYS)]))

    def getlist(self, cb, ctx):
        self.call(F_GETLIST, [self.p(P_LIST), cb, ctx])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.pool = [lib.tpw_u_alloc(sz) for sz in POOL]
        CL.host_begin(lib, {i + 1: a for i, a in enumerate(self.pool)}, case["queues"])

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptr32(self, a):
        return self.lib.tpw_u_off(a) if a else 0

    def set_gsys(self, v):
        self.lib.tpw_u_gset(v)

    def get_gsys(self):
        return self.lib.tpw_u_gget()

    def gsize(self):
        return self.lib.tpw_u_gsize()

    def set_cfg(self, text, port):
        self.lib.tpw_u_cfg(text.encode(), T.s16(port))

    def sys_set(self, i, v):
        self.lib.tpw_u_sys_set(self.p(P_SYS), i, v)

    def sys_get(self, i):
        return self.lib.tpw_u_sys_get(self.p(P_SYS), i) & (MASK if i not in SYS_PTR else (1 << 64) - 1)

    def list_set(self, i, v):
        self.lib.tpw_u_list_set(self.p(P_LIST), i, v)

    def list_get(self, i):
        return self.lib.tpw_u_list_get(self.p(P_LIST), i)

    def rcv_set(self, i, v):
        self.lib.tpw_u_rcv_set(self.p(P_RCV), i, v)

    def rcv_get(self, i):
        return self.lib.tpw_u_rcv_get(self.p(P_RCV), i)

    def mklist(self, park):
        self.lib.tpw_u_mklist(self.p(P_LIST), self.p(P_NODE), park)

    def call(self, fn, args):
        raise NotImplementedError

    def conn(self):
        return self.lib.tpw_u_conn(self.p(P_SYS))

    def getlist(self, cb, ctx):
        self.lib.tpw_u_getlist(self.p(P_LIST), cb, ctx)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, i32, u32, i64 = C.c_uint64, C.c_int32, C.c_uint32, C.c_int64
    sigs = {
        "gsize": (i64, []), "alloc": (u64, [u32]), "off": (u32, [u64]), "cfg": (None, [C.c_char_p, i32]), "gset": (None, [u64]), "gget": (i64, []),
        "sys_set": (None, [u64, i32, u64]), "sys_get": (i64, [u64, i32]), "list_set": (None, [u64, i32, u64]),
        "list_get": (i64, [u64, i32]), "rcv_set": (None, [u64, i32, u64]), "rcv_get": (i64, [u64, i32]),
        "mklist": (None, [u64, u64, u64]), "upload": (None, [i32, u64]), "goto": (None, [i32, u64, u64]),
        "ftp": (None, [u64, i32, i32]), "conn": (i64, [u64]), "theme": (u64, [u64, i32]), "ride": (u64, [u64, i32]),
        "getlist": (None, [u64, u64, u64]),
    }
    for name, (res, args) in sigs.items():
        f = getattr(lib, "tpw_u_" + name)
        f.restype, f.argtypes = res, args
    lib.tpw_u_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_u_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def sys_pointer(s, case):
    return 0 if case["gnull"] else s.p(P_SYS)


def build(s, case):
    op = case["op"]
    s.set_gsys(sys_pointer(s, case))
    if op == "conn":
        s.set_cfg(case["server"], case["port"])
        s.sys_set(0, s.p(P_CITY) if case["city"] else 0)
        s.sys_set(1, case["clock0"])
        s.sys_set(2, case["wanted0"])
        s.sys_set(3, s.p(P_PW) if case["pw"] else 0)
        s.sys_set(4, case["f34"])
        s.sys_set(5, case["f38"])
        if not case["gnull"]:
            pass
    elif op == "goto":
        s.mklist(s.p(P_PARK) if case["park"] else 0)
    elif op in ("theme", "ride"):
        n = 4 if op == "theme" else 13
        ptrs = (1, 2, 3) if op == "theme" else (1, 2, 9)
        ws = list(case["words"][:n])
        blocks = [s.p(P_A), s.p(P_B), s.p(P_C)]
        for k, i in enumerate(ptrs):
            ws[i] = s.ptr32(blocks[k]) if case["blocks"][k] else 0
        s.words(s.p(P_SYS), ws)
    elif op == "getlist":
        for i in range(10):
            s.list_set(i, case["lwords"][i])
        s.list_set(10, s.p(P_RCV) if case["rcv"] else 0)
        s.rcv_set(0, s.p(P_INFOS) if case["infos"] else 0)
        s.rcv_set(1, case["refs"])


def execute(s, case):
    op = case["op"]
    if op == "upload":
        if isinstance(s, Ppc):
            s.call(F_UPLOAD, [case["err"], case["ctx"]])
        else:
            s.lib.tpw_u_upload(T.s32(case["err"]), case["ctx"])
        return 0
    if op == "goto":
        if isinstance(s, Ppc):
            s.call(F_GOTO, [case["err"], case["ctx"], s.p(P_LIST)])
        else:
            s.lib.tpw_u_goto(T.s32(case["err"]), case["ctx"], s.p(P_LIST))
        return 0
    if op == "ftp":
        if isinstance(s, Ppc):
            s.call(F_FTP, [case["self"], case["a"], case["b"]])
        else:
            s.lib.tpw_u_ftp(case["self"], T.s32(case["a"]), T.s32(case["b"]))
        return 0
    if op == "conn":
        return s.conn()
    if op in ("theme", "ride"):
        obj = 0 if case["null"] else s.p(P_SYS)
        if isinstance(s, Ppc):
            r = s.call(F_THEME if op == "theme" else F_RIDE, [obj, case["flag"]])
        else:
            r = (s.lib.tpw_u_theme if op == "theme" else s.lib.tpw_u_ride)(obj, T.s32(case["flag"]))
        return 1 if r == obj else 0
    if op == "getlist":
        s.getlist(case["cb"], case["ctx"])
        return 0


def observe(s, case, answer):
    op = case["op"]
    out = {"answer": answer, "log": s.log(), "gsys": s.get_gsys(), "gsize": s.gsize()}
    if op == "conn":
        out["sys"] = [s.sys_get(i) for i in range(6)]
    elif op in ("theme", "ride"):
        n = 4 if op == "theme" else 13
        ptrs = (1, 2, 3) if op == "theme" else (1, 2, 9)
        ws = s.rd_words(s.p(P_SYS), n)
        out["words"] = [(1 if w else 0) if i in ptrs else w for i, w in enumerate(ws)]
    elif op == "getlist":
        out["list"] = [s.list_get(i) for i in range(11)]
        out["rcv"] = [s.rcv_get(0), s.rcv_get(1)]
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


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL32 = [0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x10000, 0xFFFF, 0x8000]


def r32(r):
    return r.choice(SPECIAL32) if r.random() < 0.4 else r.getrandbits(32)


def base(r, op, **kw):
    c = {
        "op": op, "gnull": r.random() < 0.5,
        "queues": {
            "AreWeLoggedIn": [r.choice([0, 1, 2, 0xFFFFFFFF, 0x10000])],
            "Connect": [r.choice([0, 1, 1, 2, 0xFFFFFFFF, 0x100])],
            "LbTime_GetClock": [r32(r)],
            "GetLanguageId": [r32(r)],
        },
    }
    c.update(kw)
    return c


def text(r):
    return "".join(r.choice("abcdefghijklmnopqrstuvwxyz0123456789.-") for _ in range(r.randrange(0, 24)))


# ---- UploadPostLoginCallbackFn
def upload_case(r, gnull=None):
    c = base(r, "upload", err=r32(r), ctx=r32(r))
    if gnull is not None:
        c["gnull"] = gnull
    return c


def upload_edges():
    r = random.Random(11)
    for g in (True, False):
        for _ in range(3):
            yield upload_case(r, g)


# ---- GotoPlayerFTPPostGetParkInfoCallbackFn
def goto_case(r, gnull=None, park=None):
    c = base(r, "goto", err=r32(r), ctx=r32(r), park=r.random() < 0.7 if park is None else park)
    if gnull is not None:
        c["gnull"] = gnull
    return c


def goto_edges():
    r = random.Random(12)
    for g, p in itertools.product((True, False), (True, False)):
        yield goto_case(r, g, p)


# ---- CFSOnlineFTPControl::CallbackFn
def ftp_case(r, gnull=None):
    c = base(r, "ftp", a=r32(r), b=r32(r), self=r.choice([0, r32(r)]))
    if gnull is not None:
        c["gnull"] = gnull
    return c


def ftp_edges():
    r = random.Random(13)
    for g in (True, False):
        for _ in range(3):
            yield ftp_case(r, g)


# ---- CFSOnlineSystem::ReturnConnectionToCityServer
def conn_case(r, city=None, connect=None, pw=None):
    c = base(r, "conn", city=r.random() < 0.3 if city is None else city, clock0=r32(r), wanted0=r.choice([0, 1, 5, r32(r)]),
             f34=r32(r), f38=r32(r), pw=r.random() < 0.7 if pw is None else pw, server=text(r),
             port=r.choice([0, 1, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16)]))
    c["gnull"] = False
    if connect is not None:
        c["queues"]["Connect"] = [connect]
    return c


def conn_edges():
    r = random.Random(14)
    for city, connect, pw in itertools.product((True, False), (0, 1, 2, 0xFFFFFFFF), (True, False)):
        yield conn_case(r, city, connect, pw)


# ---- the destructors
def dtor_case(op):
    def make(r, null=None, blocks=None, flag=None):
        n = 4 if op == "theme" else 13
        c = base(r, op, null=r.random() < 0.1 if null is None else null, words=[r32(r) for _ in range(n)],
                 blocks=[r.random() < 0.7 for _ in range(3)] if blocks is None else blocks,
                 flag=r.choice([0, 1, 0xFFFF, 0x8000, 0x7FFF, 0x10000, 0x10001, 0xFFFF0000, 2, r32(r)]) if flag is None else flag)
        return c
    return make


def dtor_edges(op, seed):
    make = dtor_case(op)

    def edges():
        r = random.Random(seed)
        for blocks in itertools.product((True, False), repeat=3):
            for flag in (0, 1, 0xFFFF, 0x8000, 0x7FFF, 0x10000, 0x10001, 0xFFFFFFFF):
                yield make(r, False, list(blocks), flag)
        for flag in (0, 1, 0xFFFF):
            yield make(r, True, [True, True, True], flag)
    return edges


# ---- CFSOnlineSystemCityList::GetList
def getlist_case(r, logged=None, rcv=None, refs=None, infos=None, gnull=None):
    c = base(r, "getlist", lwords=[r32(r) for _ in range(10)], rcv=r.random() < 0.7 if rcv is None else rcv,
             infos=r.random() < 0.6 if infos is None else infos,
             refs=r.choice([1, 2, 0, 5, 0xFFFFFFFF, 0x80000000, r32(r)]) if refs is None else refs, cb=r32(r), ctx=r32(r))
    if logged is not None:
        c["queues"]["AreWeLoggedIn"] = [logged]
    if gnull is not None:
        c["gnull"] = gnull
    return c


def getlist_edges():
    r = random.Random(15)
    for logged, rcv, refs, infos, gnull in itertools.product((0, 1), (False, True), (1, 2, 0), (False, True), (False, True)):
        if not rcv and (refs != 1 or infos):
            continue
        yield getlist_case(r, logged, rcv, refs, infos, gnull)
    for refs in (0xFFFFFFFF, 0x80000000, 0x7FFFFFFF):
        yield getlist_case(r, 1, True, refs, True, False)


def rnd(make):
    return lambda r: make(r)


TESTS = [
    FnTest("UploadPostLoginCallbackFn", ppc, host, edge=upload_edges, rnd=rnd(upload_case)),
    FnTest("GotoPlayerFTPPostGetParkInfoCallbackFn", ppc, host, edge=goto_edges, rnd=rnd(goto_case)),
    FnTest("CFSOnlineSystem::ReturnConnectionToCityServer", ppc, host, edge=conn_edges, rnd=rnd(conn_case)),
    FnTest("ccThemeInfo::~ccThemeInfo", ppc, host, edge=dtor_edges("theme", 16), rnd=rnd(dtor_case("theme"))),
    FnTest("ccRideInfo::~ccRideInfo", ppc, host, edge=dtor_edges("ride", 17), rnd=rnd(dtor_case("ride"))),
    FnTest("CFSOnlineFTPControl::CallbackFn", ppc, host, edge=ftp_edges, rnd=rnd(ftp_case)),
    FnTest("CFSOnlineSystemCityList::GetList", ppc, host, edge=getlist_edges, rnd=rnd(getlist_case)),
]
