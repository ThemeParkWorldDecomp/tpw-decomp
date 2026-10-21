"""Unit v51047, nine functions of the main app, against libtpw_unit_std_basic_istream_basic_istream.dylib
(tests/unit_std_basic_istream_basic_istream.cpp), src/game/save/FS_LoadSave.cpp, src/game/ui/InterfaceKnob.cpp, src/game/ui/UIGrapher.cpp, src/game/ui/ui_2dmap.cpp:
  std::basic_istream<char>::basic_istream 0x1020a4c0, CUIGrapher::Clear 0x101fd9d0, CUIGrapher::CUIGrapher 0x101fd6e8,
  CFSLoadSave::GetNextSaveFileDetails 0x101fcd08, CUI2DMapSprite::~CUI2DMapSprite 0x10230ea4, SetVisibility 0x10230f78,
  SetScale 0x10231094, entry 0x102381a4, InterfaceKnob::operator- 0x101d038c.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses
(big-endian) for the original and in the host library's memory (little-endian) for the translation. The callees outside the unit
are fakes on both sides that record their arguments and answer from the case: ios_base::init, basic_ios::widen,
LbDraw_RectangleFilled, TPUniStrEq, CEZSprite::GetSpriteDetails and DeleteSpriteInstance, the sprite's operator delete, the
runtime start-up, main, exit and the knob's WhatAmI (the glue call). TbTList::Find and RemoveAt run for real on both sides.

What is compared: the answer, the whole object or block written (pointers as offsets into the block or as names of the vtable
globals), the detail block of a sprite, the sprite list after a destructor, and the call log. No fused operations.
Not tested: a NULL list pointer in the map (the original reads through it), the host's missing delete flag (the destructor is
reached through CUI2DMapSpriteDtor, which keeps the Mac's flag and its delete).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_istream_basic_istream"
MASK = 0xFFFFFFFF

F_IST, F_CLEAR, F_CTOR, F_GNSFD = 0x1020A4C0, 0x101FD9D0, 0x101FD6E8, 0x101FCD08
F_SDTOR, F_SETVIS, F_SETSCALE, F_ENTRY, F_KNOB = 0x10230EA4, 0x10230F78, 0x10231094, 0x102381A4, 0x101D038C
SIZES = {F_IST: 200, F_CLEAR: 84, F_CTOR: 136, F_GNSFD: 108, F_SDTOR: 168, F_SETVIS: 104, F_SETSCALE: 112, F_ENTRY: 68,
         F_KNOB: 128}
OWN = set(SIZES)

C_INIT, C_WIDEN, C_RECT, C_UNISTR = 0x10238968, 0x10072960, 0x1023AD98, 0x1000548C
C_FIND, C_REMOVEAT, C_DELINST, C_GETDET = 0x10231600, 0x10231738, 0x1023AFA8, 0x1023AE40
C_DELETE, C_RTINIT, C_MAIN, C_EXIT, C_GLUE = 0x10238218, 0x10238134, 0x10000208, 0x1023A0A8, 0x1023BAA8

ADDR = dict(OstreamVt1=0x1029D0B0, IosBaseVt=0x103BD034, IfstreamVt=0x102B173C, SpriteVt=0x102B2958, UI2DMap=0x1039742C)
# sizeof(CUIGrapher), mGrid, mBG; the rest of the host layout (sprite and knob fields) is read from the library.
HOST_LAYOUT_FIXED = [0x44, 0x3A, 0x3F]

CUR = {}
IDX = {}


def s32(v):
    return T.s32(v)


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.update({C_FIND, C_REMOVEAT})


def st_init(e):
    e.cl.rec("Init", ((e.r(3) - CUR["base"]) & MASK, e.r(4) & MASK))


def st_widen(e):
    L = e.cl
    L.rec("Widen", ((e.r(3) - CUR["base"]) & MASK, e.r(4) & 0xFF))
    e.set_r(3, L.ret("Widen") & 0xFF)


def st_rect(e):
    l, t, r, b = struct.unpack(">4i", e.read(e.r(3), 16))
    e.cl.rec("Rect", (l, t, r, b, e.r(4) & MASK, (e.r(5) >> 24) & 0xFF))


def st_unistr(e):
    L = e.cl
    L.rec("TPUniStrEq", (IDX.get(e.r(3), -1), IDX.get(e.r(4), -1)))
    e.set_r(3, L.ret("TPUniStrEq") & 1)


def st_delinst(e):
    e.cl.rec("DeleteSpriteInstance", (e.r(4) & MASK,))
    e.set_r(3, 1)


def st_getdet(e):
    e.cl.rec("GetSpriteDetails", (e.r(4) & MASK,))
    e.set_r(3, CUR["details"])


def st_delete(e):
    if e.r(3) == CUR.get("sprite"):
        e.cl.rec("Delete", (1,))


def st_rtinit(e):
    e.cl.rec("RuntimeInit", ())


def st_main(e):
    assert e.r(3) == 0 and e.u32(e.r(4)) == 0
    e.cl.rec("main", ())


def st_exit(e):
    e.cl.rec("exit", (e.r(3) & MASK,))


def st_glue(e):
    idx = 0 if e.r(3) == CUR["obj0"] else 1
    e.cl.rec("WhatAmI", (idx,))
    e.set_r(3, e.cl.ret("WhatAmI:%d" % idx) & MASK)


for _a, _f in ((C_INIT, st_init), (C_WIDEN, st_widen), (C_RECT, st_rect), (C_UNISTR, st_unistr), (C_DELINST, st_delinst),
               (C_GETDET, st_getdet), (C_DELETE, st_delete), (C_RTINIT, st_rtinit), (C_MAIN, st_main), (C_EXIT, st_exit),
               (C_GLUE, st_glue)):
    def _glued(e, _f=_f):
        # the real glue saves the caller's TOC in the linkage area (14(r1)) and the caller reloads it after the call
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        _f(e)

    STUBS.internal(_a, _glued)


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

    def pw(self, addr):
        """The word that holds a pointer to an arena block: the Mac's address, the host's offset from the arena base (Ptr32)."""
        return (addr - self.arena) & MASK

    def norm(self, w, base, size):
        """A word with the addresses of the vtable globals as names and pointers into the block (base is its pw) as offsets."""
        w &= MASK
        for name in ("OstreamVt1", "IosBaseVt", "IfstreamVt", "SpriteVt"):
            a = self.g(name) & MASK
            if w == a:
                return name
            if w == a + 0xC:
                return name + "+c"
        if base <= w < base + size:
            return ("o", w - base)
        return w


class Ppc(Side):
    fmt = ">I"
    is_host = False
    sprite_off, vis_off, knob_off = 0x28, 0x2C, 0x158
    det_sprite, det_scale, width_off = 0x28, 0x2C, 0xC   # SetScale's detail block (the sprite instance) and its sprite
    arena = 0

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        CUR.clear()
        IDX.clear()
        self.saved = CL.Stubs.activate(STUBS, e) if False else STUBS.activate(e)

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

    def call(self, addr, args):
        return T.call_ppc(self.e, addr, args)[0] & MASK

    def put_sprite(self, p, v):
        self.words(p + 0x28, [v])

    def get_sprite(self, p):
        return self.rd_word(p + 0x28)


class Host(Side):
    fmt = "<I"
    is_host = True

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.sprite_off, self.vis_off, self.knob_off = lib.layout[4], lib.layout[5], lib.layout[7]
        self.det_sprite, self.det_scale, self.width_off = lib.layout[8], lib.layout[9], lib.layout[10]
        self.arena = lib.tpw_isg_arena_base()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_isg_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_isg_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def put_sprite(self, p, v):
        self.write(p + self.sprite_off, struct.pack("<Q", v & MASK))

    def get_sprite(self, p):
        return struct.unpack("<Q", self.read(p + self.sprite_off, 8))[0]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_isg_"
    lib.tpw_isg_addr.restype, lib.tpw_isg_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_isg_alloc.restype, lib.tpw_isg_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_isg_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_isg_arena_base.restype, lib.tpw_isg_arena_base.argtypes = C.c_uint64, []
    lib.tpw_isg_istream.restype, lib.tpw_isg_istream.argtypes = None, [C.c_uint64, C.c_int32, C.c_uint32]
    lib.tpw_isg_clear.restype, lib.tpw_isg_clear.argtypes = None, [C.c_uint64]
    lib.tpw_isg_ctor.restype, lib.tpw_isg_ctor.argtypes = None, [C.c_uint64]
    lib.tpw_isg_gnsfd.restype, lib.tpw_isg_gnsfd.argtypes = C.c_uint64, [C.POINTER(C.c_uint64), C.c_int32, C.c_uint64]
    lib.tpw_isg_setlist.restype, lib.tpw_isg_setlist.argtypes = None, [C.POINTER(C.c_uint64), C.c_int32]
    lib.tpw_isg_getlist.restype, lib.tpw_isg_getlist.argtypes = C.c_int32, [C.POINTER(C.c_uint64), C.c_int32]
    lib.tpw_isg_set_details.restype, lib.tpw_isg_set_details.argtypes = None, [C.c_uint64]
    lib.tpw_isg_sdtor.restype, lib.tpw_isg_sdtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_isg_setvis.restype, lib.tpw_isg_setvis.argtypes = None, [C.c_uint64, C.c_int32]
    lib.tpw_isg_setscale.restype, lib.tpw_isg_setscale.argtypes = None, [C.c_uint64, C.c_float]
    lib.tpw_isg_entry.restype, lib.tpw_isg_entry.argtypes = None, []
    lib.tpw_isg_knob_sub.restype, lib.tpw_isg_knob_sub.argtypes = C.c_int32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_isg_layout(out)
    lib.layout = list(out)[:n]
    assert lib.layout[:3] == HOST_LAYOUT_FIXED, lib.layout
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the operations


def op_ist(s, case):
    size = 0x200
    obj = s.alloc(size)
    s.words(obj, case["fill"])
    s.words(obj, [s.pw(obj) + case["iosoff"]])
    if s.is_host:
        s.lib.tpw_isg_istream(obj, s32(case["flag"]), case["sb"])
    else:
        CUR["base"] = obj & MASK
        s.call(F_IST, [obj, case["flag"] & MASK, case["sb"]])
    words = s.rd_words(obj, size // 4)
    mem = [s.norm(w, s.pw(obj), size) for w in words]
    # mFill is one byte (stb) at ios + 0x40: its word is the byte and the three untouched bytes, whatever the byte order
    ios = mem[0][1] if isinstance(mem[0], tuple) else 0
    idx = (ios + 0x40) // 4
    keep = (words[idx] >> 8 == case["fill"][idx] >> 8) if s.is_host else (words[idx] & 0xFFFFFF == case["fill"][idx] & 0xFFFFFF)
    mem[idx] = (s.read(obj + ios + 0x40, 1), keep)
    return {"mem": mem}


def op_grapher(s, case):
    obj = s.alloc(0x60)
    s.words(obj, case["fill"][:12])
    s.write(obj + 0x30, b"".join(struct.pack(">I", w & MASK) for w in case["fill"][12:]))   # the colours are bytes: same order
    clear = case["op"] == "clear"
    if s.is_host:
        (s.lib.tpw_isg_clear if clear else s.lib.tpw_isg_ctor)(obj)
    else:
        s.call(F_CLEAR if clear else F_CTOR, [obj])
    return {"ints": s.rd_words(obj, 12), "tail": s.read(obj + 0x30, 0x30)}


def op_gnsfd(s, case):
    n = case["n"]
    elems = [s.alloc(0x20) for _ in range(n)]
    pi = case["prev"]
    prev = elems[pi] if 0 <= pi < n else s.alloc(0x20)
    if s.is_host:
        arr = (C.c_uint64 * 16)(*elems)
        ans = s.lib.tpw_isg_gnsfd(arr, n, prev)
    else:
        nodes = [s.alloc(12) for _ in range(n)]
        for i in range(n):
            s.words(nodes[i], [elems[i], nodes[i + 1] if i + 1 < n else 0, nodes[i - 1] if i > 0 else 0])
        lst = s.alloc(16)
        s.words(lst, [0, nodes[0] if n else 0, nodes[-1] if n else 0, n])
        IDX[prev] = 100
        for i in range(n):
            IDX[elems[i]] = i
        ans = s.call(F_GNSFD, [lst, prev])
    where = {prev: 100}
    for i in range(n):
        where[elems[i]] = i
    return {"ans": -1 if ans == 0 else where.get(ans, -2)}


def list_state(s, pool):
    where = {p: i for i, p in enumerate(pool)}
    if s.is_host:
        out = (C.c_uint64 * 24)()
        n = s.lib.tpw_isg_getlist(out, 16)
        return {"count": out[0], "head_null": out[1], "tail_last": out[2], "seq": [where.get(out[4 + i], -1) for i in range(n)]}
    lst = s.rd_word(s.g("UI2DMap") + 0x20080)
    _, head, tail, count = s.rd_words(lst, 4)
    seq, node, last = [], head, 0
    while node and len(seq) < 16:
        seq.append(where.get(s.rd_word(node), -1))
        last = node
        node = s.rd_word(node + 4)
    return {"count": count, "head_null": 1 if head == 0 else 0, "tail_last": 1 if last == tail else 0, "seq": seq}


def op_sdtor(s, case):
    pool = [s.alloc(0x80) for _ in range(4)]
    for i, p in enumerate(pool):
        s.words(p, [case["word0"][i]])
        s.put_sprite(p, case["inst"][i])
        s.write(p + s.vis_off, struct.pack(s.fmt, case["vis"][i] & MASK)) if s.is_host else s.words(p + 0x2C, [case["vis"][i]])
    target = 0 if case["null"] else pool[0]
    members = [pool[i] for i in case["list"]]
    if s.is_host:
        arr = (C.c_uint64 * 16)(*members)
        s.lib.tpw_isg_setlist(arr, len(members))
        ans = s.lib.tpw_isg_sdtor(target, s32(case["flag"]))
    else:
        nodes = [s.alloc(12) for _ in members]
        n = len(members)
        for i in range(n):
            s.words(nodes[i], [members[i], nodes[i + 1] if i + 1 < n else 0, nodes[i - 1] if i > 0 else 0])
        lst = s.alloc(16)
        s.words(lst, [0, nodes[0] if n else 0, nodes[-1] if n else 0, n])
        s.words(s.g("UI2DMap") + 0x20080, [lst])
        CUR["sprite"] = target
        ans = s.call(F_SDTOR, [target, case["flag"] & MASK])
    res = {"ans": 1 if ans else 0, "list": list_state(s, pool)}
    for i, p in enumerate(pool):
        res["w0_%d" % i] = s.norm(s.rd_word(p), p, 0) if (i or not case["null"]) else None
        res["inst_%d" % i] = s.get_sprite(p) & MASK
        res["vis_%d" % i] = s.rd_word(p + s.vis_off)
    return res


def details(s, case, with_inner):
    det = s.alloc(0x60)
    inner = 0
    if not with_inner:   # SetVisibility reads the flags word only
        s.words(det, case["det"])
    else:
        # SetScale: the detail block is the sprite instance; the Mac's words 0 .. 0x27 sit at the same places on the host, the sprite
        # (Mac 0x28) is a native pointer there, so the scale (Mac 0x2c) and the words after it move (det_scale).
        s.words(det, case["det"][:10])
        s.words(det + s.det_scale, case["det"][11:])
        inner = s.alloc(0x20)
        s.words(inner, case["inner"])
        if s.is_host:
            s.write(det + s.det_sprite, struct.pack("<Q", inner))
        else:
            s.words(det + 0x28, [inner])
    if s.is_host:
        s.lib.tpw_isg_set_details(det)
    else:
        CUR["details"] = det
    return det, inner


def make_sprite(s, case):
    sp = s.alloc(0x80)
    s.put_sprite(sp, case["inst"])
    if s.is_host:
        s.write(sp + s.vis_off, struct.pack("<I", case["vis"] & MASK))
    else:
        s.words(sp + 0x2C, [case["vis"]])
    return sp


def op_setvis(s, case):
    sp = make_sprite(s, case)
    det, _ = details(s, case, False)
    if s.is_host:
        s.lib.tpw_isg_setvis(sp, s32(case["arg"]))
    else:
        s.call(F_SETVIS, [sp, case["arg"] & MASK])
    return {"vis": s.rd_word(sp + s.vis_off), "det": s.rd_words(det, 16)}


def op_setscale(s, case):
    sp = make_sprite(s, case)
    det, inner = details(s, case, True)
    s.write(inner + s.width_off, struct.pack(s.fmt[0] + "H", case["w"]))
    if s.is_host:
        s.lib.tpw_isg_setscale(sp, bitsf(case["scale"]))
    else:
        T.call_ppc(s.e, F_SETSCALE, [sp, T.F(bitsf(case["scale"]))])
    after = s.rd_words(det + s.det_scale, 5)
    d = s.rd_words(det, 10) + [0, canon_f(after[0])] + after[1:]
    return {"det": d}


def op_entry(s, case):
    if s.is_host:
        s.lib.tpw_isg_entry()
    else:
        s.call(F_ENTRY, [])
    return {}


def op_knob(s, case):
    same = case["same"]
    if s.is_host:
        a = s.alloc(0x400)
        b = a if same else s.alloc(0x400)
    else:
        a = s.alloc(0x200)
        b = a if same else s.alloc(0x200)
        vt = s.alloc(0x40)
        s.words(vt + 0x14, [s.alloc(8)])
        for o in {a, b}:
            s.words(o + 0x118, [vt])
    s.words(a + s.knob_off, [case["v0"]])
    if not same:
        s.words(b + s.knob_off, [case["v1"]])
    if s.is_host:
        ans = s.lib.tpw_isg_knob_sub(a, b)
    else:
        CUR["obj0"] = a
        ans = s32(s.call(F_KNOB, [a, b]))
    return {"ans": ans, "v0": s.rd_word(a + s.knob_off), "v1": s.rd_word(b + s.knob_off)}


OPS = {"ist": op_ist, "clear": op_grapher, "ctor": op_grapher, "gnsfd": op_gnsfd, "sdtor": op_sdtor, "setvis": op_setvis,
       "setscale": op_setscale, "entry": op_entry, "knob": op_knob}


def run(s, case):
    try:
        res = OPS[case["op"]](s, case)
        res["log"] = s.log()
        return res
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the cases

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


def rw(r):
    return r.getrandbits(32)


def rint(r):
    return r.choice([0, 1, -1, 2, 0x7FFFFFFF, -0x80000000, r.randrange(-100, 100), r.randrange(-(1 << 31), 1 << 31)]) & MASK


def ist_case(r, flag=None, iosoff=None):
    c = {"op": "ist", "queues": {"Widen": [r.randrange(256)]}}
    c["fill"] = [rw(r) | 0x80000000 for _ in range(128)]   # never an address of the block
    c["flag"] = r.choice([0, 1, 0xFFFF, 0x10000, 0x8000, 0x18000, r.getrandbits(32)]) if flag is None else flag
    c["iosoff"] = r.choice(range(0x10, 0x84, 4)) if iosoff is None else iosoff
    c["sb"] = rw(r)
    return c


def ist_edges():
    r = random.Random(11)
    for flag in (0, 1, 0xFFFF, 0x10000, 0x8000, 0x18000, 0xFFFFFFFF, 0x7FFF):
        for iosoff in (0x10, 0x4C, 0x80):
            yield ist_case(r, flag, iosoff)


def grapher_case(r, op):
    fill = []
    for i in range(24):
        fill.append(rint(r) if r.random() < 0.5 else rw(r))
    return {"op": op, "queues": {}, "fill": fill}


def grapher_edges(op):
    def gen():
        r = random.Random(12 if op == "clear" else 13)
        yield {"op": op, "queues": {}, "fill": [0] * 24}
        yield {"op": op, "queues": {}, "fill": [MASK] * 24}
        for _ in range(3):
            yield grapher_case(r, op)
    return gen


def gnsfd_case(r, n=None, prev=None, eq=None):
    n = r.randrange(0, 9) if n is None else n
    c = {"op": "gnsfd", "n": n}
    c["prev"] = (r.randrange(-1, n) if n else -1) if prev is None else prev
    c["queues"] = {"TPUniStrEq": [r.choice([0, 0, 1]) for _ in range(r.randrange(1, 10))] if eq is None else eq}
    return c


def gnsfd_edges():
    r = random.Random(14)
    yield gnsfd_case(r, 0, -1, [0])
    yield gnsfd_case(r, 0, -1, [1])
    yield gnsfd_case(r, 1, 0, [1])
    yield gnsfd_case(r, 1, -1, [0])
    yield gnsfd_case(r, 3, 1, [1])
    yield gnsfd_case(r, 3, 1, [0])
    yield gnsfd_case(r, 3, -1, [0, 0, 1])
    yield gnsfd_case(r, 3, 2, [0, 1])
    yield gnsfd_case(r, 3, 0, [0, 0, 0, 1])
    yield gnsfd_case(r, 8, 5, [0] * 7 + [1])
    yield gnsfd_case(r, 8, 5, [0, 0, 0, 0, 1])


def sdtor_case(r, null=None, flag=None, lst=None):
    c = {"op": "sdtor", "queues": {}}
    c["null"] = (r.random() < 0.1) if null is None else null
    c["flag"] = r.choice([0, 1, 0xFFFF, 0x10000, 0x8000, 0x18000, 0x10001, r.getrandbits(32), 5]) if flag is None else flag
    c["list"] = [r.randrange(0, 4) for _ in range(r.randrange(0, 6))] if lst is None else lst
    c["word0"] = [rw(r) for _ in range(4)]
    c["inst"] = [rw(r) for _ in range(4)]
    c["vis"] = [rint(r) for _ in range(4)]
    return c


def sdtor_edges():
    r = random.Random(15)
    yield sdtor_case(r, True, 0, [0, 1])
    yield sdtor_case(r, True, 1, [])
    yield sdtor_case(r, False, 0, [])
    yield sdtor_case(r, False, 1, [0])
    yield sdtor_case(r, False, 0, [0])
    yield sdtor_case(r, False, 0x10000, [1, 0, 2])
    yield sdtor_case(r, False, 0x18000, [1, 2, 0])
    yield sdtor_case(r, False, 1, [1, 2, 3])
    yield sdtor_case(r, False, 1, [0, 1, 0, 2])
    yield sdtor_case(r, False, 1, [3, 2, 0, 0])


def sprite_case(r, op):
    c = {"op": op, "queues": {}}
    c["inst"] = rw(r)
    c["vis"] = rint(r)
    c["det"] = [rw(r) for _ in range(16)]
    c["inner"] = [rw(r) for _ in range(8)]
    return c


def setvis_case(r, vis=None, arg=None):
    c = sprite_case(r, "setvis")
    if vis is not None:
        c["vis"] = vis
    c["arg"] = (c["vis"] if r.random() < 0.3 else rint(r)) if arg is None else arg
    return c


def setvis_edges():
    r = random.Random(16)
    for vis, arg in ((0, 0), (0, 1), (1, 0), (1, 1), (0, MASK), (MASK, 0), (5, 5), (0x100000000 - 2, 2)):
        for flags in (0, MASK, 1, 0xFFFFFFFE):
            c = setvis_case(r, vis & MASK, arg & MASK)
            c["det"][4] = flags
            yield c


def setscale_case(r, w=None, scale=None):
    c = sprite_case(r, "setscale")
    c["w"] = r.choice([0, 1, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16), r.randrange(1, 300)]) if w is None else w
    c["scale"] = rf(r) if scale is None else scale
    return c


def setscale_edges():
    r = random.Random(17)
    for w in (0, 1, 0x7FFF, 0x8000, 0xFFFF, 100):
        for scale in (0, 0x80000000, 0x3F800000, 0xBF800000, 0x7FC00000, 0x7F800000, 0x40490FDB):
            yield setscale_case(r, w, scale)


def entry_case(r):
    return {"op": "entry", "queues": {}}


def knob_case(r, same=None, k0=None, k1=None):
    c = {"op": "knob"}
    c["same"] = (r.random() < 0.1) if same is None else same
    k0 = r.randrange(0, 4) if k0 is None else k0
    k1 = r.choice([k0, k0, r.randrange(0, 4)]) if k1 is None else k1
    c["queues"] = {"WhatAmI:0": [k0], "WhatAmI:1": [k1]}
    c["v0"] = rint(r)
    c["v1"] = rint(r)
    return c


def knob_edges():
    r = random.Random(18)
    yield knob_case(r, False, 11, 11)
    yield knob_case(r, False, 11, 10)
    yield knob_case(r, True, 11, 11)
    yield knob_case(r, False, 0, 0)
    c = knob_case(r, False, 3, 3)
    c["v0"], c["v1"] = 0x7FFFFFFF, MASK
    yield c
    c = knob_case(r, False, 3, 3)
    c["v0"], c["v1"] = 0x80000000, 1
    yield c


TESTS = [
    FnTest("std::basic_istream<char,std::char_traits<char>>::basic_istream", ppc, host, edge=ist_edges, rnd=ist_case),
    FnTest("CUIGrapher::Clear", ppc, host, edge=grapher_edges("clear"), rnd=lambda r: grapher_case(r, "clear")),
    FnTest("CUIGrapher::CUIGrapher", ppc, host, edge=grapher_edges("ctor"), rnd=lambda r: grapher_case(r, "ctor")),
    FnTest("CFSLoadSave::GetNextSaveFileDetails", ppc, host, edge=gnsfd_edges, rnd=gnsfd_case),
    FnTest("CUI2DMapSprite::~CUI2DMapSprite", ppc, host, edge=sdtor_edges, rnd=sdtor_case),
    FnTest("CUI2DMapSprite::SetVisibility", ppc, host, edge=setvis_edges, rnd=setvis_case),
    FnTest("CUI2DMapSprite::SetScale", ppc, host, edge=setscale_edges, rnd=setscale_case),
    FnTest("entry", ppc, host, edge=lambda: iter([entry_case(None)]), rnd=entry_case),
    FnTest("InterfaceKnob::operator-", ppc, host, edge=knob_edges, rnd=knob_case),
]
