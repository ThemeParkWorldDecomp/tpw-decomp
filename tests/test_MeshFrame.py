"""CMesh's per-frame members and two small engine functions (engine library;
src/engine/DirectDraw.cpp, src/engine/Mesh.cpp, src/engine/RenderList.cpp): CMesh::Prepare 0x1001df78, Preprocess 0x1001dccc, PreprocessTextures
0x1001db1c, StaticCalculateLights 0x1001e26c, ColorVertices 0x10003570 and DotProduct
0x10025cac.

A case lays a small mesh file out as one image of bytes, on both sides: the PowerPC side in
emulator memory (big-endian words, absolute addresses), the host in an arena block (little-endian,
arena offsets in the pointer fields). The image holds the header, four texture records, four
texture objects with their entries, the nodes (tree links, flags, area bytes, box, matrix), their
face sets, the mesh words the nodes keep their vertex colours in, four map list heads and four
spare matrices. Objects in it are named by their offset in the image. The CMesh itself, the
engine's singletons and the globals the code reads are laid out around it by the case.

Everything the six functions call outside themselves is a fake on both sides that records its
arguments: the math unit (MultMatrix writes a + 3 b + index as words), the render list's AddEntry,
the texture cache's Prepare and Remove, a texture entry's Lock, the light manager's AddLights,
PrepareLighting and CalcNodeVertexColors, a map list's PrepareLights (which also moves the light
chain), the height field's CheckGamutArea, the clipping object's CheckBoundingBox (which leaves
a depth in its second word), the mesh's callback and the assertion (which ends the call on
both sides: the original would abort). CsTexture::GetTextureEntry runs as the original. A
script is a list of steps run on one mesh; after each the result and the state of the mesh, the
image and the engine data are compared, and at the end the log of fake calls.

Coverage and mutation results: NOTES.md, "engine / the mesh object's per-frame members".
"""
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
SHIM_LIBRARY = "unit_MeshFrame"
MASK = 0xFFFFFFFF

# The unit (run as the original).
PREPARE, PREPROCESS, TEXTURES, LIGHTS = 0x1001DF78, 0x1001DCCC, 0x1001DB1C, 0x1001E26C
COLOR_VERTICES, DOT_PRODUCT, GET_ENTRY = 0x10003570, 0x10025CAC, 0x100171EC
UNIT = [PREPARE, PREPROCESS, TEXTURES, LIGHTS, COLOR_VERTICES, DOT_PRODUCT, GET_ENTRY]
# What they call (fakes).
PREPARE_LIGHTS, ADD_LIGHTS, PREPARE_LIGHTING, CALC_COLOURS = 0x10017454, 0x100148C4, 0x100146D4, 0x10014B28
CHECK_GAMUT, CHECK_BOX, ADD_ENTRY = 0x1000E760, 0x1003F4D4, 0x100262E0
CACHE_PREPARE, CACHE_REMOVE, ENTRY_LOCK = 0x10032DDC, 0x10034610, 0x10041738

# Engine data.
DISPLAY, SYSTEM, SETTINGS = 0x10165634, 0x10165F80, 0x10165FC8
MESH_DELAY, ENTRY_FLAGS, CHAIN = 0x100B0100, 0x100B0504, 0x100AF194
FINAL_PTR, LIGHT_MATRIX, MATH_PTR, MAPWHO_PTR = 0x100AFF70, 0x100B00BC, 0x100AFE94, 0x100AF618
HFIELD_PTR, CLIP_PTR, RLIST_PTR, TCACHE_PTR, LM_PTR = 0x100AF11C, 0x102B3638, 0x1016562C, 0x10173184, 0x100AF1B8
STATIC_GUARD = 0x100524A4   # the guard byte and the matrix of operator*'s static result (Prepare's)
SAVED = [(DISPLAY, 8), (DISPLAY + 0x80, 8), (DISPLAY + 0x928, 16), (SYSTEM, 0x20), (SETTINGS, 4),
         (MESH_DELAY, 0x408), (ENTRY_FLAGS, 4), (CHAIN, 4), (FINAL_PTR, 4), (LIGHT_MATRIX, 64),
         (MATH_PTR, 4), (MAPWHO_PTR, 4), (HFIELD_PTR, 4), (CLIP_PTR, 4), (RLIST_PTR, 4),
         (TCACHE_PTR, 4), (LM_PTR, 4), (STATIC_GUARD, 0x48)]

# Labels of the singletons (the same numbers as the enum in tests/unit_MeshFrame.cpp).
L_MATH, L_RLIST, L_CACHE, L_LM, L_HFIELD, L_CLIP = 1, 2, 3, 4, 5, 6
WHERE_MESH, WHERE_OTHER, ABORTED = -2, -1, -777

NTEX, NOBJ, NENT = 4, 4, 6
MESH_BYTES = 0x15C


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


NAN, INF, NEG0 = 0x7FC00000, fb(float("inf")), 0x80000000


# ----------------------------------------------------------------- the image


def layout(spec):
    """Offsets of everything in the image (the same on both sides). Offset 0 is never used."""
    nodes = spec["nodes"]
    off = 0x10
    out = {}

    def take(n):
        nonlocal off
        o = off
        off += (n + 15) & ~15
        return o

    out["header"] = take(0xB8)
    out["textures"] = take(8 * NTEX)
    out["slots"] = [take(0x20) for _ in range(NOBJ)]
    out["entries"] = [take(0x60) for _ in range(NENT)]
    out["nodes"] = take(0xA0 * max(1, len(nodes)))
    out["sets"] = [take(0x10 * max(1, len(n["sets"]))) for n in nodes]
    cursor, index = 0, []
    for n in nodes:
        index.append(cursor if n["words"] else None)
        if n["words"]:
            cursor += n["f5e"] + 2
    out["word_index"] = index
    out["word_count"] = cursor + 4
    out["words"] = take(4 * out["word_count"])
    out["lists"] = [take(0x10) for _ in range(4)]
    out["mats"] = [take(0x40) for _ in range(4)]
    out["size"] = off
    return out


def node_at(lay, i):
    return lay["nodes"] + 0xA0 * i


