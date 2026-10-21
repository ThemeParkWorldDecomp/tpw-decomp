"""Unit v5994: the online city record's constructor and destructor, the park record's assignment, the server record's destructor and the
ride list's post-login callback, against libtpw_unit_CFSOnlineCity_CFSOnlineCity.dylib (tests/unit_CFSOnlineCity_CFSOnlineCity.cpp), main app
("SimTheme Park"), src/game/online/FS_OnlineList.cpp:
  CFSOnlineCity::CFSOnlineCity 0x1017bcb4, CFSOnlineCity::~CFSOnlineCity 0x1017be40 (the body is CFSOnlineCity::Destruct),
  CFSOnlinePark::operator= 0x1017c2e4, RideListPostLoginCallbackFn 0x1017b604, CFSOnlineServer::~CFSOnlineServer 0x1017c96c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native layout of the host classes) for the translation. The callees
outside the unit are fakes on both sides that record their arguments: the strings' constructor, destructor and virtual assignments (the
Mac calls the assignments through __ptr_glue and a vtable the module builds), the list calls (AddTail, Find, RemoveAt), operator new and
delete, the online system's constructor and the ride list's two callees.

The world of a case: the objects with their fields, the list owners, the city info block with its strings, the Find answer, the global
gOnlineSystem. What is compared: the event log, the fields the function writes (floats canonical), the answer of the assignment, the
global. The vtable word the Mac constructors and destructors store has no host counterpart. No fused operations. Not tested: a NULL
this for the constructor and the assignment (the original reads through it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineCity_CFSOnlineCity"
MASK = 0xFFFFFFFF

FN_CITY_CTOR, FN_CITY_DTOR, FN_PARK_ASSIGN, FN_RIDE, FN_SRV_DTOR = 0x1017BCB4, 0x1017BE40, 0x1017C2E4, 0x1017B604, 0x1017C96C
SIZES = {FN_CITY_CTOR: 312, FN_CITY_DTOR: 196, FN_PARK_ASSIGN: 324, FN_RIDE: 140, FN_SRV_DTOR: 228}
OWN = set(SIZES)

STR_CTOR, STR_DTOR, OP_DELETE, OP_NEW = 0x10238740, 0x10238710, 0x10238218, 0x102384B8
CITY_ADDTAIL, CITY_FIND, CITY_REMOVE = 0x1017D93C, 0x1017D7D8, 0x1017D858
SRV_FIND, SRV_REMOVE = 0x1017D120, 0x1017D258
SYS_CTOR, COMPLETE_GET, LOGGED_GET = 0x1016F7D0, 0x10174410, 0x1017B6C8
GSYS = 0x10380AB0
# code addresses the fake string vtables point at (any text addresses: the stubs replace what is there)
CODE_COPY16, CODE_PTR16, CODE_COPY8 = 0x10013800, 0x10012604, 0x100137B0

LAYN = ["city_sz", "city_mList", "city_mCityID", "city_mPosX", "city_mPosY", "city_mPosZ", "city_mValue", "city_mF58", "city_mF5C",
        "city_mF60", "city_mF64", "city_s0C", "city_s20", "city_s44",
        "park_sz", "park_mParkID", "park_mF48", "park_mF4C", "park_mF50", "park_mF54", "park_mF58", "park_mF78", "park_mF90",
        "park_mFA8", "park_mFAA", "park_mFAC", "park_mFB0", "park_mFB4", "park_mFB8", "park_mFBC", "park_s0C", "park_s20", "park_s34",
        "park_s7C", "park_s94",
        "srv_sz", "srv_s10", "srv_s24", "srv_s40", "srv_s54", "srv_s6C",
        "citylist_sz", "srvlist_sz", "ridelist_sz", "sys_sz"]
LAY = {}

MAC_SZ = {"city_sz": 0x68, "park_sz": 0xC0, "srv_sz": 0x80, "citylist_sz": 0x34, "srvlist_sz": 0x34, "ridelist_sz": 0x34, "sys_sz": 0x404}
MAC_OFF = {"city_s0C": 0x0C, "city_s20": 0x20, "city_s44": 0x44,
           "park_s0C": 0x0C, "park_s20": 0x20, "park_s34": 0x34, "park_s7C": 0x7C, "park_s94": 0x94,
           "srv_s10": 0x10, "srv_s24": 0x24, "srv_s40": 0x40, "srv_s54": 0x54, "srv_s6C": 0x6C}

CITY_FIELDS = [("mCityID", 0x08, False), ("mPosX", 0x34, True), ("mPosY", 0x38, True), ("mPosZ", 0x3C, True), ("mValue", 0x40, False),
               ("mF58", 0x58, False), ("mF5C", 0x5C, False), ("mF60", 0x60, False), ("mF64", 0x64, False)]
PARK_FIELDS = [("mParkID", 0x08, 4), ("mF48", 0x48, 4), ("mF4C", 0x4C, 4), ("mF50", 0x50, 4), ("mF54", 0x54, 4), ("mF58", 0x58, 32),
               ("mF78", 0x78, 4), ("mF90", 0x90, 4), ("mFA8", 0xA8, 2), ("mFAA", 0xAA, 2), ("mFAC", 0xAC, 2), ("mFB0", 0xB0, 4),
               ("mFB4", 0xB4, 4), ("mFB8", 0xB8, 4), ("mFBC", 0xBC, 4)]
PARK_STRINGS = [("s0C", 11, 21, 16), ("s20", 12, 22, 16), ("s34", 13, 23, 16), ("s7C", 14, 24, 8), ("s94", 15, 25, 16)]
SRV_STRINGS = ["s10", "s24", "s40", "s54", "s6C"]
CITY_STRINGS = ["s0C", "s20", "s44"]
VT = {}
CUR = {}


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)

_rec_str_ctor = CL.recorder("StrCtor", "N")
_rec_str_ptr = CL.recorder("StrAssignPtr", "LS")
_rec_sys_ctor = CL.recorder("SystemCtor", "N")


def st_str_ctor(e):
    p = e.r(3)
    _rec_str_ctor(e)
    e.write(p, struct.pack(">I", VT["vt16"]))
    e.set_r(3, p)


def st_str_dtor(e):
    e.cl.rec("StrDtor", (e.cl.lab(e.r(3)), s32(e.r(4))))


def st_copy(name):
    def f(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))
    return f


def st_ptr(e):
    _rec_str_ptr(e)


def st_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def st_new(e):
    n = e.r(3)
    e.cl.rec("new", (s32(n),))
    e.set_r(3, e.alloc(n))


def st_find(name):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), L.lab(e.r(4))))
        e.set_r(3, L.ret(name) & MASK)
    return f


def st_remove(name):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), e.u32(e.r(4))))
    return f


def st_addtail(e):
    L = e.cl
    L.rec("CityAddTail", (L.lab(e.r(3)), L.lab(e.r(4))))


def st_sys_ctor(e):
    p = e.r(3)
    _rec_sys_ctor(e)
    e.write(p + 0x100, struct.pack(">I", CUR["list"]))
    e.set_r(3, p)


def st_complete(e):
    e.cl.rec("CompleteGetList", (e.cl.lab(e.r(3)), s32(e.r(4))))


def st_logged(e):
    e.cl.rec("LoggedInGetList", (e.cl.lab(e.r(3)),))


TOC = 0x10267CC0     # r2 of the main app (TOC[-0x6680] is at 0x10261640); the glue the fakes replace would have saved it at 0x14(r1)


def with_toc(f):
    def g(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", TOC))
        f(e)
    return g


for _a, _f in ((STR_CTOR, st_str_ctor), (STR_DTOR, st_str_dtor), (OP_DELETE, st_delete), (OP_NEW, st_new),
               (CODE_COPY16, st_copy("StrCopyAssign16")), (CODE_PTR16, st_ptr), (CODE_COPY8, st_copy("StrCopyAssign8")),
               (CITY_ADDTAIL, st_addtail), (CITY_FIND, st_find("CityFind")), (CITY_REMOVE, st_remove("CityRemoveAt")),
               (SRV_FIND, st_find("ServerFind")), (SRV_REMOVE, st_remove("ServerRemoveAt")), (SYS_CTOR, st_sys_ctor),
               (COMPLETE_GET, st_complete), (LOGGED_GET, st_logged)):
    STUBS.internal(_a, with_toc(_f))


def setup(runner):
    emu = runner.emu
    T.install_std_stubs(emu)
    blk = bytearray(0x100)
    tv = {CODE_COPY16: 0x80, CODE_PTR16: 0x88, CODE_COPY8: 0x90}
    base = T.persist_alloc(emu, 0x100)
    for code, off in tv.items():
        blk[off:off + 8] = struct.pack(">II", code, TOC)
    blk[0x1C:0x20] = struct.pack(">I", base + 0x80)           # vt16 at +0: slot 0x1c, slot 0x20
    blk[0x20:0x24] = struct.pack(">I", base + 0x88)
    blk[0x40 + 0x1C:0x40 + 0x20] = struct.pack(">I", base + 0x90)   # vt8 at +0x40
    emu.write(base, bytes(blk))
    VT["vt16"], VT["vt8"] = base, base + 0x40


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def wstr(self, text):
        data = b"".join(struct.pack(self.fmt[0] + "H", ord(c)) for c in text) + b"\0\0"
        a = self.alloc(len(data))
        self.write(a, data)
        return a

    def block(self, n):
        a = self.alloc(n)
        self.write(a, bytes(n))
        return a

    def make_objs(self, case):
        op, o = case["op"], {}
        if op in ("city_ctor", "city_dtor"):
            o["owner"], o["tbl"], o["city"] = self.block(self.sz("citylist_sz")), self.block(0x10), self.block(self.sz("city_sz"))
            if op == "city_ctor":
                o["info"] = self.block(0x30)
        elif op == "server_dtor":
            o["owner"], o["tbl"], o["srv"] = self.block(self.sz("srvlist_sz")), self.block(0x10), self.block(self.sz("srv_sz"))
        elif op == "park_assign":
            o["dst"], o["src"] = self.block(self.sz("park_sz")), self.block(self.sz("park_sz"))
        else:
            o["list"], o["sys"] = self.block(self.sz("ridelist_sz")), self.block(self.sz("sys_sz"))
        return o

    def labels(self, case, o):
        op = case["op"]
        if op in ("city_ctor", "city_dtor"):
            m = {o["owner"]: 1, o["tbl"]: 2, o["city"]: 3}
            if op == "city_dtor":
                for i, n in enumerate(CITY_STRINGS):
                    m[o["city"] + self.off("city_" + n)] = 10 + i
        elif op == "server_dtor":
            m = {o["owner"]: 1, o["tbl"]: 2, o["srv"]: 3}
            for i, n in enumerate(SRV_STRINGS):
                m[o["srv"] + self.off("srv_" + n)] = 10 + i
        elif op == "park_assign":
            m = {o["dst"]: 3, o["src"]: 4}
            for n, ld, ls, _ in PARK_STRINGS:
                m[o["dst"] + self.off("park_" + n)] = ld
                m[o["src"] + self.off("park_" + n)] = ls
        else:
            m = {o["list"]: 1, o["sys"]: 2}
        return m


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.o = self.make_objs(case)
        self.L = e.cl = CL.PpcLog(self.labels(case, self.o), case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def sz(self, k):
        return MAC_SZ[k]

    def off(self, k):
        return MAC_OFF[k]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ref(self, a):
        return a

    def log(self):
        return self.L.log

    def prep(self, case):
        o, op = self.o, case["op"]
        if op in ("city_dtor", "server_dtor"):
            obj = o["city"] if op == "city_dtor" else o["srv"]
            self.words(obj + 4, [o["owner"]])
        if op in ("city_ctor", "city_dtor", "server_dtor"):
            self.words(o["owner"] + 0x2C, [o["tbl"]])
        if op == "park_assign":
            for n, _, _, bits in PARK_STRINGS:
                for k in ("dst", "src"):
                    self.words(o[k] + MAC_OFF["park_" + n], [VT["vt16"] if bits == 16 else VT["vt8"]])
        if op == "ride":
            CUR["list"] = o["list"]
            if case["exists"]:
                self.words(o["sys"] + 0x100, [o["list"]])
            self.words(GSYS, [o["sys"] if case["exists"] else 0])

    def call(self, case):
        o, op = self.o, case["op"]
        if op == "city_ctor":
            args, fn = [o["city"], o["info"], o["owner"]], FN_CITY_CTOR
        elif op == "city_dtor":
            args, fn = [0 if case["null"] else o["city"], case["flag"] & MASK], FN_CITY_DTOR
        elif op == "server_dtor":
            args, fn = [0 if case["null"] else o["srv"], case["flag"] & MASK], FN_SRV_DTOR
        elif op == "park_assign":
            args, fn = [o["dst"], o["src"]], FN_PARK_ASSIGN
        else:
            args, fn = [case["error"] & MASK, 0], FN_RIDE
        return T.call_ppc(self.e, fn, args)[0] & MASK

    def city_owner(self, city):
        return self.rd_word(city + 4)

    def gsys(self):
        return self.rd_word(GSYS)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.o = self.make_objs(case)
        inv = {lab: a for a, lab in self.labels(case, self.o).items()}
        CL.host_begin(lib, inv, case["queues"])

    def done(self):
        pass

    def sz(self, k):
        return LAY[k]

    def off(self, k):
        return LAY[k]

    def alloc(self, n):
        return self.lib.tpw_cc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ref(self, a):
        return self.lib.tpw_cc_off(a)

    def log(self):
        return CL.host_end(self.lib)

    def prep(self, case):
        o, op, lib = self.o, case["op"], self.lib
        if op in ("city_dtor", "city_ctor"):
            lib.tpw_cc_owner_set(0, o["owner"], o["tbl"])
        if op == "city_dtor":
            lib.tpw_cc_city_set_owner(o["city"], o["owner"])
            lib.tpw_cc_city_init(o["city"])
        if op == "server_dtor":
            lib.tpw_cc_owner_set(1, o["owner"], o["tbl"])
            lib.tpw_cc_server_init(o["srv"], o["owner"])
        if op == "park_assign":
            lib.tpw_cc_park_init(o["dst"])
            lib.tpw_cc_park_init(o["src"])
        if op == "ride":
            if case["exists"]:
                lib.tpw_cc_sys_make(o["sys"], o["list"])
            lib.tpw_cc_gsys_set(o["sys"] if case["exists"] else 0)

    def call(self, case):
        o, op, lib = self.o, case["op"], self.lib
        if op == "city_ctor":
            lib.tpw_cc_city_ctor(o["city"], o["info"], o["owner"])
            return 0
        if op == "city_dtor":
            lib.tpw_cc_city_dtor(0 if case["null"] else o["city"], s32(case["flag"]))
            return 0
        if op == "server_dtor":
            lib.tpw_cc_server_dtor(0 if case["null"] else o["srv"], s32(case["flag"]))
            return 0
        if op == "park_assign":
            return lib.tpw_cc_park_assign(o["dst"], o["src"])
        lib.tpw_cc_ride(s32(case["error"]), o["list"])
        return 0

    def city_owner(self, city):
        return self.lib.tpw_cc_city_owner(city)

    def gsys(self):
        g = self.lib.tpw_cc_gsys_get()
        self.lib.tpw_cc_gsys_set(0)
        return g


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, u32, i32 = C.c_uint64, C.c_uint32, C.c_int32
    sigs = {"alloc": (u64, [u32]), "off": (u32, [u64]), "owner_set": (None, [i32, u64, u64]), "city_set_owner": (None, [u64, u64]),
            "city_owner": (u64, [u64]), "city_init": (None, [u64]), "city_ctor": (None, [u64, u64, u64]), "city_dtor": (None, [u64, i32]),
            "server_init": (None, [u64, u64]), "server_dtor": (None, [u64, i32]), "park_init": (None, [u64]),
            "park_assign": (u64, [u64, u64]), "sys_make": (u64, [u64, u64]), "gsys_set": (None, [u64]), "gsys_get": (u64, []),
            "ride": (None, [i32, u64])}
    for name, (res, args) in sigs.items():
        f = getattr(lib, "tpw_cc_" + name)
        f.restype, f.argtypes = res, args
    lib.tpw_cc_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 64)()
    n = lib.tpw_cc_layout(out)
    assert n == len(LAYN), (n, len(LAYN))
    LAY.update(zip(LAYN, list(out)[:n]))
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o, op = s.o, case["op"]
    if op == "city_ctor":
        i = case["info"]
        ptrs = [s.ref(s.wstr(t)) for t in i["texts"]]
        s.words(o["info"], [i["f00"], ptrs[0], ptrs[1], i["fl"][0], i["fl"][1], i["fl"][2], i["w18"], ptrs[2]] + i["w20"])
    if op == "park_assign":
        for k, vals in (("dst", case["dst"]), ("src", case["src"])):
            for (name, mac, size), v in zip(PARK_FIELDS, vals):
                off = (MAC_OFF.get("x") or 0)
                a = o[k] + (mac if isinstance(s, Ppc) else LAY["park_" + name])
                if size == 32:
                    s.write(a, bytes(v))
                else:
                    s.write(a, struct.pack(s.fmt[0] + ("I" if size == 4 else "H"), v))
    s.prep(case)


def observe(s, case, answer):
    o, op = s.o, case["op"]
    d = {}
    if op == "city_ctor":
        d["owner_ok"] = s.city_owner(o["city"]) == o["owner"]
        for name, mac, fl in CITY_FIELDS:
            a = o["city"] + (mac if isinstance(s, Ppc) else LAY["city_" + name])
            v = s.rd_word(a)
            d[name] = canon_f(v) if fl else v
    elif op == "park_assign":
        d["is_dst"] = answer == o["dst"]
        for k in ("dst", "src"):
            for name, mac, size in PARK_FIELDS:
                a = o[k] + (mac if isinstance(s, Ppc) else LAY["park_" + name])
                d[k + name] = s.read(a, size) if size == 32 else struct.unpack(s.fmt[0] + ("I" if size == 4 else "H"), s.read(a, size))[0]
    elif op == "ride":
        g = s.gsys()
        d["gsys"] = 0 if g == 0 else (1 if g == o["sys"] and case["exists"] else 2)
    d["log"] = s.log()
    return d


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]
TEXTS = ["", "A", "Paris", "café ÿ", "中文 text", "x" * 20]
FLAGS = [1, 0, -1, 2, 0x7FFF, 0x8000, 0x10000, 0x10001, -0x10000, 0xFFFF, 0x7FFF0001]
FINDS = [0, 0x1000, 0x12345678, 1, 0xFFFFFFFF]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def w32(r):
    return r.choice([0, 1, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF]) if r.random() < 0.3 else r.getrandbits(32)


# ---- city ctor
def city_ctor_case(r, fl=None):
    c = {"op": "city_ctor", "queues": {}}
    c["info"] = {"f00": w32(r), "texts": [r.choice(TEXTS) for _ in range(3)], "fl": fl or [rf(r), rf(r), rf(r)], "w18": w32(r),
                 "w20": [w32(r) for _ in range(4)]}
    return c


def city_ctor_edges():
    r = random.Random(5001)
    for x in SPECIAL:
        yield city_ctor_case(r, [x, fb(1.0), fb(-2.5)])
        yield city_ctor_case(r, [fb(3.0), x, fb(0.1)])
        yield city_ctor_case(r, [fb(0.5), fb(7.0), x])
    for _ in range(4):
        yield city_ctor_case(r)


def city_ctor_rnd(r):
    return city_ctor_case(r)


# ---- destructors
def dtor_case(op):
    def f(r, flag=None, find=None, null=False):
        return {"op": op, "queues": {"CityFind": [r.choice(FINDS) if find is None else find],
                                     "ServerFind": [r.choice(FINDS) if find is None else find]},
                "flag": r.choice(FLAGS) if flag is None else flag, "null": null}
    return f


def dtor_edges(op):
    def g():
        f, r = dtor_case(op), random.Random(5002)
        for flag in FLAGS:
            for find in (0, 0x1000):
                yield f(r, flag, find)
        yield f(r, 1, 0x1000, True)
        yield f(r, 0, 0, True)
        yield f(r, 1, 0xFFFFFFFF)
    return g


def dtor_rnd(op):
    f = dtor_case(op)
    return lambda r: f(r)


# ---- park assign
def park_case(r, same=False):
    def vals():
        out = []
        for name, mac, size in PARK_FIELDS:
            out.append(bytes(r.getrandbits(8) for _ in range(32)) if size == 32 else
                       (w32(r) if size == 4 else r.choice([0, 0xFFFF, 0x8000, r.getrandbits(16)])))
        return out
    d = vals()
    return {"op": "park_assign", "queues": {}, "dst": d, "src": list(d) if same else vals()}


def park_edges():
    r = random.Random(5003)
    yield park_case(r, True)
    for _ in range(4):
        yield park_case(r)


def park_rnd(r):
    return park_case(r)


# ---- ride callback
def ride_case(r, error=None, exists=None):
    return {"op": "ride", "queues": {}, "error": r.choice([0, 1, -1, 2, 0x10000, 0x7FFFFFFF]) if error is None else error,
            "exists": r.choice([True, False]) if exists is None else exists}


def ride_edges():
    r = random.Random(5004)
    for error in (0, 1, -1):
        for exists in (True, False):
            yield ride_case(r, error, exists)


def ride_rnd(r):
    return ride_case(r)


TESTS = [
    FnTest("CFSOnlineCity::CFSOnlineCity", ppc, host, edge=city_ctor_edges, rnd=city_ctor_rnd),
    FnTest("CFSOnlineCity::~CFSOnlineCity", ppc, host, edge=dtor_edges("city_dtor"), rnd=dtor_rnd("city_dtor")),
    FnTest("CFSOnlinePark::operator=", ppc, host, edge=park_edges, rnd=park_rnd),
    FnTest("RideListPostLoginCallbackFn", ppc, host, edge=ride_edges, rnd=ride_rnd),
    FnTest("CFSOnlineServer::~CFSOnlineServer", ppc, host, edge=dtor_edges("server_dtor"), rnd=dtor_rnd("server_dtor")),
]
