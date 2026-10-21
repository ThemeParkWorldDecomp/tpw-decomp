"""The game's mesh loading: LoadMesh, CreateMeshTypeEngineMesh, the mesh paths (GetMeshPath,
Get/SetFileAccessIndex, BuildSharedTexturePath, SetNewMeshPaths, DestroyPathAllocs,
OverrideTexturePaths), CreateMeshFromM3D2Header, CreateEngineMesh (main program,
0x100b7d04..0x100b9140 and 0x10064cf0, 0x10064f40; src/game/MeshLoad.cpp, src/game/TPMesh.cpp).

A case is a script over the three mesh folders, a mesh header with a few textures that
the test lays out on both sides, and two mesh records. After every operation the test
compares the result, the fakes' events (the mesh file loader, the engine mesh object's
construction and Init, the allocator, CheckFileExists with the path it is asked about,
the quickload index) and the state: the folder table and its indices as texts, the
header's table pointer and count, every texture's folder as text, the mesh records.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_MeshLoad"

FN = dict(LoadMesh=0x100b7d04, CreateMeshTypeEngineMesh=0x100b7dd4, GetMeshPath=0x100b88a0, GetFileAccessIndex=0x100b88f0,
          SetFileAccessIndex=0x100b8928, BuildSharedTexturePath=0x100b8960, SetNewMeshPaths=0x100b89dc,
          DestroyPathAllocs=0x100b8df4, OverrideTexturePaths=0x100b8ea8, CreateMeshFromM3D2Header=0x10064cf0,
          CreateEngineMesh=0x10064f40)
ALLOC, FREE, INDEXED, EXISTS, THEME_PATH = 0x10064bd4, 0x10064c54, 0x10067b50, 0x100b98a4, 0x1015139c
ALLOCS, ACCESS, COUNT, TABLE, MESH_INDEX, TEXTURE_INDEX, SHARED_INDEX, QUICKLOAD, SHARED_NAME = (
    0x102e49dc, 0x102e49e0, 0x102e49e4, 0x102e49e8, 0x1029eb6c, 0x1029eb70, 0x1029eb74, 0x102c79a0, 0x103963bc)
FOLDERS = ["rides:coaster", "rides:coaster:", "textures", "shared:", "x"]
SKIPPED = -8


def setup(runner):
    T.install_std_stubs(runner.emu)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def image(names, order, ptr):
    """A mesh header with one texture info per name; ptr(offset): the side's pointer."""
    infos = 0xb8
    texts = infos + 16 * len(names)
    data = bytearray(texts + 0x14 * len(names))
    struct.pack_into(order + "H", data, 0x36, len(names))
    struct.pack_into(order + "I", data, 0x54, ptr(infos))
    for i, name in enumerate(names):
        struct.pack_into(order + "I", data, infos + 16 * i + 0xc, ptr(texts + 0x14 * i))
        data[texts + 0x14 * i:texts + 0x14 * i + len(name)] = name.encode()
    return bytes(data)


