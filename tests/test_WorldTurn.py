"""The world's turn and its thing lists (unit w121, main app), against libtpw_unit_WorldTurn.dylib (tests/unit_WorldTurn.cpp):

  src/World_turn.cpp   CWorld::DoGameTurn (0x1013b278), DoOnlineGameTurn (0x1013b608), UpdateThingsInEngine (0x1013de70),
                       UpdateBumperCarsInEngine (0x1013e09c), AddThingToThingList (0x1013b054), _CThingArrayEntry::FreeMe (0x1013af58),
                       DeleteAllThings (0x1013ac4c), DeletePendingThings (0x1013dda8), GetNextUsedThing (0x1013ae08), GetUsedThingHead
                       (0x1013adb0), IsParkClosed (0x1013ff7c), ParkGates_GetStatus (0x1013ffb4)

The twelve run as the original on the PowerPC side and as src/ on the host, and call each other for real. Every other callee is a fake that
records the same event on both sides (name, integers, texts): GAME_ASSERTMSG (with the world's state word), CTRACE (with its first %d),
the thing's GetThingId, Process and KillMe, the people's UpdatePositionInEngine and IsInsidePark, BMP_DrawCarsForRide, the park analyser's
visitors, CMacroAI::Update, the action recorder's DoTick, ParkGates_Close, the script pointer and variable, the pending list's RemoveHead,
the game type constructor and operator delete (a labelled block only).

A case describes the thing table once (the used rows in list order, the free rows, the things, per-row overrides of the index word and the
links), the world's four words, the pending list, the fakes' queues and the float; both sides are built from it. The world is raw memory of
the Mac's size over the fill 0xa5, the things are blocks of 0x80 bytes over 0xa5. What a case compares: the answer, the event log, the rows
of the thing table that changed (pointers as a block number or a row), both heads, the pending list's count and queue, the world's four
words and that every other byte of it still holds the fill, every thing block field and that its other bytes still hold the fill, and the
heap's delete count.
"""
import ctypes as C
import math
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_WorldTurn"

# name -> (the host's op number, the original's entry)
OPS = {
    "doTurn": (0, 0x1013B278), "online": (1, 0x1013B608), "update": (2, 0x1013DE70), "bumper": (3, 0x1013E09C),
    "add": (4, 0x1013B054), "freeMe": (5, 0x1013AF58), "deleteAll": (6, 0x1013AC4C), "deletePending": (7, 0x1013DDA8),
    "getNext": (8, 0x1013AE08), "getHead": (9, 0x1013ADB0), "closed": (10, 0x1013FF7C), "gates": (11, 0x1013FFB4),
}
OWN = {entry for _, entry in OPS.values()}

TABLE, ROWS, ROW_BYTES = 0x1034CBFC, 0x2800, 0x14
USED_HEAD, FREE_HEAD = 0x1034CBF4, 0x1034CBF8
LIST, LIST_COUNT = 0x1034CBD8, 0x1034CBE4
RECORDER = 0x1037EC44
GAME_TYPE, GAME_GUARD = 0x102B3AA0, 0x102B3AA4
WORLD_SIZE = 0x1DA748
W_TURN, W_CLOSED, W_GATES, W_STATE = 0x1DA70C, 0x1DA710, 0x1DA732, 0x1DA738
BLOCK = 0x80
MASK = 0xFFFFFFFF
L_WORLD, L_ANALYSER, L_SCRIPT, L_RECORDER, L_LIST, L_BLOCK = 1, 2, 3, 4, 5, 10

ENTRY_ZERO = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"
STAFF_TEXT = "!" * 94 + " Counting staff outside park\n"

FILL_WORLD = b"\xa5" * WORLD_SIZE
FILL_BLOCK = b"\xa5" * BLOCK
DEFAULT_TABLE = b"".join(struct.pack(">5I", 0, i, 0, 0, 0) for i in range(ROWS))
HOST_TABLE = (C.c_int64 * (6 * ROWS + 8))()


def setup(runner):
    T.install_std_stubs(runner.emu)


# ------------------------------------------------------------------ the case

STEPS = [0.0, 1.0, -1.0, 0.5, 1.401298464324817e-45, 3.4028234663852886e38, math.inf, -math.inf, math.nan, 123.456, -0.0]


def thing(kind=1, dead=0, id=None, w20=0, w24=0):
    """A thing block: the kind (+2), the dead flag (+3), its own id (None: its row), the words at +0x20 and +0x24."""
    return dict(kind=kind, dead=dead, id=id, w20=w20, w24=w24)


def make(op, **kw):
    """A case with every field defaulted. used: the rows of the used list, newest first; free: the rows of the free list; things: row ->
    thing (a used row without one holds a NULL thing); fix: row -> overrides (index, next, prev, nfree; None means NULL); heads: the
    two heads when they are not the lists' first rows (a row or None); arg: the thing passed to AddThingToThingList and
    GetNextUsedThing (None: NULL); row: the entry FreeMe is called on; pending: the ids RemoveHead answers; pcount: the list's
    count word (default: len(pending)); q: the fakes' queues; analyser/script: whether GetParkAnalyser and RSSE_GetScriptPointer
    answer a block; gtype/guard/ctor: the game type object."""
    c = dict(op=op, used=[], free=[], things={}, fix={}, heads=None, arg=None, row=0, world=1, turn=0, closed=0, gates=0, state=0,
             pending=[], pcount=None, q={}, step=1.0, analyser=1, script=1, gtype=0, guard=1, ctor=0)
    c.update(kw)
    return c


def listing(rows, **kw):
    """The used list `rows` (newest first), each row holding a thing of the kind given."""
    return dict(used=list(rows), things={r: thing(**kw) for r in rows})


