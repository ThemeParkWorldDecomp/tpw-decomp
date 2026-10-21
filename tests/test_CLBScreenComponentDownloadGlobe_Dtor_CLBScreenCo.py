"""Unit v5553, the online download globe's destructor and Hide, against libtpw_unit_CLBScreenComponentDownloadGlobe_Dtor_CLBScreenCo.dylib
(tests/unit_CLBScreenComponentDownloadGlobe_Dtor_CLBScreenCo.cpp), main app ("SimTheme Park"),
src/game/online/lobby_globe.cpp:
  CLBScreenComponentDownloadGlobe::~CLBScreenComponentDownloadGlobe 0x100af330, CLBScreenComponentDownloadGlobe::Hide 0x100ae63c

A case is a world and a call. The world: the globe object (visible word, two lists of globes / city flags, the two sound words), the lists'
nodes (the real RemoveHead of the original runs on the PowerPC side, the header's on the host), gei_Display's flags, gLobbyMeshes. The
callees outside the unit are fakes on both sides that record the same events: __ptr_glue (the virtual calls: IsVisible, the globes', flags'
and lists' destructors, told apart by the TVector in r12), operator delete (label of the block, -1 for a node), the base destructor, the
controllers, the meshes and the sound.

What is compared: the answer (this), the event log, the object's fields (visible, both list pointers as present or not, both sounds) and
gei_Display's flags. Not tested: the vtable word the PowerPC destructor stores (the host's vptr is the C++ one); a NULL list (the original
reads it unconditionally). No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CLBScreenComponentDownloadGlobe_Dtor_CLBScreenCo"
MASK = 0xFFFFFFFF

FN_DTOR, FN_HIDE = 0x100AF330, 0x100AE63C
SIZES = {FN_DTOR: 300, FN_HIDE: 312}
OWN = set(SIZES)

PTR_GLUE = 0x1023BAA8
OP_DELETE = 0x10238218
REMOVE_WORLD = 0x1022B6AC
REMOVE_OVERLAY = 0x101E2314
DELETE_MESH = 0x100A88A4
FADE = 0x100DE0FC
BASE_DTOR = 0x100B6C2C

ADDR = dict(flags=0x103BC8D0, meshes=0x102E48D0)
HOST_LAYOUT = []

POOL = {}      # name -> PowerPC address (persistent blocks), filled by setup
TV = {}        # TVector address -> kind


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ptr_glue(e):
    L = e.cl
    kind = TV.get(e.r(12))
    r3 = e.r(3)
    if kind == "isvis":
        L.rec("IsVisible", (L.lab(r3),))
        e.set_r(3, L.ret("IsVisible") & MASK)
    elif kind == "globe":
        L.rec("globeDtor", (L.lab(r3),))
        if T.s16(e.r(4)) > 0:
            L.rec("delete", (L.lab(r3),))
    elif kind == "flag":
        L.rec("flagDtor", (L.lab(r3),))
        if T.s16(e.r(4)) > 0:
            L.rec("delete", (L.lab(r3),))
    elif kind == "list":
        if T.s16(e.r(4)) > 0:
            L.rec("delete", (L.lab(r3),))
    else:
        L.rec("UNKNOWN_VCALL", (e.r(12),))


def stub_delete(e):
    # the host's operator delete records only the blocks it has a label for (the list nodes are not logged)
    if e.cl.lab(e.r(3)) != -1:
        e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def stub_base_dtor(e):
    e.cl.rec("baseDtor", (e.cl.lab(e.r(3)),))
    if e.r(4) & MASK:
        e.cl.rec("BADFLAG", (e.r(4) & MASK,))


STUBS.internal(PTR_GLUE, stub_ptr_glue)
STUBS.internal(OP_DELETE, stub_delete)
STUBS.internal(BASE_DTOR, stub_base_dtor)
STUBS.internal(REMOVE_WORLD, lambda e: e.cl.rec("RemoveWorldLobbyController", ()))
STUBS.internal(REMOVE_OVERLAY, lambda e: e.cl.rec("RemoveOnlineCityInfoOverlay", ()))
STUBS.internal(DELETE_MESH, lambda e: e.cl.rec("DeleteMesh", (e.cl.lab(e.r(3)),)))
STUBS.internal(FADE, lambda e: e.cl.rec("Fade", (s32(e.r(3)),)))


def setup(runner):
    T.install_std_stubs(runner.emu)
    emu = runner.emu
    # gei_Display is an import: its TOC slot (0x1026067c, listing address 0x103bc8d0) holds the emulator's stand-in address
    ADDR["flags"] = emu.u32(0x1026067C)
    POOL["this"] = T.persist_alloc(emu, 0x80)
    POOL["glist"] = T.persist_alloc(emu, 16)
    POOL["flist"] = T.persist_alloc(emu, 16)
    POOL["mesh"] = T.persist_alloc(emu, 16)
    for i in range(6):
        POOL["g%d" % i] = T.persist_alloc(emu, 16)
        POOL["f%d" % i] = T.persist_alloc(emu, 16)
    tvs = T.persist_alloc(emu, 64)
    for i, kind in enumerate(("isvis", "globe", "flag", "list")):
        TV[tvs + 8 * i] = kind
        POOL["tv_" + kind] = tvs + 8 * i
    for name, slot, kind in (("vt_this", 0x2C, "isvis"), ("vt_globe", 8, "globe"), ("vt_flag", 8, "flag"), ("vt_list", 8, "list")):
        POOL[name] = T.persist_alloc(emu, 0x40)
        emu.write(POOL[name] + slot, struct.pack(">I", POOL["tv_" + kind]))


LABELS = {"this": 1, "glist": 2, "flist": 3, "mesh": 4}
for _i in range(6):
    LABELS["g%d" % _i] = 10 + _i
    LABELS["f%d" % _i] = 20 + _i


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({POOL[n]: v for n, v in LABELS.items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name] if name in ADDR else POOL[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def build(self, case):
        P = POOL
        self.words(P["this"], [P["vt_this"], case["vis"]])
        self.words(P["this"] + 8, [P["glist"]])
        self.words(P["this"] + 0x6C, [P["flist"], 0, case["f74"], case["f78"]])
        for lst, vt, kind, ents, mask in (("glist", "vt_list", "g", case["g"], case["gmask"]), ("flist", "vt_list", "f", case["f"], case["fmask"])):
            nodes = []
            for i in range(len(ents)):
                obj = P["%s%d" % (kind, i)] if ents[i] else 0
                if obj:
                    self.words(obj, [P["vt_globe" if kind == "g" else "vt_flag"]])
                nodes.append((self.e.alloc(12), obj))
            for i, (n, obj) in enumerate(nodes):
                nxt = nodes[i + 1][0] if i + 1 < len(nodes) else 0
                prv = nodes[i - 1][0] if i > 0 else 0
                self.words(n, [obj, nxt, prv])
            head = nodes[0][0] if nodes else 0
            tail = nodes[-1][0] if nodes else 0
            self.words(P[lst], [P[vt], head, tail, len(nodes)])
        self.words(ADDR["meshes"], [P["mesh"]])
        self.words(ADDR["flags"], [case["dflags"]])

    def call(self, case):
        if case["op"] == "dtor":
            r3, _, _ = T.call_ppc(self.e, FN_DTOR, [0 if case["null"] else POOL["this"], case["flag"] & MASK])
            return 1 if r3 == POOL["this"] else r3
        T.call_ppc(self.e, FN_HIDE, [POOL["this"]])
        return 0

    def fields(self):
        w = self.rd_words(POOL["this"], 0x7C // 4)
        return [w[1], int(w[2] != 0), int(w[0x6C // 4] != 0), w[0x74 // 4], w[0x78 // 4]]


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.lib.tpw_dg_build(s32(case["vis"]), s32(case["f74"]), s32(case["f78"]), case["gmask"], len(case["g"]), case["fmask"],
                              len(case["f"]), case["dflags"])
        CL.host_begin(lib, {v: self.g(n) for n, v in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_dg_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def build(self, case):
        pass   # done in __init__: the labels need the blocks' addresses

    def call(self, case):
        if case["op"] == "dtor":
            r = self.lib.tpw_dg_dtor(s32(case["flag"]), 1 if case["null"] else 0)
            return 1 if r == self.g("this") else r
        self.lib.tpw_dg_hide()
        return 0

    def fields(self):
        out = (C.c_int32 * 5)()
        self.lib.tpw_dg_read(out)
        return [out[0] & MASK, out[1], out[2], out[3] & MASK, out[4] & MASK]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_dg_addr.restype, lib.tpw_dg_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_dg_alloc.restype, lib.tpw_dg_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_dg_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_dg_build.restype = None
    lib.tpw_dg_build.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_uint32, C.c_int32, C.c_uint32, C.c_int32, C.c_uint32]
    lib.tpw_dg_read.restype, lib.tpw_dg_read.argtypes = None, [C.POINTER(C.c_int32)]
    lib.tpw_dg_hide.restype, lib.tpw_dg_hide.argtypes = None, []
    lib.tpw_dg_dtor.restype, lib.tpw_dg_dtor.argtypes = C.c_uint64, [C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_dg_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.build(case)
        ans = s.call(case)
        lg = s.log()
        return {"answer": ans, "log": lg, "fields": s.fields(), "dflags": s.rd_words(s.g("flags"), 1)[0]}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op, g=None, f=None):
    ng = r.randrange(0, 5) if g is None else len(g)
    nf = r.randrange(0, 5) if f is None else len(f)
    gl = [r.choice([1, 1, 1, 0]) for _ in range(ng)] if g is None else g
    fl = [r.choice([1, 1, 1, 0]) for _ in range(nf)] if f is None else f
    return {"op": op, "g": gl, "f": fl,
            "gmask": sum(1 << i for i, x in enumerate(gl) if x), "fmask": sum(1 << i for i, x in enumerate(fl) if x),
            "vis": r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)]), "f74": r.choice([0, r.getrandbits(32)]),
            "f78": r.choice([0, r.getrandbits(32)]), "dflags": r.choice([0, 0x02000000, 0xFFFFFFFF, r.getrandbits(32)]),
            "flag": 0, "null": False,
            "queues": {"IsVisible": r.choice([[0], [1, 0], [1, 1], [1, 5], [5, 1], [0xFFFFFFFF, 0x100000000]])}}


def dtor_case(r, g=None, f=None, flag=None, null=False):
    c = base(r, "dtor", g, f)
    c["flag"] = r.choice([0, 1, 2, -1, 0x7FFF, 0x8000, 0x10000, 0x18000, 0xFFFF8000, r.getrandbits(32)]) & MASK if flag is None else flag
    c["null"] = null
    return c


def dtor_edges():
    r = random.Random(5301)
    yield dtor_case(r, [], [], 1)
    yield dtor_case(r, [1], [1], 1)
    yield dtor_case(r, [0], [0], 0)
    yield dtor_case(r, [1, 0, 1], [0, 1], 0)
    yield dtor_case(r, [1, 1, 1, 1, 1, 1], [1, 1, 1, 1, 1, 1], 1)
    for fl in (0, 1, 0xFFFF, 0x8000, 0x7FFF, 0x10000, 0x10001, 0xFFFFFFFF, 0x80000000):
        yield dtor_case(r, [1], [1], fl)
    yield dtor_case(r, [1], [1], 1, True)
    yield dtor_case(r, [1], [1], 0, True)


def dtor_random(r):
    return dtor_case(r)


def hide_case(r, g=None, f=None, q=None):
    c = base(r, "hide", g, f)
    if q is not None:
        c["queues"] = {"IsVisible": q}
    return c


def hide_edges():
    r = random.Random(5302)
    for q in ([0], [1, 0], [1, 1], [5, 0], [0xFFFFFFFF, 0xFFFFFFFF], [0x100000000, 0x100000000]):
        yield hide_case(r, [1, 0, 1], [1, 1], q)
        yield hide_case(r, [], [], q)
    yield hide_case(r, [1], [], [1, 1])
    yield hide_case(r, [], [1], [1, 1])
    yield hide_case(r, [0, 0], [0], [1, 1])


def hide_random(r):
    return hide_case(r)


TESTS = [
    FnTest("CLBScreenComponentDownloadGlobe::~CLBScreenComponentDownloadGlobe", ppc, host, edge=dtor_edges, rnd=dtor_random),
    FnTest("CLBScreenComponentDownloadGlobe::Hide", ppc, host, edge=hide_edges, rnd=hide_random),
]
