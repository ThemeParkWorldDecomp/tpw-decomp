"""Unit v51044: ten small functions of the online, lobby and 2D map code, against libtpw_unit_std_less_std_basic_string_operator.dylib
(tests/unit_std_less_std_basic_string_operator.cpp), main app ("SimTheme Park"), src/game/online/online.cpp, src/game/ui/ui_2dmap.cpp, src/game/ui/ui_worldLobbyController.cpp:
  std::less<wstring>::operator() 0x1021eb7c, std::allocator<nocase_string>::destroy 0x1021ecb8, basic_streambuf<wchar_t>::uflow 0x1021e4dc,
  std::allocator<pair<const nocase_string,BuddyInfo>>::destroy 0x10222c78, std::allocator<wstring>::destroy 0x102238d4,
  pair<const nocase_string,BuddyInfo>::pair 0x10227840, ShowWorldLobbySats 0x1022b7e8, HideWorldLobbySats 0x1022b85c,
  WorldLobbySetZoomButton 0x1022ba30, CUI2DMapSprite::CUI2DMapSprite 0x10230d64

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory at the Mac's addresses, big-endian,
and the host library's memory, little-endian). The callees outside the unit (the wstring operator<, operator delete, the nocase string
copy, the streambuf's underflow slot, InterfaceWindow::GetWindow, UI_SendMessage, InterfaceButton::SetState, CEZSprite's
CreateSpriteInstance and GetSpriteDetails, CUI2DMapSprite::SetVisibility, TbTList::AddTail) are fakes on both sides that record their
arguments and answer from the case's queues.

The world of a case: the blocks and constants the case names (each gets a label in the log), the string rep with its count, the
satellite parent global, the sprite list word of gUI2DMap, a details block. What is compared: the answer, the words the function may
write, and the event log. No fused operations. Not tested: Show/HideWorldLobbySats with a NULL parent (the original calls GetWindow on
NULL without a check; the fake would see this == 0, host compilers may assume this != NULL), a NULL rep in the destroy functions (read
unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_less_std_basic_string_operator"
MASK = 0xFFFFFFFF
P = "tpw_v51044_"

ENTRY = dict(
    WideStringLess=0x1021EB7C, AllocDestroyNocase=0x1021ECB8, Uflow=0x1021E4DC, AllocDestroyPair=0x10222C78,
    AllocDestroyWString=0x102238D4, PairCtor=0x10227840, ShowWorldLobbySats=0x1022B7E8, HideWorldLobbySats=0x1022B85C,
    WorldLobbySetZoomButton=0x1022BA30, SpriteCtor=0x10230D64)
OWN = set(ENTRY.values())

LT, DELETE, COPY, PTR_GLUE = 0x10225E8C, 0x10238218, 0x1022213C, 0x1023BAA8
GETWINDOW, SENDMSG, SETSTATE = 0x101DBE40, 0x101C9518, 0x101CB1D0
CREATE, DETAILS_FN, SETVIS, ADDTAIL = 0x1023AFC0, 0x1023AE40, 0x10230F78, 0x102318F0

ADDR = dict(gparent=0x10396FEC, gmap=0x1039742C, gei=0x103BCA60)
# host: sprite fields mF00, mKind, mMode, mMatch, scale (mF20 + 4), mSprite, mVisible, sizeof; CUI2DMap::mBytes; NocaseRep refs and data;
# BuddyEntry mDirect and mFlag; MslWStreambuf::mGNext
HOST_LAYOUT = [0, 0x30, 0x34, 0x38, 0x24, 0x40, 0x48, 0x50, 0, 8, 0x10, 8, 9, 0x10]
MAC_SPRITE = [0, 0x14, 0x1C, 0x20, 0x24, 0x28, 0x2C]
HOST_SPRITE = [0, 0x30, 0x34, 0x38, 0x24, 0x40, 0x48]

CUR = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


s32 = T.s32

# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes
STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_lt(e):
    L = e.cl
    L.rec("Lt", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.set_r(3, L.ret("Lt") & MASK)


def stub_delete(e):
    L = e.cl
    L.rec("Delete", (L.lab(e.r(3)),))


def stub_copy(e):
    L = e.cl
    L.rec("Copy", (L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5)), s32(e.r(6))))


def stub_ptr_glue(e):
    L = e.cl
    L.rec("Underflow", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("Underflow") & 0xFFFF)


def stub_getwindow(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("GetWindow") & MASK)


def stub_sendmsg(e):
    L = e.cl
    L.rec("SendMessage", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


def stub_setstate(e):
    L = e.cl
    L.rec("SetState", (L.lab(e.r(3)), T.s8(e.r(4)), T.s8(e.r(5))))
    e.set_r(3, 0)


def stub_create(e):
    L = e.cl
    L.rec("CreateSpriteInstance", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("CreateSpriteInstance") & MASK)


def stub_details(e):
    L = e.cl
    L.rec("GetSpriteDetails", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, CUR["details"])


def stub_setvis(e):
    L = e.cl
    o = e.r(3)
    L.rec("SetVisibility", (L.lab(o), s32(e.r(4)), s32(e.u32(o + 0x14)), s32(e.u32(o + 0x1C)), s32(e.u32(o + 0x20)),
                            s32(e.u32(o + 0x2C)), e.u32(o + 0x28)))


def stub_addtail(e):
    L = e.cl
    L.rec("AddTail", (L.lab(e.r(3)), L.lab(e.r(4))))


for _a, _f in ((LT, stub_lt), (DELETE, stub_delete), (COPY, stub_copy), (PTR_GLUE, stub_ptr_glue), (GETWINDOW, stub_getwindow),
               (SENDMSG, stub_sendmsg), (SETSTATE, stub_setstate), (CREATE, stub_create), (DETAILS_FN, stub_details),
               (SETVIS, stub_setvis), (ADDTAIL, stub_addtail)):
    STUBS.internal(_a, _f)


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

    def ptrw(self, a, v):
        """A pointer word: 4 bytes on the Mac, 8 on the host (host blocks are not 32-bit addressable)."""
        self.write(a, struct.pack(">I", v & MASK) if self.fmt == ">I" else struct.pack("<Q", v))

    def ptrr(self, a):
        return self.rd_word(a) if self.fmt == ">I" else struct.unpack("<Q", self.read(a, 8))[0]

    def make_world(self, case):
        """Blocks first (so the log can name them), then the constants; labels 1, 2, ... in that order."""
        self.b = {}
        labels = {}
        n = 0
        for name, size in case["blocks"]:
            self.b[name] = self.alloc(size)
            n += 1
            labels[n] = self.b[name]
        for name, val in case["consts"]:
            self.b[name] = self.g(name) if val is None else val
            n += 1
            labels[n] = self.b[name]
        return labels


class Ppc(Side):
    fmt = ">I"
    data_off = 0xC
    pair_off = 4
    sprite_offs = MAC_SPRITE

    def __init__(self, e, case):
        self.e = e
        labels = self.make_world(case)
        self.L = e.cl = CL.PpcLog({a: l for l, a in labels.items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "gei":      # an import: the loader binds the TOC slot at 0x10261e68 (the file says 0x103bca60); the code loads that word
            return self.e.u32(0x10261E68)
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_details(self, a):
        CUR["details"] = a

    def call(self, op, args):
        r3, _, _ = T.call_ppc(self.e, ENTRY[op], [a & MASK if isinstance(a, int) else a for a in args])
        return r3 & MASK

    def uflow(self, case):
        b = self.b
        self.words(b["obj"], [b["vt"]])
        self.words(b["obj"] + 8, [b["buf"] + 2 * case["k"]])
        r = self.call("Uflow", [b["obj"]])
        return r, self.rd_word(b["obj"] + 8)


class Host(Side):
    fmt = "<I"
    data_off = 0x10
    pair_off = 8
    sprite_offs = HOST_SPRITE

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        labels = self.make_world(case)
        CL.host_begin(lib, labels, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return getattr(self.lib, P + "addr")(name.encode())

    def alloc(self, n):
        return getattr(self.lib, P + "alloc")(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_details(self, a):
        self.ptrw(self.g("details"), a)

    def call(self, op, args):
        r = getattr(self.lib, P + op)(*args)
        return (r or 0) & 0xFFFFFFFFFFFFFFFF

    def uflow(self, case):
        b = self.b
        out = C.c_uint64(0)
        r = getattr(self.lib, P + "Uflow")(b["obj"], b["buf"] + 2 * case["k"], C.byref(out))
        return r, out.value


U64, U32, I32 = C.c_uint64, C.c_uint32, C.c_int32
SIGS = dict(
    WideStringLess=(U32, [U64, U64, U64]), AllocDestroyNocase=(None, [U64, U64]), AllocDestroyPair=(None, [U64, U64]),
    AllocDestroyWString=(None, [U64, U64]), PairCtor=(U64, [U64, U64]), Uflow=(U32, [U64, U64, C.POINTER(U64)]),
    ShowWorldLobbySats=(None, []), HideWorldLobbySats=(None, []), WorldLobbySetZoomButton=(None, [I32]),
    SpriteCtor=(None, [U64, U64, I32, I32, I32, C.c_float]))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, P + "addr").restype, getattr(lib, P + "addr").argtypes = U64, [C.c_char_p]
    getattr(lib, P + "alloc").restype, getattr(lib, P + "alloc").argtypes = U64, [U32]
    getattr(lib, P + "layout").argtypes = [C.POINTER(C.c_int64)]
    for name, (res, args) in SIGS.items():
        f = getattr(lib, P + name)
        f.restype, f.argtypes = res, args
    out = (C.c_int64 * 32)()
    n = getattr(lib, P + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, [hex(x) for x in list(out)[:n]]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the operations
def op_less(s, c):
    b = s.b
    r = s.call("WideStringLess", [b["functor"], b["a"], b["b"]])
    return {"answer": r & 0xFF, "log": s.log()}


def op_destroy(name):
    def run(s, c):
        b = s.b
        if c["has_p"]:
            s.words(b["rep"] + 8, [c["refs"]])
            s.ptrw(b["rep"] + s.data_off, b["data"])
            s.ptrw(b["p"], b["rep"])
        s.call(name, [b["alloc"], b["p"] if c["has_p"] else 0])
        out = {"log": s.log()}
        if c["has_p"]:
            slot = s.ptrr(b["p"])
            out["slot"] = "rep" if slot == b["rep"] else slot
            out["refs"] = s.rd_word(b["rep"] + 8)
        return out
    return run


def op_pair(s, c):
    b = s.b
    s.words(b["src"], [c["key"]])
    s.write(b["src"] + s.pair_off, bytes(c["flags"]))
    s.write(b["self"] + s.pair_off, bytes(c["dflags"]))
    r = s.call("PairCtor", [b["self"], b["src"]])
    return {"answer": "self" if r == b["self"] else r, "flags": list(s.read(b["self"] + s.pair_off, 2)), "log": s.log()}


def op_uflow(s, c):
    b = s.b
    s.write(b["buf"], struct.pack(s.fmt[0] + "8H", *c["chars"]))
    r, after = s.uflow(c)
    return {"answer": r & MASK, "adv": after - b["buf"], "log": s.log()}


def op_lobby(op):
    def run(s, c):
        b = s.b
        s.ptrw(s.g("gparent"), b["parent"] if c["has_parent"] else 0)
        s.call(op, [c["zoom"]] if op == "WorldLobbySetZoomButton" else [])
        return {"log": s.log()}
    return run


def op_sprite(s, c):
    b = s.b
    s.words(b["details"], c["details"])
    s.set_details(b["details"])
    s.ptrw(s.g("gmap") + 0x20080, b["list"])
    if isinstance(s, Ppc):
        r3, _, _ = T.call_ppc(s.e, ENTRY["SpriteCtor"], [b["obj"], b["image"], c["id"] & MASK, c["a"] & MASK, c["b"] & MASK,
                                                          T.F(bitsf(c["scale"]))])
        assert r3 & MASK == b["obj"]
    else:
        s.call("SpriteCtor", [b["obj"], b["image"], s32(c["id"]), s32(c["a"]), s32(c["b"]), bitsf(c["scale"])])
    return {"fields": [s.rd_word(b["obj"] + o) for o in s.sprite_offs], "details": s.rd_words(b["details"], 8), "log": s.log()}


def run(s, c, fn):
    try:
        return fn(s, c)
    finally:
        s.done()


def mk(fn):
    return (lambda e, case: run(Ppc(e, case), case, fn)), (lambda lib, case: run(Host(lib, case), case, fn))


# ---------------------------------------------------------------------------------------------------------------- the cases
def case(op, r, blocks=(), consts=(), queues=None, **kw):
    c = {"op": op, "blocks": list(blocks), "consts": list(consts), "queues": queues or {}}
    c.update(kw)
    return c


LT_VALUES = [0, 1, 2, 0xFF, 0x100, 0x1FF, 0xFFFFFF00, 0x80000000, 0xDEADBE01]


def less_case(r, v=None):
    v = r.choice(LT_VALUES) if v is None else v
    return case("less", r, consts=[("functor", 0x5000010), ("a", 0x5000020), ("b", 0x5000030)], queues={"Lt": [v]})


def less_edges():
    r = random.Random(11)
    for v in LT_VALUES:
        yield less_case(r, v)


REFS = [0, 1, 2, 3, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF]


def destroy_case(r, refs=None, has_p=True):
    refs = r.choice(REFS) if refs is None else refs
    return case("destroy", r, blocks=[("p", 8), ("rep", 0x20)], consts=[("data", 0x5000040), ("alloc", 0x5000050)], refs=refs, has_p=has_p)


def destroy_edges():
    r = random.Random(12)
    yield destroy_case(r, 0, False)
    for refs in REFS:
        yield destroy_case(r, refs)


def destroy_random(r):
    return destroy_case(r, r.choice(REFS + [r.randrange(1, 50)]), r.random() < 0.9)


def pair_case(r):
    return case("pair", r, blocks=[("self", 16), ("src", 16)], key=r.getrandbits(32),
                flags=[r.choice([0, 1, 0xFF, r.randrange(256)]) for _ in range(2)],
                dflags=[r.randrange(256) for _ in range(2)])


def pair_edges():
    r = random.Random(13)
    for _ in range(4):
        yield pair_case(r)


CHARS = [0, 1, 0x41, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF]


def uflow_case(r, v=None, k=None):
    v = r.choice([0xFFFF, 0x1FFFF, 0, 1, 0x12345, 0xFFFF0000, 0xABCDFFFF]) if v is None else v
    return case("uflow", r, blocks=[("obj", 0x80), ("vt", 0x40), ("buf", 0x20)], queues={"Underflow": [v]},
                k=r.randrange(8) if k is None else k, chars=[r.choice(CHARS + [r.getrandbits(16)]) for _ in range(8)])


def uflow_edges():
    r = random.Random(14)
    for v in (0xFFFF, 0x1FFFF, 0, 0xFFFF0000, 0xABCDFFFF):
        yield uflow_case(r, v, 0)
        yield uflow_case(r, v, 7)


def lobby_case(r, op, has_parent=True, zoom=0):
    return case(op, r, consts=[("parent", 0x5000100), ("child", 0x5000200)], queues={"GetWindow": [0x5000200]},
                has_parent=has_parent, zoom=zoom)


def sats_edges(op):
    def gen():
        yield lobby_case(random.Random(15), op)
    return gen


def zoom_edges():
    r = random.Random(16)
    for zoom in (1, 0, 2, -1, 0x10000001, 0x7FFFFFFF):
        yield lobby_case(r, "zoom", True, zoom)
    yield lobby_case(r, "zoom", False, 1)
    yield lobby_case(r, "zoom", False, 0)


def zoom_random(r):
    return lobby_case(r, "zoom", r.random() < 0.8, r.choice([1, 0, 2, -1, r.randrange(-5, 5), r.getrandbits(32) - 2 ** 31]))


SCALES = [0, 0x80000000, 0x3F800000, 0xBF800000, 0x7F800000, 0xFF800000, 0x00000001, 0x7F7FFFFF]


def sprite_case(r):
    k = r.random()
    scale = r.choice(SCALES) if k < 0.2 else fb(r.uniform(-8, 8))
    return case("sprite", r, blocks=[("obj", 0x60), ("details", 0x20)],
                consts=[("gei", None), ("image", 0x5000300), ("list", 0x5000500)],
                queues={"CreateSpriteInstance": [r.choice([0x5000400, 0x5000404, 0x7FFFFFF0])]},
                id=r.choice([0, 1, 5, -1, r.getrandbits(32) - 2 ** 31]), a=r.choice([0, 1, 2, 3, r.getrandbits(32) - 2 ** 31]),
                b=r.choice([0, -1, r.getrandbits(32) - 2 ** 31]), scale=scale, details=[r.getrandbits(32) for _ in range(8)])


def sprite_edges():
    r = random.Random(17)
    for _ in range(4):
        c = sprite_case(r)
        yield c
    c = sprite_case(r)
    c["details"][4] = 0
    yield c
    c = sprite_case(r)
    c["details"][4] = 0xFFFFFFFF
    yield c


def tests():
    out = []

    def add(name, fn, edge, rnd):
        p, h = mk(fn)
        out.append(FnTest(name, p, h, edge=edge, rnd=rnd))

    add("std::less<std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>>::operator()", op_less, less_edges,
        lambda r: less_case(r))
    add("std::allocator<nocase_string>::destroy", op_destroy("AllocDestroyNocase"), destroy_edges, destroy_random)
    add("std::basic_streambuf<wchar_t,std::char_traits<wchar_t>>::uflow", op_uflow, uflow_edges, lambda r: uflow_case(r))
    add("std::allocator<std::pair<const_nocase_string,BuddyInfo>>::destroy", op_destroy("AllocDestroyPair"), destroy_edges, destroy_random)
    add("std::allocator<std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>>::destroy",
        op_destroy("AllocDestroyWString"), destroy_edges, destroy_random)
    add("std::pair<const_nocase_string,BuddyInfo>::pair", op_pair, pair_edges, pair_case)
    add("ShowWorldLobbySats", op_lobby("ShowWorldLobbySats"), sats_edges("show"), lambda r: lobby_case(r, "show"))
    add("HideWorldLobbySats", op_lobby("HideWorldLobbySats"), sats_edges("hide"), lambda r: lobby_case(r, "hide"))
    add("WorldLobbySetZoomButton", op_lobby("WorldLobbySetZoomButton"), zoom_edges, zoom_random)
    add("CUI2DMapSprite::CUI2DMapSprite", op_sprite, sprite_edges, sprite_case)
    return out


TESTS = tests()
