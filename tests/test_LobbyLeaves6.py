"""Unit w75, the lobby's small leaves part 6, against libtpw_unit_LobbyLeaves6.dylib (tests/unit_LobbyLeaves6.cpp), in the main app
("SimTheme Park"):
  src/Game_exit.cpp            CGame::ExitLevel 0x101496ec
  src/InGame_start.cpp         CInGame::Start 0x1014b304
  src/ui_progress_cursor.cpp   CursorLibrary::RemoveAll 0x101cc8ec, FuckOffUIProgressBar 0x101e0294
  src/ui_image_dtor.cpp        InterfaceImage::~InterfaceImage 0x101cfebc
  src/MeshDestroy.cpp          DestroyEngineMesh 0x10065004, DestroyMeshInstance 0x100631c4
  src/RideWrapping.cpp         StripRideWrapping 0x101ee150

A case is a world and an operation, built twice from one description: in emulator memory at the Mac's layouts for the original and by
the unit's entry points (tpw_l6_*) for the translation. Every callee outside the unit is a fake that records the same event on both
sides, the destructors reached through a virtual (the cursors', the engine mesh's) being fake functions in a vtable the object points
at (called through the original's __ptr_glue), and operator delete a fake that records the deletion of a labelled block. The fakes of the
world and of the sky objects can be told by the case's queues to change the globals behind the function's back (the Mac reads them
again), the mesh draw fake to clear the draw flag as the real one does or not, and the node fake writes the shared word.

What only the PowerPC side can see is checked there and compared as a number the host reports as 1: the answers in r3 (ExitLevel 1,
DestroyMeshInstance 1, the image destructor's `this`), the world's destructor getting the flag -1 (not deleting) and the window
destructor the flag 0 and the image's vtable word, __destroy_arr's size 0x10, count 0x1d and destructor 0x101522e0.

Not tested: the destructors on a NULL object (the Mac tests for it; the host never calls with NULL), __destroy_arr's order (not in the
dump: the runtime library is an import; the fake follows the rule tests/test_bflWadTree.py uses: last element first, flag -1).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyLeaves6"
MASK = 0xFFFFFFFF
MINUS = 0xFFFFFFFF

EXIT_LEVEL, IN_GAME_START, REMOVE_ALL, FUCK_OFF = 0x101496EC, 0x1014B304, 0x101CC8EC, 0x101E0294
IMAGE_DTOR, DESTROY_ENGINE_MESH, DESTROY_MESH_INSTANCE, STRIP_RIDE_WRAPPING = 0x101CFEBC, 0x10065004, 0x100631C4, 0x101EE150
PTR_GLUE = 0x1023BAA8
WORLD_EXIT, WORLD_FREE, STAFF_DTOR, CLEAR_DATABASE = 0x1013A9FC, 0x1013A264, 0x10128F68, 0x101546E8
SKY_DESTROY, SKY_DTOR, SUB_DTOR, CLEAR_THEME = 0x1015B804, 0x1015B61C, 0x101522E0, 0x10149334
SEND_MESSAGE, WINDOW_DTOR = 0x101C9518, 0x101D9BA4
DISABLE_MESH_DRAW, RELEASE_FREE = 0x100653A4, 0x10064C54
STRIP_NODE = 0x101EDAFC
G_WORLD, G_CENTRE, G_DATABASE = 0x1037EBFC, 0x1037F6C4, 0x1037F6C8
G_CURSORS, G_PROGRESS, G_COLOURS = 0x10395C2C, 0x10395C6C, 0x102DC08C
IMAGE_VTABLE = 0x102AFAEC

K_GAME, K_WORLD, K_WORLD2, K_DATABASE, K_SKY, K_SKY_OBJECTS, K_SKY_OBJECTS2, K_CENTRE = range(1, 9)
K_MESH, K_OTHER_MESH, K_ENGINE, K_ALLOCATION = 1, 2, 3, 4
K_NODE, K_CURSOR = 10, 40
K_HEADER, K_DATA, K_POINT, K_FULL, K_MINI, K_STRAY = 1, 7, 8, 100, 200, 300
SUBS = 0x1D


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def rs(e, a, n):
    return list(struct.unpack(">" + "I" * n, e.read(a, 4 * n)))


# ---------------------------------------------------------------------------------------------------------------- the fakes


def build_stubs():
    S = CL.Stubs()
    R = CL.recorder
    S.real = {EXIT_LEVEL, IN_GAME_START, REMOVE_ALL, FUCK_OFF, IMAGE_DTOR, DESTROY_ENGINE_MESH, DESTROY_MESH_INSTANCE,
              STRIP_RIDE_WRAPPING, PTR_GLUE}

    def hook_world(key):
        def hook(e):
            action = e.cl.ret(key, 0)
            if action == 1:
                ws(e, G_WORLD, [e.l6.world2])
            elif action == 2:
                ws(e, G_WORLD, [0])
        return hook

    def world_fn(name, key):
        hook = hook_world(key)

        def fn(e):
            e.cl.rec(name, (e.cl.lab(e.r(3)),))
            hook(e)
        return fn

    def staff_dtor(e):
        # The Mac passes the flag -1: the block is kept (the host records 1 here, a positive flag would double the delete).
        e.cl.rec("CStaffDB.dtor", (e.cl.lab(e.r(3)), int(T.s16(e.r(4)) <= 0)))

    def sky_destroy(e):
        e.cl.rec("CSkyObjects.Destroy", (e.cl.lab(e.r(3)),))
        if e.cl.ret("SkyAction", 0) == 1:
            ws(e, e.l6.game + 0x38, [e.l6.sky2])

    def sub_dtor(e):
        index = (e.r(3) - e.l6.centre) // 0x10
        e.cl.rec("CSubscriptionHeader.dtor", (index,))
        if T.s16(e.r(4)) != -1:
            e.cl.rec("BadSubscriptionFlag", (T.s16(e.r(4)),))

    def delete(e):
        L, block = e.cl, e.r(3)
        if block and L.lab(block) > 0:
            L.rec("delete", (L.lab(block),))
            L.forget(block)

    def disable_mesh_draw(e):
        e.cl.rec("DisableMeshDraw", (e.cl.lab(e.r(3)),))
        if e.cl.ret("Disable", 0) == 1:
            ws(e, e.r(3), [e.u32(e.r(3)) & ~1])

    def window_dtor(e):
        # The vtable word the image stored is the image's by now; the Mac passes the flag 0 to the base destructor.
        e.cl.rec("Window.dtor", (e.cl.lab(e.r(3)), int(e.u32(e.r(3) + 0x118) == IMAGE_VTABLE), T.s32(e.r(4))))

    def strip_node(e):
        L, counter = e.cl, e.r(8)
        seen = e.l6.seen
        if counter not in seen:
            seen.append(counter)
        ident = seen.index(counter) + 1
        L.rec("StripNodeWrapping", (L.lab(e.r(3)), L.lab(e.r(4)), CL.fbits(e.f(1)), L.lab(e.r(6)), L.lab(e.r(7)), ident,
                                    T.s32(e.u32(counter))))
        ws(e, counter, [L.ret("Counter", 0)])

    S.internal(WORLD_EXIT, world_fn("CWorld.ExitLevel", "ExitAction"))
    S.internal(WORLD_FREE, world_fn("CWorld.Free", "FreeAction"))
    S.internal(STAFF_DTOR, staff_dtor)
    S.internal(CLEAR_DATABASE, R("CRideDatabase.ClearDatabase", "L"))
    S.imp("Close__4CSkyFv", R("CSky.Close", "L"))
    S.internal(SKY_DESTROY, sky_destroy)
    S.internal(SKY_DTOR, R("CSkyObjects.dtor", "D"))
    S.internal(SUB_DTOR, sub_dtor)
    S.internal(CLEAR_THEME, R("CGame.ClearThemeResources", "L"))
    S.internal(SEND_MESSAGE, R("UI_SendMessage", "Liii"))
    S.internal(WINDOW_DTOR, window_dtor)
    S.internal(DISABLE_MESH_DRAW, disable_mesh_draw)
    S.internal(RELEASE_FREE, R("MReleaseFreeF", "L"))
    S.internal(STRIP_NODE, strip_node)
    S.imp("__dl__FPv", delete)
    return S


STUBS = build_stubs()


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if hasattr(e, "l6_cursor_vtable"):
        return
    e.l6 = None
    # The cursor's vtable (the Mac keeps the pointer at +0x10): slot +8 is a pointer to the descriptor of the deleting destructor.
    cursor_dtor = T.fake_function(e, lambda em: em.l6.cursor_dtor(em))
    e.l6_cursor_vtable = T.persist_alloc(e, 0x40)
    ws(e, e.l6_cursor_vtable + 8, [cursor_dtor])
    # The engine mesh's (the pointer is at +0x150).
    engine_dtor = T.fake_function(e, lambda em: CL.recorder("EngineMesh.dtor", "D")(em))
    e.l6_engine_vtable = T.persist_alloc(e, 0x40)
    ws(e, e.l6_engine_vtable + 8, [engine_dtor])

    def destroy_arr(em, call):
        # The runtime's __destroy_arr(first, destructor, size, count): the destructor on each element from the last to the first
        # with the flag -1. What the original passes is recorded when it is not what the translation implies.
        first, dtor, size, count = em.r(3), em.r(4), em.r(5), em.r(6)
        if size != 0x10 or count != SUBS:
            em.cl.rec("BadDestroyArr", (size, count))
        if em.u32(dtor) != SUB_DTOR:
            em.cl.rec("BadDestroyDtor", (em.u32(dtor),))
        for i in reversed(range(count)):
            call(dtor, first + i * size, MINUS)
        return None

    T.stub_cb(e, "__destroy_arr", destroy_arr)


class Ppc:
    """One case on the PowerPC side: the fakes active, a fresh log, the case's state for the fakes."""

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case.get("q"))
        e.l6 = self
        self.seen = []
        self.game = self.world2 = self.sky2 = self.centre = 0
        self.poison = -1
        self.cursors = []
        self.nodes = []
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)
        self.e.l6 = None

    def call(self, address, *args):
        return T.call_ppc(self.e, address, list(args))[0]

    def cursor_dtor(self, e):
        this = e.r(3)
        CL.recorder("Cursor.dtor", "D")(e)
        if this in self.cursors and self.cursors.index(this) == self.poison:
            ws(e, self.nodes[self.cursors.index(this)] + 0x10, [0])


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


