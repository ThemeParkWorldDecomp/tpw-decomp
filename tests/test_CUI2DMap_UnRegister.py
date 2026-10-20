"""Unit v5959, the 2D map's sprite list functions and map picture functions, against libtpw_unit_CUI2DMap_UnRegister.dylib
(tests/unit_CUI2DMap_UnRegister.cpp), main app ("SimTheme Park"),
src/CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_UnRegister.cpp:
  UnRegister 0x10230cd4, DeleteSpriteListEntries 0x10230bd4, UpdateAllSpritePositions 0x10230974, CloseSprites 0x102307ec,
  Zoom 0x1022fa1c, LoadMap 0x1022dbdc, UpdateMap 0x1022db4c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. The map is a block of 0x20100 bytes written by Mac offset. The
sprite list is a real TbTList on both sides (its Find, RemoveAt and RemoveHead run as the original on the PowerPC side and as the host
templates on the host side); the list is built with the Mac layout in emulator memory and with AddTail on the host. Deleting a sprite is
the Mac's virtual deleting destructor (a fake through a vtable and a function descriptor on the PowerPC side) and the host's destructor
(a recording fake). The callees outside the unit are fakes on both sides recording the same events: UpdateSpritePosition,
UpdateMapZoom, UpdateAllDrawRelated, CEZSprite DeleteSprite/Disable/Close, CMapZoom::Update, get_theme_path, the string, TGA loader and
render area classes (the string fakes keep the text, so the path the loader is asked for shows the order of the appends).

The world of a case: the map's words 0x20..0x5f, the list (entries are sprites or NULL), the six image words, the theme path, the file
name, the separator character, the texture format words, and the loader's answers (width and height). What is compared: the map words,
the list (count, head, tail, each node's data and previous node's data), the pointer word after CloseSprites, the deleting-destructor
call of the list in CloseSprites (PowerPC side only: the host's is the compiler's delete), and the event log.

No fused operations. Not tested: a NULL list (every function reads it unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CUI2DMap_UnRegister"
MASK = 0xFFFFFFFF

FN_UNREG, FN_DELETE, FN_UPDALL, FN_CLOSE, FN_ZOOM, FN_LOAD, FN_UPDMAP = (
    0x10230CD4, 0x10230BD4, 0x10230974, 0x102307EC, 0x1022FA1C, 0x1022DBDC, 0x1022DB4C)
SIZES = {FN_UNREG: 84, FN_DELETE: 100, FN_UPDALL: 96, FN_CLOSE: 188, FN_ZOOM: 128, FN_LOAD: 460, FN_UPDMAP: 100}
OWN = set(SIZES)

UPDATE_SPRITE_POS, UPDATE_MAP_ZOOM, UPDATE_ALL_DRAW = 0x10230A0C, 0x1022F4D4, 0x1022E2C4
G_DELETE_SPRITE, G_DISABLE, G_CLOSE, G_MAPZOOM_UPDATE = 0x1023AE58, 0x1023AE70, 0x1023AE88, 0x1023AF48
G_STR_CTOR, G_STR_ADDCHAR, G_STR_ADDSTR, G_STR_DTOR = 0x102382D8, 0x1023A4F8, 0x10238380, 0x102382F0
G_TGA_CTOR, G_TGA_DTOR, G_AREA_CTOR = 0x10238FB0, 0x10238FE0, 0x10238E30
G_READHEADER, G_ALLOC, G_LOAD, G_FREE = 0x10238FC8, 0x10238FF8, 0x10239010, 0x10239028
G_MOVE, G_CONVDTOR = 0x1023AEA0, 0x1023AEB8
GET_THEME_PATH = 0x1015139C
# code addresses the fake vtable slots point to (functions of other units, stubbed in this module only)
SPRITE_DTOR_CODE, LIST_DTOR_CODE = 0x10230D64, 0x10231014

# the TOC slots (the asm's comments): ez 0x103bca60, mz 0x103bc948, ts 0x103bc920, tga 0x103970d0, sep 0x102b2910 are what they hold
SLOTS = dict(ez=0x10261E68, mz=0x10261E6C, ts=0x10260B84, tga=0x102643C0, sep=0x102643C4)
HOST_LAYOUT = [0x30]
NSPR = 4


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cs(e, a):
    t = e.cstr(a)
    return t.encode("latin1") if isinstance(t, str) else bytes(t)


def str_ctor(e, log):
    obj = e.r(3)
    buf = e.alloc(0x200)
    e.write(buf, cs(e, e.r(4)) + b"\0")
    e.write(obj + 8, struct.pack(">I", buf))
    return obj


def str_add_char(e, log):
    obj = e.r(3)
    ch = e.read(e.r(4), 1)
    data = e.u32(obj + 8)
    e.write(data, cs(e, data) + ch + b"\0")
    return obj


def str_add_str(e, log):
    obj = e.r(3)
    data = e.u32(obj + 8)
    other = e.u32(e.r(4) + 8)
    e.write(data, cs(e, data) + cs(e, other) + b"\0")
    return obj


def read_header(e, log):
    area = e.r(7)
    e.write(area + 0x1C, struct.pack(">II", e.cl.ret("ReadW") & MASK, e.cl.ret("ReadH") & MASK))
    return 0


def list_dtor(e):
    e.cl.listdel.append((e.r(3), s32(e.r(4))))


STUBS.internal(UPDATE_SPRITE_POS, CL.recorder("UpdateSpritePosition", "LL"))
STUBS.internal(UPDATE_MAP_ZOOM, CL.recorder("UpdateMapZoom", "L"))
STUBS.internal(UPDATE_ALL_DRAW, CL.recorder("UpdateAllDrawRelated", "L"))
STUBS.imp("DeleteSprite__9CEZSpriteFPv", CL.recorder("DeleteSprite", "Lu"))
STUBS.imp("Disable__9CEZSpriteFv", CL.recorder("Disable", "L"))
STUBS.imp("Close__9CEZSpriteFv", CL.recorder("Close", "L"))
STUBS.imp("Update__8CMapZoomFiiii", CL.recorder("MapZoomUpdate", "Liiii"))
STUBS.internal(GET_THEME_PATH, CL.recorder("GetThemePath", "", ret=lambda e, log: e.tp))
STUBS.imp("__ct__26TbDynamicStringTemplate<c>FPCc", CL.recorder("StrCtor", "Ns", ret=str_ctor))
STUBS.imp("__apl__26TbDynamicStringTemplate<c>FRCc", CL.recorder("StrAddChar", "Lx", ret=str_add_char))
STUBS.imp("__apl__26TbDynamicStringTemplate<c>FRC15TbStringBase<c>", CL.recorder("StrAddStr", "LL", ret=str_add_str))
STUBS.imp("__dt__26TbDynamicStringTemplate<c>Fv", CL.recorder("StrDtor", "Li"))
STUBS.imp("__ct__11TbTGALoaderFv", CL.recorder("TgaCtor", "N"))
STUBS.imp("__dt__11TbTGALoaderFv", CL.recorder("TgaDtor", "Li"))
STUBS.imp("__ct__12TbRenderAreaFv", CL.recorder("AreaCtor", "N"))
STUBS.imp("ReadHeader__19TbGraphicFileLoaderFPCcPCcP12TbRenderArea", CL.recorder("ReadHeader", "xLsLL", ret=read_header))
STUBS.imp("AllocMemory__12TbRenderAreaFv", CL.recorder("AllocArea", "xL"))
STUBS.imp("Load__19TbGraphicFileLoaderFRC12TbRenderAreaPCcPCcP12TbRenderArea", CL.recorder("Load", "xLLsLL"))
STUBS.imp("FreeMemory__12TbRenderAreaFv", CL.recorder("FreeArea", "L"))
STUBS.imp("Move__8TbRAConvFR12TbRenderAreaR12TbRenderAreaP9TbPalette", CL.recorder("Move", "xLLLL"))
STUBS.imp("__dt__8TbRAConvFv", CL.recorder("ConvDtor", "Li"))


def sprite_dtor(e):
    """The deleting destructor with flag 1: the host's delete is logged by the call log as "delete" of the labelled pointer."""
    if s32(e.r(4)) > 0:
        e.cl.rec("delete", (e.cl.lab(e.r(3)),))


