"""Unit v51046, nine small lobby, interface and ESP sprite functions, against libtpw_unit_OnlineIslandParkDownloadCompleteCallback_2.dylib
(tests/unit_OnlineIslandParkDownloadCompleteCallback_2.cpp), main app ("SimTheme Park"),
src/game/HLSprite.cpp, src/game/online/lobby_globe.cpp, src/game/online/lobby_island.cpp, src/game/online/lobby_parser.cpp, src/game/ui/intrface.cpp:
  OnlineIslandParkDownloadCompleteCallback 0x100b4420, PostGetTop10ParksListCallbackFunction 0x100b3e28,
  CLBScreenComponentIslands::ProcessCancel 0x100b1f3c, __sinit_lobby_parser_cpp 0x100b6ae4,
  CLBScreenComponentDownloadGlobe::Show 0x100ae590, CLBScreenComponentDownloadGlobe::ProcessCancel 0x100ae46c,
  CIntrface::DrawFPSCounter 0x1007dae8, __sinit_HLSprite_cpp 0x1007d53c, ESP_GetRandomActionFrom 0x1007c9f8.

A case is a world and a call. The world is built twice by one piece of Python: in emulator memory (big-endian) for the original and in the
host library (little-endian objects set through small setters, since host pointers are native) for the translation. Every callee outside the
unit is a fake on both sides recording its arguments (virtual slots go through a forged vtable on the PowerPC side). Compared: the answer,
the log, gOnlineSystem and its word 0x384, the FPS buffer and the file info state.
No fused operations. Not tested: a NULL screen or island (the original reads them unconditionally), NaN or infinite frame rates (the
C library's formatting of them differs between the two sprintf).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_OnlineIslandParkDownloadCompleteCallback_2"
MASK = 0xFFFFFFFF

F_DLCB, F_TOP10, F_ISLC, F_SINIT_L, F_SHOW, F_GCANCEL, F_FPS, F_SINIT_H, F_ESP = (
    0x100b4420, 0x100b3e28, 0x100b1f3c, 0x100b6ae4, 0x100ae590, 0x100ae46c, 0x1007dae8, 0x1007d53c, 0x1007c9f8)
SIZES = {F_DLCB: 72, F_TOP10: 68, F_ISLC: 100, F_SINIT_L: 148, F_SHOW: 112, F_GCANCEL: 220, F_FPS: 96, F_SINIT_H: 176, F_ESP: 104}
OWN = set(SIZES)

ADDR = dict(
    wide=0x102e4998, wideNode=0x102e49ac, folder=0x102e4978, folderNode=0x102e498c, file=0x102e4958, fileNode=0x102e496c,
    wdtor=0x103bcbb8, ndtor=0x103bcd38, hlArray=0x102dbdfc, ctorGlue=0x103bca78, dtorGlue=0x103bc914, arrDtor=0x102661f8,
    arrNode=0x102dc07c, espList=0x102dbd48, espNode=0x102dbd70, fileInfo=0x102db3ec, fiDtor=0x10265730, fiNode=0x102db514,
    printf=0x103bc8ec, gonline=0x10380ab0, screens=0x102e49b8, world=0x1037ebfc, groups=0x1029d368, sprites=0x102db520)
LABELED = ["wide", "wideNode", "folder", "folderNode", "file", "fileNode", "wdtor", "ndtor", "hlArray", "ctorGlue", "dtorGlue", "arrDtor",
           "arrNode", "espList", "espNode", "fileInfo", "fiDtor", "fiNode", "printf"]
OBJS = ["o%d" % i for i in range(8)]

ISVIS_CODE, GLOBE_BT_CODE, ISLANDS_4C_CODE = 0x100a0010, 0x100ae9c0, 0x100b45d0

HOST_LAYOUT = []
PERSIST = {}


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ctor(e):
    """The online system's constructor: records the new label, sets word 0x384 from the queue."""
    L = e.cl
    obj = e.r(3)
    L.rec("CFSOnlineSystem", (L.newlab(obj),))
    e.write(obj + 0x384, struct.pack(">I", L.ret("CFSOnlineSystem_f384") & MASK))