def image(spec, order, ptr):
    """The image's bytes for one side. order: its byte order; ptr(offset): its pointer field's
    value for that offset of the image."""
    lay = layout(spec)
    data = bytearray(lay["size"])

    def put(o, fmt, v):
        struct.pack_into(order + fmt, data, o, v)

    put(lay["header"] + 0x30, "I", spec["header_flags"])
    if spec["nodes"]:
        put(lay["header"] + 0x78, "I", ptr(lay["nodes"]))
    for k, t in enumerate(spec["textures"]):
        o = lay["textures"] + 8 * k
        put(o, "H", t["f00"])
        put(o + 2, "H", t["f02"])
        put(o + 4, "I", ptr(lay["slots"][t["obj"]]) if t["obj"] is not None else 0)
    for i, n in enumerate(spec["nodes"]):
        o = node_at(lay, i)
        put(o, "I", n["flags"])
        put(o + 8, "I", ptr(node_at(lay, n["next"])) if n["next"] >= 0 else 0)
        put(o + 0xC, "I", ptr(node_at(lay, n["child"])) if n["child"] >= 0 else 0)
        for k, w in enumerate(n["matrix"]):
            put(o + 0x10 + 4 * k, "I", w)
        put(o + 0x5A, "H", len(n["sets"]))
        put(o + 0x5E, "H", n["f5e"])
        put(o + 0x6C, "I", ptr(lay["sets"][i]))
        for k, w in enumerate(n["box"]):
            put(o + 0x78 + 4 * k, "I", w)
        for k, b in enumerate(n["area"]):
            struct.pack_into("b", data, o + 0x90 + k, b)
        put(o + 0x98, "I", ptr(lay["textures"] + 8 * n["f98"]) if n["f98"] is not None else 0)
        if n["words"]:
            base = lay["words"] + 4 * lay["word_index"][i]
            put(o + 0x9C, "I", ptr(base))
            put(base, "I", n["stamp"])
            for k in range(1, n["f5e"] + 2):
                put(base + 4 * k, "I", 0x77770000 + 16 * i + k)
        for k, s in enumerate(n["sets"]):
            put(lay["sets"][i] + 16 * k, "I", ptr(lay["textures"] + 8 * s) if s is not None else 0)
    for k, m in enumerate(spec["mats"]):
        for j, w in enumerate(m):
            put(lay["mats"][k] + 4 * j, "I", w)
    return bytes(data), lay


def decode(data, order, unptr, lay, spec):
    """Every field of the image the code can read or write, as a list of numbers (pointer
    fields as offsets in the image)."""
    def g(o, fmt):
        return struct.unpack_from(order + fmt, data, o)[0]

    out = [g(lay["header"] + 0x30, "I"), unptr(g(lay["header"] + 0x78, "I"))]
    for k in range(NTEX):
        o = lay["textures"] + 8 * k
        out += [g(o, "H"), g(o + 2, "H"), unptr(g(o + 4, "I"))]
    for i, n in enumerate(spec["nodes"]):
        o = node_at(lay, i)
        out += [g(o, "I"), unptr(g(o + 8, "I")), unptr(g(o + 0xC, "I")), g(o + 0x5A, "H"), g(o + 0x5E, "H"),
                unptr(g(o + 0x6C, "I")), unptr(g(o + 0x98, "I")), unptr(g(o + 0x9C, "I"))]
        out += [g(o + 0x10 + 4 * k, "I") for k in range(16)] + [g(o + 0x78 + 4 * k, "I") for k in range(6)]
        out += list(data[o + 0x90:o + 0x94])
        out += [unptr(g(lay["sets"][i] + 16 * k, "I")) for k in range(len(n["sets"]))]
    out += [g(lay["words"] + 4 * k, "I") for k in range(lay["word_count"])]
    for o in lay["lists"]:
        out += [g(o + 4 * k, "I") for k in range(4)]
    for o in lay["mats"]:
        out += [g(o + 4 * k, "I") for k in range(16)]
    for o in lay["entries"]:
        out += [g(o + 4 * k, "I") for k in range(8)]
    return out


# ----------------------------------------------------------------- the case


def new_node(**kw):
    n = dict(flags=0, f5e=0, area=(0, 0, 0, 0), words=False, stamp=0, sets=[], f98=None, next=-1, child=-1,
             box=[fb(float(k)) for k in range(6)], matrix=[0x3F800000 + 7 * k for k in range(16)])
    n.update(kw)
    return n


def new_spec(**kw):
    spec = dict(header_flags=0, textures=[dict(f00=0, f02=0, obj=None) for _ in range(NTEX)],
                objs=[[None] * 4 for _ in range(NOBJ)], nodes=[new_node()],
                mats=[[0x40000000 + 0x10001 * (16 * m + k) for k in range(16)] for m in range(4)])
    spec.update(kw)
    return spec


def new_case(**kw):
    c = dict(spec=new_spec(),
             mesh=dict(flags=0, f90=-1, f94=-1, f14c=0x5A5A0000, callback=0,
                       matrix=[0x3F000000 + k for k in range(16)], matrix2=[0x3E000000 + 3 * k for k in range(16)]),
             final=[0x41000000 + 5 * k for k in range(16)], light=[0x42000000 + 9 * k for k in range(16)],
             map_f30=None, map_list6=None, chain=None,
             display=dict(flags=0, frame=7, f84=3, f928=-2, f92c=2, f930=-2, f934=2),
             system=dict(flags=0xC0000000, light_frame=5, renderer=3), settings=0, entry_flags=0,
             light_mask=-1, clip=(0x11223344, fb(2.5)), delay_count=0, queues={}, script=[("prepare",)])
    c.update(kw)
    return c


def merge(c, **kw):
    """The case with a field of a sub-dictionary replaced."""
    for key, value in kw.items():
        if isinstance(c.get(key), dict) and isinstance(value, dict):
            c[key] = {**c[key], **value}
        else:
            c[key] = value
    return c


# ----------------------------------------------------------------- PowerPC side


def w32(e, a, *values):
    e.write(a, struct.pack(">" + "I" * len(values), *[v & MASK for v in values]))


def words_at(e, a, n):
    return list(struct.unpack(">" + "I" * n, e.read(a, 4 * n)))


class Abort(Exception):
    """The assertion ended the call."""


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if hasattr(e, "mf_math"):
        return
    # The math unit: an object whose +4 is a vtable with MultMatrix at +0x20, and the mesh's
    # callback; both reach the current case's side.
    vtable = T.persist_alloc(e, 0x40)
    e.mf_math = T.persist_alloc(e, 0x10)
    w32(e, e.mf_math + 4, vtable)
    w32(e, vtable + 0x20, T.fake_function(e, lambda em: em.mf_side.mult(em)))
    e.mf_callback = T.fake_function(e, lambda em: em.mf_side.callback(em))