def plan(c):
    """The blocks, the table rows and the heads of a case, in the shape both sides are built from."""
    specs, block_of = [], {}
    for r in sorted(c["things"]):
        block_of[r] = len(specs)
        specs.append(c["things"][r])
    arg_block = None
    if c["arg"] is not None:
        arg_block = len(specs)
        specs.append(c["arg"])
    used, free = c["used"], c["free"]
    rows = sorted(set(used) | set(free) | set(c["things"]) | set(c["fix"]) | {0})
    table = []

    def enc(row):
        return 0 if row is None else row + 1

    for r in rows:
        fx = c["fix"].get(r, {})
        nxt = prv = nfree = None
        if r in used:
            i = used.index(r)
            nxt = used[i + 1] if i + 1 < len(used) else None
            prv = used[i - 1] if i > 0 else None
        if r in free:
            i = free.index(r)
            nfree = free[i + 1] if i + 1 < len(free) else None
        nxt, prv, nfree = fx.get("next", nxt), fx.get("prev", prv), fx.get("nfree", nfree)
        table.append((r, block_of[r] + 1 if r in block_of else 0, fx.get("index", r) & MASK, enc(nxt), enc(prv), enc(nfree)))
    heads = c["heads"] if c["heads"] is not None else (used[0] if used else None, free[0] if free else None)
    return specs, arg_block, table, (enc(heads[0]), enc(heads[1]))


def thing_id(spec, row):
    return (row if spec["id"] is None else spec["id"]) & 0xFFFF


def spec_ids(c, specs):
    """The own id of each block: the row of a table thing, the spec's own id for the argument."""
    rows = sorted(c["things"])
    return [thing_id(spec, rows[k] if k < len(rows) else 0) for k, spec in enumerate(specs)]


def floatbits(x):
    return T.f32_bits(x)


# ------------------------------------------------------------------ the PowerPC side


class State:
    """What the fakes of one case share: the pending queue and the answers."""

    def __init__(self, c, world, analyser, script):
        self.queue = list(c["pending"])
        self.pos = 0
        self.world = world
        self.analyser = analyser
        self.script = script
        self.ctor = c["ctor"]
        self.deletes = 0


def stubs(e, L, st):
    S = CL.Stubs()
    S.real.update(OWN)
    world = st.world

    def state():
        return T.s32(e.u32(world + W_STATE))

    def turn():
        return e.u32(world + W_TURN)

    def text(a):
        return e.cstr(a)[:159] if a else "(null)"

    S.internal(0x1000DDE8, lambda e: L.rec("GAME_ASSERTMSG", (T.s32(e.r(3)), state()), (text(e.r(4)),)))

    def ctrace(e):
        fmt = text(e.r(4))
        L.rec("CTRACE", (e.r(3), e.r(5) if "%d" in fmt else 0), (fmt,))

    S.internal(0x1006FCBC, ctrace)

    def get_id(e):
        L.rec("GetThingId", (L.lab(e.r(4)),))
        e.write(e.r(3), e.read(e.r(4), 2) if e.r(4) else b"\0\0")

    S.internal(0x1012E1F8, get_id)
    S.internal(0x1012E230, lambda e: L.rec("Process", (L.lab(e.r(3)), turn())))

    def kill_me(e):
        L.rec("KillMe", (L.lab(e.r(3)), state()))
        if L.ret("killmode", 0) == 1:
            thing_addr = e.r(3)
            e.write(thing_addr + 2, b"\x02\x01")
            st.queue.insert(st.pos, struct.unpack(">H", e.read(thing_addr, 2))[0])
            e.write(LIST_COUNT, struct.pack(">I", (e.u32(LIST_COUNT) + 1) & MASK))

    S.internal(0x1012E728, kill_me)
    S.internal(0x101149CC, lambda e: L.rec("UpdatePositionInEngine", (L.lab(e.r(3)), CL.fbits(e.f(1)))))

    def inside(e):
        L.rec("IsInsidePark", (L.lab(e.r(3)),))
        e.set_r(3, T.u32(L.ret("inside", 1)))

    S.internal(0x10115A80, inside)
    S.internal(0x10028030, lambda e: L.rec("BMP_DrawCarsForRide", (e.r(3), CL.fbits(e.f(1)))))

    def visitors(e):
        L.rec("NumberOfVisitorsInPark", (L.lab(e.r(3)),))
        e.set_r(3, T.u32(L.ret("visitors", 0)))

    S.internal(0x100E7278, visitors)

    def get_analyser(e):
        L.rec("GetParkAnalyser", (L.lab(e.r(3)),))
        e.set_r(3, st.analyser)

    S.internal(0x1013EBC8, get_analyser)
    S.internal(0x101009DC, lambda e: L.rec("CMacroAI::Update", (L.lab(e.r(3)), turn())))
    S.internal(0x10144F80, lambda e: L.rec("DoTick", (L.lab(e.r(3)),)))
    S.internal(0x1014013C, lambda e: L.rec("ParkGates_Close", (L.lab(e.r(3)), T.s32(e.r(4)))))

    def script_pointer(e):
        L.rec("RSSE_GetScriptPointer", (e.r(3),))
        e.set_r(3, st.script)

    S.internal(0x100D668C, script_pointer)

    def getvar(e):
        L.rec("RSSE_Getvar", (L.lab(e.r(3)), T.s32(e.r(4))))
        e.set_r(3, T.u32(L.ret("var", 1)))

    S.internal(0x100D6C58, getvar)

    def remove_head(e):
        count = e.u32(LIST_COUNT)
        L.rec("RemoveHead", (L.lab(e.r(4)), T.s32(count)))
        e.write(LIST_COUNT, struct.pack(">I", (count - 1) & MASK))
        if st.pos < len(st.queue):
            ident = st.queue[st.pos]
            st.pos += 1
        else:
            ident = 0xFFFF
        e.write(e.r(3), struct.pack(">H", ident))

    S.internal(0x101414A4, remove_head)
    S.internal(0x1016F5D8, lambda e: e.write(e.r(3), struct.pack(">I", st.ctor & MASK)))

    def op_delete(e):
        p = e.r(3)
        if p:
            st.deletes += 1
            if L.lab(p) > 0:
                L.rec("delete", (L.lab(p),))
                L.forget(p)

    S.imp("__dl__FPv", op_delete)
    return S


def ppc_table_report(e, addr_of_block, initial):
    """The rows that differ from the table the case started with (pointers as a block number or a row + 1) and the two heads."""
    final = e.read(TABLE, ROWS * ROW_BYTES)
    rows = []

    def row_of(w):
        if w == 0:
            return 0
        off = w - TABLE
        return off // ROW_BYTES + 1 if 0 <= off < ROWS * ROW_BYTES and off % ROW_BYTES == 0 else -1

    for i in range(ROWS):
        a = final[i * ROW_BYTES:(i + 1) * ROW_BYTES]
        if a != initial[i * ROW_BYTES:(i + 1) * ROW_BYTES]:
            w = struct.unpack(">5I", a)
            rows.append((i, addr_of_block.get(w[0], -1) if w[0] else 0, w[1], row_of(w[2]), row_of(w[3]), row_of(w[4])))
    return rows, (row_of(e.u32(USED_HEAD)), row_of(e.u32(FREE_HEAD)))


