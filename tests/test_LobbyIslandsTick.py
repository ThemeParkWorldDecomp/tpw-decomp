"""Differential test for src/lobby_islands_tick.cpp (the lobby's island screen, part 2: the
per-frame work), against libtpw_unit_LobbyIslandsTick.dylib (tests/unit_LobbyIslandsTick.cpp).

Functions: CLBScreenComponentIslands::Tick, CalcCamera, GotoNextIsland and GotoPreviousIsland,
CLBScreenComponentThemeIslands::Tick, and the flying meshes' Init, Tick and Render.

A case is a world and a script. The world is the lobby (its step, its random generator's state,
its description), the lobby view, the sound banks, the user info (absent, so that the code makes
it, or present with a user), the screen (every number from 0x10 to 0xe4), and up to five islands
in the screen's list with one of them current. The script is a few operations run one after
the other on that world (so that a screen can pass through its states). After every operation
the test compares the events of the fakes and the state of everything the code could have
changed: the screen, the islands, the view, the lobby's generator, the user info.

On the PowerPC side the screen, the islands and the list are memory images with the Mac's layout
and the functions under test (and the list's Find) are the original's code. Everything they call
is a stand-in that records its arguments: the weather (CRain), the sounds, the meshes' PlayAnim
and GetAnimTimeRemaining, CLBLobby::GetRand, rand, the advisor's SetTimeoutResponse, the flying
mesh groups' Tick, the user info's constructor, and the screen's two virtual members that the code
calls through the vtable (EnterCurrentIsland, slot 0x50; SetCurrentIsland, slot 0x54; the second
one moves the current island when the case says the real one would). Return values come from
queues the case fills. NaNs are compared as one value.

The original's fused multiply-adds and subtracts run under HardwareRounding, as in
test_OpenGLDraw.py: Unicorn rounds them twice.
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyIslandsTick"

MASK = 0xFFFFFFFF
FN = dict(Tick=0x100B0AB8, CalcCamera=0x100B1BAC, GotoNext=0x100B2638, GotoPrevious=0x100B2808, ThemeTick=0x100B29DC,
          FlyInit=0x100AB790, FlyTick=0x100AB7C4, FlyRender=0x100AB83C)
FIND = 0x100B4EF8        # TbTList<CLBIsland*, CLBIsland*>::Find, the original's code here too
# Callees outside the unit.
GET_RAND, FADE, SEND, SOUND_TICK = 0x100AA98C, 0x100DE0FC, 0x100DD9FC, 0x100DD668
PLAY_ANIM, ANIM_TIME, SET_TIMEOUT, GROUP_TICK, USER_CTOR = 0x100A7D28, 0x100A7DF0, 0x100A71C8, 0x100AB1A8, 0x1017DE54
# Globals: the lobby, the lobby view, the screen system, the user info, the sound state.
G_LOBBY, G_VIEW, G_SCREENS, G_USER, G_SOUND = 0x102E4888, 0x102E48CC, 0x102E49B8, 0x10380B34, 0x102F7C94
# Labels of the named objects (as in the unit library).
LAB_SCREEN, LAB_VIEW, LAB_LOBBY, LAB_RAIN, LAB_ADVISOR, LAB_FLYING, LAB_MESH, LAB_ISLAND, LAB_GROUP, LAB_USER = 1, 2, 3, 4, 5, 6, 10, 30, 40, 60
CODE_RANGES = [(FN["Tick"], FN["Tick"] + 4284), (FN["CalcCamera"], FN["CalcCamera"] + 428)]

# The screen's words from 0x10 to 0xe4: (name, kind) with kind i (integer), f (float) or 3 (three floats).
SCREEN = [("f10", "i"), ("state", "i"), ("angle", "f"), ("spin", "f"), ("radius", "f"), ("vert", "f"),
          ("cam", "3"), ("camgoal", "3"), ("camtarget", "3"), ("camtargetgoal", "3"), ("centre", "3"), ("size", "3"),
          ("wspeed", "f"), ("warrival", "f"), ("wpos", "3"), ("wdest", "3"), ("wdir", "3"), ("wdirgoal", "3"),
          ("lookmax", "f"), ("lookspeed", "f"), ("lookarrival", "f"), ("wlook", "3"), ("wlookgoal", "3"),
          ("lookdir", "3"), ("lookdirgoal", "3"), ("fe4", "i")]
SCREEN_KINDS = "".join({"i": "i", "f": "f", "3": "fff"}[k] for _, k in SCREEN)
assert len(SCREEN_KINDS) == 54


def fb(x):
    """A float32's bits."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bf(w):
    """The float32 with the bits w."""
    return struct.unpack(">f", struct.pack(">I", w & MASK))[0]


def canon(w):
    """A float's bits with every NaN the same."""
    return 0x7FC00000 if (w & 0x7F800000) == 0x7F800000 and (w & 0x007FFFFF) else w