STUBS.internal(SPRITE_DTOR_CODE, sprite_dtor)
STUBS.internal(LIST_DTOR_CODE, list_dtor)


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

    def images(self, ws):
        self.words(self.map + 0x20068, ws)

    def lab(self, a):
        if a == 0xDEAD:
            return "none"
        if a in self.sprites:
            return 10 + self.sprites.index(a)
        return a


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        # the objects behind the TOC slots (the data imports sit where the slot says at run time)
        self.addr = {k: e.u32(v) for k, v in SLOTS.items()}
        self.map = e.alloc(0x20100)
        self.sprites = [e.alloc(0x40) for _ in range(NSPR)]
        fixed = {self.map: 1, self.map + 0x2C: 2, self.addr["ez"]: 3, self.addr["mz"]: 4, self.addr["tga"]: 5}
        for i, sp in enumerate(self.sprites):
            fixed[sp] = 10 + i
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.L.listdel = []
        self.list = 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return self.addr[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def vtable_for(self, code):
        toc = self.e.r(2)
        desc = self.alloc(8)
        self.words(desc, [code, toc])
        vt = self.alloc(16)
        self.words(vt, [0, 0, desc])
        return vt

    def init_sprites(self):
        vt = self.vtable_for(SPRITE_DTOR_CODE)
        for sp in self.sprites:
            self.words(sp, [vt])
        self.vt_list = self.vtable_for(LIST_DTOR_CODE)

    def mklist(self, items):
        lst = self.alloc(0x10)
        nodes = [self.alloc(12) for _ in items]
        for i, (n, d) in enumerate(zip(nodes, items)):
            self.words(n, [d, nodes[i + 1] if i + 1 < len(nodes) else 0, nodes[i - 1] if i else 0])
        self.words(lst, [self.vt_list, nodes[0] if nodes else 0, nodes[-1] if nodes else 0, len(nodes)])
        self.list = lst
        return lst

    def put_ptr(self, a, p):
        self.words(a, [p])

    def set_ptrs(self, theme, tga):
        self.e.tp = theme
        self.words(self.addr["tga"] + 8, [tga])

    def dump(self):
        lst = self.list

        def data(node):
            return self.rd_word(node) if node else 0xDEAD

        head, tail = self.rd_word(lst + 4), self.rd_word(lst + 8)
        out = [self.rd_word(lst + 0xC), data(head), data(tail)]
        node, n = head, 0
        while node and n < 8:
            out += [data(node), data(self.rd_word(node + 8))]
            node, n = self.rd_word(node + 4), n + 1
        return [self.lab(x) if i else x for i, x in enumerate(out)]

    def listdel(self):
        return [(a == self.list, f) for a, f in self.L.listdel]

    def call(self, op, case):
        return call_ppc_op(self, op, case)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        p = lib.tpw_c2m_alloc
        self.map = lib.tpw_c2m_alloc(0x20100)
        self.sprites = [lib.tpw_c2m_newsprite() for _ in range(NSPR)]
        fixed = {1: self.map, 2: self.map + 0x2C, 3: self.g("ez"), 4: self.g("mz"), 5: self.g("tga")}
        for i, sp in enumerate(self.sprites):
            fixed[10 + i] = sp
        CL.host_begin(lib, fixed, case["queues"])
        self.list = 0

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_c2m_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_c2m_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def init_sprites(self):
        pass

    def mklist(self, items):
        lst = self.lib.tpw_c2m_newlist()
        for d in items:
            self.lib.tpw_c2m_addtail(lst, d)
        self.list = lst
        return lst

    def put_ptr(self, a, p):
        self.write(a, struct.pack("<Q", p))

    def images(self, ws):
        # the host keeps each image as a native pointer at 0x200b8 + 8 * k (CUI2DMap::HostImage), the Mac word at 0x20068 + 4 * k
        self.words(self.map + 0x20068, ws)
        self.write(self.map + 0x200B8, b"".join(struct.pack("<Q", w & MASK) for w in ws))

    def set_ptrs(self, theme, tga):
        self.lib.tpw_c2m_setptrs(theme, tga)

    def dump(self):
        out = (C.c_uint64 * 64)()
        n = self.lib.tpw_c2m_dump(self.list, out, 60)
        return [self.lab(x) if i else x for i, x in enumerate(list(out)[:n])]

    def listdel(self):
        return [(True, 1)]


def call_ppc_op(s, op, case):
    e = s.e
    m = s.map
    if op == "unreg":
        sprite = case["arg"]
        T.call_ppc(e, FN_UNREG, [m, s.sprite_addr(sprite)])
    elif op == "delete":
        T.call_ppc(e, FN_DELETE, [m])
    elif op == "updall":
        T.call_ppc(e, FN_UPDALL, [m])
    elif op == "close":
        T.call_ppc(e, FN_CLOSE, [m])
    elif op == "zoom":
        T.call_ppc(e, FN_ZOOM, [m, case["zoom"] & MASK, T.F(bitsf(case["amount"]))])
    elif op == "load":
        T.call_ppc(e, FN_LOAD, [m, case["reload"] & MASK])
    elif op == "updmap":
        T.call_ppc(e, FN_UPDMAP, [m])
    return 0


def sprite_addr(self, i):
    if i is None or i < 0:
        return 0
    return self.sprites[i]


Side.sprite_addr = sprite_addr


def call_host_op(s, op, case):
    L = s.lib
    m = s.map
    if op == "unreg":
        L.tpw_c2m_unregister(m, s.sprite_addr(case["arg"]))
    elif op == "delete":
        L.tpw_c2m_delete_entries(m)
    elif op == "updall":
        L.tpw_c2m_update_all(m)
    elif op == "close":
        L.tpw_c2m_close(m)
    elif op == "zoom":
        L.tpw_c2m_zoom(m, s32(case["zoom"]), bitsf(case["amount"]))
    elif op == "load":
        L.tpw_c2m_load(m, s32(case["reload"]))
    elif op == "updmap":
        L.tpw_c2m_update_map(m)
    return 0


Host.call = lambda self, op, case: call_host_op(self, op, case)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_c2m_"
    u64, u32, i32, f32 = C.c_uint64, C.c_uint32, C.c_int32, C.c_float
    sig = {
        "addr": (u64, [C.c_char_p]),
        "alloc": (u64, [u32]),
        "newsprite": (u64, []),
        "newlist": (u64, []),
        "addtail": (None, [u64, u64]),
        "dump": (i32, [u64, C.POINTER(u64), i32]),
        "setptrs": (None, [u64, u64]),
        "unregister": (None, [u64, u64]),
        "delete_entries": (None, [u64]),
        "update_all": (None, [u64]),
        "close": (None, [u64]),
        "zoom": (None, [u64, i32, f32]),
        "load": (None, [u64, i32]),
        "update_map": (None, [u64]),
    }
    for name, (res, args) in sig.items():
        f = getattr(lib, p + name)
        f.restype, f.argtypes = res, args
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.words(s.map + 0x20, case["mapw"])
    if case["op"] == "zoom":
        s.words(s.map + 0x5C, [case["z"]])
    s.images(case["images"])
    if case["items"] is not None:
        s.init_sprites()
        lst = s.mklist([s.sprite_addr(i) for i in case["items"]])
        s.put_ptr(s.map + 0x20080, lst)
    if case["op"] in ("load", "updmap"):
        theme = s.alloc(64)
        s.write(theme, case["theme"] + b"\0")
        tga = s.alloc(64)
        s.write(tga, case["tga"] + b"\0")
        s.set_ptrs(theme, tga)
        s.write(s.g("sep"), bytes([case["sep"]]))
        s.words(s.g("ts") + 0x30, case["ts"])


def observe(s, case):
    op = case["op"]
    o = {"log": s.log(), "map": s.rd_words(s.map + 0x20, 16)}
    if op in ("unreg", "delete", "updall"):
        o["list"] = s.dump()
    if op == "close":
        o["listptr"] = s.rd_word(s.map + 0x20080)
        o["listdel"] = s.listdel()
    return o


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"], case)
        return observe(s, case)
    finally:
        s.done()


LAST = {}


def ppc(e, case):
    LAST["ppc"] = run(Ppc(e, case), case)
    return LAST["ppc"]


def host(lib, case):
    h = run(Host(lib, case), case)
    if h != LAST.get("ppc"):
        print("PPC ", LAST.get("ppc", {}).get("log"))
        print("HOST", h["log"])
    return h


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-4, 4))
    return fb(r.uniform(-300, 300))


