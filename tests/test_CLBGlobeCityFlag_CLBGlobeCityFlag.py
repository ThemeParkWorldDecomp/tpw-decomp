"""Unit v5546, the online globe's city flag, against libtpw_unit_CLBGlobeCityFlag_CLBGlobeCityFlag.dylib
(tests/unit_CLBGlobeCityFlag_CLBGlobeCityFlag.cpp), main app ("SimTheme Park"), src/CLBGlobeCityFlag_CLBGlobeCityFlag.cpp:
  CLBGlobeCityFlag::CLBGlobeCityFlag 0x100ac5c8, CLBGlobeCityFlag::~CLBGlobeCityFlag 0x100ac86c

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory, big-endian, and the host library's
static buffers). The callees outside the unit are recording fakes on both sides: _wcsicmp (answers from a queue), InstanceMeshType,
Show, SetDirection, SetPosition, Scale, the globe's two virtuals (through __ptr_glue on the Mac) and the delete of the destructor's
flag. Names and strings are logged as a hash of their characters.

The world of a case: the city (name, position, value), gLobbyMeshes, the flag block. What is compared: the answer, the flag's fields as
labels, the city's numbers, the mesh block, the event log.

fmadds is hooked with HardwareRounding. Not tested: a NULL city or globe (read unconditionally); the delete flag > 0 on the host is
the wrapper's event (the compiler's delete), and the Mac's string at 0x1023e91e is not known, so both sides compare against their own.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CLBGlobeCityFlag_CLBGlobeCityFlag"
MASK = 0xFFFFFFFF

FN_CT, FN_DT = 0x100ac5c8, 0x100ac86c
SIZES = {FN_CT: 580, FN_DT: 112}
OWN = set(SIZES)

INSTANCE, SHOW, SETDIR, SETPOS, SCALE = 0x100a8408, 0x100a7860, 0x100a798c, 0x100a790c, 0x100a7c90
WCSICMP, PTRGLUE, DELETE = 0x102399b8, 0x1023baa8, 0x10238218

G_MESHES = 0x102e48d0
HOST_LAYOUT = [8, 16, 24, 32, 0, 8, 12, 16, 20, 168]
OFF_PPC = dict(mesh=4, globe=8, city=0xC, name=0x14, value=0x40, x=0x34, y=0x38, z=0x3C)
OFF_HOST = dict(mesh=8, globe=16, city=24, name=0, value=8, x=12, y=16, z=20)
PN = ("meshes", "mesh", "globe", "flag", "city", "name")
LABELS = {"meshes": 1, "mesh": 2, "globe": 3, "flag": 4, "city": 5}
P = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


def hsh(units):
    h = 0
    for c in units:
        h = (h * 33 + c) & MASK
    return h


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cstr_units(e, a):
    out = []
    while len(out) < 64:
        c = e.cstr_byte(a + len(out)) if False else e.read(a + len(out), 1)[0]
        if c == 0:
            break
        out.append(c)
    return out


def wide_units(e, a):
    out = []
    while len(out) < 64:
        c = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if c == 0:
            break
        out.append(c)
    return out


def stub_wcsicmp(e):
    L = e.cl
    L.rec("wcsicmp", (hsh(wide_units(e, e.r(3))), 1 if e.r(4) else 0))
    e.set_r(3, L.ret("wcsicmp") & MASK)


def stub_instance(e):
    L = e.cl
    L.rec("InstanceMeshType", (L.lab(e.r(3)), hsh(cstr_units(e, e.r(4))), hsh(cstr_units(e, e.r(5))), s32(e.r(6)), e.r(7),
                               L.lab(e.r(8)), L.lab(e.r(9))))
    e.set_r(3, P["mesh"])


def stub_show(e):
    e.cl.rec("Show", (e.cl.lab(e.r(3)),))


def vec(e, name):
    def fn(e):
        a = e.r(4)
        e.cl.rec(name, (e.cl.lab(e.r(3)), canon_f(e.u32(a)), canon_f(e.u32(a + 4)), canon_f(e.u32(a + 8))))
    return fn


def stub_scale(e):
    e.cl.rec("Scale", (e.cl.lab(e.r(3)), canon_f(fb(e.f(1))), canon_f(fb(e.f(2))), canon_f(fb(e.f(3)))))


def stub_glue(e):
    e.cl.rec("vcall", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), e.r(12) & 0xFF))


def stub_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


STUBS.imp("_wcsicmp__10NS_MacDozeFPCwPCw", stub_wcsicmp)
STUBS.internal(INSTANCE, stub_instance)
STUBS.internal(SHOW, stub_show)
STUBS.internal(SETDIR, vec(None, "SetDirection"))
STUBS.internal(SETPOS, vec(None, "SetPosition"))
STUBS.internal(SCALE, stub_scale)
def keep_toc(fn):
    """The glue saves the TOC at 0x14(r1) for the caller's `lwz r2, 0x14(r1)` that follows."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return wrapped


STUBS.internal(PTRGLUE, keep_toc(stub_glue))
STUBS.internal(DELETE, keep_toc(stub_delete))


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    sizes = dict(meshes=16, mesh=64, globe=16, flag=64, city=0x68, name=96)
    for n in PN:
        P[n] = T.persist_alloc(e, sizes[n])
    vt = T.persist_alloc(e, 0x80)
    e.write(vt + 0x40, struct.pack(">II", 0xAAAA0040, 0xAAAA0044))
    e.write(P["globe"], struct.pack(">I", vt))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def label(self, p):
        if p == 0:
            return 0
        for n, v in LABELS.items():
            if self.addr[n] == p:
                return v
        return -1


