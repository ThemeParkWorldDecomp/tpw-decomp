"""Differential test for src/lobby_meshes.cpp (CLBLobbyMeshes, CLBMeshType, CLBMeshInstance
and their lists) and GetUniqueID, against libtpw_unit_LobbyMeshes.dylib.

A case is a small world (six ride sets, the first the height field's, each with a mesh
whose node holds a matrix) and a script: make the system, instance mesh types, show, place,
turn, scale and animate instances, destroy them, take types and instances out of the lists
and put them back, the height field, the empty members, and at the end destroy the system.
After every operation: the answer, the calls recorded (the ride and height field code, the
draw switches, the matrix helpers, the name compares), the two lists, every instance still
alive, the six matrices bit for bit (but any NaN as one value: a direction straight up or
of no length divides by zero, and the sign of a NaN that passes through a multiply-subtract
is not reproduced, see include/ppc_math.h), and operator new's and delete's counts.

The generator keeps a model of the two lists and of which instances are alive, so that a
script never puts an object in a list twice (the system would delete it twice) and never
uses a dead one.
"""
import ctypes as C
import random
import struct

import calllog as CL
import test_FSSystem as FS
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyMeshes"

MINUS = 0xFFFFFFFF
w32 = FS.w32
F = FS.F
U = dict(
    inst_dtor=0x100A7794, Show=0x100A7860, SetPosition=0x100A790C, SetDirection=0x100A798C, RotateY=0x100A7C0C,
    Scale=0x100A7C90, PlayAnim=0x100A7D28, GetAnimTimeRemaining=0x100A7DF0, SetRideSigns=0x100A7E84,
    ctor=0x100A7EFC, dtor=0x100A802C, inst_list_dtor=0x100A8108, Init=0x100A81AC, Tick=0x100A81DC, Render=0x100A820C,
    Close=0x100A8240, WindowMessagesFn=0x100A8270, ShowHeightfield=0x100A82B0, HideHeightfield=0x100A835C,
    InstanceMeshType=0x100A8408, DeleteMeshTypes=0x100A86FC, type_list_dtor=0x100A8804,
    DeleteMeshInstanceListEntries=0x100A88A4, RegisterType=0x100A8948, UnRegisterType=0x100A89AC,
    RegisterInstance=0x100A8A40, UnRegisterInstance=0x100A8AA8,
    i_ct=0x100A9C24, i_dt=0x100A9C8C, i_find=0x100A9D3C, i_removeall=0x100A9DC0, i_removeat=0x100A9E74,
    i_removehead=0x100A9F5C, t_ct=0x100AA034, t_dt=0x100AA094, t_find=0x100AA13C, t_removeall=0x100AA1B4,
    t_removeat=0x100AA260, t_removehead=0x100AA340, t_addtail=0x100AA410, i_addtail=0x100AA4EC,
    GetUniqueID=0x1006FC5C)
G_MESHES, G_SETS, G_HEIGHTFIELD = 0x102E48D0, 0x102C79A4, 0x102C6C74
SETS = 6
FOLDERS = ["data:lobby:globe", "DATA:Lobby:Globe", "data:lobby:island", "data:lobby"]
NAMES = ["bullfrog", "hpin", "HPin", "tree", "Bullfrog", "t"]


def hash_text(text):
    h = 2166136261
    for ch in text.encode("latin-1"):
        h = ((h ^ ch) * 16777619) & MINUS
    return h


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bf(bits):
    return struct.unpack(">f", struct.pack(">I", bits & MINUS))[0]


def canon(bits):
    return 0x7FC00000 if bits & 0x7F800000 == 0x7F800000 and bits & 0x007FFFFF else bits