def base(r, op):
    return {
        "op": op,
        "queues": {"ReadW": [r.randrange(1, 600)], "ReadH": [r.randrange(1, 600)]},
        "mapw": [r.getrandbits(32) for _ in range(16)],
        "images": [r.getrandbits(32) for _ in range(6)],
        "items": None,
        "theme": r.choice([b"Data:Themes:Jungle", b"X", b"", b"Hello:World:Path"]),
        "tga": r.choice([b"2dmap.tga", b"map.tga", b"", b"a"]),
        "sep": r.choice([0x3A, 0x2F, 0x41, 0x20, 0x7E]),
        "ts": [r.getrandbits(32) for _ in range(8)],
    }


def rand_items(r):
    n = r.choice([0, 1, 2, 3, 4, 4])
    return [r.choice([-1, 0, 1, 2, 3]) for _ in range(n)]


ITEM_EDGES = [[], [0], [-1], [0, 1], [1, 0, 2], [0, -1, 1], [-1, -1], [1, 1], [0, 1, 2, 3], [3, -1, 2, 0], [2, 2, 2]]


def list_case(op, r, items=None):
    c = base(r, op)
    items = rand_items(r) if items is None else items
    if op in ("delete", "close"):
        # a sprite listed twice would be deleted twice (a crash on the host, not a case of the original)
        seen, uniq = set(), []
        for i in items:
            if i >= 0 and i in seen:
                continue
            seen.add(i)
            uniq.append(i)
        items = uniq
    c["items"] = items
    return c


