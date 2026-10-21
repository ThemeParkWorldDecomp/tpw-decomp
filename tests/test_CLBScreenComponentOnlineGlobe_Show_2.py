"""Unit v5923, the online globe window's Show, Hide, GlobeZoomIn, GlobeZoomOut, SortOutZoomConstants, CreateCities and UnRegister, against
libtpw_unit_CLBScreenComponentOnlineGlobe_Show_2.dylib (tests/unit_CLBScreenComponentOnlineGlobe_Show_2.cpp), main app
("SimTheme Park"), src/game/online/lobby_globe.cpp:
  Show 0x100ad788, Hide 0x100ada68, GlobeZoomIn 0x100adca4, GlobeZoomOut 0x100add2c, SortOutZoomConstants 0x100addb4,
  CreateCities 0x100ade44, UnRegister 0x100ae228

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, 8-byte pointers) for the translation; every field is written and read by
name (FIELDS: the Mac offset, and the host's offset from the library's layout export). The callees outside the unit are fakes on both sides
that record their arguments: the virtuals IsVisible, RegisterGlobe (slot 0x38) and IsCityWanted (slot 0x48) (on the Mac through a vtable of
TVectors whose code addresses are stubbed), SetRaindrops, the lobby view's fog and light calls, SOUND_SendEvent and FadeSoundEffect,
InstanceMeshType, the mesh's Show, the online system's constructor, StopProcessing and GetList, the world lobby sat calls, the two
Delete...ListEntries, DeleteMeshInstanceListEntries and the city flag constructor. UnRegister runs the real list Find and RemoveAt on both sides
and the list is read back whole.

The world of a case: the globe's words, the lobby view, display flags, sound bank word, lobby globals, advisor, the online system (absent or
present), the queues (IsVisible, the two sound handles, which cities are wanted) and the city or flag list. What is compared: the log and every
field of the globe, the view, the display flags, the advisor, the online system pointer's state, the globe object Show makes and the list.

No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CLBScreenComponentOnlineGlobe_Show_2"
MASK = 0xFFFFFFFF
TOC = 0x10267CC0

F_SHOW, F_HIDE, F_ZIN, F_ZOUT, F_SORT, F_CREATE, F_UNREG = 0x100AD788, 0x100ADA68, 0x100ADCA4, 0x100ADD2C, 0x100ADDB4, 0x100ADE44, 0x100AE228
SIZES = {F_SHOW: 676, F_HIDE: 176, F_ZIN: 68, F_ZOUT: 68, F_SORT: 68, F_CREATE: 164, F_UNREG: 84}
OWN = set(SIZES)
ENTRY = {"show": F_SHOW, "hide": F_HIDE, "zin": F_ZIN, "zout": F_ZOUT, "sort": F_SORT, "create": F_CREATE, "unreg": F_UNREG}

# virtual targets (addresses of the globe's own Render, Close and ProcessCancel, never called by this unit): slots 0x2c, 0x38, 0x48
V_ISVISIBLE, V_REGISTER, V_WANTED = 0x100AD4CC, 0x100AD50C, 0x100AD690
CB_MAC = 0x102666A0

ADDR = dict(lobby=0x102E4888, view=0x102E48CC, meshes=0x102E48D0, screens=0x102E49B8, sys=0x10380AB0, rain=0x103BCAA8,
            display=0x103BC8D0, thunder=0x102F7CBC)

IMPORT_SLOTS = dict(display=0x1026067C, rain=0x10261058)

LAYKEYS = ["g_vis", "g_obj", "g_f0c", "g_f1c", "g_f20", "g_f24", "g_f28", "g_f2c", "g_f30", "g_f34", "g_f38", "g_f44", "g_list", "g_f74",
           "g_f78", "g_size", "v_fov", "v_up", "v_right", "v_down", "v_sky", "v_size", "l_globals", "l_size", "lg_radius", "lg_size",
           "s_adv", "s_size", "a_hasto", "a_size", "o_clist", "o_size", "c_mesh", "c_screen", "c_size"]
LAY = {}

# name: (Mac offset, host base key, host addition); the host's offsets come from the layout export
FIELDS = {
    "g_vis": (0x04, "g_vis", 0), "g_obj": (0x0C, "g_obj", 0), "g_f10": (0x10, "g_f0c", 4), "g_f14": (0x14, "g_f0c", 8),
    "g_f18": (0x18, "g_f0c", 12), "g_f1c": (0x1C, "g_f1c", 0), "g_f20": (0x20, "g_f20", 0), "g_f24": (0x24, "g_f24", 0),
    "g_f28": (0x28, "g_f28", 0), "g_f2c": (0x2C, "g_f2c", 0), "g_f30": (0x30, "g_f30", 0), "g_f34": (0x34, "g_f34", 0),
    "g_f38": (0x38, "g_f38", 0), "g_f44": (0x44, "g_f44", 0), "g_f48": (0x48, "g_f44", 4), "g_f4c": (0x4C, "g_f44", 8),
    "g_f50": (0x50, "g_f44", 12), "g_f54": (0x54, "g_f44", 16), "g_zoom": (0x58, "g_f44", 0x14), "g_f68": (0x68, "g_f44", 0x24),
    "g_list": (0x6C, "g_list", 0), "g_f74": (0x74, "g_f74", 0), "g_f78": (0x78, "g_f78", 0),
    "v_fov": (0x04, "v_fov", 0), "v_up": (0x28, "v_up", 0), "v_right": (0x2C, "v_right", 0), "v_down": (0x30, "v_down", 0),
    "v_s0": (0x50, "v_sky", 0), "v_s1": (0x54, "v_sky", 4), "v_s2": (0x58, "v_sky", 8),
    "l_globals": (0x10, "l_globals", 0), "lg_radius": (0x14, "lg_radius", 0), "s_adv": (0x14, "s_adv", 0), "a_hasto": (0x18, "a_hasto", 0),
    "o_clist": (0xE0, "o_clist", 0), "c_mesh": (0x04, "c_mesh", 0), "c_screen": (0x08, "c_screen", 0),
}
PTRS = {"g_obj", "g_list", "l_globals", "s_adv", "o_clist", "c_mesh", "c_screen"}
GLOBE_WORDS = ["g_vis", "g_f10", "g_f14", "g_f18", "g_f1c", "g_f20", "g_f24", "g_f28", "g_f2c", "g_f30", "g_f34", "g_f38", "g_f44", "g_f48",
               "g_f4c", "g_f50", "g_f54", "g_zoom", "g_f68", "g_f74", "g_f78"]
VIEW_WORDS = ["v_fov", "v_up", "v_right", "v_down", "v_s0", "v_s1", "v_s2"]
NAMES = ["globe", "view", "meshes", "rain", "sys0", "clist", "mesh"]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = {}


def reg(addr, name, spec="", ret=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret))


reg(0x10239AF0, "SetRaindrops", "Li")
reg(0x100A9970, "SetFogOff", "L")
reg(0x100A9428, "SetGlobalAmbientLight", "Liu")
reg(0x100DD9FC, "SOUND_SendEvent", "iiiiii", "i")
reg(0x100DE0FC, "SOUND_FadeSoundEffect", "i")
reg(0x100A8408, "InstanceMeshType", "Lssiuuu", lambda e, log: CUR["mesh"])
reg(0x100A7860, "MeshShow", "L")
reg(0x10171404, "StopProcessing", "L")
reg(0x10178718, "GetList", "LLL")
reg(0x1022B6AC, "RemoveWorldLobbyController")
reg(0x1022B85C, "HideWorldLobbySats")
reg(0x1022B7E8, "ShowWorldLobbySats")
reg(0x100ADF48, "DeleteGlobeListEntries", "L")
reg(0x100AE104, "DeleteCityListEntries", "L")
reg(0x100A88A4, "DeleteMeshInstanceListEntries", "L")
reg(0x100AC5C8, "CityFlag", "NLu")
reg(V_ISVISIBLE, "IsVisible", "L", "i")
reg(V_REGISTER, "RegisterGlobe", "LL")
reg(V_WANTED, "IsCityWanted", "Lu", "i")

_ctor = CL.recorder("CFSOnlineSystem", "N")


def stub_ctor(e):
    this = e.r(3)
    _ctor(e)
    e.write(this + 0xE0, struct.pack(">I", CUR["clist"]))


STUBS.internal(0x1016F7D0, stub_ctor)


def stub_dir(e):
    e.cl.rec("SetGlobalDirectionalLight", (e.cl.lab(e.r(3)), s32(e.r(4)), e.u32(e.r(5)) if e.r(5) else -7, e.r(6) & MASK, s32(e.r(7))))


STUBS.internal(0x100A94F8, stub_dir)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def off(self, key):
        mac, hk, add = FIELDS[key]
        return mac if self.mac else LAY[hk] + add

    def put(self, base, key, v):
        a = base + self.off(key)
        if key in PTRS:
            self.wptr(a, v)
        else:
            self.words(a, [v])

    def get(self, base, key):
        a = base + self.off(key)
        return self.rptr(a) if key in PTRS else self.rd_word(a)

    def make_objects(self):
        self.o = dict(view=self.alloc(self.sz("v_size", 0x70)), lobby=self.alloc(self.sz("l_size", 0x38)),
                      lglob=self.alloc(self.sz("lg_size", 0x6C)), screens=self.alloc(self.sz("s_size", 0x1C)),
                      adv=self.alloc(self.sz("a_size", 0x30)), meshes=self.alloc(16), rain=self.alloc(16),
                      sys0=self.alloc(self.sz("o_size", 0x404)), clist=self.alloc(16), mesh=self.alloc(16))

    def name_of(self, p):
        if p == 0:
            return 0
        for k, v in self.o.items():
            if v == p:
                return k
        return "other"


class Ppc(Side):
    fmt = ">I"
    mac = True

    def __init__(self, e, case):
        self.e = e
        self.fixed_cb = CB_MAC
        vt = e.alloc(0x60)
        tvs = [e.alloc(8) for _ in range(3)]
        for tv, code in zip(tvs, (V_ISVISIBLE, V_REGISTER, V_WANTED)):
            e.write(tv, struct.pack(">II", code, TOC))
        for off, tv in zip((0x2C, 0x38, 0x48), tvs):
            e.write(vt + off, struct.pack(">I", tv))
        g = e.alloc(0x80)
        e.write(g, struct.pack(">I", vt))
        self.make_objects()
        self.o["globe"] = g
        CUR["mesh"], CUR["clist"] = self.o["mesh"], self.o["clist"]
        o = self.o
        fixed = {g: 1, o["view"]: 2, o["meshes"]: 3, o["rain"]: 4, o["sys0"]: 5, o["clist"]: 6, o["mesh"]: 7, CB_MAC: 8}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def sz(self, key, dflt):
        return dflt

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        # the two data imports: the code reads through the TOC slot, which the loader filled with the variable's real address
        if name in IMPORT_SLOTS:
            return self.e.u32(IMPORT_SLOTS[name])
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def wptr(self, a, v):
        self.words(a, [v])

    def rptr(self, a):
        return self.rd_word(a)

    def log(self):
        return self.L.log

    def fakes(self):
        pass

    def mklist(self, kind, ids):
        lst = self.alloc(0x10)
        prev = 0
        nodes = []
        for i in ids:
            n = self.alloc(12)
            self.words(n, [i, 0, prev])
            if prev:
                self.words(prev + 4, [n])
            prev = n
            nodes.append(n)
        self.words(lst, [0x1234, nodes[0] if nodes else 0, nodes[-1] if nodes else 0, len(ids)])
        return lst

    def dump(self, lst):
        count, head, tail = self.rd_word(lst + 0xC), self.rd_word(lst + 4), self.rd_word(lst + 8)
        out = [count, self.rd_word(tail) if tail else 0]
        n = head
        while n and len(out) < 32:
            out.append(self.rd_word(n))
            n = self.rd_word(n + 4)
        return out

    def call(self, op, args):
        T.call_ppc(self.e, ENTRY[op], [a & MASK for a in args])


class Host(Side):
    fmt = "<I"
    mac = False

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_objects()
        self.o["globe"] = lib.tpw_g_newglobe()
        o = self.o
        self.cb = lib.tpw_g_addr(b"cb")
        CL.host_begin(lib, {1: o["globe"], 2: o["view"], 3: o["meshes"], 4: o["rain"], 5: o["sys0"], 6: o["clist"], 7: o["mesh"],
                            8: self.cb}, case["queues"])

    def sz(self, key, dflt):
        return LAY[key]

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_g_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_g_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def wptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def fakes(self):
        self.wptr(self.g("fakemesh"), self.o["mesh"])
        self.wptr(self.g("fakecitylist"), self.o["clist"])

    def mklist(self, kind, ids):
        lst = self.lib.tpw_g_newlist(kind)
        for i in ids:
            self.lib.tpw_g_add(kind, lst, i)
        return lst

    def dump(self, lst):
        out = (C.c_int64 * 40)()
        n = self.lib.tpw_g_dumpflags(lst, out)
        return list(out)[:2 + n]

    def call(self, op, args):
        getattr(self.lib, "tpw_g_" + op)(*args)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for n in ("addr", "alloc", "newglobe", "newlist", "dumpflags"):
        getattr(lib, "tpw_g_" + n).restype = C.c_uint64 if n != "dumpflags" else C.c_int32
    lib.tpw_g_addr.argtypes = [C.c_char_p]
    lib.tpw_g_alloc.argtypes = [C.c_uint32]
    lib.tpw_g_newlist.argtypes = [C.c_int32]
    lib.tpw_g_add.argtypes = [C.c_int32, C.c_uint64, C.c_uint64]
    lib.tpw_g_dumpflags.argtypes = [C.c_uint64, C.POINTER(C.c_int64)]
    lib.tpw_g_layout.argtypes = [C.POINTER(C.c_int64)]
    for op in ("show", "hide", "zin", "zout", "sort"):
        getattr(lib, "tpw_g_" + op).argtypes = [C.c_uint64]
    lib.tpw_g_create.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_g_unreg.argtypes = [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 64)()
    n = lib.tpw_g_layout(out)
    assert n == len(LAYKEYS), n
    LAY.update(zip(LAYKEYS, list(out)[:n]))
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o = s.o
    s.fakes()
    for name, key in (("lobby", "lobby"), ("view", "view"), ("meshes", "meshes"), ("screens", "screens"), ("rain", "rain")):
        s.wptr(s.g(name), o[key])
    s.words(s.g("display"), [case["display"]])
    s.words(s.g("thunder"), [case["thunder"]])
    s.put(o["lobby"], "l_globals", o["lglob"])
    s.put(o["lglob"], "lg_radius", case["radius"])
    s.put(o["screens"], "s_adv", o["adv"])
    s.put(o["adv"], "a_hasto", case["hasto"])
    for k in VIEW_WORDS:
        s.put(o["view"], k, case[k])
    if case["sys"]:
        s.wptr(s.g("sys"), o["sys0"])
        s.put(o["sys0"], "o_clist", o["clist"])
    else:
        s.wptr(s.g("sys"), 0)
    g = o["globe"]
    for k in GLOBE_WORDS:
        s.put(g, k, case[k])
    if case["op"] == "unreg":
        s.lst = s.mklist(1, case["ids"])
        s.put(g, "g_list", s.lst)
    elif case["op"] == "create":
        s.lst = s.mklist(0, case["ids"])


def execute(s, case):
    g = s.o["globe"]
    if case["op"] == "create":
        s.call("create", [g, s.lst])
    elif case["op"] == "unreg":
        s.call("unreg", [g, case["arg"]])
    else:
        s.call(case["op"], [g])


def observe(s, case):
    o = s.o
    g = o["globe"]
    r = {"log": s.log()}
    for k in GLOBE_WORDS:
        r[k] = s.get(g, k)
    r["g_f0c_obj"] = None
    obj = s.get(g, "g_obj")
    if obj == 0 or obj in o.values():
        r["obj"] = s.name_of(obj)
    else:
        r["obj"] = ("new", s.name_of(s.get(obj, "c_mesh")), s.name_of(s.get(obj, "c_screen")))
    for k in VIEW_WORDS:
        r[k] = s.get(o["view"], k)
    r["hasto"] = s.get(o["adv"], "a_hasto")
    r["display"] = s.rd_word(s.g("display"))
    sysp = s.rptr(s.g("sys"))
    if sysp == 0:
        r["sys"] = 0
    elif sysp == o["sys0"]:
        r["sys"] = 1
    else:
        r["sys"] = ("new", s.name_of(s.get(sysp, "o_clist")))
    if case["op"] == "unreg":
        r["list"] = s.dump(s.lst)
    return r


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000]


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL)
    return fb(r.uniform(-300, 300))


def ri(r):
    return r.choice([0, 1, 2, 0xFFFFFFFF, r.randrange(0, 1 << 32)])


def base(r, op):
    c = {"op": op, "display": r.randrange(0, 1 << 32), "thunder": r.randrange(1, 1000), "radius": rf(r), "hasto": ri(r),
         "sys": r.choice([0, 1]), "ids": [], "arg": 0}
    for k in GLOBE_WORDS:
        c[k] = rf(r) if k in ("g_f10", "g_f14", "g_f18", "g_f1c", "g_f20", "g_f24", "g_f28", "g_f2c", "g_f30", "g_f34", "g_f38", "g_f44",
                              "g_f48", "g_f4c", "g_f50", "g_f54") else ri(r)
    for k in VIEW_WORDS:
        c[k] = rf(r) if k in ("v_fov", "v_up", "v_right") else ri(r)
    c["g_vis"] = r.choice([0, 1])
    c["g_zoom"] = r.choice([0, 1, 5, 0x80000000])
    c["queues"] = {"IsVisible": [r.choice([0, 1])], "SOUND_SendEvent": [r.randrange(1, 1 << 20), r.randrange(1, 1 << 20)],
                   "IsCityWanted": [r.choice([0, 1])]}
    return c


def simple_edges(op, seed, with_vis):
    def gen():
        r = random.Random(seed)
        for vis in (0, 1) if with_vis else (0,):
            for sysv in (0, 1):
                for f38 in (None, 0, 0x80000000, 0x7FC00000):
                    for zoom in (0, 1, 7):
                        c = base(r, op)
                        c["queues"]["IsVisible"] = [vis]
                        c["sys"] = sysv
                        c["g_zoom"] = zoom
                        if f38 is not None:
                            c["g_f38"] = f38
                        yield c
    return gen


def simple_random(op):
    return lambda r: base(r, op)


def create_case(r, n=None, wanted=None):
    c = base(r, "create")
    n = r.randrange(0, 7) if n is None else n
    c["ids"] = [0x1000 + 4 * i + r.randrange(0, 3) for i in range(n)]
    c["queues"]["IsCityWanted"] = [r.choice([0, 1]) for _ in range(max(1, n))] if wanted is None else wanted
    return c


def create_edges():
    r = random.Random(11)
    for n in (0, 1, 2, 5):
        for wanted in ([1], [0], [1, 0], [0, 1], [0, 0, 1], [1, 1, 0]):
            yield create_case(r, n, wanted)


def unreg_case(r, n=None, pos=None):
    c = base(r, "unreg")
    n = r.randrange(0, 6) if n is None else n
    c["ids"] = [0x2000 + 0x10 * i for i in range(n)]
    if pos is None:
        pos = r.randrange(-1, n) if n else -1
    c["arg"] = 0x2000 + 0x10 * pos if pos >= 0 else 0x9990 + r.randrange(0, 4)
    return c


def unreg_edges():
    r = random.Random(12)
    for n in (0, 1, 2, 3, 5):
        for pos in range(-1, n):
            yield unreg_case(r, n, pos)


TESTS = [
    FnTest("CLBScreenComponentOnlineGlobe::Show", ppc, host, edge=simple_edges("show", 1, True), rnd=simple_random("show")),
    FnTest("CLBScreenComponentOnlineGlobe::Hide", ppc, host, edge=simple_edges("hide", 2, True), rnd=simple_random("hide")),
    FnTest("CLBScreenComponentOnlineGlobe::GlobeZoomIn", ppc, host, edge=simple_edges("zin", 3, False), rnd=simple_random("zin")),
    FnTest("CLBScreenComponentOnlineGlobe::GlobeZoomOut", ppc, host, edge=simple_edges("zout", 4, False), rnd=simple_random("zout")),
    FnTest("CLBScreenComponentOnlineGlobe::SortOutZoomConstants", ppc, host, edge=simple_edges("sort", 5, False), rnd=simple_random("sort")),
    FnTest("CLBScreenComponentOnlineGlobe::CreateCities", ppc, host, edge=create_edges, rnd=create_case),
    FnTest("CLBScreenComponentOnlineGlobe::UnRegister", ppc, host, edge=unreg_edges, rnd=unreg_case),
]
