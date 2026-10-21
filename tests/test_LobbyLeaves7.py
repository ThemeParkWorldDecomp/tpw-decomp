"""Unit w81, the lobby's small leaves part 7, against libtpw_unit_LobbyLeaves7.dylib (tests/unit_LobbyLeaves7.cpp), in the main app
("SimTheme Park"):
  src/game/Game.cpp    CGame::ClearThemeResources 0x10149334
  src/game/rides/RideDB.cpp        CRideDatabase::ClearDatabase 0x101546e8
  src/game/rides/RidePlacement.cpp        StripNodeWrapping 0x101edafc
  src/game/ui/UI_Base.cpp         activateUI 0x101ca30c
  src/game/ui/UIInterface.cpp            UIInterface::Pause 0x101ef8a0
  src/game/World.cpp          CWorld::ExitLevel 0x1013a9fc, CWorld::Free 0x1013a264, CWorld::FreeThingHeap 0x1013ac08
  src/game/people/StaffDB.cpp        CStaffDB::~CStaffDB 0x10128f68
  src/game/park/SkyObjects.cpp  CSkyObjects::DestroySkyObjects 0x1015b804

A case is a world of objects and an operation, built twice from one description: in emulator memory at the Mac's layouts for the original and by
the unit's entry points (tpw_l7_*) for the translation. Every callee outside the unit is a fake that records the same event on both sides
(the resource manager, the ride record's destructor and the allocator among them); the fakes can be told by the case's queues to change the
globals behind the function's back (the resource manager pointer, the ride count and slots, gGameWorld, the sky count, the window links),
which shows that the Mac reads them again.

What only the PowerPC side can see is checked there and compared as a number the host reports as 1: the answers in r3 (the staff
destructor's `this`), the NULL object of the staff destructor, __destroy_arr's size 0x14, count 0x20 and element destructor 0x101283d4,
the ride destructor's flag (a positive one frees the record, which both sides log as a deletion), and Free's call of FreeThingHeap.

Not tested: the destructors on a NULL object (the host never calls with NULL; the staff destructor's NULL case is checked on the PowerPC side
only), a ride slot the destructor is given as NULL, __destroy_arr's order (not in the dump: the runtime library is an import; the fake follows
the rule tests/test_bflWadTree.py uses: last element first, flag -1), the empty loop of FreeThingHeap (silent), the data map read past the
tables (the generators keep the cells inside the buffer on both sides).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyLeaves7"
MASK = 0xFFFFFFFF

CLEAR_THEME, CLEAR_DATABASE, STRIP_NODE, ACTIVATE_UI, UI_PAUSE = 0x10149334, 0x101546E8, 0x101EDAFC, 0x101CA30C, 0x101EF8A0
WORLD_EXIT, WORLD_FREE, FREE_HEAP, STAFF_DTOR, SKY_DESTROY = 0x1013A9FC, 0x1013A264, 0x1013AC08, 0x10128F68, 0x1015B804
GET_THEME, FIND_RESOURCE, UNREGISTER = 0x1014A058, 0x10165448, 0x10165244
DELETE_CUSTOM, DEALLOC_STRINGS, RIDE_DTOR = 0x10156E34, 0x101550BC, 0x1015457C
ALLOC, FREE, ERROR_BOX = 0x10064BD4, 0x10064C54, 0x1006FD3C
POST_MESSAGE, GET_CHILD, GET_NEXT, PAUSE_UI = 0x101C9400, 0x101DBAC4, 0x101DBB38, 0x1018679C
ASSIGN_MOUSE, DELETE_MAP, DELETE_ALL, CLEAR_BEHAVIOURS, BMP_CLOSE = 0x1007E2E0, 0x1010136C, 0x1013AC4C, 0x10133358, 0x10025CB0
STAFF_INFO_DTOR = 0x101283D4
G_GAME, G_TP, G_MOUSE, G_WORLD, G_BALANCE = 0x1037F134, 0x103B74C8, 0x102DC1F4, 0x1037EBFC, 0x102B4568
NUM_SKY_OBJECTS = G_BALANCE + 0x6E4
FLAG_OFFSET = 0x1DA738

# The names ClearThemeResources looks the standing resources up by, in the order of its calls: (offset in gTPGlobals, default text).
NAMES = [(0x4614, "SharedTex"), (0x4628, "Small_SharedTex"), (0x4650, "Rides"), (0x4740, "Features"), (0x472C, "Sideshow"),
         (0x4704, "Upgrades"), (0x4718, "Shops"), (0x47CC, "LocalAdvisor"), (0x46C8, "Terrain"), (0x47E0, "Dynamic"),
         (0x47F4, "Hoarding"), (0x4808, "MiscMesh"), (0x481C, "Queue")]
CALLS = len(NAMES) + 1


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def pattern(n, fill):
    return bytes([fill & 0xFF]) * n


def f32(x):
    """x rounded to a single, as a Python float (saturating to infinity)."""
    try:
        return struct.unpack(">f", struct.pack(">f", x))[0]
    except OverflowError:
        return math.copysign(math.inf, x)


def bits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def from_bits(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon(b):
    """A float's bits with every NaN one value."""
    return 0x7FC00000 if (b & 0x7FFFFFFF) > 0x7F800000 else b & MASK


# ---------------------------------------------------------------------------------------------------------------- the fakes


