"""Unit v5571, the online globe's Tick and WindowMessagesFn, against libtpw_unit_CLBScreenComponentOnlineGlobe_Tick.dylib
(tests/unit_CLBScreenComponentOnlineGlobe_Tick.cpp), main app ("SimTheme Park"), src/game/online/lobby_globe.cpp:
  CLBScreenComponentOnlineGlobe::Tick 0x100acce0, CLBScreenComponentOnlineGlobe::WindowMessagesFn 0x100ad54c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian,
4-byte pointers) for the original, and in the host library's memory (little-endian, 8-byte pointers, host offsets from tpw_og_layout) for the
translation. Callees outside the unit are fakes on both sides: CMatrix::TransformVertex (logs its input, answers the ray point from the case),
CLBLobby::IsKeyDown, UpdateOnlineCityInfoOverlay (logs the city's number or -1), the six Globe moves and WorldLobbySetZoomButton.
sqrt, sin and cos are the C library on both sides.

The world of a case: the lobby (size, step), its globals (radii), the view, the display ratio, the RMB-scroll setting, the sphere centre
and view target, the globe's floats and a list of city flags. What is compared: the globe's floats and selected flag, the view's position,
target and light offsets, and the event log.

Fused operations: the whole of Tick (HardwareRounding over both functions). Not tested: NaN or infinite inputs (the processors make
different NaNs through fused operations), a NULL city list or lobby (read unconditionally).
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CLBScreenComponentOnlineGlobe_Tick"
MASK = 0xFFFFFFFF

FN_TICK, FN_WMF = 0x100acce0, 0x100ad54c
SIZES = {FN_TICK: 1968, FN_WMF: 252}
OWN = set(SIZES)

TRANSFORM = 0x1023A960
ISKEY = 0x100A66B0
UPDATE = 0x101E1F64
ZOOMBTN = 0x1022BA30
GLOBE_FNS = {"GlobeLeft": 0x100ADB54, "GlobeRight": 0x100ADBA8, "GlobeUp": 0x100ADBFC, "GlobeDown": 0x100ADC50,
             "GlobeZoomIn": 0x100ADCA4, "GlobeZoomOut": 0x100ADD2C}

ADDR = dict(lobby=0x102E4888, view=0x102E48CC, matrix=0x103BCA98, display=0x103BC8D0, settings=0x1038071C,
            centre=0x1029E71C, target=0x1029E728)

# the layout names in the order of tpw_og_layout, with the Mac's offsets
NAMES = [
    ("lobby.tick", 0x08), ("lobby.globals", 0x10), ("lobby.w", 0x20), ("lobby.h", 0x24), ("lobby.size", 0x40),
    ("glob.in", 0x18), ("glob.out", 0x14), ("glob.size", 0x6C),
    ("view.pos", 0x08), ("view.tgt", 0x14), ("view.up", 0x28), ("view.right", 0x2C), ("view.down", 0x30), ("view.size", 0x64),
    ("g.scale", 0x10), ("g.damp", 0x18), ("g.size", 0x34), ("g.spinx", 0x44), ("g.spinxs", 0x48), ("g.spiny", 0x4C),
    ("g.spinys", 0x50), ("g.radius", 0x54), ("g.zoom", 0x58), ("g.hx", 0x5C), ("g.hy", 0x60), ("g.hz", 0x64),
    ("g.pick", 0x68), ("g.sel", 0x70), ("g.flags", 0x6C), ("g.f38", 0x38), ("g.f3c", 0x3C), ("g.f40", 0x40), ("g.total", 0x7C),
    ("list.head", 0x04), ("list.size", 0x10), ("node.next", 0x04), ("node.size", 0x0C),
    ("flag.city", 0x0C), ("flag.size", 0x10),
    ("city.value", 0x40), ("city.x", 0x34), ("city.y", 0x38), ("city.z", 0x3C), ("city.size", 0x68),
    ("disp.f04", 0x04), ("set.rmb", 0x38),
]
MAC_OFF = dict(NAMES)
SLOTS = {"display": 0x1026067C, "matrix": 0x10260880}
HOST_OFF = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_transform(e):
    """CMatrix::TransformVertex(void* out, void* in, int type): logs the three input floats and the type, writes the case's point."""
    L = e.cl
    inp = struct.unpack(">3I", e.read(e.r(5), 12))
    L.rec("TransformVertex", tuple(canon_f(b) for b in inp) + (T.s32(e.r(6)),))
    out = (L.ret("tvx") & MASK, L.ret("tvy") & MASK, L.ret("tvz") & MASK)
    e.write(e.r(4), struct.pack(">3I", *out))