def w32(e, a, v):
    """A big-endian word into the emulator's memory."""
    e.write(a, struct.pack(">I", v & MASK))


def u32(v):
    """The low 32 bits."""
    return v & MASK


# ----------------------------------------------------------------- the emulator's fused multiply-adds


LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Unicorn (QEMU 5.0) computes fmadds, fmsubs, fnmadds and fnmsubs in double precision and
    rounds the result to single, which can differ by one unit in the last place from the
    processor's single rounding of the exact result. A code hook over the functions that use
    them looks at each instruction before it runs and rewrites its target register just before
    the next one, with the C library's fmaf (one rounding)."""

    changed = 0    # results the correction replaced (all cases so far), for the notes

    def __init__(self, e):
        self.e, self.pending, self.hooks = e, None, []

    def __enter__(self):
        for begin, end in CODE_RANGES:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            target, value = self.pending
            self.pending = None
            corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
            if fpr(e, target) != corrected and not (value != value and fpr_double(e, target) != fpr_double(e, target)):
                HardwareRounding.changed += 1
            uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        for x in (a, c, b):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        subtract = kind in (28, 30)
        value = float(LIBM.fmaf(a, c, -b if subtract else b))
        if kind in (30, 31):
            value = -value
        self.pending = (rd, value)


# ----------------------------------------------------------------- the stand-ins


def build_stubs(emu):
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(FN.values())
    S.real.add(FIND)
    S.imp("rand", R("rand", "", lambda e, L: L.ret("rand", 0)))
    S.imp("__nw__FUl", lambda e: e.set_r(3, e.alloc(max(e.r(3), 4), b"\xa5" * max(e.r(3), 4))))
    S.imp("SetRainType__5CRainFi", R("Rain.SetRainType", "Li"))
    S.imp("SetRaindrops__5CRainFi", R("Rain.SetRaindrops", "Li"))
    S.imp("LightningFlash__5CRainFv", R("Rain.LightningFlash", "L"))

    def add_lightning(e):
        # (this, start x y z, end x y z in r4 to r9, a in r10, b on the stack at 0x38(r1))
        L = e.cl
        words = [canon(e.r(i)) for i in range(4, 10)]
        L.rec("Rain.AddLightning", [L.lab(e.r(3))] + words + [T.s32(e.r(10)), T.s32(e.u32(e.r(1) + 0x38))])

    S.imp("AddLightning__5CRainF10_D3DVECTOR10_D3DVECTORii", add_lightning)
    S.internal(GET_RAND, R("Lobby.GetRand", "", lambda e, L: L.ret("GetRand", 0x12345678)))
    S.internal(FADE, R("Sound.Fade", "i"))
    S.internal(SEND, R("Sound.Send", "iiiiii", lambda e, L: L.ret("Sound", 1000)))
    S.internal(SOUND_TICK, R("Sound.Tick"))
    S.internal(PLAY_ANIM, R("Mesh.PlayAnim", "Lii"))
    S.internal(ANIM_TIME, R("Mesh.AnimTime", "L", lambda e, L: L.ret("Anim:%d" % L.lab(e.r(3)), 0)))
    S.internal(SET_TIMEOUT, R("Advisor.SetTimeoutResponse", "Liu"))
    S.internal(GROUP_TICK, R("Group.Tick", "L"))

    def user_ctor(e):
        L, obj = e.cl, e.r(3)
        L.rec("UserInfo.ctor", (L.newlab(obj),))
        w32(e, obj + 0x60, L.ret("GameUser", -1))

    S.internal(USER_CTOR, user_ctor)
    return S


STUBS = None


def setup(runner):
    """The standard stubs, the recorders, and the screen's vtable: slot 0x50 EnterCurrentIsland and
    slot 0x54 SetCurrentIsland."""
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    STUBS = build_stubs(e)

    def enter(e):
        e.cl.rec("Screen.EnterCurrentIsland", (e.cl.lab(e.r(3)),))

    def set_current(e):
        L, screen, island = e.cl, e.r(3), e.r(4)
        L.rec("Screen.SetCurrentIsland", (L.lab(screen), L.lab(island)))
        if e.li_sets_current:
            w32(e, screen + 0xC, island)

    vtable = T.persist_alloc(e, 0x60)
    w32(e, vtable + 0x50, T.fake_function(e, enter))
    w32(e, vtable + 0x54, T.fake_function(e, set_current))
    e.li_vtable = vtable
    e.li_sets_current = 0


# ----------------------------------------------------------------- PowerPC side