def ppc(e, c):
    op = c["op"]
    code, entry = OPS[op]
    specs, arg_block, table, heads = plan(c)
    ids = spec_ids(c, specs)
    # the memory of the case
    world = e.alloc(WORLD_SIZE, FILL_WORLD)
    e.write(world + W_TURN, struct.pack(">I", c["turn"] & MASK))
    e.write(world + W_CLOSED, struct.pack(">I", c["closed"] & MASK))
    e.write(world + W_GATES, struct.pack(">H", c["gates"] & 0xFFFF))
    e.write(world + W_STATE, struct.pack(">I", c["state"] & MASK))
    analyser = e.alloc(0x40, b"\xa5" * 0x40) if c["analyser"] else 0
    script = e.alloc(0x40, b"\xa5" * 0x40) if c["script"] else 0
    blocks = []
    for k, spec in enumerate(specs):
        data = bytearray(FILL_BLOCK)
        struct.pack_into(">H", data, 0, ids[k])
        data[2], data[3] = spec["kind"] & 0xFF, spec["dead"] & 0xFF
        struct.pack_into(">II", data, 0x20, spec["w20"] & MASK, spec["w24"] & MASK)
        blocks.append(e.alloc(BLOCK, bytes(data)))
    block_addr = {a: k + 1 for k, a in enumerate(blocks)}
    data = bytearray(DEFAULT_TABLE)
    for r, tw, index, nxt, prv, nfree in table:
        struct.pack_into(">5I", data, r * ROW_BYTES, blocks[tw - 1] if tw else 0, index,
                         TABLE + (nxt - 1) * ROW_BYTES if nxt else 0, TABLE + (prv - 1) * ROW_BYTES if prv else 0,
                         TABLE + (nfree - 1) * ROW_BYTES if nfree else 0)
    initial = bytes(data)
    e.write(TABLE, initial)
    e.write(USED_HEAD, struct.pack(">I", TABLE + (heads[0] - 1) * ROW_BYTES if heads[0] else 0))
    e.write(FREE_HEAD, struct.pack(">I", TABLE + (heads[1] - 1) * ROW_BYTES if heads[1] else 0))
    count = len(c["pending"]) if c["pcount"] is None else c["pcount"]
    e.write(LIST_COUNT, struct.pack(">I", count & MASK))
    e.write(GAME_TYPE, struct.pack(">I", c["gtype"] & MASK))
    e.write(GAME_GUARD, bytes([c["guard"] & 0xFF]))
    out = e.alloc(16, b"\xee" * 16)
    fixed = {world: L_WORLD, analyser: L_ANALYSER, script: L_SCRIPT, RECORDER: L_RECORDER, LIST: L_LIST}
    for k, a in enumerate(blocks):
        fixed[a] = L_BLOCK + k
    L = CL.PpcLog(fixed, c["q"])
    e.cl = L
    st = State(c, world, analyser, script)
    S = stubs(e, L, st)
    saved = S.activate(e)
    result = {}
    try:
        w = world if c["world"] else 0
        arg = blocks[arg_block] if arg_block is not None else 0
        step = T.F(c["step"])
        args = {
            "doTurn": [world], "online": [world], "update": [world, step], "bumper": [world, step], "add": [out + 4, w, arg],
            "freeMe": [TABLE + c["row"] * ROW_BYTES], "deleteAll": [world], "deletePending": [world], "getNext": [out + 4, w, arg],
            "getHead": [out + 4, w], "closed": [world], "gates": [world],
        }[op]
        r3 = T.call_ppc(e, entry, args)[0]
        if op in ("add", "getNext", "getHead"):
            result["ret"] = struct.unpack(">H", e.read(out + 4, 2))[0]
            result["outguard"] = int(e.read(out, 4) == b"\xee" * 4 and e.read(out + 6, 10) == b"\xee" * 10)
        elif op in ("closed", "gates"):
            result["ret"] = r3
        if op == "online":
            result["guard"] = e.read(GAME_GUARD, 1)[0]
        rows, hd = ppc_table_report(e, block_addr, initial)
        result["table"] = rows
        result["heads"] = hd
        result["pending"] = (T.s32(e.u32(LIST_COUNT)), len(st.queue) - st.pos, st.queue[st.pos:])
        wbytes = bytearray(e.read(world, WORLD_SIZE))
        result["world"] = (struct.unpack(">I", wbytes[W_TURN:W_TURN + 4])[0], struct.unpack(">I", wbytes[W_CLOSED:W_CLOSED + 4])[0],
                           struct.unpack(">H", wbytes[W_GATES:W_GATES + 2])[0], struct.unpack(">I", wbytes[W_STATE:W_STATE + 4])[0])
        wbytes[W_TURN:W_TURN + 8] = b"\xa5" * 8
        wbytes[W_GATES:W_GATES + 2] = b"\xa5\xa5"
        wbytes[W_STATE:W_STATE + 4] = b"\xa5" * 4
        result["worldfill"] = int(bytes(wbytes) == FILL_WORLD)
        report = []
        for a in blocks:
            b = e.read(a, BLOCK)
            rest = b[4:0x20] + b[0x28:]
            report.append((struct.unpack(">H", b[:2])[0], b[2], b[3], struct.unpack(">I", b[0x20:0x24])[0],
                           struct.unpack(">I", b[0x24:0x28])[0], int(rest == b"\xa5" * len(rest))))
        result["things"] = report
        result["heap"] = st.deletes
        result["log"] = L.log
    finally:
        CL.Stubs.deactivate(e, saved)
    return result


# ------------------------------------------------------------------ the host side