def build_stubs(heap_recorded):
    """The PowerPC fakes. heap_recorded: FreeThingHeap is a fake that notes its `this` (to show that Free called it) instead of the original."""
    S = CL.Stubs()
    R = CL.recorder
    S.real = {CLEAR_THEME, CLEAR_DATABASE, STRIP_NODE, ACTIVATE_UI, UI_PAUSE, WORLD_EXIT, WORLD_FREE, STAFF_DTOR, SKY_DESTROY}
    if not heap_recorded:
        S.real.add(FREE_HEAP)

    def manager_hook(key):
        def hook(e):
            action = e.cl.ret(key, 0)
            if action == 1:
                ws(e, G_GAME + 0x34, [e.l7.manager_b])
            elif action == 2:
                ws(e, G_GAME + 0x34, [0])
            elif action == 3:
                ws(e, G_GAME + 0x34, [e.l7.manager_a])
        return hook

    swap, swap_unregistered = manager_hook("Swap"), manager_hook("SwapU")

    swap_theme = manager_hook("SwapT")

    def get_theme(e):
        e.cl.rec("GetCurrentThemeName", (e.cl.lab(e.r(3)),))
        swap_theme(e)
        e.set_r(3, e.r(3))

    def find(e):
        L = e.cl
        L.rec("FindResource", (L.lab(e.r(3)),), (CL.tds_text(e, e.r(4)),))
        result = T.s32(L.ret("Find", 0))
        if result == 1:
            ws(e, e.r(5), [e.l7.resources[L.ret("Res", 0) & 15]])
        swap(e)
        e.set_r(3, T.u32(result))

    def unregister(e):
        e.cl.rec("UnregisterResource", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))
        swap_unregistered(e)
        e.set_r(3, 1)

    def delete_custom(e):
        e.cl.rec("DeleteCustomEffects", (e.cl.lab(e.r(3)),))
        count = e.cl.ret("CustomCount", -9999)
        if count != -9999:
            ws(e, e.r(3) + 0x528, [count])

    def dealloc(e):
        L, ride, db = e.cl, e.r(3), e.l7.db
        L.rec("DeallocStrings", (L.lab(ride),))
        action = L.ret("Dealloc", 0)
        if action == 1:
            ws(e, db + 0x528, [L.ret("DeallocCount", 0)])
        elif action == 2:
            for i in range(12):
                if e.u32(db + 4 * i) == ride:
                    ws(e, db + 4 * i, [e.l7.other_ride])

    def alloc(e):
        L, size = e.cl, e.r(3)
        L.rec("MReleaseAllocF", (size, e.r(4)))
        if size == 0:
            e.set_r(3, 0)
        else:
            block = e.alloc(size, bytes(size))
            L.newlab(block)
            e.set_r(3, block)

    def error_box(e):
        e.cl.rec("ERRORBox", (), (e.cstr(e.r(3)), e.cstr(e.r(4))))

    def post_message(e):
        L, window = e.cl, e.r(3)
        L.rec("UI_PostMessage", (L.lab(window), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))))
        action = L.ret("Relink", 0)
        child, nxt = e.l7.child, e.l7.next
        if action == 1:
            nxt[window] = 0
        elif action == 2 and nxt[window]:
            nxt[window] = nxt[nxt[window]]
        elif action == 3:
            child[window] = 0

    def get_child(e):
        e.cl.rec("GetChild", (e.cl.lab(e.r(3)), T.s32(e.r(4))))
        e.set_r(3, e.l7.child[e.r(3)])

    def get_next(e):
        e.cl.rec("GetNext", (e.cl.lab(e.r(3)),))
        e.set_r(3, e.l7.next[e.r(3)])

    def world_flag(e):
        return e.u32(e.l7.world + FLAG_OFFSET)

    def assign_mouse(e):
        L = e.cl
        L.rec("AssignMouseMode", (L.lab(e.r(3)), L.lab(e.r(4)), world_flag(e)))
        action = L.ret("MouseAction", 0)
        if action == 1:
            ws(e, G_WORLD, [e.l7.other_world])
        elif action == 2:
            ws(e, G_WORLD, [0])

    def delete_map(e):
        e.cl.rec("DeleteMapInEngine", (e.cl.lab(e.r(3) - 0x2D8), world_flag(e)))

    def delete_all(e):
        e.cl.rec("DeleteAllThings", (e.cl.lab(e.r(3)), world_flag(e)))

    def staff_info_dtor(e):
        e.cl.rec("CStaffInfo.dtor", ((e.r(3) - e.l7.staff_base) // 0x14,))

    def remove_add(e):
        L = e.cl
        L.rec("RemoveAdd", (L.lab(e.r(3)), L.lab(e.r(4))))
        count = L.ret("Count", -9999)
        if count != -9999:
            ws(e, NUM_SKY_OBJECTS, [count])
        replace = L.ret("Replace", -1)
        if 0 <= replace < 16:
            ws(e, e.l7.objects + 4 * replace, [e.l7.other_object])

    def delete(e):
        L, block = e.cl, e.r(3)
        if block and L.lab(block) > 0:
            L.rec("delete", (L.lab(block),))
            L.forget(block)

    def modf(e):
        fraction, whole = math.modf(e.f(1))
        e.write(e.r(5), struct.pack(">d", whole))
        e.set_f1(fraction)

    def free_heap(e):
        e.l7.heap_calls.append(e.r(3))

    S.internal(GET_THEME, get_theme)
    S.internal(FIND_RESOURCE, find)
    S.internal(UNREGISTER, unregister)
    S.internal(DELETE_CUSTOM, delete_custom)
    S.internal(DEALLOC_STRINGS, dealloc)
    S.internal(RIDE_DTOR, R("RideInfo.dtor", "D"))
    S.internal(ALLOC, alloc)
    S.internal(FREE, R("MReleaseFreeF", "L"))
    S.internal(ERROR_BOX, error_box)
    S.internal(POST_MESSAGE, post_message)
    S.internal(GET_CHILD, get_child)
    S.internal(GET_NEXT, get_next)
    S.internal(PAUSE_UI, R("PauseUI"))
    S.internal(ASSIGN_MOUSE, assign_mouse)
    S.internal(DELETE_MAP, delete_map)
    S.internal(DELETE_ALL, delete_all)
    S.internal(CLEAR_BEHAVIOURS, lambda e: e.cl.rec("clear_behaviours", (world_flag(e),)))
    S.internal(BMP_CLOSE, lambda e: e.cl.rec("BMP_CloseSystem", (world_flag(e),)))
    S.internal(STAFF_INFO_DTOR, staff_info_dtor)
    S.imp("RemoveAdd__4CSkyFPv", remove_add)
    S.imp("__dl__FPv", delete)
    S.imp("modf", modf)
    if heap_recorded:
        S.internal(FREE_HEAP, free_heap)
    return S


STUBS = build_stubs(False)
STUBS_HEAP = build_stubs(True)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if hasattr(e, "l7_setup_done"):
        return
    e.l7_setup_done = True
    e.l7 = None

    def destroy_arr(em, call):
        # The runtime's __destroy_arr(first, destructor, size, count): the destructor on each element from the last to the first with the
        # flag -1. What the original passes is recorded when it is not what the translation implies.
        first, dtor, size, count = em.r(3), em.r(4), em.r(5), em.r(6)
        if size != 0x14 or count != 0x20:
            em.cl.rec("BadDestroyArr", (size, count))
        if em.u32(dtor) != STAFF_INFO_DTOR:
            em.cl.rec("BadDestroyDtor", (em.u32(dtor),))
        for i in reversed(range(count)):
            call(dtor, first + i * size, MASK)
        return None

    T.stub_cb(e, "__destroy_arr", destroy_arr)


class Ppc:
    """One case on the PowerPC side: the fakes active, a fresh log, the case's state for the fakes."""

    def __init__(self, e, case, stubs=STUBS):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case.get("q"))
        e.l7 = self
        self.manager_a = self.manager_b = 0
        self.resources = []
        self.db = self.other_ride = 0
        self.child, self.next = {}, {}
        self.world = self.other_world = 0
        self.staff_base = 0
        self.objects = self.other_object = 0
        self.heap_calls = []
        self.saved = stubs.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)
        self.e.l7 = None

    def call(self, address, *args):
        return T.call_ppc(self.e, address, list(args))[0]


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


def host_only(entry):
    def run(lib, case):
        state, log = host_run(lib, entry, case["d"], case.get("q"))
        return dict(state=state, log=log)
    return run


