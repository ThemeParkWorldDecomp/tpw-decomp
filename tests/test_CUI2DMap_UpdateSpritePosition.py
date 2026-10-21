"""Unit v5200, the map panel's sprite position, coaster tracks and grid details, against libtpw_unit_CUI2DMap_UpdateSpritePosition.dylib
(tests/unit_CUI2DMap_UpdateSpritePosition.cpp), main app ("SimTheme Park"),
src/game/ui/ui_2dmap.cpp:
  CUI2DMap::UpdateSpritePosition 0x10230a0c, CUI2DMap::DrawCoasterTracks 0x1022f160, CUI2DMap::StoreMapGridDetails 0x1022de40

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments: the sprite's SetPosition and SetScale, the CDraw constructor, SetCurrent,
GetClipRect and destructor, the coaster track queries, the colour scale, BezierPosition, the four Antialiased line routines (the colour and
the line as two events each), the cell queries (the cell as its number in the world's cell array), the assertion and GetDBInfo.

The world of a case: the map object (width, height, depth, centre, zoom), the sprite, the screen size, the world with its cells and track
cells, the thing table with its things, the coaster tracks (controls linked through their next pointers; the host's pointers are 8 bytes, the
Mac's 4) and the clip rectangle. What is compared: the map object's fields, the sprite, the whole map grid, and the event log. A thing's ride
record follows it at +0x40; its UI type is written at the Mac's +0x7a8 on the PPC side and at the host field of the same name on the host
side (tests/ridelayout.py).

Fused operations: UpdateSpritePosition has two fmadds and DrawCoasterTracks fctiwz; the PowerPC side runs under HardwareRounding. Not tested:
a map wider or higher than 128 (the original then writes through a NULL entry), thing ids past the end of the thing table or a thing id of
a free entry in a chain (a NULL read), a zero map width in DrawCoasterTracks (an infinite scale, fctiwz of a NaN), and cyclic thing chains
(an endless loop).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ridelayout
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CUI2DMap_UpdateSpritePosition"
MASK = 0xFFFFFFFF

UPD, DRAW, STORE = 0x10230A0C, 0x1022F160, 0x1022DE40
SIZES = {UPD: 388, DRAW: 832, STORE: 1104}
OWN = set(SIZES)

# callees (in-binary stand-ins and glue)
IS_HOARDING, IS_RIDE, IS_ENTRANCE, IS_EXIT = 0x1009CA9C, 0x1009C61C, 0x1009C4DC, 0x1009C524
IS_TRACK, IS_PYLON, IS_PATH, IS_QUEUE, IS_FLAG = 0x1009C96C, 0x1009C7B0, 0x1009C44C, 0x1009C490, 0x1009BB70
GET_EQUIV, IS_TRACKX, ASSERT, GET_DBINFO = 0x100F48C4, 0x100A367C, 0x1000DDE8, 0x10107CD4
SET_POSITION, SET_SCALE = 0x10231014, 0x10231094
NUM_TRACKS, NTH_TRACK, SCALE_COLOUR, BEZIER = 0x100458AC, 0x100458E8, 0x1022BB44, 0x1004DA98
AA = {1: 0x10006904, 2: 0x10006394, 3: 0x10005E30, 4: 0x100058C0}
CDRAW_CT, CDRAW_SET, CDRAW_CLIP, CDRAW_DT = 0x1023AED0, 0x1023AEE8, 0x1023AF00, 0x1023AF30

ADDR = dict(thing=0x1034CBFC, gworld=0x1037EBFC, end=0x102B2912, colours=0x103970F0, sw=0x102B2B54, sh=0x102B2B50)
# sizeof(CThingArrayEntry) is the host's own (checked for being larger than the Mac's 20); the rest is checked exactly
HOST_LAYOUT = [0x1DA748, 0x50, 0x58, 0x18, 4]   # the sprite is 0x50 on the host since v5652 added its show status fields
HOST_ENTRY = [0]

PPC_OBJ = {}          # the persistent objects of the PowerPC side
CUR = {"tracks": {}, "ctl": {}, "draw": None}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


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

_internal = STUBS.internal


def _internal_saving_toc(addr, fn):
    # the cross-TOC glue the caller reaches saves r2 in the linkage area (0x14(r1)); the caller reloads it afterwards
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        return fn(e)
    return _internal(addr, wrapped)


STUBS.internal = _internal_saving_toc


def cell_index(a):
    return (a - PPC_OBJ["world"] - 0x2D8) // 0x44


def track_index(a):
    return (a - PPC_OBJ["world"] - 0x1102D8) // 0x28


def mk_cell_stub(name, with_flag=False):
    def stub(e):
        L = e.cl
        if with_flag:
            L.rec(name, (cell_index(e.r(3)), e.r(4) & 0xFFFF))
        else:
            L.rec(name, (cell_index(e.r(3)),))
        e.set_r(3, L.ret(name) & MASK)
    return stub


for _name, _addr in (("IsHoarding", IS_HOARDING), ("IsRide", IS_RIDE), ("IsEntrance", IS_ENTRANCE), ("IsExit", IS_EXIT),
                     ("IsTrack", IS_TRACK), ("IsCoasterPylon", IS_PYLON), ("IsPath", IS_PATH), ("IsQueue", IS_QUEUE)):
    STUBS.internal(_addr, mk_cell_stub(_name))
STUBS.internal(IS_FLAG, mk_cell_stub("IsFlag", True))


def stub_equiv(e):
    L = e.cl
    L.rec("GetTrackCellEquiv", (cell_index(e.r(3)),))
    e.set_r(3, (PPC_OBJ["world"] + 0x1102D8 + L.ret("equiv") * 0x28) & MASK)


def stub_trackx(e):
    L = e.cl
    L.rec("IsTrackX", (track_index(e.r(3)),))
    e.set_r(3, L.ret("IsTrackX") & MASK)


def stub_dbinfo(e):
    L = e.cl
    a = e.r(3)
    L.rec("GetDBInfo", (struct.unpack(">H", e.read(a + 0x30, 2))[0],))
    e.set_r(3, (a + 0x40) & MASK)


STUBS.internal(GET_EQUIV, stub_equiv)
STUBS.internal(IS_TRACKX, stub_trackx)
STUBS.internal(GET_DBINFO, stub_dbinfo)
STUBS.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))


def stub_set_position(e):
    L = e.cl
    w = struct.unpack(">3I", e.read(e.r(4), 12))
    L.rec("SetPosition", (L.lab(e.r(3)), canon_f(w[0]), canon_f(w[1]), canon_f(w[2])))


def stub_set_scale(e):
    L = e.cl
    L.rec("SetScale", (L.lab(e.r(3)), canon_f(fb(e.f(1)))))


STUBS.internal(SET_POSITION, stub_set_position)
STUBS.internal(SET_SCALE, stub_set_scale)


def stub_ct(e):
    CUR["draw"] = e.r(3)
    e.cl.rec("CDraw", (0,))


def stub_set_current(e):
    L = e.cl
    L.rec("SetCurrent", (int(e.r(3) == CUR["draw"]), L.lab(e.r(4))))


def stub_clip(e):
    e.cl.rec("GetClipRect", (int(e.r(3) == CUR["draw"]),))
    e.set_r(3, PPC_OBJ["rect"])


def stub_dt(e):
    e.cl.rec("~CDraw", (int(e.r(3) == CUR["draw"]), s32(e.r(4))))


STUBS.internal(CDRAW_CT, stub_ct)
STUBS.internal(CDRAW_SET, stub_set_current)
STUBS.internal(CDRAW_CLIP, stub_clip)
STUBS.internal(CDRAW_DT, stub_dt)


def stub_num_tracks(e):
    L = e.cl
    L.rec("ui_GetNumCoasterTracks", (0,))
    e.set_r(3, L.ret("ui_GetNumCoasterTracks") & MASK)


def stub_nth_track(e):
    L = e.cl
    i = e.r(3)
    L.rec("GetNthCoasterTrackStart", (i,))
    head, out = CUR["tracks"].get(i & 7, (0, 0))
    e.write(e.r(4), struct.pack(">I", out))
    e.set_r(3, head)


def stub_scale_colour(e):
    L = e.cl
    t = ADDR["colours"]
    L.rec("UI2DMReturnScaleColour", (s32(e.r(4)), e.r(5) - t, e.r(6) - t, e.r(7) - t))
    words = [L.ret("scw") & MASK for _ in range(9)]
    e.write(e.r(3), struct.pack(">9I", *words))


def stub_bezier(e):
    L = e.cl
    L.rec("BezierPosition", (CUR["ctl"].get(e.r(3), -1), CUR["ctl"].get(e.r(4), -1), fb(e.f(1))))
    x, y, z = L.ret("bx") & MASK, L.ret("by") & MASK, L.ret("bz") & MASK
    e.write(e.r(6), struct.pack(">3I", x, y, z))


def mk_aa(n):
    def stub(e):
        L = e.cl
        area, rect, col, frm, to = e.r(4), e.r(5), e.r(6), e.r(7), e.r(8)
        L.rec("AAColour", struct.unpack(">9I", e.read(col, 36)))
        rc = struct.unpack(">4i", e.read(rect, 16))
        f = struct.unpack(">2i", e.read(frm, 8))
        t = struct.unpack(">2i", e.read(to, 8))
        L.rec("Antialiased", (n, L.lab(area)) + rc + f + t + (int(rect == PPC_OBJ["rect"]),))
    return stub


STUBS.internal(NUM_TRACKS, stub_num_tracks)
STUBS.internal(NTH_TRACK, stub_nth_track)
STUBS.internal(SCALE_COLOUR, stub_scale_colour)
STUBS.internal(BEZIER, stub_bezier)
for _n, _a in AA.items():
    STUBS.internal(_a, mk_aa(_n))


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    PPC_OBJ["map"] = T.persist_alloc(e, 0x20200)
    PPC_OBJ["sprite"] = T.persist_alloc(e, 0x40)
    PPC_OBJ["world"] = T.persist_alloc(e, 0x1DA748 + 0x100)
    PPC_OBJ["rect"] = T.persist_alloc(e, 0x10)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    endian = ">"

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def h(self, a, v):
        self.write(a, struct.pack(self.endian + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        m, sp = PPC_OBJ["map"], PPC_OBJ["sprite"]
        self.L = e.cl = CL.PpcLog({m: 1, sp: 2, m + 0x2C: 4}, case["queues"])
        CUR["tracks"] = {}
        CUR["ctl"] = {}
        CUR["draw"] = None
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def obj(self, name):
        return PPC_OBJ[name]

    def entry(self, i):
        return ADDR["thing"] + i * 20

    def ptr(self, a, v):
        self.write(a, struct.pack(">I", v))

    def alloc(self, n):
        return self.e.alloc(n)

    def rec(self, mac):
        return mac

    def rec_size(self):
        return 0x7B0

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_track(self, i, head, out):
        CUR["tracks"][i] = (head, out)

    def set_ctl(self, i, addr):
        CUR["ctl"][addr] = i

    def call(self, op):
        m, sp = PPC_OBJ["map"], PPC_OBJ["sprite"]
        with ROUNDING(self.e):
            if op == "update":
                T.call_ppc(self.e, UPD, [m, sp])
            elif op == "draw":
                T.call_ppc(self.e, DRAW, [m])
            else:
                T.call_ppc(self.e, STORE, [m])


class Host(Side):
    fmt = "<I"
    endian = "<"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        m, sp = self.obj("map"), self.obj("sprite")
        CL.host_begin(lib, {1: m, 2: sp, 4: m + 0x20100}, case["queues"])   # the host map's render area (CUI2DMap::HostArea)

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cm_addr(name.encode())

    def obj(self, name):
        return self.lib.tpw_cm_obj({"map": 0, "sprite": 1, "world": 2}.get(name, 3)) if name != "rect" else self.g("rect")

    def entry(self, i):
        return self.g("thing") + i * HOST_ENTRY[0]

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def alloc(self, n):
        return self.lib.tpw_cm_alloc(n)

    def rec(self, mac):
        return ridelayout.host_offset(self.lib, mac)

    def rec_size(self):
        return max(0x7B0, ridelayout.host_size(self.lib))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_track(self, i, head, out):
        self.lib.tpw_cm_settrack(i, head, out)

    def set_ctl(self, i, addr):
        self.lib.tpw_cm_setctl(i, addr)

    def call(self, op):
        m, sp = self.obj("map"), self.obj("sprite")
        if op == "update":
            self.lib.tpw_cm_update(m, sp)
        elif op == "draw":
            self.lib.tpw_cm_draw(m)
        else:
            self.lib.tpw_cm_store(m)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cm_"
    f = getattr(lib, p + "addr")
    f.restype, f.argtypes = C.c_uint64, [C.c_char_p]
    f = getattr(lib, p + "alloc")
    f.restype, f.argtypes = C.c_uint64, [C.c_uint32]
    f = getattr(lib, p + "obj")
    f.restype, f.argtypes = C.c_uint64, [C.c_int32]
    getattr(lib, p + "settrack").argtypes = [C.c_int32, C.c_uint64, C.c_uint64]
    getattr(lib, p + "setctl").argtypes = [C.c_int32, C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "update").argtypes = [C.c_uint64, C.c_uint64]
    getattr(lib, p + "draw").argtypes = [C.c_uint64]
    getattr(lib, p + "store").argtypes = [C.c_uint64]
    ridelayout.bind(lib)
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    got = list(out)[:n]
    assert got[1:] == HOST_LAYOUT, got
    assert got[0] >= 20
    HOST_ENTRY[0] = got[0]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

FILL = b"\xa5" * 0x20000


def build(s, case):
    m = s.obj("map")
    op = case["op"]
    s.words(m + 0x24, [case["w"], case["h"]])
    if op == "update":
        sp = s.obj("sprite")
        s.words(s.g("sw"), [case["sw"]])
        s.words(s.g("sh"), [case["sh"]])
        s.words(m + 0x54, case["centre"])      # 0x54 x, 0x58 y, 0x5c zoom
        s.words(sp, [0, case["sx"], case["sy"], case["old_x"], case["old_y"], case["status"]])
        s.words(sp + 0x24, [case["scale"]])
    elif op == "draw":
        s.words(m + 0x3C, [case["depth"]])
        s.words(s.obj("rect") if True else 0, case["rect"])
        n = 0
        for i in range(8):
            s.set_track(i, 0, 0)
        for i, tr in enumerate(case["tracks"]):
            nodes = [s.alloc(0x58) for _ in range(tr["n"])]
            for k, a in enumerate(nodes):
                s.set_ctl(n, a)
                n += 1
                s.ptr(a + 0x18, nodes[k + 1] if k + 1 < len(nodes) else 0)
            out = s.alloc(0x58)
            s.words(out + 0x40, [tr["colour"]])
            s.set_track(i, nodes[0] if nodes else 0, out)
    else:
        world = s.obj("world")
        s.ptr(s.g("gworld"), world)
        s.h(s.g("end"), 0)
        s.write(m + 0x60, FILL)
        for i, th in enumerate(case["things"], 1):
            a = s.alloc(0x40 + s.rec_size())
            b = bytearray(0x40 + s.rec_size())
            E = s.endian
            struct.pack_into(E + "H", b, 0, i)
            b[2] = th["kind"]
            struct.pack_into(E + "H", b, 0xA, th["next"])
            struct.pack_into(E + "H", b, 0x2E, th["flags"])
            struct.pack_into(E + "H", b, 0x30, i)
            struct.pack_into(E + "i", b, 0x40 + s.rec(0x7A8), th["db"])
            s.write(a, bytes(b))
            s.ptr(s.entry(i), a)
        s.ptr(s.entry(0), s.alloc(16) if case["e0"] else 0)
        for idx, (parent, thing) in case["cells"].items():
            base = world + 0x2D8 + idx * 0x44
            s.h(base + 0x10, parent)
            s.h(base + 0x24, thing)
        for idx, parent in case["tracks"].items():
            s.h(world + 0x1102D8 + idx * 0x28 + 0x10, parent)


def observe(s, case):
    op = case["op"]
    m = s.obj("map")
    out = {"log": s.log(), "map": s.rd_words(m, 0x18)}
    if op == "update":
        w = s.rd_words(s.obj("sprite"), 12)
        w[3], w[4] = canon_f(w[3]), canon_f(w[4])
        out["sprite"] = w
    elif op == "store":
        raw = s.read(m + 0x60, 0x20000)
        out["grid"] = [struct.unpack(s.endian + "iH", raw[i:i + 6]) + (raw[i + 6:i + 8],) for i in range(0, 0x20000, 8)]
    return out


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"])
        return observe(s, case)
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
    if k < 0.06:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


# ---- UpdateSpritePosition
def upd_case(r, **kw):
    c = {"op": "update", "queues": {}}
    c["w"] = r.choice([1, 2, 64, 128, 0, -1, r.randrange(1, 400)])
    c["h"] = r.choice([1, 2, 64, 128, 0, -1, r.randrange(1, 400)])
    c["sw"] = r.choice([640, 800, 1024, 0, r.randrange(0, 2 ** 32)])
    c["sh"] = r.choice([480, 600, 768, 0, r.randrange(0, 2 ** 32)])
    c["centre"] = [fb(r.uniform(0, 128)), fb(r.uniform(0, 128)), fb(r.uniform(0.1, 6)) if r.random() < 0.8 else rf(r)]
    c["sx"] = fb(r.uniform(0, 128)) if r.random() < 0.8 else rf(r)
    c["sy"] = fb(r.uniform(0, 128)) if r.random() < 0.8 else rf(r)
    c["old_x"] = r.randrange(0, 2 ** 32)
    c["old_y"] = r.randrange(0, 2 ** 32)
    c["status"] = r.choice([-1, 0, 1, 2, 3, 4, r.randrange(-5, 6), r.randrange(-2 ** 31, 2 ** 31)])
    c["scale"] = fb(r.uniform(0, 3)) if r.random() < 0.8 else rf(r)
    c.update(kw)
    return c


def upd_edges():
    r = random.Random(5200)
    for st in (-2 ** 31, -1, 0, 1, 2, 3, 4, 2 ** 31 - 1):
        yield upd_case(r, status=st)
    for kw in (dict(w=0), dict(h=0), dict(sw=0), dict(sh=0), dict(sw=0x80000000), dict(sh=0xFFFFFFFF), dict(w=-5, h=-7)):
        yield upd_case(r, **kw)
    for sp in SPECIAL:
        yield upd_case(r, sx=sp)
        yield upd_case(r, centre=[sp, fb(10.0), fb(2.0)])
        yield upd_case(r, centre=[fb(1.0), sp, fb(2.0)])
        yield upd_case(r, centre=[fb(1.0), fb(1.0), sp])
        yield upd_case(r, scale=sp, sy=sp)


def upd_random(r):
    return upd_case(r)


# ---- DrawCoasterTracks
DEPTHS = [-8, -7, -1, 0, 1, 7, 8, 9, 15, 16, 17, 23, 24, 25, 31, 32, 33, 39, 40, 41, 0x7FFFFFF0]


def draw_case(r, **kw):
    c = {"op": "draw"}
    c["w"] = r.choice([1, 2, 10, 64, 128, 500, 3000, r.randrange(1, 3000), -r.randrange(1, 100)])
    c["h"] = r.randrange(0, 200)
    c["depth"] = r.choice(DEPTHS) if r.random() < 0.5 else 8 * r.randrange(0, 7) - 7 + r.randrange(0, 8)
    c["rect"] = [r.randrange(-5, 5), r.randrange(-5, 5), r.randrange(0, 700), r.randrange(0, 700)]
    nt = r.choice([0, 1, 2, 3, 3])
    c["tracks"] = [{"n": r.choice([0, 1, 2, 2, 3]), "colour": fb(r.uniform(-600, 600))} for _ in range(nt)]
    big = r.random() < 0.15
    lim = 2000.0 if not big else 40000.0
    c["queues"] = {
        "ui_GetNumCoasterTracks": [nt],
        "bx": [fb(r.uniform(-lim, lim)) for _ in range(140)],
        "by": [fb(r.uniform(-lim, lim)) for _ in range(140)],
        "bz": [fb(r.uniform(-lim, lim)) for _ in range(140)],
        "scw": [r.randrange(0, 2 ** 32) for _ in range(60)],
    }
    c.update(kw)
    if "nt" in kw:
        c["queues"]["ui_GetNumCoasterTracks"] = [kw["nt"]]
    return c


def draw_edges():
    r = random.Random(5201)
    for d in DEPTHS:
        c = draw_case(r, depth=d)
        c["tracks"] = [{"n": 3, "colour": fb(100.5)}, {"n": 2, "colour": fb(-1.0)}]
        c["queues"]["ui_GetNumCoasterTracks"] = [2]
        yield c
    for col in (0.0, -1.0, 255.5, 256.0, 300.7, -300.5, 1e6, -1e6):
        c = draw_case(r, depth=24)
        c["tracks"] = [{"n": 2, "colour": fb(col)}]
        c["queues"]["ui_GetNumCoasterTracks"] = [1]
        yield c
    for nt in (0, 1):
        c = draw_case(r)
        c["tracks"] = [{"n": 0, "colour": fb(3.0)}, {"n": 1, "colour": fb(3.0)}][:nt + 1]
        c["queues"]["ui_GetNumCoasterTracks"] = [nt + 1]
        yield c
    for w in (1, 2, 3, 10):
        yield draw_case(r, w=w, depth=17)
    # a list longer than the queues' first samples, all four routines
    for d in (1, 9, 17, 25):
        c = draw_case(r, depth=d)
        c["tracks"] = [{"n": 3, "colour": fb(77.0)}, {"n": 2, "colour": fb(5.0)}, {"n": 3, "colour": fb(6.0)}]
        c["queues"]["ui_GetNumCoasterTracks"] = [3]
        yield c


def draw_random(r):
    return draw_case(r)


# ---- StoreMapGridDetails
QUEUES = ["IsHoarding", "IsRide", "IsEntrance", "IsExit", "IsTrack", "IsCoasterPylon", "IsPath", "IsQueue", "IsFlag", "IsTrackX"]


def store_case(r, w=None, h=None, K=None, pr=None):
    if w is None:
        w = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, r.randrange(1, 9)])
    if h is None:
        h = r.choice([0, 1, 2, 3, 4, 5, 6, r.randrange(1, 7)])
    big = w * h > 40
    if K is None:
        K = 0 if r.random() < 0.1 else r.randrange(1, 3 if big else 7)
    things = []
    for i in range(1, K + 1):
        things.append(dict(
            kind=3 if r.random() < 0.75 else r.choice([0, 1, 2, 4, 5, 255]),
            next=0 if (i == K or r.random() < 0.4) else r.randrange(i + 1, K + 1),
            flags=r.choice([0, 1, 0x40, 0x10, 0x02, r.randrange(0x10000), r.randrange(0x10000)]),
            db=r.choice([-3, -1, 0, 1, 2, 3, 4, 7, 1 << 30, 0, 2, 3])))
    cells = {}
    for y in range(h):
        for x in range(w):
            idx = y * 128 + x
            cells[idx] = (r.choice([0, idx + 1, r.randrange(1, 41), r.randrange(1, 41)]), r.randrange(0, K + 1))
    for idx in range(-1, 41):
        if idx not in cells:
            cells[idx] = (r.randrange(0, 41), r.randrange(0, K + 1))
    tracks = {idx: r.randrange(0, 41) for idx in range(-1, 41)}
    c = {"op": "store", "w": w, "h": h, "things": things, "cells": cells, "tracks": tracks, "e0": r.random() < 0.3}
    q = {}
    for name in QUEUES:
        p = (pr or {}).get(name, r.choice([0, 0.2, 0.5, 0.8, 1]))
        q[name] = [1 if r.random() < p else 0 for _ in range(3000)]
    q["equiv"] = [r.randrange(0, 41) for _ in range(60)]
    c["queues"] = q
    return c


def store_edges():
    r = random.Random(5202)
    for w, h, K in ((0, 0, 3), (1, 1, 3), (0, 3, 2), (3, 0, 2), (2, 2, 0), (2, 2, 5), (30, 1, 2), (1, 30, 1), (5, 5, 6), (6, 6, 6)):
        yield store_case(r, w, h, K)
    zero = {n: 0 for n in QUEUES}
    # one query true at a time, the rest false; and the chain walks with every database kind
    for name in QUEUES:
        pr = dict(zero)
        pr[name] = 1
        yield store_case(r, 4, 4, 5, pr)
    for a in ("IsRide", "IsEntrance", "IsExit", "IsTrack", "IsCoasterPylon"):
        for track in (0, 1):
            pr = dict(zero)
            pr[a] = 1
            pr["IsTrack"] = track if a != "IsTrack" else 1
            pr["IsTrackX"] = 1
            yield store_case(r, 3, 3, 6, pr)
            pr["IsTrackX"] = 0
            yield store_case(r, 3, 3, 6, pr)
    for db in (-3, -1, 0, 1, 2, 3, 4):
        for fl in (0, 1, 0x40, 0x10, 0x02, 0x12, 0x3F):
            c = store_case(r, 2, 2, 3, dict(zero, IsRide=1))
            for t in c["things"]:
                t["kind"], t["db"], t["flags"], t["next"] = 3, db, fl, 0
            yield c
    for pr in (dict(IsRide=1, IsTrack=0, IsCoasterPylon=1), dict(IsRide=0, IsTrack=1, IsCoasterPylon=0), dict(IsRide=0, IsTrack=0, IsCoasterPylon=0),
               dict(IsRide=1, IsTrack=1, IsCoasterPylon=1)):
        c = store_case(r, 3, 3, 4, dict(zero, IsEntrance=1, **pr))
        for t in c["things"]:
            t["kind"], t["db"] = 3, 0
        yield c


def store_random(r):
    return store_case(r)


TESTS = [
    FnTest("CUI2DMap::UpdateSpritePosition", ppc, host, edge=upd_edges, rnd=upd_random),
    FnTest("CUI2DMap::DrawCoasterTracks", ppc, host, edge=draw_edges, rnd=draw_random),
    FnTest("CUI2DMap::StoreMapGridDetails", ppc, host, edge=store_edges, rnd=store_random),
]
