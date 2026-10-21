"""Unit v5985, the map area window's message function, against libtpw_unit_CUI2DMap_MapAreaCallback.dylib
(tests/unit_CUI2DMap_MapAreaCallback.cpp), main app ("SimTheme Park"),
src/game/ui/ui_2dmap.cpp:
  CUI2DMap::MapAreaCallback 0x1022d0ac

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments: UpdateMap, MoveCentre, GetMapGridCell (answers NULL or an entry of the map's
grid), the string class (constructor, assignment, destructor: the data pointer is the text given), CStaff::GetName (the text at thing +
0x100), CObject::GetUIName and TbITable::FormatString (both write a short text into the conversion buffer; FormatString also records the
integer tag it is given), GetParent, GetWindow, the virtual set-text (through __ptr_glue: the test also checks r12 is the table entry at
+0x2c of the table at window +0x118), SetCameraPOI, UI_SendMessage, the status panel, the default window handler and GAME_ASSERTMSG.

The world of a case: the screen size, the setting byte (player settings +0x38), the two "no thing" halfwords, the thing array (six slots,
things with a kind byte at +2, a word at +0x1d8 and a name at +0x100), the map object (mouse position, size, centre, zoom, drag flag,
hovered sprite and thing id, the sprite list of up to five sprites) and the text resource slot (+0x564). What is compared: the answer, the
event log, the map object (its first 0x60 bytes, the hovered sprite as an index, the thing id, the rest) and the first 16 characters of
the conversion buffer.

Fused operations (fnmsubs, fmadds) and fctiwz run under HardwareRounding. Not tested: a thing id beyond the six slots (it would read the
array past the ones the test fills), a NULL list, a NULL answer of GetWindow (the original uses it unchecked), zoom 0 or NaN centres
(fctiwz of a value above 2^31 differs between Unicorn and the hardware).
"""
import copy
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CUI2DMap_MapAreaCallback"
MASK = 0xFFFFFFFF

FN = 0x1022D0AC
SIZES = {FN: 2652}
OWN = set(SIZES)

UPDATE_MAP, MOVE_CENTRE, GET_GRID_CELL = 0x1022DB4C, 0x1022F8A4, 0x1022DDD0
GET_NAME, GET_UI_NAME = 0x10126F98, 0x10110690
GET_PARENT, GET_WINDOW, CALL_DEFAULT = 0x101DBAFC, 0x101DBE40, 0x101DE318
SET_CAMERA_POI, UI_SEND_MESSAGE, GOTO_STATUS = 0x10033650, 0x101C9518, 0x10181E2C
ASSERT_MSG, PTR_GLUE = 0x1000DDE8, 0x1023BAA8

ADDR = dict(w=0x102B2B54, h=0x102B2B50, set=0x1038071C, text=0x1037FF18, buf=0x10380C48, none1=0x102B290C, none2=0x102B290E,
            arr=0x1034CBFC)
NT = 6          # thing slots
NS = 5          # sprites in the pool
MAC = dict(PW=4, head=4, nxt=4, prv=8, vis=0x2C, thing=0x18, stride=0x14, athing=0, slot=0x564,
           hov=0x20060, drag=0x20084, panel=4)
HOST_ONLY = dict(hov=0x200A0, drag=0x200A8, panel=0x200B0)   # the translation keeps these three outside the Mac offsets


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def put_string(e, log):
    """Constructor and assignment: the data pointer (+8) becomes the text given; answers the string."""
    e.write((e.r(3) + 8) & MASK, struct.pack(">I", e.r(4)))
    return e.r(3)