class PpcSide:
    """The case's world as a memory image with the Mac's layout, the stand-ins active, and the
    calls under test. close() puts the globals it overwrote back."""

    def __init__(self, e, case):
        self.e = e
        self.world = case["world"]
        e.heap_in_scratch = True
        e.li_sets_current = self.world["setcur"]
        self.fixed = {}
        self.saved = {}
        self.rain_cell = e.data_import("gei_Rain")
        for address, size in ((G_LOBBY, 4), (G_VIEW, 4), (G_SCREENS, 4), (G_USER, 4), (G_SOUND + 0x28, 8), (self.rain_cell, 4)):
            self.saved[address] = e.read(address, size)
        self.build_lobby()
        self.build_view()
        self.build_sounds_and_user()
        self.build_screen()
        e.cl = self.L = CL.PpcLog(self.fixed, case.get("q"))
        self.old = STUBS.activate(e)

    def alloc(self, size, label=None):
        """A block of the image, filled with 0xa5 (a byte the code under test must not depend on)."""
        address = self.e.alloc(size, b"\xa5" * size)
        if label is not None:
            self.fixed[address] = label
        return address

    def build_lobby(self):
        """The lobby (gLobby): the step at 8, its description at 0x10, the generator's state at 0x34."""
        e, world = self.e, self.world
        description = self.alloc(0x6C)
        for offset, word in zip((8, 0xC, 0x10), world["globals"]):
            w32(e, description + offset, word)
        self.lobby = self.alloc(0x38, LAB_LOBBY)
        w32(e, self.lobby + 8, world["units"])
        w32(e, self.lobby + 0x10, description)
        w32(e, self.lobby + 0x34, world["seed"])
        w32(e, G_LOBBY, self.lobby)

    def build_view(self):
        """The lobby view (gLobbyView): position 8, target 0x14, sky 0x44 and 0x50, fade 0x5c and 0x60."""
        e, v = self.e, self.world["view"]
        self.view = self.alloc(0x64, LAB_VIEW)
        for offset, words in ((8, v["pos"]), (0x14, v["target"]), (0x44, v["sky_current"]), (0x50, v["sky_target"])):
            for i, word in enumerate(words):
                w32(e, self.view + offset + 4 * i, word)
        w32(e, self.view + 0x5C, v["fade_current"])
        w32(e, self.view + 0x60, v["fade_target"])
        w32(e, G_VIEW, self.view)

    def build_sounds_and_user(self):
        """The screen system (only its advisor), the rain, the sound banks and the user info."""
        e, world = self.e, self.world
        screens = self.alloc(0x1C)
        w32(e, screens + 0x14, self.alloc(16, LAB_ADVISOR))
        w32(e, G_SCREENS, screens)
        w32(e, self.rain_cell, self.alloc(16, LAB_RAIN))
        table = e.alloc(8 * 4, b"".join(struct.pack(">I", u32(bank)) for bank in world["banks"]))
        w32(e, G_SOUND + 0x28, world["thunder"])
        w32(e, G_SOUND + 0x2C, table)
        if world["user"] is None:
            w32(e, G_USER, 0)
        else:
            user = self.alloc(0xD4, LAB_USER)
            w32(e, user + 0x60, world["user"])
            w32(e, G_USER, user)

    def build_screen(self):
        """The islands, their list (head 4, tail 8, count 0xc; a node: value, next, previous) and the
        screen with its numbers from 0x10 to 0xe4."""
        e, world = self.e, self.world
        self.islands = []
        for i, island in enumerate(world["islands"]):
            mesh = self.alloc(16, LAB_MESH + 2 * i)
            zoom = self.alloc(16, LAB_MESH + 2 * i + 1)
            obj = self.alloc(0x74, LAB_ISLAND + i)
            w32(e, obj + 4, mesh)
            w32(e, obj + 8, zoom)
            w32(e, obj + 0x14, island["heading"])
            w32(e, obj + 0x18, island["sound_set"])
            w32(e, obj + 0x1C, island["handles"][0])
            w32(e, obj + 0x20, island["handles"][1])
            w32(e, obj + 0x5C, island["mask"])
            w32(e, obj + 0x60, island["raining"])
            for k in range(3):
                w32(e, obj + 0x3C + 4 * k, island["pos"][k])
                w32(e, obj + 0x64 + 4 * k, island["sky"][k])
            self.islands.append(obj)
        lst = self.alloc(0x10)
        w32(e, lst + 4, 0)
        previous = 0
        for obj in self.islands:
            node = e.alloc(12, struct.pack(">III", obj, 0, previous))
            w32(e, (previous + 4) if previous else (lst + 4), node)
            previous = node
        w32(e, lst + 8, previous)
        w32(e, lst + 0xC, world.get("count_word", len(self.islands)))
        self.screen = self.alloc(0xEC, LAB_SCREEN)
        w32(e, self.screen, e.li_vtable)
        w32(e, self.screen + 4, 0)
        w32(e, self.screen + 8, lst)
        w32(e, self.screen + 0xC, self.islands[world["current"]] if world["current"] >= 0 else 0)
        for i, word in enumerate(screen_words(world["screen"])):
            w32(e, self.screen + 0x10 + 4 * i, word)

    def call(self, address, *args):
        """A call of the original under the fused multiply-add correction."""
        with HardwareRounding(self.e):
            return T.call_ppc(self.e, address, [a & MASK for a in args])[0]

    def op(self, op):
        """1: Islands::Tick, 2: CalcCamera, 3: GotoNextIsland, 4: GotoPreviousIsland, 5: ThemeIslands::Tick."""
        address = {1: FN["Tick"], 2: FN["CalcCamera"], 3: FN["GotoNext"], 4: FN["GotoPrevious"], 5: FN["ThemeTick"]}[op]
        self.call(address, self.screen)

    def state(self):
        """Everything the code under test can change, in the order of tpw_li_state."""
        e, L = self.e, self.L
        out = [L.lab(e.u32(self.screen + 0xC))]
        words = [e.u32(self.screen + 0x10 + 4 * i) for i in range(54)]
        out += [canon(w) if kind == "f" else w for w, kind in zip(words, SCREEN_KINDS)]
        view = self.view
        out += [canon(e.u32(view + 8 + 4 * i)) for i in range(3)] + [canon(e.u32(view + 0x14 + 4 * i)) for i in range(3)]
        out += [e.u32(view + 0x50 + 4 * i) for i in range(3)] + [e.u32(view + 0x44 + 4 * i) for i in range(3)]
        out += [e.u32(view + 0x5C), e.u32(view + 0x60)]
        out += [e.u32(self.lobby + 0x34), canon(e.u32(self.lobby + 8))]
        user = e.u32(G_USER)
        out += [1 if user else 0, e.u32(user + 0x60) if user else 0]
        for obj in self.islands:
            out += [canon(e.u32(obj + 0x14)), e.u32(obj + 0x18), e.u32(obj + 0x1C), e.u32(obj + 0x20)]
            out += [canon(e.u32(obj + 0x3C + 4 * i)) for i in range(3)]
            out += [e.u32(obj + 0x5C), e.u32(obj + 0x60)] + [e.u32(obj + 0x64 + 4 * i) for i in range(3)]
        return out

    def close(self):
        CL.Stubs.deactivate(self.e, self.old)
        for address, data in self.saved.items():
            self.e.write(address, data)