def pattern(n, fill):
    return bytes([fill & 0xFF]) * n


# ------------------------------------------------------------------------------------------------------------ CGame::ExitLevel
# description: world, display word, sky objects, message centre, fill byte


def exit_ppc(e, case):
    p = Ppc(e, case)
    try:
        L = p.L
        world, flags, sky, centre, fill = case["d"]
        game = p.game = e.alloc(0x40, pattern(0x40, fill))
        ws(e, game + 0x38, [0])
        w1 = e.alloc(0x40) if world else 0
        p.world2 = e.alloc(0x40)
        s1 = e.alloc(8) if sky else 0
        p.sky2 = e.alloc(8)
        p.centre = c1 = e.alloc(0x1D0) if centre else 0
        sky_address, display = e.data_import("gei_Sky"), e.data_import("gei_Display")
        for address, label in ((game, K_GAME), (w1, K_WORLD), (p.world2, K_WORLD2), (G_DATABASE, K_DATABASE), (sky_address, K_SKY),
                               (s1, K_SKY_OBJECTS), (p.sky2, K_SKY_OBJECTS2), (c1, K_CENTRE)):
            if address:
                L.labels[address] = label
        ws(e, G_WORLD, [w1])
        ws(e, G_CENTRE, [c1])
        ws(e, game + 0x38, [s1])
        ws(e, display, [flags])
        before = e.read(game, 0x40)
        answer = p.call(EXIT_LEVEL, game)
        after = e.read(game, 0x40)
        state = [L.lab(e.u32(G_WORLD)), L.lab(e.u32(game + 0x38)), L.lab(e.u32(G_CENTRE)), e.u32(display)]
        state += [int(address != 0 and L.lab(address) > 0) for address in (w1, p.world2, s1, p.sky2, c1)]
        state.append(int(before[:0x38] == after[:0x38] and before[0x3C:] == after[0x3C:]))
        state.append(int(answer == 1))
        return dict(state=state, log=L.log)
    finally:
        p.done()