class PpcSide:
    def __init__(self, e, case):
        self.e, self.names = e, case["names"]
        self.L = CL.PpcLog({}, case.get("q"))
        e.cl = self.L
        size = len(image(self.names, ">", lambda o: 0))
        self.header = e.alloc(size)
        e.write(self.header, image(self.names, ">", lambda o: self.header + o))
        self.theme = e.alloc(32, b"disc:levels:fantasy\0")
        self.meshes = [0, 0]
        self.folders_valid = False      # the textures' folders point into tables that still exist

    def alloc(self, e):
        L, size, flags = self.L, e.r(3), e.r(4)
        L.rec("Alloc", (size, flags))
        block = e.alloc(size + 16, (b"\0" if flags & 1 else b"\xa5") * (size + 16))
        L.newlab(block)
        e.set_r(3, block)

    def free(self, e):
        self.L.rec("Free", (self.L.lab(e.r(3)),))
        self.L.forget(e.r(3))
        e.set_r(3, 0)

    def mesh_ctor(self, e):
        self.L.rec("Mesh.ctor", (self.L.lab(e.r(3)),))

    def mesh_init(self, e):
        header = e.r(4)
        self.L.rec("Mesh.Init", (self.L.lab(e.r(3)), 0 if header == 0 else 1 if header == self.header else -1))
        e.set_r(3, 1)

    def op_new(self, e):
        block = e.alloc(e.r(3), bytes(e.r(3)))
        self.L.newlab(block)
        e.set_r(3, block)

    def stubs(self):
        e, L = self.e, self.L
        st = CL.Stubs()
        st.real = set(FN.values())
        st.internal(ALLOC, self.alloc)
        st.internal(FREE, self.free)
        st.internal(EXISTS, lambda e: (L.rec("CheckFileExists", (), (e.cstr(e.r(3)),)), e.set_r(3, L.ret("Exists", 0))))
        st.internal(INDEXED, lambda e: (L.rec("CheckIndexedFileExists", (T.s32(e.r(4)), T.s32(e.r(5))), (e.cstr(e.r(3)),)),
                                        e.set_r(3, L.ret("Indexed", 0))))
        st.internal(THEME_PATH, lambda e: e.set_r(3, self.theme))
        st.imp("LoadM3D2Header__FPcUl", lambda e: (L.rec("LoadM3D2Header", (e.r(4),), (e.cstr(e.r(3)),)),
                                                   e.set_r(3, self.header if L.ret("Load", 1) else 0)))
        st.imp("__ct__5CMeshFv", self.mesh_ctor)
        st.imp("Init__5CMeshFP15tag_sM3D2Header", self.mesh_init)
        st.imp("__nw__FUl", self.op_new)

        def strcmp(e):
            a, b = e.read(e.r(3), 1100).split(b"\0")[0], e.read(e.r(4), 1100).split(b"\0")[0]
            e.set_r(3, (a > b) - (a < b))

        def strcat(e):
            e.write(e.r(3), e.read(e.r(3), 1100).split(b"\0")[0] + e.read(e.r(4), 1100).split(b"\0")[0] + b"\0")

        st.imp("strcmp", strcmp)
        st.imp("strcat", strcat)
        return st

    def reset(self):
        e = self.e
        for a, v in ((ALLOCS, 0), (ACCESS, 0), (COUNT, 0), (TABLE, 0), (MESH_INDEX, -1), (TEXTURE_INDEX, -1),
                     (SHARED_INDEX, -1), (QUICKLOAD, 0)):
            w32(e, a, v)
        e.write(SHARED_NAME, b"sharedtex\0")

    def call(self, name, *args):
        return T.call_ppc(self.e, FN[name], list(args))[0]

    def folder(self, which):
        return self.e.alloc(32, FOLDERS[which].encode() + b"\0") if which >= 0 else 0

    def table_ready(self):
        """OverrideTexturePaths falls back on the table's first folder: there must be one."""
        return not self.names or self.e.u32(TABLE) != 0

    def op(self, op, a):
        e, L = self.e, self.L
        if op == 1:
            root = e.alloc(16, (b"disc:" if a[0] else b"hd:data:") + b"\0")
            self.call("SetNewMeshPaths", root, self.folder(a[1]), self.folder(a[2]), self.folder(a[3]))
            return 0, ""
        if op == 2:
            self.call("DestroyPathAllocs")
            self.folders_valid = False
            return 0, ""
        if op == 3:
            path = self.call("GetMeshPath")
            return 0, e.cstr(path) if path else "(null)"
        if op == 4:
            self.call("SetFileAccessIndex", a[0])
            return T.s32(self.call("GetFileAccessIndex")), ""
        if op == 5:
            out = e.alloc(128)
            self.call("BuildSharedTexturePath", out)
            return 0, e.cstr(out)
        if op == 6:
            if a[0] and not self.table_ready():
                return None
            self.call("OverrideTexturePaths", self.header if a[0] else 0, a[1], a[2])
            self.folders_valid = self.folders_valid or bool(a[0])
            return int(bool(a[0]) and e.u32(self.header + 0x4c) == e.u32(TABLE)), ""
        if op == 7:
            if not self.table_ready():
                return None
            name = e.alloc(32, b"rides:coaster:car.md2\0")
            mesh = self.call("LoadMesh", name, 0, a[1], a[2], a[3])
            if mesh:
                self.meshes[a[0] & 1] = mesh
                self.folders_valid = True
            return L.lab(mesh), ""
        if op == 8:
            mesh = self.call("CreateMeshFromM3D2Header", self.header if a[1] else 0)
            if mesh:
                self.meshes[a[0] & 1] = mesh
            return L.lab(mesh), ""
        if op in (9, 10, 12) and not self.meshes[a[0] & 1]:
            return None
        if op == 9:
            self.call("CreateEngineMesh", self.meshes[a[0] & 1])
            return 0, ""
        if op == 10:
            self.call("CreateMeshTypeEngineMesh", self.meshes[a[0] & 1])
            return 0, ""
        if op == 11:
            w32(e, QUICKLOAD, a[0])
            return 0, ""
        if op == 12:
            w32(e, self.meshes[a[0] & 1] + 8, self.meshes[~a[0] & 1] if a[1] else 0)
            return 0, ""
        return -99, ""

    def state(self):
        e, L, u = self.e, self.L, self.e.u32
        s = lambda a: T.s32(u(a))
        text = lambda p: e.cstr(p) if p else "(null)"
        out = [s(COUNT), s(MESH_INDEX), s(TEXTURE_INDEX), s(SHARED_INDEX), s(ACCESS)]
        tables, table = 0, u(ALLOCS)
        while table:
            tables, table = tables + 1, u(table)
        out += [tables, L.lab(u(ALLOCS))]
        texts = [text(u(u(TABLE) + 4 * index)) if index != -1 and u(TABLE) else "(null)"
                 for index in (s(MESH_INDEX), s(TEXTURE_INDEX), s(SHARED_INDEX))]
        paths = u(self.header + 0x4c)
        out += [0 if not paths else 1, struct.unpack(">H", e.read(self.header + 0x34, 2))[0]]
        texts += [text(u(self.header + 0xb8 + 16 * i + 4)) if self.folders_valid else "(stale)" for i in range(len(self.names))]
        for mesh in self.meshes:
            out.append(L.lab(mesh))
            if mesh:
                header = u(mesh + 4)
                out += [u(mesh), 0 if header == 0 else 1 if header == self.header else -1, L.lab(u(mesh + 0x1c)),
                        L.lab(u(mesh + 0x20)), s(mesh + 0x34), L.lab(u(mesh + 8))]
        return out, texts