def stub_iskey(e):
    e.cl.rec("IsKeyDown", (T.s32(e.r(4)),))
    e.set_r(3, e.cl.ret("IsKeyDown") & MASK)


def stub_update(e):
    city = e.r(3)
    e.cl.rec("Update", (-1 if city == 0 else T.s32(e.s32(city + MAC_OFF["city.value"])),))


def stub_zoombtn(e):
    e.cl.rec("ZoomButton", (T.s32(e.r(3)),))


def make_globe_stub(name):
    def stub(e):
        e.cl.rec(name, ())
    return stub


STUBS.imp("TransformVertex__7CMatrixFPvPvi", stub_transform)
STUBS.internal(ISKEY, stub_iskey)
STUBS.internal(UPDATE, stub_update)
STUBS.internal(ZOOMBTN, stub_zoombtn)
for _n, _a in GLOBE_FNS.items():
    STUBS.internal(_a, make_globe_stub(_n))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def o(self, key):
        return self.offs[key]


class Ppc(Side):
    fmt = ">I"
    offs = MAC_OFF

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        # the two engine data imports sit wherever the loader put them: ask the program's TOC slots
        if name in SLOTS:
            return self.e.u32(SLOTS[name])
        return ADDR[name]

    def ptr(self, a, v):
        self.write(a, struct.pack(">I", v))

    def rd_ptr(self, a):
        return struct.unpack(">I", self.read(a, 4))[0]

    def sel(self, a, v):
        self.ptr(a, v)

    def rd_sel(self, a):
        return self.rd_ptr(a)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        with ROUNDING(self.e):
            T.call_ppc(self.e, fn, args)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.offs = HOST_OFF
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_og_addr(name.encode())

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    # mF70, the selected flag, is a 32-bit arena offset on the host (lobby.h)
    def sel(self, a, v):
        self.write(a, struct.pack("<I", self.lib.tpw_og_arena_offset(v)))

    def rd_sel(self, a):
        return self.lib.tpw_og_arena_ptr(struct.unpack("<I", self.read(a, 4))[0])

    def alloc(self, n):
        return self.lib.tpw_og_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        if fn == FN_TICK:
            self.lib.tpw_og_tick(args[0])
        else:
            self.lib.tpw_og_wmf(args[0], T.s32(args[1]), T.s32(args[2]), T.s32(args[3]))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_og_addr.restype, lib.tpw_og_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_og_alloc.restype, lib.tpw_og_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_og_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_og_tick.restype, lib.tpw_og_tick.argtypes = None, [C.c_uint64]
    lib.tpw_og_wmf.restype, lib.tpw_og_wmf.argtypes = None, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_og_arena_ptr.restype, lib.tpw_og_arena_ptr.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_og_arena_offset.restype, lib.tpw_og_arena_offset.argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 64)()
    n = lib.tpw_og_layout(out)
    assert n == len(NAMES), n
    for (k, _), v in zip(NAMES, list(out)[:n]):
        HOST_OFF[k] = v
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

GLOBE_FIELDS = ["g.size", "g.spinx", "g.spinxs", "g.spiny", "g.spinys", "g.radius", "g.hx", "g.hy", "g.hz", "g.scale", "g.damp"]


def build(s, case):
    cities, flags, nodes = [], [], []
    for i, (x, y, z) in enumerate(case["cities"]):
        city = s.alloc(s.o("city.size"))
        s.words(city + s.o("city.value"), [100 + i])
        s.words(city + s.o("city.x"), [x, y, z])
        flag = s.alloc(s.o("flag.size"))
        s.ptr(flag + s.o("flag.city"), city)
        cities.append(city)
        flags.append(flag)
        nodes.append(s.alloc(s.o("node.size")))
    for i, node in enumerate(nodes):
        s.ptr(node, flags[i])
        if i + 1 < len(nodes):
            s.ptr(node + s.o("node.next"), nodes[i + 1])
    lst = s.alloc(s.o("list.size"))
    if nodes:
        s.ptr(lst + s.o("list.head"), nodes[0])

    glob = s.alloc(s.o("glob.size"))
    s.words(glob + s.o("glob.in"), [case["radius_in"]])
    s.words(glob + s.o("glob.out"), [case["radius_out"]])
    lobby = s.alloc(s.o("lobby.size"))
    s.words(lobby + s.o("lobby.tick"), [case["tick"]])
    s.ptr(lobby + s.o("lobby.globals"), glob)
    s.words(lobby + s.o("lobby.w"), [case["w"]])
    s.words(lobby + s.o("lobby.h"), [case["h"]])
    view = s.alloc(s.o("view.size"))
    s.words(view + s.o("view.pos"), case["eye"])
    s.words(view + s.o("view.tgt"), [0x11111111, 0x22222222, 0x33333333])
    s.words(view + s.o("view.up"), [0x44444444, 0x55555555, 0x66666666])
    mat = s.alloc(0x40)

    s.ptr(s.g("lobby"), lobby)
    s.ptr(s.g("view"), view)
    s.ptr(s.g("matrix"), mat)
    s.words(s.g("display") + s.o("disp.f04"), [case["ratio"]])
    s.write(s.g("settings") + s.o("set.rmb"), bytes([case["rmb"]]))
    s.words(s.g("centre"), case["centre"])
    s.words(s.g("target"), case["target"])

    g = s.alloc(s.o("g.total"))
    for key, val in case["globe"].items():
        s.words(g + s.o(key), [val])
    s.ptr(g + s.o("g.flags"), lst)
    s.sel(g + s.o("g.sel"), flags[case["sel0"]] if flags and case["sel0"] is not None else 0)
    return {"g": g, "view": view, "flags": flags}


