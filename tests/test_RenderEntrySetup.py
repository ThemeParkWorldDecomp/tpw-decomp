"""SRenderListEntry::GetFaceSetBlendMode and SetupPrimitive (src/tb_renderentry_setup.cpp), original
versus translation.

The one callee that is a fake on both sides is CDrawPrimitive::GetNextPrimitive (tests/unit_RenderEntrySetup.cpp
has the host's): it records every argument and answers the case's queue "Prim" (0: NULL, else the one
primitive of the case, whose bytes are all 0xa5 before the call, so a stray write shows). The real one reaches
into the render memory's tables and has its own test (tests/test_PrimitiveBuffer.py). CTextureEntry::GetBlendingMode
and StoreAsWrapping run as they are on both sides (the unit library holds src/tb_textureentry.cpp), and so do
CsTexture::GetTextureEntry and CTextureEntry::GetTexture (inline in the translation, real on the PowerPC side).

A case is a world: the render list entry's flags, the node (its flags, its face sets with their four flag bytes
and texture record, the texture records), the texture objects with their frames, the texture entries (flags,
page, mF38, mF40), the display's and texture settings' flags and the renderer. SetupPrimitive is called on one
face set; GetFaceSetBlendMode on one face set and one texture entry, which is the one the face set's record gives
as RenderByTexture finds it or, in the random cases, any entry or none. Compared: the results, *extra, the call
log, the primitive's flags and mF34, and that no other byte of the primitive changed.

Not tested: a face set number outside the node's face sets, a frame beyond the texture object's entries.
"""
import contextlib
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_RenderEntrySetup"
MASK = 0xffffffff

SETUP, BLEND, GET_NEXT = 0x10026044, 0x10025e8c, 0x10006a58
# CTextureEntry::GetBlendingMode, StoreAsWrapping, CsTexture::GetTextureEntry, CTextureEntry::GetTexture:
# real on the PowerPC side (an earlier module may have left a stand-in on the unit's own two).
REAL = [SETUP, BLEND, 0x100410a4, 0x10041848, 0x100171ec, 0x10025e40]

DRAW_VAR, DISPLAY, SETTINGS, RENDERER = 0x10052b3c, 0x10165634, 0x10165fc8, 0x10165f9c

TEXTURES, FRAMES, PAGES = 6, 4, 8
ENTRY_SIZE, RECORD_SIZE = 0x5c, 0x38
PAGE_BASE = 0x7a000000          # a page's address on the PowerPC side (never dereferenced)
F38_ADDRESS = 0x7a00ff00        # what a non-null mF38 holds there
POISON = 0xa5a5a5a5
EXTRA_BEFORE = 0x5a5a5a5a
LABEL_DRAW, LABEL_PRIMITIVE, LABEL_PAGE = 1, 5, 10

# Flags the two functions read.
DISPLAY_FOG, DISPLAY_SPECULAR = 0x2000, 0x400
NODE_BITS = [0x2, 0x2000000, 0x8000000, 0x40000, 0x80000]
ENTRY_BITS = [0x10000000, 0x4000000, 0x8000000, 0x2000000]
TEXTURE_BITS = [0x100, 0x200, 0x10000, 0x20000, 0x100000, 0x400, 0x1000, 0x4000000]
SETTINGS_FRAMES = 0x40
RECORD_PLAIN = 0x800
FACE_SET_BLEND = 0x2


def ws(e, a, values):
    e.write(a, struct.pack(">%dI" % len(values), *[v & MASK for v in values]))


# ----------------------------------------------------------------- the node's image