class Ppc:
    """One case on the PowerPC side."""

    def __init__(self, e, c):
        self.e, self.c = e, c
        e.mf_side = self
        self.aborted = False
        self.saved = [(a, e.read(a, n)) for a, n in SAVED]
        spec = c["spec"]
        self.lay = layout(spec)
        self.size = self.lay["size"]
        self.base = e.alloc(self.size)
        data, self.lay = image(spec, ">", lambda off: self.base + off)
        e.write(self.base, data)
        # texture objects: +8 the entries array, +0xc the count
        for t in range(NOBJ):
            entries = e.alloc(16)
            w32(e, entries, *[self.base + self.lay["entries"][x] if x is not None else 0 for x in spec["objs"][t]])
            w32(e, self.base + self.lay["slots"][t] + 8, entries)
            e.write(self.base + self.lay["slots"][t] + 0xC, struct.pack(">h", 4))
        # the mesh
        m = c["mesh"]
        self.mesh = e.alloc(MESH_BYTES, b"\xa5" * MESH_BYTES)
        w32(e, self.mesh + 4, m["flags"])
        w32(e, self.mesh + 8, self.base + self.lay["header"])
        w32(e, self.mesh + 0x10, *m["matrix"])
        w32(e, self.mesh + 0x50, *m["matrix2"])
        w32(e, self.mesh + 0x90, m["f90"], m["f94"])
        w32(e, self.mesh + 0x98, *([0] * 33))
        w32(e, self.mesh + 0x140, e.mf_callback if m["callback"] else 0)
        w32(e, self.mesh + 0x14C, m["f14c"])
        # the engine's objects
        self.map = e.alloc(0x80)
        self.hfield, self.clip, self.rlist, self.cache, self.lm = (e.alloc(0x20) for _ in range(5))
        self.final = e.alloc(64)
        w32(e, self.final, *c["final"])
        w32(e, LIGHT_MATRIX, *c["light"])
        lists = lambda i: self.base + self.lay["lists"][i] if i is not None else 0
        w32(e, self.map + 0x30, lists(c["map_f30"]))
        w32(e, self.map + 0x2C, lists(c["map_list6"]))
        w32(e, CHAIN, lists(c["chain"]))
        d, s = c["display"], c["system"]
        w32(e, DISPLAY, d["flags"])
        w32(e, DISPLAY + 0x80, d["frame"], d["f84"])
        w32(e, DISPLAY + 0x928, d["f928"], d["f92c"], d["f930"], d["f934"])
        w32(e, SYSTEM, s["flags"], s["light_frame"])
        w32(e, SYSTEM + 0x1C, s["renderer"])
        w32(e, SETTINGS, c["settings"])
        w32(e, ENTRY_FLAGS, c["entry_flags"])
        w32(e, self.lm + 4, c["light_mask"])
        w32(e, self.clip, *c["clip"])
        w32(e, MESH_DELAY, *([0] * 256))
        w32(e, MESH_DELAY + 0x400, c["delay_count"])
        for a, v in ((MATH_PTR, e.mf_math), (MAPWHO_PTR, self.map), (HFIELD_PTR, self.hfield), (CLIP_PTR, self.clip),
                     (RLIST_PTR, self.rlist), (TCACHE_PTR, self.cache), (LM_PTR, self.lm), (FINAL_PTR, self.final)):
            w32(e, a, v)
        e.cl = self.log = CL.PpcLog({}, c["queues"])
        self.stubs = self.make_stubs()
        self.old = self.stubs.activate(e)

    # -- helpers
    def where(self, p):
        if p == 0:
            return 0
        if self.base <= p < self.base + self.size:
            return p - self.base
        if self.mesh <= p < self.mesh + MESH_BYTES:
            return WHERE_MESH
        return WHERE_OTHER

    def matrix(self, a):
        w = words_at(self.e, a, 16)
        self.log.rec("Matrix", w[:8])
        self.log.rec("Matrix", w[8:])

    # -- the fakes
    def make_stubs(self):
        st = CL.Stubs()
        st.real.update(UNIT)
        e, L = self.e, self.log

        def prepare_lights(em):
            chain = em.u32(CHAIN)
            L.rec("PrepareLights", [self.where(em.r(3)), self.where(chain)])
            if em.r(3):
                w32(em, CHAIN, em.r(3))

        def add_lights(em):
            L.rec("AddLights", [L_LM, self.where(em.r(4)), self.where(em.u32(CHAIN))])
            em.set_r(3, 1)

        def prepare_lighting(em):
            L.rec("PrepareLighting", [L_LM, self.where(em.r(5)), self.where(em.u32(CHAIN))])
            self.matrix(em.r(4))
            em.set_r(3, 1)

        def calc_colours(em):
            L.rec("CalcNodeVertexColors", [L_LM, self.where(em.r(4)), self.where(em.r(5)), T.s32(em.u32(em.r(3) + 4))])

        def check_gamut(em):
            area = [T.s8(b) for b in em.read(em.r(4), 4)]
            L.rec("CheckGamutArea", [L_HFIELD] + area)
            em.set_r(3, T.u32(L.ret("Gamut", 1)))

        def check_box(em):
            L.rec("CheckBoundingBox", [L_CLIP] + words_at(em, em.r(4), 6))
            self.matrix(em.r(5))
            w32(em, em.r(3) + 4, L.ret("Depth", 0))
            em.set_r(3, L.ret("Clip", 0x40) & 0xFF)

        def add_entry(em):
            L.rec("AddEntry", [L_RLIST, self.where(em.r(6)), self.where(em.r(7)), em.r(8)])
            self.matrix(em.r(4))
            self.matrix(em.r(5))
            em.set_r(3, T.u32(L.ret("AddEntry", 1)))

        def cache_prepare(em):
            L.rec("CachePrepare", [L_CACHE, self.where(em.r(4)), CL.fbits(em.f(1))])

        def cache_remove(em):
            L.rec("CacheRemove", [L_CACHE, self.where(em.r(4))])
            em.set_r(3, 1)

        def lock(em):
            L.rec("EntryLock", [self.where(em.r(3))])

        def assertion(em):
            L.rec("Assert", [T.s32(em.r(5))], [em.cstr(em.r(3)), em.cstr(em.r(4))])
            self.aborted = True
            em.uc.emu_stop()

        for addr, fn in ((PREPARE_LIGHTS, prepare_lights), (ADD_LIGHTS, add_lights), (PREPARE_LIGHTING, prepare_lighting),
                         (CALC_COLOURS, calc_colours), (CHECK_GAMUT, check_gamut), (CHECK_BOX, check_box),
                         (ADD_ENTRY, add_entry), (CACHE_PREPARE, cache_prepare), (CACHE_REMOVE, cache_remove),
                         (ENTRY_LOCK, lock)):
            st.internal(addr, fn)
        st.imp("__assertion_failed", assertion)
        return st

    def mult(self, em):
        a, b, out = em.r(4), em.r(5), em.r(6)
        self.log.rec("MultMatrix", [L_MATH])
        self.matrix(a)
        self.matrix(b)
        wa, wb = words_at(em, a, 16), words_at(em, b, 16)
        w32(em, out, *[(wa[i] + 3 * wb[i] + i) & MASK for i in range(16)])

    def callback(self, em):
        self.log.rec("Callback", [self.where(em.r(3))])

    # -- running
    def at(self, offset):
        return self.base + offset

    def run(self, step):
        """One step of a script; returns its result (ABORTED when the assertion ended it)."""
        e, lay = self.e, self.lay
        kind = step[0]
        if kind == "set":
            frame, f84, flags, light_frame, mesh_flags = step[1:]
            w32(e, DISPLAY + 0x80, frame, f84)
            w32(e, SYSTEM, flags, light_frame)
            w32(e, self.mesh + 4, mesh_flags)
            return 0
        if kind == "prepare":
            entry, args = PREPARE, [self.mesh]
        elif kind == "preprocess":
            entry, args = PREPROCESS, [self.mesh, self.at(step[1]), self.at(step[2]), self.at(step[3])]
        elif kind == "textures":
            entry, args = TEXTURES, [self.mesh, self.at(step[1])]
        else:
            lights = {0: 0, WHERE_MESH: self.mesh + 0x98}.get(step[3], None)
            entry = LIGHTS
            args = [self.mesh, self.at(step[1]), self.at(step[2]), self.at(step[3]) if lights is None else lights, step[4]]
        try:
            r3 = T.call_ppc(e, entry, args)[0]
        except RuntimeError:
            if not self.aborted:
                raise
            return ABORTED
        return 0 if kind == "textures" else T.s32(r3)

    def state(self):
        e, m = self.e, self.mesh
        u, s = e.u32, lambda a: T.s32(e.u32(a))
        out = [u(m + 4), s(m + 0x90), s(m + 0x94), u(m + 0x14C), int(u(m + 0x140) != 0)]
        out += words_at(e, m + 0x10, 16) + words_at(e, m + 0x50, 16) + words_at(e, m + 0x98, 33)
        out += [u(ENTRY_FLAGS), self.where(u(CHAIN)), s(u(LM_PTR) + 4), u(DISPLAY), u(SYSTEM), s(SYSTEM + 4),
                u(SETTINGS), u(u(CLIP_PTR)), u(u(CLIP_PTR) + 4), s(MESH_DELAY + 0x400)]
        for i in range(4):
            p = u(MESH_DELAY + 4 * i)
            out.append(0 if p == 0 else WHERE_MESH if p == m else WHERE_OTHER)
        out += words_at(e, u(FINAL_PTR), 16) + words_at(e, LIGHT_MATRIX, 16)
        return out

    def image_state(self):
        data = self.e.read(self.base, self.size)
        return decode(data, ">", lambda v: v - self.base if v else 0, self.lay, self.c["spec"])

    def close(self):
        CL.Stubs.deactivate(self.e, self.old)
        for a, data in self.saved:
            self.e.write(a, bytes(data))