def execute(s, case, w):
    if case["op"] == "tick":
        s.call(FN_TICK, [w["g"]])
    else:
        s.call(FN_WMF, [w["g"], case["msg"] & MASK, case["a"] & MASK, case["b"] & MASK])


def observe(s, case, w):
    g, view = w["g"], w["view"]
    sel = s.rd_sel(g + s.o("g.sel"))
    sel_i = -1 if sel == 0 else (w["flags"].index(sel) if sel in w["flags"] else -2)
    return {
        "log": s.log(),
        "globe": [canon_f(s.rd_word(g + s.o(k))) for k in GLOBE_FIELDS],
        "zoom": s.rd_word(g + s.o("g.zoom")),
        "sel": sel_i,
        "pos": [canon_f(b) for b in s.rd_words(view + s.o("view.pos"), 3)],
        "tgt": [canon_f(b) for b in s.rd_words(view + s.o("view.tgt"), 3)],
        "light": [canon_f(b) for b in s.rd_words(view + s.o("view.up"), 3)],
    }


def run(s, case):
    try:
        w = build(s, case)
        execute(s, case, w)
        return observe(s, case, w)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def f3(v):
    return [fb(x) for x in v]


def ray_hit(eye, out, centre):
    """The ray-sphere hit point in double precision (to put cities near it), or None."""
    e = [bitsf(b) for b in eye]
    o = [bitsf(b) for b in out]
    c = [bitsf(b) for b in centre]
    d = [o[i] - e[i] for i in range(3)]
    n = math.sqrt(sum(x * x for x in d)) or 1.0
    d = [x / n for x in d]
    t = [c[i] - e[i] for i in range(3)]
    proj = sum(d[i] * t[i] for i in range(3))
    perp = sum((t[i] - d[i] * proj) ** 2 for i in range(3))
    if perp >= 13456.0:
        return None
    dist = proj - math.sqrt(13456.0 - perp)
    if dist < 0.001:
        dist = proj + math.sqrt(13456.0 - perp)
    return [e[i] + dist * d[i] for i in range(3)]