# ------------------------------------------------------------------------------------------------ CGame::ClearThemeResources
# description: the length of the current theme's name, its characters, the first manager (0 none, 1 the first)
# queues: Find, Res, Swap, SwapU (one value per call; the first call is the theme's own resource)

THEME_NAMES = ["fantasy", "", "x", "Fantasy Theme", "a name of nineteen!", "jungle"]


def theme_ppc(e, case):
    p = Ppc(e, case)
    try:
        L = p.L
        name = case["name"]
        this = e.alloc(0x40, pattern(0x40, 0xA5))
        CL.tds_set(e, this, "the wrong object")
        CL.tds_set(e, G_GAME, name)
        for offset, text in NAMES:
            CL.tds_set(e, G_TP + offset, text)
        p.manager_a, p.manager_b = e.alloc(16, pattern(16, 1)), e.alloc(16, pattern(16, 2))
        p.resources = [e.alloc(16, pattern(16, 3 + i)) for i in range(16)]
        ws(e, G_GAME + 0x34, [p.manager_a if case["manager"] else 0])
        for address, label in ((G_GAME, 1), (this, 2), (p.manager_a, 3), (p.manager_b, 4)):
            L.labels[address] = label
        for i, address in enumerate(p.resources):
            L.labels[address] = 50 + i
        p.call(CLEAR_THEME, this)
        state = [L.lab(e.u32(G_GAME + 0x34)), int(CL.tds_text(e, G_GAME) == name), int(CL.tds_text(e, this) == "the wrong object")]
        return dict(state=state, log=L.log)
    finally:
        p.done()


def theme_host(lib, case):
    name = case["name"]
    state, log = host_run(lib, "tpw_l7_theme", [len(name)] + [ord(c) for c in name] + [case["manager"]], case.get("q"))
    return dict(state=state, log=log)


def theme_case(r, find=None, res=None, swap=None, swap_u=None, name=None, manager=None, swap_t=None):
    find = [r.choice([1, 1, 0, 2, -1, 0x7FFFFFFF, -0x80000000, 1, 0]) for _ in range(CALLS)] if find is None else find
    res = [r.randrange(16) for _ in range(CALLS)] if res is None else res
    swap = [r.choice([0, 0, 0, 1, 2, 3]) for _ in range(CALLS)] if swap is None else swap
    swap_u = [r.choice([0, 0, 0, 1, 2, 3]) for _ in range(CALLS)] if swap_u is None else swap_u
    swap_t = [r.choice([0, 0, 0, 1, 2, 3])] if swap_t is None else swap_t
    return dict(name=r.choice(THEME_NAMES) if name is None else name, manager=r.choice([1, 1, 0]) if manager is None else manager,
                q={"Find": find, "Res": res, "Swap": swap, "SwapU": swap_u, "SwapT": swap_t})


def edge_theme():
    r = random.Random(8101)
    zeros = [0] * CALLS
    yield theme_case(r, [1] * CALLS, list(range(CALLS)), zeros, zeros, "fantasy", 1)
    yield theme_case(r, zeros, zeros, zeros, zeros, "fantasy", 1)
    yield theme_case(r, [1] * CALLS, list(range(CALLS)), zeros, zeros, "", 0)
    # the theme-name call itself replaces the manager (the manager is read after it)
    for action in (1, 2, 3):
        yield theme_case(r, [1] * CALLS, list(range(CALLS)), zeros, zeros, "fantasy", 1, [action])
        yield theme_case(r, [1] * CALLS, list(range(CALLS)), zeros, zeros, "fantasy", 0, [action])
    for k in range(CALLS):
        # only the k-th call finds something
        find = [1 if i == k else 0 for i in range(CALLS)]
        yield theme_case(r, find, [r.randrange(16)] * CALLS, zeros, zeros, THEME_NAMES[k % len(THEME_NAMES)], 1)
        # the manager is replaced after the k-th find (by the second one, by nothing, by the first again), whether it found or not
        for action in (1, 2, 3):
            swap = [action if i == k else 0 for i in range(CALLS)]
            yield theme_case(r, [1] * CALLS, list(range(CALLS)), swap, zeros, "jungle", 1)
            yield theme_case(r, zeros, zeros, swap, zeros, "jungle", 1)
        # ... or after the k-th unregister
        for action in (1, 2):
            swap_u = [action if i == k else 0 for i in range(CALLS)]
            yield theme_case(r, [1] * CALLS, list(range(CALLS)), zeros, swap_u, "jungle", 1)


def random_theme(r):
    return theme_case(r)


# ------------------------------------------------------------------------------------------------ CRideDatabase::ClearDatabase
# description: the ride count, the cached id, the cached ride (0 none, 1 + k the k-th ride, 100 the other one), the fill byte
# queues: CustomCount (the count DeleteCustomEffects leaves), Dealloc (per ride: 0, 1 the count becomes DeallocCount, 2 the ride's slot is
# given the other ride), DeallocCount

RIDES = 12


def database_ppc(e, case):
    p = Ppc(e, case)
    try:
        L = p.L
        count, cached_id, cached, fill = case["d"]
        db = p.db = e.alloc(0x740, pattern(0x740, fill))
        rides = [e.alloc(0x20, pattern(0x20, 0x40 + i)) for i in range(RIDES)]
        other = p.other_ride = e.alloc(0x20, pattern(0x20, 0x7F))
        ws(e, db, [rides[i] if i < RIDES else 0 for i in range(0x14A)])
        ws(e, db + 0x528, [count])
        e.write(db + 0x730, struct.pack(">H", cached_id))
        ws(e, db + 0x734, [0 if cached == 0 else other if cached == 100 else rides[cached - 1]])
        L.labels[db] = 1
        for i, address in enumerate(rides):
            L.labels[address] = 20 + i
        L.labels[other] = 80
        middle = e.read(db + 0x52C, 0x730 - 0x52C)
        p.call(CLEAR_DATABASE, db)
        state = [T.s32(e.u32(db + 0x528)), struct.unpack(">H", e.read(db + 0x730, 2))[0], L.lab(e.u32(db + 0x734))]
        state += [L.lab(e.u32(db + 4 * i)) for i in range(16)]
        state += [int(L.lab(address) > 0) for address in rides]
        state.append(int(L.lab(other) > 0))
        state.append(int(e.read(db + 0x52C, 0x730 - 0x52C) == middle))
        return dict(state=state, log=L.log)
    finally:
        p.done()


