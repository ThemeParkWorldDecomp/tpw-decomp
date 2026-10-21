"""The engine's frame (src/engine/Gamut.cpp, src/engine/MapWho.cpp and its helpers): CMapWho::Render,
RenderWindow, PrepareWorldspace, ALTTAB_RestoreState, the render option accessors,
CGamut::GetQuads, CMeshDelay's Reset and Prepare, CLoadSurface::Display and the three FPU
switches, against the original. Every callee outside the unit is a fake on both sides
(tests/unit_MapRender.cpp on the host) that records its arguments and answers from the
case's queues; the virtual calls go through fake vtables laid out as the original's (the
draw primitive's at +0x2c, the renderer's, the particle object's at +0xa0, the links' and
the meshes' at +0x150), the function pointers through fake transition vectors. A case sets
up the objects, runs one function, and compares the call log and every field the unit may
change. CMapWho::Render is void in the translation (its callers ignore the result): the
PowerPC side checks its result against the path its log shows instead."""
import ctypes as C
import math
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_MapRender"
MASK = 0xFFFFFFFF

# The unit (run as the original on the PowerPC side).
RENDER, RENDER_WINDOW, PREPARE_WORLDSPACE, ALTTAB = 0x10018D70, 0x10018470, 0x100182BC, 0x100194A8
SET_OPTIONS, GET_OPTIONS, GET_QUADS = 0x10017AD8, 0x10017B10, 0x1000BC40
FPU_SINGLE, FPU_RESTORE, FPU_FULL = 0x10038B60, 0x10038B98, 0x10038BC8
DELAY_RESET, DELAY_PREPARE, LOAD_DISPLAY = 0x1001CA64, 0x1001CA98, 0x10017270
UNIT = [RENDER, RENDER_WINDOW, PREPARE_WORLDSPACE, ALTTAB, SET_OPTIONS, GET_OPTIONS, GET_QUADS,
        FPU_SINGLE, FPU_RESTORE, FPU_FULL, DELAY_RESET, DELAY_PREPARE, LOAD_DISPLAY]

# Engine data the unit and the fakes use.
SYSTEM, DISPLAY = 0x10165F80, 0x10165634
INTERNAL_VIEW, VIEW_PTR, MAPWHO_PTR = 0x100AFFF8, 0x100B0078, 0x100AF618
HFIELD_PTR, GAMUT_PTR, TCACHE_PTR = 0x100AF11C, 0x10052DB8, 0x10173184
RENDERLIST_PTR, SPRITES_PTR, RENDERMEMORY_PTR = 0x1016562C, 0x10173154, 0x10165630
DRAWPRIM_PTR, PARTICLE_PTR, RAIN_PTR = 0x10052B3C, 0x100B0508, 0x100B1628
SKY, MAPZOOM, STARFIELD, SHADOW = 0x101661EC, 0x100AFE20, 0x10173158, 0x10166018
SPLASH, ERRORS, CAPTURE = 0x10169110, 0x10052B44, 0x10052A40
DDRAW_PTR, DD_RESULT, LIGHTS_PTR = 0x10052AD0, 0x10052B40, 0x100AF1B8
IS_FIRST_PERSON, UPDATE_MOUSE = 0x101735CC, 0x101735C0
MESH_DELAY, ENTRY_FLAGS = 0x100B0100, 0x100B0504
FRAME_MODE, LAST_VIEW, LAST_VIEW_MADE = 0x10051C0C, 0x100AF5D4, 0x100AF614
SAVED = [(SYSTEM, 0x48), (DISPLAY, 0x93C), (INTERNAL_VIEW, 0x84), (MAPWHO_PTR, 4), (HFIELD_PTR, 4),
         (GAMUT_PTR, 4), (TCACHE_PTR, 4), (RENDERLIST_PTR, 4), (SPRITES_PTR, 4), (RENDERMEMORY_PTR, 4),
         (DRAWPRIM_PTR, 4), (PARTICLE_PTR, 4), (RAIN_PTR, 4), (DDRAW_PTR, 4), (DD_RESULT, 4),
         (LIGHTS_PTR, 4), (IS_FIRST_PERSON, 4), (UPDATE_MOUSE, 4), (MESH_DELAY, 0x408),
         (FRAME_MODE, 4), (LAST_VIEW, 0x41)]

# Fixed labels, as in tests/unit_MapRender.cpp.
(L_MAP, L_OTHER_MAP, L_DISPLAY, L_SYSTEM, L_HFIELD, L_GAMUT, L_TCACHE, L_RENDERLIST, L_SPRITES,
 L_RENDERMEMORY, L_DRAWPRIM, L_PARTICLE, L_RAIN, L_SKY, L_MAPZOOM, L_STARFIELD, L_SHADOW, L_SPLASH,
 L_ERRORS, L_CAPTURE, L_DDRAW, L_RENDER_SYSTEM, L_LOAD_SURFACE, L_SURFACE, L_BACK, L_MESH_DELAY,
 L_LIGHTS) = range(1, 28)
L_FIRST_MESH = 30
NODES, HEADS, LINKS, MESHES = 96, 43, 8, 8

# The value every return queue starts with (a case overrides some).
QUEUES = {"ClearViewport": [1], "RSTempShutdown": [1], "RSReInitialize": [1], "DPInit": [1], "Fog": [1],
          "BeginDraw": [1], "EndDraw": [1], "FilledBox": [1], "EdgeBox": [1], "HFPrepare": [1],
          "HFExecute": [1], "SkyShadow": [1], "TCExecute": [1], "TCClose": [1], "TCReInitialize": [1],
          "RLRender": [1], "SMRender": [1], "PlotParticles": [1], "RainRender": [1], "SkyRender": [1],
          "ZoomExecute": [1], "StarExecute": [1], "DDTempShutdown": [1], "DDReInitialize": [1],
          "AltTabStatus": [0], "Blt": [0], "FrontWindow": [1], "IsFirstPerson": [0]}


# Render's code: its eight fmadds (the stereo eyes and windows) round once, as the processor's.
RENDER_CODE = (0x10018D70, 0x10019480)
LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