def screen_words(screen):
    out = []
    for name, kind in SCREEN:
        value = screen[name]
        out += list(value) if kind == "3" else [value]
    return out


def world_words(world):
    """The case as the words tpw_li_begin reads (the order of its Reader)."""
    out = [len(world["islands"]), world.get("count_word", len(world["islands"])) & MASK, world["current"] & MASK, 0 if world["user"] is None else 1, (world["user"] or 0) & MASK,
           world["seed"], world["units"], *world["globals"], world["thunder"] & MASK, *[b & MASK for b in world["banks"]], world["setcur"]]
    v = world["view"]
    out += [*v["pos"], *v["target"], *[x & MASK for x in v["sky_target"]], *[x & MASK for x in v["sky_current"]], v["fade_current"] & MASK, v["fade_target"] & MASK]
    out += screen_words(world["screen"])
    for island in world["islands"]:
        out += [island["heading"], island["sound_set"] & MASK, island["handles"][0] & MASK, island["handles"][1] & MASK, *island["pos"],
                island["mask"] & MASK, island["raining"] & MASK, *[x & MASK for x in island["sky"]]]
    return [w & MASK for w in out]


def islands_ppc(e, case):
    side = PpcSide(e, case)
    try:
        states, marks = [side.state()], []
        for op in case["script"]:
            side.op(op)
            states.append(side.state())
            marks.append(len(side.L.log))
        return dict(states=states, marks=marks, log=side.L.log)
    finally:
        side.close()


def islands_host(lib, case):
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    CL.host_begin(lib, {}, case.get("q"))
    words = world_words(case["world"])
    lib.tpw_li_begin((C.c_uint32 * len(words))(*words))
    state_fn = lib.tpw_li_state
    state_fn.restype = C.c_int32
    values = (C.c_uint32 * 512)()
    states, marks = [list(values[:state_fn(values)])], []
    try:
        for op in case["script"]:
            lib.tpw_li_op(op)
            states.append(list(values[:state_fn(values)]))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    return dict(states=states, marks=marks, log=log)


# ----------------------------------------------------------------- the flying meshes


