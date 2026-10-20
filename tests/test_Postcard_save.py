"""Unit v5906, Postcard::save, SetWorldBoundingArea and SetHeightfieldTexture, against libtpw_unit_Postcard_save.dylib
(tests/unit_Postcard_save.cpp), main app ("SimTheme Park"), src/Postcard_Postcard_save.cpp:
  Postcard::save 0x100c0c20, SetWorldBoundingArea 0x100b9180, SetHeightfieldTexture 0x100b8080

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the UPixels constructors, Convert, GetData and destructor, the libjpeg compressor calls, fopen, fclose) are fakes on both sides
that record the arguments the real ones read; the JPEG fakes keep the scan line counter (start sets it to 0, each write adds 1) so the
original's loop runs as the real library's would.

The world of a case: save: a card of w x h pixels (pointer, width and height set at the side's offsets), the pixel and GetData buffers, a
path string. SetWorldBoundingArea: two headers (box words at 0x80) and the node whose matrix the second one points at (0x78), possibly the
same header twice. SetHeightfieldTexture: a header with a height field (width, height, quads), the saved copy global (or NULL), the option
bits, and the texture table (entries, textures). What is compared: the answer, the event log, the boxes of both headers, the quads of the
height field and of the copy.

SetWorldBoundingArea uses fused multiply-adds: HardwareRounding is on for its call. Not tested: Postcard::save's failing fopen (fprintf and
exit; exit ends the process and cannot be compared); a negative category in SetHeightfieldTexture (reads before the table's entries); a
texture index past the entry's values.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_Postcard_save"
MASK = 0xFFFFFFFF

FN_SAVE, FN_BOUND, FN_TEX = 0x100C0C20, 0x100B9180, 0x100B8080
SIZES = {FN_SAVE: 480, FN_BOUND: 308, FN_TEX: 632}
OWN = set(SIZES)

ADDR = dict(copy=0x102C6C80, options=0x102B2B48)
# host layout: card w, h, pixels; entry stride, count, values; table entries; texture stride, index; header mHField, mF78, mBox;
# hfield mF18, mQuads; quad size
HOST_LAYOUT = [120, 124, 136, 24, 8, 16, 8, 12, 8, 0x6C, 0x78, 0x80, 0x18, 0x2C, 4]
MAC_LAYOUT = [0x50, 0x54, 0x5C, 12, 4, 8, 4, 12, 8, 0x6C, 0x78, 0x80, 0x18, 0x2C, 4]


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

ST = {}


def hsh(s):
    if isinstance(s, str):
        s = s.encode("latin-1")
    h = 0
    for c in s:
        h = (h * 31 + c) & MASK
    return h


def gl(e, name):
    return e.gl[name]


def which(a):
    return 2 if a == ST.get("up2") else (1 if a == ST.get("up1") else 0)


def st_up6(e):
    ST["up1"] = e.r(3)
    e.cl.rec("UPixels6", (s32(e.r(4)), int(e.r(5) == e.u32(gl(e, "cardpix"))), s32(e.r(6)), s32(e.r(7)), s32(e.r(8)), int(e.r(9) != 0)))


def st_up4(e):
    ST["up2"] = e.r(3)
    e.cl.rec("UPixels4", (s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), int(e.r(7) != 0)))


def st_convert(e):
    e.cl.rec("Convert", (int(e.r(3) == ST.get("up2")), int(e.r(4) == ST.get("up1")), e.r(5) & 0xFF))


def st_getdata(e):
    e.cl.rec("GetData", (int(e.r(3) == ST.get("up2")),))
    e.set_r(3, e.u32(gl(e, "pix")))


def st_dtor(e):
    e.cl.rec("dtor", (which(e.r(3)), s32(e.r(4))))


def st_stderr(e):
    e.cl.rec("StdError", ())
    e.set_r(3, 0x6000)


def st_create(e):
    e.cl.rec("Create", (e.u32(e.r(3)), s32(e.r(4)), e.r(5) & MASK))


def st_fopen(e):
    e.cl.rec("fopen", (hsh(e.cstr(e.r(3))), hsh(e.cstr(e.r(4)))))
    e.set_r(3, 0x7777)


def st_dest(e):
    e.cl.rec("stdio_dest", (e.r(4) & MASK,))


def st_defaults(e):
    c = e.r(3)
    e.cl.rec("set_defaults", (e.u32(c + 0x1C), e.u32(c + 0x20), s32(e.u32(c + 0x24)), s32(e.u32(c + 0x28))))


def st_quality(e):
    e.cl.rec("set_quality", (s32(e.r(4)), s32(e.r(5))))


def st_start(e):
    c = e.r(3)
    e.cl.rec("start", (s32(e.r(4)), e.u32(c + 0x1C), e.u32(c + 0x20), s32(e.u32(c + 0x24)), s32(e.u32(c + 0x28))))
    e.write(c + 0xDC, struct.pack(">I", 0))


def st_write(e):
    c = e.r(3)
    row = e.u32(e.r(4))
    e.cl.rec("write", ((row - e.u32(gl(e, "pix"))) & MASK, e.r(5) & MASK))
    e.write(c + 0xDC, struct.pack(">I", (e.u32(c + 0xDC) + 1) & MASK))
    e.set_r(3, 1)


def st_finish(e):
    e.cl.rec("finish", ())


def st_destroy(e):
    e.cl.rec("destroy", ())


def st_fclose(e):
    e.cl.rec("fclose", (e.r(3) & MASK,))
    e.set_r(3, 0)


for addr, fn in ((0x102390B8, st_up6), (0x102393B8, st_up4), (0x102390E8, st_convert), (0x102393D0, st_getdata), (0x10239100, st_dtor),
                 (0x10239400, st_stderr), (0x1023A090, st_create), (0x102389B0, st_fopen), (0x1023A0C0, st_dest),
                 (0x1023A0D8, st_defaults), (0x1023A0F0, st_quality), (0x1023A108, st_start), (0x1023A120, st_write),
                 (0x1023A138, st_finish), (0x102388C0, st_fclose), (0x1023A150, st_destroy)):
    STUBS.internal(addr, fn)


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

    def halves(self, a, ws):
        """Quads given as 32-bit values: the first half is the quad's field at +0, the second the field at +2 (halfwords per side)."""
        hs = []
        for w in ws:
            hs += [(w >> 16) & 0xFFFF, w & 0xFFFF]
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", v) for v in hs))

    def rd_halves(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def pw(self, a, p):
        """A native pointer field."""
        self.write(a, struct.pack(self.pfmt, p))

    def P(self, a):
        """A Ptr32 field's word for the address a (0 stays 0)."""
        return 0 if a == 0 else a - self.base

    def setup_layout(self, lay):
        (self.card_w, self.card_h, self.card_pix, self.e_stride, self.e_cnt, self.e_val, self.t_entries, self.x_stride,
         self.x_idx) = lay[:9]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    base = 0

    def __init__(self, e, case):
        self.e = e
        e.gl = {}
        ST.clear()
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.setup_layout(MAC_LAYOUT)
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name in ADDR:
            return ADDR[name]
        if name not in self.e.gl:
            self.e.gl[name] = self.e.alloc(8)
        return self.e.gl[name]

    def alloc(self, n):
        return self.e.alloc(max(n, 8))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def save(self, card, path):
        r3, _, _ = T.call_ppc(self.e, FN_SAVE, [card, path])
        return r3 & MASK

    def bound(self, a, b):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN_BOUND, [a, b])
        return 0

    def tex(self, header, table, x, y, cat, tex, rot):
        T.call_ppc(self.e, FN_TEX, [header, table, x & MASK, y & MASK, cat & MASK, tex & MASK, rot & MASK])
        return 0


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.setup_layout(lib.lay)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    @property
    def base(self):
        return self.lib.tpw_pc_base()   # (the arena exists after its first allocation)

    def g(self, name):
        return self.lib.tpw_pc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_pc_alloc(max(n, 8))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def save(self, card, path):
        return self.lib.tpw_pc_save(card, path) & MASK

    def bound(self, a, b):
        self.lib.tpw_pc_bound(a, b)
        return 0

    def tex(self, header, table, x, y, cat, tex, rot):
        self.lib.tpw_pc_tex(header, table, s32(x), s32(y), s32(cat), s32(tex), s32(rot))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_pc_addr.restype, lib.tpw_pc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_pc_alloc.restype, lib.tpw_pc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pc_base.restype, lib.tpw_pc_base.argtypes = C.c_uint64, []
    lib.tpw_pc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pc_save.restype, lib.tpw_pc_save.argtypes = C.c_int32, [C.c_uint64, C.c_uint64]
    lib.tpw_pc_bound.restype, lib.tpw_pc_bound.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_pc_tex.restype = None
    lib.tpw_pc_tex.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_pc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.lay = list(out)[:n]
    lib.base_ = lib.tpw_pc_base()
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the worlds