class HardwareRounding:
    """A code hook over Render that replaces each fmadds result with fmaf's: Unicorn computes
    fmadds as a double fused multiply-add rounded again to single (FINDINGS 2026-10-08), the
    processor rounds once (FINDINGS 2026-10-10, tests/test_NodeGamuts.py does the same)."""

    def __init__(self, e):
        self.e, self.pending = e, None
        self.hook = e.uc.hook_add(UC_HOOK_CODE, self.step, begin=RENDER_CODE[0], end=RENDER_CODE[1] - 1)

    def close(self):
        self.e.uc.hook_del(self.hook)

    def fpr(self, i):
        v = self.e.uc.reg_read(ppcemu.FPR[i])
        bits = v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]
        return struct.unpack(">d", struct.pack(">Q", bits))[0]

    def step(self, uc, address, size, user):
        if self.pending is not None:
            target, value = self.pending
            self.pending = None
            uc.reg_write(ppcemu.FPR[target], struct.unpack(">Q", struct.pack(">d", value))[0])
        word = struct.unpack(">I", self.e.read(address, 4))[0]
        if word >> 26 == 59 and (word >> 1) & 31 == 29:   # fmadds frD, frA, frC, frB
            a, c, b = self.fpr((word >> 16) & 31), self.fpr((word >> 6) & 31), self.fpr((word >> 11) & 31)
            if all(math.isfinite(x) for x in (a, c, b)):
                self.pending = ((word >> 21) & 31, float(LIBM.fmaf(a, c, b)))


def ws(e, a, values):
    e.write(a, struct.pack(">" + "I" * len(values), *[v & MASK for v in values]))


def rs(e, a, n):
    return list(struct.unpack(">" + "I" * n, e.read(a, n * 4)))


def canon(w):
    """A NaN's bit pattern as one value (fused multiply-adds propagate them differently)."""
    return 0x7FC00000 if w & 0x7F800000 == 0x7F800000 and w & 0x007FFFFF else w


def canon_log(log):
    """The events whose words are computed floats, with their NaNs as one value."""
    out = []
    for name, ints, texts in log:
        if name in ("ViewA", "ViewB"):
            ints = tuple(canon(v & MASK) for v in ints)
        elif name == "SetWindow":
            ints = (ints[0],) + tuple(canon(v & MASK) for v in ints[1:5]) + tuple(ints[5:])
        out.append((name, ints, texts))
    return out

# ----------------------------------------------------------------- PowerPC side


def method(name, spec="L", ret=None):
    """A recorder for a fake member function; ret "i" answers from the queue `name`, 1 a
    constant 1 (the unit ignores those results)."""
    return CL.recorder(name, spec, ret=(lambda e, L: 1) if ret == 1 else ret)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if hasattr(e, "mr_vtables"):
        return
    fake = lambda fn: T.fake_function(e, fn)

    def vtable(slots, size=0x40):
        vt = T.persist_alloc(e, size)
        for off, fn in slots.items():
            ws(e, vt + off, [fake(fn)])
        return vt
    e.mr_vtables = dict(
        draw=vtable({0x0C: method("DPInit", ret="i"), 0x20: method("Fog", "Lffu", ret="i"),
                     0x24: method("BeginDraw", ret="i"), 0x28: method("EndDraw", ret="i"),
                     0x30: method("FilledBox", "LffffiL", ret="i"), 0x38: method("EdgeBox", ret="i")}),
        renderer=vtable({0x08: method("ClearViewport", ret="i"), 0x1C: method("RSTempShutdown", ret="i"),
                         0x20: method("RSReInitialize", ret="i")}),
        particle=vtable({0x14: method("ParticlePrepare", ret=1)}),
        link=vtable({0x0C: lambda em: em.mr.link_prepare(em), 0x10: lambda em: em.mr.link_type(em),
                     0x14: lambda em: em.mr.link_data(em)}),
        mesh=vtable({0x14: lambda em: em.mr.mesh_prepare(em)}))
    e.mr_functions = dict(first_person=fake(method("IsFirstPerson", "", ret="i")),
                          mouse=fake(method("UpdateMouse", "")),
                          callback=fake(lambda em: em.mr.final_callback(em)))