def fake(addr, name, spec, ret=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret))


fake(0x100a7d28, "PlayAnim", "Lii")
fake(0x10171404, "StopProcessing", "L")
fake(0x100a7128, "ClearAll", "L")
fake(0x100ad788, "OnlineGlobeShow", "L")
fake(0x1022b038, "CreateWorldLobbyController", "b")
fake(0x101e20f0, "CreateOnlineCityInfoOverlay", "")
fake(0x100b4a98, "DownloadFail", "L")
fake(0x100b4b48, "OnlineBacktrack", "L")
fake(0x100b4880, "CreateParks", "LL")
fake(0x1013b1e4, "GetRand", "L", "i")
_pf = CL.recorder("printf_xy", "Liis")


def pf_wrap(e):
    return _pf(e)


STUBS.internal(0x10238b30, pf_wrap)


def stub_sprintf(e):
    """sprintf(buf, fmt, double): the format must be the frame counter's; the double comes in r5/r6."""
    fmt = e.cstr(e.r(4))
    x = struct.unpack(">d", struct.pack(">II", e.r(5) & MASK, e.r(6) & MASK))[0]
    text = (fmt % x) if fmt == "FPS: %4g\n" else "BAD FORMAT " + fmt
    e.write(e.r(3), text.encode() + b"\0")
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
    e.set_r(3, e.r(3))


STUBS.internal(0x10238578, stub_sprintf)
fake(0x10238dd0, "CSpriteList", "L")
fake(0x10238800, "WString", "LS")
fake(0x102382d8, "NString", "Ls")
fake(0x10238758, "construct_array", "LLLii")
fake(0x10239940, "register", "LLL")
fake(ISVIS_CODE, "IsVisible", "L", "i")
fake(GLOBE_BT_CODE, "GlobeBacktrack", "L")
fake(ISLANDS_4C_CODE, "Slot4c", "L")
fake(0x102384b8, "operator_new", "i", lambda e, log: e.alloc(0x410))
STUBS.internal(0x1016f7d0, stub_ctor)