class Image:
    """The node and the records it points to, in the file's layout: described once, rendered
    big-endian with absolute addresses (PowerPC) or little-endian with offsets in the image that
    the host turns into Ptr32s (the `relocs`). The first 16 bytes are padding, so no pointer
    target is at offset 0."""

    def __init__(self):
        self.size = 0x10
        self.fields, self.pointers = [], []

    def reserve(self, n, align=4):
        self.size = (self.size + align - 1) & ~(align - 1)
        at = self.size
        self.size += n
        return at

    def put(self, at, fmt, *values):
        self.fields.append((at, fmt, values))

    def ptr(self, at, target):
        self.pointers.append((at, target))

    def render(self, big, base=0):
        out = bytearray(b"\xa5" * self.size)
        e = ">" if big else "<"
        for at, fmt, values in self.fields:
            struct.pack_into(e + fmt, out, at, *values)
        relocs = []
        for at, target in self.pointers:
            struct.pack_into(e + "I", out, at, 0 if target is None else (base + target if big else target))
            relocs.append(at)
        return bytes(out), relocs


def build_image(case):
    """The image of a case, with the offsets of the node, the face sets and the texture records."""
    img = Image()
    sets, records = case["sets"], case["records"]
    node = img.reserve(0xa0)
    sets_at = img.reserve(0x10 * len(sets))
    records_at = img.reserve(8 * max(1, len(records)))
    img.put(node, "I", case["node"])
    img.put(node + 0x5a, "H", len(sets))
    img.ptr(node + 0x6c, sets_at)
    for i, (record, flag_bytes) in enumerate(sets):
        a = sets_at + 0x10 * i
        img.ptr(a, records_at + 8 * record if record >= 0 else None)
        img.put(a + 0xc, "4B", *flag_bytes)
    for i, (f00, frame, texture) in enumerate(records):
        img.put(records_at + 8 * i, "HH", f00, frame)   # the texture pointer is set per side
    return img, node, sets_at, records_at


# ----------------------------------------------------------------- the PowerPC side


def page_address(label):
    return 0 if label == 0 else PAGE_BASE + 0x100 * label


@contextlib.contextmanager
def guarded(e):
    """The engine globals a case overwrites, put back afterwards (other modules share them)."""
    regions = [(DRAW_VAR, 4), (DISPLAY, 4), (SETTINGS, 4), (RENDERER, 4)]
    saved = [(a, e.read(a, n)) for a, n in regions]
    try:
        yield
    finally:
        for a, data in saved:
            e.write(a, data)


class PpcWorld:
    """Memory, labels and the fake of one case on the PowerPC side."""

    def __init__(self, e, case):
        self.e, self.case = e, case
        fixed = {page_address(k): LABEL_PAGE + k for k in range(1, PAGES + 1)}
        img, node, self.sets_at, records_at = build_image(case)
        self.image = e.alloc(img.size)
        data, _ = img.render(True, self.image)
        e.write(self.image, data)
        entries = case["entries"]
        self.entries = e.alloc(ENTRY_SIZE * max(1, len(entries)))
        for k, (flags, page, f38, f40) in enumerate(entries):
            a = self.entries + ENTRY_SIZE * k
            ws(e, a + 8, [flags])
            ws(e, a + 0x18, [page_address(page)])
            ws(e, a + 0x38, [F38_ADDRESS if f38 else 0])
            ws(e, a + 0x40, [f40])
        textures = []
        for t in range(TEXTURES):
            array = e.alloc(4 * FRAMES)
            ws(e, array, [self.entry_address(k) for k in case["textures"][t]])
            a = e.alloc(0x10)
            ws(e, a, [POISON, POISON, array, POISON])
            textures.append(a)
        for i, (f00, frame, texture) in enumerate(case["records"]):
            ws(e, self.image + records_at + 8 * i + 4, [textures[texture] if texture >= 0 else 0])
        self.this = e.alloc(0x14)
        ws(e, self.this, [POISON, self.image + node, case["flags"], POISON, POISON])
        self.primitive = e.alloc(RECORD_SIZE, b"\xa5" * RECORD_SIZE)
        self.extra = e.alloc(4)
        ws(e, self.extra, [EXTRA_BEFORE])
        self.draw = e.alloc(0x30, b"\xa5" * 0x30)
        fixed.update({self.draw: LABEL_DRAW, self.primitive: LABEL_PRIMITIVE})
        ws(e, DRAW_VAR, [self.draw])
        ws(e, DISPLAY, [case["display"]])
        ws(e, SETTINGS, [case["settings"]])
        ws(e, RENDERER, [case["renderer"]])
        self.L = CL.PpcLog(fixed, case["q"])
        e.cl = self.L

    def entry_address(self, k):
        return self.entries + ENTRY_SIZE * k if k >= 0 else 0

    def stubs(self):
        st = CL.Stubs()
        st.real = set(REAL)
        st.internal(GET_NEXT, self.get_next)
        return st

    def get_next(self, e):
        L = self.L
        L.rec("GetNextPrimitive", [L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), e.r(6) & MASK, T.s32(e.r(7)),
                                   L.lab(e.r(8))])
        e.set_r(3, self.primitive if L.ret("Prim", 1) != 0 else 0)

    def primitive_state(self):
        """The flags, mF34 and the number of other bytes that changed."""
        data = self.e.read(self.primitive, RECORD_SIZE)
        strays = sum(1 for i, b in enumerate(data) if not (i < 4 or 0x34 <= i < 0x38) and b != 0xa5)
        return [T.s32(struct.unpack(">I", data[:4])[0]), T.s32(struct.unpack(">I", data[0x34:0x38])[0]), strays]