class Side:
    """One case on the PowerPC side: the objects, the fakes' answers and the state."""

    def __init__(self, e, c):
        self.e, self.c = e, c
        e.mr = self
        self.saved = [(a, e.read(a, n)) for a, n in SAVED]
        vt, fn = e.mr_vtables, e.mr_functions
        self.map = e.alloc(0x7C, b"\xa5" * 0x7C)
        self.other = e.alloc(0x7C, b"\xa5" * 0x7C) if c["separate"] else self.map
        self.nodes = e.alloc(NODES * 8)
        self.cells = e.alloc(36 * 4)
        self.links = [e.alloc(8) for _ in range(LINKS)]
        self.link_ids = {a: i + 1 for i, a in enumerate(self.links)}
        self.data = [e.alloc(12) for _ in range(LINKS)]
        self.windows = [e.alloc(0x18) for _ in range(4)]
        self.window_matrices = [e.alloc(64) for _ in range(2)]
        self.view = e.alloc(64)
        self.hfield = e.alloc(0x230)
        self.gamut = e.alloc(0x2D8)
        small = {name: e.alloc(16) for name in ("tcache", "renderlist", "sprites", "rendermemory", "rain",
                                                "lights", "load", "surface", "back", "port", "port2")}
        self.small = small
        self.draw = e.alloc(0x40)
        self.particle = e.alloc(0xAC)
        self.renderer = e.alloc(0x10)
        self.ddraw = e.alloc(0x20)
        self.meshes = [e.alloc(0x15C) for _ in range(MESHES)]
        # the objects and their vtables
        ws(e, self.draw + 0x2C, [vt["draw"]])
        ws(e, self.particle + 0xA0, [vt["particle"]])
        ws(e, self.renderer, [vt["renderer"]])
        for i, link in enumerate(self.links):
            ws(e, link, [vt["link"]])
            ws(e, self.data[i], list(c["links"][i][2]))
        for i, mesh in enumerate(self.meshes):
            ws(e, mesh + 4, [c["mesh_flags"][i]])
            ws(e, mesh + 0x150, [vt["mesh"]])
        ws(e, small["load"], [small["surface"]])
        ws(e, self.ddraw + 0x14, [small["back"]])
        ws(e, small["lights"] + 4, [c["light_mask"]])
        # the lists
        node = lambda i: 0 if i < 0 else self.nodes + 8 * i
        for i in range(NODES):
            link = c["node_link"][i]
            ws(e, self.nodes + 8 * i, [self.links[link - 1] if link > 0 else 0, node(c["node_next"][i])])
        ws(e, self.cells, [node(h) for h in c["heads"][7:]])
        # the map
        m = self.map
        ws(e, m + 0x10, [self.cells])
        ws(e, m + 0x14, [node(h) for h in c["heads"][:7]])
        ws(e, m + 0x30, [node(c["f30"])])
        ws(e, m + 0x34, [small["load"] if c["load_surface"] else 0])
        ws(e, m + 0x38, [c["map_w"], c["map_h"], c["f40"]])
        quads = {0: 0, 1: self.gamut + 0xC4, 2: self.hfield + 0x128}[c["gamut"]]
        ws(e, m + 0x44, [quads, c["game_flags"]])
        ws(e, m + 0x60, self.windows)
        ws(e, m + 0x70, [c["window_count"], c["f74"], fn["callback"] if c["callback"] else 0])
        if self.other != m:
            ws(e, self.other + 0x30, [node(c["global_f30"])])
        for i, (flags, x, y, w, h, which) in enumerate(c["windows"]):
            matrix = self.window_matrices[which] if which < 2 else INTERNAL_VIEW + 64 * (which - 2)
            ws(e, self.windows[i], [flags, x, y, w, h, matrix])
        for q, base in enumerate((self.gamut + 0xC4, self.hfield + 0x128)):
            ws(e, base, c["rows"][q])
            e.write(base + 8, bytes(v & 0xFF for v in c["low"][q]) + bytes(v & 0xFF for v in c["high"][q]))
        ws(e, self.window_matrices[0], c["window_matrices"][:16])
        ws(e, self.window_matrices[1], c["window_matrices"][16:])
        ws(e, self.view, c["view"])
        # the engine's globals
        d = c["display"]
        ws(e, DISPLAY, [d["flags"]])
        ws(e, DISPLAY + 8, [d["width"], d["height"]])
        ws(e, DISPLAY + 0x80, [d["frame"], d["f84"]])
        ws(e, DISPLAY + 0x8B8, [d["sep"], d["fog_color"]])
        ws(e, DISPLAY + 0x8D0, [d["fog_near"], d["fog_far"]])
        ws(e, DISPLAY + 0x924, [d["fade"]])
        ws(e, DISPLAY + 0x938, [d["f938"]])
        ws(e, SYSTEM, [c["system_flags"]])
        ws(e, SYSTEM + 0x10, [small["port"]])
        ws(e, SYSTEM + 0x1C, [c["renderer"]])
        ws(e, SYSTEM + 0x24, [self.renderer])
        ws(e, INTERNAL_VIEW, c["internal"])
        ws(e, VIEW_PTR, [self.view])
        ws(e, MAPWHO_PTR, [self.other])
        for a, v in ((HFIELD_PTR, self.hfield), (GAMUT_PTR, self.gamut), (TCACHE_PTR, small["tcache"]),
                     (RENDERLIST_PTR, small["renderlist"]), (SPRITES_PTR, small["sprites"]),
                     (RENDERMEMORY_PTR, small["rendermemory"]), (DRAWPRIM_PTR, self.draw),
                     (PARTICLE_PTR, self.particle), (RAIN_PTR, small["rain"]), (DDRAW_PTR, self.ddraw),
                     (DD_RESULT, c["dd_result"]), (LIGHTS_PTR, small["lights"]),
                     (IS_FIRST_PERSON, fn["first_person"]), (UPDATE_MOUSE, fn["mouse"]),
                     (ENTRY_FLAGS, c["entry_flags"]), (FRAME_MODE, c["frame_mode"])):
            ws(e, a, [v])
        ws(e, MESH_DELAY, [self.meshes[i] for i in c["delay"]])
        ws(e, MESH_DELAY + 0x400, [c["delay_count"]])
        ws(e, LAST_VIEW, c["last_view"])
        e.write(LAST_VIEW_MADE, bytes([c["last_view_made"] & 0xFF]))
        self.delay_adds = [c["queues"].get(f"MeshAdd:{i}", [-1])[0] for i in range(MESHES)]
        labels = {m: L_MAP, DISPLAY: L_DISPLAY, SYSTEM: L_SYSTEM, self.hfield: L_HFIELD, self.gamut: L_GAMUT,
                  small["tcache"]: L_TCACHE, small["renderlist"]: L_RENDERLIST, small["sprites"]: L_SPRITES,
                  small["rendermemory"]: L_RENDERMEMORY, self.draw: L_DRAWPRIM, self.particle: L_PARTICLE,
                  small["rain"]: L_RAIN, SKY: L_SKY, MAPZOOM: L_MAPZOOM, STARFIELD: L_STARFIELD,
                  SHADOW: L_SHADOW, SPLASH: L_SPLASH, ERRORS: L_ERRORS, CAPTURE: L_CAPTURE, self.ddraw: L_DDRAW,
                  self.renderer: L_RENDER_SYSTEM, small["load"]: L_LOAD_SURFACE, small["surface"]: L_SURFACE,
                  small["back"]: L_BACK, MESH_DELAY: L_MESH_DELAY, small["lights"]: L_LIGHTS}
        if self.other != m:
            labels[self.other] = L_OTHER_MAP
        labels.update({mesh: L_FIRST_MESH + i for i, mesh in enumerate(self.meshes)})
        e.cl = self.log = CL.PpcLog(labels, {**QUEUES, **c["queues"]})
        self.stubs = self.make_stubs()
        self.old = self.stubs.activate(e)
        self.rounding = HardwareRounding(e)

    def make_stubs(self):
        st = CL.Stubs()
        st.real.update(UNIT)
        for addr, fn in {
                0x1001A8C8: self.preprocess, 0x1002806C: self.set_window, 0x10028200: method("ResetWindow"),
                0x1000E144: method("DivideAndConquer", ret=1), 0x1000ED18: self.hfield_prepare,
                0x1000F69C: method("HFExecute", ret="i"), 0x10011180: method("SkyShadow", ret="i"),
                0x10009F04: method("CalculateGamut", ret=1), 0x10034204: method("TCRefresh", ret=1),
                0x10033688: method("TCExecute", ret="i"), 0x10034514: method("TCClose", ret="i"),
                0x1003447C: method("TCReInitialize", "Li", ret="i"), 0x10026478: method("RLReset"),
                0x10026954: method("RLRender", ret="i"), 0x10031770: method("SMReset"),
                0x100319C8: method("SMPrepare", ret=1), 0x100317E4: method("SMRender", ret="i"),
                0x10026C34: method("RMReset", "Li"), 0x1001F754: method("PlotParticles", ret="i"),
                0x10021554: method("RainPrepare", ret=1), 0x10023F40: method("RainRender", ret="i"),
                0x1002B5B4: method("SkyPrepare", ret=1), 0x1002BB04: method("SkyRender", ret="i"),
                0x10019F94: method("ZoomPrepare", ret=1), 0x1001A0C0: method("ZoomExecute", ret="i"),
                0x1003219C: method("StarPrepare", ret=1), 0x10032200: method("StarExecute", ret="i"),
                0x100293CC: method("ResetLights"), 0x1002940C: method("AddLight", "Luuu", ret=1),
                0x1002D344: method("SplashClose", ret=1), 0x10008904: method("Push", "Lii"),
                0x100089BC: method("AltTabStatus", ret="i"), 0x100287D8: self.update_lights,
                0x10028AA4: method("Flush", "", ret=1), 0x100288A0: method("RSClose", "", ret=1),
                0x100032D0: self.capture, 0x10004DFC: method("DDTempShutdown", ret="i"),
                0x10004E7C: method("DDReInitialize", ret="i"), 0x10004CC0: method("DDClose", ret=1)}.items():
            st.internal(addr, fn)
        st.imp("FrontWindow", self.front_window)
        st.imp("memcmp", self.memcmp)
        st.imp("__assertion_failed", CL.recorder("Assert", "ssi"))
        st.imp("Blt__19IDirectDrawSurface4FPQ313SamsUtilities10URectangle8LongRectP19IDirectDrawSurface4"
               "PQ313SamsUtilities10URectangle8LongRectUlP8_DDBLTFX", CL.recorder("Blt", "LLLLuL", ret="i"))
        return st

    # -- fakes that record state as well
    def preprocess(self, e):
        w = rs(e, INTERNAL_VIEW, 16)
        self.log.rec("ViewA", w[:8])
        self.log.rec("ViewB", w[8:])
        e.set_r(3, 1)

    def set_window(self, e):
        floats = [CL.fbits(e.f(i)) for i in (1, 2, 3, 4)]
        lights = e.u32(LIGHTS_PTR)
        self.log.rec("SetWindow", [self.log.lab(e.r(3))] + floats +
                     [e.u32(DISPLAY), e.u32(SYSTEM), T.s32(e.u32(lights + 4))])

    def hfield_prepare(self, e):
        self.log.rec("HFPrepare", [self.log.lab(e.r(3)), e.u32(ENTRY_FLAGS)])
        e.set_r(3, T.u32(self.log.ret("HFPrepare")))

    def update_lights(self, e):
        self.log.rec("UpdateLights", [self.log.lab(e.r(3)), e.u32(SYSTEM)])

    def capture(self, e):
        self.log.rec("Capture", [self.log.lab(e.r(3)), e.u32(SYSTEM), e.u32(DISPLAY)])
        e.set_r(3, 1)

    def front_window(self, e):
        self.log.rec("FrontWindow")
        e.set_r(3, self.small["port"] if self.log.ret("FrontWindow") else self.small["port2"])

    def memcmp(self, e):
        a, b = e.read(e.r(3), e.r(5)), e.read(e.r(4), e.r(5))
        e.set_r(3, 0 if a == b else (1 if a > b else MASK))

    def node(self, p):
        if p == 0:
            return -1
        return (p - self.nodes) // 8 if self.nodes <= p < self.nodes + NODES * 8 else -2

    def link_prepare(self, e):
        i = self.link_ids[e.r(3)]
        self.log.rec("LinkPrepare", [i, self.node(e.u32(e.u32(MAPWHO_PTR) + 0x30))])
        e.set_r(3, T.u32(self.log.ret(f"LinkPrepare:{i}", 1)))

    def link_type(self, e):
        i = self.link_ids[e.r(3)]
        self.log.rec("LinkType", [i])
        e.set_r(3, T.u32(self.c["links"][i - 1][0]))

    def link_data(self, e):
        i = self.link_ids[e.r(3)]
        self.log.rec("LinkData", [i])
        e.set_r(3, self.data[i - 1] if self.c["links"][i - 1][1] else 0)

    def mesh_prepare(self, e):
        mesh = e.r(3)
        count = e.u32(MESH_DELAY + 0x400)
        self.log.rec("MeshPrepare", [self.log.lab(mesh), e.u32(mesh + 4), T.s32(count)])
        index = self.meshes.index(mesh)
        add = self.delay_adds[index]
        if add >= 0:
            ws(e, MESH_DELAY + 4 * T.s32(count), [self.meshes[add]])
            ws(e, MESH_DELAY + 0x400, [count + 1])
            self.delay_adds[index] = -1
        e.set_r(3, 1)

    def final_callback(self, e):
        self.log.rec("FinalCallback", [T.s32(e.u32(self.map + 0x74))])

    # -- running and reading back
    def run(self, op, arg):
        e, m = self.e, self.map
        args = {1: [m], 2: [m, self.windows[0]], 3: [m], 4: [m], 5: [m, arg], 6: [m], 7: [self.gamut],
                8: [MESH_DELAY], 9: [MESH_DELAY], 10: [self.small["load"]], 11: []}[op]
        entry = {1: RENDER, 2: RENDER_WINDOW, 3: PREPARE_WORLDSPACE, 4: ALTTAB, 5: SET_OPTIONS, 6: GET_OPTIONS,
                 7: GET_QUADS, 8: DELAY_RESET, 9: DELAY_PREPARE, 10: LOAD_DISPLAY, 11: FPU_SINGLE}[op]
        r3 = T.call_ppc(e, entry, [a & MASK for a in args])[0]
        if op == 1:
            return self.render_result_matches(r3)
        if op == 7:
            return {0: 0, self.gamut + 0xC4: 1, self.hfield + 0x128: 2}.get(r3, -1)
        if op == 11:
            T.call_ppc(e, FPU_RESTORE, [arg & MASK])
            T.call_ppc(e, FPU_FULL, [arg & MASK])
            return T.s16(r3)
        return 0 if op in (5, 8) else T.s32(r3)

    def render_result_matches(self, r3):
        """Render's result is what its log says: GetAltTabStatus's answer after a failed
        window, 0 after the alt-tab recovery failed, 1 otherwise. The host's Render is void
        and answers 0 here, so this answers 0 when the result matches."""
        log = self.log.log
        if log and log[-1][0] == "AltTabStatus":
            expected = T.u32(self.log.queues["AltTabStatus"][0])
        elif log and log[-1][0] == "Push" and log[-1][1][2] == T.s32(0xB0754013):
            expected = 0
        else:
            expected = 1
        return 0 if r3 == expected else ("render result", r3, expected)

    def state(self):
        e, m = self.e, self.map
        out = rs(e, INTERNAL_VIEW, 32)
        out += [e.u32(DISPLAY), e.u32(DISPLAY + 0x84), e.u32(DISPLAY + 0x938), e.u32(SYSTEM)]
        out.append({0: 0, self.gamut + 0xC4: 1, self.hfield + 0x128: 2}.get(e.u32(m + 0x44), -1) & MASK)
        out += [self.node(e.u32(m + 0x30)) & MASK, self.node(e.u32(e.u32(MAPWHO_PTR) + 0x30)) & MASK]
        out += [e.u32(m + 0x74), e.u32(m + 0x48), e.u32(MESH_DELAY + 0x400)]
        out += [e.u32(mesh + 4) for mesh in self.meshes]
        out += [e.u32(ENTRY_FLAGS), e.u32(FRAME_MODE), T.s8(e.read(LAST_VIEW_MADE, 1)[0]) & MASK]
        out += rs(e, LAST_VIEW, 16)
        out += [e.u32(self.small["lights"] + 4), e.u32(DD_RESULT)]
        for w in self.windows:
            out += rs(e, w, 5)
        return out

    def close(self):
        self.rounding.close()
        CL.Stubs.deactivate(self.e, self.old)
        for a, data in self.saved:
            self.e.write(a, bytes(data))