def build_stubs(emu):
    S = CL.Stubs()
    R = CL.recorder
    FS.string_stubs(S)
    S.real.update(U.values())
    S.imp("_stricmp__10NS_MacDozeFPCcPCc", lambda e: (
        e.cl.rec("_stricmp", (), (e.cstr(e.r(3), limit=400)[:159], e.cstr(e.r(4), limit=400)[:159])),
        e.set_r(3, T.u32(FS.icmp(e.cstr(e.r(3), limit=400), e.cstr(e.r(4), limit=400))))))
    S.internal(0x100A56C8, R("CLBLobbySystem", "L"))
    S.internal(0x100A5744, R("~CLBLobbySystem", "L"))
    S.internal(0x10004F00, lambda e: e.cl.rec("SetLevelPath", (), (FS.s_read(e, e.r(4), False),)))
    def news(e):
        return sum(1 for kind, address, _ in e.alloc_log if kind == "new")

    def listed_instances(e):
        lst = e.u32(e.lm_system + 0xC)
        return T.s32(e.u32(lst + 0xC)) if lst else -1

    # (LoadLevelHeightfield with operator new's count so far, DestroyRideInstance with the
    # instance list's count: they pin the order of the constructor and of an instance's end.)
    S.internal(0x1005BC44, lambda e: (e.cl.rec("LoadLevelHeightfield", (T.s32(e.r(3)), T.s32(e.r(4)), news(e))), e.set_r(3, 1)))
    S.internal(0x1005C0FC, R("DestroyLevelHeightfield"))
    S.internal(0x10065178, R("EnableMeshDraw", "L"))
    S.internal(0x100650C4, R("EnableMeshDrawRange", "Luu"))
    S.internal(0x100653A4, R("DisableMeshDraw", "L"))
    S.internal(0x1006548C, R("DisableMeshDrawRange", "Luu"))
    S.internal(0x100655D8, lambda e: e.cl.rec("ScaleMesh", (e.cl.lab(e.r(3)),) + struct.unpack(">3I", e.read(e.r(4), 12))))

    def stack(e, n):
        return [e.u32(e.r(1) + 0x38 + 4 * i) for i in range(n)]

    def load_ride_type(e):
        L = e.cl
        sign2, d, x = stack(e, 3)
        L.rec("LoadRideType", (e.r(5), e.r(6), e.r(7), e.r(8), e.r(9), T.s32(e.r(10)), T.s32(sign2), T.s32(d), T.s32(x)),
              (e.cstr(e.r(3), limit=400), e.cstr(e.r(4), limit=400)))
        e.set_r(3, L.ret("RideType", 7) & MINUS)

    def instance_ride(e):
        L = e.cl
        f, g = stack(e, 2)
        L.rec("InstanceRide", (e.r(3), e.r(4), e.r(5), e.r(6), T.s32(e.r(7)), e.r(8), T.s32(e.r(9)), T.s32(e.r(10)), f, T.s32(g)))
        e.set_r(3, L.ret("Instance", 1) & MINUS)

    S.internal(0x10068FD0, load_ride_type)
    S.internal(0x10069B24, instance_ride)
    S.internal(0x1006A3EC, lambda e: e.cl.rec("DestroyRideInstance", (e.r(3), listed_instances(e))))
    S.internal(0x1006A9F0, R("DestroyAllRideTypes"))
    S.internal(0x100C6670, lambda e: e.cl.rec("SetCurRideAnim", (e.cl.lab(e.r(3)), e.r(4), e.r(5), e.r(6), CL.fbits(e.f(1)), e.r(8))))
    S.internal(0x100C7930, R("GetCurrentAnimStats", "Luuuu", lambda e, L: L.ret("AnimTime", 0) & MINUS))

    def identity(e):
        e.cl.rec("SetMatrixIdentity", (e.cl.lab(e.r(3)),))
        e.write(e.r(3), b"".join(struct.pack(">f", 1.0 if k % 5 == 0 else 0.0) for k in range(16)))

    S.internal(0x100C0E28, identity)
    S.internal(0x100C174C, lambda e: e.cl.rec("RotateLMatrixXYZNoTrans", (e.cl.lab(e.r(3)), e.r(4), e.u32(e.r(5)))))
    S.internal(0x101EED5C, lambda e: e.cl.rec("RegenerateRideSign", (e.r(3), T.s32(e.r(6)), T.s32(e.r(7))),
                                              (e.cstr(e.r(4), limit=400), e.cstr(e.r(5), limit=400))))
    return S