def database_case(r, count=None, cached=None, custom=None, dealloc=None, dealloc_count=None):
    count = r.choice([0, 1, 2, 5, RIDES, RIDES, r.randint(0, RIDES), -1, -5, -0x80000000]) if count is None else count
    cached = r.choice([0, 0, 1, 5, RIDES, 100]) if cached is None else cached
    custom = r.choice([-9999, -9999, -9999, 0, 3, -2, RIDES]) if custom is None else custom
    dealloc = [r.choice([0, 0, 0, 1, 2]) for _ in range(RIDES)] if dealloc is None else dealloc
    # (only one ride may be given the other ride: the host would delete the other ride twice, which its allocator refuses)
    first_two = [i for i, action in enumerate(dealloc) if action == 2]
    dealloc = [0 if action == 2 and i != first_two[0] else action for i, action in enumerate(dealloc)]
    dealloc_count = [r.choice([0, 1, 4, -1, RIDES, 7]) for _ in range(RIDES)] if dealloc_count is None else dealloc_count
    return dict(d=[count, r.getrandbits(16), cached, r.randint(0, 255)],
                q={"CustomCount": [custom], "Dealloc": dealloc, "DeallocCount": dealloc_count})


def edge_database():
    r = random.Random(8102)
    none = [0] * RIDES
    for count in (0, 1, 2, 7, RIDES, -1, -2, -0x80000000, 0x7FFFFFFF if False else RIDES):
        for cached in (0, 1, 100):
            yield database_case(r, count, cached, -9999, none)
    # DeleteCustomEffects leaves another count (it is read after the call: the loop runs that many times; negative runs none)
    for custom in (0, 3, RIDES, -4):
        yield database_case(r, 6, 2, custom, none)
    # DeallocStrings of a ride changes the count (the next test uses the new one) or replaces the ride's slot (the destructor gets that one)
    for ride in (0, 1, 3, RIDES - 1):
        for action, count in ((1, 0), (1, 1), (1, 3), (1, RIDES), (1, -1), (2, 0)):
            dealloc = [action if i == ride else 0 for i in range(RIDES)]
            yield database_case(r, RIDES, 100, -9999, dealloc, [count] * RIDES)
    yield database_case(r, 5, 3, -9999, [2] + [0] * (RIDES - 1))


def random_database(r):
    return database_case(r)


def database_host(lib, case):
    state, log = host_run(lib, "tpw_l7_database", case["d"], case.get("q"))
    return dict(state=state, log=log)


# ------------------------------------------------------------------------------------------------ StripNodeWrapping
# description (all numbers): V, F, S, P, amount bits, data set, point set, counter, 19 characters of the header's name, V u bits, V v bits,
# F faces of 3 vertices, S face sets (first face, face count, flags: a ushort, -1 for no flag word, -2 the previous set's word), V
# vertex-to-point entries, P points of 3 bits, 1024 bytes of map (the data pointer is at byte 512)