def ppc(e, case):
    side = PpcSide(e, case)
    words = (ALLOCS, ACCESS, COUNT, TABLE, MESH_INDEX, TEXTURE_INDEX, SHARED_INDEX, QUICKLOAD)
    saved_words = [(a, e.u32(a)) for a in words]
    saved_name = e.read(SHARED_NAME, 16)
    saved = side.stubs().activate(e)
    try:
        side.reset()
        results, states, marks, skipped = [], [], [], []
        for op, args in case["script"]:
            result = side.op(op, args)
            skipped.append(result is None)
            results.append((SKIPPED, "") if result is None else result)
            states.append(side.state())
            marks.append(len(side.L.log))
    finally:
        CL.Stubs.deactivate(e, saved)
        for a, v in saved_words:
            w32(e, a, v)
        e.write(SHARED_NAME, saved_name)
    case["_skipped"] = skipped
    return dict(results=results, log=side.L.log, marks=marks, states=states)


def host(lib, case):
    def F(name, result, *args):
        fn = lib[name]
        fn.restype, fn.argtypes = result, list(args)
        return fn

    names = case["names"]
    arena = F("tpw_ml_arena_base", C.c_void_p)()
    size = len(image(names, "<", lambda o: 0))
    block = F("tpw_ml_alloc", C.c_void_p, C.c_uint32)(size)
    C.memmove(block, image(names, "<", lambda o: block - arena + o), size)
    F("tpw_ml_reset", None, C.c_void_p)(block)
    CL.host_begin(lib, {}, case.get("q"))
    op_fn = F("tpw_ml_op", C.c_int64, C.c_int32, C.POINTER(C.c_int32), C.c_char_p)
    state_fn = F("tpw_ml_state", C.c_int32, C.POINTER(C.c_int64), C.c_char_p)
    results, states, marks = [], [], []
    buffer, text, texts = (C.c_int64 * 64)(), C.create_string_buffer(256), C.create_string_buffer(4096)
    try:
        for (op, args), skip in zip(case["script"], case["_skipped"]):
            if skip:
                results.append((SKIPPED, ""))
            else:
                text.value = b""
                padded = (C.c_int32 * 8)(*[T.s32(v) for v in (list(args) + [0] * 8)[:8]])
                results.append((op_fn(op, padded, text), text.value.decode("latin-1")))
            count = state_fn(buffer, texts)
            states.append((list(buffer[:count]), texts.value.decode("latin-1").split("\n")[:-1]))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    return dict(results=results, log=log, marks=marks, states=states)


