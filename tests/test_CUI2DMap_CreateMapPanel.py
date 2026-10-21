"""Unit v5986, the map panel's opening, against libtpw_unit_CUI2DMap_CreateMapPanel.dylib (tests/unit_CUI2DMap_CreateMapPanel.cpp), main app
("SimTheme Park"), src/game/ui/ui_2dmap.cpp:
  CUI2DMap::CreateMapPanel 0x1022c080

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the menu test, the pause, the fog and render options, the window calls, the layer and filter buttons, the heightfield size, the
map's other methods, the map zoom, the advisor and the help text) are fakes on both sides that record their arguments; the virtual Enable
(slot 0x1c of the table at window +0x118, through __ptr_glue) is faked too and also records whether the slot held the expected function.

The world of a case: the map object's header words, its sprite words, the camera target, the heightfield size, the game and display
words, the windows (the depth of window 0x990) and the answers of the fakes. What is compared: the event log, the map's words and halves the
function reads or writes, the game's word at +0x3c and the display's fog colour.

No fused operations (fmuls only; fctiwz is given values far below 2^31). Not tested: a NULL window from GetWindow (the original reads it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CUI2DMap_CreateMapPanel"
MASK = 0xFFFFFFFF

FN = 0x1022c080

ADDR = dict(map=0x1039742c, game=0x1037f134, display=0x103bc8d0, mapzoom=0x103bc948, script=0x102b2366, cbfn=0x102689f0,
            areacb=0x102689e8, v_gamew=0x103949e4, v_root=0x10395c24, v_mapwho=0x103bc908, v_tcache=0x103bc9c0,
            camera=0x102b5bd8, hfw=0x102c6c68, hfh=0x102c6c64)
WIN_IDS = [0x990, 0x991, 0x995, 0x982, 0x987, 0x989, 0x993, 0x994, 0x983, 0x98b, 0x996, 0x98a, 0x984, 0x981, 0x988, 0x992]
OBJ_NAMES = ["root", "gamew", "panel", "font", "mapwho", "tcache"] + ["w%x" % i for i in WIN_IDS]
LABELS = {n: i + 1 for i, n in enumerate(OBJ_NAMES)}
GLOBAL_LABELS = {"game": 23, "display": 24, "mapzoom": 25, "map": 26, "script": 27, "cbfn": 28, "areacb": 29}
HOST_LAYOUT = [360, 96, 0x8bc, 0x20130]   # InterfaceWindow::mZ and CGame::mF3C sit elsewhere on the host (Mac 0xdc and 0x3c)
OBJ_SIZE = 0x200
MAPW = [0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x44, 0x48, 0x4c, 0x50, 0x54, 0x58, 0x5c]

PPC_OBJS = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.add(FN)


def glue(fn):
    """What the glue does before it calls on: the caller's TOC goes to 0x14(r1), where the code reads it back."""
    def wrapper(e):
        e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapper


def mk(name, kinds, ret=False):
    def fn(e):
        L = e.cl
        vals = []
        for k, kind in enumerate(kinds):
            v = e.r(3 + k)
            vals.append(L.lab(v) if kind == "L" else (s32(v) if kind == "i" else v & MASK))
        L.rec(name, tuple(vals))
        if ret:
            e.set_r(3, L.ret(name) & MASK)
    return fn


def stub(addr, name, kinds, ret=False):
    STUBS.internal(addr, glue(mk(name, kinds, ret)))


def stub_getwindow(e):
    L = e.cl
    wid = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), wid))
    e.set_r(3, PPC_OBJS["w%x" % wid])


def stub_build(e):
    L = e.cl
    L.rec("Build", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))
    e.set_r(3, PPC_OBJS["panel"])


def stub_new(e):
    e.cl.rec("new", (e.r(3) & MASK,))
    e.set_r(3, e.alloc(e.r(3)))


def stub_font(e):
    e.cl.rec("GetFontTableEntry", (e.r(3) & MASK,))
    e.set_r(3, PPC_OBJS["font"])


def stub_ptr_glue(e):
    L = e.cl
    L.rec("Enable", (L.lab(e.r(3)), T.s16(e.r(4)), 1 if e.r(12) == PPC_OBJS["desc"] else 0))