def unreg_case(r, items=None, arg=None):
    c = list_case("unreg", r, items)
    c["arg"] = r.choice([-1, 0, 1, 2, 3]) if arg is None else arg
    return c


def unreg_edges():
    r = random.Random(1)
    for items in ITEM_EDGES:
        for arg in (-1, 0, 1, 2, 3):
            yield unreg_case(r, items, arg)


def unreg_random(r):
    return unreg_case(r)


def simple_edges(op, seed):
    def gen():
        r = random.Random(seed)
        for items in ITEM_EDGES:
            yield list_case(op, r, items)
    return gen


def simple_random(op):
    return lambda r: list_case(op, r)


def zoom_case(r, zoom=None, z=None, amount=None):
    c = base(r, "zoom")
    c["zoom"] = r.choice([-1, 0, 1, 2, 0x7FFFFFFF, -0x80000000, 0, 1]) if zoom is None else zoom
    c["z"] = (fb(r.uniform(0, 4)) if r.random() < 0.8 else rf(r)) if z is None else z
    c["amount"] = (fb(r.uniform(-1, 1)) if r.random() < 0.8 else rf(r)) if amount is None else amount
    return c


def zoom_edges():
    r = random.Random(6)
    for zoom in (-1, 0, 1, 2, 3, -0x80000000, 0x7FFFFFFF):
        for z in (fb(1.0), fb(3.0), fb(0.5), fb(2.0), fb(2.9), fb(3.5), 0x7FC00000, 0x7F800000, 0xFF800000, 0, 0x80000000):
            for amount in (fb(0.25), fb(-0.25), 0, 0x7FC00000, fb(5.0)):
                yield zoom_case(r, zoom, z, amount)