def get_ui_name(e, log):
    n = log.lab(e.r(3))
    e.write(e.r(4), struct.pack(">4H", ord("N"), ord("0") + (n // 10) % 10, ord("0") + n % 10, 0))
    return 0


def get_grid_cell(e, log):
    v = log.ret("GetMapGridCell")
    return 0 if v == 0 else (e.r(3) + 0x60 + 8 * v) & MASK


def ptr_glue(e, log):
    assert e.r(12) == e.side.D, "the virtual is not the entry at +0x2c of the table at +0x118"
    return 0


def stub_format(e):
    L = e.cl
    L.rec("FormatString", (L.lab(e.r(3)), e.r(5), e.r(6)))
    last = ord("x")
    if e.r(6) == 0x1C8:
        magic, _, ident, value = struct.unpack(">4I", e.read(e.r(7), 16))
        L.rec("FormatTag", (magic, ident, s32(value)))
        last = ord("0") + (s32(value) & 7)
    e.write(e.r(4), struct.pack(">4H", ord("F"), ord("0") + e.r(6) % 10, last, 0))
    e.set_r(3, 0)


def stub_poi(e):
    L = e.cl
    x = struct.unpack(">I", e.read(e.r(3), 4))[0]
    y = struct.unpack(">I", e.read(e.r(4), 4))[0]
    L.rec("SetCameraPOI", (canon_f(x), canon_f(y), e.r(5), e.r(6), e.r(7)))


STUBS.internal(UPDATE_MAP, CL.recorder("UpdateMap", "L"))
STUBS.internal(MOVE_CENTRE, CL.recorder("MoveCentre", "Lif"))
STUBS.internal(GET_GRID_CELL, CL.recorder("GetMapGridCell", "Lii", ret=get_grid_cell))
STUBS.imp("__ct__26TbDynamicStringTemplate<w>FPCw", CL.recorder("StrCtor", "xS", ret=put_string))
STUBS.imp("__as__26TbDynamicStringTemplate<w>FPCw", CL.recorder("StrAssign", "xS", ret=put_string))
STUBS.imp("__dt__26TbDynamicStringTemplate<w>Fv", CL.recorder("StrDtor", "xi"))
STUBS.imp("FormatString__8TbITableCFPwUiUle", stub_format)
STUBS.internal(GET_NAME, CL.recorder("GetName", "L", ret=lambda e, log: (e.r(3) + 0x100) & MASK))
STUBS.internal(GET_UI_NAME, CL.recorder("GetUIName", "Lxu", ret=get_ui_name))
STUBS.internal(GET_PARENT, CL.recorder("GetParent", "L", ret=lambda e, log: (e.r(3) + 0x200) & MASK))
STUBS.internal(GET_WINDOW, CL.recorder("GetWindow", "Li", ret=lambda e, log: (e.r(3) + 0x200) & MASK))
STUBS.internal(CALL_DEFAULT, CL.recorder("CallDefaultWindowHandler", "Liii", ret="i"))
STUBS.internal(PTR_GLUE, CL.recorder("PtrGlue", "LS", ret=ptr_glue))
STUBS.internal(SET_CAMERA_POI, stub_poi)
STUBS.internal(UI_SEND_MESSAGE, CL.recorder("UI_SendMessage", "Liii", ret="i"))
STUBS.internal(GOTO_STATUS, CL.recorder("UIMain_GotoStatusPanelForThing", "L"))
STUBS.internal(ASSERT_MSG, CL.recorder("GAME_ASSERTMSG", "is"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory; the world's blocks are allocated up front so the log can name them."""

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

    def u16s(self, a, vs):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(vs), *vs))

    def rd_u16s(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def blocks(self):
        self.map = self.alloc(0x20100)
        self.win = self.alloc(0x1000)
        self.m5 = self.alloc(0x100)
        self.table = self.alloc(0x100)
        self.things = [self.alloc(0x400) for _ in range(NT)]
        self.sprites = [self.alloc(0x60) for _ in range(NS)]
        self.window = self.win
        self.D = self.win + 0x700
        fixed = {self.map: 1, self.win: 2, self.win + 0x200: 3, self.win + 0x400: 4, self.m5: 5, self.table: 6}
        for i, t in enumerate(self.things):
            fixed[t] = 20 + i
        return fixed


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.side = self
        self.__dict__.update(MAC)
        fixed = self.blocks()
        self.L = e.cl = CL.PpcLog(fixed, copy.deepcopy(case["queues"]))
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

    def ptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_word(a)

    def log(self):
        return self.L.log

    def call(self, case):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN, [self.map, self.window, case["msg"] & MASK, case["a"] & MASK, case["b"] & MASK])
        return s32(r3)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.__dict__.update(lib.lay)
        self.__dict__.update(HOST_ONLY)
        fixed = self.blocks()
        CL.host_begin(lib, {label: addr for addr, label in fixed.items()}, copy.deepcopy(case["queues"]))

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_map_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_map_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, case):
        return self.lib.tpw_map_call(self.map, self.window, case["msg"], case["a"], case["b"])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    f = lib.tpw_map_addr
    f.restype, f.argtypes = C.c_uint64, [C.c_char_p]
    f = lib.tpw_map_alloc
    f.restype, f.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_map_layout.argtypes = [C.POINTER(C.c_int64)]
    f = lib.tpw_map_call
    f.restype, f.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_map_layout(out)
    v = list(out)[:n]
    assert v[4] == 0x18 and v[9] == 0xC and v[8] == 8, v
    lib.lay = dict(PW=v[8], head=v[0], nxt=v[1], prv=v[2], vis=v[3], thing=v[4], stride=v[5], athing=v[6], slot=v[7])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    s.words(s.g("w"), [c["W"]])
    s.words(s.g("h"), [c["H"]])
    s.write(s.g("set") + 0x38, bytes([c["setting"]]))
    s.ptr(s.g("text") + s.slot, s.table)
    s.half(s.g("none1"), c["none1"])
    s.half(s.g("none2"), c["none2"])
    s.u16s(s.g("buf"), [0] * 32)
    for i in range(NT):
        s.ptr(s.g("arr") + i * s.stride + s.athing, s.things[i] if c["slots"][i] else 0)
        t = s.things[i]
        s.write(t + 2, bytes([c["kinds"][i]]))
        s.words(t + 0x1D8, [c["vals"][i]])
        s.u16s(t + 0x100, [ord("S"), ord("0") + i, 0])
    # the sprites and their list
    for i, sp in enumerate(c["pool"]):
        a = s.sprites[i]
        s.words(a + 0xC, [sp["x"]])
        s.words(a + 0x10, [sp["y"]])
        s.words(a + s.vis, [sp["vis"]])
        s.ptr(a + s.thing, s.things[sp["thing"]] if sp["thing"] >= 0 else 0)
    lst = s.alloc(0x40)
    nodes = [s.alloc(0x20) for _ in c["order"]]
    for k, i in enumerate(c["order"]):
        s.ptr(nodes[k], s.sprites[i])
        s.ptr(nodes[k] + s.nxt, nodes[k + 1] if k + 1 < len(nodes) else 0)
        s.ptr(nodes[k] + s.prv, nodes[k - 1] if k else 0)
    s.ptr(lst + s.head, nodes[0] if nodes else 0)
    # the map object
    m = s.map
    s.ptr(m + s.panel, s.m5)
    s.words(m + 0x10, [c["mx"], c["my"]])
    s.words(m + 0x24, [c["mw"], c["mh"]])
    s.words(m + 0x54, [fb(c["cx"]), fb(c["cy"]), fb(c["zoom"])])
    s.ptr(m + s.hov, s.sprites[c["hover"]] if c["hover"] >= 0 else 0)
    s.half(m + 0x20064, c["half"])
    s.ptr(m + 0x20080, lst)
    s.words(m + s.drag, [c["flag"]])
    n = c["queues"]["GetMapGridCell"][0]
    if n:
        s.half(m + 0x60 + 8 * n + 4, c["cellid"])
    s.words(s.win + 0x400 + 0x118, [s.win + 0x600])
    s.words(s.win + 0x600 + 0x2C, [s.D])


def observe(s, c, answer):
    m = s.map
    head = s.rd_words(m, 24)
    head[1] = 0
    head[21], head[22] = canon_f(head[21]), canon_f(head[22])
    hv = s.rd_ptr(m + s.hov)
    hover = -1 if hv == 0 else (s.sprites.index(hv) if hv in s.sprites else -2)
    return {"answer": answer, "log": s.log(), "head": head, "hover": hover, "half": s.rd_half(m + 0x20064),
            "tail": s.rd_words(m + 0x20068, 6) + s.rd_words(m + s.drag, 1) + s.rd_words(m + 0x20088, 4),
            "buf": s.rd_u16s(s.g("buf"), 16)}


def run(s, c):
    try:
        build(s, c)
        return observe(s, c, s.call(c))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x3F800000, 0xBF800000, 0x00000001]
MSGS = [0x10001, 0x10003, 0x1E, 0x10006, 0x10004, 0x10005, 0x10002, 0, 0x10007, 0x1D, 0x10000, 0x7FFFFFFF, -1]


def f32(x):
    return T.f32(x)


def make(r, **over):
    msg = over.get("msg", r.choice(MSGS))
    W = r.choice([640, 800, 1024, 1280, 1600, 2048, r.randrange(100, 4000)])
    H = r.choice([480, 600, 768, 1024, 1536, r.randrange(100, 3000)])
    if r.random() < 0.15:
        W, H = 2048, 1536
    mw, mh = r.randrange(1, 129), r.randrange(1, 129)
    wide = r.random() < 0.1
    if wide:
        none1 = none2 = 0xFFFF
    else:
        none1, none2 = r.choice([0, 1, 2, 3]), r.choice([0, 1, 2, 3])
    ids = list(range(NT)) + [none1, none2]
    ids = [i for i in ids if i < NT or wide]
    if msg in (0x10001, 0x10003):
        px, py = r.randrange(0, 2048), r.randrange(0, 1536)
        a = (px << 16) | py
        if r.random() < 0.1:
            a = r.randrange(0, 1 << 32)
            px, py = a >> 16, a & 0xFFFF
    else:
        px, py = r.randrange(0, 2048), r.randrange(0, 1536)
        a = 0 if msg == 0x10006 and r.random() < 0.8 else r.choice([0, 1, 1, 1, 2, -1, r.randrange(1 << 31)])
        if msg == 0x10006 and over.get("a") is None and r.random() < 0.8:
            a = 0
    mx, my = r.randrange(0, 2048), r.randrange(0, 1536)
    if msg in (0x10001, 0x10003):
        fx, fy = f32(f32(px * W) / 2048.0), f32(f32((0x600 - py) * H) / 1536.0)
    else:
        fx, fy = f32(f32(mx * W) / 2048.0), f32(f32((0x600 - my) * H) / 1536.0)
    pool = []
    for _ in range(NS):
        if r.random() < 0.6:
            x, y = fb(fx + r.uniform(-6, 6)), fb(fy + r.uniform(-6, 6))
        else:
            x, y = fb(r.uniform(0, 2100)), fb(r.uniform(0, 1600))
        if r.random() < 0.03:
            x = r.choice(SPECIAL)
        if r.random() < 0.03:
            y = r.choice(SPECIAL)
        pool.append({"x": x, "y": y, "vis": r.choice([0, 1, 1, 1, 2, -1]), "thing": r.choice([-1, -1] + list(range(NT)))})
    order = [i for i in range(NS) if r.random() < 0.7]
    r.shuffle(order)
    c = {
        "op": "call", "msg": msg, "a": a, "b": r.choice([0, 1, -5, r.randrange(1 << 31)]),
        "W": W, "H": H, "setting": r.choice([0, 1, 1, 0x80]), "none1": none1, "none2": none2,
        "slots": [r.random() < 0.8 if i else r.random() < 0.15 for i in range(NT)],
        "kinds": [r.choice([1, 1, 4, 5, 6, 7, 8, 3, 2, 9, 0, 0x13, 0xFF]) for _ in range(NT)],
        "vals": [r.choice([0, 0, 1, -7, 123456, r.randrange(1 << 32)]) for _ in range(NT)],
        "mx": mx, "my": my, "mw": mw, "mh": mh,
        "cx": r.uniform(-10, mw + 10), "cy": r.uniform(-10, mh + 10), "zoom": r.uniform(0.25, 6.0),
        "flag": r.choice([0, 1, 1, 2]), "hover": r.choice([-1] + list(range(NS))), "pool": pool, "order": order,
        "half": r.choice(ids), "cellid": r.choice(ids),
        "queues": {"GetMapGridCell": [r.choice([0, 1, 2, 3])], "CallDefaultWindowHandler": [r.randrange(-5, 1 << 30)],
                   "UI_SendMessage": [r.randrange(0, 5)]},
    }
    for k in ("cx", "cy", "zoom"):
        c[k] = f32(c[k])
    c.update({k: v for k, v in over.items()})
    return c


def edges():
    r = random.Random(5985)
    for msg in (0x10001, 0x10003, 0x1E, 0x10006, 0x10004, 0x10005, 0x10002, 0, 0x10007, 0x1D):
        for k in range(14):
            yield make(r, msg=msg)
    # a == 1 and other values for the drag flag messages, a == 0 and not for the click
    for msg in (0x10004, 0x10005):
        for a in (1, 0, 2, -1):
            yield make(r, msg=msg, a=a)
    for a in (0, 1, -1, 7):
        for k in range(4):
            yield make(r, msg=0x10006, a=a)
    # exact screen size: the sprite at exactly 5.0 and 4.75 away, ties, the hovered sprite changing kind
    for k in range(12):
        c = make(r, msg=0x10003, W=2048, H=1536, a=(300 << 16) | 0x600 - 200 if False else (300 << 16) | 200)
        fx, fy = 300.0, float(0x600 - 200)
        c["pool"][0].update(x=fb(fx + 5.0), y=fb(fy), vis=1)
        c["pool"][1].update(x=fb(fx + 4.75), y=fb(fy), vis=1)
        c["pool"][2].update(x=fb(fx + 4.75), y=fb(fy), vis=1)
        c["pool"][3].update(x=fb(fx), y=fb(fy + 5.0), vis=1)
        c["pool"][4].update(x=fb(fx - 3.0), y=fb(fy + 4.0), vis=1)
        c["order"] = [0, 1, 2, 3, 4] if k % 2 else [4, 2, 1, 0, 3]
        yield c
    # the mouse in the middle of the screen: no centre offset
    for msg in (0x10003, 0x10006):
        for k in range(6):
            c = make(r, msg=msg, W=2048, H=1536, mx=1024, my=0x600 - 768, a=(1024 << 16) | 768 if msg != 0x10006 else 0)
            yield c


def rnd(r):
    return make(r)


TESTS = [
    FnTest("CUI2DMap::MapAreaCallback", ppc, host, edge=edges, rnd=rnd),
]