def tick_case(r, **kw):
    c = {"op": "tick", "msg": 0, "a": 0, "b": 0}
    centre = [r.uniform(-20, 20) for _ in range(3)]
    ang = r.uniform(0, 6.28)
    el = r.uniform(-1.2, 1.2)
    rad = r.uniform(150, 500)
    eye = [centre[0] + rad * math.cos(ang) * math.cos(el), centre[1] + rad * math.sin(el), centre[2] + rad * math.sin(ang) * math.cos(el)]
    aim = [centre[i] + r.uniform(-1, 1) * r.choice([40, 110, 200]) for i in range(3)]
    k = r.uniform(0.02, 0.3)
    out = [eye[i] + k * (aim[i] - eye[i]) for i in range(3)]
    c["eye"], c["centre"], c["out"] = f3(eye), f3(centre), f3(out)
    c["target"] = f3([r.uniform(-30, 30) for _ in range(3)])
    hit = ray_hit(c["eye"], c["out"], c["centre"])
    cities = []
    for _ in range(r.choice([0, 1, 2, 3, 5])):
        if hit is not None and r.random() < 0.7:
            p = [hit[i] + r.uniform(-12, 12) for i in range(3)]
        else:
            p = [centre[i] + r.uniform(-130, 130) for i in range(3)]
        cities.append(tuple(f3(p)))
    c["cities"] = cities
    c["sel0"] = r.choice([None, 0]) if cities else None
    c["w"] = r.choice([r.randrange(0, 1100), 1024, 0, 2048, 2060, 40])
    c["h"] = r.choice([r.randrange(0, 800), 768, 0, 1536, 1540, 10])
    c["ratio"] = fb(r.uniform(0.5, 1.0))
    c["tick"] = fb(r.uniform(0.01, 0.6))
    c["radius_in"] = fb(r.uniform(150, 300))
    c["radius_out"] = fb(r.uniform(300, 600))
    c["rmb"] = r.choice([0, 1])
    spin = r.choice([0.5, 5, 30])
    c["globe"] = {
        "g.scale": fb(r.uniform(0.1, 2.0)), "g.damp": fb(r.uniform(0.85, 1.0)), "g.size": fb(r.uniform(100, 400)),
        "g.spinx": fb(r.uniform(-7, 7)), "g.spinxs": fb(r.uniform(-spin, spin)), "g.spiny": fb(r.uniform(-1.6, 1.6)),
        "g.spinys": fb(r.uniform(-spin, spin)), "g.radius": fb(r.uniform(100, 600)), "g.zoom": r.choice([0, 1]),
        "g.hx": fb(r.uniform(-5, 5)), "g.hy": fb(r.uniform(-5, 5)), "g.hz": fb(r.uniform(-5, 5)),
        "g.pick": r.choice([0, 1, 1]), "g.f38": fb(r.choice([300.0, r.uniform(100, 400)])), "g.f3c": fb(r.choice([150.0, r.uniform(50, 300)])),
        "g.f40": fb(r.choice([1.5, r.uniform(0.5, 1.6)])),
    }
    c["queues"] = {"IsKeyDown": [r.choice([0, 1, 5])], "tvx": [c["out"][0]], "tvy": [c["out"][1]], "tvz": [c["out"][2]]}
    for key, val in kw.items():
        if key in c["globe"] or key.startswith("g."):
            c["globe"][key] = val
        else:
            c[key] = val
    return c


def tick_edges():
    r = random.Random(571)
    for pick in (0, 1):
        for rmb in (0, 1):
            for key in (0, 1):
                for zoom in (0, 1):
                    c = tick_case(r, rmb=rmb)
                    c["globe"]["g.pick"], c["globe"]["g.zoom"] = pick, zoom
                    c["queues"]["IsKeyDown"] = [key]
                    yield c
    for w in (0, 20, 21, 1024, 2027, 2028, 2048, 3000):
        for h in (0, 15, 16, 768, 1520, 1521, 1536, 2000):
            c = tick_case(r, rmb=0, w=w, h=h)
            yield c
    for w, h in ((0, 0), (1024, 768), (2048, 1536), (1030, 700), (500, 1600)):
        yield tick_case(r, rmb=1, w=w, h=h)
    # the pitch clamps
    for sy, sys_ in ((1.49, 50.0), (-1.49, -50.0), (1.5, 0.0), (-1.5, 0.0)):
        yield tick_case(r, **{"g.spiny": fb(sy), "g.spinys": fb(sys_), "g.f40": fb(1.5)})


def tick_random(r):
    return tick_case(r)


def wmf_case(r, msg=None, a=None):
    c = tick_case(r)
    c["op"] = "wmf"
    c["msg"] = r.choice([0x1000A, 0x10006, 0x10005, 0, 0x1000B]) if msg is None else msg
    c["a"] = r.choice([0x2500, 0x2600, 0x2700, 0x2800, 2, 0x2400, 0x2900, 1, 0]) if a is None else a
    c["b"] = r.randrange(-3, 3)
    return c


def wmf_edges():
    r = random.Random(572)
    for msg in (0x1000A, 0x10006, 0x10005, 0x10009, 0x1000B, 0):
        for a in (0x2500, 0x2600, 0x2700, 0x2800, 0x2501, 0x24FF, 0x2601, 0x2701, 0x2801, 2, 1, 3, 0):
            for zoom in (0, 1):
                c = wmf_case(r, msg, a)
                c["globe"]["g.zoom"] = zoom
                yield c


def wmf_random(r):
    return wmf_case(r)


TESTS = [
    FnTest("CLBScreenComponentOnlineGlobe::Tick", ppc, host, edge=tick_edges, rnd=tick_random),
    FnTest("CLBScreenComponentOnlineGlobe::WindowMessagesFn", ppc, host, edge=wmf_edges, rnd=wmf_random),
]
