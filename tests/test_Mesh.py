"""CMesh: construction, Init, Close, DestroyBumpmappingNormals, DeleteRideBase,
DeleteRideSign, GetData, GetLinkType (engine library, 0x1001cb44..0x1001f0f4;
src/engine/Mesh.cpp).

A case lays a small mesh image out on both sides (header, texture records and infos, face
sets, nodes: the fields CMesh reads, everything else zero; the original's in the Mac's
byte order with addresses, the translation's in the host's with arena offsets) and runs a
script on one CMesh over it. After every operation the test compares the result, the
fakes' events (texture objects made, loaded and closed, the allocator, the link base
table, the height field, the delayed loads, the loading picture, errors) and the state:
the object, the texture records with their objects' entries, the face sets' flag byte,
each node's block and its place in the mesh's buffer of words, the texture settings.

Operations the original cannot survive in the state it would meet are left out on both
sides: the PowerPC side decides, the host side skips the same ones.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_Mesh"

FN = dict(ctor=0x1001cb44, dtor=0x1001cd68, Init=0x1001ce38, Close=0x1001d244, DestroyBumpmappingNormals=0x1001e468,
          GetData=0x1001e598, GetLinkType=0x1001e5c4, DeleteRideBase=0x1001ebd0, DeleteRideSign=0x1001f020)
TEX_CTOR, TEX_LOAD, TEX_CLOSE, TEX_ENTRY = 0x100167b0, 0x10016b30, 0x10016fc0, 0x100171ec
REGISTER, UNREGISTER, FIELD_INIT, FIELD_CLOSE, DELAYED, SPLASH, ERROR_PUSH = (
    0x10019700, 0x10019768, 0x1000d740, 0x1000d8b8, 0x1001670c, 0x1002d7a0, 0x10008904)
P_MALLOC, P_FREE, P_SMALL, P_FIELD, DISPLAY, SETTINGS, SYSTEM = (
    0x10051f70, 0x10051f74, 0x101735c4, 0x100af11c, 0x10165634, 0x10165fc8, 0x10165f80)
MESH_BYTES = 0x15c
SKIPPED = -8


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if not hasattr(e, "ms_fns"):
        e.ms_fns = {name: T.fake_function(e, (lambda n: lambda emu: getattr(emu.ms_side, n)())(name))
                    for name in ("cb_malloc", "cb_free", "cb_small")}


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def image(spec, order, ptr):
    """The mesh image for one side. order: its byte order; ptr(offset): its pointer to that
    offset of the image. Returns the bytes and the offsets of the four tables."""
    textures, sets, nodes = spec["textures"], spec["sets"], spec["nodes"]
    t = 0xb8
    i = t + 8 * len(textures)
    f = i + 16 * len(textures)
    n = f + 16 * len(sets)
    data = bytearray(n + 0xa0 * len(nodes))

    def put(offset, fmt, value):
        struct.pack_into(order + fmt, data, offset, value)

    name = spec["name"].encode()
    data[0x18:0x18 + len(name)] = name
    put(0x30, "I", spec["flags"])
    put(0x36, "H", len(textures))
    put(0x3a, "H", spec["uv"])
    put(0x3c, "H", len(sets))
    put(0x44, "H", len(nodes))
    put(0x50, "I", ptr(t))
    put(0x54, "I", ptr(i))
    put(0x64, "I", ptr(f))
    put(0x6c, "I", ptr(4) if spec["hfield"] else 0)
    put(0x70, "I", ptr(n))
    for k, (flags, frames) in enumerate(textures):
        put(t + 8 * k, "H", flags)
        put(i + 16 * k + 0xa, "H", frames)
    for k, texture in enumerate(sets):
        put(f + 16 * k, "I", ptr(t + 8 * texture) if texture is not None else 0)
        data[f + 16 * k + 0xc] = spec["set_bytes"][k]
    first = 0
    for k, node in enumerate(nodes):
        o = n + 0xa0 * k
        put(o, "I", node["flags"])
        put(o + 0x5a, "H", node["sets"])
        put(o + 0x5e, "H", node["words"])
        put(o + 0x6c, "I", ptr(f + 16 * first))
        first += node["sets"]
    return bytes(data), (t, i, f, n)


class PpcSide:
    def __init__(self, e, case):
        self.e, self.spec = e, case["mesh"]
        e.ms_side = self
        self.L = CL.PpcLog({}, case.get("q"))
        e.cl = self.L
        self.mesh = e.alloc(MESH_BYTES, b"\xa5" * MESH_BYTES)
        data, tables = image(self.spec, ">", lambda offset: 0)
        self.header = e.alloc(len(data))
        data, (self.t, self.i, self.f, self.n) = image(self.spec, ">", lambda offset: self.header + offset)
        e.write(self.header, data)
        self.t, self.i, self.f, self.n = (self.header + x for x in (self.t, self.i, self.f, self.n))
        for k, node in enumerate(self.spec["nodes"]):
            if node["block"]:
                block = e.alloc(32)
                self.L.newlab(block)
                w32(e, self.n + 0xa0 * k + 0x74, block)
        self.field = e.alloc(16)
        self.arrays = {}
        self.built = False

    # ---- fakes
    def cb_malloc(self):
        e, L = self.e, self.L
        L.rec("Malloc", (e.r(3),))
        if not L.ret("Malloc", 1):
            e.set_r(3, 0)
            return
        block = e.alloc(e.r(3) + 16, bytes(e.r(3) + 16))
        L.newlab(block)
        e.set_r(3, block)

    def cb_free(self):
        e, L = self.e, self.L
        block = e.u32(e.r(3))
        L.rec("Free", (L.lab(block),))
        L.forget(block)
        w32(e, e.r(3), 0)

    def cb_small(self):
        self.L.rec("UseSmallTextures")
        self.e.set_r(3, 1 if self.L.ret("Small", 0) else 0)

    def op_new(self, e):
        L = self.L
        L.rec("new", (e.r(3),))
        if not L.ret("New", 1):
            e.set_r(3, 0)
            return
        block = e.alloc(e.r(3), b"\xa5" * e.r(3))
        L.newlab(block)
        e.set_r(3, block)

    def op_delete(self, e):
        self.L.rec("delete", (self.L.lab(e.r(3)),))
        self.L.forget(e.r(3))

    def tex_ctor(self, e):
        e.write(e.r(3), bytes(16))
        self.L.rec("Tex.ctor", (self.L.lab(e.r(3)),))

    def tex_load(self, e):
        L, this, record, info = self.L, e.r(3), e.r(4), e.r(5)
        L.rec("Tex.Load", (L.lab(this), (record - self.t) // 8, (info - self.i) // 16))
        frames = struct.unpack(">H", e.read(info + 0xa, 2))[0]
        entries = e.alloc(4 * (frames + 1), bytes(4 * (frames + 1)))
        for k in range(frames):
            if L.ret("Entry", 1):
                entry = e.alloc(0x5c, bytes(0x5c))
                L.newlab(entry)
                w32(e, entries + 4 * k, entry)
        w32(e, this + 8, entries)
        e.write(this + 0xc, struct.pack(">h", T.s16(frames)))
        e.set_r(3, L.ret("Tex.Load", 1))

    def tex_close(self, e):
        self.L.rec("Tex.Close", (self.L.lab(e.r(3)),))
        e.set_r(3, self.L.ret("Tex.Close", 1))

    def destroy_array(self, e):
        L, first = self.L, e.r(3)
        if not first:
            return
        if e.r(4):                                   # an array of texture objects: each one closes
            for k in reversed(range(self.arrays[first])):
                L.rec("Tex.Close", (L.lab(first + 0x10 * k),))
                L.ret("Tex.Close", 1)
        L.rec("delete", (L.lab(first - 8),))
        L.forget(first - 8)

    def stubs(self):
        e, L = self.e, self.L
        st = CL.Stubs()
        st.real = set(FN.values()) | {TEX_ENTRY}
        st.internal(TEX_CTOR, self.tex_ctor)
        st.internal(TEX_LOAD, self.tex_load)
        st.internal(TEX_CLOSE, self.tex_close)
        st.internal(REGISTER, lambda e: (L.rec("RegisterLinkBase", (int(e.r(3) == self.mesh + 0x154),)), e.set_r(3, 1)))
        st.internal(UNREGISTER, lambda e: (L.rec("FreeLinkBase", (int(e.r(3) == self.mesh + 0x154),)), e.set_r(3, 1)))
        st.internal(FIELD_INIT, lambda e: (L.rec("HField.Init", (int(e.r(4) == self.header),)), e.set_r(3, 1)))
        st.internal(FIELD_CLOSE, CL.recorder("HField.Close", "", ret=lambda e, L: 1))
        st.internal(DELAYED, CL.recorder("DelayedLoad"))
        st.internal(SPLASH, CL.recorder("Splash", "", ret=lambda e, L: 1))
        st.internal(ERROR_PUSH, CL.recorder("Error", "xii"))
        def strcmp(e):
            a, b = e.read(e.r(3), 64).split(b"\0")[0], e.read(e.r(4), 64).split(b"\0")[0]
            e.set_r(3, (a > b) - (a < b))

        st.imp("strcmp", strcmp)
        st.imp("__nw__FUl", self.op_new)
        st.imp("__dl__FPv", self.op_delete)
        st.imp("__destroy_new_array", self.destroy_array)
        return st

    def reset(self):
        e = self.e
        for a, v in ((P_MALLOC, e.ms_fns["cb_malloc"]), (P_FREE, e.ms_fns["cb_free"]), (P_SMALL, e.ms_fns["cb_small"]),
                     (P_FIELD, 0), (DISPLAY, 0), (DISPLAY + 8, 800), (SETTINGS, 0), (SYSTEM, 0)):
            w32(e, a, v)

    def call(self, name, *args):
        return T.call_ppc(self.e, FN[name], list(args))[0]

    def plant(self, sign, count, flag, bits):
        """A ride base or sign as the code that makes them would leave it."""
        e, L, m = self.e, self.L, self.mesh
        old = e.alloc(8 + 8 * (count + 1), bytes(8 + 8 * (count + 1)))
        L.newlab(old)
        textures = e.alloc(8 + 0x10 * (count + 1), bytes(8 + 0x10 * (count + 1)))
        L.newlab(textures)
        for k in range(count + 1):
            L.rec("Tex.ctor", (L.newlab(textures + 8 + 0x10 * k),))
        self.arrays[textures + 8] = count + 1
        for k in range(count):
            value = 0
            if bits >> k & 1:
                value = e.alloc(0x10, bytes(0x10))
                L.newlab(value)
            e.write(old + 8 + 8 * k, struct.pack(">II", value, self.t + 8 * (k % len(self.spec["textures"])) + 4))
        base = 0x130 if sign else 0x124
        e.write(m + base, struct.pack(">IIi", old + 8, textures + 8, count))
        if flag:
            w32(e, m + 4, e.u32(m + 4) | (0x40000000 if sign else 0x20000000))

    def op(self, op, a):
        e, L, m = self.e, self.L, self.mesh
        if op == 12:
            w32(e, SETTINGS, a[0])
            w32(e, DISPLAY, a[1])
            w32(e, DISPLAY + 8, a[2])
            w32(e, SYSTEM, a[3])
            w32(e, P_FIELD, self.field if a[4] else 0)
            return 0
        if op == 1:
            self.call("ctor", m, 1)
            self.built = True
            return 0
        if not self.built:
            return None
        header, words = e.u32(m + 8), e.u32(m + 0x144)
        if op == 2:
            return T.s32(self.call("Init", m, self.header if a[0] else 0))
        if op in (3, 9):
            # Close frees the buffer of words: it has to be one (Init can stop before it is).
            if header and words and L.lab(words) <= 0:
                return None
            if op == 3:
                return T.s32(self.call("Close", m))
            self.call("dtor", m, 0xFFFF)
            self.built = False
            return 0
        if op == 4:
            return T.s32(self.call("DestroyBumpmappingNormals", m)) if header == self.header else None
        if op == 5:
            return T.s32(self.call("DeleteRideBase", m))
        if op == 6:
            return T.s32(self.call("DeleteRideSign", m))
        if op == 7:
            value = self.call("GetData", m)
            return 0 if value == 0 else 1 if value == self.header else -1
        if op == 8:
            return T.s32(self.call("GetLinkType", m))
        if op in (10, 11):
            if not self.spec["textures"]:
                return None
            self.plant(op == 11, a[0], a[1], a[2])
            return 0
        return -99

    def state(self):
        e, L, u, m = self.e, self.L, self.e.u32, self.mesh
        s = lambda a: T.s32(u(a))
        built = self.built
        header = u(m + 8)
        out = [u(m + 4), -7 if not built else 0 if header == 0 else 1 if header == self.header else -1,
               s(m + 0x90), s(m + 0x94), u(m + 0x98), u(m + 0x118), u(m + 0x11c), u(m + 0x13c), u(m + 0x140)]
        out += [int(u(m + 0x124) != 0) if built else -7, int(u(m + 0x128) != 0) if built else -7, s(m + 0x12c)]
        out += [int(u(m + 0x130) != 0) if built else -7, int(u(m + 0x134) != 0) if built else -7, s(m + 0x138)]
        for base in (0x10, 0x50):
            values = struct.unpack(">16f", e.read(m + base, 64))
            out += [int(values[k] == (1.0 if k % 5 == 0 else 0.0)) for k in range(16)]
        buffer = u(m + 0x144)
        words = built and bool(u(m + 4) & 0x10000000) and L.lab(buffer) > 0
        out += [L.lab(buffer) if words else -7, (u(m + 0x148) - buffer) // 4 if words else -7]
        out.append(int(u(m + 0x148) == 0) if built else -7)
        out.append(u(self.header + 0x30))
        for k in range(len(self.spec["textures"])):
            texture = u(self.t + 8 * k + 4)
            out.append(L.lab(texture))
            entries = u(texture + 8) if texture and L.lab(texture) > 0 else 0
            count = struct.unpack(">h", e.read(texture + 0xc, 2))[0] if entries else 0
            for frame in range(3):
                entry = u(entries + 4 * frame) if entries and frame < count else 0
                out.append(u(entry + 8) if entry else -7)
        out += [e.read(self.f + 16 * k + 0xc, 1)[0] for k in range(len(self.spec["sets"]))]
        for k in range(len(self.spec["nodes"])):
            node = self.n + 0xa0 * k
            word = u(node + 0x9c)
            out += [L.lab(u(node + 0x74)), 0 if word == 0 else 1 + (word - buffer) // 4 if words else -1,
                    u(word) if word and words else 0]
        out.append(u(SETTINGS))
        return out


def ppc(e, case):
    side = PpcSide(e, case)
    words = (P_MALLOC, P_FREE, P_SMALL, P_FIELD, DISPLAY, DISPLAY + 8, SETTINGS, SYSTEM)
    saved_words = [(a, e.u32(a)) for a in words]
    saved = side.stubs().activate(e)
    try:
        side.reset()
        results, states, marks, skipped = [], [], [], []
        for op, args in case["script"]:
            result = side.op(op, args)
            skipped.append(result is None)
            results.append(SKIPPED if result is None else result)
            states.append(side.state())
            marks.append(len(side.L.log))
    finally:
        CL.Stubs.deactivate(e, saved)
        for a, v in saved_words:
            w32(e, a, v)
    case["_skipped"] = skipped
    return dict(results=results, log=side.L.log, marks=marks, states=states)


def host(lib, case):
    def F(name, result, *args):
        fn = lib[name]
        fn.restype, fn.argtypes = result, list(args)
        return fn

    spec = case["mesh"]
    arena = F("tpw_ms_arena_base", C.c_void_p)()
    size = len(image(spec, "<", lambda offset: 0)[0])
    block = F("tpw_ms_alloc", C.c_void_p, C.c_uint32)(size)
    data, (t, i, f, n) = image(spec, "<", lambda offset: block - arena + offset)
    C.memmove(block, data, size)
    F("tpw_ms_reset", None, C.c_void_p)(block)
    CL.host_begin(lib, {}, case.get("q"))
    new_block = F("tpw_ms_block", C.c_void_p, C.c_uint32)
    for k, node in enumerate(spec["nodes"]):
        if node["block"]:
            C.memmove(block + n + 0xa0 * k + 0x74, struct.pack("<I", new_block(32) - arena), 4)
    op_fn = F("tpw_ms_op", C.c_int64, C.c_int32, C.POINTER(C.c_int32))
    state_fn = F("tpw_ms_state", C.c_int32, C.POINTER(C.c_int64), C.c_int32)
    results, states, marks = [], [], []
    buffer, built = (C.c_int64 * 256)(), False
    try:
        for (op, args), skip in zip(case["script"], case["_skipped"]):
            if skip:
                results.append(SKIPPED)
            else:
                padded = (C.c_int32 * 8)(*[T.s32(v) for v in (list(args) + [0] * 8)[:8]])
                results.append(op_fn(op, padded))
                built = True if op == 1 else False if op == 9 else built
            states.append(list(buffer[:state_fn(buffer, int(built))]))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    return dict(results=results, log=log, marks=marks, states=states)


# ----------------------------------------------------------------- cases


def mesh_of(r, **given):
    textures = [(r.choice([0, 0, 0x1000, 0x1000, 0x30, 0x1030, 0xffff]), r.choice([0, 1, 1, 2, 3]))
                for _ in range(given.get("textures", r.randrange(0, 4)))]
    nodes, sets = [], []
    for _ in range(given.get("nodes", r.randrange(0, 4))):
        count = r.randrange(0, 4)
        nodes.append(dict(flags=r.choice([0, 0, 0x200, 0x201, r.getrandbits(32)]), sets=count, words=r.randrange(0, 5),
                          block=r.random() < 0.6))
        sets += [r.randrange(len(textures)) if textures and r.random() < 0.8 else None for _ in range(count)]
    words = sum(node["words"] for node in nodes)
    return dict(textures=textures, sets=sets, nodes=nodes, set_bytes=[r.getrandbits(8) for _ in sets],
                uv=(words + 3) // 4 + r.randrange(2), hfield=given.get("hfield", r.random() < 0.5),
                flags=given.get("flags", r.choice([0, 0, 0x200, 0x2, 0x202, r.getrandbits(32)])),
                name=given.get("name", r.choice(["mesh", "BlueprintMesh", "BlueprintMes", "BlueprintMeshes", ""])))


def globals_op(r):
    return (12, [r.choice([0, 0, 0x20, 0x100, 0x120, 0x17a, r.getrandbits(32)]), r.choice([0, 0, 2, r.getrandbits(32)]),
                 r.choice([0x200, 0x280, 800, 1024]), r.choice([0, 1, 1, r.getrandbits(32)]), r.randrange(2)])


def mesh_op(r):
    kind = r.choice([1, 2, 2, 2, 2, 3, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 12])
    if kind == 2:
        return (2, [r.choice([1, 1, 1, 1, 0])])
    if kind in (10, 11):
        return (kind, [r.randrange(0, 4), r.choice([1, 1, 0]), r.getrandbits(4)])
    if kind == 12:
        return globals_op(r)
    return (kind, [])


def queues(r, fail):
    q = {"Small": [r.randrange(2) for _ in range(6)], "Tex.Close": [r.choice([1, 1, 0]) for _ in range(12)],
         "Tex.Load": [r.choice([1, 1, 0]) for _ in range(8)], "Entry": [r.choice([1, 1, 1, 0]) for _ in range(12)]}
    if fail:
        q["Malloc"] = [1 if r.random() < 0.7 else 0 for _ in range(r.randrange(1, 5))] + [1]
        q["New"] = [1 if r.random() < 0.7 else 0 for _ in range(r.randrange(1, 6))] + [1]
    return q


def case_of(r, length, fail=False, **given):
    script = [(1, []), globals_op(r)] + [mesh_op(r) for _ in range(length)]
    return dict(mesh=mesh_of(r, **given), q=queues(r, fail), script=script)


def edge():
    r = random.Random(121)
    plain = lambda **given: mesh_of(r, **given)
    # Built, initialised, closed, destroyed; no header; twice.
    yield dict(mesh=plain(), script=[(1, []), (7, []), (8, []), (2, [1]), (7, []), (2, [1]), (3, []), (3, []), (9, [])])
    yield dict(mesh=plain(), script=[(1, []), (2, [0]), (3, []), (9, [])])
    # The mark on the textures: header flag 0x200, display flag 2, width, small textures.
    for flags in (0x200, 0):
        for display in (0, 2):
            for width in (0x200, 0x280, 800):
                for small in (0, 1):
                    yield dict(mesh=plain(flags=flags, textures=2, name="mesh"), q={"Small": [small]},
                               script=[(1, []), (12, [0, display, width, 0, 0]), (2, [1]), (3, [])])
    # The height field: wanted by the header, there or not, with and without the object.
    for flags in (2, 0):
        for hfield in (False, True):
            for there in (0, 1):
                yield dict(mesh=plain(flags=flags, hfield=hfield), script=[(1, []), (12, [0, 0, 800, 0, there]), (2, [1]), (3, []), (9, [])])
    # The blueprint's face sets; delayed loading; the loading picture.
    for name in ("BlueprintMesh", "BlueprintMes", "BlueprintMeshes", "blueprintmesh", ""):
        yield dict(mesh=plain(name=name, nodes=2), script=[(1, []), (2, [1]), (3, [])])
    for settings in (0, 0x100, 0x40000000, 0x40000100, 0x20, 0x120):
        for system in (0, 1, 2, 3):
            yield dict(mesh=plain(textures=2, nodes=2), script=[(1, []), (12, [settings, 0, 800, system, 0]), (2, [1]), (4, []), (3, [])])
    # No memory: for a texture object, for the buffer of words.
    for q in ({"New": [0]}, {"New": [1, 0]}, {"Malloc": [0]}, {"Tex.Load": [0]}, {"Entry": [0, 1, 0]}, {"Tex.Close": [0]}):
        yield dict(mesh=plain(textures=3, nodes=2), q=q, script=[(1, []), (2, [1]), (3, []), (3, [])])
    # The ride base and the ride sign: with and without their flag, some old textures none.
    for kind in (10, 11):
        for count in (0, 1, 3):
            for flag in (0, 1):
                for bits in (0, 5, 15):
                    yield dict(mesh=plain(textures=2), script=[(1, []), (2, [1]), (kind, [count, flag, bits]), (5, []), (6, []),
                                                                 (5, []), (6, []), (3, [])])
        yield dict(mesh=plain(textures=3), script=[(1, []), (2, [1]), (kind, [3, 1, 7]), (3, []), (9, [])])
    for _ in range(40):
        yield case_of(r, 12)
    for _ in range(12):
        yield case_of(r, 12, fail=True)


TESTS = [
    FnTest("CMesh: construction, Init, Close and what Close undoes", ppc, host, edge=edge,
           rnd=lambda r: case_of(r, 14, fail=r.random() < 0.3)),
]