def strip_ppc(e, case):
    p = Ppc(e, case)
    try:
        L = p.L
        d = list(case["d"])
        V, F, S, P, amount_bits, data_set, point_set, counter0 = d[:8]
        pos = 8
        name = bytes(d[pos:pos + 19]) + b"\0"
        pos += 19
        uv_bits = d[pos:pos + 2 * V]
        pos += 2 * V
        faces = [d[pos + 3 * i:pos + 3 * i + 3] for i in range(F)]
        pos += 3 * F
        sets = [d[pos + 3 * i:pos + 3 * i + 3] for i in range(S)]
        pos += 3 * S
        table = d[pos:pos + V]
        pos += V
        points = [d[pos + 3 * i:pos + 3 * i + 3] for i in range(P)]
        pos += 3 * P
        map_bytes = bytes(d[pos:pos + 1024])
        header = e.alloc(0x100, pattern(0x100, 0x66))
        e.write(header + 0x18, name)
        node = e.alloc(0xA0, pattern(0xA0, 0x77))
        uvs = e.alloc(0x20 * (V // 4 + 1), pattern(0x20 * (V // 4 + 1), 0x11))
        face_block = e.alloc(8 * (F + 1), pattern(8 * (F + 1), 0x22))
        set_block = e.alloc(0x10 * (S + 1), pattern(0x10 * (S + 1), 0x33))
        table_block = e.alloc(2 * (V + 1), pattern(2 * (V + 1), 0x44))
        point_block = e.alloc(12 * (P + 1), pattern(12 * (P + 1), 0x55))
        map_block = e.alloc(1024, map_bytes)
        textures = [e.alloc(8, pattern(8, 0x99)) for _ in range(S + 1)]
        counter = e.alloc(4)
        e.write(node + 0x5A, struct.pack(">HHH", S, F, V))
        ws(e, node + 0x68, [uvs, set_block, face_block])
        ws(e, node + 0x94, [table_block])
        for i in range(V):
            ws(e, uvs + (i >> 2) * 0x20 + (i & 3) * 4, [uv_bits[i]])
            ws(e, uvs + (i >> 2) * 0x20 + (i & 3) * 4 + 0x10, [uv_bits[V + i]])
        for i, face in enumerate(faces):
            ws(e, face_block + 8 * i, [0])
            e.write(face_block + 8 * i + 2, struct.pack(">HHH", *face))
        flag_words = []
        for i, (first, count, flags) in enumerate(sets):
            if flags == -2 and i > 0:
                pointer = e.u32(set_block + 0x10 * (i - 1))
            elif flags >= 0:
                pointer = textures[i]
                e.write(pointer, struct.pack(">H", flags))
            else:
                pointer = 0
            ws(e, set_block + 0x10 * i, [pointer])
            e.write(set_block + 0x10 * i + 4, struct.pack(">HH", first, count))
            flag_words.append(pointer)
        if V:
            e.write(table_block, struct.pack(">" + "H" * V, *table))
        for i, point in enumerate(points):
            ws(e, point_block + 12 * i, point)
        ws(e, counter, [counter0])
        for address, label in ((header, 1), (node, 2), (map_block + 512, 3), (point_block, 4)):
            L.labels[address] = label
        snapshot = [face_block, 8 * (F + 1), table_block, 2 * (V + 1), point_block, 12 * (P + 1), map_block, 1024, set_block, 0x10 * (S + 1)]
        before = b"".join(e.read(snapshot[i], snapshot[i + 1]) for i in range(0, len(snapshot), 2))
        p.call(STRIP_NODE, header, node, T.F(from_bits(amount_bits)), map_block + 512 if data_set else 0, point_block if point_set else 0, counter)
        state = []
        for i in range(V):
            state.append(canon(e.u32(uvs + (i >> 2) * 0x20 + (i & 3) * 4)))
            state.append(canon(e.u32(uvs + (i >> 2) * 0x20 + (i & 3) * 4 + 0x10)))
        for i in range(S):
            pointer = e.u32(set_block + 0x10 * i)
            state.append(struct.unpack(">H", e.read(pointer, 2))[0] if pointer else 0xFFFFFFFF)
        state.append(T.s32(e.u32(counter)))
        after = b"".join(e.read(snapshot[i], snapshot[i + 1]) for i in range(0, len(snapshot), 2))
        state.append(int(before == after))
        return dict(state=state, log=L.log)
    finally:
        p.done()


strip_host = host_only("tpw_l7_strip")


def region_values(amount):
    """Texture coordinates whose fractions lie in every region of pass 1 (and on the boundaries) for the given amount."""
    a = amount if math.isfinite(amount) and 0 < amount < 1 else 0.02
    g = f32(1.0 - a)
    mags = [0.0, 1e-6, a * 0.5, a * 0.99, a, a * 1.01, 0.3, 0.5, 0.7, g * 0.99, g, g * 1.01, 0.999, 0.9999999]
    values = []
    for n in (0, 1, 2, 3):
        for m in mags:
            for sign in (1.0, -1.0):
                values.append(bits(f32(sign * (n + m))))
    return sorted(set(values))


def finite_bits(r, low, high):
    return bits(f32(r.uniform(low, high)))


def strip_case(r, V=None, F=None, S=None, P=None, amount=None, data_set=None, point_set=None, counter=None, uv=None, faces=None,
               sets=None, table=None, points=None, map_bytes=None, name=None, aimed=None):
    """aimed: None for a mixed case, True for one aimed at the data map (centroids inside the map, near zero height)."""
    V = r.choice([0, 1, 3, 4, 5, 8, 12, 16]) if V is None else V
    F = (r.randint(0, 10) if V > 0 else 0) if F is None else F
    S = r.randint(0, 4) if S is None else S
    P = r.randint(1, 6) if P is None else P
    aimed = r.random() < 0.6 if aimed is None else aimed
    if amount is None:
        amount = r.choice([0x3CA3D70A, 0x3CA3D70A, 0x3DCCCCCD, 0x3F000000, 0x3E800000, 0x3F400000, 0, 0x80000000, 0x3F800000, 0x3F666666,
                           0x358637BD, 0x3FC00000, 0xBE800000, 0x7FC00000, 0x7F800000, bits(f32(r.random()))])
    a = from_bits(amount)
    if uv is None:
        pool = region_values(a)
        uv = [r.choice(pool) if r.random() < 0.7 else finite_bits(r, -4, 4) for _ in range(2 * V)]
        for i in range(len(uv)):
            roll = r.random()
            if roll < 0.04:
                uv[i] = r.choice([0x7FC00000, 0x7F800000, 0xFF800000, 0x7F7FFFFF, 0xFF7FFFFF, 0x80000000, 0x00000001, 0x3F800000, 0x00000000])
            elif roll < 0.3 and aimed:
                uv[i] = bits(f32(r.choice([0.0, 0.25, 0.5, 0.75, 1.0, 1.0000001, -0.0000001, 0.9999999, r.random()])))
    if faces is None:
        faces = [[r.randrange(V) for _ in range(3)] for _ in range(F)] if V else []
    if sets is None:
        sets = []
        for i in range(S):
            first = r.randint(0, F)
            count = r.randint(0, F - first)
            flags = r.choice([0, 0x30, 0x8000, 0x8030, 0xFFFF, 0xFFCF, 0x0010, 0x0020, 0x1234, -1, -1, -2, 0x7FFF, r.getrandbits(16)])
            sets.append([first, count, flags])
    if table is None:
        table = [r.randrange(P) for _ in range(V)]
    if points is None:
        points = []
        for _ in range(P):
            if aimed:
                y = r.choice([0.0, 0.0, 0.05, -0.05, 0.099, -0.099, 0.1, -0.1, 0.2, -0.3, 7.0, bits_to_float(r)])
                points.append([bits(f32(r.choice([r.uniform(0, 150), 0.0, 10.0, 9.999999, 20.0, 150.0, 155.0, -5.0, -0.5, r.uniform(-15, 160)]))),
                               bits(f32(y)),
                               bits(f32(r.choice([r.uniform(0, 150), 0.0, 10.0, 9.999999, 20.0, 150.0, 155.0, -5.0, -0.5, r.uniform(-15, 160)])))])
            else:
                points.append([bits(f32(r.uniform(-200, 200))), bits(f32(r.uniform(-1, 1))), bits(f32(r.uniform(-200, 200)))])
    if map_bytes is None:
        density = r.choice([0.0, 0.3, 0.5, 0.9, 1.0])
        map_bytes = [r.choice([1, 0xFF, 0x80, 7, r.getrandbits(8) | 1]) if r.random() < density else 0 for _ in range(1024)]
    name = "".join(r.choice("abcdefgh_ ") for _ in range(r.randint(0, 18))) if name is None else name
    data = [V, F, S, P, amount, r.randint(0, 1) if data_set is None else data_set, r.randint(0, 1) if point_set is None else point_set,
            r.choice([0, 0, 0, 1, 5, -1]) if counter is None else counter]
    data += [ord(c) for c in name] + [0] * (19 - len(name))
    data += list(uv)
    for face in faces:
        data += list(face)
    for s in sets:
        data += list(s)
    data += list(table)
    for point in points:
        data += list(point)
    data += list(map_bytes)
    return dict(d=data)


def bits_to_float(r):
    return from_bits(r.choice([0x3DCCCCCD, 0xBDCCCCCD, 0x3DCCCCCC, 0xBDCCCCCC, 0x3DCCCCCE, 0xBDCCCCCE, 0x3DCCCCCB, 0xBDCCCCCB]))


def edge_strip():
    r = random.Random(8103)
    # Pass 1 alone (no faces): fractions in every region, for a range of amounts.
    for amount in (0x3CA3D70A, 0, 0x3F800000, 0x3F000000, 0x3E800000, 0x3F400000, 0x3DCCCCCD, 0x3F666666, 0x358637BD, 0x3FC00000, 0xBE800000,
                   0x7FC00000, 0x7F800000):
        a = from_bits(amount)
        pool = region_values(a)
        for start in range(0, len(pool), 16):
            chunk = pool[start:start + 16]
            if len(chunk) < 16:
                chunk = chunk + pool[:16 - len(chunk)]
            shuffled = chunk[5:] + chunk[:5]
            yield strip_case(r, 16, 0, 0, 1, amount, 0, 0, 0, uv=chunk + shuffled, faces=[], sets=[])
    # Faces: every corner shared (visited), the lowest corner negative, huge, infinite.
    for lows in ([-0.3, 0.2, 0.9], [-1.5, 2.5, 3.0], [0.0, 0.0, 0.0], [-2.0, -2.0, -0.0], [5e30, 1e30, 3e38], [-1e30, 2.0, 1.0]):
        u = [bits(f32(x)) for x in lows] + [bits(f32(0.5))]
        v = [bits(f32(x)) for x in reversed(lows)] + [bits(f32(-0.5))]
        for faces in ([[0, 1, 2]], [[0, 1, 2], [1, 2, 3]], [[3, 3, 3], [0, 0, 1]], [[0, 1, 2], [0, 1, 2]]):
            yield strip_case(r, 4, len(faces), 1, 1, 0x3CA3D70A, 0, 0, 0, uv=u + v, faces=faces, sets=[[0, len(faces), 0x30]])
    for u in ([0x7F800000, 0xFF800000, 0x7FC00000, 0x3F800000], [0x7F7FFFFF, 0x80000000, 0x00000001, 0xFF7FFFFF]):
        v = list(reversed(u))
        yield strip_case(r, 4, 2, 1, 1, 0x3CA3D70A, 0, 0, 0, uv=u + v, faces=[[0, 1, 2], [1, 2, 3]], sets=[[0, 2, 0x30]])
    # The data map: with and without the data and the point pointers, counter 0 and not 0, aimed and mixed, for every layout of sets.
    for data_set in (0, 1):
        for point_set in (0, 1):
            for counter in (0, 1):
                for aimed in (True, False, True):
                    yield strip_case(r, r.choice([4, 8, 12]), r.randint(2, 10), r.randint(1, 4), None, 0x3CA3D70A, data_set, point_set, counter,
                                     aimed=aimed)
    # Face sets alone: first/count at the ends, empty sets, the same flag word twice.
    for sets in ([[0, 0, 0x30]], [[0, 3, 0x8030]], [[2, 1, 0xFFFF], [0, 0, -2]], [[0, 3, 0x30], [0, 0, -2], [1, 2, -2]], [[1, 2, -1], [0, 3, 0x10]]):
        for data_set, point_set in ((0, 0), (1, 1), (1, 0)):
            yield strip_case(r, 4, 3, len(sets), 3, 0x3CA3D70A, data_set, point_set, 0, sets=sets, faces=[[0, 1, 2], [1, 2, 3], [0, 2, 3]],
                             aimed=True)
    # Boundary heights of the centroid (+-0.1 exactly and one unit either side) and boundary cells (x or z a multiple of 10).
    for y_bits in (0x3DCCCCCD, 0xBDCCCCCD, 0x3DCCCCCC, 0xBDCCCCCC, 0x3DCCCCCE, 0xBDCCCCCE, 0x00000000, 0x80000000):
        for x in (0.0, 10.0, 9.999999, 20.0, -0.5, -10.0, 150.0, 159.99998):
            point = [bits(f32(x)), y_bits, bits(f32(x))]
            map_bytes = [r.choice([0, 1]) for _ in range(1024)]
            yield strip_case(r, 4, 1, 1, 1, 0x3CA3D70A, 1, 1, 0, uv=[bits(f32(2.5)), bits(f32(0.5)), bits(f32(-0.25)), bits(f32(0.0))] * 2,
                             faces=[[0, 1, 2]], sets=[[0, 1, 0x30]], table=[0, 0, 0, 0], points=[point], map_bytes=map_bytes)


def centroid(a, b, c, variant):
    """The centroid coordinate of three single-precision values: variant 0 is the original's ((a + b) + c) / 3, 1 sums the other way round,
    2 multiplies by a single-precision third."""
    if variant == 0:
        return f32(f32(f32(a + b) + c) / 3.0)
    if variant == 1:
        return f32(f32(a + f32(b + c)) / 3.0)
    return f32(f32(f32(a + b) + c) * f32(1.0 / 3.0))


def cell_of(value):
    return int(f32(value / 10.0))


def boundary_triples(kind, r, wanted):
    """Triples of single-precision values around a map-cell edge (kind 'cell') or around +-0.1 (kind 'height') whose centroid lands on a
    different side of the edge when the sum is associated the other way (variant 1) or the division is a multiplication (variant 2)."""
    found = {1: [], 2: []}
    for _ in range(200000):
        if kind == "cell":
            base = f32(10.0 * r.randint(1, 15))
        else:
            base = f32(r.choice([0.1, -0.1]))
        spread = 6 if kind == "cell" else 40
        triple = [from_bits(bits(base) + r.randint(-spread, spread)) if bits(base) < 0x80000000 else from_bits(bits(base) - r.randint(-spread, spread)) for _ in range(3)]
        for variant in (1, 2):
            if len(found[variant]) >= wanted:
                continue
            first, other = centroid(*triple, 0), centroid(*triple, variant)
            if kind == "cell":
                differs = cell_of(first) != cell_of(other)
            else:
                differs = (-0.1 < first < 0.1) != (-0.1 < other < 0.1)
            if differs:
                found[variant].append(triple)
        if all(len(v) >= wanted for v in found.values()):
            break
    return found


def edge_strip_rounding():
    """Faces whose centroid is on the edge of a map cell or of the height window, where the order of the three additions and the division
    by 3 decide the outcome."""
    r = random.Random(8107)
    uv = [bits(2.5), bits(0.5), bits(-0.25), bits(0.0)] * 2
    for kind in ("cell", "height"):
        for variant, triples in boundary_triples(kind, r, 4).items():
            for triple in triples:
                for axis in ((0, 2) if kind == "cell" else (1,)):
                    points = [[bits(0.0), bits(0.0), bits(0.0)] for _ in range(3)]
                    for i in range(3):
                        points[i][axis] = bits(triple[i])
                    # The map is non-zero at the cell the original reaches only (so a different cell or window shows in the flag word).
                    cx, cz = [cell_of(centroid(*[from_bits(points[i][a]) for i in range(3)], 0)) if a in (0, 2) else 0 for a in (0, 2)]
                    map_bytes = [0] * 1024
                    map_bytes[512 + cx * 16 + cz] = 1
                    for flags in (0x30,):
                        yield strip_case(r, 4, 1, 1, 3, 0x3CA3D70A, 1, 1, 0, uv=uv, faces=[[0, 1, 2]], sets=[[0, 1, flags]],
                                         table=[0, 1, 2, 0], points=points, map_bytes=map_bytes)


def edge_strip_extremes():
    """Minima that are the largest single values, zeros of both signs, coordinates exactly at the edges of the unit square."""
    r = random.Random(8108)
    big = [bits(5e30), bits(3e38), bits(2e31), bits(3e30)]
    yield strip_case(r, 4, 2, 1, 1, 0x3CA3D70A, 0, 0, 0, uv=big + list(reversed(big)), faces=[[0, 1, 2], [2, 3, 0]], sets=[[0, 2, 0x30]])
    yield strip_case(r, 4, 1, 1, 1, 0, 0, 0, 0, uv=[bits(0.0), bits(-0.0), bits(0.5), bits(0.25)] * 2, faces=[[0, 1, 2]],
                     sets=[[0, 1, 0x30]])
    yield strip_case(r, 4, 1, 1, 1, 0, 0, 0, 0, uv=[bits(-0.0), bits(0.0), bits(0.5), bits(0.25)] * 2, faces=[[0, 1, 2]],
                     sets=[[0, 1, 0x30]])
    # u and v land exactly on 0 and 1 (the unit square is closed): with the map reached (a hit) and without a map; counters 0 and 1.
    for data_set, point_set in ((1, 1), (0, 0), (1, 0)):
        for u, v in (([0.0, 1.0, 0.5, 0.5], [0.0, 0.0, 0.0, 0.0]), ([0.0, 0.0, 0.0, 0.5], [0.0, 1.0, 0.5, 0.5]),
                     ([0.0, 1.0, 1.0, 0.0], [0.0, 0.0, 1.0, 1.0]), ([0.0, 1.0000001, 0.5, 0.5], [0.0, 0.0, 0.0, 0.0]),
                     ([0.0, 0.0, 0.0, 0.5], [0.0, 1.0000001, 0.5, 0.5]), ([0.0, -0.0000001, 0.5, 0.5], [0.0, 0.0, 0.0, 0.0])):
            for counter in (0, 1):
                yield strip_case(r, 4, 1, 1, 1, 0, data_set, point_set, counter, uv=[bits(x) for x in u] + [bits(x) for x in v],
                                 faces=[[0, 1, 2]], sets=[[0, 1, 0x30]], table=[0, 0, 0, 0], points=[[bits(5.0), bits(0.0), bits(5.0)]],
                                 map_bytes=[1] * 1024)


def random_strip(r):
    return strip_case(r)


# ------------------------------------------------------------------------------------------------ activateUI
# description: the active argument, the window count, the parent of each window but the first (the call starts at window 0)
# queues: Relink (per posted message: 0 nothing, 1 the window's next is cleared, 2 skipped, 3 its child is cleared)


def activate_ppc(e, case):
    p = Ppc(e, case)
    try:
        L = p.L
        active, count = case["d"][:2]
        parents = case["d"][2:]
        windows = [e.alloc(16, pattern(16, 0x70 + i)) for i in range(count)]
        index = {address: i for i, address in enumerate(windows)}
        child, nxt = {0: 0}, {0: 0}
        for address in windows:
            child[address] = nxt[address] = 0
        last = {}
        for i in range(1, count):
            parent = parents[i - 1]
            if parent not in last:
                child[windows[parent]] = windows[i]
            else:
                nxt[windows[last[parent]]] = windows[i]
            last[parent] = i
        p.child, p.next = child, nxt
        for i, address in enumerate(windows):
            L.labels[address] = 100 + i
        p.call(ACTIVATE_UI, windows[0], active & MASK)
        state = []
        for address in windows:
            state.append(index[child[address]] if child[address] else -1)
            state.append(index[nxt[address]] if nxt[address] else -1)
        return dict(state=state, log=L.log)
    finally:
        p.done()


def activate_host(lib, case):
    state, log = host_run(lib, "tpw_l7_activate", case["d"], case.get("q"))
    return dict(state=state, log=log)


def activate_case(r, parents=None, count=None, active=None, relink=None):
    count = r.choice([1, 2, 3, 6, 9, 14, 24, 40]) if count is None else count
    if parents is None:
        shape = r.choice(["random", "random", "deep", "wide"])
        if shape == "deep":
            parents = [i for i in range(count - 1)]
        elif shape == "wide":
            parents = [0] * (count - 1)
        else:
            parents = [r.randrange(i + 1) for i in range(count - 1)]
    count = len(parents) + 1
    active = r.choice([0, 1, -1, 2, 0x100, -0x80000000, 0x7FFFFFFF, 0x10000]) if active is None else active
    relink = [r.choice([0, 0, 0, 0, 0, 1, 2, 3]) for _ in range(4 * count)] if relink is None else relink
    return dict(d=[active, count] + list(parents), q={"Relink": relink})


def edge_activate():
    r = random.Random(8104)
    for active in (0, 1, -1, 2, 0x100, -0x80000000, 0x7FFFFFFF):
        yield activate_case(r, [], active=active, relink=[0])
        yield activate_case(r, [0, 0, 1, 2, 3, 3], active=active, relink=[0] * 8)
    # Chains of every depth around the six levels the Mac unrolls (the seventh level recurses), and a complete tree
    for depth in range(1, 15):
        yield activate_case(r, list(range(depth)), active=1, relink=[0])
    yield activate_case(r, [0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7], active=1, relink=[0])
    # A window with children at every level of a seven-deep chain with siblings
    yield activate_case(r, [0, 1, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 3, 2, 1, 0], active=0, relink=[0])
    # A posted message unlinks things: the next of window k (read after its children), its child, or the next but one
    for action in (1, 2, 3):
        for position in range(0, 6):
            relink = [action if i == position else 0 for i in range(8)]
            yield activate_case(r, [0, 0, 0, 1, 1, 2, 3], active=1, relink=relink)
            yield activate_case(r, [0, 1, 2, 3, 4, 5, 6, 7], active=1, relink=relink)


def random_activate(r):
    return activate_case(r)


# ------------------------------------------------------------------------------------------------ UIInterface::Pause


def pause_ppc(e, case):
    p = Ppc(e, case)
    try:
        this = e.alloc(16, pattern(16, case["fill"]))
        p.call(UI_PAUSE, this)
        return dict(state=[1], log=p.L.log)
    finally:
        p.done()


pause_host = host_only("tpw_l7_pause")


def edge_pause():
    return [dict(d=[0], fill=0), dict(d=[0], fill=0xFF)]


def random_pause(r):
    return dict(d=[0], fill=r.getrandbits(8))


# ------------------------------------------------------------------------------------------------ CWorld::ExitLevel
# description: the mouse (0 none, 1 one), gGameWorld (0 none, 1 the world the call is on, 2 another), the fill byte
# queues: MouseAction (1 gGameWorld becomes the other world, 2 it is cleared; after AssignMouseMode)


def exit_ppc(e, case):
    p = Ppc(e, case)
    try:
        L = p.L
        mouse, global_world, fill = case["d"]
        # The Mac only touches this + 0x1da738: the block holds the flag word and is placed so that the world's address is below it.
        block = e.alloc(0x100, pattern(0x100, fill))
        world = block + 0x40 - FLAG_OFFSET
        other_block = e.alloc(0x100, pattern(0x100, 0x5A))
        other = other_block + 0x40 - FLAG_OFFSET
        ws(e, world + FLAG_OFFSET, [7])
        ws(e, other + FLAG_OFFSET, [9])
        p.world, p.other_world = world, other
        mouse_address = e.alloc(16, pattern(16, 0x33)) if mouse else 0
        ws(e, G_MOUSE, [mouse_address])
        ws(e, G_WORLD, [0 if global_world == 0 else world if global_world == 1 else other])
        L.labels[world], L.labels[other] = 1, 2
        if mouse_address:
            L.labels[mouse_address] = 3
        skip = world + FLAG_OFFSET
        before = e.read(block, 0x100)
        p.call(WORLD_EXIT, world)
        after = e.read(block, 0x100)
        flag = skip - block
        state = [L.lab(e.u32(G_MOUSE)), L.lab(e.u32(G_WORLD)), T.s32(e.u32(world + FLAG_OFFSET)), int(mouse_address != 0 and L.lab(mouse_address) > 0),
                 int(before[:flag] + before[flag + 4:] == after[:flag] + after[flag + 4:])]
        return dict(state=state, log=L.log)
    finally:
        p.done()


exit_host = host_only("tpw_l7_exit")


def exit_case(r, mouse=None, global_world=None, action=None):
    mouse = r.randint(0, 1) if mouse is None else mouse
    global_world = r.choice([0, 1, 2, 2]) if global_world is None else global_world
    action = r.choice([0, 0, 1, 2]) if action is None else action
    return dict(d=[mouse, global_world, r.randint(0, 255)], q={"MouseAction": [action]})


def edge_exit():
    r = random.Random(8105)
    for mouse in (0, 1):
        for global_world in (0, 1, 2):
            for action in (0, 1, 2):
                yield exit_case(r, mouse, global_world, action)


def random_exit(r):
    return exit_case(r)


# ------------------------------------------------------------------------------------------------ CWorld::Free, CWorld::FreeThingHeap
# description: the operation (0 Free, 1 FreeThingHeap), the fill byte


def free_ppc(e, case):
    op, fill = case["d"]
    p = Ppc(e, case, STUBS_HEAP if op == 0 else STUBS)
    try:
        world = e.alloc(0x100, pattern(0x100, fill))
        before = e.read(world, 0x100)
        p.call(WORLD_FREE if op == 0 else FREE_HEAP, world)
        called = int(p.heap_calls == [world]) if op == 0 else 1
        return dict(state=[int(e.read(world, 0x100) == before), called], log=p.L.log)
    finally:
        p.done()


free_host = host_only("tpw_l7_free")


def edge_free(op):
    return lambda: [dict(d=[op, fill]) for fill in (0, 0xA5, 0xFF)]


def random_free(op):
    return lambda r: dict(d=[op, r.randint(0, 255)])


# ------------------------------------------------------------------------------------------------ CStaffDB::~CStaffDB
# description: the flag (r4)


def staff_ppc(e, case):
    p = Ppc(e, case)
    try:
        L = p.L
        flag = case["d"][0]
        block = p.staff_base = e.alloc(0x280, pattern(0x280, 0xA5))
        L.labels[block] = 1
        answer = p.call(STAFF_DTOR, block, flag & MASK)
        alive = int(L.lab(block) > 0)
        # A NULL object: nothing happens (no destructor, no free) and the answer is NULL.
        events = len(L.log)
        null_answer = p.call(STAFF_DTOR, 0, 1)
        ok = int(answer == block and null_answer == 0 and len(L.log) == events)
        return dict(state=[alive, ok], log=L.log[:events])
    finally:
        p.done()


staff_host = host_only("tpw_l7_staff")


def edge_staff():
    for flag in (1, 0, -1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x7FFFFFFF, 0x80000000, 0xFFFF0001, 0xFFFF8000, 0x12345678):
        yield dict(d=[T.s32(flag & MASK)])


def random_staff(r):
    return dict(d=[T.s32(r.choice([1, 0, MASK, r.getrandbits(32), r.getrandbits(16), r.getrandbits(32) & 0xFFFF0000, r.getrandbits(32) | 0x8000,
                                   (r.getrandbits(16) << 16) | 1]))])


# ------------------------------------------------------------------------------------------------ CSkyObjects::DestroySkyObjects
# description: the count (the balance word at 0x6e4), the number of objects built, the fill byte
# queues: Count (after each RemoveAdd, the count becomes this; -9999 leaves it), Replace (after each RemoveAdd, this slot is given the other object)


def sky_ppc(e, case):
    p = Ppc(e, case)
    try:
        L = p.L
        count, objects, fill = case["d"]
        block = p.objects = e.alloc(4 * 16, pattern(4 * 16, fill))
        things = [e.alloc(8, pattern(8, 0x80 + i)) for i in range(objects)]
        other = p.other_object = e.alloc(8, pattern(8, 0x6F))
        for i, address in enumerate(things):
            ws(e, block + 4 * i, [address])
            L.labels[address] = 30 + i
        L.labels[block], L.labels[e.data_import("gei_Sky")], L.labels[other] = 1, 2, 90
        ws(e, NUM_SKY_OBJECTS, [count])
        p.call(SKY_DESTROY, block)
        state = [T.s32(e.u32(NUM_SKY_OBJECTS))]
        state += [L.lab(e.u32(block + 4 * i)) if i < objects else 0 for i in range(16)]
        return dict(state=state, log=L.log)
    finally:
        p.done()


sky_host = host_only("tpw_l7_sky")


def sky_case(r, count=None, objects=None, counts=None, replace=None):
    objects = r.randint(0, 16) if objects is None else objects
    count = r.choice([0, 1, objects, objects, r.randint(-2, 16), -1, -0x80000000]) if count is None else count
    counts = [r.choice([-9999, -9999, -9999, 0, 1, 3, -1, 16]) for _ in range(16)] if counts is None else counts
    replace = [r.choice([-1, -1, -1, 0, 1, 2, 5, 15, 16]) for _ in range(16)] if replace is None else replace
    return dict(d=[count, objects, r.randint(0, 255)], q={"Count": counts, "Replace": replace})


def edge_sky():
    r = random.Random(8106)
    none = [-9999] * 16
    nothing = [-1] * 16
    for objects in (0, 1, 2, 5, 16):
        for count in (0, 1, 2, 5, 16, -1, -0x80000000):
            yield sky_case(r, count, objects, none, nothing)
    # the count changes after the first call (read again every time round), the next slot is replaced behind its back
    for after in (0, 1, 2, 3, -1, 16):
        yield sky_case(r, 5, 8, [after] + none, nothing)
        yield sky_case(r, 5, 8, [-9999, -9999, after] + none, nothing)
    for slot in (0, 1, 2, 4, 15):
        yield sky_case(r, 5, 8, none, [slot] + nothing)
        yield sky_case(r, 5, 8, none, [1, 2, slot] + nothing)


def random_sky(r):
    return sky_case(r)


TESTS = [
    FnTest("CGame::ClearThemeResources", theme_ppc, theme_host, edge=edge_theme, rnd=random_theme),
    FnTest("CRideDatabase::ClearDatabase", database_ppc, database_host, edge=edge_database, rnd=random_database),
    FnTest("StripNodeWrapping", strip_ppc, strip_host,
           edge=lambda: list(edge_strip()) + list(edge_strip_rounding()) + list(edge_strip_extremes()), rnd=random_strip),
    FnTest("activateUI", activate_ppc, activate_host, edge=edge_activate, rnd=random_activate),
    FnTest("UIInterface::Pause", pause_ppc, pause_host, edge=edge_pause, rnd=random_pause),
    FnTest("CWorld::ExitLevel", exit_ppc, exit_host, edge=edge_exit, rnd=random_exit),
    FnTest("CWorld::Free", free_ppc, free_host, edge=edge_free(0), rnd=random_free(0)),
    FnTest("CWorld::FreeThingHeap", free_ppc, free_host, edge=edge_free(1), rnd=random_free(1)),
    FnTest("CStaffDB::~CStaffDB", staff_ppc, staff_host, edge=edge_staff, rnd=random_staff),
    FnTest("CSkyObjects::DestroySkyObjects", sky_ppc, sky_host, edge=edge_sky, rnd=random_sky),
]