def stub_hfield(e):
    L = e.cl
    a, b = e.r(3), e.r(4)
    L.rec("GetHFieldMaxVertexIndices", (s32(a - ADDR["map"]), s32(b - ADDR["map"])))
    e.write(a, struct.pack(">I", L.ret("HFieldW") & MASK))
    e.write(b, struct.pack(">I", L.ret("HFieldH") & MASK))


stub(0x101fc1ec, "MainMenuUp", "", True)
stub(0x10149904, "Pause", "Lii")
stub(0x101d8b64, "UI_PauseTimerSystem", "")
stub(0x10008f00, "ADV_Kill", "i")
stub(0x1023a570, "SetFogColor", "Lu")
stub(0x10239808, "GetRenderOptions", "L", True)
stub(0x10239820, "SetRenderOptions", "Li")
stub(0x10239838, "Flush", "Li")
stub(0x10180514, "UIMain_CheckPanel", "")
stub(0x101c9518, "UI_SendMessage", "Liii")
STUBS.internal(0x101deb78, glue(stub_build))
stub(0x101805a0, "UIMain_SetPanel", "L")
stub(0x101dccec, "CaptureKb", "Li")
STUBS.internal(0x101dbe40, glue(stub_getwindow))
STUBS.internal(0x102384b8, glue(stub_new))
STUBS.internal(0x1018d11c, glue(CL.recorder("TextSpriteCtor", "Nu")))
stub(0x101db624, "SetClientDrawObject", "LL")
stub(0x101da204, "SetFontJust", "Lii")
stub(0x101da2d8, "SetFontWrap", "Lii")
STUBS.internal(0x10180270, glue(stub_font))
stub(0x101da118, "SetFont", "LL")
stub(0x101da3ac, "SetFontColour", "Liiii")
stub(0x101de130, "SetClientDrawText", "LL")
STUBS.internal(0x1023baa8, glue(stub_ptr_glue))
stub(0x101dd8bc, "setZ", "Li")
stub(0x101cb1d0, "SetState", "Lii")
stub(0x101cc148, "SetCurrent", "Li")
stub(0x101cb4d4, "SetAutoRepeat", "Lii")
stub(0x101dcba8, "SetCallbackFunction", "LL")
STUBS.internal(0x1005c250, glue(stub_hfield))
stub(0x1022de40, "StoreMapGridDetails", "L")
stub(0x1022fad0, "InitSprites", "L")
stub(0x1022dbdc, "LoadMap", "Li")
stub(0x1022db4c, "UpdateMap", "L")
stub(0x1023afd8, "MapZoomInit", "Luiii")
stub(0x1023aff0, "DisableFiltering", "L")
stub(0x101821cc, "UIMain_AdvisorMessage", "i")
stub(0x1018c7a4, "MoveHelpText", "ii")


def setup(runner):
    T.install_std_stubs(runner.emu)
    # the engine's data imports: the TOC slot holds the address the loader gave the import
    for name, slot in (("display", 0x1026067c), ("mapwho_unused", 0x102607e4), ("mapzoom", 0x10261e6c), ("tcache_unused", 0x10260674)):
        if not name.endswith("_unused"):
            ADDR[name] = runner.emu.u32(slot)
    ADDR["v_mapwho"] = runner.emu.u32(0x102607e4)
    ADDR["v_tcache"] = runner.emu.u32(0x10260674)
    for n in OBJ_NAMES:
        PPC_OBJS[n] = T.persist_alloc(runner.emu, OBJ_SIZE)
    PPC_OBJS["vt"] = T.persist_alloc(runner.emu, 0x40)
    PPC_OBJS["desc"] = T.persist_alloc(runner.emu, 8)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"

    def ptr(self, a, v):
        self.write(a, struct.pack(">I", v & MASK))

    zoff = 0xdc
    f3coff = 0x3c

    def __init__(self, e, case):
        self.e = e
        self.objs = PPC_OBJS
        fixed = {PPC_OBJS[n]: LABELS[n] for n in OBJ_NAMES}
        fixed.update({ADDR[n]: l for n, l in GLOBAL_LABELS.items()})
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def create(self):
        with_r3 = T.call_ppc(self.e, FN, [ADDR["map"]])
        return with_r3[0] & MASK


