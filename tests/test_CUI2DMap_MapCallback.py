"""Unit v5526, the map panel's window message function, against libtpw_unit_CUI2DMap_MapCallback.dylib (tests/unit_CUI2DMap_MapCallback.cpp),
main app ("SimTheme Park"), src/CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_MapCallback.cpp:
  CUI2DMap::MapCallback 0x1022c7e8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee
(MoveCentre, Zoom, UpdateMap, UpdateMapZoom, CloseSprites, FreeMemory, SetFogColor, SetRenderOptions, Unpause, the timer, SendMessage, the
advisor, the panel, the keyboard capture, the help text, SetCameraPOI and the default window handler) is a fake on both sides that
records its arguments; the default handler and SendMessage answer from the case's queues. The two jump tables run as the original.

The world of a case: the map object (words 0 .. 0x5c), gGame, the RMB scroll setting, gGameWindow, gei_MapWho and the windows. What is
compared: the answer, the object's words, gGame's word 0x3c and the event log.

No fused operations; fctiwz only sees values below 2^31. Not tested: map sizes past 2^24 (fctiwz saturation differs in the emulator).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CUI2DMap_MapCallback"
MASK = 0xFFFFFFFF

FN = 0x1022c7e8
SIZES = {FN: 2180}
OWN = set(SIZES)

ADDR = dict(gGame=0x1037f134, settings=0x1038071c, gameWindow=0x103949e4, mapWho=0x103bc908, display=0x103bc8d0)
# the data imports (gei_Display, gei_MapWho) live where the TOC slots of the image say, not at the listing's addresses
TOC = dict(display=0x1026067c, mapWho=0x102607e4)
HOST_LAYOUT =[96, 0x38, 0x20100]    # gGame's word 0x3c is at 96 on the host (the class has a vtable and pointers)

OBJ_WORDS = 0x60 // 4


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def this_only(name):
    def fn(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)),))
    return fn


def this_int_float(name):
    def fn(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)), s32(e.r(4)), fb(e.f(1))))
    return fn


def stub_noargs(name):
    def fn(e):
        e.cl.rec(name, ())
    return fn


for addr, name in ((0x1022f4d4, "UpdateMapZoom"), (0x102307ec, "CloseSprites"), (0x1022db4c, "UpdateMap"), (0x101499d0, "Unpause")):
    STUBS.internal(addr, this_only(name))
STUBS.internal(0x1022f8a4, this_int_float("MoveCentre"))
STUBS.internal(0x1022fa1c, this_int_float("Zoom"))
STUBS.internal(0x101d8ba0, stub_noargs("UnPauseTimerSystem"))
STUBS.internal(0x1018116c, stub_noargs("DecideKbCapture"))


def stub_send(e):
    L = e.cl
    L.rec("SendMessage", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("SendMessage") & MASK)


def stub_advisor(e):
    e.cl.rec("CancelAdvisorMessage", (s32(e.r(3)),))


def stub_panel(e):
    e.cl.rec("SetPanel", (e.cl.lab(e.r(3)),))


def stub_help(e):
    e.cl.rec("MoveHelpText", (T.s16(e.r(3)), T.s16(e.r(4))))


def stub_poi(e):
    L = e.cl
    x = struct.unpack(">I", e.read(e.r(3), 4))[0]
    z = struct.unpack(">I", e.read(e.r(4), 4))[0]
    L.rec("SetCameraPOI", (x, z, e.r(5), e.r(6), e.r(7)))


def stub_default(e):
    L = e.cl
    L.rec("Default", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("Default") & MASK)


STUBS.internal(0x101c9518, stub_send)
STUBS.internal(0x1018227c, stub_advisor)
STUBS.internal(0x101805a0, stub_panel)
STUBS.internal(0x1018c7a4, stub_help)
STUBS.internal(0x10033650, stub_poi)
STUBS.internal(0x101de318, stub_default)


def save_toc(e):
    """What the glue does first: r2 into the caller's frame word 0x14 (the call site reloads it after the bl)."""
    pass


def imp_free(e):
    save_toc(e)
    e.cl.rec("FreeMemory", (e.cl.lab(e.r(3)),))


def imp_fog(e):
    save_toc(e)
    e.cl.rec("SetFogColor", (e.cl.lab(e.r(3)), e.r(4) & MASK))