def exit_host(lib, case):
    state, log = host_run(lib, "tpw_l6_exit", case["d"], case.get("q"))
    return dict(state=state + [1], log=log)


def exit_case(r, world=None, flags=None, sky=None, centre=None, actions=None):
    world = r.randint(0, 1) if world is None else world
    flags = r.choice([0x400000, 0x3FFFFF, 0, 0xFFFFFFFF, 0xFFBFFFFF, 0x800000, r.getrandbits(32), r.getrandbits(32) | 0x400000,
                      r.getrandbits(32) & ~0x400000]) if flags is None else flags
    sky = r.randint(0, 1) if sky is None else sky
    centre = r.randint(0, 1) if centre is None else centre
    exit_action, free_action, sky_action = actions if actions else (r.choice([0, 0, 1, 2]), r.choice([0, 0, 1, 2]), r.choice([0, 0, 1]))
    return dict(d=[world, flags, sky, centre, r.randint(0, 255)],
                q={"ExitAction": [exit_action], "FreeAction": [free_action], "SkyAction": [sky_action]})


def edge_exit():
    r = random.Random(7501)
    for world in (0, 1):
        for flags in (0x400000, 0x3FFFFF, 0, 0xFFBFFFFF):
            for sky in (0, 1):
                for centre in (0, 1):
                    yield exit_case(r, world, flags, sky, centre, (0, 0, 0))
    # The world's functions replace or clear the global behind the function's back (the Mac reads it again each time).
    for exit_action in (0, 1, 2):
        for free_action in (0, 1, 2):
            yield exit_case(r, 1, 0, 0, 0, (exit_action, free_action, 0))
    # The sky objects' own function replaces the member (it is read again for the destructor).
    yield exit_case(r, 0, 0x400000, 1, 1, (0, 0, 1))
    yield exit_case(r, 1, 0x400000, 1, 1, (1, 1, 1))