def ppc(e, c):
    side = Side(e, c)
    try:
        result = side.run(c["op"], c["arg"])
        return dict(result=result, state=side.state(), log=canon_log(side.log.log))
    finally:
        side.close()

# ----------------------------------------------------------------- host side


class MrWindow(C.Structure):
    _fields_ = [("flags", C.c_uint32), ("x", C.c_uint32), ("y", C.c_uint32), ("width", C.c_uint32),
                ("height", C.c_uint32), ("matrix", C.c_int32)]


class MrLink(C.Structure):
    _fields_ = [("type", C.c_int32), ("hasData", C.c_int32), ("data", C.c_uint32 * 3)]


class MrCase(C.Structure):
    _fields_ = [("displayFlags", C.c_uint32), ("width", C.c_int32), ("height", C.c_int32),
                ("frameCount", C.c_int32), ("f84", C.c_int32), ("separation", C.c_uint32),
                ("fogColor", C.c_uint32), ("fogNear", C.c_uint32), ("fogFar", C.c_uint32), ("fade", C.c_uint32),
                ("f938", C.c_uint32), ("systemFlags", C.c_uint32), ("renderer", C.c_int32),
                ("windowCount", C.c_int32), ("hasLoadSurface", C.c_int32), ("f40", C.c_int32),
                ("f74", C.c_int32), ("hasCallback", C.c_int32), ("separateGlobal", C.c_int32),
                ("mapWidth", C.c_int32), ("mapHeight", C.c_int32), ("f30", C.c_int32), ("globalF30", C.c_int32),
                ("frameMode", C.c_int32), ("lastViewMade", C.c_int32), ("lightMask", C.c_int32),
                ("entryFlags", C.c_uint32), ("gamut", C.c_int32), ("gameWindowFlags", C.c_uint32),
                ("ddResult", C.c_int32), ("meshFlags", C.c_uint32 * MESHES), ("delayCount", C.c_int32),
                ("delay", C.c_int32 * MESHES), ("windows", MrWindow * 4), ("view", C.c_uint32 * 16),
                ("internal", C.c_uint32 * 32), ("lastView", C.c_uint32 * 16), ("windowMatrices", C.c_uint32 * 32),
                ("heads", C.c_int32 * HEADS), ("nodeLink", C.c_int32 * NODES), ("nodeNext", C.c_int32 * NODES),
                ("links", MrLink * LINKS), ("rows", (C.c_int32 * 2) * 2), ("low", (C.c_int8 * 128) * 2),
                ("high", (C.c_int8 * 128) * 2)]