def host(lib, c):
    op = c["op"]
    code, entry = OPS[op]
    specs, arg_block, table, heads = plan(c)
    ids = spec_ids(c, specs)
    lib.tpw_wt_begin.argtypes = [C.c_int32]
    lib.tpw_wt_ptr.argtypes = [C.c_int32]
    lib.tpw_wt_ptr.restype = C.c_void_p
    lib.tpw_wt_thing.argtypes = [C.c_int32] + [C.c_uint32] * 5
    lib.tpw_wt_table.argtypes = [C.POINTER(C.c_uint32), C.c_int32, C.c_uint32, C.c_uint32]
    lib.tpw_wt_get_table.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_wt_world.argtypes = [C.c_uint32] * 4
    lib.tpw_wt_get_world.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_wt_pending.argtypes = [C.c_int32, C.POINTER(C.c_uint16), C.c_int32]
    lib.tpw_wt_get_pending.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_wt_get_thing.argtypes = [C.c_int32, C.POINTER(C.c_uint32)]
    lib.tpw_wt_fakes.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_wt_gametype.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_wt_call.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_wt_call.restype = C.c_int64
    lib.tpw_wt_begin(len(specs))
    for k, spec in enumerate(specs):
        lib.tpw_wt_thing(k, ids[k], spec["kind"] & 0xFF, spec["dead"] & 0xFF, spec["w20"] & MASK, spec["w24"] & MASK)
    flat = [x for row in table for x in row]
    lib.tpw_wt_table((C.c_uint32 * max(len(flat), 1))(*flat), len(table), heads[0], heads[1])
    lib.tpw_wt_world(c["turn"] & MASK, c["closed"] & MASK, c["gates"] & 0xFFFF, c["state"] & MASK)
    count = len(c["pending"]) if c["pcount"] is None else c["pcount"]
    ident = c["pending"]
    lib.tpw_wt_pending(count, (C.c_uint16 * max(len(ident), 1))(*ident), len(ident))
    lib.tpw_wt_fakes(c["analyser"], c["script"])
    # the game type: the type the constructor makes when the Mac's guard byte is 0, else the case's
    lib.tpw_wt_gametype(T.s32(c["ctor"] if (c["guard"] & 0xFF) == 0 else c["gtype"]), T.s32(c["ctor"]))
    fixed = {L_WORLD: lib.tpw_wt_ptr(L_WORLD), L_ANALYSER: lib.tpw_wt_ptr(L_ANALYSER) if c["analyser"] else 0,
             L_SCRIPT: lib.tpw_wt_ptr(L_SCRIPT) if c["script"] else 0, L_RECORDER: lib.tpw_wt_ptr(L_RECORDER),
             L_LIST: lib.tpw_wt_ptr(L_LIST)}
    for k in range(len(specs)):
        fixed[L_BLOCK + k] = lib.tpw_wt_ptr(L_BLOCK + k)
    CL.host_begin(lib, fixed, c["q"])
    heap = (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")
    for i in range(4):
        heap[i] = 0
    thing_arg = arg_block + 1 if arg_block is not None else 0
    try:
        r = lib.tpw_wt_call(code, c["world"], thing_arg, c["row"], floatbits(c["step"]))
    finally:
        log = CL.host_end(lib)
    result = {}
    if op in ("add", "getNext", "getHead"):
        result["ret"] = r & 0xFFFF
        result["outguard"] = 1
    elif op in ("closed", "gates"):
        result["ret"] = r & MASK
    if op == "online":
        result["guard"] = 1 if (c["guard"] & 0xFF) == 0 else c["guard"] & 0xFF
    lib.tpw_wt_get_table(HOST_TABLE)
    n = HOST_TABLE[0]
    result["table"] = [tuple(HOST_TABLE[1 + 6 * j + i] for i in range(6)) for j in range(n)]
    result["heads"] = (HOST_TABLE[1 + 6 * n], HOST_TABLE[2 + 6 * n])
    pend = (C.c_int64 * 300)()
    lib.tpw_wt_get_pending(pend)
    result["pending"] = (T.s32(pend[0]), pend[1], [pend[2 + i] for i in range(pend[1])])
    w = (C.c_uint32 * 5)()
    lib.tpw_wt_get_world(w)
    result["world"] = (w[0], w[1], w[2], w[3])
    result["worldfill"] = w[4]
    report = []
    for k in range(len(specs)):
        t = (C.c_uint32 * 6)()
        lib.tpw_wt_get_thing(k, t)
        report.append(tuple(t))
    result["things"] = report
    result["heap"] = heap[2]
    result["log"] = log
    return result


# ------------------------------------------------------------------ the cases

IDX_WORDS = [0, 1, 0x27FF, 0xFFFF, 0x10000, 0x10003, 0x12345, 0x80000000, 0xFFFFFFFF]
ALL_KINDS = list(range(0, 0x15)) + [0x7F, 0x80, 0xFF]
STAFF_KINDS = [4, 5, 6, 7, 8]
WORDS = [0, 1, 2, 0x80000000, 0xFFFFFFFF, 0x7FFFFFFF, 0x12345678]
STATES = [0, 1, 2, 4, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 3]


def with_things(rows, kinds=None, **kw):
    """The used list `rows` (newest first) with a thing in each; kinds gives each row's kind (default 1)."""
    kinds = kinds or [1] * len(rows)
    return dict(used=list(rows), things={r: thing(kind=k, **kw) for r, k in zip(rows, kinds)})


def edges_get_head():
    yield make("getHead")
    yield make("getHead", world=0)
    for k in (1, 2, 0x27FF, 3):
        yield make("getHead", used=[k, 9])
        yield make("getHead", used=[k], world=0)
    for idx in IDX_WORDS:
        yield make("getHead", used=[5, 3, 2], fix={5: dict(index=idx)})
        yield make("getHead", used=[5], fix={5: dict(index=idx)}, world=0)
    yield make("getHead", used=[5, 3], free=[8, 9])
    yield make("getHead", used=[], free=[8, 9], heads=(None, 8))


def edges_get_next():
    for ident in (5, 3, 2, 0, 1, 0x27FF, 7):
        yield make("getNext", used=[5, 3, 2], arg=thing(id=ident))
        yield make("getNext", used=[5, 3, 2], arg=thing(id=ident), world=0)
    yield make("getNext", used=[5, 3, 2], arg=None)
    yield make("getNext", used=[5, 3, 2], arg=None, world=0)
    for idx in IDX_WORDS:
        yield make("getNext", used=[5, 3], fix={3: dict(index=idx)}, arg=thing(id=5))
    yield make("getNext", used=[0x27FF, 4], arg=thing(id=0x27FF))
    yield make("getNext", used=[0x27FF, 4], arg=thing(id=4))
    yield make("getNext", used=[5, 3], arg=thing(id=0x10005))
    yield make("getNext", used=[5], arg=thing(id=5, kind=3, dead=1, w20=7, w24=9))
    yield make("getNext", used=[5, 3], fix={5: dict(next=None)}, arg=thing(id=5))


def edges_add():
    yield make("add", free=[], used=[])
    yield make("add", free=[], used=[3, 4])
    yield make("add", free=[], used=[3, 4], world=0, arg=None)
    for n in (1, 2, 5):
        free = [7, 8, 9, 10, 11][:n]
        for used in ([], [3], [3, 4, 5]):
            yield make("add", free=free, used=used, arg=thing(id=6))
    yield make("add", free=[7, 8], used=[3, 4], arg=None)
    yield make("add", free=[7, 8], used=[3, 4], arg=thing(id=6), world=0)
    yield make("add", free=[7, 8], used=[3], fix={7: dict(prev=9)}, arg=thing(id=6))
    yield make("add", free=[7, 8], used=[3], fix={7: dict(prev=3, next=4)}, arg=thing(id=6))
    yield make("add", free=[7, 8], used=[], things={7: thing()}, arg=thing(id=6))
    yield make("add", free=[7, 8], used=[3], things={7: thing(), 3: thing()}, arg=thing(id=6))
    for idx in IDX_WORDS:
        yield make("add", free=[7, 8], used=[3], fix={7: dict(index=idx)}, arg=thing(id=6))
    yield make("add", free=[0, 1], used=[2], arg=thing(id=6))
    yield make("add", free=[0x27FF], used=[], arg=thing(id=6))
    yield make("add", free=[7, 8], used=[3], fix={8: dict(index=0x12345)}, heads=(3, 8), arg=thing(id=6))
    yield make("add", free=[7, 8], used=[3, 4], heads=(None, 7), arg=thing(id=6))


def edges_free_me():
    base = with_things([3, 5, 7])
    for row in (3, 5, 7):
        yield make("freeMe", row=row, **base)
    yield make("freeMe", row=4, **with_things([4]))
    yield make("freeMe", row=5, fix={5: dict(prev=None)}, **base)
    yield make("freeMe", row=5, fix={5: dict(next=None)}, **base)
    yield make("freeMe", row=7, fix={7: dict(prev=None)}, **base)
    yield make("freeMe", row=3, fix={3: dict(next=None)}, **base)
    yield make("freeMe", row=5, used=[3, 5, 7], things={3: thing(), 7: thing()})
    yield make("freeMe", row=3, used=[5, 7], things={5: thing(), 7: thing()})
    yield make("freeMe", row=9, used=[5], things={5: thing(), 9: thing()})
    yield make("freeMe", row=5, free=[9, 2], **base)
    yield make("freeMe", row=5, free=[], **base)
    yield make("freeMe", row=5, fix={5: dict(index=0x12345, nfree=9)}, **base)
    yield make("freeMe", row=0x27FF, **with_things([0x27FF, 5]))
    yield make("freeMe", row=1, **with_things([1]))
    yield make("freeMe", row=0, **with_things([0, 5]))
    yield make("freeMe", row=5, heads=(7, None), **base)
    yield make("freeMe", row=3, fix={3: dict(prev=7)}, **base)
    yield make("freeMe", row=5, used=[3, 5, 7], things={5: thing(kind=3, dead=1, w20=5, w24=6)})


def edges_delete_pending():
    base = with_things([3, 5, 7])
    yield make("deletePending", **base)
    yield make("deletePending", pending=[], free=[20, 21], **base)
    for ids in ([5], [3], [7], [5, 3], [7, 5, 3], [3, 3], [5, 7], [3, 5, 7]):
        yield make("deletePending", pending=ids, **base)
    yield make("deletePending", pending=[0], **base)
    yield make("deletePending", pending=[9], **base)
    yield make("deletePending", pending=[3, 5, 7, 9, 11], free=[20], **base)
    yield make("deletePending", pending=[0x27FF], **with_things([0x27FF, 3]))
    yield make("deletePending", pending=[0x27FF, 3], **with_things([0x27FF, 3]))
    yield make("deletePending", pending=[5], fix={5: dict(prev=None)}, **base)
    yield make("deletePending", pending=[5, 5, 5], **base)
    yield make("deletePending", pending=[1, 2, 3, 4, 5], used=[1, 2, 3, 4, 5], things={r: thing() for r in (1, 2, 3, 4, 5)})
    yield make("deletePending", pending=[0x80, 3], used=[0x80, 3], things={0x80: thing(), 3: thing()})


def edges_delete_all():
    for state in STATES:
        yield make("deleteAll", state=state, **with_things([3, 5]))
        yield make("deleteAll", state=state)
    for n in (1, 2, 5):
        yield make("deleteAll", **with_things(list(range(1, n + 1))))
    for dead in (1, 2, 0x80, 0xFF):
        yield make("deleteAll", **with_things([3, 5, 7], dead=dead))
    mixed = dict(used=[3, 5, 7, 9], things={3: thing(dead=1), 5: thing(), 7: thing(dead=2), 9: thing(kind=3)})
    yield make("deleteAll", **mixed)
    yield make("deleteAll", q={"killmode": [1]}, **mixed)
    for n in (1, 2, 5):
        yield make("deleteAll", q={"killmode": [1]}, **with_things(list(range(1, n + 1))))
    yield make("deleteAll", q={"killmode": [1]}, pending=[3], **with_things([3, 5, 7]))
    yield make("deleteAll", q={"killmode": [0]}, pending=[3], **with_things([3, 5, 7]))
    yield make("deleteAll", pending=[5, 7], **with_things([3, 5, 7]))
    yield make("deleteAll", used=[3, 5, 7], things={3: thing(id=5), 5: thing(), 7: thing()})
    yield make("deleteAll", q={"killmode": [1]}, used=[3, 5, 7], things={3: thing(id=5), 5: thing(), 7: thing()})
    yield make("deleteAll", used=[3, 5], things={0: thing(), 3: thing(), 5: thing()})
    yield make("deleteAll", used=[3, 5], things={0: thing(), 3: thing(), 5: thing()}, q={"killmode": [1]})
    yield make("deleteAll", state=4, **with_things([0x27FF, 1]))
    yield make("deleteAll", q={"killmode": [1]}, free=[20, 21], **with_things([0x27FF, 1, 2]))


def edges_update():
    yield make("update")
    yield make("update", step=2.5)
    yield make("update", **with_things(list(range(1, len(ALL_KINDS) + 1)), ALL_KINDS))
    for dead in (0, 1, 2, 0xFF, 0x80):
        yield make("update", **with_things([1, 2, 3, 4, 5, 6, 7, 8, 9, 10], [1, 4, 5, 6, 7, 8, 0x12, 3, 0, 2], dead=dead))
    for step in STEPS:
        yield make("update", step=step, **with_things([1, 2, 3, 4], [1, 4, 3, 0x12]))
    yield make("update", used=[3, 5, 7], things={3: thing(1), 7: thing(1)})
    yield make("update", used=[3, 5, 7], things={3: thing(1, id=5), 5: thing(4), 7: thing(8)})
    yield make("update", used=[3, 5, 7], things={3: thing(1, id=7), 5: thing(4), 7: thing(8)})
    yield make("update", used=[3], things={0: thing(), 3: thing(1)})
    yield make("update", used=[0x27FF, 1], things={0x27FF: thing(7), 1: thing(0x12)})
    yield make("update", used=[3, 5], things={3: thing(1, w20=5, w24=6), 5: thing(3, w24=7)}, fix={3: dict(index=0x10003)})


def edges_bumper():
    yield make("bumper")
    for w24 in (1, 0x100, 0x80000000, 0xFFFFFFFF, 0xFF00, 0, 0xFF, 0x12345678):
        yield make("bumper", step=0.25, **with_things([4], [3], w24=w24, w20=0x77))
    for kind in (0, 1, 2, 4, 0x12, 0x13, 0x83, 0xFF):
        yield make("bumper", **with_things([4], [kind], w24=5))
    for dead in (1, 2, 0xFF):
        yield make("bumper", **with_things([4, 5], [3, 3], w24=5, dead=dead))
    for step in STEPS:
        yield make("bumper", step=step, **with_things([1, 2, 3], [3, 1, 3], w24=9))
    yield make("bumper", used=[3, 5, 7], things={3: thing(3, w24=1), 7: thing(3, w24=2)})
    yield make("bumper", used=[3, 5, 7], things={3: thing(3, id=5, w24=1), 5: thing(3, w24=2), 7: thing(3, w24=3)})
    yield make("bumper", used=[3], things={0: thing(), 3: thing(3, w24=1)})
    yield make("bumper", **with_things([1, 2, 3, 4, 5, 6], [3, 3, 1, 3, 4, 3], w24=0x41))


def edges_closed():
    for word in WORDS + [0x100, 0xFFFF, 0x10000]:
        yield make("closed", closed=word)
    yield make("closed", closed=7, **with_things([3]))


def edges_gates():
    for turn in (0, 29):
        yield make("gates", gates=0, turn=turn)
    yield make("gates", gates=0, **with_things([5], [3], w20=1))
    for g in (1, 5, 0x27FF):
        for word in (0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x1234):
            yield make("gates", gates=g, things={g: thing(kind=3, w20=word)})
        for var in (1, 0, -1, 2, -2 ** 31, 0x7FFFFFFF):
            yield make("gates", gates=g, things={g: thing(kind=3, w20=0x55)}, q={"var": [var]})
    yield make("gates", gates=5, things={5: thing(kind=3, w20=3)}, script=0)
    yield make("gates", gates=5, things={0: thing(), 5: thing(kind=3, w20=3)})
    yield make("gates", gates=0x10005, things={5: thing(kind=3, w20=3)})
    yield make("gates", gates=5, things={5: thing(kind=3, w20=3, dead=1)}, closed=1)


def gate_case(**kw):
    """A turn that reaches the staff scan unless the case breaks one of the guards: the turn before is 29, the park is closed, the gates
    object (row 5) is in the table, the script variable is 1 and nobody is inside."""
    things = {5: thing(kind=3, w20=0x77)}
    used = [5]
    for row, kind in zip(kw.pop("staff_rows", ()), kw.pop("staff", ())):
        things[row] = thing(kind=kind)
        used.append(row)
    for row, spec in kw.pop("more", {}).items():
        things[row] = spec
        used.append(row)
    base = dict(turn=29, closed=1, gates=5, used=used, things=things, q={"var": [1], "visitors": [0], "inside": [1]})
    for key, value in kw.pop("q", {}).items():
        base["q"][key] = value
    base.update(kw)
    return make("doTurn", **base)


def edges_do_turn():
    yield make("doTurn")
    yield make("doTurn", **with_things([3, 5, 7], [1, 3, 4]))
    for state in STATES:
        yield make("doTurn", state=state, **with_things([3, 5, 7], [1, 3, 4]))
    for dead in (1, 2, 0xFF):
        yield make("doTurn", **with_things([3, 5, 7], [1, 3, 4], dead=dead))
    yield make("doTurn", used=[3, 5, 7], things={3: thing(1), 7: thing(1)})
    yield make("doTurn", used=[3, 5, 7], things={3: thing(1, id=5), 5: thing(3), 7: thing(4)})
    yield make("doTurn", used=[3], things={0: thing(), 3: thing()})
    for turn in (0, 28, 29, 30, 57, 58, 59, 0x7FFFFFFF, 0xFFFFFFFD, 0xFFFFFFFE, 0xFFFFFFFF, 0x1FFFFFFE, 89, 119):
        yield gate_case(turn=turn, staff_rows=[10, 11], staff=[4, 8])
    # every guard failing on its own
    yield gate_case(gates=0, staff_rows=[10], staff=[4])
    yield gate_case(turn=28, staff_rows=[10], staff=[4])
    yield gate_case(closed=0, staff_rows=[10], staff=[4])
    yield gate_case(q={"var": [0]}, staff_rows=[10], staff=[4])
    yield gate_case(q={"var": [-1]}, staff_rows=[10], staff=[4])
    yield gate_case(q={"var": [2]}, staff_rows=[10], staff=[4])
    yield gate_case(q={"var": [1, 5]}, staff_rows=[10], staff=[4])
    yield gate_case(q={"visitors": [1]}, staff_rows=[10], staff=[4])
    yield gate_case(q={"visitors": [-1]}, staff_rows=[10], staff=[4])
    yield gate_case(q={"visitors": [0x7FFFFFFF]}, staff_rows=[10], staff=[4])
    yield gate_case(script=0, staff_rows=[10], staff=[4])
    yield gate_case(analyser=0, staff_rows=[10], staff=[4])
    yield gate_case(analyser=0, q={"visitors": [3]}, staff_rows=[10], staff=[4])
    for word in (1, 2, 0x80000000, 0xFFFFFFFF, 0x100):
        yield gate_case(closed=word, staff_rows=[10], staff=[4])
    # the staff scan
    yield gate_case()
    yield gate_case(more={20: thing(1), 21: thing(3), 22: thing(9), 23: thing(0x12)})
    for kind in range(0, 0x15):
        yield gate_case(staff_rows=[10], staff=[kind], q={"inside": [0]})
    for kind in STAFF_KINDS:
        yield gate_case(staff_rows=[10], staff=[kind], q={"inside": [1]})
        yield gate_case(staff_rows=[10], staff=[kind], q={"inside": [0]})
        yield gate_case(staff_rows=[10], staff=[kind], q={"inside": [-1]})
        yield gate_case(staff_rows=[10], staff=[kind], q={"inside": [2]})
    rows, kinds = [10, 11, 12, 13, 14], [4, 5, 6, 7, 8]
    yield gate_case(staff_rows=rows, staff=kinds, q={"inside": [1, 1, 1, 1, 1]})
    yield gate_case(staff_rows=rows, staff=kinds, q={"inside": [0, 1, 1, 1, 1]})
    yield gate_case(staff_rows=rows, staff=kinds, q={"inside": [1, 1, 0, 1, 1]})
    yield gate_case(staff_rows=rows, staff=kinds, q={"inside": [1, 1, 1, 1, 0]})
    yield gate_case(staff_rows=rows, staff=kinds, q={"inside": [1, 1, 1, 0, 1]})
    yield gate_case(staff_rows=rows, staff=kinds, q={"inside": [0, 0, 0, 0, 0]})
    yield gate_case(staff_rows=rows, staff=kinds, q={"inside": [1, 1, 1, 1, 1]}, turn=0xFFFFFFFF)
    # dead staff and non-staff are not asked
    yield gate_case(more={20: thing(4, dead=1), 21: thing(5, dead=2), 22: thing(8, dead=0xFF)}, q={"inside": [0]})
    yield gate_case(more={20: thing(4, dead=1), 21: thing(5)}, q={"inside": [0]})
    yield gate_case(more={20: thing(1), 21: thing(3), 22: thing(9), 23: thing(0x12), 24: thing(0x13)}, q={"inside": [0]})
    # the gates object is not in the used list; pending deletions at the end
    yield make("doTurn", turn=29, closed=1, gates=5, used=[10], things={5: thing(kind=3, w20=9), 10: thing(kind=4)},
               q={"var": [1], "visitors": [0], "inside": [1]})
    yield gate_case(pending=[10, 11], staff_rows=[10, 11], staff=[4, 5])
    yield gate_case(pending=[20], more={20: thing(1)}, free=[30, 31])
    yield make("doTurn", used=[3], things={3: thing(1)}, pending=[3], turn=5)
    yield gate_case(staff_rows=[10, 11], staff=[4, 5], q={"inside": [1, 1]}, state=4)
    yield gate_case(staff_rows=[10, 11], staff=[4, 5], q={"inside": [1, 1]}, gates=0x10005)
    yield gate_case(more={20: thing(1, id=21), 21: thing(4), 22: thing(8)}, q={"inside": [0]})


def edges_online():
    for gtype in (0, 1, 2, 3, 0xFFFFFFFF):
        for guard in (0, 1, 0x80, 0xFF, 2):
            for ctor in (0, 1, 2):
                yield make("online", gtype=gtype, guard=guard, ctor=ctor, turn=5, **with_things([3, 5], [1, 4]))
    yield make("online", gtype=1, guard=1, turn=29, closed=1, gates=5, used=[5, 10], things={5: thing(kind=3, w20=1), 10: thing(kind=4)},
               q={"var": [1], "inside": [0]})
    yield make("online", gtype=1, guard=0, ctor=1, state=4, turn=0xFFFFFFFF)


# ------------------------------------------------------------------ the random cases


def rand_rows(r, n):
    rows = []
    while len(rows) < n:
        row = r.choice([r.randrange(1, 40), r.randrange(1, 40), r.randrange(1, ROWS), 0x27FF, 1, 2])
        if row not in rows:
            rows.append(row)
    return rows


def rand_word(r):
    return r.choice([0, 1, 2, 0x80000000, 0xFFFFFFFF, 0x7FFFFFFF, r.getrandbits(32), r.getrandbits(32), r.getrandbits(8), 0x100, 0xFF00])


def rand_step(r):
    return r.choice(STEPS + [T.rfloat(r), T.rfloat(r), T.rfloat_mod(r)])


def rand_thing(r, kinds, **kw):
    spec = dict(kind=r.choice(kinds), dead=r.choice([0, 0, 0, 0, 0, 1, 2, 0x80, 0xFF]), id=None, w20=rand_word(r), w24=rand_word(r))
    spec.update(kw)
    return spec


def rand_case(op, r):
    walk = op in ("update", "bumper", "doTurn", "deleteAll")
    n = r.choice([0, 1, 1, 2, 2, 3, 3, 4, 5, 6, 8]) if op != "online" else r.choice([0, 1, 2])
    rows = rand_rows(r, n)
    r.shuffle(rows)
    kinds = ALL_KINDS if op not in ("bumper", "doTurn") else [1, 3, 3, 4, 5, 6, 7, 8, 9, 0x12, 0, 2, 0xFF]
    c = make(op, used=list(rows))
    c["step"] = rand_step(r)
    nulls_ok = op in ("update", "bumper", "doTurn")
    for i, row in enumerate(rows):
        if nulls_ok and r.random() < 0.08:
            continue
        spec = rand_thing(r, kinds)
        if op == "bumper" and r.random() < 0.6:
            spec["kind"] = 3
        if walk and r.random() < 0.08 and i + 1 < len(rows):
            spec["id"] = rows[r.randrange(i + 1, len(rows))]      # the walk follows the id's row: a later row of the list
        c["things"][row] = spec
    if op in ("getHead", "getNext", "add", "freeMe", "deletePending") and r.random() < 0.8:
        for row in rows:
            c["things"][row] = rand_thing(r, ALL_KINDS)
    for row in rows:
        if r.random() < 0.25:
            c["fix"][row] = dict(index=row | (r.getrandbits(16) << 16))
    c["free"] = [x for x in rand_rows(r, r.choice([0, 0, 1, 2, 3, 5])) if x not in rows]
    c["pending"] = []
    c["world"] = r.choice([0, 1, 1])
    c["state"] = r.choice(STATES)
    c["turn"] = r.choice([0, 28, 29, 59, 0xFFFFFFFF, 0x7FFFFFFF, r.getrandbits(32), 30 * r.randrange(1, 1000) - 1])
    c["closed"] = r.choice([0, 1, 1, 2, r.getrandbits(32)])
    c["gates"] = r.choice([0, 0, r.choice(rows) if rows else 0, r.randrange(1, 40)])
    c["analyser"] = r.choice([0, 1, 1, 1])
    c["script"] = r.choice([0, 1, 1, 1])
    c["q"] = {"inside": [r.choice([0, 1, 1, 2, -1]) for _ in range(r.randrange(1, 6))],
              "visitors": [r.choice([0, 0, 0, 1, 5, -1, 0x7FFFFFFF])],
              "var": [r.choice([1, 1, 1, 0, -1, 2, -2 ** 31, 0x7FFFFFFF])],
              "killmode": [r.choice([0, 1])]}
    if op == "getNext":
        c["arg"] = None if r.random() < 0.1 else thing(id=r.choice(rows + [0, 1, r.randrange(0, ROWS)] if rows else [0, 1]))
    if op == "add":
        c["arg"] = None if r.random() < 0.15 else thing(id=r.randrange(0, ROWS))
        if c["free"] and r.random() < 0.1:
            c["things"][c["free"][0]] = thing()
        if c["free"] and r.random() < 0.3:
            c["fix"][c["free"][0]] = dict(prev=r.choice(rows + [None] if rows else [None]), index=rand_word(r))
    if op == "freeMe":
        pool = rows + c["free"] + [r.randrange(0, ROWS)]
        c["row"] = r.choice(pool)
        if r.random() < 0.3 and rows:
            fx = c["fix"].setdefault(r.choice(rows), {})
            fx[r.choice(["prev", "next"])] = r.choice(rows + [None])
    if op in ("deletePending", "deleteAll", "doTurn"):
        pool = rows + [0, r.randrange(0, ROWS)]
        c["pending"] = [r.choice(pool) for _ in range(r.choice([0, 0, 1, 2, 3]))] if rows or r.random() < 0.5 else []
        if op == "deletePending" and r.random() < 0.7:
            c["pending"] = [x for x in rows if r.random() < 0.7]
    if op == "deleteAll":
        c["things"] = {row: rand_thing(r, [1, 3, 4, 9, 0x12]) for row in rows}
    if op == "closed":
        c["closed"] = rand_word(r)
    if op in ("gates", "doTurn", "online"):
        g = c["gates"]
        if g:
            c["things"][g] = rand_thing(r, [3], dead=r.choice([0, 0, 1]))
            c["gates"] = g if r.random() < 0.9 else g | (r.getrandbits(8) << 16)
    if op == "online":
        c["gtype"] = r.choice([0, 1, 1, 2, 3, 0xFFFFFFFF, 0x80000000])
        c["guard"] = r.choice([0, 1, 0x80, 0xFF, 2])
        c["ctor"] = r.choice([0, 1, 2, 3])
    if op == "doTurn":
        if r.random() < 0.6:
            # aim at the gate block: a gates object, a closed park, the turn before a multiple of 30, nobody inside, and things that
            # the staff scan may visit (a guard is broken now and then)
            g = (c["gates"] & 0xFFFF) or r.randrange(1, 40)
            c["gates"] = g
            c["things"][g] = rand_thing(r, [3], dead=r.choice([0, 0, 1]))
            for row in rows:
                if row not in c["things"]:
                    c["things"][row] = rand_thing(r, [4, 5, 6, 7, 8, 4, 5, 1, 3, 9, 0x12, 0, 2, 0xFF])
            c["closed"] = r.choice([1, 2, 0xFFFFFFFF, r.getrandbits(32) | 1])
            c["turn"] = r.choice([30 * r.randrange(1, 100000) - 1, 29, 0xFFFFFFFF])
            c["state"] = r.choice([0, 0, 1, 2, 4])
            c["analyser"] = 1
            c["q"]["var"] = [1]
            c["q"]["visitors"] = [0]
            if r.random() < 0.3:
                flip = r.choice(["closed", "var", "visitors", "turn", "gates"])
                if flip == "closed":
                    c["closed"] = 0
                elif flip == "var":
                    c["q"]["var"] = [r.choice([0, 2, -1])]
                elif flip == "visitors":
                    c["q"]["visitors"] = [r.choice([1, 3, -1])]
                elif flip == "turn":
                    c["turn"] += 1
                else:
                    c["gates"] = 0
        if any(row not in c["things"] for row in rows):
            c["closed"] = 0                      # the staff scan does not test for a NULL thing
    return c


def tests():
    names = {
        "doTurn": "CWorld::DoGameTurn", "online": "CWorld::DoOnlineGameTurn", "update": "CWorld::UpdateThingsInEngine",
        "bumper": "CWorld::UpdateBumperCarsInEngine", "add": "CWorld::AddThingToThingList", "freeMe": "CWorld::_CThingArrayEntry::FreeMe",
        "deleteAll": "CWorld::DeleteAllThings", "deletePending": "CWorld::DeletePendingThings", "getNext": "CWorld::GetNextUsedThing",
        "getHead": "CWorld::GetUsedThingHead", "closed": "CWorld::IsParkClosed", "gates": "CWorld::ParkGates_GetStatus",
    }
    edges = {
        "doTurn": edges_do_turn, "online": edges_online, "update": edges_update, "bumper": edges_bumper, "add": edges_add,
        "freeMe": edges_free_me, "deleteAll": edges_delete_all, "deletePending": edges_delete_pending, "getNext": edges_get_next,
        "getHead": edges_get_head, "closed": edges_closed, "gates": edges_gates,
    }
    out = []
    for op in names:
        out.append(FnTest(names[op], ppc, host, edge=edges[op], rnd=lambda r, op=op: rand_case(op, r)))
    return out


TESTS = tests()