def ppc(e, c):
    side = Ppc(e, c)
    try:
        results, states, images = [], [], []
        for step in c["script"]:
            r = side.run(step)
            results.append(r)
            states.append(side.state())
            images.append(side.image_state())
            if r == ABORTED:
                break
        return dict(results=results, states=states, images=images, log=side.log.log)
    finally:
        side.close()


# ----------------------------------------------------------------- host side


class MfSetup(C.Structure):
    _fields_ = [("imageSize", C.c_uint32), ("headerOffset", C.c_int32), ("textureSlots", C.c_int32 * 4),
                ("textureEntries", (C.c_int32 * 4) * 4), ("meshFlags", C.c_uint32), ("meshF90", C.c_int32),
                ("meshF94", C.c_int32), ("meshF14C", C.c_uint32), ("hasCallback", C.c_int32),
                ("meshMatrix", C.c_uint32 * 16), ("meshMatrix2", C.c_uint32 * 16), ("finalMatrix", C.c_uint32 * 16),
                ("lightMatrix", C.c_uint32 * 16), ("mapF30", C.c_int32), ("mapList6", C.c_int32), ("chain", C.c_int32),
                ("displayFlags", C.c_uint32), ("frame", C.c_int32), ("f84", C.c_int32), ("f928", C.c_int32),
                ("f92C", C.c_int32), ("f930", C.c_int32), ("f934", C.c_int32), ("systemFlags", C.c_uint32),
                ("lightFrame", C.c_int32), ("renderer", C.c_int32), ("settingsFlags", C.c_uint32),
                ("entryFlags", C.c_uint32), ("lightMask", C.c_int32), ("clipWord0", C.c_uint32),
                ("clipDepth", C.c_uint32), ("delayCount", C.c_int32)]


def host(lib, c):
    CL.host_begin(lib, {}, c["queues"])
    spec = c["spec"]
    lay = layout(spec)
    lib.tpw_mf_alloc.restype = C.c_void_p
    lib.tpw_mf_alloc.argtypes = [C.c_uint32]
    lib.tpw_mf_arena_base.restype = C.c_void_p
    addr = lib.tpw_mf_alloc(lay["size"])
    arena = lib.tpw_mf_arena_base()
    data, lay = image(spec, "<", lambda off: addr + off - arena)
    C.memmove(addr, data, len(data))
    s = MfSetup()
    s.imageSize, s.headerOffset = lay["size"], lay["header"]
    for t in range(NOBJ):
        s.textureSlots[t] = lay["slots"][t]
        for f in range(4):
            x = spec["objs"][t][f]
            s.textureEntries[t][f] = lay["entries"][x] if x is not None else 0
    m = c["mesh"]
    s.meshFlags, s.meshF90, s.meshF94, s.meshF14C, s.hasCallback = m["flags"], m["f90"], m["f94"], m["f14c"], m["callback"]
    s.meshMatrix[:], s.meshMatrix2[:] = m["matrix"], m["matrix2"]
    s.finalMatrix[:], s.lightMatrix[:] = c["final"], c["light"]
    lists = lambda i: lay["lists"][i] if i is not None else 0
    s.mapF30, s.mapList6, s.chain = lists(c["map_f30"]), lists(c["map_list6"]), lists(c["chain"])
    d, sy = c["display"], c["system"]
    s.displayFlags, s.frame, s.f84 = d["flags"], d["frame"], d["f84"]
    s.f928, s.f92C, s.f930, s.f934 = d["f928"], d["f92c"], d["f930"], d["f934"]
    s.systemFlags, s.lightFrame, s.renderer = sy["flags"], sy["light_frame"], sy["renderer"]
    s.settingsFlags, s.entryFlags, s.lightMask = c["settings"], c["entry_flags"], c["light_mask"]
    s.clipWord0, s.clipDepth, s.delayCount = c["clip"][0], c["clip"][1], c["delay_count"]
    lib.tpw_mf_setup.argtypes = [C.POINTER(MfSetup)]
    lib.tpw_mf_setup(C.byref(s))
    lib.tpw_mf_run.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_mf_run.restype = C.c_int64
    lib.tpw_mf_state.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_mf_state.restype = C.c_int32
    lib.tpw_mf_set.argtypes = [C.c_int32, C.c_int32, C.c_uint32, C.c_int32, C.c_uint32]
    base_off = addr - arena
    results, states, images = [], [], []
    for step in c["script"]:
        kind = step[0]
        if kind == "set":
            lib.tpw_mf_set(*step[1:])
            r = 0
        else:
            op = {"prepare": 1, "preprocess": 2, "textures": 3, "lights": 4}[kind]
            values = list(step[1:])
            if kind == "lights":
                values[3] &= MASK   # the flags as the unsigned word the original gets
            r = lib.tpw_mf_run(op, (C.c_int64 * 4)(*values))
            if kind == "textures":
                r = 0
            elif r != ABORTED:
                r = T.s32(r & MASK)
        results.append(r)
        out = (C.c_int64 * 256)()
        n = lib.tpw_mf_state(out)
        states.append(list(out[:n]))
        raw = C.string_at(addr, lay["size"])
        images.append(decode(raw, "<", lambda v: v - base_off if v else 0, lay, spec))
        if r == ABORTED:
            break
    return dict(results=results, states=states, images=images, log=CL.host_end(lib))