def flying_ppc(e, case):
    saved = STUBS.activate(e)
    try:
        e.heap_in_scratch = True
        group_objects = [e.alloc(8, b"\xa5" * 8) for _ in range(case["groups"])]
        lst = e.alloc(0x10, b"\xa5" * 0x10)
        w32(e, lst + 4, 0)
        previous = 0
        for obj in group_objects:
            node = e.alloc(12, struct.pack(">III", obj, 0, previous))
            w32(e, (previous + 4) if previous else (lst + 4), node)
            previous = node
        w32(e, lst + 8, previous)
        w32(e, lst + 0xC, len(group_objects))
        system = e.alloc(8, b"\xa5" * 8)
        w32(e, system + 4, lst)
        fixed = {system: LAB_FLYING}
        fixed.update({obj: LAB_GROUP + i for i, obj in enumerate(group_objects)})
        e.cl = L = CL.PpcLog(fixed)
        marks = []
        for op in case["script"]:
            T.call_ppc(e, {6: FN["FlyInit"], 7: FN["FlyTick"], 8: FN["FlyRender"]}[op], [system])
            marks.append(len(L.log))
        return dict(marks=marks, log=L.log, count=e.u32(lst + 0xC))
    finally:
        CL.Stubs.deactivate(e, saved)


def flying_host(lib, case):
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    CL.host_begin(lib, {})
    lib.tpw_li_flying_begin(case["groups"])
    marks = []
    try:
        for op in case["script"]:
            lib.tpw_li_flying_op(op)
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    out = (C.c_uint32 * 4)()
    lib.tpw_li_flying_state(out)
    return dict(marks=marks, log=log, count=out[0])


# ----------------------------------------------------------------- cases


def lcg(s):
    return (s * 1664525 + 1013904223) & MASK


def rf(r, lo, hi):
    return fb(r.uniform(lo, hi))


def pick(r, *choices):
    return r.choice(choices)


def point(r, lo, hi):
    return [rf(r, lo, hi) for _ in range(3)]


def plan_seed(r, mask, lightning, sound):
    """A random generator state that makes the first step's low bits give (or not) a lightning
    strike for `mask` (an island's mask, 0 or negative for none) and then the sound check pass
    (or not). Gives up after a while and takes whatever it has."""
    s = r.getrandbits(32)
    for _ in range(4000):
        s = r.getrandbits(32)
        t = lcg(s)
        struck = mask > 0 and (t & mask) == 1
        if mask > 0:
            u = t
        else:
            u = s
        if struck:
            for _ in range(4):
                u = lcg(u)
        sounds = (lcg(u) & 0xF) == 1
        if (not lightning or struck) and (lightning or not struck) and sounds == sound:
            return s
    return s


MASKS = [0, 1, 1, 3, 3, 7, 0xF, 0x1F, 0xFF, 0x7FFFFFFF, -1, -5, 0x80000000]


def make_island(r, centre, spread, mask, raining=None):
    return dict(
        heading=rf(r, -7, 7), sound_set=r.randrange(4), handles=[pick(r, 0, 0, 5, 77, -1, 1000), pick(r, 0, 0, 6, 78, -1, 2000)],
        pos=[fb(r.uniform(-spread, spread) + centre[0]), fb(r.uniform(-5, 20)), fb(r.uniform(-spread, spread) + centre[2])],
        mask=mask, raining=pick(r, 0, 1, 1, 2, -1) if raining is None else raining, sky=[pick(r, r.randrange(256) << 16, r.getrandbits(32)) for _ in range(3)])


def make_screen(r, state, units, centre, islands, current):
    """A screen of the given state, with its numbers in plausible ranges; a state 1 screen is
    often within the step of its heading."""
    s = dict(f10=r.getrandbits(32), state=state)
    s["angle"] = rf(r, -9, 9)
    s["spin"] = rf(r, -3, 3)
    s["radius"] = rf(r, 5, 200)
    s["vert"] = rf(r, -5, 120)
    for name in ("cam", "camgoal", "camtarget", "camtargetgoal"):
        s[name] = [fb(c + r.uniform(-200, 200)) for c in centre]
    s["centre"] = [fb(c + r.uniform(-30, 30)) for c in centre]
    s["size"] = point(r, 0, 300)
    s["wspeed"] = rf(r, 0, 60)
    s["warrival"] = pick(r, rf(r, 0, 100), rf(r, 100, 90000), 0)
    s["wpos"] = [fb(c + r.uniform(-300, 300)) for c in centre]
    s["wdest"] = [fb(c + r.uniform(-300, 300)) for c in centre]
    s["wdir"] = point(r, -1, 1)
    s["wdirgoal"] = point(r, -1, 1)
    s["lookmax"] = rf(r, 0, 40)
    s["lookspeed"] = pick(r, rf(r, 0, 40), rf(r, -5, 5), 0)
    s["lookarrival"] = pick(r, rf(r, 0, 100), rf(r, 100, 90000), 0)
    s["wlook"] = [fb(c + r.uniform(-300, 300)) for c in centre]
    s["wlookgoal"] = [fb(c + r.uniform(-300, 300)) for c in centre]
    s["lookdir"] = point(r, -1, 1)
    s["lookdirgoal"] = point(r, -1, 1)
    s["fe4"] = r.getrandbits(32)
    if r.random() < 0.08:
        s["wdest"] = list(s["wpos"])         # nothing to aim at: the x axis
    if r.random() < 0.08:
        s["wlookgoal"] = list(s["wlook"])
    if r.random() < 0.04:
        s["wdir"] = [fb(0.0)] * 3             # normalising a zero vector
    if r.random() < 0.04:
        s["lookdir"] = [fb(0.0)] * 3
    if state == 1 and current >= 0 and r.random() < 0.5:
        # within the step of the heading, from either side, and now and then wrapped round a turn
        target = (math.pi + bf(islands[current]["heading"])) % (2 * math.pi)
        margin = 0.025 * bf(units)
        angle = target + r.choice([-1, 1]) * r.uniform(0, margin * 1.6) + r.choice([0, 0, 1, -1]) * 2 * math.pi
        s["angle"] = fb(angle)
    return s