def random_exit(r):
    return exit_case(r)


# ------------------------------------------------------------------------------------------------------------ CInGame::Start
# description: the 1024 palette bytes, the 5 x 50 colour bytes, nothing else


def start_ppc(e, case):
    p = Ppc(e, case)
    try:
        palette_bytes, colour_bytes = case["palette"], case["colours"]
        palette = e.data_import("_lbGlobalPalette")
        e.write(palette, bytes(palette_bytes))
        block = bytearray(0x120)
        for r in range(5):
            block[r * 0x38:r * 0x38 + 50] = bytes(colour_bytes[r * 50:(r + 1) * 50])
            block[r * 0x38 + 50:r * 0x38 + 52] = bytes([0x30 + r, 0x40 + r])
            block[r * 0x38 + 52:r * 0x38 + 56] = struct.pack(">I", 0x1000 + r * 0x111)
        block[0x118:0x11C] = struct.pack(">I", 0x2000)
        e.write(G_COLOURS, bytes(block))
        p.call(IN_GAME_START)
        after = e.read(G_COLOURS, 0x120)
        state = []
        for r in range(5):
            state += list(after[r * 0x38:r * 0x38 + 50])
        state.append(int(e.read(palette, 1024) == bytes(palette_bytes)))
        rest = all(after[r * 0x38 + 50:r * 0x38 + 56] == block[r * 0x38 + 50:r * 0x38 + 56] for r in range(5))
        state.append(int(rest and after[0x118:0x11C] == block[0x118:0x11C]))
        return dict(state=state, log=p.L.log)
    finally:
        p.done()


def start_host(lib, case):
    state, log = host_run(lib, "tpw_l6_start", list(case["palette"]) + list(case["colours"]))
    return dict(state=state, log=log)


def start_case(palette, colours):
    return dict(palette=list(palette), colours=list(colours))