# ----------------------------------------------------------------- the cases


def rnd_bits(r, bits, p=0.4):
    return sum(b for b in bits if r.random() < p)


def tree(r, n):
    """Links for n nodes forming one tree from node 0 (each node is the next or the child of
    an earlier one, each slot used once)."""
    nxt, child = [-1] * n, [-1] * n
    for i in range(1, n):
        options = [(j, "n") for j in range(i) if nxt[j] < 0] + [(j, "c") for j in range(i) if child[j] < 0]
        j, kind = r.choice(options)
        (nxt if kind == "n" else child)[j] = i
    return nxt, child


NODE_BITS = (0x8000, 0x200, 0x10, 0x40000, 0x80000, 0x2000, 0x4000000, 0x20, 0x1000000, 0x2000000, 0x1, 0x100000)
HEADER_BITS = (0x200, 0x200000, 0x800000, 0x1, 0x2)
MESH_BITS = (0x1, 0x4, 0x10, 0x2, 0x10000000)
DEPTHS = (fb(2.5), fb(-1.0), 0, NEG0, NAN, INF, fb(1e-30), fb(3.0e9), 0xFF800000)
CLIPS = (0, 0x40, 0x41, 0x7F, 0xC0, 0xFF, 0x3F, 0x01, 0x80, 0x5A)


def rspec(r, nodes=None, bias=1.0):
    n = nodes if nodes is not None else r.choice([1, 2, 3, 4, 5, 7])
    textures = [dict(f00=rnd_bits(r, (0x80, 0x400, 0x1, 0x100), 0.5), f02=r.randrange(4),
                     obj=r.choice([None, 0, 1, 2, 3, 0, 1])) for _ in range(NTEX)]
    objs = [[r.choice([None, r.randrange(NENT), r.randrange(NENT)]) for _ in range(4)] for _ in range(NOBJ)]
    nxt, child = tree(r, n)
    ns = []
    for i in range(n):
        f5e = r.choice([0, 1, 2, 3, 4])
        # most nodes are drawable: the skip bits are rare, the others half
        flags = rnd_bits(r, NODE_BITS[3:], 0.3 * bias) | (r.choice(NODE_BITS[:3]) if r.random() < 0.12 else 0)
        flags |= r.getrandbits(32) & 0x30000000 if r.random() < 0.1 else 0
        wide = r.random() < 0.25   # the area bytes over their whole signed range
        ns.append(new_node(flags=flags, f5e=f5e, area=tuple(r.randrange(-128, 128) if wide else r.randrange(-3, 4) for _ in range(4)),
                           words=r.random() < 0.95, stamp=r.choice([0, 5, 7, r.getrandbits(32)]),
                           sets=[r.choice([None, 0, 1, 2, 3, 0, 1]) for _ in range(r.choice([0, 1, 2, 3]))],
                           f98=r.choice([None, 0, 1, 2, 3]), next=nxt[i], child=child[i],
                           box=[r.getrandbits(32) for _ in range(6)], matrix=[r.getrandbits(32) for _ in range(16)]))
    return dict(header_flags=rnd_bits(r, HEADER_BITS, 0.3), textures=textures, objs=objs, nodes=ns,
                mats=[[r.getrandbits(32) for _ in range(16)] for _ in range(4)])


def rqueues(r):
    q = {}
    if r.random() < 0.7:
        q["Gamut"] = [r.choice([0, 1, 1]) for _ in range(r.randrange(1, 6))]
    if r.random() < 0.8:
        q["Clip"] = [r.choice(CLIPS + (0x40, 0x40, 0x41)) for _ in range(r.randrange(1, 6))]
    if r.random() < 0.8:
        q["Depth"] = [r.choice(DEPTHS) for _ in range(r.randrange(1, 6))]
    if r.random() < 0.2:
        q["AddEntry"] = [r.choice([1, 1, 0]) for _ in range(r.randrange(1, 5))]
    return q


def rcase(r, script_kind, nodes=None, bias=1.0):
    spec = rspec(r, nodes, bias)
    lay = layout(spec)
    n = len(spec["nodes"])
    word = lambda: r.getrandbits(32)
    c = new_case(
        spec=spec,
        mesh=dict(flags=rnd_bits(r, MESH_BITS, 0.25), f90=r.choice([-1, 7, 6, r.randrange(-3, 12)]),
                  f94=r.choice([-1, 3, 2, r.randrange(-3, 6)]), f14c=word(), callback=r.randrange(2),
                  matrix=[word() for _ in range(16)], matrix2=[word() for _ in range(16)]),
        final=[word() for _ in range(16)], light=[word() for _ in range(16)],
        map_f30=r.choice([None, 0, 1, 2, 3]), map_list6=r.choice([None, 0, 1, 2, 3]), chain=r.choice([None, 0, 1]),
        display=dict(flags=rnd_bits(r, (0x800, 0x1000, 0x2, 0x400), 0.3), frame=r.choice([7, 7, 8]), f84=r.choice([3, 3, 4]),
                     **{k: (r.randrange(-130, 130) if r.random() < 0.25 else r.randrange(-3, 4))
                        for k in ("f928", "f92c", "f930", "f934")}),
        system=dict(flags=rnd_bits(r, (0x40000000, 0x80000000, 0x1), 0.5), light_frame=r.choice([5, 7, -1, 6]),
                    renderer=r.choice([3, 4, 4, 1])),
        settings=rnd_bits(r, (0x40, 0x100, 0x4), 0.5), entry_flags=r.choice([0, 0x01000000, 0x02000000, 0x03000000, word()]),
        light_mask=r.choice([-1, 0, 0x12345678, T.s32(word())]),
        clip=(word(), r.choice(DEPTHS)), delay_count=r.choice([0, 1, 5, 0xFE, 0xFF, 0x100, 0x1000]),
        queues=rqueues(r))
    node = lambda: node_at(lay, r.randrange(n))
    mat = lambda: r.choice(lay["mats"])
    if script_kind == "prepare":
        steps = []
        for _ in range(r.randrange(1, 5)):
            if r.random() < 0.55:
                steps.append(("prepare",))
            else:
                steps.append(("set", r.choice([7, 8, 9]), r.choice([3, 4]), rnd_bits(r, (0x40000000, 0x80000000), 0.5),
                              r.choice([5, 6, 7]), rnd_bits(r, MESH_BITS, 0.3)))
        c["script"] = steps
    elif script_kind == "preprocess":
        c["script"] = [("preprocess", node_at(lay, r.randrange(n)), mat(), mat()) for _ in range(r.randrange(1, 3))]
    elif script_kind == "textures":
        c["script"] = [("textures", node()) for _ in range(r.randrange(1, 4))]
    else:
        c["script"] = [("lights", node(), mat(), r.choice([0, WHERE_MESH, lay["mats"][3]]), word())
                       for _ in range(r.randrange(1, 4))]
    return c


