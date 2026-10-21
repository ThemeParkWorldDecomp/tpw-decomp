"""Unit v5948, the world lobby window's creation (CreateWorldLobbyController) and the satellite number windows' message function
(SatelliteMsg), against libtpw_unit_CreateWorldLobbyController.dylib (tests/unit_CreateWorldLobbyController.cpp), main app
("SimTheme Park"), src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_worldLobbyController.cpp:
  CreateWorldLobbyController 0x1022b038, SatelliteMsg 0x1022ae30

A case is a world and a call. The world is built by one piece of Python code per side: in emulator memory at the Mac's addresses
(big-endian) for the original, and in the host library (tpw_cwl_world) for the translation. The callees outside the unit (the window
methods, the font table, the auto repeat, the number text object's new and constructor, the online system's new and constructor,
EnterChatSatellite, ProcessGetChatSatellitePlayers, the two virtual calls Enable and SetValue through __ptr_glue) are fakes on both
sides that record their arguments and answer from the case.

The world of a case: the satellite parent word, the lobby (with or without a window), the wanted word, the online system (existing
with four list words, or made by the fake constructor with them). What is compared: the answer, the log, the wanted word, and which
parent and system are set afterwards. No fused operations. Not tested: a NULL lobby (read unconditionally); the bool argument other
than 0 and 1 (a C++ bool cannot hold them); the callee-defined window pointers are plain blocks.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateWorldLobbyController"
MASK = 0xFFFFFFFF

FN_CREATE, FN_SAT = 0x1022B038, 0x1022AE30
SIZES = {FN_CREATE: 1600, FN_SAT: 464}
OWN = set(SIZES)

ADDR = dict(lobby=0x102E4888, wanted=0x10396FE8, parent=0x10396FEC, sys=0x10380AB0)
HOST_LAYOUT = [4]
SCRIPT, CB_WORLD, CB_SAT = 0x102B20F8, 0x102689A8, 0x102689A0
VT_ENABLE, VT_SETVALUE = 0xE1000001, 0xE1000002
NWIN = 15


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = [None]      # the Ppc side of the running case


def win_of_id(side, ident):
    i = ident - 0x16000 + 1
    return side.wins[i if 1 <= i <= 13 else 0]


def st(addr, fn):
    STUBS.internal(addr, fn)


def s_build(e):
    s = CUR[0]
    e.cl.rec("Build", (e.cl.lab(e.r(3)), 1 if e.r(4) == SCRIPT else 0, 1 if e.r(5) == CB_WORLD else 0))
    e.set_r(3, s.wins[0])


def s_setextra(e):
    e.cl.rec("setExtraParam", (e.cl.lab(e.r(3)), T.s16(e.r(4)), e.r(5) & MASK))
    e.set_r(3, 0)


def s_getextra(e):
    e.cl.rec("getExtraParam", (e.cl.lab(e.r(3)), T.s16(e.r(4))))
    e.set_r(3, e.cl.ret("getExtraParam") & MASK)


def s_getwindow(e):
    e.cl.rec("GetWindow", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, win_of_id(CUR[0], s32(e.r(4))))


def s_show(e):
    e.cl.rec("Show", (e.cl.lab(e.r(3)), T.s16(e.r(4))))
    e.set_r(3, 0)


def s_cdo(e):
    e.cl.rec("SetClientDrawObject", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def s_font(e):
    e.cl.rec("SetFont", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def s_fontjust(e):
    e.cl.rec("SetFontJust", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def s_callback(e):
    e.cl.rec("SetCallbackFunction", (e.cl.lab(e.r(3)), 1 if e.r(4) == CB_SAT else 0))


def s_default(e):
    e.cl.rec("CallDefaultWindowHandler", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, e.cl.ret("CallDefaultWindowHandler") & MASK)


def s_autorepeat(e):
    e.cl.rec("SetAutoRepeat", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def s_gettable(e):
    e.cl.rec("GetFontTableEntry", (e.r(3) & MASK,))
    e.set_r(3, CUR[0].font)


def s_enter(e):
    e.cl.rec("EnterChatSatellite", (e.cl.lab(e.r(3)), s32(e.r(4))))


def s_process(e):
    e.cl.rec("ProcessGetChatSatellitePlayers", ())


def s_new(e):
    save_toc(e)
    size = e.r(3) & MASK
    e.cl.rec("new", (size,))
    e.set_r(3, e.alloc(size))


NUMBER_CTOR = CL.recorder("NumberText", "Ni")
SYS_CTOR = CL.recorder("OnlineSystem", "N")


def s_numberctor(e):
    NUMBER_CTOR(e)


def s_sysctor(e):
    obj = e.r(3)
    SYS_CTOR(e)
    mk_system_ppc(e, obj, CUR[0].vals)


def save_toc(e):
    """The glue stores the caller's TOC at 0x14(r1); the original reloads r2 from there after the call."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def s_glue(e):
    save_toc(e)
    target = e.r(12) & MASK
    if target == VT_ENABLE:
        e.cl.rec("Enable", (e.cl.lab(e.r(3)), T.s16(e.r(4))))
    elif target == VT_SETVALUE:
        e.cl.rec("SetValue", (e.cl.lab(e.r(3)), s32(e.r(4))))
    else:
        raise AssertionError("unexpected virtual call %x" % target)