def make_world(r, mode=None):
    """A random world. mode: None (any), or "browse" / "wander" to force the camera's mode."""
    if mode is None:
        mode = r.choice(["browse", "browse", "wander"])
    count = r.choice([0, 1, 2, 3, 3, 4, 5]) if mode == "wander" else r.choice([1, 2, 3, 4, 5])
    centre = [r.uniform(-500, 500), 0.0, r.uniform(-500, 500)]
    spread = r.choice([30, 80, 150, 400])
    shared = r.choice(MASKS)
    islands = []
    for _ in range(count):
        mask = shared if r.random() < 0.7 else r.choice(MASKS)
        islands.append(make_island(r, centre, spread, mask))
    current = r.randrange(count) if count and (mode == "browse" or r.random() < 0.4) else -1
    state = r.choice([0, 0, 1, 1, 2, 2, 2, 3, -1, 7])
    units = pick(r, rf(r, 0.05, 3), rf(r, 0.05, 3), rf(r, 0, 10), fb(0.0), fb(0.16), fb(1.0))
    screen = make_screen(r, state, units, centre, islands, current)
    if r.random() < 0.5:
        screen["radius"] = fb(pick(r, 8.0, 8.0, 8.1, 9.0, 12.0, 30.0))
    user = None if r.random() < 0.15 else (r.choice([0, 1, 2, 3]) if mode == "browse" else r.choice([-1, -1, 0, 1]))
    if mode == "wander" and count and user is not None and user != -1 and current >= 0:
        user = -1 if r.random() < 0.8 else user
    sound = r.random() < 0.3
    lightning = r.random() < 0.4
    cur_mask = islands[current]["mask"] if current >= 0 else shared
    seed = plan_seed(r, cur_mask, lightning, sound) if r.random() < 0.8 else r.getrandbits(32)
    spin_radius = fb(8.0) if r.random() < 0.2 else rf(r, 5, 200)
    return dict(
        units=units, seed=seed, globals=[rf(r, -3, 3), spin_radius, rf(r, -5, 120)],
        thunder=pick(r, 11, 22, -3), banks=[r.randrange(-5, 400) for _ in range(8)], setcur=r.randrange(2),
        view=dict(pos=[fb(c + r.uniform(-120, 120)) for c in centre], target=point(r, -500, 500),
                  sky_target=[r.getrandbits(32) for _ in range(3)], sky_current=[r.getrandbits(32) for _ in range(3)],
                  fade_current=pick(r, 0x10000, 0x10000, 0, 12345, -4), fade_target=r.getrandbits(32)),
        user=user, screen=screen, islands=islands, current=current)


def make_queues(r, world):
    q = {"GetRand": [r.getrandbits(32) for _ in range(r.randrange(1, 10))],
         "rand": [r.randrange(0, 32768) for _ in range(r.randrange(1, 6))],
         "Sound": [r.randrange(-3, 3000) for _ in range(r.randrange(1, 6))],
         "GameUser": [r.choice([-1, 0, 1, 2])]}
    for i in range(len(world["islands"])):
        q["Anim:%d" % (LAB_MESH + 2 * i)] = [r.choice([-5, 0, 0, 1, 100]) for _ in range(r.randrange(1, 4))]
    return q


def random_case(op_choices, longest=3):
    def make(r):
        world = make_world(r)
        return dict(world=world, q=make_queues(r, world), script=[r.choice(op_choices) for _ in range(r.randrange(1, longest + 1))])
    return make


def make_goto_world(r):
    world = make_world(r, "browse")
    count = r.choice([0, 1, 1, 2, 3, 5])
    centre = [0.0, 0.0, 0.0]
    world["islands"] = [make_island(r, centre, 100, 0) for _ in range(count)]
    world["current"] = r.choice([-1] + list(range(count))) if count else -1
    world["screen"]["state"] = r.choice([0, 0, 0, 0, 1, 2, 3, -1])
    return world


def goto_case(r):
    world = make_goto_world(r)
    return dict(world=world, q=make_queues(r, world), script=[r.choice([3, 4]) for _ in range(r.randrange(1, 5))])


