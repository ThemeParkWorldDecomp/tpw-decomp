"""Unit v51012, the ui_utils helpers, against libtpw_unit_UI_DrawPostcardText_2.dylib (tests/unit_UI_DrawPostcardText_2.cpp), main app
("SimTheme Park"), src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_utils.cpp:
  UI_DrawPostcardText 0x1018bbc4, __sinit_ui_utils_cpp 0x1018bda8, MakeRenderAreaFromRAW 0x1018b434,
  InterfaceDrawEngineHGradient::CalcUVs 0x10189d70, InterfaceVisitorTimeTextObject::Change 0x1018efcc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit are fakes on both sides
that record their arguments: LbPalette_FindColour (answers from the case), TbFontRenderParams::UseBlendedMode and UseAlphaWriteMode (the
object), TbRenderArea::TbRenderArea, UI_DrawText (two events: its arguments, then the stack argument, the in-use word and the render area at
the time of the call), TbITable::FormatString (the tag it is given; it writes a short text made from it) and InterfaceTextObjectSprite::Change
(its flags and the text, or the change's label when passed through). MakeRenderAreaFromRAW runs as the original inside UI_DrawPostcardText.

The world of a case: the render area (ten random words), the in-use word, the opaque colour, the time change block. What is compared: the
event log, the render area, the in-use word, the colour, the uv block. No fused operations. Not tested: a NULL change or a NULL table in
the time object's Change (the original reads them unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UI_DrawPostcardText_2"
MASK = 0xFFFFFFFF

FN_POST, FN_SINIT, FN_RAW, FN_UVS, FN_TIME = 0x1018BBC4, 0x1018BDA8, 0x1018B434, 0x10189D70, 0x1018EFCC
SIZES = {FN_POST: 288, FN_SINIT: 104, FN_RAW: 244, FN_UVS: 76, FN_TIME: 444}
OWN = set(SIZES)

CALLEE_FIND, CALLEE_BLEND, CALLEE_ALPHA, CALLEE_AREA_CTOR = 0x10238BF0, 0x1023A060, 0x10238EC0, 0x10238E30
CALLEE_DRAWTEXT, CALLEE_FORMAT, CALLEE_SPRITE = 0x1018B578, 0x10239730, 0x1018D798

ADDR = dict(area=0x1038CD00, inuse=0x1038CD28, colour=0x1038CD2C, palette=0x103BCD28, rparams=0x1037FF18 + 0x4FC)
FAKE_CHANGE, FAKE_SELF, FAKE_TABLE = 0x30000000, 0x30002000, 0x30003000
TABLE_SLOT = 0x1037FF18 + 0x564
VTAG_VT = 0x103BCAF0
HOST_LAYOUT = [0x20]


def s32(v):
    return T.s32(v)


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_find(e):
    L = e.cl
    pal = e.r(3)
    L.rec("FindColour", (L.lab(ADDR["palette"] if pal == e.u32(0x10260D44) else pal), e.r(4) & 0xFF, e.r(5) & 0xFF, e.r(6) & 0xFF))
    e.set_r(3, L.ret("FindColour") & 0xFF)


def stub_blend(e):
    e.cl.rec("UseBlendedMode", (e.cl.lab(e.r(4)),))


def stub_alpha(e):
    e.cl.rec("UseAlphaWriteMode", (e.cl.lab(e.r(4)),))


def stub_area_ctor(e):
    e.cl.rec("TbRenderArea", (e.cl.lab(e.r(3)),))


def stub_drawtext(e):
    L = e.cl
    rect = e.read(e.r(4), 16)
    l, t, r, b = struct.unpack(">4i", rect)
    a, red, green, blue, idx = e.read(e.r(5), 5)
    packed = (a << 24) | (red << 16) | (green << 8) | blue
    L.rec("UI_DrawText", (e.r(3) & MASK, l, t, r, b, packed, idx, e.r(6) & 0xFF, e.r(7) & 0xFF, e.r(8) & 0xFF, s32(e.r(9)),
                          L.lab(e.r(10))))
    on_current = s32(e.u32(e.r(1) + 0x38))
    w = struct.unpack(">10I", e.read(ADDR["area"], 40))
    L.rec("UI_DrawText2", (on_current, s32(e.u32(ADDR["inuse"])), w[0], w[1], w[2], w[3], s32(w[4]), s32(w[5]), w[6], s32(w[7]),
                           s32(w[8]), s32(w[9])))


def stub_format(e):
    L = e.cl
    tag = e.r(7)
    magic, vptr, tid, value, flag = struct.unpack(">5I", e.read(tag, 20))
    L.rec("FormatString", (L.lab(e.r(3)), e.r(5) & MASK, e.r(6) & MASK, magic, tid, s32(value), s32(flag), 1 if vptr == e.u32(0x10261344) else 0))
    v = s32(value)
    chars = [0x46, 0x41 + (e.r(6) & 0xF), 0x61 + (v & 15), 0x61 + ((v >> 4) & 15), 0x30 + (s32(flag) & 1), 0x61 + (tid & 15), 0]
    e.write(e.r(4), b"".join(struct.pack(">H", c) for c in chars))


def stub_sprite(e):
    L = e.cl
    change = e.r(5)
    if change == FAKE_CHANGE:
        L.rec("SpriteChange", (L.lab(e.r(3)), e.r(4) & MASK, L.lab(change), 0, 0, 0, 0, 0, 0))
        return
    style = e.u32(change + 0x10)
    text = e.u32(style)
    cs = []
    while len(cs) < 0x40:
        c = struct.unpack(">H", e.read(text + 2 * len(cs), 2))[0]
        if c == 0:
            break
        cs.append(c)
    c5 = (cs + [0] * 5)[:5]
    L.rec("SpriteChange", (L.lab(e.r(3)), e.r(4) & MASK, 0, *c5, len(cs)))


STUBS.imp("LbPalette_FindColour__FPC9TbPaletteUcUcUc", stub_find)
STUBS.imp("UseBlendedMode__18TbFontRenderParamsFv", stub_blend)
STUBS.imp("UseAlphaWriteMode__18TbFontRenderParamsFv", stub_alpha)
STUBS.imp("__ct__12TbRenderAreaFv", stub_area_ctor)
STUBS.internal(CALLEE_DRAWTEXT, stub_drawtext)
STUBS.imp("FormatString__8TbITableCFPwUiUle", stub_format)
STUBS.internal(CALLEE_SPRITE, stub_sprite)


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


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["area"]: 1, ADDR["rparams"]: 2, ADDR["palette"]: 3, FAKE_SELF: 4, FAKE_TABLE: 5,
                                   FAKE_CHANGE: 6}, case["queues"])
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

    def set_area(self, ws):
        self.e.write(ADDR["area"], struct.pack(">10I", *ws))

    def get_area(self):
        return list(struct.unpack(">10I", self.e.read(ADDR["area"], 40)))

    def set_table(self):
        self.e.write(TABLE_SLOT, struct.pack(">I", FAKE_TABLE))

    def call(self, entry, args):
        T.call_ppc(self.e, entry, [a & MASK for a in args])

    def uvs_this(self, frac, uvw):
        this = self.alloc(0x130)
        blk = self.alloc(0x40)
        self.words(this + 0x120, [frac])
        self.words(this + 0x84, [blk])
        self.words(blk, uvw)
        return this, blk

    def call_post(self, a):
        self.call(FN_POST, a)

    def call_sinit(self):
        self.call(FN_SINIT, [])

    def call_raw(self, pix, w, h, ty, use):
        self.call(FN_RAW, [pix, w, h, ty, ADDR["area"] if use else 0])

    def call_uvs(self, frac, uvw):
        this, blk = self.uvs_this(frac, uvw)
        self.call(FN_UVS, [this])
        return self.rd_words(blk, 16)

    def call_time(self, flags, t, passthrough):
        if passthrough:
            change = FAKE_CHANGE
        else:
            change = self.alloc(0x28)
            self.write(change, struct.pack(">I", t & MASK))
        self.call(FN_TIME, [FAKE_SELF, flags, change])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("area"), 2: self.g("rparams"), 3: self.g("palette"), 4: FAKE_SELF, 5: FAKE_TABLE, 6: FAKE_CHANGE},
                      case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ui2_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ui2_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_area(self, ws):
        self.lib.tpw_ui2_area_set((C.c_uint32 * 10)(*[w & MASK for w in ws]))

    def get_area(self):
        out = (C.c_uint32 * 10)()
        self.lib.tpw_ui2_area_get(out)
        return list(out)

    def set_table(self):
        self.lib.tpw_ui2_table_set(FAKE_TABLE)

    def call_post(self, a):
        text, pix, w, h, l, t, r, b, colour, extra = a
        self.lib.tpw_ui2_postcard(text & MASK, pix & MASK, s32(w), s32(h), s32(l), s32(t), s32(r), s32(b), colour & MASK, s32(extra))

    def call_sinit(self):
        self.lib.tpw_ui2_sinit()

    def call_raw(self, pix, w, h, ty, use):
        self.lib.tpw_ui2_raw(pix & MASK, s32(w), s32(h), s32(ty), 1 if use else 0)

    def call_uvs(self, frac, uvw):
        blk = self.alloc(0x40)
        self.words(blk, uvw)
        self.lib.tpw_ui2_calcuvs(frac & MASK, blk)
        return self.rd_words(blk, 16)

    def call_time(self, flags, t, passthrough):
        if passthrough:
            change = FAKE_CHANGE
        else:
            change = self.alloc(0x28)
            self.write(change, struct.pack(">I", t & MASK))
        self.lib.tpw_ui2_time(FAKE_SELF, flags & MASK, change)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ui2_"
    lib.tpw_ui2_addr.restype, lib.tpw_ui2_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ui2_alloc.restype, lib.tpw_ui2_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ui2_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ui2_area_set.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_ui2_area_get.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_ui2_table_set.argtypes = [C.c_uint64]
    lib.tpw_ui2_postcard.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32,
                                     C.c_uint32, C.c_int32]
    lib.tpw_ui2_raw.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_ui2_calcuvs.argtypes = [C.c_uint32, C.c_uint64]
    lib.tpw_ui2_time.argtypes = [C.c_uint64, C.c_uint32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ui2_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.set_area(case["area"])
    s.words(s.g("inuse"), [case["inuse"]])
    s.write(s.g("colour"), bytes(case["colour"]))
    s.set_table()


def execute(s, case):
    op = case["op"]
    if op == "post":
        s.call_post(case["args"])
    elif op == "sinit":
        s.call_sinit()
    elif op == "raw":
        s.call_raw(*case["args"])
    elif op == "uvs":
        return [canon_f(w) for w in s.call_uvs(*case["args"])]
    else:
        s.call_time(*case["args"])
    return None


def observe(s, case, answer):
    return {"uv": answer, "log": s.log(), "area": s.get_area(), "inuse": s.rd_word(s.g("inuse")), "colour": s.read(s.g("colour"), 5)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def base(r, op):
    return {"op": op, "queues": {"FindColour": [r.randrange(256)]}, "area": [r.getrandbits(32) for _ in range(10)],
            "inuse": r.choice([0, 1, 5, 0x80000000]), "colour": [r.randrange(256) for _ in range(5)]}


def u32(r):
    return r.choice([0, 1, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, r.getrandbits(32), r.getrandbits(32), r.randrange(0, 4000)])


# ---- UI_DrawPostcardText
def post_case(r, colour=None):
    c = base(r, "post")
    c["args"] = [0x30001000 + r.randrange(4) * 0x10, r.getrandbits(32), u32(r), u32(r), u32(r), u32(r), u32(r), u32(r),
                 u32(r) if colour is None else colour, r.getrandbits(32)]
    return c


def post_edges():
    r = random.Random(11)
    for colour in (0, 0xFFFFFFFF, 0x01020304, 0x80FF7F00, 0xFF000000, 0x0000FF00):
        yield post_case(r, colour)


def post_random(r):
    return post_case(r)


# ---- __sinit_ui_utils_cpp
def sinit_edges():
    r = random.Random(12)
    for _ in range(3):
        yield base(r, "sinit")


def sinit_random(r):
    return base(r, "sinit")


# ---- MakeRenderAreaFromRAW
def raw_case(r, ty=None, use=None):
    c = base(r, "raw")
    c["args"] = [r.getrandbits(32), u32(r), u32(r), r.choice([-1, 0, 1, 2, 3, 4, 5, 0x7FFFFFFF, -0x80000000]) if ty is None else ty,
                 r.choice([1, 1, 1, 0]) if use is None else use]
    return c


def raw_edges():
    r = random.Random(13)
    for ty in (-1, 0, 1, 2, 3, 4, 5, 100, -0x80000000, 0x7FFFFFFF):
        for use in (1, 0):
            yield raw_case(r, ty, use)


def raw_random(r):
    return raw_case(r)


# ---- CalcUVs
def uvs_case(r, frac=None):
    c = base(r, "uvs")
    c["args"] = [rf(r) if frac is None else frac, [rf(r) for _ in range(16)]]
    return c


def uvs_edges():
    r = random.Random(14)
    for frac in (0, 0x80000000, fb(100.0), fb(1.0), fb(50.0), 0x7FC00000, 0x7F800000, 0xFF800000, 1):
        yield uvs_case(r, frac)


def uvs_random(r):
    return uvs_case(r)


# ---- Change
def time_case(r, t=None, flags=None):
    c = base(r, "time")
    if flags is None:
        flags = r.choice([0x80, 0x180, 0x100, 0, 0xFFFFFF7F, r.getrandbits(32), r.getrandbits(32) | 0x80])
    if t is None:
        t = r.choice([0, 1, 0x18, 0x19, 0x30, 0x31, -1, 5, 48, 47, 49, 1000, r.randrange(-100, 100), r.randrange(0, 5000),
                      r.getrandbits(32)])
    c["args"] = [flags, t & MASK, (flags & 0x80) == 0]
    return c


def time_edges():
    r = random.Random(15)
    for t in (0, 1, 0x17, 0x18, 0x19, 0x1A, 0x2F, 0x30, 0x31, 0x32, 47, 48, 72, 96, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, -5 & MASK,
              0xFFFFFFFE, 0x80000001, 0xFFFFFFFF):
        yield time_case(r, t, 0x80)
    for flags in (0, 0x100, 0x7F, 0xFFFFFF7F, 0x1000080, 0x81):
        yield time_case(r, 10, flags)


def time_random(r):
    return time_case(r)


TESTS = [
    FnTest("UI_DrawPostcardText", ppc, host, edge=post_edges, rnd=post_random),
    FnTest("__sinit_ui_utils_cpp", ppc, host, edge=sinit_edges, rnd=sinit_random),
    FnTest("MakeRenderAreaFromRAW", ppc, host, edge=raw_edges, rnd=raw_random),
    FnTest("InterfaceDrawEngineHGradient::CalcUVs", ppc, host, edge=uvs_edges, rnd=uvs_random),
    FnTest("InterfaceVisitorTimeTextObject::Change", ppc, host, edge=time_edges, rnd=time_random),
]