class PpcSide:
    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        del e.alloc_log[:]
        fixed = {}
        self.matrices = []
        for i in range(SETS):
            flags, c0, c4 = case["world"][i * 3:i * 3 + 3]
            node = e.alloc(0xA0, b"\xa5" * 0xA0)
            e.write(node + 0x10, b"".join(struct.pack(">f", float(i * 100 + k)) for k in range(16)))
            header = e.alloc(0xB8, b"\xa5" * 0xB8)
            w32(e, header + 0x78, node)
            mesh = e.alloc(0x40, b"\xa5" * 0x40)
            w32(e, mesh + 4, header)
            rset = e.alloc(0xC8, b"\xa5" * 0xC8)
            e.write(rset + 4, struct.pack(">II", flags, mesh))
            e.write(rset + 0xC0, struct.pack(">II", c0, c4))
            w32(e, G_SETS + 4 * i, 0 if i == 0 else rset)
            if i == 0:
                w32(e, G_HEIGHTFIELD, rset)
            fixed[mesh], fixed[rset], fixed[node + 0x10] = 10 + i, 20 + i, 30 + i
            self.matrices.append(node + 0x10)
        self.system = e.lm_system = e.alloc(0x10, b"\xa5" * 0x10)
        e.write(self.system + 0xC, b"\0\0\0\0")
        fixed[self.system] = 5
        w32(e, G_MESHES, 0)
        self.L = e.cl = CL.PpcLog(fixed, case.get("q"))
        self.slots, self.spare = [], 0

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a if isinstance(a, T.F) else a & MINUS for a in args])[0]

    def virtual(self, obj, slot, *args):
        e = self.e
        return self.call(e.u32(e.u32(e.u32(obj) + slot)), obj, *args)

    def text(self, value):
        return self.e.alloc(len(value) + 1, value.encode("latin-1") + b"\0")

    def op(self, op, a, folder, name):
        e, system = self.e, self.system
        instance = self.slots[a[0]] if 20 <= op < 40 else 0
        if op == 1:
            e.write(system, b"\xa5" * 0x10)
            self.call(U["ctor"], system)
            w32(e, G_MESHES, system)
        elif op == 2:
            self.virtual(system, 8, 0)
        elif op == 3:
            self.call(U["ShowHeightfield"], system)
        elif op == 4:
            self.call(U["HideHeightfield"], system)
        elif op == 5:
            for slot in (0xC, 0x10, 0x14, 0x18):
                self.virtual(system, slot)
            self.virtual(system, 0x1C, a[0], a[1], a[2])
        elif op == 6:
            self.slots.append(self.call(U["InstanceMeshType"], system, self.text(folder), self.text(name), a[0], a[1], a[2], a[3]))
        elif op == 7:
            self.call(U["DeleteMeshInstanceListEntries"], system)
        elif op == 8:
            self.call(U["DeleteMeshTypes"], system)
        elif op == 9:
            node = e.u32(e.u32(system + 8) + 4)
            for _ in range(a[0]):
                node = e.u32(node + 4) if node else 0
            if not node:
                return -1
            self.spare = e.u32(node)
            self.virtual(system, 0x24, self.spare)
        elif op == 10:
            self.virtual(system, 0x20, self.spare)
        elif op == 11:
            self.virtual(system, 0x24, self.spare)
        elif op == 20:
            self.call(U["Show"], instance)
        elif op in (21, 22):
            point = e.alloc(12, struct.pack(">3I", *[v & MINUS for v in a[1:4]]))
            self.call(U["SetPosition" if op == 21 else "SetDirection"], instance, point)
        elif op == 23:
            self.call(U["RotateY"], instance, T.F(bf(a[1])))
        elif op == 24:
            self.call(U["Scale"], instance, T.F(bf(a[1])), T.F(bf(a[2])), T.F(bf(a[3])))
        elif op == 25:
            self.call(U["PlayAnim"], instance, a[1], a[2])
        elif op == 26:
            return self.call(U["GetAnimTimeRemaining"], instance)
        elif op == 27:
            self.call(U["SetRideSigns"], instance, a[1], a[2])
        elif op == 28:
            self.virtual(instance, 8, 1)
        elif op == 29:
            self.virtual(system, 0x2C, instance)
        elif op == 30:
            self.virtual(system, 0x28, instance)
        elif op == 31:
            w32(e, instance + 4, a[1])
        else:
            return -99
        return 0

    def state(self, system_alive, alive):
        e, out = self.e, []
        s_text = lambda obj: hash_text(FS.s_read(e, obj, False))   # noqa: E731
        if system_alive:
            out.append(T.s32(e.u32(self.system + 4)))
            for field, is_types in ((8, True), (0xC, False)):
                lst = e.u32(self.system + field)
                if not lst:
                    out.append(-1)
                    continue
                out.append(T.s32(e.u32(lst + 0xC)))
                node = e.u32(lst + 4)
                while node:
                    item = e.u32(node)
                    if is_types:
                        out += [e.u32(item), s_text(item + 4), s_text(item + 0x18)]
                    else:
                        out.append(self.slots.index(item) if item in self.slots else -1)
                    node = e.u32(node + 4)
        for slot, live in zip(self.slots, alive):
            if live:
                out += [e.u32(slot + 4), T.s32(e.u32(slot + 8)), s_text(slot + 0xC), s_text(slot + 0x20)]
        for matrix in self.matrices:
            out += [canon(v) for v in struct.unpack(">16I", e.read(matrix, 64))]
        out.append(sum(1 for kind, address, _ in e.alloc_log if kind == "new"))
        out.append(sum(1 for kind, address, _ in e.alloc_log if kind == "delete" and address))
        return out