def ppc_setup(e, case):
    with guarded(e):
        w = PpcWorld(e, case)
        st = w.stubs()
        saved = st.activate(e)
        try:
            r3 = T.call_ppc(e, SETUP, [w.this, case["faceset"], w.extra])[0]
            return {"ret": w.L.lab(r3), "extra": T.s32(e.u32(w.extra)), "primitive": w.primitive_state(),
                    "log": w.L.log}
        finally:
            st.deactivate(e, saved)


def ppc_blend(e, case):
    with guarded(e):
        w = PpcWorld(e, case)
        st = w.stubs()
        saved = st.activate(e)
        try:
            face_set = w.image + w.sets_at + 0x10 * case["faceset"]
            r3 = T.call_ppc(e, BLEND, [w.this, face_set, w.entry_address(case["entry"])])[0]
            return {"ret": r3 & MASK, "log": w.L.log}
        finally:
            st.deactivate(e, saved)


# ----------------------------------------------------------------- the host side


def array(ctype, values):
    return (ctype * max(1, len(values)))(*values)


def host_world(lib, case):
    """Builds the case's world in the unit library; returns the offsets of the face sets."""
    lib.tpw_rs_pointer.restype = C.c_void_p
    lib.tpw_rs_pointer.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_rs_image.argtypes = [C.c_char_p, C.c_int32, C.POINTER(C.c_uint32), C.c_int32]
    lib.tpw_rs_entries.argtypes = [C.POINTER(C.c_uint32), C.c_int32]
    lib.tpw_rs_textures.argtypes = [C.POINTER(C.c_int32), C.c_int32]
    lib.tpw_rs_record_texture.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_rs_globals.argtypes = [C.c_uint32, C.c_uint32, C.c_int32]
    lib.tpw_rs_entry.argtypes = [C.c_int32, C.c_uint32]
    img, node, sets_at, records_at = build_image(case)
    data, relocs = img.render(False)
    lib.tpw_rs_image(data, len(data), array(C.c_uint32, relocs), len(relocs))
    lib.tpw_rs_entries(array(C.c_uint32, [w & MASK for entry in case["entries"] for w in entry]), len(case["entries"]))
    lib.tpw_rs_textures(array(C.c_int32, [k for t in case["textures"] for k in t]), len(case["textures"]))
    for i, (f00, frame, texture) in enumerate(case["records"]):
        lib.tpw_rs_record_texture(records_at + 8 * i, texture)
    lib.tpw_rs_globals(case["display"], case["settings"], T.s32(case["renderer"]))
    lib.tpw_rs_entry(node, case["flags"])
    fixed = {LABEL_DRAW: lib.tpw_rs_pointer(0, 0), LABEL_PRIMITIVE: lib.tpw_rs_pointer(2, 0)}
    for k in range(1, PAGES + 1):
        fixed[LABEL_PAGE + k] = lib.tpw_rs_pointer(1, k)
    CL.host_begin(lib, fixed, case["q"])
    return sets_at


