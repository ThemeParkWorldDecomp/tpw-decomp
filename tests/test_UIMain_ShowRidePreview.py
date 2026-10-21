"""Unit v5013: the ride and sprite previews, MAKECOLUMNBUTTON and UIMain_ZoomToThing, against libtpw_unit_UIMain_ShowRidePreview.dylib
(tests/unit_UIMain_ShowRidePreview.cpp), main app ("SimTheme Park"), src/game/ui/ui_main.cpp:
  UIMain_ShowRidePreview 0x10181400, UIMain_ShowSpritePreview 0x10181648, MAKECOLUMNBUTTON 0x101818e4, UIMain_ZoomToThing 0x10181b00

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. The callees outside the unit (CalcZTrans, CreateRidePreview,
DestroyRidePreview, UIMain_DestroySpritePreview, ESP_Spawn2DCopyOfScriptAt, the InterfaceWindow setters, the sprite constructor,
FindDrawObject, GAME_ASSERTMSG, SetCameraPOI) are fakes on both sides that record their arguments and answer from the case's queues.

The world of a case: the rectangle, the preview globals, the peep block, gText's resolution group, the window block, the thing table entries.
What is compared: the event log and the globals the call may change (the window's help text word too).

No fused operations. Not tested: sprite scales whose quarter is beyond 2^31 (fctiwz saturation differs in the emulator); thing ids beyond the
0x2800 entries of the table.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UIMain_ShowRidePreview"
MASK = 0xFFFFFFFF

FN_RIDE, FN_SPRITE, FN_COL, FN_ZOOM = 0x10181400, 0x10181648, 0x101818E4, 0x10181B00
OWN = {FN_RIDE, FN_SPRITE, FN_COL, FN_ZOOM}

CALC_Z, CREATE_RIDE, DESTROY_RIDE = 0x101879D0, 0x1006C3BC, 0x1006CCD0
DESTROY_SPRITE, ESP_SPAWN, ITOS_CTOR = 0x10181864, 0x100DA590, 0x1018D11C
SET_CLIENT_OBJ, FIND_DRAW, SET_DRAW = 0x101DB624, 0x101CD2D4, 0x101DB474
FONT_JUST, FONT_WRAP, SET_FONT, FONT_COLOUR = 0x101DA204, 0x101DA2D8, 0x101DA118, 0x101DA3AC
TEXT_UP, TEXT_DOWN, CLIENT_TEXT = 0x101CB518, 0x101CB584, 0x101DE130
ASSERT, SET_POI = 0x1000DDE8, 0x10033650

GTEXT = 0x1037FF18
FONT_BASE = GTEXT + 0x1C
ADDR = dict(ride=0x10380C44, peep=0x10380C3C, handle=0x10380C40, skip=0x102A7866, arr=0x1034CBFC)
HOST_OFF = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_calc(e):
    L = e.cl
    L.rec("CalcZTrans", (s32(e.r(3)),))
    e.set_f1(bitsf(L.ret("CalcZTrans")))


def stub_create(e):
    L = e.cl
    L.rec("CreateRidePreview", tuple(canon_f(fb(e.f(i))) for i in range(1, 6)) + (e.r(8) & MASK,))
    e.set_r(3, L.ret("CreateRidePreview") & MASK)


def stub_esp(e):
    L = e.cl
    L.rec("ESP_Spawn2DCopyOfScriptAt", (s32(e.r(3)),) + tuple(canon_f(fb(e.f(i))) for i in range(1, 5)))
    e.set_r(3, L.ret("ESP_Spawn2DCopyOfScriptAt") & MASK)


def stub_set_font(e):
    L = e.cl
    p = e.r(4) & MASK
    idx = -1 if p == 0 else (p - FONT_BASE) // 0x18
    L.rec("SetFont", (L.lab(e.r(3)), idx))


def stub_poi(e):
    L = e.cl
    L.rec("SetCameraPOI", (fb(e.f32(e.r(3))), fb(e.f32(e.r(4))), e.r(5) & MASK, e.r(6) & MASK, e.r(7) & MASK))


STUBS.internal(CALC_Z, stub_calc)
STUBS.internal(CREATE_RIDE, stub_create)
STUBS.internal(DESTROY_RIDE, CL.recorder("DestroyRidePreview", "u"))
STUBS.internal(DESTROY_SPRITE, CL.recorder("UIMain_DestroySpritePreview", ""))
STUBS.internal(ESP_SPAWN, stub_esp)
STUBS.internal(ITOS_CTOR, CL.recorder("InterfaceTextObjectSprite", "Nu"))
STUBS.internal(SET_CLIENT_OBJ, CL.recorder("SetClientDrawObject", "LL"))
STUBS.internal(FIND_DRAW, CL.recorder("FindDrawObject", "s", "i"))
STUBS.internal(SET_DRAW, CL.recorder("SetDrawObject", "Lu"))
STUBS.internal(FONT_JUST, CL.recorder("SetFontJust", "Lii"))
STUBS.internal(FONT_WRAP, CL.recorder("SetFontWrap", "Lii"))
STUBS.internal(SET_FONT, stub_set_font)
STUBS.internal(FONT_COLOUR, CL.recorder("SetFontColour", "Liiii"))
STUBS.internal(TEXT_UP, CL.recorder("SetTextUpColour", "Lu"))
STUBS.internal(TEXT_DOWN, CL.recorder("SetTextDownColour", "Lu"))
STUBS.internal(CLIENT_TEXT, CL.recorder("SetClientDrawText", "LS"))
STUBS.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(SET_POI, stub_poi)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def ptr(self, a, v):
        """A pointer-sized global: four bytes on the Mac, eight on the host."""
        if self.fmt == "<I":
            self.write(a, struct.pack("<Q", v & MASK))
        else:
            self.words(a, [v])

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rect(self, l, t, r, b):
        a = self.alloc(32)
        self.write(a + self.rect_off, struct.pack(self.fmt[0] + "hhhh", l, t, r, b))
        return a

    def utf16(self, s):
        return self.mk(("".join(s) + "\0").encode("utf-16-be" if self.fmt[0] == ">" else "utf-16-le"))

    def mk(self, data):
        a = self.alloc(len(data) + 8)
        self.write(a, data)
        return a


class Ppc(Side):
    fmt = ">I"
    rect_off = 4
    help_off = 0x8C

    def __init__(self, e, case):
        self.e = e
        self.win = e.alloc(0x100)
        self.L = e.cl = CL.PpcLog({self.win: 1}, case["queues"])
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

    def set_thing(self, idx, p):
        self.words(ADDR["arr"] + idx * 20, [p])

    def set_group(self, v):
        self.words(GTEXT + 4, [v])

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.rect_off = HOST_OFF["rect"]
        self.help_off = HOST_OFF["help"]
        self.win = lib.tpw_mct_alloc(0x100)
        CL.host_begin(lib, {1: self.win}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_mct_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_mct_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_thing(self, idx, p):
        self.lib.tpw_mct_setthing(idx, p)

    def set_group(self, v):
        self.lib.tpw_mct_setgroup(v)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_mct_"
    lib.tpw_mct_addr.restype, lib.tpw_mct_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_mct_alloc.restype, lib.tpw_mct_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_mct_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_mct_setthing.argtypes = [C.c_uint32, C.c_uint64]
    lib.tpw_mct_setgroup.argtypes = [C.c_uint32]
    lib.tpw_mct_ride.argtypes = [C.c_uint64, C.c_uint32, C.c_int32]
    lib.tpw_mct_sprite.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_float]
    lib.tpw_mct_column.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_mct_zoom.argtypes = [C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_mct_layout(out)
    HOST_OFF["rect"], HOST_OFF["help"] = list(out)[:n]
    lib.bound = True


def build(s, case):
    op = case["op"]
    if op == "ride":
        s.ptr(s.g("ride"), case["old"])
    elif op == "sprite":
        s.ptr(s.g("peep"), case["old"])
        s.words(s.g("handle"), [case["oldh"]])
    elif op == "col":
        s.set_group(case["group"])
        s.words(s.win + s.help_off, [0x12345678])
    else:
        s.half(s.g("skip"), case["skip"])
        s.set_thing(0, 0)
        if case["e0"]:
            s.set_thing(0, s.mk(b"\0" * 16))
        s.thing = None
        if case["thing"] is not None:
            s.thing = s.mk(bytes([0, 0, 0, 0, case["thing"][0], 0, case["thing"][1], 0] + [0] * 8))
            s.set_thing(case["id"], s.thing)
        else:
            if case["id"] != 0:
                s.set_thing(case["id"], 0)


def execute(s, case):
    op = case["op"]
    if op == "ride":
        rect = s.rect(*case["rect"])
        if isinstance(s, Ppc):
            s.call(FN_RIDE, [rect, case["rid"], case["z"] & MASK])
        else:
            s.lib.tpw_mct_ride(rect, case["rid"], case["z"])
    elif op == "sprite":
        rect = s.rect(*case["rect"])
        s.peep = s.mk(b"\0" * 12 + struct.pack(s.fmt[0] + "I", case["handle"] & MASK) + b"\0" * 16)
        if isinstance(s, Ppc):
            T.call_ppc(s.e, FN_SPRITE, [rect, s.peep, case["z"] & MASK, T.F(case["scale"])])
        else:
            s.lib.tpw_mct_sprite(rect, s.peep, case["z"], case["scale"])
    elif op == "col":
        text = s.utf16(case["text"])
        a = case["args"]
        if isinstance(s, Ppc):
            s.call(FN_COL, [s.win, text, a[0] & MASK, a[1] & MASK, a[2] & MASK, a[3] & MASK, a[4] & MASK])
        else:
            s.lib.tpw_mct_column(s.win, text, *a)
    else:
        if isinstance(s, Ppc):
            s.call(FN_ZOOM, [case["id"] << 16])
        else:
            s.lib.tpw_mct_zoom(case["id"])


def observe(s, case):
    op = case["op"]
    out = {"log": s.log()}
    if op == "ride":
        out["ride"] = s.rd_word(s.g("ride"))
    elif op == "sprite":
        p = s.rd_word(s.g("peep"))
        out["peep"] = 0 if p == 0 else (1 if p == s.peep & MASK else 2)
        out["handle"] = s.rd_word(s.g("handle"))
    elif op == "col":
        out["help"] = s.rd_word(s.win + s.help_off)
    return out


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


# ---------------------------------------------------------------------------------------------------------------- cases


def ride_case(r, rect=None, old=None):
    zf = fb(r.uniform(-50, 50))
    if rect is None:
        rect = tuple(r.randrange(-3000, 3000) for _ in range(4))
    return {"op": "ride", "rect": rect, "rid": r.choice([0, 1, 0xFFFFFFFF, r.randrange(1 << 32)]), "z": r.choice([0, -1, r.randrange(-1000, 1000)]),
            "old": r.choice([0, r.randrange(1, 1 << 31)]) if old is None else old,
            "queues": {"CalcZTrans": [zf], "CreateRidePreview": [r.randrange(1, 1 << 31)]}}


def ride_edges():
    r = random.Random(11)
    yield ride_case(r, (0, 0, 0, 0), 0)
    yield ride_case(r, (0x400, 0x300, 0x800, 0x600), 5)
    yield ride_case(r, (-32768, 32767, 32767, -32768), 0)
    yield ride_case(r, (1024, 768, 1024, 768), 0x7FFFFFFF)


def sprite_case(r, handle=None, old=None, scale=None):
    return {"op": "sprite", "rect": tuple(r.randrange(-3000, 3000) for _ in range(4)),
            "handle": r.choice([0, 1, r.randrange(1, 1 << 20)]) if handle is None else handle,
            "z": r.choice([0, -1, r.randrange(-1000, 1000)]),
            "scale": T.f32(r.uniform(-100, 3000)) if scale is None else scale,
            "old": r.choice([0, r.randrange(1, 1 << 31)]) if old is None else old, "oldh": r.randrange(1 << 20),
            "queues": {"CalcZTrans": [fb(r.uniform(-50, 50))], "ESP_Spawn2DCopyOfScriptAt": [r.randrange(1, 1 << 20)]}}


def sprite_edges():
    r = random.Random(12)
    for h in (0, 5):
        for old in (0, 77):
            yield sprite_case(r, h, old)
    yield sprite_case(r, 5, 0, 0.0)
    yield sprite_case(r, 5, 0, -0.0)
    yield sprite_case(r, 5, 0, 3.9)
    yield sprite_case(r, 5, 0, -3.9)


def col_case(r, font=None):
    return {"op": "col", "group": r.randrange(4), "text": "".join(r.choice("abcXYZ 019") for _ in range(r.randrange(0, 12))),
            "args": [r.choice([0, 1, 12, 13, 14, -1, 0x7FFFFFFF, r.randrange(0, 13)]) if font is None else font] +
                    [r.choice([0, 255, 256, -1, r.randrange(0, 256)]) for _ in range(3)] + [r.choice([0, -1, r.randrange(1 << 30)])],
            "queues": {"FindDrawObject": [r.randrange(1, 1 << 30)]}}


def col_edges():
    r = random.Random(13)
    for f in (0, 12, 13, -1, 5):
        yield col_case(r, f)


def zoom_case(r, skip=None, thing=-1, e0=None):
    cid = r.choice([0, 1, 0x27FF, r.randrange(0x2800)])
    if thing == -1:
        thing = r.choice([None, (r.randrange(256), r.randrange(256))])
    return {"op": "zoom", "id": cid, "skip": cid if skip == "same" else (r.randrange(0x10000) if skip is None else skip),
            "thing": thing, "e0": r.choice([0, 1]) if e0 is None else e0, "queues": {}}


def zoom_edges():
    r = random.Random(14)
    yield zoom_case(r, "same", (1, 1))
    yield zoom_case(r, 0x3000, None, 0)
    yield zoom_case(r, 0x3000, (0, 0), 0)
    yield zoom_case(r, 0x3000, (255, 255), 1)
    yield zoom_case(r, 0x3000, (127, 1), 0)
    yield zoom_case(r, 0x3000, (128, 0), 0)


TESTS = [
    FnTest("UIMain_ShowRidePreview", ppc, host, edge=ride_edges, rnd=lambda r: ride_case(r)),
    FnTest("UIMain_ShowSpritePreview", ppc, host, edge=sprite_edges, rnd=lambda r: sprite_case(r)),
    FnTest("MAKECOLUMNBUTTON", ppc, host, edge=col_edges, rnd=lambda r: col_case(r)),
    FnTest("UIMain_ZoomToThing", ppc, host, edge=zoom_edges, rnd=lambda r: zoom_case(r)),
]