def imp_opts(e):
    save_toc(e)
    e.cl.rec("SetRenderOptions", (e.cl.lab(e.r(3)), s32(e.r(4))))


STUBS.imp("FreeMemory__12TbRenderAreaFv", imp_free)
STUBS.imp("SetFogColor__8CDisplayFUl", imp_fog)
STUBS.imp("SetRenderOptions__7CMapWhoFi", imp_opts)

_dumped = []


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.off3c = 0x3c
        self.map = e.alloc(0x100)
        self.blocks = {k: e.alloc(0x40) for k in ("win", "gw", "panel", "mw")}
        fixed = {self.map: 1, self.map + 0x2c: 2, self.g("display"): 3, ADDR["gGame"]: 4, self.blocks["win"]: 5, self.blocks["gw"]: 6,
                 self.blocks["panel"]: 7, self.blocks["mw"]: 8}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def ptr_word(self, address):
        return address

    def native_ptr(self, where, block):
        self.write(where, struct.pack(">I", block))

    def g(self, name):
        return self.e.u32(TOC[name]) if name in TOC else ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, message, a, b):
        r3, _, _ = T.call_ppc(self.e, FN, [self.map, self.blocks["win"], message & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off3c = HOST_LAYOUT[0]
        self.map = lib.tpw_mc_alloc(0x20100)
        self.blocks = {k: lib.tpw_mc_alloc(0x40) for k in ("win", "gw", "panel", "mw")}
        fixed = {1: self.map, 2: self.map + 0x2c, 3: self.g("display"), 4: self.g("gGame"), 5: self.blocks["win"], 6: self.blocks["gw"],
                 7: self.blocks["panel"], 8: self.blocks["mw"]}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def native_ptr(self, where, block):
        """A global pointer: a native 8-byte pointer on the host."""
        self.write(where, struct.pack("<Q", block))

    def ptr_word(self, address):
        """The Ptr32 word for a block: the host's is an arena offset."""
        return self.lib.tpw_mc_offset(address)

    def g(self, name):
        return self.lib.tpw_mc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_mc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, message, a, b):
        return self.lib.tpw_mc_call(self.map, self.blocks["win"], s32(message), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_mc_addr.restype, lib.tpw_mc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_mc_alloc.restype, lib.tpw_mc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_mc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_mc_offset.restype, lib.tpw_mc_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_mc_call.restype = C.c_uint32
    lib.tpw_mc_call.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_mc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    o = case["obj"]
    ws = [0] * OBJ_WORDS
    for k, v in o.items():
        ws[k // 4] = v
    ws[1] = s.ptr_word(s.blocks["panel"])
    s.words(s.map, ws)
    s.words(s.g("gGame") + s.off3c, [case["g3c"]])
    s.write(s.g("settings") + 0x38, bytes([case["rmb"]]))
    s.native_ptr(s.g("gameWindow"), s.blocks["gw"])
    s.native_ptr(s.g("mapWho"), s.blocks["mw"])


def observe(s, case, answer):
    obj = s.rd_words(s.map, OBJ_WORDS)
    if obj[1] == s.ptr_word(s.blocks["panel"]):
        obj[1] = 0xFFFF0007           # the panel pointer as its label
    return {"answer": answer, "log": s.log(), "obj": obj, "g3c": s.rd_word(s.g("gGame") + s.off3c)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["msg"], case["a"], case["b"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


MESSAGES = [0x10001, 0x10003, 0x10008, 0x10009, 0x1000a, 0x1000b, 0x10002, 0x10004, 0x10007, 0x1000c, 0x1000d, 0x14, 0x15, 0x1e, 0x100,
            0x101, 0x13, 0x16, 0x1d, 0x1f, 0xff, 0x102, 0, 1, 0x10000, 0x10010, -1, 0x7fffffff]
KEYS = {
    0x10008: list(range(0x97f, 0x98d)),
    0x10009: list(range(0x97f, 0x98d)),
    0x1000a: [0x2500, 0x2600, 0x2700, 0x2800, 0x6b00, 0x6d00, 0x2400, 0x2501, 0x2900, 0x6c00, 0x6a00, 0x6e00, 0, -1],
    0x1000b: [0x1b, 0x1a, 0x1c, 0, -1],
    0x100: list(range(0x97f, 0x99a)) + [-1, -2, 0, 0x1b, 0x2500],
    0x101: list(range(0x98a, 0x9a0)) + [-1, 0],
}
BS = {
    0x1000b: [0, 0x10000, 0xff0000, 0x1000000, 0x1ff0000, 0x12345, -1],
    0x101: [0, 1, 2, -1],
}


def rbits(r):
    k = r.random()
    if k < 0.1:
        return fb(r.choice([0.0, -0.0, float("nan"), float("inf"), float("-inf"), -1.0, 1.0, 0.5, -0.5]))
    if k < 0.4:
        return fb(r.uniform(-3, 140))
    if k < 0.7:
        return fb(float(r.randrange(-3, 140)))
    return fb(r.uniform(-1000, 1000))


def base(r):
    pos = r.choice([lambda: r.randrange(0, 2049), lambda: r.randrange(0, 1537), lambda: r.choice([0, 20, 21, 2027, 2028, 1535, 15, 16, 1521, 1522, 2048, -1, 3000]),
                    lambda: (r.randrange(0, 2048) << 16) | r.randrange(0, 1536)])
    obj = {8: r.randrange(0, 1 << 32), 0xc: r.randrange(-5, 1 << 31), 0x10: pos(), 0x14: pos(), 0x18: r.randrange(0, 1 << 8) & MASK,
           0x1c: r.randrange(0, 6), 0x20: r.randrange(0, 5), 0x24: r.choice([r.randrange(-2, 130), 128, 1, 0]), 0x28: r.choice([r.randrange(-2, 130), 128, 1, 0]),
           0x54: rbits(r), 0x58: rbits(r)}
    return {"msg": 0, "a": 0, "b": 0, "obj": {k: v & MASK for k, v in obj.items()}, "g3c": r.randrange(0, 3), "rmb": r.choice([0, 0, 1, 2, 0x80]),
            "queues": {"Default": [r.choice([0, 1, -1, 0x1234])], "SendMessage": [r.choice([0, 5])]}}


def mk(r, msg, a=None, b=None):
    c = base(r)
    c["msg"] = msg
    keys = KEYS.get(msg)
    c["a"] = (r.choice(keys) if keys and r.random() < 0.9 else r.choice([0, 1, -1, r.randrange(0, 1 << 16), r.randrange(-(1 << 31), 1 << 31)])) if a is None else a
    bs = BS.get(msg)
    c["b"] = (r.choice(bs) if bs and r.random() < 0.8 else r.choice([0, 1, -1, r.randrange(-(1 << 31), 1 << 31)])) if b is None else b
    return c


def edges():
    r = random.Random(5526)
    for msg in MESSAGES:
        keys = KEYS.get(msg, [0, 1, -1, 0x12345678, 0x00050007, 0xffff, 0x10000, 0x7fffffff, -0x80000000])
        for a in keys:
            for b in BS.get(msg, [0, 1]):
                yield mk(r, msg, a, b)
    # the mouse messages near the edges of the window
    for x in (0, 20, 21, 1000, 2027, 2028, 2048, 3000, -5):
        for y in (0, 15, 16, 700, 1521, 1522, 1536, 2000, -5):
            for rmb in (0, 1):
                c = mk(r, 0x1e, 0, 0)
                c["obj"][0x10] = x & MASK
                c["obj"][0x14] = y & MASK
                c["rmb"] = rmb
                yield c
    # the centre clamp
    for x in (-1.0, 0.0, -0.0, 5.0, 127.0, 128.0, 200.0, float("nan"), float("inf"), float("-inf"), 7.5):
        for size in (0, 1, 128):
            c = mk(r, 0x100, -1, 0)
            c["obj"][0x54] = fb(x)
            c["obj"][0x58] = fb(x)
            c["obj"][0x24] = size
            c["obj"][0x28] = size
            yield c
    # the packed mouse position
    for a in (0, 0x10001, 0xffffffff, 0x12345678, 0x80000000):
        for msg in (0x10001, 0x10003):
            yield mk(r, msg, a if a < 1 << 31 else a - (1 << 32), 0)


def rnd(r):
    return mk(r, r.choice(MESSAGES))


TESTS = [
    FnTest("CUI2DMap::MapCallback", ppc, host, edge=edges, rnd=rnd),
]