class Ppc(Side):
    fmt = ">I"
    off = OFF_PPC

    def __init__(self, e, case):
        self.e = e
        self.addr = P
        self.L = e.cl = CL.PpcLog({P[n]: v for n, v in LABELS.items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def setptr(self, a, v):
        self.words(a, [v])

    def rdptr(self, a):
        return self.rd_words(a, 1)[0]

    def units(self, a, us):
        self.write(a, b"".join(struct.pack(">H", u) for u in us + [0]))

    def meshes_global(self):
        return G_MESHES

    def log(self):
        return self.L.log

    def ctor(self, case):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN_CT, [P["flag"], P["globe"], P["city"]])
        return r3 & MASK

    def dtor(self, case):
        arg0 = 0 if case["null"] else P["flag"]
        r3, _, _ = T.call_ppc(self.e, FN_DT, [arg0, (case["flag"] & 0xFFFF) | (case["hi"] << 16)])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    off = OFF_HOST

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_cf_prep()
        self.addr = {n: lib.tpw_cf_addr(n.encode()) for n in PN}
        CL.host_begin(lib, {v: self.addr[n] for n, v in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rdptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def units(self, a, us):
        self.write(a, b"".join(struct.pack("<H", u) for u in us + [0]))

    def meshes_global(self):
        return self.lib.tpw_cf_addr(b"gLobbyMeshes")

    def log(self):
        return CL.host_end(self.lib)

    def ctor(self, case):
        return self.lib.tpw_cf_ctor()

    def dtor(self, case):
        return self.lib.tpw_cf_dtor(1 if case["null"] else 0, s32(case["flag"] << 16) >> 16)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cf_addr.restype, lib.tpw_cf_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cf_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cf_ctor.restype, lib.tpw_cf_ctor.argtypes = C.c_uint64, []
    lib.tpw_cf_dtor.restype, lib.tpw_cf_dtor.argtypes = C.c_uint64, [C.c_int32, C.c_int16]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cf_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o, A = s.off, s.addr
    s.write(A["flag"], bytes(64))
    s.write(A["city"], bytes(0x68 if isinstance(s, Ppc) else 24))
    s.write(A["mesh"], bytes(range(16)))
    s.setptr(s.meshes_global(), A["meshes"])
    s.units(A["name"], list(case["name"]))
    s.setptr(A["city"] + o["name"], A["name"])
    s.words(A["city"] + o["x"], case["pos"])
    s.words(A["city"] + o["value"], [case["value"]])
    if case["op"] == "dt":
        s.setptr(A["flag"] + o["mesh"], A["mesh"])
        s.setptr(A["flag"] + o["globe"], A["globe"])
        s.setptr(A["flag"] + o["city"], A["city"])


def execute(s, case):
    return s.ctor(case) if case["op"] == "ct" else s.dtor(case)


def observe(s, case, answer):
    o, A = s.off, s.addr
    r = {"answer": s.label(answer), "log": s.log(),
         "city": s.rd_words(A["city"] + o["x"], 3) + s.rd_words(A["city"] + o["value"], 1),
         "mesh": bytes(s.read(A["mesh"], 16))}
    if case["op"] == "ct":
        r["fields"] = [s.label(s.rdptr(A["flag"] + o[k])) for k in ("mesh", "globe", "city")]
        r["vptr"] = s.rd_words(A["flag"], 1)[0] != 0 if isinstance(s, Ppc) else s.rdptr(A["flag"]) != 0
    return r


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


# ---------------------------------------------------------------------------------------------------------------- cases

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]
VALUES = [-5, 0, 1, 79, 80, 81, 160, 161, 1000, 0x7FFFFFFF, -0x80000000, -41, -40, -39]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def base(r, op):
    name = [r.randrange(65, 123) for _ in range(r.randrange(0, 9))]
    return {"op": op, "queues": {"wcsicmp": [r.choice([0, 1])]}, "name": name,
            "pos": [rf(r), rf(r), rf(r)], "value": r.choice(VALUES + [r.randrange(-300, 400) & MASK]) & MASK,
            "flag": 0, "hi": 0, "null": False}


def ct_edges():
    r = random.Random(11)
    for q in (0, 1):
        for v in VALUES:
            c = base(r, "ct")
            c["queues"] = {"wcsicmp": [q]}
            c["value"] = v & MASK
            yield c
    for p in ([0, 0, 0], [fb(1.0), 0, 0], [0, fb(-2.0), 0], [0, 0, fb(3.0)], [fb(1e30), fb(1e30), fb(1e30)],
              [0x7F800000, 0, 0], [0x7FC00000, 0, 0], [fb(1e-30)] * 3, [0x80000000, 0, 0x80000000]):
        c = base(r, "ct")
        c["pos"] = p
        yield c


def ct_random(r):
    return base(r, "ct")


def dt_case(r, flag=None, null=None):
    c = base(r, "dt")
    c["flag"] = r.choice([0, 1, -1, 0x7FFF, -0x8000, 2, r.randrange(-40000, 40000)]) & 0xFFFF if flag is None else flag & 0xFFFF
    c["hi"] = r.randrange(0, 0x10000)
    c["null"] = (r.random() < 0.1) if null is None else null
    return c


def dt_edges():
    r = random.Random(12)
    for f in (0, 1, -1, 0x7FFF, -0x8000, 2, 0x8000, 0x10000):
        for n in (False, True):
            yield dt_case(r, f, n)


def dt_random(r):
    return dt_case(r)


TESTS = [
    FnTest("CLBGlobeCityFlag::CLBGlobeCityFlag", ppc, host, edge=ct_edges, rnd=ct_random),
    FnTest("CLBGlobeCityFlag::~CLBGlobeCityFlag", ppc, host, edge=dt_edges, rnd=dt_random),
]