def host_case(c):
    d = c["display"]
    s = MrCase()
    s.displayFlags, s.width, s.height, s.frameCount, s.f84 = d["flags"], d["width"], d["height"], d["frame"], d["f84"]
    s.separation, s.fogColor, s.fogNear, s.fogFar = d["sep"], d["fog_color"], d["fog_near"], d["fog_far"]
    s.fade, s.f938 = d["fade"], d["f938"]
    s.systemFlags, s.renderer, s.windowCount = c["system_flags"], c["renderer"], c["window_count"]
    s.hasLoadSurface, s.f40, s.f74, s.hasCallback = c["load_surface"], c["f40"], c["f74"], c["callback"]
    s.separateGlobal, s.mapWidth, s.mapHeight = c["separate"], c["map_w"], c["map_h"]
    s.f30, s.globalF30, s.frameMode, s.lastViewMade = c["f30"], c["global_f30"], c["frame_mode"], c["last_view_made"]
    s.lightMask, s.entryFlags, s.gamut = T.s32(c["light_mask"]), c["entry_flags"], c["gamut"]
    s.gameWindowFlags, s.ddResult, s.delayCount = c["game_flags"], T.s32(c["dd_result"]), c["delay_count"]
    s.meshFlags[:] = c["mesh_flags"]
    s.delay[:] = c["delay"]
    for i, (flags, x, y, w, h, which) in enumerate(c["windows"]):
        s.windows[i] = MrWindow(flags, x, y, w, h, which)
    s.view[:], s.internal[:], s.lastView[:] = c["view"], c["internal"], c["last_view"]
    s.windowMatrices[:] = c["window_matrices"]
    s.heads[:], s.nodeLink[:], s.nodeNext[:] = c["heads"], c["node_link"], c["node_next"]
    for i, (kind, has, vector) in enumerate(c["links"]):
        s.links[i] = MrLink(T.s32(kind), has, (C.c_uint32 * 3)(*vector))
    for q in range(2):
        s.rows[q][0], s.rows[q][1] = [T.s32(v) for v in c["rows"][q]]
        s.low[q][:] = [T.s8(v) for v in c["low"][q]]
        s.high[q][:] = [T.s8(v) for v in c["high"][q]]
    return s


def host(lib, c):
    CL.host_begin(lib, {}, {**QUEUES, **c["queues"]})
    begin, run, state = lib.tpw_mr_begin, lib.tpw_mr_run, lib.tpw_mr_state
    begin.argtypes, begin.restype = [C.POINTER(MrCase)], None
    run.argtypes, run.restype = [C.c_int32, C.c_int32], C.c_int32
    state.argtypes, state.restype = [C.POINTER(C.c_uint32)], C.c_int32
    s = host_case(c)
    begin(C.byref(s))
    result = run(c["op"], T.s32(c["arg"]))
    log = CL.host_end(lib)
    out = (C.c_uint32 * 256)()
    n = state(out)
    return dict(result=result, state=list(out[:n]), log=canon_log(log))

# ----------------------------------------------------------------- the cases


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


IDENTITY = [fb(1.0) if i % 5 == 0 else 0 for i in range(16)]
GAME_WINDOW = (0x1F31, fb(-1.0), fb(-1.0), fb(2.0), fb(2.0), 2)