STUBS = None


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    if not FS.VT:
        FS.make_vtables(runner.emu)
    STUBS = build_stubs(runner.emu)


def meshes_ppc(e, case):
    saved = STUBS.activate(e)
    try:
        side = PpcSide(e, case)
        answers, states, marks = [], [], []
        for op, a, folder, name, system_alive, alive in case["script"]:
            answers.append(T.s32(side.op(op, list(a), folder, name) & MINUS))
            states.append(side.state(system_alive, alive))
            marks.append(len(side.L.log))
        return dict(answers=answers, states=states, marks=marks, log=side.L.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def meshes_host(lib, case):
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    CL.host_begin(lib, {}, case.get("q"))
    F(lib, "tpw_lm_begin", None, C.POINTER(C.c_uint32))((C.c_uint32 * (SETS * 3))(*case["world"]))
    op_fn = F(lib, "tpw_lm_op", C.c_int64, C.c_int32, C.POINTER(C.c_int32), C.c_char_p, C.c_char_p)
    state_fn = F(lib, "tpw_lm_state", C.c_int32, C.c_int32, C.POINTER(C.c_int32), C.POINTER(C.c_int64))
    answers, states, marks = [], [], []
    values = (C.c_int64 * 2048)()
    try:
        for op, a, folder, name, system_alive, alive in case["script"]:
            answers.append(T.s32(op_fn(op, (C.c_int32 * 8)(*[T.s32(v & MINUS) for v in a]), folder.encode("latin-1"),
                                       name.encode("latin-1")) & MINUS))
            flags = (C.c_int32 * 64)(*[int(v) for v in alive])
            states.append(list(values[:state_fn(int(system_alive), flags, values)]))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    return dict(answers=answers, states=states, marks=marks, log=log)


class Script:
    """Builds a script and keeps the model: which instances are alive and which are listed,
    the type list (the names, lower case), and the type taken out of it, if any."""

    def __init__(self):
        self.steps, self.alive, self.listed = [], [], []
        self.system = False
        self.types = []         # the type list: (folder, name) in lower case
        self.spare = None       # the type taken out last
        self.spare_listed = False

    def add(self, op, *a, folder="", name=""):
        self.steps.append((op, (list(a) + [0] * 8)[:8], folder, name, self.system, list(self.alive)))

    def construct(self):
        self.system = True
        self.add(1)

    def instance(self, folder, name, over=0, a=1, s1=0, s2=0):
        self.alive.append(True)
        self.listed.append(True)
        key = (folder.lower(), name.lower())
        if key not in [entry[:2] for entry in self.types]:
            self.types.append(key + (len(self.steps),))
        self.add(6, over, a, s1, s2, folder=folder, name=name)

    def take_type(self, position):
        if position < len(self.types):
            self.spare = self.types.pop(position)
            self.spare_listed = False
        self.add(9, position)

    def return_type(self):
        self.types.append(self.spare)
        self.spare_listed = True
        self.add(10)

    def drop_type(self):               # UnRegister of the type kept aside: out of the list if it is there
        if self.spare_listed:
            self.types.remove(self.spare)
            self.spare_listed = False
        self.add(11)

    def delete_instance(self, k):
        self.alive[k] = False
        self.listed[k] = False
        self.add(28, k)

    def delete_listed(self):
        for k in range(len(self.alive)):
            if self.listed[k]:
                self.alive[k] = self.listed[k] = False
        self.add(7)

    def unregister(self, k):
        self.listed[k] = False
        self.add(29, k)

    def register(self, k):
        self.listed[k] = True
        self.add(30, k)

    def destroy(self):
        for k in range(len(self.alive)):
            if self.listed[k]:
                self.alive[k] = self.listed[k] = False
        self.system = False
        self.add(2)

    def live(self):
        return [k for k, v in enumerate(self.alive) if v]


def fl(r):
    k = r.random()
    if k < 0.1:
        return fb(r.choice([0.0, 1.0, -1.0, 0.5, 100.0]))
    if k < 0.9:
        return fb(r.uniform(-50.0, 50.0))
    return fb(r.uniform(-1e-3, 1e-3))


def world(r):
    out = []
    for _ in range(SETS):
        out += [r.choice([0, 0x4000, 0x4000, 0x20, 0xFFFFBFFF, MINUS, 0x4020]), r.getrandbits(32), r.getrandbits(32)]
    return out


def random_script(r, length):
    s = Script()
    s.construct()
    for _ in range(length):
        k = r.random()
        live = s.live()
        if k < 0.22 and len(s.alive) < 40:
            s.instance(r.choice(FOLDERS), r.choice(NAMES), r.choice([0, 0, 1, 5]), r.choice([0, 1, 7]),
                       r.choice([0, 0x1234]), r.choice([0, 0x5678]))
        elif k < 0.26:
            s.add(r.choice([3, 4]))
        elif k < 0.28:
            s.add(5, 1, 2, 3)
        elif k < 0.31:
            s.take_type(r.randrange(4))
        elif k < 0.36 and s.spare is not None:
            if s.spare_listed or r.random() < 0.3:
                s.drop_type()
            else:
                s.return_type()
        elif k < 0.38:
            s.delete_listed()
        elif not live:
            continue
        else:
            i = r.choice(live)
            if k < 0.46:
                s.add(20, i)
            elif k < 0.54:
                s.add(21, i, fl(r), fl(r), fl(r))
            elif k < 0.64:
                s.add(22, i, fl(r), fl(r), fl(r))
            elif k < 0.69:
                s.add(23, i, fl(r))
            elif k < 0.74:
                s.add(24, i, fl(r), fl(r), fl(r))
            elif k < 0.80:
                s.add(25, i, r.randrange(-1, 6), r.choice([0, 1, 2, -1]))
            elif k < 0.84:
                s.add(26, i)
            elif k < 0.88:
                s.add(27, i, r.choice([0, 0x100]), r.choice([0, 0x200]))
            elif k < 0.92:
                s.delete_instance(i)
            elif k < 0.95:
                (s.unregister if s.listed[i] else s.register)(i)
            elif k < 0.97:
                s.add(31, i, r.choice([1, 2, 3, 5]))    # (0 is in the edge cases: only the animation calls allow it)
            else:
                s.add(29, i)                 # leaving twice is harmless
                s.listed[i] = False
    return s


def queues(r):
    return {"Instance": [r.randrange(1, SETS) for _ in range(r.randrange(1, 12))],
            "RideType": [r.choice([0, 1, 7, 300, MINUS]) for _ in range(r.randrange(1, 6))],
            "AnimTime": [r.choice([0, 1, 250, MINUS]) for _ in range(r.randrange(1, 4))]}


def meshes_case(r):
    s = random_script(r, r.randrange(6, 40))
    end = r.random()
    # A type taken out of the list and never put back stays allocated on both sides; the
    # system's end then works on what is listed. DeleteMeshTypes alone leaves the system
    # without a type list, so nothing may follow it.
    if end < 0.7:
        s.destroy()
    elif end < 0.8:
        s.add(8)
    return dict(world=world(r), q=queues(r), script=s.steps)


def meshes_edge():
    r = random.Random(23)
    # The same mesh asked for three times, in other letter cases: one type, three instances.
    s = Script()
    s.construct()
    s.instance("data:lobby:globe", "bullfrog")
    s.instance("DATA:LOBBY:GLOBE", "BullFrog", 1, 9, 0x11, 0x22)
    s.instance("data:lobby:globe", "bullfrog")
    s.instance("data:lobby:globe", "hpin", 1, 3, 0x11, 0x22)
    s.instance("data:lobby", "bullfrog")
    for k in range(5):
        s.add(20, k)
        s.add(20, k)
    s.destroy()
    yield dict(world=[0x4000, 0x00010002, 0x00030004] * SETS, q={"Instance": [1, 2, 3, 4, 5], "RideType": [11, 12, 13]},
               script=s.steps)
    # The height field with and without a range of the map; instances likewise.
    for flags in (0, 0x4000, MINUS, 0xFFFFBFFF):
        s = Script()
        s.construct()
        s.add(4)
        s.add(3)
        s.add(3)
        s.add(4)
        s.add(4)
        s.instance("a", "b")
        s.add(20, 0)
        s.add(5, 7, 8, 9)
        s.destroy()
        yield dict(world=[flags, 0xAAAABBBB, 0xCCCCDDDD] * SETS, q={"Instance": [3]}, script=s.steps)
    # Directions: along each axis, straight up and down (no "across": the length is 0),
    # nothing at all, tiny and huge.
    for d in ((0, 0, 1), (1, 0, 0), (0, 0, -1), (0.6, 0, 0.8), (1, 1, 1), (0, 1, 0), (0, -2, 0), (0, 0, 0), (1e-20, 0, 1e-20),
              (1e20, 5, -1e20), (3, 4, 12), (-0.25, 0.5, 0.125)):
        s = Script()
        s.construct()
        s.instance("f", "n")
        s.add(21, 0, fb(1.5), fb(-2.5), fb(3.25))
        s.add(22, 0, fb(d[0]), fb(d[1]), fb(d[2]))
        s.add(23, 0, fb(0.75))
        s.add(24, 0, fb(2.0), fb(3.0), fb(4.0))
        s.add(21, 0, fb(9.0), fb(8.0), fb(7.0))
        yield dict(world=[0] * (SETS * 3), q={"Instance": [2]}, script=s.steps)
    # Animations and signs; instance number 0 plays nothing.
    s = Script()
    s.construct()
    s.instance("data:lobby:globe", "hpin")
    for loop in (0, 1, 2, -1):
        s.add(25, 0, 3, loop)
    s.add(26, 0)
    s.add(27, 0, 0x100, 0x200)
    s.add(31, 0, 0)
    s.add(25, 0, 3, 1)
    s.add(25, 0, 3, 0)
    s.add(26, 0)
    s.add(27, 0, 0, 0)
    yield dict(world=[0] * (SETS * 3), q={"Instance": [4], "AnimTime": [1234]}, script=s.steps)
    # Lists: an instance leaves and comes back, one is destroyed by hand, a type is taken out
    # (the next request for its mesh loads it again, numbered by the list's count).
    s = Script()
    s.construct()
    for name in ("a", "b", "c"):
        s.instance("f", name)
    s.unregister(1)
    s.add(29, 1)
    s.delete_listed()
    s.register(1)
    s.instance("f", "a")
    s.take_type(1)
    s.instance("f", "b")
    s.drop_type()
    s.return_type()
    s.instance("f", "b")
    s.take_type(7)
    s.drop_type()
    s.delete_instance(3)
    s.destroy()
    yield dict(world=world(r), q=queues(r), script=s.steps)
    # A type in the list twice: its destructor takes the second entry out, so it is freed once.
    s = Script()
    s.construct()
    s.instance("f", "a")
    s.instance("f", "b")
    s.take_type(0)
    s.return_type()
    s.return_type()
    s.destroy()
    yield dict(world=world(r), q=queues(r), script=s.steps)
    # The system made and destroyed with nothing in it; DeleteMeshTypes by itself.
    s = Script()
    s.construct()
    s.destroy()
    yield dict(world=world(r), script=s.steps)
    s = Script()
    s.construct()
    s.instance("f", "a")
    s.instance("g", "a")
    s.add(8)
    yield dict(world=world(r), q=queues(r), script=s.steps)
    for _ in range(60):
        yield meshes_case(r)


def unique_ppc(e, case):
    saved = STUBS.activate(e)
    try:
        return T.call_ppc(e, U["GetUniqueID"], [case["c"] & MINUS, case["n"] & MINUS])[0]
    finally:
        CL.Stubs.deactivate(e, saved)


def unique_host(lib, case):
    return F(lib, "tpw_lm_unique", C.c_uint32, C.c_int32, C.c_uint32)(T.s32(case["c"] & MINUS), case["n"] & MINUS)


def unique_edge():
    for c in (0, 18000, 1, -1, 0x7FFFFFFF):
        for n in (0, 1, 5, MINUS, 0x80000000):
            yield dict(c=c, n=n)


TESTS = [FnTest("CLBLobbyMeshes", meshes_ppc, meshes_host, edge=meshes_edge, rnd=meshes_case),
         FnTest("GetUniqueID", unique_ppc, unique_host, edge=unique_edge,
                rnd=lambda r: dict(c=r.getrandbits(32), n=r.getrandbits(32)))]