def host_setup(lib, case):
    host_world(lib, case)
    lib.tpw_rs_setup.restype = C.c_int32
    lib.tpw_rs_setup.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int32)]
    lib.tpw_rs_primitive.argtypes = [C.POINTER(C.c_int32)]
    out = (C.c_int32 * 3)()
    ret = lib.tpw_rs_setup(case["faceset"], EXTRA_BEFORE, out)
    log = CL.host_end(lib)
    primitive = (C.c_int32 * 3)()
    lib.tpw_rs_primitive(primitive)
    return {"ret": ret, "extra": out[0], "primitive": list(primitive), "log": log}


def host_blend(lib, case):
    sets_at = host_world(lib, case)
    lib.tpw_rs_blend.restype = C.c_int32
    lib.tpw_rs_blend.argtypes = [C.c_int32, C.c_int32]
    ret = lib.tpw_rs_blend(sets_at + 0x10 * case["faceset"], case["entry"])
    return {"ret": ret & MASK, "log": CL.host_end(lib)}


# ----------------------------------------------------------------- cases
#
# A case is a dict:
#   flags                       the render list entry's flags
#   node                        the node's flags
#   sets                        its face sets: (record number (-1: none), the four flag bytes)
#   records                     the texture records: (mF00, frame mF02, texture object (-1: none))
#   textures                    per texture object (TEXTURES of them), FRAMES entry numbers (-1: NULL)
#   entries                     (flags, page (0: none), mF38 set, mF40)
#   display, settings, renderer the globals
#   faceset                     the face set both functions work on
#   entry                       GetFaceSetBlendMode's texture entry (-1: NULL)
#   q                           queues: "Prim" is GetNextPrimitive's answer (0: NULL)


def derived_entry(case):
    """The texture entry RenderByTexture and SetupPrimitive find for the case's face set."""
    record = case["sets"][case["faceset"]][0]
    if record < 0:
        return -1
    f00, frame, texture = case["records"][record]
    if texture < 0:
        return -1
    return case["textures"][texture][frame if case["settings"] & SETTINGS_FRAMES else 0]


def finish(case):
    """Fills in what a case leaves out: the entry GetFaceSetBlendMode gets is the face set's own."""
    case.setdefault("entry", derived_entry(case))
    return case


# The four entries of the main texture object: the first is the one a case varies, the others
# differ in everything that shows (page, flags, mF40), so a wrong frame is seen.
OTHERS = [(0x1000, 2, 0, 0x22222222), (0x1400, 3, 1, 0x33333333), (0x1400, 4, 0, 0x44444444)]


def one(node=0, flags=0, display=0, settings=0, renderer=1, set_bytes=(0, 0, 0, 0), record=(0, 0),
        texture=(0x1000, 1, 0, 0x11111111), has_record=True, has_texture=True, prim=1, **more):
    """A world with one face set. Its record (mF00, frame) names texture object 0 (unless has_record / has_texture
    say otherwise), whose frames are `texture` (flags, page, mF38, mF40) and the three OTHERS; no entry at all when
    `texture` is None."""
    frames = [0, 1, 2, 3] if texture is not None else [-1, -1, -1, -1]
    case = {
        "flags": flags, "node": node,
        "sets": [(0 if has_record else -1, list(set_bytes))],
        "records": [(record[0], record[1], 0 if has_texture else -1)],
        "textures": [frames] + [[-1] * FRAMES for _ in range(TEXTURES - 1)],
        "entries": [texture if texture is not None else (0, 0, 0, 0)] + OTHERS,
        "display": display, "settings": settings, "renderer": renderer,
        "faceset": 0, "q": {"Prim": [prim]},
    }
    case.update(more)
    return finish(case)