def load_case(r, op="load", reload=None, ts1=None):
    c = base(r, op)
    c["reload"] = r.choice([0, 1, -1, 2, 0x10000]) if reload is None else reload
    if ts1 is not None:
        c["ts"][1] = (c["ts"][1] & ~0x20) | ts1
    return c


def load_edges():
    r = random.Random(7)
    for reload in (0, 1, -1, 2, 0x10000, -0x80000000):
        for bit in (0, 0x20):
            yield load_case(r, "load", reload, bit)


def updmap_edges():
    r = random.Random(8)
    for bit in (0, 0x20):
        yield load_case(r, "updmap", 1, bit)
    c = load_case(r, "updmap", 1)
    c["queues"] = {"ReadW": [1], "ReadH": [1]}
    yield c
    c = load_case(r, "updmap", 1)
    c["queues"] = {"ReadW": [0], "ReadH": [0]}
    yield c


TESTS = [
    FnTest("CUI2DMap::UnRegister", ppc, host, edge=unreg_edges, rnd=unreg_random),
    FnTest("CUI2DMap::DeleteSpriteListEntries", ppc, host, edge=simple_edges("delete", 2), rnd=simple_random("delete")),
    FnTest("CUI2DMap::UpdateAllSpritePositions", ppc, host, edge=simple_edges("updall", 3), rnd=simple_random("updall")),
    FnTest("CUI2DMap::CloseSprites", ppc, host, edge=simple_edges("close", 4), rnd=simple_random("close")),
    FnTest("CUI2DMap::Zoom", ppc, host, edge=zoom_edges, rnd=lambda r: zoom_case(r)),
    FnTest("CUI2DMap::LoadMap", ppc, host, edge=load_edges, rnd=lambda r: load_case(r)),
    FnTest("CUI2DMap::UpdateMap", ppc, host, edge=updmap_edges, rnd=lambda r: load_case(r, "updmap", 1)),
]