def case(op, arg=0, lists=None, **kw):
    """A case: the lobby's frame by default (one game window, the OpenGL renderer, no
    links); kw overrides any field, `lists` gives the 7 global lists and the map cells as
    lists of link ids (1..8)."""
    c = dict(op=op, arg=arg,
             display=dict(flags=0x04000000, width=640, height=480, frame=7, f84=3, sep=fb(1.5),
                          fog_color=0x44DDFFFF, fog_near=fb(50.0), fog_far=fb(300.0), fade=0x80, f938=fb(0.5)),
             system_flags=0x03000000, renderer=3, window_count=1, load_surface=0, f40=0x30, f74=-1, callback=0,
             separate=0, map_w=4, map_h=3, f30=-1, global_f30=-1, frame_mode=-1, last_view_made=0,
             light_mask=-1, entry_flags=0, gamut=0, game_flags=0x1F31, dd_result=0,
             mesh_flags=[0x10000000 + i for i in range(MESHES)], delay_count=0, delay=list(range(MESHES)),
             windows=[GAME_WINDOW, (0x0420, fb(-0.5), fb(0.0), fb(1.0), fb(1.0), 0),
                      (0x1B11, fb(0.25), fb(-0.75), fb(0.5), fb(1.5), 1), (0x7FFF, fb(-1.0), fb(1.0), fb(2.0), fb(-2.0), 3)],
             view=[fb(0.5 + i) for i in range(16)], internal=[fb(100.0 + i) for i in range(32)],
             last_view=[fb(0.5 + i) for i in range(16)], window_matrices=[fb(-10.0 - i) for i in range(32)],
             links=[(1 if i < 3 else i % 4, 1 if i != 2 else 0, (fb(0.1 * i), fb(-1.0), fb(0.25 * i)))
                    for i in range(LINKS)],
             rows=[[0, 0], [0, 0]], low=[[0] * 128, [0] * 128], high=[[0] * 128, [0] * 128], queues={})
    for k, v in kw.items():
        if k == "display":
            c["display"] = {**c["display"], **v}
        else:
            c[k] = v
    set_lists(c, lists or [])
    return c


def set_lists(c, lists):
    """Chains the nodes for the lists (globals 0..6, then cells row by row)."""
    heads, link, nxt = [-1] * HEADS, [0] * NODES, [-1] * NODES
    used = 0
    for h, ids in enumerate(lists):
        previous = None
        for i in ids:
            link[used] = i
            if previous is None:
                heads[h] = used
            else:
                nxt[previous] = used
            previous = used
            used += 1
    c["heads"], c["node_link"], c["node_next"] = heads, link, nxt


def cells(c, rows):
    """Global lists empty and the map cells' lists from rows of lists."""
    return [[] for _ in range(7)] + [cell for row in rows for cell in row]


def quads(c, which, first, end, spans):
    """The quads `which` (0 the gamut's, 1 the height field's): rows first..end - 1, and
    (row, low, high) spans."""
    c["rows"][which] = [first, end]
    for row, low, high in spans:
        c["low"][which][row] = low
        c["high"][which][row] = high

# -- RenderWindow


def window_edges():
    yield case(2)
    for bit in (0, 0x1, 0x10, 0x20, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0xFFFFFFFF):
        yield case(2, windows=[(bit, fb(0.0), fb(0.0), fb(1.0), fb(1.0), 0)] * 4, load_surface=1)
    yield case(2, queues={"ClearViewport": [0]})
    for flags, renderer in ((0x2000, 3), (0, 4), (0x2000, 4), (0, 3)):
        yield case(2, display=dict(flags=flags), renderer=renderer)
    for load in (0, 1):
        for blt in (0, 0x887601C2):
            yield case(2, load_surface=load, queues={"Blt": [blt]})
    yield case(2, queues={"IsFirstPerson": [1]})
    yield case(2, queues={"IsFirstPerson": [-7]}, system_flags=0)
    yield case(2, system_flags=0x100)
    for flags in (0, 0x40000000, 0x80000000, 0xC0000000):
        yield case(2, system_flags=flags)
    yield case(2, queues={"BeginDraw": [0]})
    yield case(2, queues={"HFPrepare": [0]})
    lists6 = [[1], [2, 3], [], [4], [5, 6, 7], [8], [1, 2]]
    yield case(2, lists=lists6)
    for f40 in (1, -1, 0x31):
        yield case(2, lists=lists6, f40=f40)
    for k in range(1, 9):
        yield case(2, lists=lists6, queues={f"LinkPrepare:{k}": [0]})
    yield case(2, windows=[(0x20, fb(0.0), fb(0.0), fb(1.0), fb(1.0), 0)] * 4, lists=lists6, separate=1,
               global_f30=5, f30=3)
    for f74 in (7, 6):
        yield case(2, callback=1, f74=f74)
    yield case(2, callback=1, f74=7, display=dict(frame=-1))
    for key in ("TCExecute", "SkyRender", "HFExecute", "PlotParticles", "SMRender", "RainRender", "RLRender"):
        yield case(2, queues={key: [0]})
    for shadow in (0, 1):
        yield case(2, display=dict(flags=0x10000), queues={"SkyShadow": [shadow]})
    for key in ("ZoomExecute", "StarExecute"):
        yield case(2, windows=[(0x6000, fb(0.0), fb(0.0), fb(1.0), fb(1.0), 0)] * 4, queues={key: [0]})
    lights = [[]] * 6 + [[1, 4, 3, 2, 5, 1]]
    yield case(2, lists=lights)
    yield case(2, lists=lights, display=dict(flags=0x20000))
    yield case(2, lists=lights, separate=1, global_f30=0)
    yield case(2, windows=[(0x800, fb(0.0), fb(0.0), fb(1.0), fb(1.0), 0)] * 4)
    for renderer in (-1, 0, 1, 2, 3, 4, 5):
        for flags in (0, 2, 0x8000000, 0x10000000, 0x80000, 0x80002):
            yield case(2, renderer=renderer, display=dict(flags=flags, fade=0x123 + renderer))
    yield case(2, queues={"EndDraw": [0]})
    for q in ({"BeginDraw": [1, 0]}, {"FilledBox": [0]}, {"EndDraw": [1, 0]}, {"EndDraw": [1, 1, 0]}):
        yield case(2, display=dict(flags=0x80000, width=-3, height=0x7FFFFFFF), queues=q)
    yield case(2, display=dict(flags=0x80000, width=0x1000001, height=-0x80000000))
    for which in range(4):
        yield case(2, windows=[(0x1F31, fb(0.0), fb(0.0), fb(1.0), fb(1.0), which)] * 4)
    yield case(2, display=dict(fog_near=fb(float("nan")), fog_far=fb(-0.0), fog_color=0xFFFFFFFF))


