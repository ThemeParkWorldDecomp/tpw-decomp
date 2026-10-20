"""Unit w105, the ride and mesh teardown, against libtpw_unit_RideDestroy.dylib (tests/unit_RideDestroy.cpp), in the main app
("SimTheme Park"):
  src/RideDestroy.cpp     DestroyMeshHeader 0x10064d7c, DestroyMesh 0x10064de0, DestroyRideHeightMap 0x1005e9f4,
                          DestroySkirtVertices 0x1006fbd8, DMFreeMesh 0x10030ae8, DMDisableMeshRender 0x100309c8 and 0x10030a48,
                          DestroyRideAnimSet 0x1006a5e8, DestroyRideAnimSetInstance 0x10063338, DestroyRideType 0x1006a768
  src/CoasterDestroy.cpp  DestroyCoasterInstance 0x100437cc, DestroyCoasterInstanceFromEntranceID 0x100438fc,
                          DestroyCoasterInstances 0x10043994

A case is a world and an operation, built twice from one description: in emulator memory at the Mac's layouts for the original and by
the unit's entry points (tpw_rd_*) for the translation. Every callee outside the unit is a fake that records the same event on both
sides (MReleaseFreeF frees nothing: it records the label of the block, so every block of the world is labelled). The engine mesh's
destructor is reached through a vtable (the Mac keeps its pointer at +0x150, slot +8) and records "EngineMesh.dtor" and then, for the
flag 1, the deletion of the labelled object. The case's queues make DisableMeshDraw clear the draw flag as the real one does or not.

After the call the world is dumped (the set's pointers as labels, the mesh, the nodes, the animations' and headers' blocks, the
hoarding block, the dynamic set and its texture names, and the same for the "p" variant); what the original answers in r3 is
compared for the functions that answer 1.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_RideDestroy"
MASK = 0xFFFFFFFF

DESTROY_MESH_HEADER, DESTROY_MESH, HEIGHT_MAP, SKIRT = 0x10064D7C, 0x10064DE0, 0x1005E9F4, 0x1006FBD8
DM_FREE, DM_DISABLE, DM_DISABLE_XZ = 0x10030AE8, 0x100309C8, 0x10030A48
ANIM_SET, ANIM_SET_INSTANCE, RIDE_TYPE = 0x1006A5E8, 0x10063338, 0x1006A768
COASTER_INSTANCE, COASTER_FROM_ID, COASTER_INSTANCES = 0x100437CC, 0x100438FC, 0x10043994
DISABLE_MESH_DRAW, DISABLE_MESH_DRAW_XZ, DESTROY_ENGINE_MESH, RELEASE_FREE = 0x100653A4, 0x1006548C, 0x10065004, 0x10064C54
EMBEDDED, RIDE_SIGN = 0x101FFA58, 0x101EEA14
TRAINS, TRACK, PYLONS, DESTROY_QUEUE, REMOVE_OBJECT = 0x10042344, 0x10050DF4, 0x10050E84, 0x10049228, 0x101EF310
PTR_GLUE = 0x1023BAA8
G_RIDE_TYPES = 0x102D19B0
G_COUNT, G_HEAD = 0x102B68AC, 0x102B68B4
G_TABLE_COUNT, G_TABLE_FREE, G_TABLE_LOWEST, G_TABLE = 0x102B730C, 0x102B7308, 0x102B7304, 0x102B6F04

(K_SET, K_MESH, K_HEADER, K_BLOCK14, K_SKIRT, K_HEADER_BLOCK, K_ENGINE, K_TYPE_ENGINE, K_ALLOCATION) = range(1, 10)
K_NODES, K_NODE_BLOCK = 10, 11
K_ANIM_TABLE, K_ANIM, K_ANIM44, K_ENTRY_ALLOC, K_HEADER_ARRAY, K_HEADER_OF, K_HEADER_OF_BLOCK = 15, 16, 19, 22, 23, 24, 27
K_HOARD, K_HOARD_BLOCK, K_HEIGHT_MAP, K_SELF_ALLOC = 30, 31, 32, 33
K_DM, K_DM_ENTRIES, K_DM_ALLOC, K_DM_BLOCK20, K_DM_MESH, K_DM_HEADER, K_DM_MESH_ALLOC, K_DM_INFOS, K_DM_NAME = range(34, 43)
K_SHARED, K_STRAY, K_VARIANT = 45, 46, 50


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def pattern(n, fill):
    return bytes([fill & 0xFF]) * n


# ---------------------------------------------------------------------------------------------------------------- the fakes


def build_stubs():
    S = CL.Stubs()

    def R(name, spec):
        # The recorder, with the trace of the world (two hashes) added to the event it recorded.
        inner = CL.recorder(name, spec)

        def stub(e):
            inner(e)
            n, ints, texts = e.cl.log[-1]
            e.cl.log[-1] = (n, ints + e.rd.trace(), texts)
        return stub
    S.real = {DESTROY_MESH_HEADER, DESTROY_MESH, HEIGHT_MAP, SKIRT, DM_FREE, DM_DISABLE, DM_DISABLE_XZ, ANIM_SET, ANIM_SET_INSTANCE,
              RIDE_TYPE, COASTER_INSTANCE, COASTER_FROM_ID, COASTER_INSTANCES, PTR_GLUE}

    def disable(e):
        e.cl.rec("DisableMeshDraw", (e.cl.lab(e.r(3)),) + e.rd.trace())
        if e.cl.ret("Disable", 0) == 1:
            ws(e, e.r(3), [e.u32(e.r(3)) & ~1])

    def disable_xz(e):
        e.cl.rec("DisableMeshDrawXZ", (e.cl.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK) + e.rd.trace())
        if e.cl.ret("DisableXZ", 0) == 1:
            ws(e, e.r(3), [e.u32(e.r(3)) & ~1])

    def remove_object(e):
        which = {G_TABLE_COUNT: 1, G_TABLE_FREE: 2, G_TABLE_LOWEST: 3}
        e.cl.rec("RemoveObjectInstanceFromTable",
                 (which.get(e.r(3), 0), which.get(e.r(4), 0), which.get(e.r(5), 0), int(e.r(6) == G_TABLE), e.r(7) & MASK,
                  e.u32(e.r(3)), e.u32(e.r(4)), e.u32(e.r(5)), T.s32(e.u32(G_COUNT))) + e.rd.trace())
        relink = e.cl.ret("Relink", 0)
        if relink > 0:
            ws(e, G_HEAD, [e.rd.coasters[relink - 1]])

    S.internal(DISABLE_MESH_DRAW, disable)
    S.internal(DISABLE_MESH_DRAW_XZ, disable_xz)
    S.internal(DESTROY_ENGINE_MESH, R("DestroyEngineMesh", "L"))
    S.internal(RELEASE_FREE, R("MReleaseFreeF", "L"))
    S.internal(EMBEDDED, R("TPDestroyEmbeddedData", "L"))
    S.internal(RIDE_SIGN, R("DestroyRideSign", "L"))
    S.imp("Close__5CMeshFv", R("CMesh.Close", "L"))
    def trains(e):
        R("DestroyCoasterTrains", "L")(e)
        if e.cl.ret("Poison", 0) == 1:
            ws(e, e.r(3) + 0x18, [0])

    S.internal(TRAINS, trains)
    S.internal(TRACK, R("DestroyCoasterTrack", "L"))
    S.internal(PYLONS, R("DestroyCoasterPylons", "L"))
    S.internal(DESTROY_QUEUE, R("DestroyQueue", "L"))
    S.internal(REMOVE_OBJECT, remove_object)
    return S


STUBS = build_stubs()


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if hasattr(e, "rd_engine_vtable"):
        return
    e.rd = None
    # The engine mesh's vtable (the pointer is at +0x150): slot +8 is the deleting destructor.
    engine_dtor = T.fake_function(e, lambda em: CL.recorder("EngineMesh.dtor", "D")(em))
    e.rd_engine_vtable = T.persist_alloc(e, 0x40)
    ws(e, e.rd_engine_vtable + 8, [engine_dtor])


class Ppc:
    """One case on the PowerPC side: the fakes active, a fresh log."""

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case.get("q"))
        self.worlds = []
        self.coasters = None
        e.rd = self
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)
        self.e.rd = None

    def trace(self):
        """The hashes of the world as it is now (the same arithmetic as the unit's Trace)."""
        if self.coasters is not None:
            e, L = self.e, self.L
            values = [T.s32(e.u32(G_COUNT)), L.lab(e.u32(G_HEAD))] + [L.lab(e.u32(c + 0x18)) for c in self.coasters]
            values += [e.u32(G_TABLE_COUNT), e.u32(G_TABLE_FREE), e.u32(G_TABLE_LOWEST)]
            return (hash_of(values), 0)
        out = []
        for w in self.worlds:
            values = []
            snapshot(self, w, values)
            out.append(hash_of(values))
        return tuple(out) + (0,) * (2 - len(out))

    def call(self, address, *args):
        return T.call_ppc(self.e, address, list(args))[0]

    def new(self, size, label, fill=0xA5):
        address = self.e.alloc(size, pattern(size, fill))
        self.L.labels[address] = label
        return address

    def block(self, label):
        return self.new(16, label, 0x33)

    def engine(self, label):
        address = self.e.alloc(0x160, pattern(0x160, 0x77))
        ws(self.e, address + 0x150, [self.e.rd_engine_vtable])
        self.L.labels[address] = label
        return address


def hash_of(values):
    h = 0
    for v in values:
        h = (h * 1000003 + (v & MASK)) & ((1 << 61) - 1)
    return h


def host_run(lib, entry, description, queues=None):
    fn = getattr(lib, entry)
    fn.restype = C.c_int32
    fn.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    values = (C.c_int64 * len(description))(*description)
    out = (C.c_int64 * 2048)()
    CL.host_begin(lib, {}, queues)
    n = fn(values, out)
    log = CL.host_end(lib)
    return [int(out[i]) for i in range(n)], log


# ---------------------------------------------------------------------------------------------------------------- the set world


def build_mesh(p, it, base):
    e = p.e
    flags, engine, type_engine, engine_count, block14, skirt, header_block, allocation, node_count, root = [next(it) for _ in range(10)]
    node_flags = [next(it) for _ in range(3)]
    node_blocks = [next(it) for _ in range(3)]
    w = dict(count=node_count)
    mesh = w["mesh"] = p.new(0x40, base + K_MESH)
    header = w["header"] = p.new(0xB8, base + K_HEADER)
    ws(e, mesh, [flags])
    ws(e, mesh + 4, [header])
    ws(e, mesh + 0x14, [p.block(base + K_BLOCK14) if block14 else 0])
    ws(e, mesh + 0x18, [p.block(base + K_SKIRT) if skirt else 0])
    ws(e, mesh + 0x1C, [p.engine(base + K_ENGINE) if engine else 0])
    ws(e, mesh + 0x20, [p.engine(base + K_TYPE_ENGINE) if type_engine else 0])
    ws(e, mesh + 0x24, [p.block(base + K_ALLOCATION) if allocation else 0])
    ws(e, mesh + 0x34, [engine_count])
    e.write(header + 0x44, struct.pack(">H", node_count))
    ws(e, header + 0xB0, [p.block(base + K_HEADER_BLOCK) if header_block else 0])
    nodes = 0
    if node_count > 0:
        nodes = w["nodes"] = p.new(0xA0 * node_count, base + K_NODES)
        for i in range(node_count):
            ws(e, nodes + 0xA0 * i, [node_flags[i]])
            ws(e, nodes + 0xA0 * i + 0x98, [p.block(base + K_NODE_BLOCK + i) if node_blocks[i] else 0])
    ws(e, header + 0x70, [nodes])
    stray = p.new(0xA0, base + K_STRAY)
    ws(e, mesh + 0x2C, [0 if root == -1 else stray if root == -2 else nodes + 0xA0 * root])
    return w


def build_dm(p, it, base, build):
    e = p.e
    flags, entries, allocation, block20, shared, tex_count, names, mesh_flags, mesh_allocation = [next(it) for _ in range(9)]
    if not build:
        return None
    w = dict(count=tex_count)
    dm = w["dm"] = p.new(0x40, base + K_DM)
    mesh = w["mesh"] = p.new(0x40, base + K_DM_MESH)
    header = p.new(0xB8, base + K_DM_HEADER)
    sh = w["shared"] = p.new(0x20, base + K_SHARED)
    infos = w["infos"] = p.new(0x30, base + K_DM_INFOS)
    for i in range(3):
        ws(e, infos + 0x10 * i + 0xC, [p.block(base + K_DM_NAME + i) if (names >> i) & 1 else 0])
    e.write(header + 0x36, struct.pack(">H", tex_count))
    ws(e, header + 0x54, [infos])
    ws(e, sh + 0x1C, [shared])
    ws(e, mesh, [mesh_flags])
    ws(e, mesh + 4, [header])
    ws(e, mesh + 0x24, [p.block(base + K_DM_MESH_ALLOC) if mesh_allocation else 0])
    ws(e, dm, [flags])
    ws(e, dm + 8, [p.block(base + K_DM_ENTRIES) if entries else 0])
    ws(e, dm + 0x20, [p.block(base + K_DM_BLOCK20) if block20 else 0])
    ws(e, dm + 0x24, [mesh])
    ws(e, dm + 0x34, [p.block(base + K_DM_ALLOC) if allocation else 0])
    ws(e, dm + 0x3C, [sh])
    return w


def build_set(p, it, base):
    e = p.e
    (flags, anim_count, anim_mask, table, header_array, header_blocks, entry_alloc, hoard, hoard_block, height_map, self_alloc, fc0, fc4,
     has_dm) = [next(it) for _ in range(14)]
    w = dict(anims=[0, 0, 0], headers=[0, 0, 0], hoard=0, has_dm=bool(has_dm))
    w["mesh"] = build_mesh(p, it, base)
    w["dm"] = build_dm(p, it, base, bool(has_dm))
    s = w["set"] = p.new(0x100, base + K_SET)
    ws(e, s + 4, [flags])
    ws(e, s + 8, [w["mesh"]["mesh"]])
    ws(e, s + 0xA8, [anim_count])
    if table:
        array = p.new(12, base + K_ANIM_TABLE)
        for i in range(3):
            if (anim_mask >> i) & 1:
                anim = w["anims"][i] = p.new(0x48, base + K_ANIM + i)
                ws(e, anim + 0x44, [p.block(base + K_ANIM44 + i)])
            ws(e, array + 4 * i, [w["anims"][i]])
        ws(e, s + 0xAC, [array])
    else:
        ws(e, s + 0xAC, [0])
    if header_array:
        array = p.new(12, base + K_HEADER_ARRAY)
        for i in range(3):
            header = w["headers"][i] = p.new(0xB8, base + K_HEADER_OF + i)
            ws(e, header + 0xB0, [p.block(base + K_HEADER_OF_BLOCK + i) if (header_blocks >> i) & 1 else 0])
            ws(e, array + 4 * i, [header])
        ws(e, s + 0xDC, [array])
    else:
        ws(e, s + 0xDC, [0])
    ws(e, s + 0xD8, [p.block(base + K_ENTRY_ALLOC) if entry_alloc else 0])
    if hoard:
        h = w["hoard"] = p.new(0x10, base + K_HOARD)
        ws(e, h + 0xC, [p.block(base + K_HOARD_BLOCK) if hoard_block else 0])
        ws(e, s + 0xB0, [h])
    else:
        ws(e, s + 0xB0, [0])
    ws(e, s + 0xB4, [w["dm"]["dm"] if has_dm else 0])
    ws(e, s + 0xCC, [p.block(base + K_HEIGHT_MAP) if height_map else 0])
    ws(e, s + 0xD0, [0])
    ws(e, s + 0xE0, [p.block(base + K_SELF_ALLOC) if self_alloc else 0])
    ws(e, s + 0xC0, [fc0])
    ws(e, s + 0xC4, [fc4])
    return w


def snapshot(p, w, out):
    e, L = p.e, p.L
    lab = L.lab
    s = w["set"]
    out.append(e.u32(s + 4))
    for offset in (0xAC, 0xDC, 0xD8, 0xB4, 0xB0, 0xCC, 0xD0, 0xE0):
        out.append(lab(e.u32(s + offset)))
    out.append(lab(e.u32(w["hoard"] + 0xC)) if w["hoard"] else -5)
    m = w["mesh"]
    mesh = m["mesh"]
    out.append(e.u32(mesh))
    for offset in (4, 0x14, 0x18, 0x1C, 0x20, 0x24):
        out.append(lab(e.u32(mesh + offset)))
    out.append(T.s32(e.u32(mesh + 0x34)))
    out.append(lab(e.u32(m["header"] + 0xB0)))
    for i in range(3):
        if i < m["count"]:
            out.append(e.u32(m["nodes"] + 0xA0 * i))
            out.append(lab(e.u32(m["nodes"] + 0xA0 * i + 0x98)))
        else:
            out += [-5, -5]
    for i in range(3):
        out.append(lab(e.u32(w["anims"][i] + 0x44)) if w["anims"][i] else -5)
    for i in range(3):
        out.append(lab(e.u32(w["headers"][i] + 0xB0)) if w["headers"][i] else -5)
    if w["has_dm"]:
        d = w["dm"]
        dm, dmesh = d["dm"], d["mesh"]
        out.append(e.u32(dm))
        for offset in (8, 0x20, 0x34, 0x24):
            out.append(lab(e.u32(dm + offset)))
        out.append(e.u32(d["shared"] + 0x1C))
        out.append(e.u32(dmesh))
        out.append(lab(e.u32(dmesh + 0x24)))
        for i in range(3):
            out.append(lab(e.u32(d["infos"] + 0x10 * i + 0xC)) if i < d["count"] else -5)
    else:
        out += [-5] * 11


# description: op, mode, type, heap, has variant, then the set and the variant (14 + 16 + 9 numbers each)
def set_ppc(e, case):
    p = Ppc(e, case)
    try:
        d = case["d"]
        op, mode, type_, heap, has_variant = d[:5]
        it = iter(d[5:])
        world = build_set(p, it, 0)
        variant = build_set(p, it, K_VARIANT)
        p.worlds = [world, variant]
        if has_variant:
            ws(e, world["set"] + 0xD0, [variant["set"]])
        ws(e, G_RIDE_TYPES + 4 * type_, [world["set"]])
        s, mesh = world["set"], world["mesh"]["mesh"]
        dm = world["dm"]["dm"] if world["has_dm"] else 0
        answer = 1
        if op == 0:
            p.call(DESTROY_MESH_HEADER, world["mesh"]["header"])
        elif op == 1:
            p.call(DESTROY_MESH, mesh)
        elif op == 2:
            p.call(HEIGHT_MAP, s)
        elif op == 3:
            p.call(SKIRT, s)
        elif op == 4:
            answer = p.call(DM_FREE, dm) & MASK
        elif op == 5:
            answer = p.call(DM_DISABLE, dm) & MASK
        elif op == 6:
            answer = p.call(DM_DISABLE_XZ, dm, e.u32(s + 0xC0), e.u32(s + 0xC4)) & MASK
        elif op == 7:
            p.call(ANIM_SET, s, mode & MASK)
        elif op == 8:
            result = p.call(ANIM_SET_INSTANCE, s, heap & MASK, mode & MASK)
            answer = 1 if result == 1 else result & MASK
        elif op == 9:
            p.call(RIDE_TYPE, type_)
        state = [answer]
        snapshot(p, world, state)
        snapshot(p, variant, state)
        return dict(state=state, log=p.L.log)
    finally:
        p.done()


def set_host(lib, case):
    state, log = host_run(lib, "tpw_rd_set", case["d"], case.get("q"))
    return dict(state=state, log=log)


# ---------------------------------------------------------------------------------------------------------------- the coasters
# description: op, target, linked, count, the table's three counters, then five (entrance id, object number)
def coaster_ppc(e, case):
    p = Ppc(e, case)
    try:
        L = p.L
        d = case["d"]
        op, target, linked, count = d[:4]
        ws(e, G_COUNT, [count])
        ws(e, G_TABLE_COUNT, [d[4]])
        ws(e, G_TABLE_FREE, [d[5]])
        ws(e, G_TABLE_LOWEST, [d[6]])
        coasters = []
        for i in range(5):
            c = e.alloc(0x100, pattern(0x100, 0xA5))
            L.labels[c] = 200 + 10 * i
            L.labels[c + 0xB0] = 201 + 10 * i
            L.labels[c + 0xD0] = 202 + 10 * i
            ws(e, c + 0x10, [d[7 + 2 * i], d[8 + 2 * i]])
            ws(e, c + 0x18, [0])
            coasters.append(c)
        for i in range(linked - 1):
            ws(e, coasters[i] + 0x18, [coasters[i + 1]])
        p.coasters = coasters
        ws(e, G_HEAD, [coasters[0] if linked > 0 else 0])
        if op == 0:
            p.call(COASTER_INSTANCE, coasters[target])
        elif op == 1:
            p.call(COASTER_FROM_ID, target & MASK)
        else:
            p.call(COASTER_INSTANCES)
        state = [T.s32(e.u32(G_COUNT)), L.lab(e.u32(G_HEAD))]
        state += [L.lab(e.u32(c + 0x18)) for c in coasters]
        state += [e.u32(G_TABLE_COUNT), e.u32(G_TABLE_FREE), e.u32(G_TABLE_LOWEST)]
        return dict(state=state, log=L.log)
    finally:
        p.done()


def coaster_host(lib, case):
    state, log = host_run(lib, "tpw_rd_coaster", case["d"], case.get("q"))
    return dict(state=state, log=log)


# ---------------------------------------------------------------------------------------------------------------- descriptions

MESH_FLAGS = [0, 1, 2, 3, 0x4000, 0x4001, MASK, 0x80000001]
NODE_FLAGS = [0, 0x1000, 0x1001, 0xFFFFEFFF, 0x2000, MASK, 0x1000 | 0x20000]
COUNTS = [0, 1, -1, 2, 5, 0x7FFFFFFF, -2 ** 31]
WORDS = [0, 0x00010002, 0xFFFF0000, 0x0000FFFF, 0x80008000, MASK]


def mesh_desc(**k):
    d = dict(flags=1, engine=1, type_engine=1, engine_count=0, block14=1, skirt=1, header_block=1, allocation=1, nodes=3, root=1,
             nflags=[0x1000, 0x1000, 0x1000], nblocks=[1, 1, 1])
    d.update(k)
    return ([d["flags"], d["engine"], d["type_engine"], d["engine_count"], d["block14"], d["skirt"], d["header_block"], d["allocation"],
             d["nodes"], d["root"]] + list(d["nflags"]) + list(d["nblocks"]))


def dm_desc(**k):
    d = dict(flags=0x4000, entries=1, alloc=1, block20=1, shared=1, tex=3, names=7, mflags=1, malloc=1)
    d.update(k)
    return [d["flags"], d["entries"], d["alloc"], d["block20"], d["shared"], d["tex"], d["names"], d["mflags"], d["malloc"]]


def set_desc(mesh=None, dm=None, **k):
    d = dict(flags=0, anims=2, anim_mask=3, table=1, header_array=1, header_blocks=3, entry=1, hoard=1, hoard_block=1, height=1, selfa=1,
             fc0=0x00010002, fc4=0x00030004, has_dm=1)
    d.update(k)
    if d["anims"] > 0:
        d["table"] = d["header_array"] = 1
    return ([d["flags"], d["anims"], d["anim_mask"], d["table"], d["header_array"], d["header_blocks"], d["entry"], d["hoard"],
             d["hoard_block"], d["height"], d["selfa"], d["fc0"], d["fc4"], d["has_dm"]] + (mesh or mesh_desc()) + (dm or dm_desc()))


def set_case(op, mode=0, type_=7, heap=0, variant=0, main=None, other=None, disable=None, disable_xz=None):
    main = main or set_desc()
    other = other or set_desc()
    q = {}
    if disable is not None:
        q["Disable"] = disable
    if disable_xz is not None:
        q["DisableXZ"] = disable_xz
    return dict(d=[op, mode, type_, heap, variant] + main + other, q=q)


def rnd_mesh(r):
    count = r.randint(0, 3)
    root = r.choice([-1, -2] + list(range(count)))
    return mesh_desc(flags=r.choice(MESH_FLAGS + [r.getrandbits(32)]), engine=r.randint(0, 1), type_engine=r.randint(0, 1),
                     engine_count=r.choice(COUNTS + [r.randint(-100, 100)]), block14=r.randint(0, 1), skirt=r.randint(0, 1),
                     header_block=r.randint(0, 1), allocation=r.randint(0, 1), nodes=count, root=root,
                     nflags=[r.choice(NODE_FLAGS + [r.getrandbits(32)]) for _ in range(3)], nblocks=[r.randint(0, 1) for _ in range(3)])


def rnd_dm(r):
    return dm_desc(flags=r.choice([0, 0x4000, 0x2000, 0x6000, MASK, r.getrandbits(32)]), entries=r.randint(0, 1), alloc=r.randint(0, 1),
                   block20=r.randint(0, 1), shared=r.choice([0, 1, 2, 3, MASK, 0x80000000, r.getrandbits(32)]), tex=r.randint(0, 3),
                   names=r.randint(0, 7), mflags=r.choice(MESH_FLAGS + [r.getrandbits(32)]), malloc=r.randint(0, 1))


def rnd_set(r, need_dm=False):
    return set_desc(mesh=rnd_mesh(r), dm=rnd_dm(r), flags=r.choice([0, 0x4000, 0x20, 0x4020, MASK, r.getrandbits(32)]),
                    anims=r.randint(0, 3), anim_mask=r.randint(0, 7), table=r.randint(0, 1), header_array=r.randint(0, 1),
                    header_blocks=r.randint(0, 7), entry=r.randint(0, 1), hoard=r.randint(0, 1), hoard_block=r.randint(0, 1),
                    height=r.randint(0, 1), selfa=r.randint(0, 1), fc0=r.choice(WORDS + [r.getrandbits(32)]),
                    fc4=r.choice(WORDS + [r.getrandbits(32)]), has_dm=1 if need_dm else r.randint(0, 1))


def queue(r):
    return [r.randint(0, 1) for _ in range(r.randint(0, 4))] or None


def rnd_case(op):
    def make(r):
        need_dm = op in (4, 5, 6)
        return set_case(op, mode=r.choice([0, 1, -1, 2, r.getrandbits(32) - 2 ** 31]), type_=r.randint(1, 329),
                        heap=r.getrandbits(32), variant=r.randint(0, 1), main=rnd_set(r, need_dm), other=rnd_set(r),
                        disable=queue(r), disable_xz=queue(r))
    return make


def edge_cases(op):
    def gen():
        need_dm = op in (4, 5, 6)
        yield set_case(op)
        # every flag of the set's description off, one at a time
        for key in ("anim_mask", "header_blocks", "entry", "hoard", "hoard_block", "height", "selfa"):
            yield set_case(op, main=set_desc(**{key: 0}))
        for key in ("table", "header_array"):
            yield set_case(op, main=set_desc(anims=0, **{key: 0}))
        yield set_case(op, main=set_desc(anims=0))
        yield set_case(op, main=set_desc(anims=1, anim_mask=1, header_blocks=1))
        yield set_case(op, main=set_desc(anims=3, anim_mask=5, header_blocks=6))
        yield set_case(op, main=set_desc(anims=3, anim_mask=7, header_blocks=7))
        yield set_case(op, main=set_desc(flags=0x4000))
        yield set_case(op, main=set_desc(flags=0x3FFF | 0xFFFF8000))
        yield set_case(op, main=set_desc(fc0=MASK, fc4=0x80000001))
        if not need_dm:
            yield set_case(op, main=set_desc(has_dm=0))
        # the set's mesh
        for key, values in (("flags", MESH_FLAGS), ("engine", (0, 1)), ("type_engine", (0, 1)), ("engine_count", COUNTS),
                            ("block14", (0, 1)), ("skirt", (0, 1)), ("header_block", (0, 1)), ("allocation", (0, 1))):
            for value in values:
                yield set_case(op, main=set_desc(mesh=mesh_desc(**{key: value})))
        for count in range(4):
            for root in [-1, -2] + list(range(count)):
                for flags in (0x1000, 0, 0x1001, 0xFFFFEFFF):
                    yield set_case(op, main=set_desc(mesh=mesh_desc(nodes=count, root=root, nflags=[flags] * 3, nblocks=[1, 0, 1])))
        for bits in range(8):
            yield set_case(op, main=set_desc(mesh=mesh_desc(nodes=3, root=-1, nflags=[0x1000 if (bits >> i) & 1 else 0 for i in range(3)])))
            yield set_case(op, main=set_desc(mesh=mesh_desc(nodes=3, root=2, nflags=[0x1000] * 3, nblocks=[(bits >> i) & 1 for i in range(3)])))
        # the dynamic set
        for key, values in (("flags", (0, 0x4000, 0x2000, 0x6000, MASK, 0xFFFFBFFF)), ("entries", (0, 1)), ("alloc", (0, 1)),
                            ("block20", (0, 1)), ("shared", (0, 1, 2, 3, MASK, 0x80000000)), ("tex", (0, 1, 2, 3)),
                            ("mflags", MESH_FLAGS), ("malloc", (0, 1))):
            for value in values:
                yield set_case(op, main=set_desc(dm=dm_desc(**{key: value})))
        for names in range(8):
            yield set_case(op, main=set_desc(dm=dm_desc(shared=1, names=names, tex=3)))
            yield set_case(op, main=set_desc(dm=dm_desc(shared=2, names=names, tex=3)))
        # the queues of DisableMeshDraw
        for disable in ([0], [1], [0, 1], [1, 1], [1, 0]):
            for disable_xz in ([0], [1]):
                for flags, mflags in ((0x4000, 1), (0, 1), (0x4000, 0), (0, 3)):
                    yield set_case(op, main=set_desc(flags=flags, mesh=mesh_desc(flags=mflags)), disable=disable, disable_xz=disable_xz)
        # the "p" variant and the type number
        for variant in (0, 1):
            for type_ in (1, 7, 329):
                yield set_case(op, type_=type_, variant=variant)
        yield set_case(op, variant=1, other=set_desc(anims=3, anim_mask=7, header_blocks=7, flags=0x4000))
        yield set_case(op, variant=1, other=set_desc(height=0, hoard=0, has_dm=0, mesh=mesh_desc(type_engine=0, engine=0)))
        for mode in (0, 1, -1, 2, 0x80000000 - 2 ** 32):
            yield set_case(op, mode=mode)
            yield set_case(op, mode=mode, heap=0xA5A5A5A5)
    return gen


def coaster_desc(op, target=0, linked=0, count=0, counters=(0, 0, 0), ids=None, relink=None, poison=None):
    ids = ids or [(i + 1, 10 + i) for i in range(5)]
    d = [op, target, linked, count] + list(counters)
    for pair in ids:
        d += list(pair)
    q = {}
    if relink:
        q["Relink"] = relink
    if poison:
        q["Poison"] = poison
    return dict(d=d, q=q)


def coaster_edges(op):
    def gen():
        counters = (5, 2, 9)
        for linked in range(5):
            if op == 0:
                for target in range(5):
                    yield coaster_desc(0, target, linked, 7, counters)
            elif op == 1:
                for target in (1, 2, 3, 4, 5, 6, 0, MASK):
                    yield coaster_desc(1, target, linked, 7, counters)
            else:
                yield coaster_desc(2, 0, linked, 7, counters)
        # duplicate entrance ids: the first match goes
        if op == 1:
            for target in (1, 2, 3):
                yield coaster_desc(1, target, 4, 3, counters, ids=[(1, 11), (2, 12), (2, 13), (1, 14), (3, 15)])
        # the fakes change the list behind the function's back: the head moves (the table removal), the destroyed instance's next
        # pointer is trashed (the trains' destroyer)
        for relink in (1, 2, 3, 4, 5):
            for poison in (0, 1):
                yield coaster_desc(op, 1, 4, 3, counters, relink=[relink], poison=[poison])
                yield coaster_desc(op, 1, 4, 3, counters, relink=[0, relink, 0], poison=[0, poison, 1])
        yield coaster_desc(op, 0, 3, 0, (0, 0, 0))
        yield coaster_desc(op, 0, 3, -1, (MASK, MASK, MASK))
        yield coaster_desc(op, 1, 3, 2 ** 31 - 1, (1, 0, 1), ids=[(1, 0), (1, 0), (1, MASK), (0, 0), (0, 0)])
    return gen


def coaster_rnd(op):
    def make(r):
        ids = [(r.choice([0, 1, 2, 3, MASK, r.getrandbits(32)]), r.choice([0, 1, 2, 3, MASK, r.getrandbits(32)])) for _ in range(5)]
        target = r.randint(0, 4) if op == 0 else r.choice([0, 1, 2, 3, MASK] + [i for i, _ in ids])
        relink = [r.choice([0, 0, 0, 1, 2, 3, 4, 5]) for _ in range(r.randint(0, 4))] or None
        poison = [r.choice([0, 0, 1]) for _ in range(r.randint(0, 4))] or None
        return coaster_desc(op, target, r.randint(0, 4), r.choice([0, 1, 7, -1, r.randint(-1000, 1000)]),
                            tuple(r.choice([0, 1, MASK, r.getrandbits(32)]) for _ in range(3)), ids, relink, poison)
    return make


TESTS = [
    FnTest("DestroyMeshHeader", set_ppc, set_host, edge=edge_cases(0), rnd=rnd_case(0)),
    FnTest("DestroyMesh", set_ppc, set_host, edge=edge_cases(1), rnd=rnd_case(1)),
    FnTest("DestroyRideHeightMap", set_ppc, set_host, edge=edge_cases(2), rnd=rnd_case(2)),
    FnTest("DestroySkirtVertices", set_ppc, set_host, edge=edge_cases(3), rnd=rnd_case(3)),
    FnTest("DMFreeMesh", set_ppc, set_host, edge=edge_cases(4), rnd=rnd_case(4)),
    FnTest("DMDisableMeshRender", set_ppc, set_host, edge=edge_cases(5), rnd=rnd_case(5)),
    FnTest("DMDisableMeshRender (places)", set_ppc, set_host, edge=edge_cases(6), rnd=rnd_case(6)),
    FnTest("DestroyRideAnimSet", set_ppc, set_host, edge=edge_cases(7), rnd=rnd_case(7)),
    FnTest("DestroyRideAnimSetInstance", set_ppc, set_host, edge=edge_cases(8), rnd=rnd_case(8)),
    FnTest("DestroyRideType", set_ppc, set_host, edge=edge_cases(9), rnd=rnd_case(9)),
    FnTest("DestroyCoasterInstance", coaster_ppc, coaster_host, edge=coaster_edges(0), rnd=coaster_rnd(0)),
    FnTest("DestroyCoasterInstanceFromEntranceID", coaster_ppc, coaster_host, edge=coaster_edges(1), rnd=coaster_rnd(1)),
    FnTest("DestroyCoasterInstances", coaster_ppc, coaster_host, edge=coaster_edges(2), rnd=coaster_rnd(2)),
]