# ----------------------------------------------------------------- cases

TEXTURE_NAMES = ["wood.tga", "metal", "grass.wct", "a.b.c", "sky.TGA", ""]


def paths_op(r):
    pick = lambda: r.choice([-1, 0, 1, 2, 3, 4])
    return (1, [r.randrange(2), pick(), pick(), pick()])


def any_op(r):
    kind = r.choice([1, 1, 1, 2, 3, 4, 5, 6, 6, 6, 7, 7, 8, 9, 10, 11, 12])
    if kind == 1:
        return paths_op(r)
    if kind == 4:
        return (4, [r.choice([0, 1, 2, -1, 7])])
    if kind == 6:
        return (6, [r.choice([1, 1, 1, 0]), r.randrange(2), r.randrange(3)])
    if kind == 7:
        return (7, [r.randrange(2), r.randrange(2), r.randrange(3), r.randrange(2)])
    if kind == 8:
        return (8, [r.randrange(2), r.choice([1, 1, 0])])
    if kind in (9, 10):
        return (kind, [r.randrange(2)])
    if kind == 11:
        return (11, [r.randrange(2)])
    if kind == 12:
        return (12, [r.randrange(2), r.randrange(2)])
    return (kind, [])


def case_of(r, length):
    names = [r.choice(TEXTURE_NAMES) for _ in range(r.randrange(0, 4))]
    q = {"Exists": [r.choice([0, 0, 1]) for _ in range(12)], "Indexed": [r.randrange(2) for _ in range(6)],
         "Load": [r.choice([1, 1, 1, 0]) for _ in range(4)]}
    return dict(names=names, q=q, script=[any_op(r) for _ in range(length)])


def edge():
    r = random.Random(131)
    names = ["wood.tga", "metal"]
    # The three folders: each present or not, with and without its ':', the same again
    # (nothing new: no table is made), one changed, then all forgotten.
    for folders in ((0, 2, 3), (1, 2, 3), (0, -1, -1), (-1, 2, -1), (-1, -1, 3), (-1, -1, -1), (4, 4, 4)):
        yield dict(names=names, script=[(1, [0, *folders]), (3, []), (1, [0, *folders]), (1, [1, *folders]),
                                        (1, [0, folders[0], 4, folders[2]]), (3, []), (2, []), (3, []), (2, [])])
    yield dict(names=names, script=[(1, [0, 0, 2, 3]), (1, [0, -1, 2, 3]), (1, [0, 2, -1, 3]), (1, [0, 3, 2, 0]), (2, [])])
    yield dict(names=[], script=[(4, [5]), (4, [-1]), (5, []), (2, []), (4, [0])])
    # A texture's folder: the file there as .wct, as .tga, not there; the shared folder;
    # neither; the quickload index instead of the files.
    for exists in ([1], [0, 1], [0, 0], [1, 1, 1, 1]):
        for folders in ((0, 2, 3), (0, 2, -1), (0, -1, 3), (0, -1, -1)):
            for quick, check in ((0, 0), (1, 0), (1, 1)):
                yield dict(names=["wood.tga", "metal", "a.b.c"], q={"Exists": exists, "Indexed": exists},
                           script=[(1, [0, *folders]), (11, [quick]), (4, [3]), (6, [1, check, 0]), (6, [0, 0, 0])])
    # LoadMesh: the file there or not, with and without the engine mesh, one frame or all.
    for load in (1, 0):
        for no_engine in (0, 1):
            for one_frame in (0, 1):
                yield dict(names=names, q={"Load": [load]},
                           script=[(1, [0, 0, 2, 3]), (7, [0, no_engine, 2, one_frame]), (9, [0]), (9, [0])])
    yield dict(names=names, script=[(1, [0, 0, 2, 3]), (8, [0, 1]), (8, [1, 1]), (8, [0, 0]), (9, [0]), (10, [0]), (12, [0, 1]),
                                    (9, [0]), (9, [0]), (12, [0, 0]), (9, [0]), (10, [1])])
    for _ in range(40):
        yield case_of(r, 14)


TESTS = [
    FnTest("mesh paths, LoadMesh and the engine mesh makers", ppc, host, edge=edge, rnd=lambda r: case_of(r, 16)),
]
