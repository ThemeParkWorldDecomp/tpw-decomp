"""The lobby frame's last engine stand-ins (engine library, src/tb_opengl_texture_download.cpp,
src/tb_texturecache_helpers.cpp, src/tb_particle_frame.cpp and src/tb_rain_frame.cpp), original
versus translation: COpenGLTexture::Download, COpenGLDrawPrimitive's destructor,
CTextureCache::Entry_Specific_Compress and BalanceCache_ClearTexture, CParticle::Prepare and
PlotParticles, CRain::Prepare, Render and RenderLightning2.

Each case builds its world twice, in emulator memory and through tests/unit_FrameStandins.cpp, from one
description, runs the function on both and compares what comes out: the answer, the recorded calls of
the fakes (name, integers, floats by their bits, a NaN as one value) and the state of everything the call
could change. Everything outside the functions is a fake on both sides that records its arguments: the
entry's GetData, StoreAsWrapping, FlushEntryFlags and GetBlendingMode, the compressed image's
IsNaturallyFlipped, the texture's SetFlags, the cache's Prepare, the clipper's CheckBoundingBox, the matrix's
TransformVertex, RotateVertex_Y, the draw primitive's GetNextPrimitive, ProjectCurrentPrimitive,
ExecuteCurrentPrimitive and DrawSprite, the rain's drawing members, the game's clock and first-person hooks,
the particle system's four hooks, the base destructor, operator delete and the assertion handler.

Download. A case is one call on a page (bytes per pixel and pitch), an entry (flags, size, room, image row
length), its image (16-bit pixels, up to 300 x 300) and the shared upload buffer (256 x 256 pixels, filled
with a pattern so that clearing shows), at a place (x, y) of the page. The upload buffer, the entry's flags
and the log are compared. The original writes each pixel as one big-endian word and the translation as four
bytes, so the buffer holds the same bytes.

The destructor. The original destructor restores the vtable pointers, runs the base destructor when its flag
is not zero and frees the block when the flag is positive. The host compiler does the same for a plain
destructor; flag 0 (a base object's destructor) has no host equivalent and is checked to do nothing.

The cache helpers. A case is one call on a small world (a cache, six entries, two pages, a list of entries)
described as a flat list. The fakes are StoreAsWrapping (answers from a queue, in call order),
FlushEntryFlags (takes 0x7000 off the flags), CTextureBase::SetFlags and the assertion.

The particles. A case is one call on a particle object whose four hooks answer from a world of emitters
(counts, boxes, lists of records) and four texture entries; CheckBoundingBox and DrawSprite answer from
queues. TransformVertex moves its input words round and RotateVertex_Y turns the corners by a quarter turn
chosen from the angle's bits, so a corner shows what happened to it; DrawSprite dumps its record and the
four corners.

The rain. A case is one call on a weather object (every word compared, a texture pointer as its entry's
number) with its view matrices, the system flags, the clock and first-person hooks and the drawing members'
answers from queues.

Unicorn computes the fused multiply-adds in double precision and rounds twice, and gives fctiwz of a value
above INT32_MAX as 0x80000000 where the processor saturates (FINDINGS 2026-10-10 and 2026-10-11): the code
hook HardwareRounding corrects both for the functions that use them (Entry_Specific_Compress's sort key and
Render's camera). NaNs are one value wherever a float is dumped, mixed or compared.

Not tested: Download with a width, height or room below zero (the original runs a loop about four billion
times), a page that was never created (bytes per pixel 0: the PowerPC leaves the quotient undefined), a
place outside the upload buffer, and an image the copy reads outside; Entry_Specific_Compress with a
level above 12 (it reads other data) and the assertion that cannot fire.
"""
import contextlib
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_FrameStandins"
MASK = 0xFFFFFFFF
NAN, INF = math.nan, math.inf

# ---- the functions under test
DOWNLOAD, DTOR = 0x100465F8, 0x10044EE8
ESC, CLEAR = 0x10034E94, 0x1003770C
P_PREPARE, P_PLOT = 0x1001F4AC, 0x1001F754
R_PREPARE, R_RENDER, R_LIGHTNING2 = 0x10021554, 0x10023F40, 0x1002294C
FUNCTIONS = {DOWNLOAD, DTOR, ESC, CLEAR, P_PREPARE, P_PLOT, R_PREPARE, R_RENDER, R_LIGHTNING2}
# The OpenGL page's constructor, __ptr_glue and the double -> unsigned routine run as the original.
PAGE_CTOR, PTR_GLUE, CVT_UNSIGNED = 0x10045D88, 0x1004C858, 0x1004B9FC
# ---- callees that are fakes
GET_DATA, STORE_WRAP, FLUSH_FLAGS, GET_BLEND = 0x10041274, 0x10041848, 0x1004168C, 0x100410A4
SET_FLAGS, TCACHE_PREPARE, CHECK_BOX = 0x10032BD0, 0x10032DDC, 0x1003F4D4
TRANSFORM_VERTEX, ROTATE_VERTEX, DRAW_SPRITE = 0x1001C584, 0x10038CD0, 0x10007EDC
GET_NEXT, PROJECT_CURRENT, EXECUTE_CURRENT = 0x10006A58, 0x10007DF4, 0x10007360
RENDER_LIGHTNING, RENDER_RAIN, RENDER_SNOW = 0x10022C10, 0x10022EFC, 0x100233F8
NEW_LIGHTNING, ADD_SECTION = 0x100223D8, 0x1002163C
BASE_DTOR, OPERATOR_DELETE = 0x100069E0, 0x1004BC88
# ---- engine globals
DISPLAY, SYSTEM = 0x10165634, 0x10165F80
TCACHE_VAR, CLIP_VAR, FINAL_VAR, VIEW_VAR, INVVIEW_VAR = 0x10173184, 0x102B3638, 0x100AFF70, 0x100B0078, 0x100AFFF4
DRAW_VAR, CLOCK_VAR, FIRST_VAR, UPLOAD_VAR = 0x10052B3C, 0x101735C8, 0x101735CC, 0x102B3648
GUARDED = [(DISPLAY, 0x88), (SYSTEM, 4), (TCACHE_VAR, 4), (CLIP_VAR, 4), (FINAL_VAR, 4), (VIEW_VAR, 4), (INVVIEW_VAR, 4),
           (DRAW_VAR, 4), (CLOCK_VAR, 8), (UPLOAD_VAR, 4)]

UPLOAD_BYTES = 256 * 256 * 4
IMAGE_WORDS = 1 << 17
EMITTERS = 0x78
RAIN_BYTES = 0xA1CC
PAGE_BASE = 0x7D100000       # a page marker's address on the PowerPC side (never dereferenced)

# Entry flags.
ON_PAGE, QUEUED, LOCKED, FLIPPED, BLEND_KIND = 0x1000, 0x2000, 0x4000, 0x40000, 0x80000
COMPRESSED, ASKED, CENTRED = 0x10000000, 0x40000000, 0x80000000
PAGE_BITS = 0x7000


def fb(x):
    """A float's bits, as the processor holds it (any NaN is the same bits)."""
    return T.f32_bits(T.f32(x)) if x == x else 0x7FC00000


def canon(w):
    """Any NaN as one value."""
    return 0x7FC00000 if w & 0x7F800000 == 0x7F800000 and w & 0x007FFFFF else w & MASK


def cfb(x):
    """Canonical bits of a float register's value."""
    return 0x7FC00000 if math.isnan(x) else struct.unpack(">I", struct.pack(">f", x))[0]


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def ws(e, a, values):
    e.write(a, struct.pack(">%dI" % len(values), *[v & MASK for v in values]))


def rs(e, a, n):
    return list(struct.unpack(">%dI" % n, e.read(a, 4 * n)))


def signed64(v):
    v &= 0xFFFFFFFFFFFFFFFF
    return v - (1 << 64) if v >= (1 << 63) else v


def i64(values):
    values = [signed64(v) for v in values]
    return (C.c_int64 * max(len(values), 1))(*values)


# ----------------------------------------------------------------- the emulator's float corrections

LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Two things Unicorn (QEMU 5.0) does not execute as the processor does, corrected for the code under
    test and nowhere else (as in test_OpenGLDraw.py and test_FrameRest.py):
    - fmadds, fmsubs, fnmadds and fnmsubs are computed in double precision and rounded to single, which can
      differ by one unit in the last place from the processor's single rounding of the exact result; the
      result is replaced by the C library's fmaf;
    - fctiwz of a value above INT32_MAX (or +infinity) gives 0x80000000; the processor saturates to
      0x7fffffff.
    A code hook looks at each instruction before it runs and rewrites its target register just before the
    next one. ranges: the (begin, end) address ranges of the functions under test."""

    changed = 0    # results the correction replaced (all cases so far), for the notes

    def __init__(self, e, ranges):
        self.e, self.pending, self.hooks, self.ranges = e, None, [], ranges

    def __enter__(self):
        for begin, end in self.ranges:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            kind, target, value = self.pending
            self.pending = None
            if kind == "fctiwz":
                HardwareRounding.changed += 1
                uc.reg_write(ppcemu.FPR[target], (fpr(e, target) & 0xFFFFFFFF00000000) | 0x7FFFFFFF)
            else:
                corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
                if fpr(e, target) != corrected and not (value != value and fpr_double(e, target) != fpr_double(e, target)):
                    HardwareRounding.changed += 1
                uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        if word >> 26 == 63 and (word >> 1) & 0x3FF == 15:          # fctiwz frD, frB
            if fpr_double(e, rb) >= 2147483648.0:
                self.pending = ("fctiwz", rd, None)
            return
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        for x in (a, c, b):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        subtract = kind in (28, 30)
        value = float(LIBM.fmaf(a, c, -b if subtract else b))
        if kind in (30, 31):
            value = -value
        self.pending = ("fused", rd, value)


# ----------------------------------------------------------------- the PowerPC side: common


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if hasattr(e, "fs_hooks"):
        return
    # The fake object whose vtable slot 0x18 answers IsNaturallyFlipped.
    flipped = T.fake_function(e, lambda em: em.fs_flipped(em))
    e.fs_vtable = T.persist_alloc(e, 0x40)
    e.write(e.fs_vtable + 0x18, struct.pack(">I", flipped))
    # The particle system's hooks and the game's clock and first-person hooks (function descriptors).
    e.fs_hooks = {name: T.fake_function(e, (lambda n: lambda em: getattr(em.fs_side, n)(em))(name)) for name in (
        "hook_count", "hook_box", "hook_build", "hook_free", "hook_clock", "hook_first_person")}
    e.fs_side = None
    e.fs_flipped = None


@contextlib.contextmanager
def guarded(e):
    """The engine globals a case overwrites, put back afterwards (other modules share them)."""
    saved = [(a, e.read(a, n)) for a, n in GUARDED]
    try:
        yield
    finally:
        for a, data in saved:
            e.write(a, data)


def recorded(e, case, fixed):
    """A fresh call log with the case's queues and the given labels (address -> label)."""
    e.cl = CL.PpcLog(fixed, case.get("q"))
    return e.cl