def rflags(r, bits):
    return sum(b for b in bits if r.random() < 0.4)


def rqueues(r, keys, p=0.06):
    q = {}
    for k in keys:
        if r.random() < p:
            q[k] = [r.choice([0, 1]) for _ in range(r.randrange(1, 4))]
    return q


def rlists(r, n, ids=8, longest=4):
    return [[r.randrange(1, ids + 1) for _ in range(r.randrange(longest))] for _ in range(n)]


def rfloat(r):
    return r.choice([fb(r.uniform(-4, 4)), fb(r.uniform(-1e6, 1e6)), fb(0.0), fb(-0.0), fb(float("inf")),
                     fb(float("nan")), r.getrandbits(32)])


def rlinks(r):
    return [(r.choice([0, 1, 1, 2, 3, -1]), r.randrange(2), tuple(rfloat(r) for _ in range(3))) for _ in range(LINKS)]


WINDOW_BITS = (0x1, 0x10, 0x20, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000, 0x2, 0x80000000)
DISPLAY_BITS = (0x2, 0x4, 0x8, 0x400, 0x2000, 0x10000, 0x20000, 0x80000, 0x8000000, 0x10000000, 0x80000000)
SYSTEM_BITS = (0x1, 0x2, 0x40, 0x100, 0x10000000, 0x40000000, 0x80000000)
WINDOW_KEYS = ("ClearViewport", "BeginDraw", "EndDraw", "FilledBox", "HFPrepare", "HFExecute", "SkyShadow",
               "TCExecute", "RLRender", "SMRender", "PlotParticles", "RainRender", "SkyRender", "ZoomExecute",
               "StarExecute", "IsFirstPerson") + tuple(f"LinkPrepare:{k}" for k in range(1, 9))


def rwindow(r):
    return (rflags(r, WINDOW_BITS), rfloat(r), rfloat(r), rfloat(r), rfloat(r), r.randrange(4))


def window_random(r):
    c = case(2, windows=[rwindow(r) for _ in range(4)], lists=rlists(r, 7), separate=r.randrange(2),
             renderer=r.choice([-1, 0, 1, 2, 3, 3, 3, 4, 5]), system_flags=rflags(r, SYSTEM_BITS),
             display=dict(flags=rflags(r, DISPLAY_BITS), width=r.randrange(-2, 4000), height=r.randrange(-2, 4000),
                          frame=r.randrange(4), fade=r.getrandbits(32), fog_near=rfloat(r), fog_far=rfloat(r),
                          fog_color=r.getrandbits(32)),
             callback=r.randrange(2), f74=r.randrange(4), load_surface=r.randrange(2), links=rlinks(r),
             queues=rqueues(r, WINDOW_KEYS + ("Blt",)), internal=[r.getrandbits(32) for _ in range(32)],
             window_matrices=[r.getrandbits(32) for _ in range(32)], f30=r.randrange(-1, 3),
             global_f30=r.randrange(-1, 3), f40=r.choice([0x30, 0x31, 0, 1, T.s32(r.getrandbits(32))]))
    if r.random() < 0.3:
        c["windows"][0] = (c["windows"][0][0] | 0x1F31,) + c["windows"][0][1:]
    return c

# -- Render


def render_edges():
    yield case(1)
    yield case(1, system_flags=0x40)
    for flags in (0x2400, 0x2000, 0x400):
        yield case(1, display=dict(flags=flags))
    yield case(1, queues={"FrontWindow": [0]}, system_flags=0x40, display=dict(flags=0x2400))
    yield case(1, system_flags=0x1)
    for renderer in (3, 2):
        yield case(1, system_flags=0x10000000, renderer=renderer)
    yield case(1, system_flags=0x10000001, queues={"TCClose": [0]})
    # one view
    for count in (0, 1, 2, 4):
        for mode in (-1, 1, 2):
            yield case(1, frame_mode=mode, window_count=count)
    for made in (0, 1, -1):
        for same in (0, 1):
            for flags in (0, 0x40000000, 0x80000000, 0xC0000000):
                last = IDENTITY if (made == 0) == bool(same) else [fb(0.5 + i) for i in range(16)]
                view = IDENTITY if same and made == 0 else [fb(0.5 + i) for i in range(16)]
                if not same:
                    view = view[:15] + [fb(-0.0)]
                yield case(1, frame_mode=1, window_count=1, last_view_made=made, system_flags=flags,
                           view=view, last_view=last)
    yield case(1, frame_mode=1, view=[fb(0.0)] * 16, last_view=[fb(-0.0)] * 16, last_view_made=1)
    for alt in (0, 1, 5):
        yield case(1, window_count=2, queues={"BeginDraw": [0], "AltTabStatus": [alt]})
        yield case(1, window_count=3, queues={"BeginDraw": [1, 1, 0], "AltTabStatus": [alt]})
    yield case(1, system_flags=0x2)
    yield case(1, window_count=2, system_flags=0x2, queues={"EndDraw": [1, 1, 0]})
    # stereo
    for flags in (0x4, 0xC, 0x1000C, 0x10004, 0x80004):
        for mode in (-1, 2, 3):
            for count in (0, 1, 2):
                yield case(1, display=dict(flags=flags), frame_mode=mode, window_count=count)
    for flags, calls in ((0x4, 2), (0xC, 3)):
        for fail in range(calls):
            yield case(1, display=dict(flags=flags), window_count=2,
                       queues={"BeginDraw": [1] * fail + [0], "AltTabStatus": [fail]})
    for sep in (0.0, -0.0, -3.25, 1e30, float("inf"), float("nan")):
        yield case(1, display=dict(flags=0x4, sep=fb(sep)), window_count=1)
        yield case(1, display=dict(flags=0xC, sep=fb(sep)), window_count=1)
    specials = (fb(float("nan")), fb(float("inf")), fb(-0.0), fb(-1.0), fb(3e38))
    yield case(1, display=dict(flags=0x4), window_count=4,
               windows=[(0x1F31, specials[i], specials[(i + 1) % 5], specials[(i + 2) % 5],
                         specials[(i + 3) % 5], i) for i in range(4)])