def tick_edges():
    r = random.Random(11)
    for mode in ("browse", "wander"):
        for state in (0, 1, 2, 3, -1):
            for _ in range(6):
                world = make_world(r, mode)
                world["screen"]["state"] = state
                yield dict(world=world, q=make_queues(r, world), script=[1, 1])
    # the lobby with nothing in it, the user absent, the user logged out
    for user in (None, -1, 0):
        for count in (0, 1, 3):
            world = make_world(r, "wander")
            world["islands"] = world["islands"][:count]
            world["current"] = -1
            world["user"] = user
            yield dict(world=world, q=make_queues(r, world), script=[1, 5])
    # a destination or a look-at goal that shares one or two coordinates with the camera's own
    # position, or all three (the camera then aims along the x axis)
    for axes in ([0], [1], [2], [0, 1], [0, 2], [1, 2], [0, 1, 2]):
        world = make_world(r, "wander")
        world["current"] = -1
        if not world["islands"]:
            world["islands"] = [make_island(r, [0.0, 0.0, 0.0], 100, 0)]
        screen, island = world["screen"], world["islands"][0]
        screen["wdest"] = [screen["wpos"][k] if k in axes else fb(r.uniform(-50, 50)) for k in range(3)]
        screen["wdir"], screen["lookdir"] = point(r, -1, 1), point(r, -1, 1)
        screen["wspeed"] = fb(0.0)
        screen["cam"] = [fb(bf(c) + r.uniform(-20, 20)) for c in island["pos"]]
        screen["wlook"] = [island["pos"][k] if k in axes else fb(r.uniform(-50, 50)) for k in range(3)]
        screen["lookspeed"] = fb(0.0)
        yield dict(world=world, q=make_queues(r, world), script=[1])
    # a look-at goal at the centre, ten up, with no islands to look at
    world = make_world(r, "wander")
    world["islands"], world["current"] = [], -1
    world["screen"]["wlook"] = [world["screen"]["centre"][0], fb(10.0), world["screen"]["centre"][2]]
    world["screen"]["lookspeed"] = fb(0.0)
    yield dict(world=world, q=make_queues(r, world), script=[1, 1])
    # a list whose count word is not the number of its nodes (the code goes by the word)
    for count_word in (0, -1):
        for mode in ("browse", "wander"):
            world = make_world(r, mode)
            world["count_word"] = count_word
            yield dict(world=world, q=make_queues(r, world), script=[1, 1])
    # a zero step, a negative step, a huge one
    for units in (0.0, -1.0, 100.0, 1e-30):
        world = make_world(r, "browse")
        world["units"] = fb(units)
        yield dict(world=world, q=make_queues(r, world), script=[1, 1, 1])
    yield from rain_edges()
    yield from fade_edges()
    yield from poison_edges()


# Places a number can be spoiled: (where in the world, name, index). The angle-like numbers take
# a NaN but not an infinity (the original's loops that bring an angle into range never end on one).
LOOP_BOUND = {("screen", "angle"), ("screen", "spin"), ("world", "units"), ("island", "heading"), ("globals", "")}


def poison_places():
    places = [("screen", name, k) for name, kind in SCREEN if kind != "i" for k in range(3 if kind == "3" else 1)]
    places += [("world", "units", 0)] + [("globals", "", k) for k in range(3)]
    places += [("view", "pos", k) for k in range(3)] + [("island", "heading", 0)] + [("island", "pos", k) for k in range(3)]
    return places


def spoil(world, place, bits):
    where, name, k = place
    if where == "screen":
        value = world["screen"][name]
        if isinstance(value, list):
            value[k] = bits
        else:
            world["screen"][name] = bits
    elif where == "world":
        world[name] = bits
    elif where == "globals":
        world["globals"][k] = bits
    elif where == "view":
        world["view"][name][k] = bits
    elif world["islands"]:
        island = world["islands"][0]
        if name == "heading":
            island[name] = bits
        else:
            island[name][k] = bits


def poison_edges():
    """Each number the code compares or calculates with, spoiled in turn (NaN, infinities, minus zero)
    in a browsing and a wandering world, so that every comparison is seen with a NaN."""
    r = random.Random(14)
    for place in poison_places():
        values = [0x7FC00000, 0x80000000]
        if (place[0], place[1]) not in LOOP_BOUND or (place[0] == "globals" and place[2] != 0):
            values += [0x7F800000, 0xFF800000]
        for bits in values:
            for mode in ("browse", "wander"):
                world = make_world(r, mode)
                if mode == "browse":
                    world["screen"]["state"] = r.choice([0, 1, 2, 2])
                    if place[1] == "heading":
                        world["current"] = 0
                if not world["islands"] and place[0] == "island":
                    world["islands"] = [make_island(r, [0.0, 0.0, 0.0], 100, r.choice([0, 7]))]
                    world["current"] = 0 if mode == "browse" else -1
                spoil(world, place, bits)
                yield dict(world=world, q=make_queues(r, world), script=[1, 1])


