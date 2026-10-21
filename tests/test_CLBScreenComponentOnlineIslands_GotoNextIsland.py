"""Unit v5255, the online islands screen's small members, against libtpw_unit_CLBScreenComponentOnlineIslands_GotoNextIsland.dylib
(tests/unit_CLBScreenComponentOnlineIslands_GotoNextIsland.cpp), main app, src/game/online/lobby_island.cpp:
  Tick 0x100b3b28, SelectCurrentIsland 0x100b44ac, EnterCurrentIsland 0x100b45d0, GotoNextIsland 0x100b46fc,
  DownloadFailFunction 0x100b4a98, HandleOnlineBacktrack 0x100b4b48

A case is a world and a call, built by one piece of Python code in both memories (the host's fields at the host's offsets, which the library
exports, pointers 8 bytes there). The screen, its current island, the island's park and mesh, the lobby and the lobby's screens are blocks
with a tag word at 0x200 that the fakes record. The callees outside the unit (the islands' Tick and GotoNextIsland, the city lobby
controller, UpdateCityLobbyUI, CreateOnlineParkInfoPanel, PlayAnim, CGameType and SetGameType) are fakes on both sides. For
HandleOnlineBacktrack the screen and the download globe have a vtable whose slots 0x24 and 0x28 reach recording Show and Hide (through the
original's glue on the PowerPC side).

Compared: the event log, the camera state, the word at 0xe8, the lobby's state. EnterCurrentIsland: the game type object's guard is zero in
the first case only (the host's function-local static is built once, in the first host call of the module).
No fused operations. Not tested: a NULL lobby or lobby screens (read unconditionally); DownloadFailFunction with state 2 and no current island.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CLBScreenComponentOnlineIslands_GotoNextIsland"
MASK = 0xFFFFFFFF

F_TICK, F_SELECT, F_ENTER, F_GOTO, F_FAIL, F_BACK = 0x100B3B28, 0x100B44AC, 0x100B45D0, 0x100B46FC, 0x100B4A98, 0x100B4B48
SIZES = {F_TICK: 100, F_SELECT: 124, F_ENTER: 116, F_GOTO: 76, F_FAIL: 100, F_BACK: 72}
OWN = set(SIZES)

ISLANDS_TICK, ISLANDS_GOTO = 0x100B0AB8, 0x100B2638
SHOW_CTRL, HIDE_CTRL, UPDATE_UI, CREATE_PANEL = 0x101E0858, 0x101E08C0, 0x101E0928, 0x101EABB4
GT_CTOR, GT_SET, PLAY_ANIM = 0x1016F5D8, 0x1016F6A0, 0x100A7D28
SLOT_SHOW, SLOT_HIDE = 0x100B3EC8, 0x100B4170       # code the fake vtables point at (faked in place)
CB_CANCEL, CB_DONE = 0x10266750, 0x10266748

ADDR = dict(gLobby=0x102E4888, gLobbyScreens=0x102E49B8, guard=0x102B3AA4)
TAGOFF = 0x200
BLOCK = 0x240
MAC = dict(mCurrent=0xC, mCameraState=0x14, mFE4=0xE4, mFE8=0xE8, mPark=0x70, mMeshB=8, mState=0x14, mDownloadGlobe=0xC)
HOST_NAMES = ["mCurrent", "mCameraState", "mFE4", "mFE8", "mPark", "mMeshB", "mState", "mDownloadGlobe"]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def tag(e, p):
    return e.u32(p + TAGOFF) if p else 0


def mk(name, fn):
    STUBS.internal(name, fn)


mk(ISLANDS_TICK, lambda e: e.cl.rec("IslandsTick", (tag(e, e.r(3)),)))
mk(ISLANDS_GOTO, lambda e: e.cl.rec("IslandsGotoNext", (tag(e, e.r(3)),)))
mk(SHOW_CTRL, lambda e: e.cl.rec("ShowCityLobbyController", ()))
mk(HIDE_CTRL, lambda e: e.cl.rec("HideCityLobbyController", ()))
mk(UPDATE_UI, lambda e: e.cl.rec("UpdateCityLobbyUI", (tag(e, e.r(3)),)))
mk(PLAY_ANIM, lambda e: e.cl.rec("PlayAnim", (tag(e, e.r(3)), s32(e.r(4)), s32(e.r(5)))))
mk(GT_CTOR, lambda e: e.cl.rec("CGameType", ()))
mk(GT_SET, lambda e: e.cl.rec("SetGameType", (s32(e.r(4)),)))
mk(SLOT_SHOW, lambda e: e.cl.rec("Show", (tag(e, e.r(3)),)))
mk(SLOT_HIDE, lambda e: e.cl.rec("Hide", (tag(e, e.r(3)),)))


def stub_panel(e):
    e.cl.rec("CreateOnlineParkInfoPanel", (tag(e, e.r(3)), e.r(4), 1 if e.r(5) == CB_CANCEL else 0, 1 if e.r(6) == CB_DONE else 0, tag(e, e.r(7))))


mk(CREATE_PANEL, stub_panel)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def tagged(self, t):
        a = self.alloc(BLOCK)
        self.words(a + TAGOFF, [t])
        return a


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def off(self, n):
        return MAC[n]

    def ptr(self, a, v):
        self.words(a, [v])

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def vobj(self, a):
        vt = self.alloc(0x40)
        tvs, tvh = self.alloc(8), self.alloc(8)
        self.words(tvs, [SLOT_SHOW, self.e.r(2)])
        self.words(tvh, [SLOT_HIDE, self.e.r(2)])
        self.words(vt + 0x24, [tvs])
        self.words(vt + 0x28, [tvh])
        self.words(a, [vt])

    def guard(self, v):
        self.write(ADDR["guard"], bytes([v]))

    def call(self, fn, obj):
        T.call_ppc(self.e, fn, [obj & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def off(self, n):
        return self.lib.hoff[n]

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def g(self, name):
        return self.lib.tpw_oi_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_oi_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def vobj(self, a):
        self.lib.tpw_oi_vobj(a)

    def guard(self, v):
        pass

    def call(self, fn, obj):
        getattr(self.lib, "tpw_oi_" + NAMES[fn])(obj)


NAMES = {F_TICK: "Tick", F_SELECT: "SelectCurrentIsland", F_ENTER: "EnterCurrentIsland", F_GOTO: "GotoNextIsland",
         F_FAIL: "DownloadFailFunction", F_BACK: "HandleOnlineBacktrack"}


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_oi_addr.restype, lib.tpw_oi_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_oi_alloc.restype, lib.tpw_oi_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_oi_vobj.argtypes = [C.c_uint64]
    lib.tpw_oi_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in NAMES.values():
        getattr(lib, "tpw_oi_" + n).argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_oi_layout(out)
    vals = list(out)[:n]
    lib.hoff = dict(zip(HOST_NAMES, vals[:8]))
    assert vals[9] == TAGOFF and vals[8] <= TAGOFF, vals
    lib.bound = True


def build(s, case):
    o = s.tagged(0x11)
    s.vobj(o)
    isl = s.tagged(0x22)
    park = s.tagged(0x33)
    mesh = s.tagged(0x44)
    globe = s.tagged(0x55)
    s.vobj(globe)
    lobby = s.tagged(0x66)
    screens = s.tagged(0x77)
    s.ptr(isl + s.off("mPark"), park if case["park"] else 0)
    s.ptr(isl + s.off("mMeshB"), mesh)
    s.ptr(o + s.off("mCurrent"), isl if case["cur"] else 0)
    s.words(o + s.off("mCameraState"), [case["cs"]])
    s.words(o + s.off("mFE4"), [case["fe4"]])
    s.words(o + s.off("mFE8"), [case["fe8"]])
    s.ptr(screens + s.off("mDownloadGlobe"), globe)
    s.ptr(s.g("gLobby"), lobby)
    s.ptr(s.g("gLobbyScreens"), screens)
    s.words(lobby + s.off("mState"), [case["state"]])
    s.guard(case["guard"])
    return o, lobby


def run(s, case):
    try:
        o, lobby = build(s, case)
        s.call(case["fn"], o)
        return {"log": s.log(), "cs": s.rd_word(o + s.off("mCameraState")), "fe4": s.rd_word(o + s.off("mFE4")),
                "fe8": s.rd_word(o + s.off("mFE8")), "state": s.rd_word(lobby + s.off("mState"))}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, fn):
    c = base0(r, fn)
    if fn == F_FAIL and c["cs"] == 2:
        c["cur"] = 1                    # the original reads the current island's mesh without a NULL test
    return c


def base0(r, fn):
    return {"fn": fn, "queues": {}, "cs": r.choice([0, 1, 2, r.randrange(-3, 6)]) & MASK, "fe4": r.choice([0, 1, r.getrandbits(32)]),
            "fe8": r.choice([0, 1, r.getrandbits(32)]), "cur": r.choice([0, 1, 1]), "park": r.choice([0, 1, 1]),
            "state": r.getrandbits(32), "guard": 1}


def edges(fn):
    def gen():
        r = random.Random(fn & 0xFFFF)
        first = True
        for cs in (0, 1, 2, 3):
            for fe4 in (0, 5):
                for fe8 in (0, 1, 7):
                    for cur, park in ((0, 0), (1, 0), (1, 1)):
                        c = base(r, fn)
                        c.update(cs=cs, fe4=fe4, fe8=fe8, cur=cur, park=park)
                        if fn == F_FAIL and cs == 2:
                            c["cur"] = 1
                        if fn == F_ENTER and first:
                            c["guard"] = 0
                        first = False
                        yield c
    return gen


def rnd(fn):
    return lambda r: base(r, fn)


TESTS = [FnTest(NAMES[f], ppc, host, edge=edges(f), rnd=rnd(f))
         for f in (F_GOTO, F_ENTER, F_SELECT, F_FAIL, F_BACK, F_TICK)]