def u32_array(values):
    return (C.c_uint32 * max(len(values), 1))(*[v & MASK for v in values])


# ================================================================= Download

BYTES_PER_PIXEL = 4


def tiled(block, size=UPLOAD_BYTES):
    return (block * (size // len(block) + 1))[:size]


def download_ppc(e, case):
    with guarded(e):
        L = recorded(e, case, {})
        page = e.alloc(0x34, b"\xa5" * 0x34)
        T.call_ppc(e, PAGE_CTOR, [page, 1])                  # the original's constructor makes the vtables
        entry = e.alloc(0x5C, b"\xa5" * 0x5C)
        comp = e.alloc(16, struct.pack(">I", e.fs_vtable) + b"\xa5" * 12)
        image = case["image"]
        image_address = e.alloc(2 * len(image) + 16, struct.pack(">%dH" % len(image), *image))
        buffer = e.alloc(UPLOAD_BYTES, case["buffer"])
        w32(e, page + 0x10, case["bpp"])
        w32(e, page + 0x1C + 0xC, case["pitch"])
        w32(e, entry + 8, case["flags"])
        e.write(entry + 0xC, struct.pack(">hhhh", T.s16(case["w"]), T.s16(case["h"]), T.s16(case["cw"]), T.s16(case["ch"])))
        e.write(entry + 0x30, struct.pack(">h", T.s16(case["stride"])))
        w32(e, entry + 0x3C, comp if case["flags"] & COMPRESSED else image_address)
        w32(e, UPLOAD_VAR, buffer)
        L.labels.update({entry: 1, page: 2, comp: 3})

        def get_data(em):
            L.rec("GetData", (L.lab(em.r(3)),))
            em.set_r(3, image_address)

        def flipped(em):
            L.rec("IsNaturallyFlipped", (L.lab(em.r(3)),))
            em.set_r(3, T.u32(L.ret("IsNaturallyFlipped", 0)))

        st = CL.Stubs()
        st.real = FUNCTIONS | {PAGE_CTOR, PTR_GLUE}
        st.internal(GET_DATA, get_data)
        saved = st.activate(e)
        e.fs_flipped = flipped
        try:
            r3 = T.call_ppc(e, DOWNLOAD, [page, entry, case["x"], case["y"]])[0]
        finally:
            CL.Stubs.deactivate(e, saved)
        return dict(result=T.s32(r3), flags=e.u32(entry + 8), buffer=e.read(buffer, UPLOAD_BYTES), log=L.log)


def download_host(lib, case):
    lib.tpw_fs_download.restype = C.c_int64
    lib.tpw_fs_download.argtypes = [C.POINTER(C.c_int64), C.c_void_p, C.c_int32, C.c_void_p, C.POINTER(C.c_int64)]
    a = i64([case["bpp"], case["pitch"], case["flags"], case["w"], case["h"], case["cw"], case["ch"], case["stride"],
             case["x"], case["y"]])
    image = (C.c_uint16 * len(case["image"]))(*case["image"])
    buffer = C.create_string_buffer(case["buffer"], UPLOAD_BYTES)
    out = (C.c_int64 * 4)()
    CL.host_begin(lib, {}, case.get("q"))
    result = lib.tpw_fs_download(a, image, len(case["image"]), buffer, out)
    log = CL.host_end(lib)
    return dict(result=C.c_int32(result).value, flags=out[0] & MASK, buffer=bytes(buffer.raw), log=log)


def download_case(r, w, h, cw, ch, flags=0, flip=0, stride=None, x=0, y=0, page=256, bpp=BYTES_PER_PIXEL, pitch=None,
                  buffer=None):
    """One Download case: an image of w x h pixels into a room of cw x ch at (x, y) of a page `page` pixels wide."""
    stride = w if stride is None else stride
    pitch = page * bpp if pitch is None else pitch
    rows = max(h, 1) + 2
    words = min(IMAGE_WORDS - 16, rows * max(stride, w, 1) + max(w, 1) + 64)
    return dict(bpp=bpp, pitch=pitch, flags=flags & MASK, w=w, h=h, cw=cw, ch=ch, stride=stride, x=x, y=y, flip=flip,
                image=[r.getrandbits(16) for _ in range(words)], q={"IsNaturallyFlipped": [flip]},
                buffer=tiled(r.randbytes(4093)) if buffer is None else buffer)


# Each family of sizes (w, h, room width, room height).
def edge_download():
    r = random.Random(201)
    for size in (1, 2, 3, 4, 7, 8, 16, 32, 64):                    # an exact fit, every way up
        for flags, flip in ((0, 0), (FLIPPED, 0), (COMPRESSED, 0), (COMPRESSED, 1), (COMPRESSED | FLIPPED, 0),
                            (COMPRESSED | FLIPPED, 1)):
            yield download_case(r, size, size, size, size, flags, flip, x=size % 5, y=size % 7)
    yield download_case(r, 256, 256, 256, 256)                    # the whole page
    yield download_case(r, 256, 256, 256, 256, FLIPPED)
    for w, h, cw, ch in ((3, 4, 8, 8), (4, 3, 8, 8), (7, 7, 8, 8), (1, 1, 8, 8), (5, 8, 8, 8), (8, 5, 8, 8), (6, 9, 16, 16),
                         (30, 20, 32, 32), (31, 63, 64, 64), (100, 128, 128, 128), (127, 127, 128, 128), (2, 3, 4, 4)):
        for flags in (0, FLIPPED):                                 # centred: narrower, shorter, both, odd and even gaps
            yield download_case(r, w, h, cw, ch, flags, x=r.randrange(0, 5), y=r.randrange(0, 5))
    for w, h, cw, ch in ((16, 16, 8, 8), (17, 16, 8, 8), (16, 17, 8, 8), (24, 24, 16, 16), (10, 6, 7, 4), (64, 64, 32, 32),
                         (100, 100, 64, 64), (33, 33, 32, 32), (9, 9, 8, 8), (200, 150, 128, 64), (256, 256, 128, 128)):
        for flags in (0, FLIPPED):                                 # larger than the room: point-sampled
            yield download_case(r, w, h, cw, ch, flags, x=1, y=2)
    for w, h, cw, ch in ((16, 4, 8, 8), (4, 16, 8, 8), (16, 3, 8, 8), (3, 16, 8, 8), (20, 5, 8, 16), (5, 20, 16, 8),
                         (9, 2, 8, 8), (2, 9, 8, 8)):              # one way larger, the other smaller
        for flags in (0, FLIPPED):
            yield download_case(r, w, h, cw, ch, flags, x=2, y=1)
    for w, h, cw, ch in ((0, 8, 8, 8), (8, 0, 8, 8), (0, 0, 8, 8), (8, 8, 0, 8), (8, 8, 8, 0), (8, 8, 0, 0), (0, 0, 0, 0),
                         (0, 5, 0, 5), (5, 0, 5, 0), (5, 5, 0, 8), (9, 9, 0, 8), (9, 9, 8, 0)):
        yield download_case(r, w, h, cw, ch)                      # empty images and rooms
    for stride in (0, 1, 3, 8, 9, 12, 40, 300):                    # image rows of other lengths
        for flags in (0, FLIPPED):
            yield download_case(r, 8, 8, 8, 8, flags, stride=stride)
            yield download_case(r, 6, 5, 8, 8, flags, stride=stride)
            yield download_case(r, 16, 12, 8, 8, flags, stride=stride)
    for bpp, pitch in ((1, 64), (2, 128), (3, 100), (4, 100), (5, 250), (8, 512), (4, 1022), (4, 1030), (2, 1)):
        yield download_case(r, 8, 8, 8, 8, bpp=bpp, pitch=pitch, x=3, y=2)
        yield download_case(r, 5, 6, 8, 8, bpp=bpp, pitch=pitch, x=1, y=1)
    for page in (8, 16, 32, 64, 128, 256):                         # places on a page
        for x, y in ((0, 0), (page - 8, page - 8), (page // 2, 0), (0, page // 2)):
            if x >= 0 and y >= 0 and page >= 8:
                yield download_case(r, 8, 8, 8, 8, page=page, x=x, y=y)
                yield download_case(r, 5, 5, 8, 8, page=page, x=x, y=y)
    for flags in (0xFFFFFFFF & ~(COMPRESSED | FLIPPED), CENTRED, CENTRED | FLIPPED, 0x12345678 & ~(COMPRESSED | FLIPPED),
                  COMPRESSED | 0x0FFFFFFF, COMPRESSED | CENTRED | FLIPPED, 0xFFFFFFFF):
        yield download_case(r, 5, 6, 8, 8, flags, flip=1)          # other bits of the flags do nothing
        yield download_case(r, 8, 8, 8, 8, flags, flip=1)
    pattern = bytes(range(251, 255)) * 1024                        # a pattern the clear shows against
    yield download_case(r, 3, 3, 8, 8, buffer=tiled(pattern))
    yield download_case(r, 8, 8, 8, 8, buffer=b"\xff" * UPLOAD_BYTES)
    yield download_case(r, 8, 8, 8, 8, buffer=b"\x00" * UPLOAD_BYTES)


def random_download(r):
    page = r.choice([8, 16, 32, 64, 128, 256, 256])
    bpp = 4 if r.random() < 0.9 else r.choice([1, 2, 3, 5, 8])
    pitch = page * bpp if r.random() < 0.9 else r.choice([page * bpp + 4, page * bpp - 4, page * 4, 100, 1022])
    stride_words = max(pitch // bpp, 1)
    side = min(stride_words, r.choice([8, 16, 32, 64, 128, 256]))
    cw = side if r.random() < 0.7 else r.randint(1, side)
    ch = side if r.random() < 0.7 else r.randint(1, side)
    k = r.random()
    if k < 0.35:
        w, h = cw, ch
    elif k < 0.55:
        w, h = r.randint(0, cw), r.randint(0, ch)
    elif k < 0.8:
        w, h = r.randint(cw, cw * 2 + 5), r.randint(ch, ch * 2 + 5)
    else:
        w, h = r.randint(0, cw * 2 + 3), r.randint(0, ch * 2 + 3)
    w, h = min(w, 300), min(h, 300)
    flags = r.choice([0, FLIPPED, COMPRESSED, COMPRESSED | FLIPPED]) | (r.getrandbits(32) & ~(COMPRESSED | FLIPPED) & r.getrandbits(32))
    stride = w if r.random() < 0.6 else r.choice([w + r.randint(0, 9), w + 64, max(w - r.randint(0, 3), 0), r.randint(0, 300)])
    # The place: the room inside the page (the buffer is 256 pixels square at four bytes a pixel).
    limit_x = max(min(pitch // bpp, 256) - cw, 0)
    limit_y = max(min(256, (UPLOAD_BYTES // max(pitch, 1))) - ch - 1, 0)
    x = r.randint(0, limit_x) if limit_x else 0
    y = r.randint(0, limit_y) if limit_y else 0
    # The writes must stay inside the buffer.
    last = y * pitch + x * bpp + (max(ch, 1) + 1) * (pitch // max(bpp, 1)) * 4 + 4 * max(cw, 1)
    if last >= UPLOAD_BYTES - 64:
        x, y, bpp, pitch = 0, 0, 4, 1024
        cw, ch = min(cw, 64), min(ch, 64)
    return download_case(r, w, h, cw, ch, flags, flip=r.randrange(2), stride=stride, x=x, y=y, page=page, bpp=bpp, pitch=pitch)


# ================================================================= the destructor


def destroy_ppc(e, case):
    with guarded(e):
        L = recorded(e, case, {})
        if case["mode"] == 3:
            this = 0
        else:
            this = e.alloc(0x4C, b"\xa5" * 0x4C)
            e.write(this, struct.pack(">IIIII", this + 0x18, 0x10051E30, 0xFFFFFFFF, 0, 0))
            w32(e, this + 0x18 + 0x2C, 0x100522F0)
            w32(e, this + 0x18 + 0x30, 0)
            L.labels.update({this: 1, this + 0x18: 2})
        before = e.read(this, 0x4C) if this else b""

        def base_dtor(em):
            L.rec("BaseDtor", (L.lab(em.r(3)), T.s16(em.r(4))))

        def delete(em):
            a = em.r(3)
            if L.lab(a) > 0:
                L.rec("delete", (L.lab(a),))
                L.forget(a)

        st = CL.Stubs()
        st.real = FUNCTIONS
        st.internal(BASE_DTOR, base_dtor)
        st.internal(OPERATOR_DELETE, delete)
        saved = st.activate(e)
        try:
            r3 = T.call_ppc(e, DTOR, [this, case["flag"]])[0]
        finally:
            CL.Stubs.deactivate(e, saved)
        after = e.read(this, 0x4C) if this else b""
        # What the destructor rewrites (the object's vtable pointer, the base's vtable pointer and its offset) is
        # left out; the rest of the object must be as it was.
        keep = lambda data: data[:4] + data[8:0x44] + data[0x4C:] if data else b""
        return dict(returned=int(r3 == this), rest=keep(after) == keep(before), log=L.log)


def destroy_host(lib, case):
    lib.tpw_fs_destroy.argtypes = [C.c_int32]
    CL.host_begin(lib, {}, case.get("q"))
    lib.tpw_fs_destroy(case["mode"])
    return dict(returned=1, rest=True, log=CL.host_end(lib))


def edge_destroy():
    for mode, flags in ((1, (1, 2, 3, 0x7FFF, 0x10001, 0x27FFF)), (2, (-1, -2, 0x8000, 0xFFFF, 0x18000, 0x18001, 0xFFFF8000)),
                        (0, (0, 0x10000, 0x20000)), (3, (1, 0, -1, 5, 0x8000))):
        for flag in flags:
            yield dict(mode=mode, flag=flag & MASK, q={})


def random_destroy(r):
    mode = r.randrange(4)
    if mode == 1:
        flag = r.randint(1, 0x7FFF)
    elif mode == 2:
        flag = r.randint(-0x8000, -1)
    elif mode == 0:
        flag = 0
    else:
        flag = r.randint(-0x8000, 0x7FFF)
    if r.random() < 0.3:
        flag |= r.getrandbits(16) << 16      # the high half of the register is ignored
    return dict(mode=mode, flag=flag & MASK, q={})


# ================================================================= the cache helpers

CACHE_ENTRIES, CACHE_PAGES, CACHE_ARRAY = 6, 2, 8
L_CACHE, L_ENTRY, L_PAGE, L_LINK = 1, 11, 21, 31
TEXTURE_BASE = 0x1C


def blank_cache_world():
    return dict(queue=0, f18c4=0, limits=[0.0, 0.25, 0.5, 0.75, 1.0], array=[],
                entries=[dict(flags=0, cw=8, ch=8, prio=0.0, key=0, next=0, page=0, w=8, h=8) for _ in range(CACHE_ENTRIES)],
                pages=[dict(entries=0, tflags=0) for _ in range(CACHE_PAGES)])


def cache_flat(w):
    d = [w["queue"], w["f18c4"]] + [fb(x) for x in w["limits"]]
    for en in w["entries"]:
        d += [en["flags"], en["cw"], en["ch"], fb(en["prio"]), en["key"], en["next"], en["page"], en["w"], en["h"]]
    for page in w["pages"]:
        d += [page["entries"], page["tflags"]]
    d += [len(w["array"])] + list(w["array"])
    return d


def cache_ppc(e, case):
    w, op = case["world"], case["op"]
    with guarded(e):
        L = recorded(e, case, {})
        cache = e.alloc(0x1910, b"\xa5" * 0x1910)
        entries = [e.alloc(0x5C, b"\xa5" * 0x5C) for _ in range(CACHE_ENTRIES)]
        links = [e.alloc(0x20, b"\xa5" * 0x20) for _ in range(CACHE_PAGES)]
        pages = [e.alloc(0x34, b"\xa5" * 0x34) for _ in range(CACHE_PAGES)]
        for page in pages:
            T.call_ppc(e, PAGE_CTOR, [page, 1])
        bases = [p + TEXTURE_BASE for p in pages]
        array = e.alloc(4 * CACHE_ARRAY + 4, b"\xa5" * (4 * CACHE_ARRAY + 4))

        def entry_at(n):
            return entries[n - 1] if n > 0 else 0

        w32(e, cache + 0x18AC, entry_at(w["queue"]))
        w32(e, cache + 0x18C4, w["f18c4"])
        ws(e, cache + 0x18EC, [fb(x) for x in w["limits"]])
        for i, en in enumerate(w["entries"]):
            a = entries[i]
            w32(e, a + 8, en["flags"])
            e.write(a + 0xC, struct.pack(">hhhh", T.s16(en["w"]), T.s16(en["h"]), T.s16(en["cw"]), T.s16(en["ch"])))
            w32(e, a + 0x18, bases[en["page"] - 1] if en["page"] > 0 else 0)
            w32(e, a + 0x2C, fb(en["prio"]))
            w32(e, a + 0x34, en["key"])
            w32(e, a + 4, entry_at(en["next"]))
        for i, page in enumerate(w["pages"]):
            ws(e, links[i], [bases[i], entry_at(page["entries"])])
            w32(e, bases[i], page["tflags"])
        for i, n in enumerate(w["array"]):
            w32(e, array + 4 * i, entry_at(n))
        fixed = {cache: L_CACHE}
        fixed.update({a: L_ENTRY + i for i, a in enumerate(entries)})
        fixed.update({b: L_PAGE + i for i, b in enumerate(bases)})
        fixed.update({a: L_LINK + i for i, a in enumerate(links)})
        L.labels.update(fixed)

        def flush(em):
            a = em.r(3)
            L.rec("FlushEntryFlags", (L.lab(a),))
            w32(em, a + 8, em.u32(a + 8) & ~PAGE_BITS)

        def set_flags(em):
            a, flags = em.r(3), T.s32(em.r(4))
            L.rec("SetFlags", (L.lab(a), flags))
            w32(em, a, (flags & MASK) | (em.u32(a) & 0x10000000))

        st = CL.Stubs()
        st.real = FUNCTIONS | {PAGE_CTOR}
        st.internal(STORE_WRAP, CL.recorder("StoreAsWrapping", "L", ret="i"))
        st.internal(FLUSH_FLAGS, flush)
        st.internal(SET_FLAGS, set_flags)
        st.imp("__assertion_failed", lambda em: L.rec("assert", (T.s32(em.r(5)),), (em.cstr(em.r(3)), em.cstr(em.r(4)))))
        saved = st.activate(e)
        try:
            with HardwareRounding(e, [(ESC, ESC + 616)]):
                if op == 1:
                    T.call_ppc(e, ESC, [cache, array, case["args"][0] & MASK, case["args"][1] & MASK, case["args"][2] & MASK])
                else:
                    T.call_ppc(e, CLEAR, [cache, links[case["args"][0] - 1]])
        finally:
            CL.Stubs.deactivate(e, saved)
        out = [L.lab(e.u32(cache + 0x18AC)), T.s32(e.u32(cache + 0x18C4))]
        for a in entries:
            (flags, w_, h_, cw, ch) = struct.unpack(">Ihhhh", e.read(a + 8, 12))
            out += [flags, cw, ch, T.s32(e.u32(a + 0x34)), L.lab(e.u32(a + 4)), L.lab(e.u32(a + 0x18)), w_, h_, canon(e.u32(a + 0x2C))]
        for i in range(CACHE_PAGES):
            out += [L.lab(e.u32(links[i] + 4)), e.u32(bases[i])]
        return dict(state=out, log=L.log)


def cache_host(lib, case):
    w, op = case["world"], case["op"]
    lib.tpw_fs_cache.restype = C.c_int64
    lib.tpw_fs_cache.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64), C.POINTER(C.c_int64), C.c_int32]
    lib.tpw_fs_count.restype = C.c_int32
    description = i64(cache_flat(w))
    capacity = 64 + 9 * CACHE_ENTRIES + 2 * CACHE_PAGES
    out = (C.c_int64 * capacity)()
    CL.host_begin(lib, {}, case.get("q"))
    lib.tpw_fs_cache(op, i64((list(case["args"]) + [0, 0, 0])[:3]), description, out, capacity)
    n = lib.tpw_fs_count()
    log = CL.host_end(lib)
    assert n <= capacity
    return dict(state=list(out[:n]), log=log)


def cache_entry(r, **kw):
    prio = r.choice([T.f32(r.uniform(-0.2, 1.4)), 0.0, 0.0625, 0.125, 0.25, 0.5, 1.0, 0.875, -0.001, 1.5, 2.0, NAN, INF, -INF,
                     T.f32(r.uniform(-3e6, 3e6)), -1e9, 3e9, T.f32(r.uniform(0, 1))])
    en = dict(flags=r.choice([0, 0x10, 0x20, 0x30, 0x40, 0x50, 0x70, 0x80, 0xB0, 0xC0, 0xF0]) | (r.getrandbits(32) & ~0xFF & r.getrandbits(32) & ~COMPRESSED),
              cw=r.choice([1, 2, 3, 7, 8, 9, 16, 32, 64, 128, 256, r.randint(1, 300), 0, -5, 0x7FFF]),
              ch=r.choice([1, 2, 3, 7, 8, 9, 16, 32, 64, 128, 256, r.randint(1, 300), 0, -5, 0x7FFF]),
              prio=prio, key=T.s32(r.choice([0, 1, 0x20000000, r.getrandbits(32)])), next=0, page=r.randint(0, CACHE_PAGES),
              w=r.randint(0, 300), h=r.randint(0, 300))
    if r.random() < 0.5:
        en["flags"] = (en["flags"] & ~BLEND_KIND)
    en["flags"] &= MASK
    en.update(kw)
    return en


def random_cache_world(r, entries_in_array=None):
    w = blank_cache_world()
    w["entries"] = [cache_entry(r) for _ in range(CACHE_ENTRIES)]
    w["f18c4"] = T.s32(r.choice([0, 100, 5000, 100000, -1000, 0x7FFFFFFF, -0x80000000, r.getrandbits(32)]))
    lo = r.choice([-0.001, -1.0, 0.0, 0.1])
    span = r.choice([0.5, 1.0, 2.0, 0.001, 1e6])
    w["limits"] = [lo, lo + span / 8, lo + span / 4, lo + span / 2, lo + span]
    if r.random() < 0.1:
        w["limits"] = [r.choice([NAN, INF, -INF, 0.0, 1.0, T.f32(r.uniform(-2, 2))]) for _ in range(5)]
    return w


# The twelve levels of Entry_Specific_Compress (engine data 0x10051ec8): the lowest detail class (flags & 0xc0) and the
# index of the priority limit that opens the window (limits[index], limits[index + 1]].
LEVELS = [(192, 3), (192, 2), (128, 3), (192, 1), (128, 2), (64, 3), (192, 0), (128, 1), (64, 2), (128, 0), (64, 1), (64, 0)]


def rflags_levels(r, store):
    return (r.choice([0x00, 0x40, 0x80, 0xC0]) | (store << 4) | (r.getrandbits(32) & ~0xFF & r.getrandbits(32) & ~COMPRESSED & ~BLEND_KIND)) & MASK


def fit_entry(r, en, level, limits):
    """Makes an entry one that `level` cuts (or nearly): a detail class that is high enough and a priority inside the window."""
    detail, index = LEVELS[level]
    en["flags"] = (en["flags"] & ~0xC0) | r.choice([d for d in (0x00, 0x40, 0x80, 0xC0) if d >= detail])
    low, high = limits[index], limits[index + 1]
    if low == low and high == high and low < high and abs(low) < 1e30 and abs(high) < 1e30:
        en["prio"] = T.f32(r.choice([high, (low + high) / 2, low + (high - low) * 0.999, low + (high - low) * 0.001, high]))
    return en


def esc_case(r, fully_random=False):
    w = random_cache_world(r)
    n = r.randint(0, CACHE_ARRAY)
    w["array"] = [r.randint(1, CACHE_ENTRIES) for _ in range(n)]
    level = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, r.randint(0, 11), r.randint(0, 11), 12])
    if not fully_random:
        # Make the entries likely to be cut: a store level left, a priority inside the window, a detail class that fits.
        for en in w["entries"]:
            if r.random() < 0.75:
                en["flags"] = rflags_levels(r, r.randint(1, 3))
                if level < 12:
                    fit_entry(r, en, level, w["limits"])
            if r.random() < 0.7:
                en["cw"], en["ch"] = r.choice([8, 16, 32, 64, 128, 256]), r.choice([8, 16, 32, 64, 128, 256])
    answers = [r.randrange(2) for _ in range(r.randint(1, 12))]
    count = n if r.random() < 0.6 else r.randint(0, n)
    available = r.choice([-0x80000000, -0x80000000, 0x7FFFFFFF, 0, 100, 5000, w["f18c4"] - 1, w["f18c4"], w["f18c4"] + 1,
                          T.s32(r.getrandbits(32))])
    return dict(op=1, world=w, args=[count, level, available], q={"StoreAsWrapping": answers})


def edge_esc():
    r = random.Random(301)
    limits = [0.0, 0.25, 0.5, 0.75, 1.0]
    # Every level against every detail class and each side of its window.
    for level in range(13):
        for detail in (0x00, 0x40, 0x80, 0xC0):
            idx = LEVELS[level % 12][1]
            low, high = limits[idx], limits[idx + 1]
            w = blank_cache_world()
            w["limits"] = limits
            w["f18c4"] = 100000
            for en, prio in zip(w["entries"], (low - 0.01, low, low + 0.001, (low + high) / 2, high, high + 0.001)):
                en.update(flags=detail | 0x30, cw=64, ch=32, prio=T.f32(prio), key=5)
            w["array"] = [1, 2, 3, 4, 5, 6]
            yield dict(op=1, world=w, args=[6, level, -0x80000000], q={"StoreAsWrapping": [0]})
    # The cut itself: store level, wrapping answers, sizes (odd, one, eight, wrapping eight), the 0x80000 kind, the locked bit.
    for flags, wrap, cw, ch in ((0x30, 0, 64, 64), (0x10, 0, 3, 5), (0x20, 0, 1, 1), (0x30, 1, 64, 32), (0x30, 1, 8, 8), (0x10, 1, 8, 16),
                                (0x10, 1, 9, 9), (0x00, 0, 64, 64), (0x00, 1, 64, 64), (0x30, 0, 0, 0), (0x30, 0, -3, -5), (0x30, 0, 0x7FFF, 0x7FFF),
                                (0x30 | BLEND_KIND, 0, 64, 64), (0x30 | LOCKED, 0, 64, 64), (0x30 | LOCKED | BLEND_KIND, 0, 64, 64),
                                (0x30 | 0xC0, 0, 16, 16), (0x10 | BLEND_KIND, 1, 64, 64), (0x10, 1, 16, 8), (0x30, 1, 8, 64)):
        for answers in ([wrap, wrap], [wrap, wrap, wrap], [wrap, 1 - wrap], [1 - wrap, wrap]):
            w = blank_cache_world()
            w["f18c4"] = 100000
            w["entries"][0].update(flags=flags | 0xC0, cw=cw, ch=ch, prio=0.9, key=7)
            w["limits"] = limits
            w["array"] = [1]
            yield dict(op=1, world=w, args=[1, 0, -0x80000000], q={"StoreAsWrapping": answers})
    # Stops when the total is below the limit: before, at and after.
    for total, available in ((100000, 99000), (100000, 100000), (100000, 100001), (6000, 5000), (0, 0), (-5, 0), (0x7FFFFFFF, -5)):
        w = blank_cache_world()
        w["f18c4"] = total
        w["limits"] = limits
        for en in w["entries"]:
            en.update(flags=0xF0, cw=64, ch=64, prio=0.9, key=1)
        w["array"] = [1, 2, 3, 4, 5, 6]
        yield dict(op=1, world=w, args=[6, 0, available], q={"StoreAsWrapping": [0]})
    # The stop is exact: after the k-th cut the total is the limit, one below it and one above it (each cut of a 64 x 64
    # entry frees 3072 pixels).
    for k in (1, 2, 3, 5):
        for delta in (-1, 0, 1):
            w = blank_cache_world()
            w["f18c4"] = 100000
            w["limits"] = limits
            for en in w["entries"]:
                en.update(flags=0xF0, cw=64, ch=64, prio=0.9, key=1)
            w["array"] = [1, 2, 3, 4, 5, 6]
            yield dict(op=1, world=w, args=[6, 0, 100000 - 3072 * k + delta], q={"StoreAsWrapping": [0]})
    # The sort key from the priority: the edges of fctiwz (every number is inside the window of level 0, a NaN and -infinity are not).
    for prio in (-1e6, -1e7, -1e9, -INF, NAN, 1.5, 0.0, 2.0, 1e9, 1.4999, -0.0005, 0.7501, INF, 3e38, -3e38):
        for flags in (0xF0, 0xF0 | LOCKED, 0xF0 | LOCKED | BLEND_KIND):
            w = blank_cache_world()
            w["limits"] = [0.0, 0.0, 0.0, -INF, INF]
            w["entries"][0].update(flags=flags, cw=32, ch=32, prio=prio, key=3)
            w["array"] = [1]
            yield dict(op=1, world=w, args=[1, 0, -0x80000000], q={"StoreAsWrapping": [0]})
    # An entry twice in the list, an empty list, a negative count.
    w = blank_cache_world()
    w["limits"] = limits
    w["entries"][0].update(flags=0xF0, cw=64, ch=64, prio=0.9, key=1)
    w["array"] = [1, 1, 1]
    yield dict(op=1, world=w, args=[3, 0, -0x80000000], q={"StoreAsWrapping": [0]})
    yield dict(op=1, world=w, args=[0, 0, -0x80000000], q={})
    yield dict(op=1, world=w, args=[-1, 0, -0x80000000], q={})
    for _ in range(20):
        yield esc_case(r)


def random_esc(r):
    return esc_case(r, fully_random=r.random() < 0.25)


def clear_case(r):
    w = random_cache_world(r)
    # Page 1 holds a chain of entries; the queue holds another chain; the rest are on neither.
    order = list(range(1, CACHE_ENTRIES + 1))
    r.shuffle(order)
    on_page = order[:r.randint(0, 4)]
    queued = order[len(on_page):len(on_page) + r.randint(0, 2)]
    for chain in (on_page, queued):
        for a, b in zip(chain, chain[1:] + [0]):
            w["entries"][a - 1]["next"] = b
    w["pages"][0]["entries"] = on_page[0] if on_page else 0
    w["pages"][1]["entries"] = r.choice([0, queued[0] if queued else 0])
    w["queue"] = queued[0] if queued else 0
    for n in on_page:
        en = w["entries"][n - 1]
        en["flags"] = (r.getrandbits(32) & r.getrandbits(32)) & MASK
        for bit, p in ((ASKED, 0.6), (LOCKED, 0.4), (QUEUED, 0.3), (ON_PAGE, 0.5), (0x1000, 0.5), (0x2000, 0.3)):
            en["flags"] = (en["flags"] | bit) if r.random() < p else (en["flags"] & ~bit)
        en["page"] = r.choice([0, 1, 2])
    w["pages"][0]["tflags"] = r.getrandbits(32) if r.random() < 0.5 else r.choice([0, 1, 2, 4, 7, 0x10000000, 0x10000007])
    w["pages"][1]["tflags"] = r.choice([0, 1, 0x10000000, r.getrandbits(32)])
    return dict(op=2, world=w, args=[r.choice([1, 1, 2])], q={})


def edge_clear():
    r = random.Random(302)
    for asked in (0, ASKED):
        for locked in (0, LOCKED):
            for extra in (0, QUEUED, ON_PAGE | 0x7000, 0xFFFFFFFF & ~(ASKED | LOCKED)):
                w = blank_cache_world()
                w["pages"][0]["entries"] = 1
                w["pages"][0]["tflags"] = 0x10000007
                w["entries"][0].update(flags=asked | locked | extra, next=0, page=1)
                w["queue"] = 0
                yield dict(op=2, world=w, args=[1], q={})
    # An empty page; a chain of all six into an empty queue and into a queue of two.
    w = blank_cache_world()
    w["pages"][0]["tflags"] = 0x10000003
    yield dict(op=2, world=w, args=[1], q={})
    for queue in (0, 1):
        w = blank_cache_world()
        for i, en in enumerate(w["entries"]):
            en.update(flags=ASKED | (LOCKED if i % 2 else 0) | (ON_PAGE if i % 3 else 0), next=i + 2 if i < 5 else 0, page=1)
        w["pages"][0]["entries"] = 1
        yield dict(op=2, world=w, args=[1], q={})
    w = blank_cache_world()
    for i, en in enumerate(w["entries"]):
        en.update(flags=ASKED | ON_PAGE, next=0, page=2)
    w["entries"][0].update(next=2)
    w["entries"][1].update(next=0)
    w["entries"][4].update(next=6)
    w["pages"][1]["entries"] = 5
    w["queue"] = 1
    yield dict(op=2, world=w, args=[2], q={})
    for _ in range(20):
        yield clear_case(r)


def random_clear(r):
    return clear_case(r)


# ================================================================= the particles

SLOTS, P_ENTRIES = 4, 4
L_P_DRAW, L_P_CLIP, L_P_FINAL, L_P_TCACHE, L_P_ENTRY, L_P_PAGE, L_P_LIST = 2, 3, 4, 5, 11, 21, 41


def blank_particle_world():
    return dict(frame=1, f84=0, aspect=0.75, f04=-1, f08=-1, clip=[0] * EMITTERS,
                entries=[dict(flags=0, page=0, uoff=0.0, voff=0.0, usize=0.0, vsize=0.0) for _ in range(P_ENTRIES)],
                slots=[dict(entry=0, aspect=1.0) for _ in range(SLOTS)], emitters={})


def emitter(count=0, low=(0, 0, 0), high=(0, 0, 0), has_list=True, records=None):
    return dict(count=count, low=list(low), high=list(high), has_list=has_list, records=records or [])


def record(x=0, y=0, z=0, colour=0, slot=-1, size=0, angle=0, flags=0):
    return dict(x=x, y=y, z=z, colour=colour, slot=slot, size=size, angle=angle, flags=flags)


def particle_flat(w):
    d = [w["frame"], w["f84"], fb(w["aspect"]), w["f04"], w["f08"]] + list(w["clip"])
    for en in w["entries"]:
        d += [en["flags"], en["page"], fb(en["uoff"]), fb(en["voff"]), fb(en["usize"]), fb(en["vsize"])]
    for s in w["slots"]:
        d += [s["entry"], fb(s["aspect"])]
    d.append(len(w["emitters"]))
    for number in sorted(w["emitters"]):
        em = w["emitters"][number]
        d += [number, em["count"]] + em["low"] + em["high"] + [1 if em["has_list"] else 0, len(em["records"])]
        for rec in em["records"]:
            d += [rec["x"], rec["y"], rec["z"], rec["colour"], rec["slot"], rec["size"], rec["angle"], rec["flags"]]
    return d


class ParticleSide:
    """Holds the fake functions of a case (the particle system's hooks, the clock, the first-person hook): the
    function descriptors made in setup() call whatever the current side has under their names."""

    def __init__(self, e, case):
        self.e, self.case, self.w = e, case, case["world"]


def particle_ppc(e, case):
    w, op = case["world"], case["op"]
    with guarded(e):
        side = ParticleSide(e, case)
        L = recorded(e, case, {})
        particle = e.alloc(0xAC, b"\xa5" * 0xAC)
        cache, clip, final, draw = (e.alloc(0x40, b"\xa5" * 0x40) for _ in range(4))
        entries = [e.alloc(0x5C, b"\xa5" * 0x5C) for _ in range(P_ENTRIES)]
        slots = [e.alloc(16, b"\xa5" * 16) for _ in range(SLOTS)]
        pages = [PAGE_BASE + 0x100 * k for k in range(1, 9)]
        fixed = {particle: 1, draw: L_P_DRAW, clip: L_P_CLIP, final: L_P_FINAL, cache: L_P_TCACHE}
        fixed.update({a: L_P_ENTRY + i for i, a in enumerate(entries)})
        fixed.update({p: L_P_PAGE + i for i, p in enumerate(pages)})
        L.labels.update(fixed)
        for i, en in enumerate(w["entries"]):
            a = entries[i]
            w32(e, a + 8, en["flags"])
            w32(e, a + 0x18, pages[en["page"] - 1] if en["page"] > 0 else 0)
            ws(e, a + 0x1C, [fb(en["usize"]), fb(en["vsize"]), fb(en["uoff"]), fb(en["voff"])])
        for i, s in enumerate(w["slots"]):
            ws(e, slots[i], [entries[s["entry"] - 1] if s["entry"] > 0 else 0, 0x5A5A5A5A, fb(s["aspect"])])
        ws(e, DISPLAY + 4, [fb(w["aspect"])])
        ws(e, DISPLAY + 0x80, [w["frame"], w["f84"]])
        w32(e, particle + 4, w["f04"])
        w32(e, particle + 8, w["f08"])
        e.write(particle + 0xC, bytes(w["clip"]))
        ws(e, particle + 0x84, [e.fs_hooks["hook_box"], 0, 0, 0, e.fs_hooks["hook_build"], e.fs_hooks["hook_free"],
                                e.fs_hooks["hook_count"]])
        ws(e, TCACHE_VAR, [cache])
        ws(e, CLIP_VAR, [clip])
        ws(e, FINAL_VAR, [final])
        ws(e, DRAW_VAR, [draw])
        lists = {}
        for k, number in enumerate(sorted(w["emitters"])):
            em = w["emitters"][number]
            data = b""
            for rec in em["records"]:
                slot = 0xFFFFFFFF if rec["slot"] < 0 else slots[rec["slot"]]
                data += struct.pack(">iiiIIHHI", T.s32(rec["x"]), T.s32(rec["y"]), T.s32(rec["z"]), rec["colour"] & MASK, slot,
                                    rec["size"] & 0xFFFF, rec["angle"] & 0xFFFF, rec["flags"] & MASK)
            address = e.alloc(max(len(data), 0x1C), data + b"\xa5" * 0x1C)
            lists[number] = address
            L.labels[address] = L_P_LIST + k
        side.lists = lists

        def count(em):
            number = em.r(3)
            L.rec("GetEmitterParticleCount", (number,))
            c = w["emitters"].get(number)
            em.set_r(3, T.u32(c["count"]) if c else 0)

        def box(em):
            number = em.r(3)
            L.rec("GetEmitterBoundingBox", (number,))
            c = w["emitters"].get(number)
            low, high = (c["low"], c["high"]) if c else ([0, 0, 0], [0, 0, 0])
            em.write(em.r(4), struct.pack(">iii", *[T.s32(v) for v in low]))
            em.write(em.r(5), struct.pack(">iii", *[T.s32(v) for v in high]))

        def build(em):
            number = T.s32(em.r(3))
            L.rec("BuildDrawingList", (number,))
            c = w["emitters"].get(number)
            em.set_r(3, lists[number] if c and c["has_list"] else 0)

        def free(em):
            L.rec("FreeDrawingList", (L.lab(em.r(3)),))

        side.hook_count, side.hook_box, side.hook_build, side.hook_free = count, box, build, free
        e.fs_side = side
        st = CL.Stubs()
        st.real = FUNCTIONS | {PTR_GLUE}
        st.internal(TCACHE_PREPARE, CL.recorder("TCache.Prepare", "LLf"))
        st.internal(GET_BLEND, CL.recorder("GetBlendingMode", "L", ret="i"))
        st.internal(CHECK_BOX, lambda em: check_box(em, L))
        st.internal(TRANSFORM_VERTEX, lambda em: transform_vertex(em, L))
        st.internal(ROTATE_VERTEX, lambda em: rotate_vertex(em, L))
        st.internal(DRAW_SPRITE, lambda em: draw_sprite(em, L))
        saved = st.activate(e)
        try:
            r3 = T.s32(T.call_ppc(e, P_PREPARE if op == 1 else P_PLOT, [particle])[0])
        finally:
            CL.Stubs.deactivate(e, saved)
            e.fs_side = None
        state = [T.s32(e.u32(particle + 4)), T.s32(e.u32(particle + 8))] + list(e.read(particle + 0xC, EMITTERS))
        return dict(result=r3, state=state, log=L.log)


def particle_host(lib, case):
    w, op = case["world"], case["op"]
    lib.tpw_fs_particles.restype = C.c_int64
    lib.tpw_fs_particles.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64), C.c_int32]
    lib.tpw_fs_count.restype = C.c_int32
    capacity = 2 + EMITTERS + 16
    out = (C.c_int64 * capacity)()
    CL.host_begin(lib, {}, case.get("q"))
    result = lib.tpw_fs_particles(op, i64(particle_flat(w)), out, capacity)
    n = lib.tpw_fs_count()
    log = CL.host_end(lib)
    assert n <= capacity
    return dict(result=C.c_int32(result).value, state=list(out[:n]), log=log)


# -- the fakes shared by the particle and rain worlds (PowerPC side)


def check_box(em, L):
    box = rs(em, em.r(4), 6)
    L.rec("CheckBoundingBox", (L.lab(em.r(3)),) + tuple(canon(v) for v in box) + (L.lab(em.r(5)),))
    em.set_r(3, L.ret("CheckBoundingBox", 0) & 0xFF)


def transform_vertex(em, L):
    v = rs(em, em.r(5), 3)
    L.rec("TransformVertex", (L.lab(em.r(3)), T.s32(em.r(6)), v[0], v[1], v[2]))
    ws(em, em.r(4), [v[1], v[2] ^ 0x80000000, v[0], L.ret("TransformW", 0x3F800000)])


def rotate_vertex(em, L):
    a = em.r(3)
    angle = cfb(em.f(1))
    w = rs(em, a, 16)
    L.rec("RotateVertex_Y", (angle, L.lab(em.r(5)), canon(w[0]), canon(w[1]), canon(w[4]), canon(w[5]), canon(w[8]), canon(w[9]),
                             canon(w[12]), canon(w[13])))
    turn = angle & 3
    for i in range(4):
        x, y = w[4 * i], w[4 * i + 1]
        if turn == 1:
            w[4 * i], w[4 * i + 1] = y, x ^ 0x80000000
        elif turn == 2:
            w[4 * i], w[4 * i + 1] = x ^ 0x80000000, y ^ 0x80000000
        elif turn == 3:
            w[4 * i], w[4 * i + 1] = y ^ 0x80000000, x
    ws(em, a, w)


def draw_sprite(em, L):
    p = em.r(4)
    flags, vertices, f08, blend, texture, _f14, u0, v0, u1, v1, f28 = rs(em, p, 11)
    L.rec("DrawSprite", (L.lab(em.r(3)), flags, T.s32(f08), blend, L.lab(texture), canon(u0), canon(v0), canon(u1), canon(v1),
                         T.s32(f28), T.s32(em.r(5))))
    corners = rs(em, vertices, 16)
    for i in range(4):
        L.rec("SpriteCorner", (i,) + tuple(canon(v) for v in corners[4 * i:4 * i + 4]))
    em.set_r(3, T.u32(L.ret("DrawSprite", 1)))


# -- cases

def rcoord(r):
    """A particle's position or box corner: small, large, extreme."""
    k = r.random()
    if k < 0.5:
        return r.randint(-5000, 5000)
    if k < 0.8:
        return r.randint(-0x400000, 0x400000)
    return r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x1000000, 0x1000001, -0x1000001, 0x7FFFFF80, 1023, 1024, 1025])


def rsize16(r):
    return r.choice([0, 1, 2, 5, 100, 512, 1024, 2047, 2048, 4096, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16), r.randint(1, 4000)])


PARTICLE_FLAGS = (4, 0x2000, 0x20000, 0x4000, 0x10000, 0x40000)


def random_record(r, slots_used):
    flags = 0
    for bit in PARTICLE_FLAGS:
        if r.random() < 0.35:
            flags |= bit
    flags |= (r.getrandbits(32) & r.getrandbits(32) & r.getrandbits(32) & ~0x7FFFF) if r.random() < 0.3 else 0
    colour = r.choice([0, 0xFF, 0xEF, 0xF0, 0xF1, 1, 0x80, 0xFFFFFFFF, 0x12345600, r.getrandbits(32), 0xFF000000 | r.getrandbits(8)])
    return record(rcoord(r), rcoord(r), rcoord(r), colour, r.choice([-1] * slots_used + list(range(SLOTS))) if slots_used else r.randrange(-1, SLOTS),
                  rsize16(r), r.choice([0, 0, 1, 2, 3, 0x4000, 0x8000, 0xC000, r.getrandbits(16)]), flags & MASK)


def random_particle_world(r, many=False):
    w = blank_particle_world()
    w["frame"] = r.choice([0, 1, 2, 7, 100, -1, 0x7FFFFFFF, -0x80000000, r.getrandbits(31)])
    w["f84"] = r.choice([0, 0, 1, 5, -1, r.getrandbits(31)])
    w["aspect"] = r.choice([0.75, 1.0, 0.5, 1.333333, 0.0, -1.0, NAN, INF, 3.0e38, 1e-40, T.f32(r.uniform(0, 4))])
    w["f04"] = r.choice([-1, w["frame"], w["frame"] + 1, 0, r.getrandbits(31)])
    w["f08"] = r.choice([-1, w["f84"], w["f84"] + 1, 0, r.getrandbits(31)])
    w["clip"] = [r.choice([0, 0x40, 0x7C, 0x3C, 0x41, 0xFF, 0x80, r.getrandbits(8)]) if r.random() < 0.5 else 0 for _ in range(EMITTERS)]
    for en in w["entries"]:
        en.update(flags=r.choice([0, ON_PAGE, 0x100000, ON_PAGE | 0x100000, r.getrandbits(32)]), page=r.randint(0, 8),
                  uoff=T.f32(r.uniform(0, 1)), voff=T.f32(r.uniform(0, 1)), usize=T.f32(r.uniform(0, 1)),
                  vsize=T.f32(r.uniform(0, 1)))
        if r.random() < 0.15:
            en["uoff"], en["usize"] = r.choice([NAN, INF, -INF, 0.0, -0.0, 3e38]), r.choice([NAN, INF, 3e38, 1.0])
    for s in w["slots"]:
        s.update(entry=r.randint(0, P_ENTRIES), aspect=r.choice([1.0, 2.0, 0.5, 0.0, -1.0, NAN, INF, T.f32(r.uniform(0.1, 3))]))
    used = r.sample(range(EMITTERS), r.randint(1, 5 if not many else 12))
    for number in used:
        n = r.choice([0, 1, 2, 3, 5, 8])
        low = [rcoord(r) for _ in range(3)] if r.random() < 0.85 else [0, 0, 0]
        high = [rcoord(r) for _ in range(3)] if r.random() < 0.85 else [0, 0, 0]
        if r.random() < 0.2:
            low, high = [0, 0, 0], [0, 0, 0]
        if r.random() < 0.1:
            k = r.randrange(6)
            low, high = ([0, 0, 0], [rcoord(r) for _ in range(3)]) if r.random() < 0.5 else ([rcoord(r) for _ in range(3)], [0, 0, 0])
            (low if k < 3 else high)[k % 3] = 0
        count = n if r.random() < 0.9 else r.choice([-1, -5])
        records = [random_record(r, r.choice([0, 3])) for _ in range(max(count, 0))]
        w["emitters"][number] = emitter(count, low, high, has_list=r.random() < 0.9, records=records)
    return w


def particle_queues(r):
    return {"CheckBoundingBox": [r.choice([0, 0x40, 0x7C, 0x3F, 0x41, 0xFF, r.getrandbits(8)]) for _ in range(r.randint(1, 8))],
            "DrawSprite": [r.choice([1, 1, 1, 1, 0, 5]) for _ in range(r.randint(1, 16))],
            "GetBlendingMode": [r.choice([0, 0x556, 0x1256, 0x200522, r.getrandbits(32)]) for _ in range(r.randint(1, 8))],
            "TransformW": [r.choice([0x3F800000, 0, 0x40000000, 0x7FC00000, r.getrandbits(32)]) for _ in range(r.randint(1, 6))]}


def particle_case(r, op, many=False):
    w = random_particle_world(r, many)
    if op == 2 and r.random() < 0.6:
        w["f04"], w["f08"] = w["frame"] + 1, w["f84"]
        if r.random() < 0.7:
            w["clip"] = [0x40 | (c & 0x3F) if c else c for c in w["clip"]]
            for number in w["emitters"]:
                w["clip"][number] = 0x40 | r.getrandbits(6)
    q = particle_queues(r)
    if r.random() < 0.15:
        q["DrawSprite"] = [1] * r.randint(0, 6) + [0]
    return dict(op=op, world=w, q=q)


def edge_prepare():
    r = random.Random(401)
    # An empty box, each of the six words alone, then a real one; with and without a list; counts 1 and -1 and 0.
    for count in (1, 3, -1, 0):
        for low, high in (([0, 0, 0], [0, 0, 0]), ([1, 0, 0], [0, 0, 0]), ([0, 1, 0], [0, 0, 0]), ([0, 0, 1], [0, 0, 0]),
                          ([0, 0, 0], [1, 0, 0]), ([0, 0, 0], [0, 1, 0]), ([0, 0, 0], [0, 0, 1]),
                          ([-100, -200, -300], [400, 500, 600]), ([-0x80000000, 0x7FFFFFFF, 1], [0x7FFFFFFF, -0x80000000, -1])):
            for has_list in (True, False):
                w = blank_particle_world()
                w["clip"] = [0xA5] * EMITTERS
                recs = [record(slot=-1), record(slot=0), record(slot=1), record(slot=2), record(slot=3)][:max(count, 0)]
                w["emitters"][0] = emitter(count, low, high, has_list, recs)
                w["emitters"][EMITTERS - 1] = emitter(count, low, high, has_list, recs)
                w["slots"] = [dict(entry=1, aspect=1.0), dict(entry=0, aspect=1.0), dict(entry=2, aspect=1.0),
                              dict(entry=4, aspect=2.0)]
                yield dict(op=1, world=w, q={"CheckBoundingBox": [0x40, 0x13]})
    w = blank_particle_world()
    for number in range(EMITTERS):
        w["emitters"][number] = emitter(1, [number, 1, 2], [number + 5, 6, 7], True, [record(slot=number % 4)])
    w["clip"] = [0xFF] * EMITTERS
    yield dict(op=1, world=w, q={"CheckBoundingBox": [0x40, 0, 0x7F, 1]})           # every emitter active
    yield dict(op=1, world=blank_particle_world(), q={})                           # none
    for _ in range(20):
        yield particle_case(r, 1)


def random_prepare(r):
    return particle_case(r, 1)


def edge_plot():
    r = random.Random(402)
    base = blank_particle_world()
    base["frame"], base["f84"], base["aspect"] = 5, 2, 0.75
    base["clip"] = [0x40] * EMITTERS
    base["slots"] = [dict(entry=1, aspect=1.5), dict(entry=0, aspect=0.5), dict(entry=2, aspect=2.0), dict(entry=3, aspect=0.0)]
    base["entries"][0].update(flags=ON_PAGE, page=2, uoff=0.25, voff=0.5, usize=0.125, vsize=0.0625)
    base["entries"][1].update(flags=0x100000, page=3, uoff=0.5, voff=0.25, usize=0.5, vsize=0.25)
    base["entries"][2].update(flags=ON_PAGE | 0x100000, page=4, uoff=0.0, voff=0.0, usize=1.0, vsize=1.0)

    def one(rec_list, f04=-1, f08=-1, clip=0x40, count=None, has_list=True, **kw):
        w = dict(base)
        w.update(kw)
        w["f04"], w["f08"] = f04, f08
        w["clip"] = [clip] * EMITTERS
        w["emitters"] = {3: emitter(len(rec_list) if count is None else count, has_list=has_list, records=rec_list)}
        return dict(op=2, world=w, q={})

    # The gate: the frame words match, one of them differs.
    for f04, f08 in ((-1, -1), (5, 2), (5, 3), (4, 2), (5, -1), (-1, 2)):
        yield one([record(1024, 2048, 3072, 0xFF, -1, 1024)], f04, f08)
    # The clip bit, the count, the list.
    for clip in (0x00, 0x40, 0x3F, 0x7C, 0xBF, 0x80):
        yield one([record(1024, 2048, 3072, 0xFF, -1, 1024)], clip=clip)
    yield one([record()], count=0)
    yield one([record()], count=-3)
    yield one([record(), record()], has_list=False)
    # A texture: each slot, the flags of the entry, the colour's low byte of an untextured one, every flag of a particle.
    for slot in (-1, 0, 1, 2, 3):
        for colour in (0, 1, 0xEF, 0xF0, 0xFF, 0x80, 0x12345678):
            yield one([record(1024, 0, -1024, colour, slot, 1024, 0, 0)])
    for flags in range(0, 0x80000, 0x1111):
        yield one([record(100, 200, 300, 0xFF, 0, 500, 0x4000, flags & 0x7FFFF)])
    for flags in (4, 4 | 0x2000, 0x20000, 0x4000, 0x10000, 0x40000, 4 | 0x20000 | 0x4000 | 0x10000, 0x7FFFF, 0xFFFFFFFF, 0):
        for slot in (-1, 0):
            yield one([record(1, 2, 3, 0x80, slot, 3000, 0x1234, flags)])
    # The angle: zero, each quarter turn, the highest.
    for angle in (0, 1, 0x4000, 0x8000, 0xC000, 0xFFFF, 0x0FFF):
        yield one([record(10, 20, 30, 0xFF, -1, 1000, angle, 0)])
    # The sprite flag stays: a texture with 0x100000 then one without; a null entry; an entry on no page.
    yield one([record(0, 0, 0, 0, 1, 1000), record(0, 0, 0, 0, 0, 1000), record(0, 0, 0, 0, 2, 1000), record(0, 0, 0, 0, -1, 1000)])
    yield one([record(5, 5, 5, 5, 0, 100), record(5, 5, 5, 5, 3, 100)])
    # DrawSprite fails on the second of three particles and on the first.
    for failing in ([1, 0], [0], [1, 1, 0], [1, 1, 1]):
        w = one([record(1, 1, 1, 1, 0, 100), record(2, 2, 2, 2, 1, 200), record(3, 3, 3, 3, -1, 300)])
        w["q"] = {"DrawSprite": failing}
        yield w
    # Several emitters, some in view.
    w = dict(base)
    w["f04"], w["f08"] = -1, -1
    w["clip"] = [0x40 if i % 2 == 0 else 0x3F for i in range(EMITTERS)]
    w["emitters"] = {number: emitter(2, has_list=True, records=[record(number * 100, number, -number, 0x40, number % 4 - 1, 1000, number * 3, number * 7),
                                                           record(1, 2, 3, 0xFF, -1, 1024, 0, 0x40000)]) for number in (0, 1, 2, 3, 40, 119)}
    yield dict(op=2, world=w, q={})
    yield dict(op=2, world=dict(blank_particle_world(), f04=0, f08=0), q={})
    for _ in range(20):
        yield particle_case(r, 2)


def random_plot(r):
    return particle_case(r, 2, many=r.random() < 0.3)


# ================================================================= the rain

R_ENTRIES = 3
L_R_RAIN, L_R_FROM, L_R_TO, L_R_VIEW, L_R_INVERSE = 6, 7, 8, 9, 10


FLOAT_REGIONS = [(0x3328, 3), (0x9D58, 17), (0xA170, 4), (0xA1BC, 4)]
POINTER_FIELDS = (0x3348, 0x334C, 0x3350)


def blank_rain_world():
    return dict(words={}, entries=[dict(flags=0, page=0, uoff=0.0, voff=0.0, usize=0.0, vsize=0.0) for _ in range(R_ENTRIES)],
                system=0, view=[0] * 16, inverse=[0] * 16)


def rain_flat(w):
    d = [len(w["words"])]
    for offset in sorted(w["words"]):
        d += [offset, w["words"][offset]]
    for en in w["entries"]:
        d += [en["flags"], en["page"], fb(en["uoff"]), fb(en["voff"]), fb(en["usize"]), fb(en["vsize"])]
    d += [w["system"]] + list(w["view"]) + list(w["inverse"])
    return d


def rain_ppc(e, case):
    w, op = case["world"], case["op"]
    with guarded(e):
        side = ParticleSide(e, case)
        L = recorded(e, case, {})
        rain = e.alloc(RAIN_BYTES, b"\xa5" * RAIN_BYTES)
        entries = [e.alloc(0x5C, b"\xa5" * 0x5C) for _ in range(R_ENTRIES)]
        cache, draw, vfrom, vto, view, inverse = (e.alloc(0x40, b"\xa5" * 0x40) for _ in range(6))
        pages = [PAGE_BASE + 0x100 * k for k in range(1, 9)]
        fixed = {rain: L_R_RAIN, draw: L_P_DRAW, cache: L_P_TCACHE, vfrom: L_R_FROM, vto: L_R_TO, view: L_R_VIEW,
                 inverse: L_R_INVERSE}
        fixed.update({a: L_P_ENTRY + i for i, a in enumerate(entries)})
        fixed.update({p: L_P_PAGE + i for i, p in enumerate(pages)})
        L.labels.update(fixed)
        for offset, word in w["words"].items():
            if offset in POINTER_FIELDS:
                word = entries[word - 1] if word > 0 else 0
            w32(e, rain + offset, word)
        for i, en in enumerate(w["entries"]):
            a = entries[i]
            w32(e, a + 8, en["flags"])
            w32(e, a + 0x18, pages[en["page"] - 1] if en["page"] > 0 else 0)
            ws(e, a + 0x1C, [fb(en["usize"]), fb(en["vsize"]), fb(en["uoff"]), fb(en["voff"])])
        w32(e, SYSTEM, w["system"])
        ws(e, view, w["view"])
        ws(e, inverse, w["inverse"])
        ws(e, VIEW_VAR, [view])
        ws(e, INVVIEW_VAR, [inverse])
        ws(e, TCACHE_VAR, [cache])
        ws(e, DRAW_VAR, [draw])
        ws(e, CLOCK_VAR, [e.fs_hooks["hook_clock"], e.fs_hooks["hook_first_person"]])
        w32(e, CLOCK_VAR, e.fs_hooks["hook_clock"])
        w32(e, FIRST_VAR, e.fs_hooks["hook_first_person"])
        side.hook_clock = lambda em: (L.rec("ReadClock"), em.set_r(3, L.ret("ReadClock", 0) & MASK))
        side.hook_first_person = lambda em: (L.rec("IsFirstPerson"), em.set_r(3, T.u32(L.ret("IsFirstPerson", 0))))
        e.fs_side = side

        def answer(name):
            return lambda em: em.set_r(3, T.u32(L.ret(name, 1)))

        def with_floats(name):
            def stub(em):
                L.rec(name, (L.lab(em.r(3)), cfb(em.f(1)), cfb(em.f(2)), cfb(em.f(3))))
                em.set_r(3, T.u32(L.ret(name, 1)))
            return stub

        def with_vectors(name):
            def stub(em):
                L.rec(name, (L.lab(em.r(3)), L.lab(em.r(4)), L.lab(em.r(5))))
                em.set_r(3, T.u32(L.ret(name, 1)))
            return stub

        def get_next(em):
            L.rec("GetNextPrimitive", (L.lab(em.r(3)), T.s32(em.r(4)), T.s32(em.r(5)), em.r(6), T.s32(em.r(7)), L.lab(em.r(8))))
            em.set_r(3, 0x7D200000 if L.ret("GetNextPrimitive", 1) else 0)

        st = CL.Stubs()
        st.real = FUNCTIONS | {PTR_GLUE, CVT_UNSIGNED}
        st.internal(TCACHE_PREPARE, CL.recorder("TCache.Prepare", "LLf"))
        st.internal(RENDER_LIGHTNING, lambda em: (L.rec("RenderLightning", (L.lab(em.r(3)),)), answer("RenderLightning")(em)))
        st.internal(RENDER_RAIN, with_floats("RenderRain"))
        st.internal(RENDER_SNOW, with_floats("RenderSnow"))
        st.internal(NEW_LIGHTNING, with_vectors("NewRenderLightning"))
        st.internal(ADD_SECTION, with_vectors("AddLightningSection"))
        st.internal(GET_NEXT, get_next)
        st.internal(PROJECT_CURRENT, lambda em: L.rec("ProjectCurrentPrimitive", (L.lab(em.r(3)), T.s32(em.r(4)))))
        st.internal(EXECUTE_CURRENT, lambda em: (L.rec("ExecuteCurrentPrimitive", (L.lab(em.r(3)), cfb(em.f(1)))),
                                                 em.set_r(3, T.u32(L.ret("ExecuteCurrentPrimitive", 1)))))
        saved = st.activate(e)
        try:
            with HardwareRounding(e, [(R_RENDER, R_RENDER + 1128)]):
                if op == 1:
                    r3 = T.s32(T.call_ppc(e, R_PREPARE, [rain])[0])
                elif op == 2:
                    r3 = T.s32(T.call_ppc(e, R_RENDER, [rain])[0])
                else:
                    r3 = T.s32(T.call_ppc(e, R_LIGHTNING2, [rain, vfrom, vto])[0])
        finally:
            CL.Stubs.deactivate(e, saved)
            e.fs_side = None
        words = rs(e, rain, RAIN_BYTES // 4)
        for offset in POINTER_FIELDS:
            words[offset // 4] = L.lab(words[offset // 4])
        for offset, n in FLOAT_REGIONS:
            for i in range(n):
                words[offset // 4 + i] = canon(words[offset // 4 + i])
        return dict(result=r3, state=words, log=L.log)


def rain_host(lib, case):
    w, op = case["world"], case["op"]
    lib.tpw_fs_rain.restype = C.c_int64
    lib.tpw_fs_rain.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64), C.c_int32]
    lib.tpw_fs_count.restype = C.c_int32
    capacity = RAIN_BYTES // 4 + 16
    out = (C.c_int64 * capacity)()
    CL.host_begin(lib, {}, case.get("q"))
    result = lib.tpw_fs_rain(op, i64(rain_flat(w)), out, capacity)
    n = lib.tpw_fs_count()
    log = CL.host_end(lib)
    assert n <= capacity
    words = [v & MASK for v in out[:n]]
    for offset, count in FLOAT_REGIONS:
        for i in range(count):
            words[offset // 4 + i] = canon(words[offset // 4 + i])
    for offset in POINTER_FIELDS:
        words[offset // 4] = out[offset // 4]
    return dict(result=C.c_int32(result).value, state=words, log=log)


def rfl(r, special=0.1):
    """A float for a field: mostly moderate, now and then an edge value."""
    if r.random() < special:
        return r.choice([0.0, -0.0, 1.0, -1.0, 0.5, 4.0, -4.0, 8.0, 3.9999, 4.0001, -4.0001, 1e30, -1e30, NAN, INF, -INF, 1e-40,
                         3.4028234663852886e38, 16384.0, 131072.0, 0.3, 100.0])
    return T.f32(r.uniform(-10.0, 10.0))


def rmat(r, special=0.05, scale=10.0):
    m = [fb(T.f32(r.uniform(-scale, scale))) if r.random() > special else fb(rfl(r, 1.0)) for _ in range(16)]
    return m


def rain_world(r, op):
    w = blank_rain_world()
    words = w["words"]
    words[0] = r.choice([0, 1, 2, 3, 1, 2, 4, 5, -1, -2, 0x7FFFFFFF, r.getrandbits(32)])
    words[0x3308] = r.choice([0, 1, 1, -1, 7])
    words[0x330C] = r.choice([0, 2, r.getrandbits(32)])
    words[0x3318] = r.choice([0, 1, 1, 5, -1, 1024])
    for i in range(3):
        words[0x331C + 4 * i] = fb(rfl(r, 0.15))
        words[0x3328 + 4 * i] = fb(rfl(r, 0.15)) if r.random() < 0.7 else fb(T.f32(r.uniform(-4.5, 4.5)))
    words[0x3338] = fb(rfl(r, 0.1))
    words[0x333C] = fb(rfl(r, 0.1))
    words[0x3340] = fb(rfl(r, 0.1))
    words[0x3344] = r.choice([0, 1000, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, r.getrandbits(32), 5000])
    for offset in POINTER_FIELDS:
        words[offset] = r.randint(0, R_ENTRIES)
    words[0x9D98] = r.getrandbits(32)
    words[0xA188] = r.choice([0, 1, 7, 15, 0x7FFFFFFF, r.getrandbits(32)])
    for i in range(16):
        words[0x9DA4 + 4 * i] = r.choice([0, 1, 0xA5A5A5A5, r.getrandbits(32)])
    for i in range(4):
        words[0xA170 + 4 * i] = r.getrandbits(32)
    for en in w["entries"]:
        en.update(flags=r.choice([0, ON_PAGE, ON_PAGE, 0x100000, r.getrandbits(32)]), page=r.randint(0, 8), uoff=rfl(r, 0.15),
                  voff=rfl(r, 0.15), usize=rfl(r, 0.15), vsize=rfl(r, 0.15))
    w["system"] = r.choice([0, 4, 4, 5, 0x03000141, 0xFFFFFFFF, r.getrandbits(32)])
    w["view"] = rmat(r, 0.08, r.choice([1.0, 100.0, 1e5, 1e6]))
    w["inverse"] = rmat(r, 0.05)
    return w


def rain_queues(r):
    return {"ReadClock": [r.choice([0, 100, 5000, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, r.getrandbits(32), 1000, 1499, 1500, 1501])],
            "IsFirstPerson": [r.choice([0, 1, 1, -1, 5])],
            "RenderLightning": [r.choice([1, 1, 1, 0])], "RenderRain": [r.choice([1, 1, 1, 0])],
            "RenderSnow": [r.choice([1, 1, 1, 0])], "GetNextPrimitive": [r.choice([1, 1, 0])],
            "NewRenderLightning": [r.choice([1, 1, 1, 0])], "AddLightningSection": [r.choice([1, 1, 1, 0])],
            "ExecuteCurrentPrimitive": [r.choice([1, 0, 7, -1])]}


def rain_case(r, op):
    w = rain_world(r, op)
    if op == 2:
        # Make the drawing likely: drops on, a type that draws, a clock a little ahead of the last one.
        w["words"][0x3318] = r.choice([0, 1, 1, 1, 5])
        w["words"][0] = r.choice([1, 2, 3, 1, 2, 0, 4, r.getrandbits(32)])
        if r.random() < 0.7:
            w["words"][0x3344] = r.randint(0, 100000)
    if op == 3 and r.random() < 0.7:
        w["words"][0x334C] = r.randint(1, R_ENTRIES)
    return dict(op=op, world=w, q=rain_queues(r))


def edge_rain_prepare():
    r = random.Random(501)
    for f3308 in (0, 1, -1):
        for lightning in (0, 1):
            for drops in (0, 1, -1):
                for kind in (-1, 0, 1, 2, 3, 4, 0x7FFFFFFF):
                    for snow, rain_ in ((0, 0), (1, 2), (2, 3)):
                        w = blank_rain_world()
                        w["words"].update({0: kind, 0x3308: f3308, 0x3318: drops, 0x334C: lightning * 1, 0x3348: snow, 0x3350: rain_})
                        yield dict(op=1, world=w, q={})
    for _ in range(10):
        yield rain_case(r, 1)


def random_rain_prepare(r):
    return rain_case(r, 1)


def edge_render():
    r = random.Random(502)
    identity = [fb(1.0 if i % 5 == 0 else 0.0) for i in range(16)]
    base = blank_rain_world()
    base["view"] = [fb(v) for v in (1, 0, 0, 0, 0, 1, 0, 0, 0.1, 0.2, 0.3, 0, 12.5, -7.25, 3.75, 1)]
    base["inverse"] = identity
    base["words"].update({0: 1, 0x3318: 5, 0x3340: fb(0.5), 0x3344: 1000, 0x3338: fb(2.0), 0x333C: fb(3.0), 0x331C: fb(0.1),
                          0x3320: fb(0.2), 0x3324: fb(0.3), 0x3328: fb(0.5), 0x332C: fb(-0.5), 0x3330: fb(1.0)})

    def one(q=None, system=0, words=None):
        w = dict(base)
        w["words"] = dict(base["words"])
        w["words"].update(words or {})
        w["system"] = system
        queues = {"ReadClock": [1010], "IsFirstPerson": [0]}
        queues.update(q or {})
        return dict(op=2, world=w, q=queues)

    # The clock: a gap of 0, 10, 500, 501, a clock gone back, the wrap of 32 bits.
    for now, last in ((1010, 1000), (1000, 1000), (1500, 1000), (1501, 1000), (900, 1000), (0, 0xFFFFFFFF), (5, 0xFFFFFF00),
                      (0x80000000, 0x7FFFFFFF), (0, 0x80000000), (100000, 0), (3, 4000)):
        yield one({"ReadClock": [now]}, 0, words={0x3344: last})
        yield one({"ReadClock": [now]}, 4, words={0x3344: last})
    # The lightning fails; no drops; the type bits; first person.
    yield one({"RenderLightning": [0]})
    yield one({}, 0, words={0x3318: 0})
    for kind in (0, 1, 2, 3, 4, 5, 6, 7, 8, 0xFFFFFFFF):
        for first in (0, 1):
            for ok in (1, 0):
                yield one({"IsFirstPerson": [first], "RenderRain": [ok], "RenderSnow": [ok]}, 0, words={0: kind})
    # The camera: the position of the view matrix at the edges of the wrap and of the conversion.
    for position in ((0, 0, 0), (1, 1, 1), (-1, -1, -1), (7.99, 8.01, -8.0), (3.9999, 4.0, 4.0001), (1e5, -1e5, 1e6), (1e9, -1e9, 3e38),
                     (-3e38, 3e38, INF), (NAN, -INF, 0.0), (0.25, 0.5, 0.75), (131071 / 16384, 131072 / 16384, 131073 / 16384),
                     (-4.0, -4.0, -4.0), (-4.00006, -4.0001, -3.99994), (16384.0, 32768.0, -16384.0)):
        for reach in (0.5, 1.0, 0.3, -1.0, 0.0, 4.0, 1e-3, 100.0):
            w = dict(base)
            w["view"] = list(base["view"])
            w["view"][12:15] = [fb(v) for v in position]
            w["words"] = dict(base["words"])
            w["words"][0x3340] = fb(reach)
            yield dict(op=2, world=w, q={"ReadClock": [1010], "IsFirstPerson": [0]})
    # The wind: each axis through the wrap at both ends, both velocities.
    for value in (-4.0, -4.0001, -3.9999, 4.0, 4.0001, 3.9999, -100.0, 100.0, NAN, INF, -INF, 7.9999, -7.9999, 0.0):
        for kind in (1, 2):
            yield one(None, 0, words={0: kind, 0x3328: fb(value), 0x332C: fb(value), 0x3330: fb(value),
                                  0x331C: fb(1.0), 0x3320: fb(1.0), 0x3324: fb(1.0), 0x3338: fb(1.0), 0x333C: fb(1.0),
                                  0x9D98: fb(0.0)})
    for step in (-5.0, -1.0, 0.5, 3.0, 1e30, NAN, INF, -INF):
        yield one({"ReadClock": [1500]}, 0, words={0x331C: fb(step), 0x3320: fb(step), 0x3324: fb(step), 0x3338: fb(1.0)})
    for _ in range(20):
        yield rain_case(r, 2)


def random_render(r):
    return rain_case(r, 2)


def edge_lightning():
    r = random.Random(503)
    base = blank_rain_world()
    base["words"].update({0x330C: 2, 0xA188: 9})
    for flags in (0, ON_PAGE, 0x100000, ON_PAGE | 0x100000, 0xFFFFFFFF):
        for texture in (0, 1, 2):
            for page in (0, 3):
                w = dict(base)
                w["words"] = dict(base["words"])
                w["words"][0x334C] = texture
                w["entries"] = [dict(flags=flags, page=page, uoff=0.25, voff=0.5, usize=0.125, vsize=0.0625),
                                dict(flags=ON_PAGE, page=5, uoff=1.5, voff=-2.0, usize=NAN, vsize=INF), dict(flags=0, page=0, uoff=0, voff=0, usize=0, vsize=0)]
                yield dict(op=3, world=w, q={})
    for q in ({"GetNextPrimitive": [0]}, {"NewRenderLightning": [0]}, {"AddLightningSection": [0]},
              {"ExecuteCurrentPrimitive": [0]}, {"ExecuteCurrentPrimitive": [-7]}, {"ExecuteCurrentPrimitive": [123456]}):
        w = dict(base)
        w["words"] = dict(base["words"])
        w["words"][0x334C] = 1
        w["entries"] = [dict(flags=ON_PAGE, page=2, uoff=0.5, voff=0.5, usize=0.25, vsize=0.25)] + base["entries"][1:]
        yield dict(op=3, world=w, q=q)
    for _ in range(20):
        yield rain_case(r, 3)


def random_lightning(r):
    return rain_case(r, 3)


TESTS = [
    FnTest("COpenGLTexture::Download", download_ppc, download_host, edge=edge_download, rnd=random_download),
    FnTest("COpenGLDrawPrimitive::~COpenGLDrawPrimitive", destroy_ppc, destroy_host, edge=edge_destroy, rnd=random_destroy),
    FnTest("CTextureCache::Entry_Specific_Compress", cache_ppc, cache_host, edge=edge_esc, rnd=random_esc),
    FnTest("CTextureCache::BalanceCache_ClearTexture", cache_ppc, cache_host, edge=edge_clear, rnd=random_clear),
    FnTest("CParticle::Prepare", particle_ppc, particle_host, edge=edge_prepare, rnd=random_prepare),
    FnTest("CParticle::PlotParticles", particle_ppc, particle_host, edge=edge_plot, rnd=random_plot),
    FnTest("CRain::Prepare", rain_ppc, rain_host, edge=edge_rain_prepare, rnd=random_rain_prepare),
    FnTest("CRain::Render", rain_ppc, rain_host, edge=edge_render, rnd=random_render),
    FnTest("CRain::RenderLightning2", rain_ppc, rain_host, edge=edge_lightning, rnd=random_lightning),
]