def rain_edges():
    """A wandering camera that does not move, with rainy islands at chosen distances, so that the
    amount of rain lands on both sides of each of its limits."""
    r = random.Random(15)
    for distance in (0, 20, 60, 62, 63, 64, 90, 92, 93, 100, 300, 3000):
        for rainy in (0, 1, 2, 3):
            world = make_world(r, "wander")
            world["current"] = -1
            screen = world["screen"]
            here = [fb(r.uniform(-400, 400)), fb(r.uniform(0, 50)), fb(r.uniform(-400, 400))]
            screen["wpos"], screen["cam"], screen["wspeed"], screen["warrival"] = here, list(here), fb(0.0), fb(0.0)
            screen["wdest"] = point(r, -500, 500)
            world["islands"] = []
            for k in range(4):
                island = make_island(r, [0.0, 0.0, 0.0], 100, r.choice([0, 7]), raining=1 if k < rainy else 0)
                angle = r.uniform(0, 2 * math.pi)
                island["pos"] = [fb(bf(here[0]) + distance * math.cos(angle)), fb(r.uniform(-9, 99)), fb(bf(here[2]) + distance * math.sin(angle))]
                world["islands"].append(island)
            yield dict(world=world, q=make_queues(r, world), script=[1])


def fade_edges():
    """A zooming screen (state 2) with the step cut to nothing, so that its radius is as given, round
    the points where the fade is cut off or the island is entered."""
    r = random.Random(16)
    for radius in (7.99, 8.0, 8.5, 20.0, 100.0, 250.0):
        for description in (8.0, 20.0, 100.0, 250.0):
            for fade_current in (0x10000, 0):
                world = make_world(r, "browse")
                world["units"] = fb(0.0)
                world["screen"]["state"] = 2
                world["screen"]["radius"] = fb(radius)
                world["globals"][1] = fb(description)
                world["view"]["fade_current"] = fade_current
                yield dict(world=world, q=make_queues(r, world), script=[1])


def calc_case(r):
    world = make_world(r, "browse")
    world["screen"]["state"] = 3
    world["screen"]["angle"] = fb(r.uniform(-30, 30))
    if r.random() < 0.15:
        world["screen"]["angle"] = pick(r, 0x7FC00000, 0x7F800000, 0xFF800000, fb(1e30), fb(-0.0))
    return dict(world=world, q=make_queues(r, world), script=[2, 2])


def calc_edges():
    r = random.Random(12)
    for value in (0.0, 1.0, -1.0, math.pi / 2, math.pi, 100.0, 1e9, 1e30, -1e30):
        world = make_world(r, "browse")
        world["screen"]["angle"] = fb(value)
        yield dict(world=world, q=make_queues(r, world), script=[2])
    for bits in (0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000):
        world = make_world(r, "browse")
        world["screen"]["angle"] = bits
        yield dict(world=world, q=make_queues(r, world), script=[2, 2])
    for value in (0.0, 1e-3, 1.0, 5.0, 123456.0):
        world = make_world(r, "browse")
        world["units"] = fb(value)
        yield dict(world=world, q=make_queues(r, world), script=[2, 2, 2])


def goto_edges():
    r = random.Random(13)
    for count in (0, 1, 2, 3):
        for current in range(-1, count):
            for state in (0, 1):
                for setcur in (0, 1):
                    world = make_goto_world(r)
                    world["islands"] = [make_island(r, [0.0, 0.0, 0.0], 100, 0) for _ in range(count)]
                    world["current"] = current
                    world["screen"]["state"] = state
                    world["setcur"] = setcur
                    yield dict(world=world, q=make_queues(r, world), script=[3, 4, 3, 4])


def flying_case(r):
    return dict(groups=r.randrange(0, 5), script=[r.choice([6, 7, 7, 8]) for _ in range(r.randrange(1, 5))])


def flying_edges():
    for groups in range(5):
        yield dict(groups=groups, script=[6, 7, 8, 7])


TESTS = [
    FnTest("CLBScreenComponentIslands::Tick", islands_ppc, islands_host, edge=tick_edges, rnd=random_case([1])),
    FnTest("CLBScreenComponentIslands::CalcCamera", islands_ppc, islands_host, edge=calc_edges, rnd=calc_case),
    FnTest("CLBScreenComponentIslands::GotoNextIsland and GotoPreviousIsland", islands_ppc, islands_host, edge=goto_edges, rnd=goto_case),
    FnTest("CLBScreenComponentThemeIslands::Tick", islands_ppc, islands_host, edge=lambda: [], rnd=random_case([5], 2)),
    FnTest("CLBFlyingMeshSystem::Init, Tick and Render", flying_ppc, flying_host, edge=flying_edges, rnd=flying_case),
]