def build_save(s, case):
    w, h = case["w"], case["h"]
    card = s.alloc(0x100)
    pix = s.alloc(w * h * 4)
    buf = s.alloc(w * h * 3)
    path = s.alloc(len(case["path"]) + 1)
    s.write(path, case["path"] + b"\0")
    s.words(card + s.card_w, [w])
    s.words(card + s.card_h, [h])
    s.pw(card + s.card_pix, pix)
    s.words(s.g("cardpix"), [pix])
    s.words(s.g("pix"), [buf])
    return card, path


def build_bound(s, case):
    nodebytes = b"".join(struct.pack(s.fmt, v) for v in case["node"])
    node = s.alloc(len(nodebytes))
    s.write(node, nodebytes)
    ha = s.alloc(0xB0)
    hb = ha if case["same"] else s.alloc(0xB0)
    for h, words in ((ha, case["boxA"]), (hb, case["boxB"])):
        s.words(h + 0x80, words)
        s.words(h + 0x78, [s.P(node)])
    if not case["same"]:
        # a's own node pointer differs from b's, to show only b's is used
        other = s.alloc(len(nodebytes))
        s.write(other, bytes(len(nodebytes)))
        s.words(ha + 0x78, [s.P(other)])
    return ha, hb


def build_tex(s, case):
    w, h = case["w"], case["h"]
    nq = max(w * h, 1)
    quads = s.alloc(nq * 4)
    s.halves(quads, case["quads"])
    hf = s.alloc(0x40)
    s.words(hf + 0x18, [w, h])
    s.words(hf + 0x2C, [s.P(quads)])
    header = s.alloc(0xB0)
    s.words(header + 0x6C, [s.P(hf)])
    cquads = s.alloc(nq * 4)
    s.halves(cquads, case["cquads"])
    chf = s.alloc(0x40)
    s.words(chf + 0x18, [w, h])
    s.words(chf + 0x2C, [s.P(cquads)])
    s.pw(s.g("copy"), chf if case["has_copy"] else 0)
    s.words(s.g("options"), [case["options"]])
    entries_n = len(case["entries"])
    table = s.alloc(0x30)
    s.words(table, [case["tcount"]])
    ents = s.alloc(entries_n * s.e_stride + 8)
    s.pw(table + s.t_entries, ents)
    for i, (count, tex_idx) in enumerate(case["entries"]):
        vals = s.alloc(70 * s.x_stride)
        vals += 4 * s.x_stride
        for j, ti in enumerate(tex_idx):
            s.words(vals + j * s.x_stride + 0, [j])
            s.write(vals + j * s.x_stride + s.x_idx, struct.pack(s.fmt[0] + "H", ti))
        s.words(ents + i * s.e_stride + s.e_cnt, [count])
        s.pw(ents + i * s.e_stride + s.e_val, vals)
    return header, table, hf, chf, quads, cquads, nq