def edge_cases():
    yield one()
    # the display's switches
    for display in (0, 0x2000, 0x400, 0x2400, MASK, MASK & ~0x2400, 0x7ffffbff):
        yield one(display=display)
    # the node's flags, one by one and together; the pool and the vertex format follow
    for node in (0, 2, 0x2000000, 0x8000000, 0x40000, 0x80000, 0xc0000, 0x2000002, MASK, MASK & ~0x0a0c0002,
                 0x0a0c0002 ^ MASK, 0x100000, 0x20000, 0x1000000, 0x7ffbffff):
        yield one(node=node)
    # the render list entry's flags
    for flags in (0, 0x10000000, 0x4000000, 0x8000000, 0x2000000, 0x14000000, 0x18000000, 0x1c000000, 0x1e000000,
                  MASK, MASK & ~0x1e000000, 0xe1ffffff):
        yield one(flags=flags)
    # the face set's flag bytes: only bit 1 of the first counts
    for set_bytes in ((0, 0, 0, 0), (1, 0, 0, 0), (2, 0, 0, 0), (3, 0, 0, 0), (0xfd, 0xff, 0xff, 0xff), (0xff,) * 4,
                      (0, 0xff, 0xff, 0xff), (2, 2, 2, 2)):
        yield one(set_bytes=set_bytes)
    # the texture record: none, a record without a texture, the plain blending bit, frames with and without setting 0x40
    yield one(has_record=False)
    yield one(has_texture=False)
    yield one(has_record=False, texture=None)
    for f00 in (0, 0x800, 0xf7ff, 0xffff, 0x7ff, 0x1000):
        yield one(record=(f00, 0))
    for frame in range(FRAMES):
        for settings in (0, 0x40, MASK, MASK & ~0x40):
            yield one(record=(0, frame), settings=settings)
            yield one(record=(0x800, frame), settings=settings, flags=0x4000000)
    # no entry at all, and an entry slot that is NULL
    yield one(texture=None)
    yield one(texture=None, node=0x2000000)
    yield one(texture=None, display=0x2400, flags=0x1e000000, set_bytes=(2, 0, 0, 0))
    # the texture entry: every flag the functions read, with and without a page and mF38
    for flags in (0, 0x1000, 0x400, 0x1400, 0x100, 0x200, 0x300, 0x10000, 0x20000, 0x30000, 0x100000, 0x4000000,
                  0x1000 | 0x30000 | 0x100000, MASK, MASK & ~0x4000000, 0x7fffffff ^ 0x30300, 0x30300, 0x4030300):
        for f38 in (0, 1):
            for page in (0, 3):
                yield one(texture=(flags, page, f38, 0x12345678))
    # the wrapping kinds care about the renderer
    for renderer in (0, 1, 2, 3, 4, 5, -1, 0x80000004, 0x100):
        for flags in (0x10000, 0x20000, 0x1000, 0x30300, 0x4010100):
            yield one(texture=(flags, 1, 0, 0), renderer=renderer)
    # mF40 goes to the primitive as it is, and -1 without an entry
    for f40 in (0, 1, MASK, 0x80000000, 0x7fffffff, 0xdeadbeef):
        yield one(texture=(0x1000, 1, 0, f40))
    # a page that is not on the cache: flag 0x1000 clear
    yield one(texture=(0, 5, 0, 7))
    yield one(texture=(0x1000, 0, 0, 7))
    # the render memory has no record: NULL, *extra and the primitive untouched
    for texture in ((0x1000, 1, 0, 5), (0x1400, 1, 0, 5), None):
        for node in (0, 0x2000000, 0xc0000):
            yield one(texture=texture, node=node, prim=0, flags=0x2000000)
    # everything on at once
    yield one(node=0x0a0c0002, flags=0x1e000000, display=0x2400, set_bytes=(2, 0, 0, 0), settings=0x40,
              record=(0, 2), texture=(0x4130400, 1, 0, 0x99), renderer=1)
    yield one(node=0x0a0c0002, flags=0x1e000000, display=0x2400, set_bytes=(2, 0, 0, 0), settings=0x40,
              record=(0x800, 2), texture=(0x4130500, 1, 1, 0x99), renderer=4)
    yield from several_face_sets()
    yield from given_entries()