def render_random(r):
    stereo = r.random() < 0.5
    flags = rflags(r, DISPLAY_BITS)
    if stereo:
        flags |= 4
    view = [r.getrandbits(32) for _ in range(16)]
    last = view if r.random() < 0.4 else [r.getrandbits(32) for _ in range(16)]
    return case(1, display=dict(flags=flags, sep=rfloat(r), frame=r.randrange(3), fade=r.getrandbits(8)),
                system_flags=rflags(r, SYSTEM_BITS), renderer=r.choice([0, 1, 2, 3, 3, 4]),
                window_count=r.randrange(5), windows=[rwindow(r) for _ in range(4)],
                frame_mode=r.choice([-1, 1, 1, 2, 3, 0]), last_view_made=r.choice([0, 1, 1, -2]),
                view=view, last_view=last, light_mask=r.getrandbits(32), lists=rlists(r, 7, longest=3),
                links=rlinks(r), callback=r.randrange(2), f74=r.randrange(3),
                queues=rqueues(r, WINDOW_KEYS + ("FrontWindow", "AltTabStatus", "TCClose", "DPInit"), 0.03),
                internal=[r.getrandbits(32) for _ in range(32)])

# -- PrepareWorldspace


def worldspace_edges():
    yield case(3)
    yield case(3, entry_flags=0x12345678)
    light = [[]] * 6 + [[3, 1, 2]]
    yield case(3, lists=light)
    for k in (3, 1, 2):
        yield case(3, lists=light, queues={f"LinkPrepare:{k}": [0]})
    yield case(3, lists=light, queues={"HFPrepare": [0]})
    yield case(3, lists=light, separate=1, global_f30=1)
    grid = [[[1], [], [2, 3], [4]], [[5, 6], [7], [], [8, 1]], [[2], [3, 4, 5], [6], []]]
    for which in (1, 2):
        c = case(3, gamut=which, lists=cells(None, grid))
        quads(c, which - 1, 0, 3, [(0, 0, 4), (1, 1, 3), (2, 2, 2)])
        yield c
        c = case(3, gamut=which, lists=cells(None, grid))
        quads(c, which - 1, 1, 3, [(1, 3, 1), (2, 0, 1)])
        yield c
        c = case(3, gamut=which, lists=cells(None, grid))
        quads(c, which - 1, 2, 1, [(2, 0, 4)])
        yield c
    for k in range(1, 9):
        c = case(3, gamut=1, lists=cells(None, grid), queues={f"LinkPrepare:{k}": [1, 0]})
        quads(c, 0, 0, 3, [(0, 0, 4), (1, 0, 4), (2, 0, 4)])
        yield c
    c = case(3, gamut=2, lists=cells(None, grid), separate=1, global_f30=2)
    quads(c, 1, 0, 3, [(0, 3, 4), (1, 0, 2), (2, 1, 4)])
    yield c
    c = case(3, gamut=1, map_w=6, map_h=2, lists=cells(None, [[[1], [2], [3], [4], [5], [6]], [[7], [8], [1], [2], [3], [4]]]))
    quads(c, 0, 0, 2, [(0, 1, 6), (1, 2, 4)])
    yield c


def worldspace_random(r):
    w, h = r.randrange(1, 7), r.randrange(1, 7)
    grid = [[[r.randrange(1, 9) for _ in range(r.randrange(3))] for _ in range(w)] for _ in range(h)]
    lists = rlists(r, 7, longest=3) + [cell for row in grid for cell in row]
    which = r.choice([0, 1, 2])
    c = case(3, gamut=which, map_w=w, map_h=h, lists=lists, separate=r.randrange(2), global_f30=r.randrange(-1, 3),
             entry_flags=r.getrandbits(32), queues=rqueues(r, ("HFPrepare",) + tuple(f"LinkPrepare:{k}" for k in range(1, 9)), 0.04))
    for q in (0, 1):
        first = r.randrange(h + 1)
        end = r.randrange(h + 1)
        quads(c, q, first, end, [(row, r.randrange(w + 1), r.randrange(w + 1)) for row in range(h)])
    return c

# -- ALTTAB_RestoreState


ALTTAB_STEPS = ("TCClose", "RSTempShutdown", "DDTempShutdown", "DDReInitialize", "RSReInitialize",
                "TCReInitialize", "DPInit")


def alttab_edges():
    for renderer in (-1, 0, 1, 2, 3, 4, 5, 0x7FFFFFFF):
        yield case(4, renderer=renderer)
    for renderer in (1, 3):
        for step in ALTTAB_STEPS:
            yield case(4, renderer=renderer, queues={step: [0]})


def alttab_random(r):
    return case(4, renderer=r.choice([-2, 0, 1, 2, 3, 4, 9]), queues=rqueues(r, ALTTAB_STEPS, 0.12))

# -- the small members


def small_edges():
    for v in (0, 0x1F31, -1, 0x7FFFFFFF):
        yield case(5, arg=v, game_flags=0x5555)
        yield case(6, game_flags=v & MASK)
    yield case(7)
    yield case(8, delay_count=5)
    for count in range(9):
        yield case(9, delay_count=count)
    yield case(9, delay_count=2, queues={"MeshAdd:1": [3], "MeshAdd:3": [5], "MeshAdd:5": [1]},
               delay=[0, 1, 7, 7, 7, 7, 7, 7])
    yield case(9, delay_count=3, delay=[4, 4, 2, 0, 0, 0, 0, 0], mesh_flags=[0x10, 0, -1, 0x80000000, 0xEF, 1, 2, 3])
    yield case(9, delay_count=-3)
    for blt in (0, 1, -1, 0x887601C2):
        yield case(10, queues={"Blt": [blt]}, dd_result=0x55)
    for v in (0, 1, -1, 0x7FFF, -0x8000):
        yield case(11, arg=v)


def small_random(r):
    op = r.choice([5, 6, 7, 8, 9, 9, 10, 11])
    count = r.randrange(9)
    adds = {f"MeshAdd:{i}": [r.randrange(MESHES)] for i in range(MESHES) if r.random() < 0.15}
    return case(op, arg=T.s32(r.getrandbits(32)), game_flags=r.getrandbits(32), delay_count=count,
                delay=[r.randrange(MESHES) for _ in range(MESHES)], mesh_flags=[r.getrandbits(32) for _ in range(MESHES)],
                queues={"Blt": [r.choice([0, 0, 1, T.s32(r.getrandbits(32))])], **adds}, dd_result=r.getrandbits(32))


TESTS = [FnTest("CMapWho::RenderWindow", ppc, host, edge=window_edges, rnd=window_random),
         FnTest("CMapWho::Render", ppc, host, edge=render_edges, rnd=render_random),
         FnTest("CMapWho::PrepareWorldspace", ppc, host, edge=worldspace_edges, rnd=worldspace_random),
         FnTest("CMapWho::ALTTAB_RestoreState", ppc, host, edge=alttab_edges, rnd=alttab_random),
         FnTest("options, GetQuads, CMeshDelay, CLoadSurface::Display, FPU", ppc, host, edge=small_edges,
                rnd=small_random)]