def observe_tex(s, case, b):
    header, table, hf, chf, quads, cquads, nq = b
    return {"quads": s.rd_halves(quads, 2 * nq), "cquads": s.rd_halves(cquads, 2 * nq), "log": s.log()}


def run_save(s, case):
    try:
        card, path = build_save(s, case)
        return {"answer": s.save(card, path), "log": s.log()}
    finally:
        s.done()


def run_bound(s, case):
    try:
        ha, hb = build_bound(s, case)
        s.bound(ha, hb)
        return {"a": [canon_f(v) for v in s.rd_words(ha + 0x80, 6)], "b": [canon_f(v) for v in s.rd_words(hb + 0x80, 6)],
                "log": s.log()}
    finally:
        s.done()


def run_tex(s, case):
    try:
        b = build_tex(s, case)
        s.tex(b[0], b[1], *case["args"])
        return observe_tex(s, case, b)
    finally:
        s.done()


def mk(run):
    return (lambda e, case: run(Ppc(e, case), case)), (lambda lib, case: run(Host(lib, case), case))


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


# ---- save
PATHS = [b"a.jpg", b"", b"data:postcard:out.jpg", b"x" * 40]


def save_case(r, w=None, h=None, path=None):
    return {"queues": {}, "w": r.randrange(0, 6) if w is None else w, "h": r.randrange(0, 6) if h is None else h,
            "path": r.choice(PATHS) if path is None else path}