def several_face_sets():
    """A node of four face sets that differ in everything that shows: the number picks one."""
    for node in (0, 0x40000, 0x2000000):
        base = one(node=node, flags=0x4000000, settings=0x40, display=0x2000)
        base["sets"] = [(0, [0, 0, 0, 0]), (1, [2, 0, 0, 0]), (2, [0, 0, 0, 0]), (-1, [2, 9, 9, 9])]
        base["records"] = [(0, 0, 0), (0x800, 1, 1), (0, 3, 1)]
        base["textures"][1] = [3, 2, 1, 0]
        base["entries"] = [(0x1000, 1, 0, 0x100), (0x1400, 2, 0, 0x200), (0x1400, 3, 1, 0x300), (0x10100, 4, 0, 0x400)]
        for k in range(4):
            case = dict(base, faceset=k)
            del case["entry"]
            yield finish(case)


def given_entries():
    """GetFaceSetBlendMode with an entry the face set's record does not give: none with a record, one without."""
    for entry in (-1, 0, 1, 2, 3):
        for has_record in (True, False):
            for f00 in (0, 0x800):
                case = one(has_record=has_record, record=(f00, 0), display=0x2400, node=0x8000002,
                           texture=(0x4130400, 1, 0, 3))
                case["entry"] = entry
                yield case


def noise(r, bits):
    """Random bits outside the ones the functions read."""
    return r.getrandbits(32) & ~sum(bits) & MASK


def some(r, density, bits):
    """The bits that came up at this density."""
    return sum(b for b in bits if r.random() < density)


def random_case(r):
    density = r.choice([0.1, 0.5, 0.5, 0.9])
    sets_count = r.randint(1, 4)
    records_count = r.randint(0, 4)
    entries_count = r.randint(1, 6)
    entries = [(some(r, density, TEXTURE_BITS) | noise(r, TEXTURE_BITS) if r.random() < 0.7 else some(r, density, TEXTURE_BITS),
                r.randint(0, PAGES), r.randint(0, 1), r.getrandbits(32)) for _ in range(entries_count)]
    textures = [[r.randint(-1, entries_count - 1) for _ in range(FRAMES)] for _ in range(TEXTURES)]
    records = [(some(r, density, [RECORD_PLAIN]) | (r.getrandbits(16) & ~RECORD_PLAIN), r.randint(0, FRAMES - 1),
                r.randint(-1, TEXTURES - 1)) for _ in range(records_count)]
    sets = [(r.randint(-1, records_count - 1), [some(r, density, [FACE_SET_BLEND]) | (r.getrandbits(8) & ~FACE_SET_BLEND),
                                                 r.getrandbits(8), r.getrandbits(8), r.getrandbits(8)])
            for _ in range(sets_count)]
    case = {
        "flags": some(r, density, ENTRY_BITS) | noise(r, ENTRY_BITS),
        "node": some(r, density, NODE_BITS) | noise(r, NODE_BITS),
        "sets": sets, "records": records, "textures": textures, "entries": entries,
        "display": some(r, density, [DISPLAY_FOG, DISPLAY_SPECULAR]) | noise(r, [DISPLAY_FOG, DISPLAY_SPECULAR]),
        "settings": some(r, 0.5, [SETTINGS_FRAMES]) | noise(r, [SETTINGS_FRAMES]),
        "renderer": r.choice([4, 4, 1, 3, 0, 5, 2, -1, r.getrandbits(32) - 0x80000000]),
        "faceset": r.randrange(sets_count),
        "q": {"Prim": [0 if r.random() < 0.1 else 1]},
    }
    if r.random() < 0.3:
        case["entry"] = r.randint(-1, entries_count - 1)
    return finish(case)


TESTS = [
    FnTest("SRenderListEntry::GetFaceSetBlendMode", ppc_blend, host_blend, edge=lambda: list(edge_cases()),
           rnd=random_case),
    FnTest("SRenderListEntry::SetupPrimitive", ppc_setup, host_setup, edge=lambda: list(edge_cases()),
           rnd=random_case),
]
