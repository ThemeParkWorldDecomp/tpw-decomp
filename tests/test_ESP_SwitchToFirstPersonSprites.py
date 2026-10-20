"""Unit v5027, the sprite view switches and the script sprite adder, against libtpw_unit_ESP_SwitchToFirstPersonSprites.dylib
(tests/unit_ESP_SwitchToFirstPersonSprites.cpp), main app ("SimTheme Park"), src/ESP_sprites_ESP_SwitchToFirstPersonSprites.cpp:
  ESP_SwitchToFirstPersonSprites 0x1007cfc8, ESP_SwitchToThirdPersonSprites 0x1007d280, ESP_AddHLSpriteFromScript 0x1007ca9c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, 8-byte pointers) for the translation; the same code reads both back.
The callees outside the unit are fakes on both sides that record the same events: the string constructor, destructor, both assignments
and the append, the resource manager's FindResource, the resource's OpenQualifiedFile, the file object constructor, CSpriteFileIO's
Free, Load (which also sets the object's count from the case) and FlagAsSemitransparent, and
DetermineHFieldHeightQuick. operator delete is not logged (the host's is the arena's); the Free events before it show the path.

The world of a case: the view globals, the sprite table with up to four sprite sets (with or without a file object), the resource
manager token; for the adder a script block, the groups, the table of sprite sets with files and records, the cell counts and the
instance lists. What is compared: the answer, the event log, the globals and every sprite set's fields; for the adder the written words
of the script (the pointers it stores are compared as the file or record they select), the counts and the list slot written
(with the fields of the sprite listed: the Mac lists the record embedded in the block, the host the block's host record).

No fused operations (fadds, fdivs, fmuls, fctiwz on small values). Not tested: a cell width or height of 0 (divw by 0 is undefined),
float positions beyond 2^31 (fctiwz saturates differently in the emulator), a negative action number (it reads before the sprite's
action table, where the host's layout differs), a table index past the sprites set up, a total beyond the sprites set up.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ESP_SwitchToFirstPersonSprites"
MASK = 0xFFFFFFFF

FN_FIRST, FN_THIRD, FN_ADD = 0x1007cfc8, 0x1007d280, 0x1007ca9c
SIZES = {FN_FIRST: 640, FN_THIRD: 644, FN_ADD: 1060}
OWN = set(SIZES)

TPG = 0x103b74c8
RES_TOKEN = 0x4D000000
RM_TOKEN = 0x4D000100
ADDR = dict(compressed=0x1029d310, third=0x1029d360, draw=0x1029d364, groups=0x1029d368, total=0x102db1e8, sprites=0x102db520,
            cellsW=0x102dbd28, cellsH=0x102dbd24, f34=0x102dbd34, f88=0x102dc088, counts=0x102dbd7c, gcount=0x102dbd40,
            glist=0x102dbd44, lists=0x102dbdbc, maxx=0x1029d420, minx=0x1029d424, maxz=0x1029d428, minz=0x1029d42c,
            tpg=TPG, gamerm=0x1037f134 + 0x34, s4308=TPG + 0x4308, s431c=TPG + 0x431c, s42e0=TPG + 0x42e0, s42f4=TPG + 0x42f4,
            s4664=TPG + 0x4664, restoken=RES_TOKEN, rmtoken=RM_TOKEN)
LABELS = (("s4308", 1), ("s431c", 2), ("s42e0", 3), ("s42f4", 4), ("s4664", 5), ("restoken", 200), ("rmtoken", 201))
HOST_LAYOUT = [0x208, 0x20c, 0x224, 0x20, 0x114, 8, 16, 8, 0x268]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_find(e):
    L = e.cl
    L.rec("Find", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.write(e.r(5), struct.pack(">I", RES_TOKEN))
    e.set_r(3, L.ret("Find") & MASK)


def stub_open(e):
    L = e.cl
    L.rec("Open", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))
    e.set_r(3, L.ret("Open") & MASK)


def stub_load(e):
    L = e.cl
    e.write(e.r(3), struct.pack(">I", L.ret("LoadCount") & MASK))
    L.rec("Load", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.set_r(3, L.ret("LoadRet") & MASK)


def stub_new(e):
    n = e.r(3)
    if n == 0:
        n = 4
    a = e.alloc(n)
    e.write(a, bytes(n))
    e.set_r(3, a)


def stub_delete(e):
    pass     # the host's operator delete is the arena's and cannot log; the Free events before it show the path


def stub_height(e):
    L = e.cl
    L.rec("Height", (fb(e.f(1)), fb(e.f(2))))
    e.set_f1(bitsf(L.ret("Height")))


STUBS.imp("__ct__26TbDynamicStringTemplate<c>Fv", CL.recorder("StrCtor", "N"))
STUBS.imp("__dt__26TbDynamicStringTemplate<c>Fv", CL.recorder("StrDtor", "L"))
STUBS.imp("__as__26TbDynamicStringTemplate<c>FRC26TbDynamicStringTemplate<c>", CL.recorder("AssignStr", "LL"))
STUBS.imp("__as__26TbDynamicStringTemplate<c>FPCc", CL.recorder("AssignC", "Ls"))
STUBS.imp("__apl__26TbDynamicStringTemplate<c>FRC15TbStringBase<c>", CL.recorder("Append", "LL"))
STUBS.imp("__ct__Q33bfl10FileSystem5CFileFv", CL.recorder("FileCtor", "N"))
STUBS.imp("Free__13CSpriteFileIOFv", CL.recorder("Free", "L"))
STUBS.imp("FlagAsSemitransparent__13CSpriteFileIOFv", CL.recorder("Flag", "L"))
STUBS.internal(0x10165448, stub_find)
STUBS.internal(0x101630d8, stub_open)
STUBS.imp("Load__13CSpriteFileIOFPvi", stub_load)
STUBS.imp("__nw__FUl", stub_new)
STUBS.imp("__dl__FPv", stub_delete)
STUBS.internal(0x1005eda8, stub_height)


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

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def rd_byte(self, a):
        return self.read(a, 1)[0]

    def o(self, off):
        """The host offset of a field of ESP_CSprite by its Mac offset (the first pointer is wider)."""
        return off if off < 4 else off + self.psz - 4

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    rstride = 0x18
    gstride, gfirst = 12, 4

    def __init__(self, e, case):
        self.e = e
        fixed = {ADDR[n]: lab for n, lab in LABELS}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        a = self.e.alloc(n)
        self.e.write(a, bytes(n))
        return a

    heap = alloc

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, args)
        return r3 & MASK

    def listed(self, e, sc):
        """Whether the list entry `e` is the script's sprite, and that sprite's fields: the record embedded in the block."""
        if e != sc + 0xd0:
            return False, None
        ws = self.rd_words(e, 14)
        return True, dict(words=ws[:10], sprite=ws[10], f2c=ws[11], rot=ws[12], file=ws[13], base=self.rd_half(e + 0x38),
                          light=list(self.read(e + 0x3c, 4)))


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8
    rstride = 0x20
    gstride, gfirst = 16, 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {lab: self.g(n) for n, lab in LABELS}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_esp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_esp_alloc(n)

    def heap(self, n):
        return self.lib.tpw_esp_heap(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def listed(self, e, sc):
        """The host lists the script's host record (src/ScriptHostSprites.cpp), with the host's layout."""
        if e != self.lib.tpw_esp_hostsprite(sc):
            return False, None
        out = (C.c_uint64 * 16)()
        self.lib.tpw_esp_sprite_fields(e, out)
        v = list(out)
        return True, dict(words=v[:10], sprite=v[10], f2c=v[11], rot=v[12], file=v[13], base=v[14],
                          light=[(v[15] >> (8 * i)) & 0xFF for i in range(4)])

    def call(self, fn, args):
        if fn == FN_FIRST:
            return self.lib.tpw_esp_first() & MASK
        if fn == FN_THIRD:
            return self.lib.tpw_esp_third() & MASK
        self.lib.tpw_esp_add(args[0])
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_esp_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "heap").restype, getattr(lib, p + "heap").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "first").restype = C.c_uint32
    getattr(lib, p + "third").restype = C.c_uint32
    getattr(lib, p + "add").argtypes = [C.c_uint64]
    getattr(lib, p + "hostsprite").restype, getattr(lib, p + "hostsprite").argtypes = C.c_uint64, [C.c_uint64]
    getattr(lib, p + "sprite_fields").argtypes = [C.c_uint64, C.POINTER(C.c_uint64)]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_switch(s, c):
    s.write(s.g("compressed"), bytes([c["compressed"]]))
    s.words(s.g("third"), [c["third"]])
    s.words(s.g("total"), [c["total"]])
    s.ptr(s.g("gamerm"), s.g("rmtoken"))
    s.sprs = []
    for i, sp in enumerate(c["sprites"]):
        a = s.alloc(0x270)
        if sp["file"] is not None:
            f = s.heap(0x40)
            s.words(f, [sp["file"]])
            s.ptr(a, f)
        s.write(a + s.o(4), sp["name"].encode() + b"\0")
        s.write(a + s.o(0x204), bytes([sp["loaded"], sp["fp"], 0x33, sp["semi"]]))
        s.words(a + s.o(0x208), [sp["count"]])
        s.ptr(s.g("sprites") + i * s.psz, a)
        s.sprs.append(a)


def observe_switch(s, c, answer):
    out = {"answer": answer, "log": s.log(), "third": s.rd_word(s.g("third"))}
    for i, a in enumerate(s.sprs):
        f = s.rd_ptr(a)
        out["s%d" % i] = [f != 0, s.rd_byte(a + s.o(0x204)), s.rd_byte(a + s.o(0x205)), s.rd_byte(a + s.o(0x206)),
                          s.rd_byte(a + s.o(0x207)), s.rd_word(a + s.o(0x208)), s.rd_word(f) if f else None,
                          s.read(a + s.o(4), 12)]
    return out


def build_add(s, c):
    S = c["script"]
    sc = s.alloc(0x120)
    s.sc = sc
    s.words(sc + 0xd0, [0xA5A5A5A5] * (0x44 // 4))
    s.words(sc + 0x114, [S["has"]])
    for off, key in ((0xb8, "frame"), (0xac, "kind"), (0xc4, "flags"), (0xa0, "a0"), (0xb0, "b0"), (0xb4, "b4"), (0xc0, "c0"),
                     (0xa4, "f31"), (0xa8, "f30"), (0x88, "x"), (0x8c, "y"), (0x90, "z")):
        s.words(sc + off, [S[key]])
    s.write(s.g("draw"), bytes([c["draw"]]))
    s.words(s.g("cellsW"), [c["cellsW"]])
    s.words(s.g("cellsH"), [c["cellsH"]])
    s.words(s.g("f34"), [c["f34"]])
    s.words(s.g("f88"), [c["f88"]])
    s.words(s.g("gcount"), [c["gcount"]])
    s.words(s.g("counts"), c["counts"])
    for k, key in enumerate(("maxx", "minx", "maxz", "minz")):
        s.words(s.g(key), [(3, 0, 3, 0)[k]])
    for k in range(14):
        s.words(s.g("groups") + k * s.gstride + s.gfirst, [c["groups"][k]])
    s.files = []
    sets = []
    for sd in c["sets"]:
        count = sd["count"]
        rb = s.alloc(max(count, 1) * s.rstride)
        f = s.alloc(0x20)
        s.words(f, [count])
        s.ptr(f + s.psz, rb)
        s.files.append((f, rb, count))
        a = s.alloc(0x270)
        s.ptr(a, f)
        for k, (first, frames, views) in enumerate(sd["actions"]):
            ap = a + s.o(0x220) + 4 * k
            s.half(ap, first)
            s.write(ap + 2, bytes([frames, views]))
        sets.append(a)
    for t in range(32):
        s.ptr(s.g("sprites") + t * s.psz, sets[c["table"][t]])
    s.lists = []
    for k in range(16):
        arr = s.alloc(0x800 * s.psz)
        s.lists.append(arr)
        s.ptr(s.g("lists") + k * s.psz, arr)
    s.garr = s.alloc(0x40 * s.psz)
    s.ptr(s.g("glist"), s.garr)


def observe_add(s, c, answer):
    sc = s.sc
    out = {"log": s.log()}
    for off in range(0xd0, 0x114, 4):
        if off == 0xf8:
            v = s.rd_word(sc + off)
            r = ("raw", v)
            for fi, (f, rb, count) in enumerate(s.files):
                rb &= MASK      # the script holds the low 32 bits of the host's address
                if rb <= v < rb + max(count, 1) * s.rstride:
                    r = ("rec", fi, (v - rb) // s.rstride, (v - rb) % s.rstride)
            out[off] = r
        elif off == 0x104:
            v = s.rd_word(sc + off)
            r = ("raw", v)
            for fi, (f, rb, count) in enumerate(s.files):
                if v == f & MASK:
                    r = ("file", fi)
            out[off] = r
        elif off == 0x108:
            out[off] = (s.rd_half(sc + 0x108), s.rd_byte(sc + 0x10a), s.rd_byte(sc + 0x10b))
        else:
            out[off] = s.rd_word(sc + off)
    out["f34"] = s.rd_word(s.g("f34"))
    out["gcount"] = s.rd_word(s.g("gcount"))
    out["counts"] = s.rd_words(s.g("counts"), 16)
    for k in range(16):
        slot = c["counts"][k]
        if slot < 0x800:
            out["L%d" % k] = listed_entry(s, c, s.rd_ptr(s.lists[k] + slot * s.psz))
    if c["gcount"] < 0x40:
        out["G"] = listed_entry(s, c, s.rd_ptr(s.garr + c["gcount"] * s.psz))
    return out


def listed_entry(s, c, e):
    """A list slot: 1 and the listed sprite's fields when it is the script's sprite (the Mac lists the record embedded in the
    block, the host the block's host record), else what the slot holds. The sprite record and the frame file are compared as
    what they select; the frame fields only when the script's flag 0x200 has the adder write them."""
    hit, f = s.listed(e, s.sc)
    if not hit:
        return e
    r = ("raw", f["sprite"])
    for fi, (_, rb, count) in enumerate(s.files):
        if rb <= f["sprite"] < rb + max(count, 1) * s.rstride:
            r = ("rec", fi, (f["sprite"] - rb) // s.rstride, (f["sprite"] - rb) % s.rstride)
    got = [1, f["words"], r, f["f2c"], f["rot"], f["light"]]
    if c["script"]["flags"] & 0x200:
        fr = ("raw", f["file"])
        for fi, (fa, _, _) in enumerate(s.files):
            if f["file"] == fa:
                fr = ("file", fi)
        got += [fr, f["base"]]
    return got


def run(s, c):
    try:
        if c["op"] == "add":
            build_add(s, c)
            s.call(FN_ADD, [s.sc])
            return observe_add(s, c, 0)
        build_switch(s, c)
        return observe_switch(s, c, s.call(FN_FIRST if c["op"] == "first" else FN_THIRD, []))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

NAMES = ["", "kid", "SPR_BI", "a", "abcdefghij", "x" * 40]


def sw_case(r, op, third=None, comp=None, find=None, n=None, **ov):
    n = r.choice([0, 1, 2, 3, 4]) if n is None else n
    sprites = []
    for _ in range(n):
        sprites.append(dict(name=r.choice(NAMES), fp=ov.get("fp", int(r.random() < 0.7)), loaded=r.choice([0, 1]),
                            semi=ov.get("semi", r.choice([0, 1, 2])), count=r.choice([0, 5, 0x1234, 0x7fffffff]),
                            file=ov.get("file", r.choice([None, 7, 0]))))
    total = ov.get("total", n)
    c = {"op": op, "compressed": r.choice([0, 1]) if comp is None else comp,
         "third": r.choice([0, 1, 2, -1]) if third is None else third, "total": total, "sprites": sprites}
    k = max(n, 1)
    c["queues"] = {"Find": [r.choice([1, 1, 1, 0]) if find is None else find],
                   "Open": [ov.get("open", r.choice([1, 1, 0])) for _ in range(k)],
                   "LoadRet": [ov.get("loadret", r.choice([1, 1, 0])) for _ in range(k)],
                   "LoadCount": [r.choice([0, 3, 99, -1]) for _ in range(k)]}
    return c


def sw_edges(op):
    def gen():
        r = random.Random(5 if op == "first" else 6)
        same = 0 if op == "first" else 1
        for third in (same, 1 - same, 2, -1):
            for comp in (0, 1):
                yield sw_case(r, op, third=third, comp=comp, find=1, n=3)
        yield sw_case(r, op, third=1 - same, find=0, n=2)
        yield sw_case(r, op, third=1 - same, find=1, n=0)
        yield sw_case(r, op, third=1 - same, find=1, n=2, fp=0)
        yield sw_case(r, op, third=1 - same, find=1, n=2, fp=1, file=None, open=1, loadret=1, semi=1)
        yield sw_case(r, op, third=1 - same, find=1, n=2, fp=1, file=0, open=1, loadret=0, semi=0)
        yield sw_case(r, op, third=1 - same, find=1, n=2, fp=1, file=7, open=0)
        yield sw_case(r, op, third=1 - same, find=1, n=3, total=1, fp=1, open=1)
    return gen


def sw_random(op):
    def gen(r):
        return sw_case(r, op)
    return gen


def add_case(r, **ov):
    def pick(key, fn):
        return ov[key] if key in ov else fn()

    flags = 0
    for bit in (0x1000000, 0x40000, 0x2000000, 0x200):
        if r.random() < 0.4:
            flags |= bit
    flags |= r.getrandbits(24) & ~(0x40000 | 0x200 | 0x1000000 | 0x2000000) if r.random() < 0.5 else 0
    flags |= r.choice([0, 0, 0x7f000000, 0x20000000])
    flags = pick("flags", lambda: flags)
    script = dict(has=pick("has", lambda: r.choice([1, 1, 1, 5, 0])),
                  frame=pick("frame", lambda: r.choice([-1, 0, 1, 2, 5, 9, -2]) if r.random() < 0.15 else r.randrange(0, 11)),
                  kind=pick("kind", lambda: r.choice([5, 5, 0, 13, r.randrange(0, 14)])), flags=flags,
                  a0=r.getrandbits(32), b0=r.randrange(0, 4), b4=pick("b4", lambda: r.randrange(0, 64)),
                  c0=pick("c0", lambda: r.randrange(-3, 13)), f31=fb(r.uniform(-9, 9)), f30=fb(r.uniform(-9, 9)),
                  x=pick("x", lambda: fb(r.uniform(-200, 600)) if r.random() < 0.9 else fb(r.uniform(-3e6, 3e6))),
                  y=fb(r.uniform(-50, 50)),
                  z=pick("z", lambda: fb(r.uniform(-200, 600)) if r.random() < 0.9 else fb(r.uniform(-3e6, 3e6))))
    sets = []
    for _ in range(4):
        sets.append(dict(count=r.choice([0, 1, 2, 3, 6]),
                         actions=[(r.choice([0, 1, 3, 8, 0xffff]), r.choice([0, 1, 2, 5]), r.choice([0, 0, 1, 2, 4, 8]))
                                  for _ in range(16)]))
    counts = [pick("count", lambda: r.choice([0, 1, 5, 0x7ff, 0x800, 0x900, 0x7fffffff, -1]) if r.random() < 0.3 else r.randrange(0, 4))
              for _ in range(16)]
    return {"op": "add", "script": script, "draw": pick("draw", lambda: r.choice([1, 1, 1, 0])),
            "cellsW": r.randrange(1, 6), "cellsH": r.randrange(1, 6), "f34": r.getrandbits(32),
            "f88": fb(r.uniform(-3, 3)), "gcount": pick("gcount", lambda: r.randrange(0, 0x30)),
            "counts": [x & MASK if x >= 0 else x & MASK for x in counts],
            "groups": [r.randrange(0, 9) for _ in range(14)], "sets": sets,
            "table": [r.randrange(0, 4) for _ in range(32)],
            "queues": {"Height": [fb(r.uniform(-20, 20))]}}


def add_edges():
    r = random.Random(11)
    yield add_case(r, has=0)
    yield add_case(r, frame=-1)
    yield add_case(r, draw=0)
    for fl in (0, 0x1000000, 0x40000, 0x2000000, 0x200, 0x40200, 0x1000200, 0x3040200, 0x2000200):
        for kind in (5, 2):
            yield add_case(r, flags=fl, kind=kind)
    for cnt in (0x7ff, 0x800, 0x7fffffff):
        yield add_case(r, flags=0, count=cnt)
    yield add_case(r, flags=0x40000, gcount=0x3f)
    yield add_case(r, flags=0, x=fb(-500.0), z=fb(-500.0))
    yield add_case(r, flags=0, x=fb(5000.0), z=fb(5000.0))
    for c0 in range(-2, 12):
        yield add_case(r, flags=0x200, c0=c0)
    for b4 in (0, 15, 16, 17, 63):
        yield add_case(r, b4=b4)


def add_random(r):
    return add_case(r)


TESTS = [
    FnTest("ESP_SwitchToFirstPersonSprites", ppc, host, edge=sw_edges("first"), rnd=sw_random("first")),
    FnTest("ESP_SwitchToThirdPersonSprites", ppc, host, edge=sw_edges("third"), rnd=sw_random("third")),
    FnTest("ESP_AddHLSpriteFromScript", ppc, host, edge=add_edges, rnd=add_random),
]