class Host(Side):
    fmt = "<I"

    def ptr(self, a, v):
        """A native pointer: eight bytes on the host."""
        self.write(a, struct.pack("<Q", v))

    zoff = 360
    f3coff = 96

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        if not hasattr(lib, "objs"):
            lib.objs = {n: lib.tpw_cmp_alloc(OBJ_SIZE) for n in OBJ_NAMES}
            lib.tpw_cmp_set(0, lib.objs["panel"])
            lib.tpw_cmp_set(1, lib.objs["font"])
            for i, wid in enumerate(WIN_IDS):
                lib.tpw_cmp_set(2 + i, lib.objs["w%x" % wid])
        self.objs = lib.objs
        fixed = {LABELS[n]: lib.objs[n] for n in OBJ_NAMES}
        fixed.update({l: self.g(n) for n, l in GLOBAL_LABELS.items()})
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cmp_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def create(self):
        self.lib.tpw_cmp_create()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cmp_addr.restype, lib.tpw_cmp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cmp_alloc.restype, lib.tpw_cmp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cmp_set.restype, lib.tpw_cmp_set.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_cmp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cmp_create.restype, lib.tpw_cmp_create.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_cmp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o = s.objs
    s.ptr(s.g("v_root"), o["root"])
    s.ptr(s.g("v_gamew"), o["gamew"])
    s.ptr(s.g("v_mapwho"), o["mapwho"])
    s.ptr(s.g("v_tcache"), o["tcache"])
    s.words(s.g("camera"), case["cam"])
    s.words(s.g("hfw"), [case["hfw"]])
    s.words(s.g("hfh"), [case["hfh"]])
    s.words(s.g("game") + s.f3coff, [case["f3c"]])
    s.words(s.g("display") + 0x8bc, [case["fog"]])
    for wid in WIN_IDS:
        a = o["w%x" % wid]
        s.half(a + s.zoff, case["z"][wid])
        if s.fmt == ">I":
            s.words(a + 0x118, [o["vt"]])
    if s.fmt == ">I":
        s.words(o["vt"] + 0x1c, [o["desc"]])
    m = s.g("map")
    for off, w in zip(MAPW, case["mapw"]):
        s.words(m + off, [w])
    s.words(m + 0x20060, [case["p0"]])
    s.half(m + 0x20064, case["h0"])
    s.half(m + 0x20066, case["h1"])
    s.words(m + 0x20068, case["p1"])
    s.words(m + 0x20084, [case["p2"]])
    s.words(m + 0x20088, [case["p3"]])
    if s.fmt == "<I":
        # the host map's render area (CUI2DMap::HostArea at 0x20100) holds the data pointer (+0x18) and the row bytes (+0x28) that
        # the Mac's has at 0x44 and 0x50
        s.write(m + 0x20100 + 0x18, struct.pack("<Q", case["mapw"][MAPW.index(0x44)]))
        s.words(m + 0x20100 + 0x28, [case["mapw"][MAPW.index(0x50)]])


def observe(s, case, answer):
    m = s.g("map")
    panel = s.objs["panel"] & MASK
    words = []
    for off in MAPW:
        w = s.rd_word(m + off)
        words.append(-1 if off == 4 and w == panel else w)
    return {
        "log": s.log(),
        "words": words,
        "w60": s.rd_word(m + 0x20060),
        "h64": s.rd_half(m + 0x20064),
        "h66": s.rd_half(m + 0x20066),
        "p1": s.rd_words(m + 0x20068, 7),
        "p2": s.rd_word(m + 0x20084),
        "p3": s.rd_word(m + 0x20088),
        "f3c": s.rd_word(s.g("game") + s.f3coff),
        "fog": s.rd_word(s.g("display") + 0x8bc),
    }


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.create())
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def base(r):
    mapw = [r.getrandbits(32) for _ in MAPW]
    mapw[1] = 0                                              # the panel is not up
    mapw[MAPW.index(0x18)] = r.choice([r.getrandbits(7), r.getrandbits(32), 0, 0x3f, 0x7f])
    mapw[MAPW.index(0x1c)] = r.choice([-1, 0, 1, 2, 3, 4, 5, 6, r.randrange(-5, 10)]) & MASK
    mapw[MAPW.index(0x20)] = r.choice([-1, 0, 1, 2, 3, 4, 5, r.randrange(-5, 10)]) & MASK
    k = r.randrange(0, 400)
    cam = [fb(10.0 * k + r.choice([-0.5, 0.0, 0.01, 0.4, 5.0, 9.99])), fb(r.uniform(-100, 100)), fb(10.0 * r.randrange(-3, 400) + r.choice([0.0, 0.01, 7.0]))]
    hfw = r.choice([r.randrange(1, 300), 128, 1, 0, 256])
    hfh = r.choice([r.randrange(1, 300), 128, 1, 0, 256])
    return {
        "op": "create",
        "queues": {
            "MainMenuUp": [r.choice([0, 0, 0, 0, 1, 7])],
            "GetRenderOptions": [r.getrandbits(32)],
            "HFieldW": [r.choice([hfw, hfw - 1, hfw + 1, r.randrange(0, 300), k, k + 1, 0, -1, 0x7fffffff]) & MASK],
            "HFieldH": [r.choice([hfh, hfh - 1, hfh + 1, r.randrange(0, 300), 0, -1, 40, 200]) & MASK],
        },
        "cam": cam, "hfw": hfw, "hfh": hfh,
        "f3c": r.choice([0, 1, r.getrandbits(32)]), "fog": r.getrandbits(32),
        "z": {wid: r.choice([0, 1, 5, 0x100, 0x7fff, 0x8000 - 3, 0xfffe, r.getrandbits(16)]) for wid in WIN_IDS},
        "mapw": mapw,
        "p0": r.getrandbits(32), "h0": r.getrandbits(16), "h1": r.getrandbits(16), "p1": [r.getrandbits(32) for _ in range(7)],
        "p2": r.getrandbits(32), "p3": r.getrandbits(32),
    }