def mk_system_ppc(e, obj, vals):
    """The four lists' word 0x28 at the Mac's offsets: city 0xe0, theme 0xf8, server 0xfc, second in-city 0xf4."""
    for off, v in zip((0xE0, 0xF8, 0xFC, 0xF4), vals):
        lst = e.alloc(0x40)
        e.write(lst + 0x28, struct.pack(">I", v & MASK))
        e.write(obj + off, struct.pack(">I", lst))


st(0x101DEB78, s_build)
st(0x101DCFD0, s_setextra)
st(0x101DD030, s_getextra)
st(0x101DBE40, s_getwindow)
st(0x101DC1EC, s_show)
st(0x101DB624, s_cdo)
st(0x101DA118, s_font)
st(0x101DA204, s_fontjust)
st(0x101DCBA8, s_callback)
st(0x101DE318, s_default)
st(0x101CB4D4, s_autorepeat)
st(0x10180270, s_gettable)
st(0x10174BE8, s_enter)
st(0x1022A668, s_process)
st(0x102384B8, s_new)
st(0x1018D930, s_numberctor)
st(0x1016F7D0, s_sysctor)
st(0x1023BAA8, s_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def wd(self, a, w):
        self.write(a, struct.pack(self.fmt, w & MASK))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.vals = case["vals"]
        vt = e.alloc(0x40)
        e.write(vt + 0x1C, struct.pack(">I", VT_ENABLE))
        e.write(vt + 0x24, struct.pack(">I", VT_SETVALUE))
        self.wins = [e.alloc(0x200) for _ in range(NWIN)]
        for w in self.wins:
            e.write(w + 0x118, struct.pack(">I", vt))
        self.lobbywin = e.alloc(0x200)
        self.font = e.alloc(16)
        lobby = e.alloc(0x40)
        if case["lobby"]:
            e.write(lobby + 4, struct.pack(">I", self.lobbywin))
        e.write(ADDR["lobby"], struct.pack(">I", lobby))
        e.write(ADDR["wanted"], struct.pack(">I", case["wanted"] & MASK))
        e.write(ADDR["parent"], struct.pack(">I", self.wins[0] if case["parent"] else 0))
        self.pre = 0
        if case["has_sys"]:
            self.pre = e.alloc(0x404)
            mk_system_ppc(e, self.pre, self.vals)
        e.write(ADDR["sys"], struct.pack(">I", self.pre))
        fixed = {w: 1 + i for i, w in enumerate(self.wins)}
        fixed[self.lobbywin] = 20
        fixed[self.font] = 21
        if self.pre:
            fixed[self.pre] = 22
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        CUR[0] = self
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def win(self, i):
        return self.wins[i]

    def state(self):
        parent = self.rd_word(ADDR["parent"])
        sysw = self.rd_word(ADDR["sys"])
        return [self.rd_word(ADDR["wanted"]), 0 if parent == 0 else (1 if parent == self.wins[0] else 2),
                0 if sysw == 0 else (1 if sysw == self.pre else 2)]

    def create(self, publish):
        T.call_ppc(self.e, FN_CREATE, [publish & MASK])
        return 0

    def satellite(self, i, m, a, b):
        r3, _, _ = T.call_ppc(self.e, FN_SAT, [self.wins[i], m & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        vals = (C.c_int32 * 4)(*[s32(v) for v in case["vals"]])
        self.pre = lib.tpw_cwl_world(case["parent"], case["lobby"], s32(case["wanted"]), case["has_sys"], vals)
        self.wins = [lib.tpw_cwl_addr(b"win%d" % i) for i in range(NWIN)]
        fixed = {1 + i: w for i, w in enumerate(self.wins)}
        fixed[20] = lib.tpw_cwl_addr(b"lobbywin")
        fixed[21] = lib.tpw_cwl_addr(b"font")
        if self.pre:
            fixed[22] = self.pre
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def log(self):
        return CL.host_end(self.lib)

    def state(self):
        out = (C.c_int64 * 3)()
        self.lib.tpw_cwl_state(out)
        return [int(out[0]) & MASK, int(out[1]), int(out[2])]

    def create(self, publish):
        self.lib.tpw_cwl_create(publish & MASK)
        return 0

    def satellite(self, i, m, a, b):
        return self.lib.tpw_cwl_satellite(self.wins[i], s32(m), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cwl_addr.restype, lib.tpw_cwl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cwl_alloc.restype, lib.tpw_cwl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cwl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cwl_world.restype = C.c_uint64
    lib.tpw_cwl_world.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.POINTER(C.c_int32)]
    lib.tpw_cwl_state.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cwl_create.argtypes = [C.c_uint32]
    lib.tpw_cwl_satellite.restype = C.c_uint32
    lib.tpw_cwl_satellite.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cwl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def execute(s, case):
    if case["op"] == "create":
        return s.create(case["publish"])
    return s.satellite(14, *case["args"])


def run(s, case):
    try:
        answer = execute(s, case)
        return {"answer": answer, "log": s.log(), "state": s.state()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    return {"op": op, "parent": r.choice([0, 1]), "lobby": r.choice([0, 1]), "wanted": r.choice([0, 1, 5, r.getrandbits(32)]),
            "has_sys": r.choice([0, 1]), "vals": [r.choice([0, 1, 5, 0x80000000, r.getrandbits(32)]) for _ in range(4)],
            "queues": {"getExtraParam": [r.choice([0, 1, 2, 5, 6, 0xFFFFFFFF, r.getrandbits(32)])],
                       "CallDefaultWindowHandler": [r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)])]}}


def create_case(r, publish=None, parent=None, lobby=None):
    c = base(r, "create")
    c["publish"] = r.choice([0, 1]) if publish is None else publish
    if parent is not None:
        c["parent"] = parent
    if lobby is not None:
        c["lobby"] = lobby
    return c


def create_edges():
    r = random.Random(4801)
    for publish in (0, 1):
        for parent in (0, 1):
            for lobby in (0, 1):
                yield create_case(r, publish, parent, lobby)


def create_random(r):
    return create_case(r)


MSGS = [0x10006, 0x1E, 0, 0x10005, 0x10007, -1, 0x1D, 0x1F, 0x100, 0x20006, 0x6]


def sat_case(r, msg=None, vals=None, has_sys=None):
    c = base(r, "sat")
    m = r.choice(MSGS) if msg is None else msg
    c["args"] = [m, r.choice([0, 1, -1, r.getrandbits(32)]), r.choice([0, 7, r.getrandbits(32)])]
    # four values: mostly all set so the chain gets to the end
    c["vals"] = [r.choice([1, 3, 0x80000000]) for _ in range(4)] if vals is None else vals
    if has_sys is not None:
        c["has_sys"] = has_sys
    return c


def sat_edges():
    r = random.Random(4802)
    for has_sys in (0, 1):
        yield sat_case(r, 0x10006, [1, 1, 1, 1], has_sys)
        for k in range(4):
            v = [1, 1, 1, 1]
            v[k] = 0
            yield sat_case(r, 0x10006, v, has_sys)
        yield sat_case(r, 0x1E, None, has_sys)
        yield sat_case(r, 0x10005, None, has_sys)
        yield sat_case(r, 0, None, has_sys)
        yield sat_case(r, 0x1D, None, has_sys)
    for x in (0, 1, 2, 0x80000000, 0xFFFFFFFF):
        c = sat_case(r, 0x10006, [1, 1, 1, 1], 1)
        c["queues"]["getExtraParam"] = [x]
        yield c


def sat_random(r):
    c = sat_case(r)
    if r.random() < 0.3:
        c["vals"] = [r.choice([0, 1]) for _ in range(4)]
    return c


TESTS = [
    FnTest("CreateWorldLobbyController", ppc, host, edge=create_edges, rnd=create_random),
    FnTest("SatelliteMsg", ppc, host, edge=sat_edges, rnd=sat_random),
]