# ----------------------------------------------------------------- ColorVertices and DotProduct


def cv_words(c, order):
    return struct.pack(order + "I" * len(c["words"]), *c["words"])


def cv_ppc(e, c):
    buf = e.alloc(4 * len(c["words"]), cv_words(c, ">"))
    T.call_ppc(e, COLOR_VERTICES, [buf + 4 * c["first"], c["colour"], c["count"] & MASK, c["stride"] & MASK])
    return dict(words=list(struct.unpack(">" + "I" * len(c["words"]), e.read(buf, 4 * len(c["words"])))))


def cv_host(lib, c):
    raw = cv_words(c, "<")
    buf = C.create_string_buffer(raw, len(raw))
    lib.tpw_mf_color.argtypes = [C.c_void_p, C.c_uint32, C.c_int32, C.c_int32]
    lib.tpw_mf_color(C.addressof(buf) + 4 * c["first"], c["colour"], c["count"], c["stride"])
    return dict(words=list(struct.unpack("<" + "I" * len(c["words"]), buf.raw)))


def cv_case(words, colour, count, stride, first=64):
    return dict(words=words, colour=colour, count=count, stride=stride, first=first)


WORDS_BEFORE = 64   # words in front of the first vertex (a negative stride writes there)


def cv_random_words(r, n=160):
    return [r.choice([r.getrandbits(32), 0, MASK, 0x80808080, 0x01010101, 0xFF00FF00, 0x00FF00FF]) for _ in range(n)]


def cv_edges():
    r = random.Random(7)
    colours = (0, MASK, 0x01010101, 0x80808080, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x7F7F7F7F, 0xFF80FF80)
    for colour in colours:
        for count in (0, 1, 2, 3, 4, 5):
            yield cv_case(cv_random_words(r), colour, count, 0x20)
    for stride in (0, 4, 8, 0x10, 0x14, 0x20, 0x24, 0x30, -0x20, -4, -0x10):
        for count in (1, 2, 3, 6):
            yield cv_case(cv_random_words(r), r.getrandbits(32), count, stride)
    # the first vertex's colour decides every vertex; all channels at their ends
    yield cv_case([0xFFFFFFFF] * 160, MASK, 4, 0x20)
    yield cv_case([0] * 160, MASK, 4, 0x20)


def cv_random(r):
    return cv_case(cv_random_words(r), r.getrandbits(32), r.choice([0, 1, 2, 3, 4, 5, 6, 7]),
                   r.choice([0, 4, 8, 0x10, 0x14, 0x20, 0x24, 0x30, 0x20, 0x20, -0x20, -8]))


DOT_RANGE = (DOT_PRODUCT, DOT_PRODUCT + 0x40)
LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


class HardwareRounding:
    """A code hook over DotProduct that replaces each fmadds result with the C library's fmaf:
    Unicorn computes fmadds in double precision and rounds again to single (FINDINGS 2026-10-08),
    the processor rounds once (tests/test_OpenGLDraw.py does the same for the drawing members)."""

    def __init__(self, e):
        self.e, self.pending = e, None
        self.hook = e.uc.hook_add(UC_HOOK_CODE, self.step, begin=DOT_RANGE[0], end=DOT_RANGE[1] - 1)

    def close(self):
        self.e.uc.hook_del(self.hook)

    def fpr(self, i):
        v = self.e.uc.reg_read(ppcemu.FPR[i])
        bits = v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]
        return struct.unpack(">d", struct.pack(">Q", bits))[0]

    def step(self, uc, address, size, user):
        if self.pending is not None:
            target, value = self.pending
            self.pending = None
            uc.reg_write(ppcemu.FPR[target], struct.unpack(">Q", struct.pack(">d", value))[0])
        word = struct.unpack(">I", self.e.read(address, 4))[0]
        if word >> 26 == 59 and (word >> 1) & 31 == 29:   # fmadds frD, frA, frC, frB
            a, c, b = self.fpr((word >> 16) & 31), self.fpr((word >> 6) & 31), self.fpr((word >> 11) & 31)
            if all(math.isfinite(x) for x in (a, c, b)):
                assert struct.unpack(">f", struct.pack(">f", a))[0] == a, "a fused operand is not a single"
                self.pending = ((word >> 21) & 31, float(LIBM.fmaf(a, c, b)))


def canon(word):
    """A NaN's bit pattern as one value (fused multiply-adds propagate them differently)."""
    return 0x7FC00000 if (word & 0x7F800000) == 0x7F800000 and word & 0x007FFFFF else word


def dot_ppc(e, c):
    rounding = HardwareRounding(e)
    try:
        f1 = T.call_ppc(e, DOT_PRODUCT, list(c["bits"]))[2]
    finally:
        rounding.close()
    return dict(bits=canon(struct.unpack(">I", struct.pack(">f", f1))[0]))


def dot_host(lib, c):
    lib.tpw_mf_dot.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_mf_dot.restype = C.c_uint32
    return dict(bits=canon(lib.tpw_mf_dot((C.c_uint32 * 6)(*c["bits"]))))


def dot_case(*values):
    return dict(bits=[v if isinstance(v, int) else fb(v) for v in values])


def dot_edges():
    singles = (0.0, -0.0, 1.0, -1.0, 0.5, 3.0, 1e-30, -1e-30, 1e30, -1e30, 3.4e38, -3.4e38, 1e-45, 16777217.0)
    for x in singles:
        for y in singles[:8]:
            yield dot_case(x, y, 1.0, 1.0, y, 2.0)
            yield dot_case(1.0, 1.0, x, y, 2.0, 1.0)
    yield dot_case(float("inf"), 0.0, 1.0, 1.0, 1.0, 1.0)
    yield dot_case(float("inf"), 1.0, 1.0, float("-inf"), 1.0, 1.0)
    yield dot_case(NAN, 1.0, 1.0, 1.0, 1.0, 1.0)
    yield dot_case(1.0, 2.0, 3.0, 4.0, 5.0, 6.0)
    yield dot_case(0.1, 0.2, 0.3, 0.7, 0.11, 0.13)
    # products whose sum only a fused multiply-add rounds the way the processor does
    yield dot_case(fb(1.0 + 2.0 ** -12), 1.0, fb(1.0 + 2.0 ** -12), fb(-1.0 - 2.0 ** -11) , 1.0, fb(1.0 - 2.0 ** -12)) if False else dot_case(1.0, 1.0, 1.0, 1.0, 1.0, 1.0)