def edges():
    r = random.Random(5986)
    # the filters and the layer flags on every value
    for v in range(-2, 8):
        c = base(r)
        c["queues"]["MainMenuUp"] = [0]
        c["mapw"][MAPW.index(0x1c)] = v & MASK
        c["mapw"][MAPW.index(0x20)] = (v + 1) & MASK
        yield c
        c = base(r)
        c["queues"]["MainMenuUp"] = [0]
        c["mapw"][MAPW.index(0x1c)] = (v + 3) & MASK
        c["mapw"][MAPW.index(0x20)] = v & MASK
        yield c
    for flags in [0, 1, 2, 4, 8, 0x10, 0x20, 0x3f, 0x40, 0xffffffc0, 0xffffffff]:
        c = base(r)
        c["queues"]["MainMenuUp"] = [0]
        c["mapw"][MAPW.index(0x18)] = flags
        yield c
    # the panel up already, the menu up
    c = base(r)
    c["mapw"][1] = 0x1234
    c["queues"]["MainMenuUp"] = [0]
    yield c
    c = base(r)
    c["queues"]["MainMenuUp"] = [-1]
    yield c
    # the camera and the heightfield: negative, zero, on the limit, past it
    for x, z, hfw, hfh, W, H in [(-5.0, -5.0, 128, 128, 100, 100), (0.0, 0.0, 128, 128, 0, 0), (1270.0, 1270.0, 128, 128, 128, 128),
                                 (1279.9, 1280.0, 128, 128, 127, 128), (1290.0, 1500.0, 128, 128, 128, 128), (500.0, 600.0, 50, 60, 100, 100),
                                 (500.0, 600.0, 100, 100, 50, 60), (500.0, 600.0, 51, 61, 50, 60), (500.0, 600.0, 50, 60, 50, 60),
                                 (509.0, 609.0, 100, 100, 50, 60), (-0.01, -0.01, 5, 5, 5, 5), (0.05, 0.05, 1, 1, 1, 1),
                                 (500.0, 600.0, 100, 100, 51, 61), (500.0, 600.0, 100, 100, 49, 59), (500.0, 600.0, 100, 100, -3, -3),
                                 (123456.0, 7654321.0, 128, 128, 128, 128), (-123456.0, -7654321.0, 128, 128, 128, 128)]:
        c = base(r)
        c["queues"]["MainMenuUp"] = [0]
        c["cam"] = [fb(x), fb(3.0), fb(z)]
        c["hfw"], c["hfh"] = hfw, hfh
        c["queues"]["HFieldW"], c["queues"]["HFieldH"] = [W & MASK], [H & MASK]
        yield c


def rnd(r):
    return base(r)


TESTS = [
    FnTest("CUI2DMap::CreateMapPanel", ppc, host, edge=edges, rnd=rnd),
]
