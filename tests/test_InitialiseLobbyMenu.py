"""Unit v5230, the main menu's and the lobby menu's makers, against libtpw_unit_InitialiseLobbyMenu.dylib
(tests/unit_InitialiseLobbyMenu.cpp), main app ("SimTheme Park"), src/ui_main_kbcapture_InitialiseLobbyMenu.cpp:
  InitialiseMainMenu 0x101fb760, InitialiseLobbyMenu 0x101fbcd0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit are fakes on both
sides that record their arguments: the menu's constructor, Init (the handlers as labels) and AddItem (every argument), UIMain_GetString
(the string number; the answer is a token the AddItem event shows), the online system's constructor (it sets the lobby flag at 0x108 from
the case), AreWeLoggedIn and AreWeProcessing, the game type's constructor and the theme islands screen's IsVisible.

The world of a case: gMainMenu (an old value), gOnlineSystem (none, or an object with the flag at 0x108), the lobby screens, the world's
word 0x1da738, the game type object (its value and the guard byte). What is compared: gMainMenu replaced, gOnlineSystem kept or made,
the game type's value and the guard, and the event log (the order of the calls and every argument, so the positions chained from the
AddItem answers are proved).

No fused operations. The guard byte of the game type is a function-local static on the host and cannot be reset: only the very first
case of InitialiseMainMenu has it clear (the constructor runs on both sides), every other case has it set. Not tested: a NULL
gLobbyScreens or gGameWorld (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InitialiseLobbyMenu"
MASK = 0xFFFFFFFF

FN_MAIN, FN_LOBBY = 0x101FB760, 0x101FBCD0
OWN = {FN_MAIN, FN_LOBBY}

ADDR = dict(menu=0x10396A58, online=0x10380AB0, screens=0x102E49B8, world=0x1037EBFC, gtype=0x102B3AA0, guard=0x102B3AA4)
TV_LOBBY, TV_GAME, TV_ITEM = 0x10268608, 0x10268628, 0x10268650
HOST_LAYOUT = [0x1DA738, 0x1A0]     # the host's CFSOnlineSystem keeps the flag at 0x1a0 (native pointers); the Mac's is 0x108


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def ctor_online(e):
    a = e.r(3)
    CL.recorder("CFSOnlineSystem", "N")(e)
    e.write(a + 0x108, struct.pack(">I", e.cl.ret("f108") & MASK))


def stub_gametype(e):
    e.cl.rec("CGameType", ())
    e.write(ADDR["gtype"], struct.pack(">I", e.cl.ret("gametype") & MASK))


STUBS.internal(0x1018AC44, CL.recorder("UIMenu", "N"))
STUBS.internal(0x1018AD20, CL.recorder("Init", "LLL"))
STUBS.internal(0x1018AE78, CL.recorder("AddItem", "Liuiiiii", ret="i"))
STUBS.internal(0x10180468, CL.recorder("GetString", "i", ret=lambda e, log: 0x7000 + s32(e.r(3))))
STUBS.internal(0x1016F7D0, ctor_online)
STUBS.internal(0x1016F5D8, stub_gametype)
STUBS.internal(0x1017178C, CL.recorder("AreWeLoggedIn", "L", ret="i"))
STUBS.internal(0x10171498, CL.recorder("AreWeProcessing", "L", ret="i"))
STUBS.internal(0x1023BAA8, CL.recorder("IsVisible", "L", ret="i"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"
    f108 = 0x108

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({TV_LOBBY: 1, TV_GAME: 2, TV_ITEM: 3}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

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

    def setp(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return struct.unpack(">I", self.read(a, 4))[0]

    def make_screens(self):
        comp = self.alloc(0x20)
        self.words(comp, [self.alloc(0x40)])
        system = self.alloc(0x20)
        self.words(system + 4, [comp])
        return system

    def set_type(self, case):
        self.words(ADDR["gtype"], [case["type"]])
        self.write(ADDR["guard"], bytes([0 if case["guard0"] else 1]))

    def get_type(self, case):
        return self.rd_word(ADDR["gtype"]), self.read(ADDR["guard"], 1)[0]

    def rd_word(self, a):
        return struct.unpack(">I", self.read(a, 4))[0]

    def call(self, op):
        T.call_ppc(self.e, FN_MAIN if op == "main" else FN_LOBBY, [])


class Host(Side):
    fmt = "<I"
    f108 = 0x1A0

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        if case["op"] == "main" and not case["guard0"]:
            lib.tpw_ilm_settype(s32(case["type"]))
        CL.host_begin(lib, {1: self.g("cb_lobby"), 2: self.g("cb_game"), 3: self.g("cb_item")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ilm_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ilm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setp(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def make_screens(self):
        return self.lib.tpw_ilm_make_screens()

    def set_type(self, case):
        pass

    def get_type(self, case):
        return self.lib.tpw_ilm_gettype() & MASK, 1

    def call(self, op):
        (self.lib.tpw_ilm_main if op == "main" else self.lib.tpw_ilm_lobby)()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ilm_addr.restype, lib.tpw_ilm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ilm_alloc.restype, lib.tpw_ilm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ilm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ilm_make_screens.restype = C.c_uint64
    lib.tpw_ilm_settype.argtypes = [C.c_int32]
    lib.tpw_ilm_gettype.restype = C.c_int32
    out = (C.c_int64 * 32)()
    n = lib.tpw_ilm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.setp(s.g("menu"), case["oldmenu"])
    if case["online"] == "none":
        s.setp(s.g("online"), 0)
    else:
        block = s.alloc(0x500)
        s.words(block + s.f108, [case["f108"]])
        s.setp(s.g("online"), block)
    s.setp(s.g("screens"), s.make_screens())
    world = s.alloc(0x1DA740)
    s.words(world + 0x1DA738, [case["wstate"]])
    s.setp(s.g("world"), world)
    s.set_type(case)


def observe(s, case):
    online = s.rd_ptr(s.g("online"))
    prior = case["prior"]
    out = {
        "menu": s.rd_ptr(s.g("menu")) not in (0, case["oldmenu"]),
        "online": "none" if online == 0 else ("new" if case["online"] == "none" else ("same" if online == s.prior else "other")),
        "log": s.log(),
    }
    if case["op"] == "main":
        out["type"] = s.get_type(case)
    return out


def run(s, case):
    try:
        build(s, case)
        # the address of the existing online block, to tell "kept" from "made"
        s.prior = s.rd_ptr(s.g("online"))
        s.call(case["op"])
        case.setdefault("prior", 0)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, op, guard0=False, typ=None, wstate=None, online=None, f108=None, logged=None, proc=None, visible=None, newf108=None):
    c = {"op": op, "guard0": guard0}
    c["type"] = r.choice([0, 1, 2, 0, 5, MASK]) if typ is None else typ
    c["wstate"] = r.choice([0, 1, 2, 4, 4]) if wstate is None else wstate
    c["online"] = r.choice(["none", "obj"]) if online is None else online
    c["f108"] = r.choice([0, 1, 0x1234, MASK]) if f108 is None else f108
    c["oldmenu"] = r.choice([0, 0x1234])
    pick = lambda v: [r.choice([0, 1]), r.choice([0, 1])] if v is None else [v, v]
    q = {
        "AddItem": [r.choice([r.randrange(-50, 400), 0, 5, -5]) for _ in range(10)],
        "AreWeLoggedIn": pick(logged),
        "AreWeProcessing": pick(proc),
        "IsVisible": [r.choice([0, 1, 7]) if visible is None else visible],
        "f108": [r.choice([0, 1, 9]) if newf108 is None else newf108],
        "gametype": [r.choice([0, 1, 2, 4]) if not guard0 else 1],
    }
    c["queues"] = q
    return c


def main_edges():
    r = random.Random(5230)
    yield mk(r, "main", guard0=True, typ=0, online="obj", wstate=0)
    yield mk(r, "main", typ=1)
    yield mk(r, "main", typ=1, wstate=4)
    for t in (0, 2, -1 & MASK):
        yield mk(r, "main", typ=t, wstate=4)
    for online in ("none", "obj"):
        for lg in (0, 1):
            for pr in (0, 1):
                yield mk(r, "main", typ=0, wstate=0, online=online, logged=lg, proc=pr)


def main_random(r):
    return mk(r, "main")


def lobby_edges():
    r = random.Random(5231)
    for online in ("none", "obj"):
        for f108 in (0, 1, MASK):
            for nf in (0, 1):
                for lg in (0, 1):
                    for pr in (0, 1):
                        for vis in (0, 1, 7):
                            yield mk(r, "lobby", online=online, f108=f108, newf108=nf, logged=lg, proc=pr, visible=vis)


def lobby_random(r):
    return mk(r, "lobby")


TESTS = [
    FnTest("InitialiseMainMenu", ppc, host, edge=main_edges, rnd=main_random),
    FnTest("InitialiseLobbyMenu", ppc, host, edge=lobby_edges, rnd=lobby_random),
]