def setup(runner):
    T.install_std_stubs(runner.emu)
    for i in range(8):
        PERSIST["o%d" % i] = T.persist_alloc(runner.emu, 0x800)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {PERSIST[n]: i + 1 for i, n in enumerate(OBJS)}
        for i, n in enumerate(LABELED):
            fixed[ADDR[n]] = 20 + i
        fixed[e.u32(0x10260d54)] = 20 + LABELED.index("printf")   # the import's data word, as the TOC holds it
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return PERSIST[name] if name in PERSIST else ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    # world
    def set_gonline(self, p):
        self.words(ADDR["gonline"], [p])

    def get_gonline(self):
        return self.rd_word(ADDR["gonline"])

    def set_f384(self, sys, v):
        self.words(sys + 0x384, [v])

    def get_f384(self, sys):
        return self.rd_word(sys + 0x384)

    def set_screens(self, screens, advisor):
        self.words(ADDR["screens"], [screens])
        self.words(screens + 0x14, [advisor])

    def mk_vobj(self, obj, slots):
        vt = self.e.alloc(0x100)
        self.words(obj, [vt])
        for slot, code in slots.items():
            tv = self.e.alloc(8)
            self.words(tv, [code, 0])
            self.words(vt + slot, [tv])

    def isl_setup(self, screen, state, island, mesh):
        self.words(screen + 0x14, [state])
        self.words(screen + 0x0c, [island])
        self.words(island + 8, [mesh])

    def isl_state(self, screen):
        return self.rd_word(screen + 0x14)

    def set_world(self, w):
        self.words(ADDR["world"], [w])

    def set_group(self, i, first):
        self.words(ADDR["groups"] + 12 * i + 4, [first])

    def set_sprite(self, i, sprite, actions):
        self.words(ADDR["sprites"] + 4 * i, [sprite])
        self.write(sprite + 0x21e, struct.pack(">H", actions & 0xFFFF))

    def fps_fill(self, o):
        self.write(o + 0x10, b"\xaa" * 0x14)

    def fps_buf(self, o):
        return self.read(o + 0x10, 0x14)

    def fi_fill(self):
        self.write(ADDR["fileInfo"], b"\xaa" * 0x30)

    def fi_state(self):
        base = ADDR["fileInfo"]
        return [1 if self.rd_word(base + 0x20) == base + 0x24 else 0, self.read(base + 0x24, 1)[0]]

    # calls
    def call(self, fn, *args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK if isinstance(a, int) else a for a in args])
        return r3 & MASK

    def c_dlcb(self, err, ctx):
        return self.call(F_DLCB, err, ctx)

    def c_top10(self, err, ctx, lst):
        return self.call(F_TOP10, err, ctx, lst)

    def c_islc(self, o):
        return self.call(F_ISLC, o)

    def c_gcancel(self, o):
        return self.call(F_GCANCEL, o)

    def c_show(self, o):
        return self.call(F_SHOW, o)

    def c_fps(self, o, f):
        return self.call(F_FPS, o, T.F(f))

    def c_esp(self, s, i):
        return self.call(F_ESP, s, i)

    def c_sinit_l(self):
        return self.call(F_SINIT_L)

    def c_sinit_h(self):
        return self.call(F_SINIT_H)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = {i + 1: self.g(n) for i, n in enumerate(OBJS)}
        for i, n in enumerate(LABELED):
            fixed[20 + i] = self.g(n)
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_x_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_gonline(self, p):
        self.lib.tpw_x_set_gonline(p)

    def get_gonline(self):
        return self.lib.tpw_x_get_gonline()

    def set_f384(self, sys, v):
        self.lib.tpw_x_set_f384(sys, s32(v))

    def get_f384(self, sys):
        return self.lib.tpw_x_get_f384(sys) & MASK

    def set_screens(self, screens, advisor):
        self.lib.tpw_x_set_screens(screens, advisor)

    def mk_vobj(self, obj, slots):
        if 0x2c in slots:
            self.lib.tpw_x_mk_globe(obj)

    def isl_setup(self, screen, state, island, mesh):
        self.lib.tpw_x_isl_setup(screen, s32(state), island, mesh)

    def isl_state(self, screen):
        return self.lib.tpw_x_isl_state(screen) & MASK

    def set_world(self, w):
        self.lib.tpw_x_set_world(w)

    def set_group(self, i, first):
        self.lib.tpw_x_set_group(i, s32(first))

    def set_sprite(self, i, sprite, actions):
        self.lib.tpw_x_set_sprite(i, sprite, actions & 0xFFFF)

    def fps_fill(self, o):
        self.lib.tpw_x_fps_fill(o)

    def fps_buf(self, o):
        out = (C.c_uint8 * 0x14)()
        self.lib.tpw_x_fps_buf(o, out)
        return bytes(out)

    def fi_fill(self):
        self.lib.tpw_x_fi_fill()

    def fi_state(self):
        out = (C.c_int64 * 2)()
        self.lib.tpw_x_fi_state(out)
        return list(out)

    def c_dlcb(self, err, ctx):
        self.lib.tpw_x_dlcb(s32(err), ctx)
        return 0

    def c_top10(self, err, ctx, lst):
        self.lib.tpw_x_top10(s32(err), ctx, lst)
        return 0

    def c_islc(self, o):
        return self.lib.tpw_x_isl_cancel(o) & MASK

    def c_gcancel(self, o):
        return self.lib.tpw_x_globe_cancel(o) & MASK

    def c_show(self, o):
        self.lib.tpw_x_globe_show(o)
        return 0

    def c_fps(self, o, f):
        self.lib.tpw_x_fps(o, f)
        return 0

    def c_esp(self, s, i):
        return self.lib.tpw_x_esp(s32(s), s32(i)) & MASK

    def c_sinit_l(self):
        self.lib.tpw_x_sinit_lobby()
        return 0

    def c_sinit_h(self):
        self.lib.tpw_x_sinit_hl()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, i32, u32, f32 = C.c_uint64, C.c_int32, C.c_uint32, C.c_float
    sigs = {
        "addr": (u64, [C.c_char_p]), "alloc": (u64, [u32]), "layout": (i32, [C.POINTER(C.c_int64)]),
        "set_gonline": (None, [u64]), "get_gonline": (u64, []), "set_f384": (None, [u64, i32]), "get_f384": (i32, [u64]),
        "set_screens": (None, [u64, u64]), "mk_globe": (None, [u64]), "isl_setup": (None, [u64, i32, u64, u64]),
        "isl_state": (i32, [u64]), "set_world": (None, [u64]), "set_group": (None, [i32, i32]),
        "set_sprite": (None, [i32, u64, i32]), "fps_fill": (None, [u64]), "fps_buf": (None, [u64, C.POINTER(C.c_uint8)]),
        "fi_fill": (None, []), "fi_state": (None, [C.POINTER(C.c_int64)]),
        "dlcb": (None, [i32, u64]), "top10": (None, [i32, u64, u64]), "isl_cancel": (u32, [u64]), "globe_cancel": (u32, [u64]),
        "globe_show": (None, [u64]), "fps": (None, [u64, f32]), "esp": (i32, [i32, i32]), "sinit_lobby": (None, []),
        "sinit_hl": (None, []),
    }
    for name, (res, args) in sigs.items():
        fn = getattr(lib, "tpw_x_" + name)
        fn.restype, fn.argtypes = res, args
    out = (C.c_int64 * 32)()
    n = lib.tpw_x_layout(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world and the calls


def label_of(s, p):
    """A pointer as a small number the two memories share."""
    if p == 0:
        return 0
    for i, n in enumerate(OBJS):
        if p == s.g(n):
            return i + 1
    return -1


def execute(s, case):
    op = case["op"]
    o = [s.g(n) for n in OBJS]
    ex = {}
    ans = 0
    if op == "dlcb":
        s.mk_vobj(o[0], {0x4c: ISLANDS_4C_CODE})
        s.c_dlcb(case["err"], o[0])
    elif op == "top10":
        s.c_top10(case["err"], o[0], o[1])
    elif op == "islc":
        s.isl_setup(o[0], case["state"], o[6], o[5])
        ans = s.c_islc(o[0])
        ex["state"] = s.isl_state(o[0])
    elif op == "gcancel":
        s.mk_vobj(o[0], {0x4c: GLOBE_BT_CODE})
        if case["have"]:
            s.set_gonline(o[4])
            s.set_f384(o[4], case["f384"])
        else:
            s.set_gonline(0)
        ans = s.c_gcancel(o[0])
        gon = s.get_gonline()
        ex["gon"] = label_of(s, gon) if gon != 0 else 0
        ex["gon_f384"] = s.get_f384(gon) if gon else 0
    elif op == "show":
        s.mk_vobj(o[0], {0x2c: ISVIS_CODE})
        s.set_screens(o[3], o[2])
        s.c_show(o[0])
    elif op == "fps":
        s.fps_fill(o[0])
        s.c_fps(o[0], case["fps"])
        ex["buf"] = s.fps_buf(o[0]).hex()
    elif op == "esp":
        s.set_world(o[1])
        s.set_group(case["set"], case["first"])
        s.set_sprite(case["sprites"] + case["first"], o[5], case["actions"])
        ans = s.c_esp(case["set"], case["sprites"])
    elif op == "sinit_l":
        s.c_sinit_l()
    elif op == "sinit_h":
        s.fi_fill()
        s.c_sinit_h()
        ex["fi"] = s.fi_state()
    return ans, ex


def run(s, case):
    try:
        ans, ex = execute(s, case)
        d = {"answer": ans, "log": s.log()}
        d.update(ex)
        return d
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases


def mk(op, queues=None, **kw):
    c = {"op": op, "queues": queues or {}}
    c.update(kw)
    return c


def rerr(r):
    return r.choice([0, 1, -1, 2, 0x7FFFFFFF, r.randrange(-1000, 1000)])


def dlcb_edges():
    for e in (0, 1, -1, 2, 0x80000000):
        yield mk("dlcb", err=e)


def dlcb_random(r):
    return mk("dlcb", err=rerr(r))


def top10_edges():
    for e in (0, 1, -1, 2, 0x80000000):
        yield mk("top10", err=e)


def top10_random(r):
    return mk("top10", err=rerr(r))


def islc_edges():
    for st in (0, 1, 2, 3, -1, 0x80000000):
        yield mk("islc", state=st)


def islc_random(r):
    return mk("islc", state=r.choice([0, 1, 2, 3, -1, r.randrange(-5, 6)]))


def gcancel_case(r, have, f384, newf):
    return mk("gcancel", {"CFSOnlineSystem_f384": [newf]}, have=have, f384=f384)


def gcancel_edges():
    r = random.Random(11)
    for have in (0, 1):
        for f in (0, 1, -1, 0x80000000, 0x100):
            for nf in (0, 1, -1):
                yield gcancel_case(r, have, f, nf)


def gcancel_random(r):
    return gcancel_case(r, r.choice([0, 1]), r.choice([0, 1, -1, r.getrandbits(32)]), r.choice([0, 1, r.getrandbits(32)]))


def show_edges():
    for v in (0, 1, -1, 5, 0x80000000):
        yield mk("show", {"IsVisible": [v]})


def show_random(r):
    return mk("show", {"IsVisible": [r.choice([0, 1, -1, r.getrandbits(32)])]})


FPS = [0.0, 60.0, 59.94, 30.0, 1.0, -1.5, 0.1234567, 1e-10, 1e10, 123456.0, 3.4e38, -3.4e38, 1e-38, 99.99, 100.0, 9999.5, 0.5]


def fps_edges():
    for f in FPS:
        yield mk("fps", fps=T.f32(f))


def fps_random(r):
    return mk("fps", fps=T.f32(r.choice([r.uniform(0, 200), r.uniform(-1e6, 1e6), r.uniform(-1, 1) * 10 ** r.uniform(-30, 30)])))


def esp_case(r, set_, sprites, first, actions, rnd):
    return mk("esp", {"GetRand": [rnd]}, set=set_, sprites=sprites, first=first, actions=actions)


def esp_edges():
    r = random.Random(5)
    for actions in (0, 1, 2, 3, 7, 0xFFFF):
        for rnd in (0, 1, 3, 4, 5, 0xFFFFFFFF, 0x80000000, 1234567):
            yield esp_case(r, 1, 2, 3, actions, rnd)


def esp_random(r):
    return esp_case(r, r.randrange(0, 4), r.randrange(0, 6), r.randrange(0, 20), r.choice([0, 1, r.randrange(1, 60), r.getrandbits(16)]),
                    r.getrandbits(32))


def one(op):
    return lambda: iter([mk(op)])


TESTS = [
    FnTest("OnlineIslandParkDownloadCompleteCallback", ppc, host, edge=dlcb_edges, rnd=dlcb_random),
    FnTest("PostGetTop10ParksListCallbackFunction", ppc, host, edge=top10_edges, rnd=top10_random),
    FnTest("CLBScreenComponentIslands::ProcessCancel", ppc, host, edge=islc_edges, rnd=islc_random),
    FnTest("__sinit_lobby_parser_cpp", ppc, host, edge=one("sinit_l"), rnd=lambda r: mk("sinit_l")),
    FnTest("CLBScreenComponentDownloadGlobe::Show", ppc, host, edge=show_edges, rnd=show_random),
    FnTest("CLBScreenComponentDownloadGlobe::ProcessCancel", ppc, host, edge=gcancel_edges, rnd=gcancel_random),
    FnTest("CIntrface::DrawFPSCounter", ppc, host, edge=fps_edges, rnd=fps_random),
    FnTest("__sinit_HLSprite_cpp", ppc, host, edge=one("sinit_h"), rnd=lambda r: mk("sinit_h")),
    FnTest("ESP_GetRandomActionFrom", ppc, host, edge=esp_edges, rnd=esp_random),
]