def dot_random(r):
    def one():
        k = r.random()
        if k < 0.6:
            return r.getrandbits(32) & ~0x7F800000 | (r.randrange(110, 145) << 23)    # an ordinary exponent
        if k < 0.8:
            return fb(r.uniform(-4, 4))
        return r.choice([0, NEG0, INF, 0xFF800000, NAN, fb(1.0), fb(-1.0), r.getrandbits(32)])
    return dict(bits=[one() for _ in range(6)])


# ----------------------------------------------------------------- the lists of cases


def wired_spec(nodes, **kw):
    """Texture record k holds object k (record 3's has no entries); the entries differ per frame."""
    textures = [dict(f00=0, f02=k, obj=k) for k in range(NTEX)]
    objs = [[0, 1, 2, 3], [1, 2, 3, 4], [2, None, 4, 5], [None, None, None, None]]
    return new_spec(textures=textures, objs=objs, nodes=nodes, **kw)


def lit_node(**kw):
    """A node with mesh words (stamp 0), two vertices' worth of colours and two face sets."""
    n = dict(flags=0, f5e=2, words=True, stamp=0, sets=[0, 1], f98=None)
    n.update(kw)
    return new_node(**n)


def ecase(nodes=None, spec_kw=None, **kw):
    """A case over a single lit node (or the given nodes), the lobby's usual state."""
    spec = wired_spec(nodes if nodes is not None else [lit_node()], **(spec_kw or {}))
    return new_case(spec=spec, **kw)


def chain(nodes, links):
    """Sets the tree links of nodes: links is a list of (index, kind, target)."""
    for index, kind, target in links:
        nodes[index]["next" if kind == "n" else "child"] = target
    return nodes


def lay_of(c):
    return layout(c["spec"])


def with_script(c, *steps):
    c["script"] = list(steps)
    return c


def prepare_edges():
    # the stamp
    yield ecase()
    for f90, f94 in ((7, 3), (7, 2), (6, 3), (-1, -1), (7, -1)):
        yield ecase(mesh=dict(flags=0, f90=f90, f94=f94, f14c=0x5A5A0000, callback=1,
                              matrix=[0x3F000000 + k for k in range(16)], matrix2=[0x3E000000 + k for k in range(16)]))
    # the callback: absent, present with a new frame, present within the frame
    for callback in (0, 1):
        for f90 in (6, 7):
            yield ecase(mesh=dict(flags=0, f90=f90, f94=0, f14c=1, callback=callback,
                                  matrix=[0x3F000000 + k for k in range(16)], matrix2=[0x3E000000 + k for k in range(16)]))
    # the delayed list: the header flag, the mesh's own flag, the list's fill
    for header in (0, 0x200000, 0x200200):
        for flags in (0, 0x10, 0x11, 0x14):
            for count in (0, 0xFE, 0xFF, 0x100, 0x180):
                yield ecase(spec_kw=dict(header_flags=header), mesh=dict(
                    flags=flags, f90=-1, f94=-1, f14c=3, callback=1, matrix=[0x3F000000 + k for k in range(16)],
                    matrix2=[0x3E000000 + k for k in range(16)]), delay_count=count)
    # hidden and interface meshes, the entry flags
    for flags in (0, 4, 5, 1, 0x1000004):
        for header in (0, 0x200, 0x300):
            for entry in (0, 0x02000000, 0xFFFFFFFF, 0x01000000, 0xFDFFFFFF):
                yield ecase(spec_kw=dict(header_flags=header), entry_flags=entry, mesh=dict(
                    flags=flags, f90=-1, f94=-1, f14c=0x77, callback=0, matrix=[0x3F000000 + k for k in range(16)],
                    matrix2=[0x3E000000 + k for k in range(16)]))
    # the mesh's own matrix in front of the view matrix, then the guard of the static result
    c = ecase(mesh=dict(flags=1, f90=-1, f94=-1, f14c=0, callback=0, matrix=[0x3F000000 + k for k in range(16)],
                        matrix2=[0x3E000000 + k for k in range(16)]))
    yield with_script(c, ("prepare",), ("set", 8, 3, 0xC0000000, 5, 1), ("prepare",), ("set", 8, 4, 0xC0000000, 5, 1),
                      ("prepare",))
    # a header with no nodes
    yield new_case(spec=new_spec(nodes=[]))
    yield new_case(spec=new_spec(nodes=[], header_flags=0x200))
    # a frame: Prepare, again at once, a new pass, a new frame
    yield with_script(ecase(), ("prepare",), ("prepare",), ("set", 7, 4, 0xC0000000, 5, 0), ("prepare",),
                      ("set", 8, 3, 0xC0000000, 5, 0x10), ("prepare",))


def one(flags, system=0xC0000000, **kw):
    """Preprocess of one node with the given flags."""
    c = ecase([lit_node(flags=flags)], system=dict(flags=system, light_frame=5, renderer=3), **kw)
    lay = lay_of(c)
    return with_script(c, ("preprocess", node_at(lay, 0), lay["mats"][0], lay["mats"][1]))


def preprocess_edges():
    # the node's own flags against the system's
    for flags in (0, 0x8000, 0x200, 0x10, 0x8210, 0x40000, 0x80000, 0xC0000, 0x40000 | 0x10, 0x1000000, 0x20):
        for system in (0, 0x40000000, 0x80000000, 0xC0000000, 0x1):
            yield one(flags, system)
    # the area tests: the display's rectangle against the node's four bytes, each at its edge
    for flags in (0x2000 | 0x4000000, 0x2000 | 0x4000000 | 0x40000, 0x4000000, 0x2000):
        base = dict(f928=-1, f92c=1, f930=-1, f934=1)
        for area in ((1, -1, 1, -1), (1, -1, -1, -1), (1, -1, 1, 1), (-1, -1, 1, -1), (1, 1, 1, -1), (0, 0, 0, 0),
                     (1, -1, 2, -2), (2, -2, 2, -2), (-2, 2, -2, 2), (1, -1, 0, 0)):
            for tweak in ({}, dict(f928=0), dict(f928=-2), dict(f92c=0), dict(f92c=2), dict(f930=0), dict(f930=-2),
                          dict(f934=0), dict(f934=2), dict(f928=2), dict(f934=-2)):
                c = one(flags)
                c["spec"]["nodes"][0]["area"] = area
                c["display"] = {**c["display"], **base, **tweak}
                yield c
    for gamut in (0, 1):
        c = one(0x2000, queues={"Gamut": [gamut]})
        c["spec"]["nodes"][0]["area"] = (3, -4, 5, -6)
        yield c
    # the clipping result and the lighting that follows
    for clip in (0, 0x3F, 0x40, 0x41, 0x7F, 0xFF, 0x80, 0xC0):
        for entry in (0, 0x01000000, 0x02000000, 0xFF000000, 0xF0F0F0F0):
            yield one(0, queues={"Clip": [clip]}, entry_flags=entry)
    for add in (0, 1):
        yield one(0, queues={"AddEntry": [add]})
    # trees: siblings, children, a child of a node that does not walk its children, failures
    nodes = chain([lit_node(), lit_node(), lit_node(), lit_node(), lit_node()],
                  [(0, "n", 1), (1, "c", 2), (2, "n", 3), (0, "c", 4)])
    for fail in (None, 1, 2, 3, 4, 5):
        for leaf in (0, 0x20):
            c = ecase(nodes=[dict(n) for n in nodes], queues={"AddEntry": [1] * (fail - 1) + [0] if fail else [1]})
            c["spec"]["nodes"][1]["flags"] = leaf
            c["spec"]["nodes"][0]["flags"] = leaf
            lay = lay_of(c)
            yield with_script(c, ("preprocess", node_at(lay, 0), lay["mats"][0], lay["mats"][1]))
    # not every node has mesh words or a face set
    nodes = chain([lit_node(f5e=0), lit_node(words=False, f5e=0), lit_node(sets=[])], [(0, "n", 1), (1, "n", 2)])
    c = ecase(nodes=nodes)
    lay = lay_of(c)
    yield with_script(c, ("preprocess", node_at(lay, 0), lay["mats"][2], lay["mats"][3]))
    # the order of the starting node and the two matrices
    c = ecase(nodes=chain([lit_node(), lit_node()], [(0, "c", 1)]))
    lay = lay_of(c)
    yield with_script(c, ("preprocess", node_at(lay, 1), lay["mats"][3], lay["mats"][0]),
                      ("preprocess", node_at(lay, 0), lay["mats"][1], lay["mats"][2]))