def edge_start():
    r = random.Random(7502)
    yield start_case([0] * 1024, [0xA5] * 250)
    yield start_case([0xFF] * 1024, [0] * 250)
    yield start_case([(i // 4 * 3 + i % 4 * 70 + 1) & 0xFF for i in range(1024)], [0x11] * 250)
    # Every entry's four bytes distinct (alpha, red, green, blue of the result are told apart) and the colours' bytes all distinct.
    yield start_case([((i // 4) + (i % 4) * 0x40) & 0xFF for i in range(1024)], [i + 1 for i in range(250)])
    yield start_case([r.getrandbits(8) for _ in range(1024)], [0xFF] * 250)
    for _ in range(4):
        yield start_case([r.getrandbits(8) for _ in range(1024)], [r.getrandbits(8) for _ in range(250)])


def random_start(r):
    return start_case([r.getrandbits(8) for _ in range(1024)], [r.getrandbits(8) for _ in range(250)])


# ------------------------------------------------------------------------------------------------------------ CursorLibrary::RemoveAll
# description: nodes, cursor bits, poison


def cursors_ppc(e, case):
    p = Ppc(e, case)
    try:
        L = p.L
        count, bits, poison = case["d"]
        p.poison = poison
        nodes = [e.alloc(0x14, pattern(0x14, 0xA5)) for _ in range(count)]
        cursors = []
        for i, node in enumerate(nodes):
            cursor = e.alloc(0x14, pattern(0x14, 0x5A)) if (bits >> i) & 1 else 0
            if cursor:
                ws(e, cursor + 0x10, [e.l6_cursor_vtable])
                L.labels[cursor] = K_CURSOR + i
            cursors.append(cursor)
            L.labels[node] = K_NODE + i
            ws(e, node, [cursor])
            ws(e, node + 0x10, [nodes[i + 1] if i + 1 < count else 0])
        p.nodes, p.cursors = nodes, [c if c else -1 for c in cursors]
        ws(e, G_CURSORS, [nodes[0] if count else 0])
        head = e.u32(G_CURSORS)
        p.call(REMOVE_ALL)
        state = [int(e.u32(G_CURSORS) == head)]
        for i in range(count):
            state += [int(L.lab(nodes[i]) > 0), int(cursors[i] != 0 and L.lab(cursors[i]) > 0)]
        return dict(state=state, log=L.log)
    finally:
        p.done()


def cursors_host(lib, case):
    state, log = host_run(lib, "tpw_l6_cursors", case["d"])
    return dict(state=state, log=log)


def cursors_case(count, bits, poison=-1):
    return dict(d=[count, bits & ((1 << count) - 1), poison])


def edge_cursors():
    r = random.Random(7503)
    yield cursors_case(0, 0)
    for count in (1, 2, 3, 5, 12):
        for bits in (0, (1 << count) - 1, 1, 1 << (count - 1), 0b101010101010):
            yield cursors_case(count, bits)
    # A cursor's destructor clears its node's next word: the walk goes on as the Mac's (the next was read first).
    yield cursors_case(3, 0b111, 0)
    yield cursors_case(3, 0b111, 1)
    yield cursors_case(3, 0b111, 2)
    yield cursors_case(4, 0b1011, 3)
    for _ in range(4):
        count = r.randint(1, 12)
        yield cursors_case(count, r.getrandbits(12), r.randrange(count))


def random_cursors(r):
    count = r.randint(0, 12)
    return cursors_case(count, r.getrandbits(12), r.randrange(count) if count and r.random() < 0.5 else -1)


# ------------------------------------------------------------------------------------------------------------ FuckOffUIProgressBar


def progress_ppc(e, case):
    p = Ppc(e, case)
    try:
        window = e.alloc(16) if case["d"][0] else 0
        if window:
            p.L.labels[window] = 9
        ws(e, G_PROGRESS, [window])
        p.call(FUCK_OFF)
        return dict(state=[p.L.lab(e.u32(G_PROGRESS))], log=p.L.log)
    finally:
        p.done()


def progress_host(lib, case):
    state, log = host_run(lib, "tpw_l6_progress", case["d"])
    return dict(state=state, log=log)


def edge_progress():
    return [dict(d=[0]), dict(d=[1])]


def random_progress(r):
    return dict(d=[r.randint(0, 1)])


# ------------------------------------------------------------------------------------------------------------ InterfaceImage::~InterfaceImage


def image_ppc(e, case):
    p = Ppc(e, case)
    try:
        flag = case["flag"]
        image = e.alloc(0x140, pattern(0x140, 0xA5))
        ws(e, image + 0x118, [0x1234ABCD])
        p.L.labels[image] = 1
        answer = p.call(IMAGE_DTOR, image, flag & MASK)
        return dict(state=[int(p.L.lab(image) > 0), int(answer == image)], log=p.L.log)
    finally:
        p.done()


def image_host(lib, case):
    state, log = host_run(lib, "tpw_l6_image", [T.s32(case["flag"])])
    return dict(state=state, log=log)


def edge_image():
    for flag in (1, 0, -1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x7FFFFFFF, 0x80000000, 0xFFFF0001, 0xFFFF8000, 0x12345678):
        yield dict(flag=flag & MASK)


def random_image(r):
    return dict(flag=r.choice([1, 0, MASK, r.getrandbits(32), r.getrandbits(16), r.getrandbits(32) & 0xFFFF0000,
                               r.getrandbits(32) | 0x8000, (r.getrandbits(16) << 16) | 1]))


# ------------------------------------------------------------------------------------------------------------ DestroyEngineMesh, DestroyMeshInstance
# description: op, flags, other, count, other count, engine, allocation, heap, flags argument, fill


def mesh_ppc(e, case):
    p = Ppc(e, case)
    try:
        L = p.L
        op, flags, other, count, other_count, engine, allocation, heap, argument, fill = case["d"]
        mesh = e.alloc(0x40, pattern(0x40, fill))
        other_mesh = e.alloc(0x40, pattern(0x40, 0x33))
        engine_mesh = e.alloc(0x160, pattern(0x160, 0x77)) if engine else 0
        block = e.alloc(16) if allocation else 0
        if engine:
            ws(e, engine_mesh + 0x150, [e.l6_engine_vtable])
        ws(e, mesh, [flags])
        ws(e, mesh + 8, [0 if other == 0 else other_mesh if other == 1 else mesh])
        ws(e, mesh + 0x1C, [engine_mesh])
        ws(e, mesh + 0x24, [block])
        ws(e, mesh + 0x34, [count])
        ws(e, other_mesh + 0x34, [other_count])
        for address, label in ((mesh, K_MESH), (other_mesh, K_OTHER_MESH), (engine_mesh, K_ENGINE), (block, K_ALLOCATION)):
            if address:
                L.labels[address] = label
        before = e.read(mesh, 0x40)
        if op == 0:
            answer = 1
            p.call(DESTROY_ENGINE_MESH, mesh)
        else:
            answer = int(p.call(DESTROY_MESH_INSTANCE, mesh, heap & MASK, argument & MASK) == 1)
        after = e.read(mesh, 0x40)
        keep = [i for i in range(0x40) if not (i < 4 or 0x1C <= i < 0x20 or 0x34 <= i < 0x38)]
        same = all(before[i] == after[i] for i in keep)
        state = [e.u32(mesh), L.lab(e.u32(mesh + 8)), L.lab(e.u32(mesh + 0x1C)), T.s32(e.u32(mesh + 0x34)),
                 T.s32(e.u32(other_mesh + 0x34)), int(engine_mesh != 0 and L.lab(engine_mesh) > 0), int(block != 0 and L.lab(block) > 0),
                 int(same)]
        return dict(state=state, log=L.log, answer=answer)
    finally:
        p.done()


def mesh_host(lib, case):
    state, log = host_run(lib, "tpw_l6_mesh", case["d"], case.get("q"))
    return dict(state=state, log=log, answer=1)


def mesh_case(r, op=None, flags=None, other=None, count=None, engine=None, allocation=None, disable=None):
    pick_count = lambda: r.choice([0, 1, -1, 5, 0x7FFFFFFF, 0x80000000 - 2 ** 32, 100, r.randint(-1000, 1000)])
    op = r.randint(0, 1) if op is None else op
    flags = r.choice([0, 1, 2, 0x11, 0x19, 0xFFFFFFFF, 0xFFFFFFFE, 0x80000001, r.getrandbits(32)]) if flags is None else flags
    other = r.randint(0, 2) if other is None else other
    count = pick_count() if count is None else count
    engine = r.randint(0, 1) if engine is None else engine
    allocation = r.randint(0, 1) if allocation is None else allocation
    disable = r.choice([[0, 0], [1, 1], [0, 1], [1, 0], [1]]) if disable is None else disable
    return dict(d=[op, flags, other, count, pick_count(), engine, allocation, r.getrandbits(32), r.getrandbits(32), r.randint(0, 255)],
                q={"Disable": disable})


def edge_mesh():
    r = random.Random(7504)
    for op in (0, 1):
        for flags in (0, 1, 2, 3, 0xFFFFFFFE):
            for other in (0, 1, 2):
                yield mesh_case(r, op, flags, other, r.choice([0, 1, 5]), 1, 1, [0, 0])
    for op in (0, 1):
        for disable in ([0, 0], [1, 0], [1, 1], [0, 1]):
            yield mesh_case(r, op, 1, 0, 3, 1, 1, disable)
        for engine in (0, 1):
            for allocation in (0, 1):
                yield mesh_case(r, op, 0, 1, 3, engine, allocation, [0])
        for count in (0, 1, -1, 0x7FFFFFFF, -2 ** 31):
            yield mesh_case(r, op, 0, 0, count, 1, 1, [0])
            yield mesh_case(r, op, 0, 1, count, 1, 1, [0])


def random_mesh(r):
    return mesh_case(r)


# ------------------------------------------------------------------------------------------------------------ StripRideWrapping
# description: all (+0x42), split (+0x44), node argument, the flags, the amount's bits, data set, point set


def strip_ppc(e, case):
    p = Ppc(e, case)
    try:
        L = p.L
        all_count, split, node, flags, amount_bits, data_set, point_set = case["desc"]
        full_count = min(all_count, split)
        mini_count = max(all_count - split, 0)
        set_address = e.alloc(0x100, pattern(0x100, 0x44))
        mesh = e.alloc(0x40, pattern(0x40, 0x55))
        header = e.alloc(0xB8, pattern(0xB8, 0x66))
        full = e.alloc(0xA0 * (full_count + 1), pattern(0xA0 * (full_count + 1), 0x77))
        mini = e.alloc(0x58 * (mini_count + 1), pattern(0x58 * (mini_count + 1), 0x88))
        stray = e.alloc(0xA0, pattern(0xA0, 0x99))
        data = e.alloc(16)
        point = e.alloc(12)
        ws(e, set_address + 8, [mesh])
        ws(e, mesh + 4, [header])
        e.write(header + 0x42, struct.pack(">HH", all_count, split))
        ws(e, header + 0x70, [full, mini])
        L.labels[header] = K_HEADER
        for i in range(full_count):
            ws(e, full + 0xA0 * i, [flags[i]])
            L.labels[full + 0xA0 * i] = K_FULL + i
        for i in range(mini_count):
            ws(e, mini + 0x58 * i, [flags[full_count + i]])
            L.labels[mini + 0x58 * i] = K_MINI + i
        L.labels[stray], L.labels[data], L.labels[point] = K_STRAY, K_DATA, K_POINT
        argument = 0 if node == 0 else stray if node == -1 else mini + 0x58 * (node - 1000) if node >= 1000 else full + 0xA0 * (node - 1)
        amount = struct.unpack(">f", struct.pack(">I", amount_bits))[0]
        p.call(STRIP_RIDE_WRAPPING, set_address, argument, T.F(amount), data if data_set else 0, point if point_set else 0)
        return dict(state=[len(p.seen)], log=L.log)
    finally:
        p.done()


def strip_host(lib, case):
    all_count, split, node, flags, amount_bits, data_set, point_set = case["desc"]
    state, log = host_run(lib, "tpw_l6_strip", [all_count, split, node] + list(flags) + [amount_bits, data_set, point_set], case.get("q"))
    return dict(state=state, log=log)


AMOUNTS = [0x3CA3D70A, 0, 0x80000000, 0x3F800000, 0xBF800000, 0x7F800000, 0xFF800000, 0x7FC00000, 0x00000001, 0x007FFFFF]


def strip_case(r, all_count=None, split=None, node=None):
    all_count = r.choice([0, 1, 2, 3, 5, 8, 20]) if all_count is None else all_count
    if split is None:
        split = r.choice([0, 1, all_count, all_count + 1, max(all_count - 1, 0), 3, 0x8000, 0xFFFF, 2, 7])
    full_count, mini_count = min(all_count, split), max(all_count - split, 0)
    flags = [r.choice([0, 0x200, 0x1FF, 0x201, 0x400, 0xFFFFFFFF, 0x200 | r.getrandbits(32), r.getrandbits(32) & ~0x200]) for _ in range(all_count)]
    if node is None:
        node = r.choice([0, 0, 0, -1] + ([r.randint(1, full_count)] if full_count else []) + ([1000 + r.randrange(mini_count)] if mini_count else []))
    amount = r.choice(AMOUNTS + [r.getrandbits(32) & 0x7FFFFFFF if r.random() < 0.5 else 0x3C23D70A])
    if (amount & 0x7F800000) == 0x7F800000 and (amount & 0x7FFFFF):
        amount = 0x7FC00000
    counters = [r.choice([0, 5, -1, 0x7FFFFFFF, r.randint(-100, 100)]) for _ in range(all_count + 2)]
    return dict(desc=[all_count, split, node, flags[:full_count + mini_count], amount, r.randint(0, 1), r.randint(0, 1)],
                q={"Counter": counters})


def edge_strip():
    r = random.Random(7505)
    for all_count in (0, 1, 2, 3, 5):
        for split in (0, 1, all_count, all_count + 1, max(all_count - 1, 0), 0x8000, 0xFFFF):
            for node in (0, -1):
                yield strip_case(r, all_count, split, node)
    # A node given: from the full nodes, from the mini nodes (a flag-0x200 node is done too), one that is in neither.
    for _ in range(6):
        yield strip_case(r, 4, 2, 1)
        yield strip_case(r, 4, 2, 2)
        yield strip_case(r, 4, 2, 1000)
        yield strip_case(r, 4, 2, 1001)
    # Every flag combination of three full nodes and two mini nodes.
    for bits in range(32):
        case = strip_case(r, 5, 3, 0)
        case["desc"][3] = [0x200 if (bits >> i) & 1 else 0 for i in range(5)]
        yield case


def random_strip(r):
    return strip_case(r)


def edge_for_op(op):
    return lambda: (c for c in edge_mesh() if c["d"][0] == op)


TESTS = [
    FnTest("CGame::ExitLevel", exit_ppc, exit_host, edge=edge_exit, rnd=random_exit),
    FnTest("CInGame::Start", start_ppc, start_host, edge=edge_start, rnd=random_start),
    FnTest("CursorLibrary::RemoveAll", cursors_ppc, cursors_host, edge=edge_cursors, rnd=random_cursors),
    FnTest("FuckOffUIProgressBar", progress_ppc, progress_host, edge=edge_progress, rnd=random_progress),
    FnTest("InterfaceImage::~InterfaceImage", image_ppc, image_host, edge=edge_image, rnd=random_image),
    FnTest("DestroyEngineMesh", mesh_ppc, mesh_host, edge=edge_for_op(0), rnd=lambda r: mesh_case(r, 0)),
    FnTest("DestroyMeshInstance", mesh_ppc, mesh_host, edge=edge_for_op(1), rnd=lambda r: mesh_case(r, 1)),
    FnTest("StripRideWrapping", strip_ppc, strip_host, edge=edge_strip, rnd=random_strip),
]