def save_edges():
    r = random.Random(11)
    for w, h in ((0, 0), (1, 1), (0, 3), (3, 0), (1, 5), (5, 1), (4, 4), (640 // 128, 3)):
        yield save_case(r, w, h)
    for p in PATHS:
        yield save_case(r, 2, 2, p)


def save_random(r):
    return save_case(r)


# ---- SetWorldBoundingArea
def bound_case(r, same=None):
    node = [rf(r) for _ in range(24)]
    return {"queues": {}, "same": r.random() < 0.2 if same is None else same, "node": node,
            "boxA": [rf(r) for _ in range(6)], "boxB": [rf(r) for _ in range(6)]}


def bound_edges():
    r = random.Random(12)
    for same in (False, True, False, True):
        yield bound_case(r, same)
    c = bound_case(r, False)
    c["node"] = [fb(1.0) if i in (0, 5, 10) else 0 for i in range(24)]
    yield c
    c = bound_case(r, False)
    c["node"] = [SPECIAL[i % len(SPECIAL)] for i in range(24)]
    yield c


def bound_random(r):
    return bound_case(r)


# ---- SetHeightfieldTexture
def tex_case(r, args=None, ents=None, tcount=None):
    w = r.randrange(0, 5)
    h = r.randrange(0, 5)
    nq = max(w * h, 1)
    nE = r.randrange(0, 4) if tcount is None else tcount
    entries = ents
    if entries is None:
        entries = []
        for _ in range(nE + 3):
            cnt = r.choice([0, 1, 2, 3, 4, 4])
            entries.append((cnt, [r.randrange(0, 0x10000) for _ in range(64)]))
    if args is None:
        cat = r.choice([0, 0, 1, 2, 3, nE + 1, nE + 2, r.randrange(0, nE + 4)])
        texture = r.choice([8, 0x37, r.randrange(-3, 51), r.randrange(0, 5)])
        if cat != 0 or texture not in (8, 0x37):
            texture = r.choice([texture, r.randrange(0, 5)])
        args = [r.randrange(-1, w + 2), r.randrange(-1, h + 2), cat, texture, r.choice([0, 0x5A, 0xB4, 0x10E, 0x5B, r.randrange(0, 400)])]
    return {"queues": {}, "w": w, "h": h, "args": args, "entries": entries, "tcount": nE if tcount is None else tcount,
            "has_copy": r.random() < 0.7, "options": r.choice([0, 0x4000, 0xFFFFFFFF, r.randrange(0, 1 << 32)]),
            "quads": [r.randrange(0, 1 << 32) for _ in range(nq)], "cquads": [r.randrange(0, 1 << 32) for _ in range(nq)]}


def tex_edges():
    r = random.Random(13)
    for cat, texture in ((0, 8), (0, 0x37), (0, 0), (1, 0), (2, 3), (3, 7), (5, 2), (0, 9), (1, -2)):
        for rot in (0, 0x5A, 0xB4, 0x10E):
            for _ in range(2):
                c = tex_case(r, tcount=r.randrange(1, 4))
                c["w"], c["h"] = 3, 3
                c["quads"] = [r.randrange(0, 1 << 32) for _ in range(9)]
                c["cquads"] = [r.randrange(0, 1 << 32) for _ in range(9)]
                c["args"] = [r.randrange(0, 3), r.randrange(0, 3), cat, texture, rot]
                yield c
    for x, y in ((-1, 0), (0, -1), (3, 0), (0, 3), (2, 2)):
        c = tex_case(r, tcount=2)
        c["w"], c["h"] = 3, 3
        c["quads"] = [r.randrange(0, 1 << 32) for _ in range(9)]
        c["cquads"] = [r.randrange(0, 1 << 32) for _ in range(9)]
        c["args"] = [x, y, 1, 1, 90]
        yield c
        c2 = dict(c)
        c2["args"] = [x, y, 0, 8, 0]
        yield c2
    c = tex_case(r, tcount=0)
    c["args"] = [0, 0, 1, 1, 0]
    yield c
    c = tex_case(r, tcount=2)
    c["entries"] = [(0, [1] * 64), (3, [2] * 64), (3, [4] * 64), (2, [5] * 64), (0, [1] * 64)]
    c["w"], c["h"] = 3, 3
    c["quads"] = [r.randrange(0, 1 << 32) for _ in range(9)]
    c["cquads"] = [r.randrange(0, 1 << 32) for _ in range(9)]
    c["args"] = [1, 1, 2, 5, 180]
    yield c
    c["args"] = [1, 1, 3, 5, 180]
    yield c


def tex_random(r):
    return tex_case(r)


_ppc_save, _host_save = mk(run_save)
_ppc_bound, _host_bound = mk(run_bound)
_ppc_tex, _host_tex = mk(run_tex)

TESTS = [
    FnTest("Postcard::save", _ppc_save, _host_save, edge=save_edges, rnd=save_random),
    FnTest("SetWorldBoundingArea", _ppc_bound, _host_bound, edge=bound_edges, rnd=bound_random),
    FnTest("SetHeightfieldTexture", _ppc_tex, _host_tex, edge=tex_edges, rnd=tex_random),
]