def textures_case(node, settings=0, depth=fb(2.5), **kw):
    c = ecase([lit_node(**node)], settings=settings, clip=(0, depth), **kw)
    lay = lay_of(c)
    return with_script(c, ("textures", node_at(lay, 0)))


def textures_edges():
    # the record flags and the settings flag that names the frame
    for settings in (0, 0x40, 0x100):
        for f00 in (0, 0x80, 0x400, 0x480, 0xFFFF):
            for flags in (0, 0x40000, 0x80000, 0xC0000, 0x2000000):
                c = textures_case(dict(flags=flags, sets=[0, 1, 2, 3], f98=1), settings)
                for t in c["spec"]["textures"]:
                    t["f00"] = f00
                yield c
    # frames 0..3 of a record, entries missing, objects missing, records missing
    for frame in range(4):
        for obj in (None, 0, 2, 3):
            for settings in (0, 0x40):
                c = textures_case(dict(sets=[0], f98=0), settings)
                c["spec"]["textures"][0].update(f02=frame, obj=obj)
                yield c
    c = textures_case(dict(sets=[None, 0, None, 1], f98=None), 0x40)
    yield c
    # the node's extra texture: present or not, with or without an entry
    for flags in (0, 0x2000000):
        for f98 in (None, 0, 1, 3):
            yield textures_case(dict(flags=flags, sets=[], f98=f98))
    c = textures_case(dict(flags=0x2000000, sets=[], f98=0), 0x40)
    c["spec"]["textures"][0].update(f02=2, obj=0)
    yield c
    # the depth: ordinary, zero, negative, a NaN, infinities
    for depth in (fb(2.5), 0, NEG0, fb(-3.0), NAN, INF, 0xFF800000, fb(1e-40), 0x7FFFFFFF, 0xFFFFFFFF):
        yield textures_case(dict(sets=[0, 1], f98=2, flags=0x2000000), depth=depth)
    # no face sets
    yield textures_case(dict(sets=[], f98=None))
    # one texture removed from its page twice in a row
    c = textures_case(dict(sets=[0, 0], f98=0))
    c["spec"]["textures"][0]["f00"] = 0x480
    yield c


def lights_case(node=None, header=0, system=None, display=0, flags=0, lights=0, **kw):
    n = dict(f5e=2, words=True, stamp=0)
    n.update(node or {})
    c = ecase([lit_node(**n)], spec_kw=dict(header_flags=header), **kw)
    c["system"] = {**c["system"], **(system or {})}
    c["display"] = {**c["display"], "flags": display}
    lay = lay_of(c)
    return with_script(c, ("lights", node_at(lay, 0), lay["mats"][1], lights, flags))


def lights_edges():
    # nothing to do: no vertices, or the node asks for no lights; no mesh words is an assertion
    for f5e in (0, 1):
        for flags in (0, 0x1000000, 0x1000000 | 0x40000):
            yield lights_case(dict(f5e=f5e, flags=flags))
    yield lights_case(dict(words=False))
    yield lights_case(dict(words=False, f5e=0))
    # the stamp: the same frame (done), another, with the lights changed or the node asking again
    for stamp in (5, 4, 0xFFFFFFFB):
        for light_frame in (5, -1):
            for flags in (0x00000000, 0x40000000, 0x80000000):
                for node_flags in (0, 0x40000, 0x80000, 0xC0000, 0x20000):
                    yield lights_case(dict(stamp=stamp, flags=node_flags), system=dict(flags=flags, light_frame=light_frame))
    # the work: the display's two flags, the call's two flags, the lists
    for display in (0, 0x800, 0x1000, 0x1800, 0x2):
        for flags in (0, 0x01000000, 0x02000000, 0x03000000, 0x7FFFFFFF, -1):
            for f30, list6 in ((None, None), (0, None), (None, 1), (2, 3)):
                yield lights_case(display=display, flags=flags, map_f30=f30, map_list6=list6,
                                  chain=1 if display == 0x1000 else None)
    # the mask: header flag 0x800000 with renderer 4 swaps it for the call
    for header in (0, 0x800000, 0x1, 0x800001):
        for renderer in (3, 4, 5):
            for display in (0, 0x800):
                for mask in (-1, 0, 0x1234567):
                    yield lights_case(header=header, system=dict(renderer=renderer), display=display, light_mask=mask)
    # the lights argument: none, the mesh's own, another block
    for lights in (0, WHERE_MESH, layout(new_spec())["mats"][3]):
        yield lights_case(lights=lights)


TESTS = [
    FnTest("CMesh::Prepare", ppc, host, edge=prepare_edges, rnd=lambda r: rcase(r, "prepare")),
    FnTest("CMesh::Preprocess", ppc, host, edge=preprocess_edges, rnd=lambda r: rcase(r, "preprocess")),
    FnTest("CMesh::PreprocessTextures", ppc, host, edge=textures_edges, rnd=lambda r: rcase(r, "textures")),
    FnTest("CMesh::StaticCalculateLights", ppc, host, edge=lights_edges, rnd=lambda r: rcase(r, "lights")),
    FnTest("ColorVertices", cv_ppc, cv_host, edge=cv_edges, rnd=cv_random),
    FnTest("DotProduct", dot_ppc, dot_host, edge=dot_edges, rnd=dot_random),
]
