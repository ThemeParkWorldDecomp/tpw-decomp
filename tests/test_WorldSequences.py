"""The world's sequences and small helpers (unit w194, main app), against libtpw_unit_WorldSequences.dylib (tests/unit_WorldSequences.cpp):

  src/World_sequences.cpp   CWorld::PopulatePark (0x1013b6ac), StartingPopulationForOnline (0x1013b88c), StartBankruptcySequence (0x1013baa4),
                            StartWinningSequence (0x1013bc68), IsPlayingWinningAnim (0x1013bdd0), EndWinningSequence (0x1013be2c),
                            GetPrevUsedThing (0x1013aeb0), SeedRand (0x1013b244), ReconstructFreeList (0x1013dd14), GetNewPersonIndex
                            (0x10140e64), FlagItemAsSelected (0x10140ee4), ConvertDLRidesUsedToIndices (0x1014113c), RestoreEngineObjects
                            (0x1013e210), DeleteEngineObjects (0x1013e368), TbTList::RemoveHead (0x101414a4) and AddHead (0x10141ad4)

The sixteen run as the original on the PowerPC side and as src/ on the host. Every other callee is a fake that records the same event on both
sides: GAME_ASSERTMSG (the condition and the format; the winning assertion's by-value string copy is not logged), CTRACE (level and first
%d), the thing virtuals (GetThingId, KillMe, RequestBreakdown, UpdateEngine, KillSpriteScript), DestroyRideInstance, the two windows, the
sprite switch, the first person ride stop, CObject's constructor (it writes the thing's own id from the queue "id"), the people's
constructors, the map's entrance and path queries and the entrance and booth positions (IsXYValid is functional and NOT logged), the park
analyser's ride count, the gates' status (queue "gates"), SetParkOpenOrClosed, DeletePendingThings, the ride database (first, next, record
by id), _wcsicmp (ASCII case folded), operator new (logged for blocks of 0x100 bytes and more: the list nodes of 10 bytes on the Mac and 12
on the host are not compared) and operator delete (a count).

A case describes the thing table (used rows in list order, per-row overrides), the thing blocks (id, kind, dead flag, the words at +0xc and
+0x1c, the halfword at +0x2e), the world's four words, the globals (the winning thing, the ride bitmap, the "End" name), the ride database,
the map's cell lists and positions, the pending list and the fakes' queues. What a case compares: the answer, the event log, the table rows
that changed and both heads, the world's four words (and that every other byte still holds the fill), the globals, every thing block field
(and that its other bytes still hold the fill), the pending list (count, head and tail NULL flags, the forward and backward ids), the output
array of ConvertDLRidesUsedToIndices and the heap's delete count.

Not tested: a NULL thing passed to GetPrevUsedThing, a missing ride record in the ride-database lookups, a folder without a backslash or a
digit, a ride number outside 0 to 0x100 (the bitmap has 0x20 bytes), allocation failures (the Mac skips the constructor on NULL).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_WorldSequences"

OPS = {
    "populate": (0, 0x1013B6AC), "online": (1, 0x1013B88C), "bankrupt": (2, 0x1013BAA4), "startwin": (3, 0x1013BC68),
    "isplay": (4, 0x1013BDD0), "endwin": (5, 0x1013BE2C), "prev": (6, 0x1013AEB0), "seed": (7, 0x1013B244), "rfl": (8, 0x1013DD14),
    "person": (9, 0x10140E64), "flag": (10, 0x10140EE4), "convert": (11, 0x1014113C), "remove": (12, 0x101414A4), "add": (13, 0x10141AD4),
    "restore": (14, 0x1013E210), "delete": (15, 0x1013E368),
}
OWN = {entry for _, entry in OPS.values()}

TABLE, ROWS, ROW_BYTES = 0x1034CBFC, 0x2800, 0x14
USED_HEAD, FREE_HEAD = 0x1034CBF4, 0x1034CBF8
LIST, LIST_COUNT = 0x1034CBD8, 0x1034CBE4
WINNING, END_NAME, BITMAP = 0x1034CAD4, 0x1034CAD8, 0x1034CBB8
RIDES, UI = 0x1037F6C8, 0x10396440
WORLD_SIZE = 0x1DA748
W_START, W_PERSONS, W_GATES, W_STATE = 0x1DA708, 0x1DA714, 0x1DA732, 0x1DA738
CELLS, CELL_BYTES = 0x2D8, 0x44
BLOCK = 0x80
RIDE_BYTES = 0x810
MASK = 0xFFFFFFFF
L_WORLD, L_ANALYSER, L_LIST, L_RIDES, L_UI, L_BLOCK = 1, 2, 5, 6, 7, 10
OUT_BYTES = 0x210

ENTRY_ZERO = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"

FILL_WORLD = b"\xa5" * WORLD_SIZE
FILL_BLOCK = b"\xa5" * BLOCK
DEFAULT_TABLE = b"".join(struct.pack(">5I", 0, i, 0, 0, 0) for i in range(ROWS))
HOST_TABLE = (C.c_int64 * (6 * ROWS + 8))()
# the block bytes that are reported as fields (the rest must keep the fill)
FIELD_BYTES = set(range(0, 4)) | set(range(0xC, 0x10)) | set(range(0x1C, 0x20)) | set(range(0x2E, 0x30))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ------------------------------------------------------------------ helpers (the strings are the original's layout)


def wide_read(e, a, limit=1000):
    out = []
    while a and len(out) < limit:
        ch = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if ch == 0:
            break
        out.append(chr(ch))
    return "".join(out)


def s_read(e, obj, wide):
    data = e.u32(obj + 8)
    if not data:
        return ""
    return wide_read(e, data) if wide else e.cstr(data, limit=1000)


def s_write(e, obj, text, wide):
    """A TbDynamicStringTemplate in the original's layout {vtable, capacity, data, length, grow-by}."""
    if wide:
        blob = b"".join(struct.pack(">H", ord(ch) & 0xFFFF) for ch in text) + b"\0\0"
    else:
        blob = text.encode("latin-1") + b"\0"
    data = e.alloc(len(blob), blob, alignment=4)
    e.write(obj, struct.pack(">IIIII", CL.FAKE_VT, 0, data, len(text), 0))


# ------------------------------------------------------------------ the case

WORDS = [0, 1, 2, 0x80000000, 0xFFFFFFFF, 0x7FFFFFFF, 0x12345678]
STATES = [0, 1, 2, 4, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 3]
IDX_WORDS = [0, 1, 0x27FF, 0xFFFF, 0x10000, 0x10003, 0x12345, 0x80000000, 0xFFFFFFFF]
KINDS = list(range(0, 0x14)) + [0x7F, 0x80, 0xFF]


def thing(kind=1, dead=0, id=None, w0c=0, w1c=0, h2e=0):
    """A thing block: the kind (+2), the dead flag (+3), its own id (None: its row), the words at +0xc and +0x1c, the halfword at +0x2e."""
    return dict(kind=kind, dead=dead, id=id, w0c=w0c, w1c=w1c, h2e=h2e)


def ride(id, number=0, folder="pk\\ride1.wad", name="x"):
    return dict(id=id, number=number, folder=folder, name=name)


def make(op, **kw):
    """A case with every field defaulted. used: rows of the used list, newest first; free: rows of the free list; things: row -> thing (a used
    row without one holds a NULL thing); alias: more rows that hold the first block; fix: row -> overrides (index, next, prev, nfree; None is NULL); heads: the two heads when they are
    not the lists' first rows; arg: the thing GetPrevUsedThing is given; start/persons/gates/state: the world's words; winning: the
    winning thing id; bitmap: the 0x20 ride bytes; endname: the name of the "End" feature; rides: the ride database; valid/entrance/path:
    1 based map cells; pos: entrance A, B, booth A, B; nrides: the analyser's ride count; plist: the pending list's ids; pcount: its
    count word (default len); a: the seed or ride id; junk: the low 16 bits the Mac's caller leaves under an id; q: the fakes' queues."""
    c = dict(op=op, used=[], free=[], things={}, alias=[], fix={}, heads=None, arg=None, start=0, persons=0, gates=0, state=0, winning=0,
             bitmap=bytes(32), endname="End", rides=[], valid=[], entrance=[], path=[], pos=[0, 0, 0, 0], nrides=0, plist=[],
             pcount=None, a=0, junk=0, q={})
    c.update(kw)
    return c


def with_things(rows, kinds=None, **kw):
    kinds = kinds or [1] * len(rows)
    return dict(used=list(rows), things={r: thing(kind=k, **kw) for r, k in zip(rows, kinds)})


def plan(c):
    """The blocks, the table rows and the heads of a case."""
    specs, block_of = [], {}
    for r in sorted(c["things"]):
        block_of[r] = len(specs)
        specs.append(c["things"][r])
    arg_block = None
    if c["arg"] is not None:
        arg_block = len(specs)
        specs.append(c["arg"])
    used, free = c["used"], c["free"]
    alias = set(c["alias"])
    rows = sorted(set(used) | set(free) | set(c["things"]) | set(c["fix"]) | alias | {0})
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
        table.append((r, block_of[r] + 1 if r in block_of else (1 if r in alias else 0), fx.get("index", r) & MASK, enc(nxt), enc(prv), enc(nfree)))
    heads = c["heads"] if c["heads"] is not None else (used[0] if used else None, free[0] if free else None)
    return specs, arg_block, table, (enc(heads[0]), enc(heads[1]))


def spec_ids(c, specs):
    rows = sorted(c["things"])
    out = []
    for k, spec in enumerate(specs):
        row = rows[k] if k < len(rows) else 0
        out.append((row if spec["id"] is None else spec["id"]) & 0xFFFF)
    return out


def pcount_of(c):
    return len(c["plist"]) if c["pcount"] is None else c["pcount"]


# ------------------------------------------------------------------ the PowerPC side


class State:
    """What the fakes of one case share."""

    def __init__(self, c, world, analyser, rides):
        self.c = c
        self.world = world
        self.analyser = analyser
        self.rides = rides
        self.cursor = 0
        self.deletes = 0


def stubs(e, L, st):
    S = CL.Stubs()
    S.real.update(OWN)
    c = st.c

    def text(a):
        return e.cstr(a)[:159] if a else "(null)"

    S.internal(0x1000DDE8, lambda e: L.rec("GAME_ASSERTMSG", (T.s32(e.r(3)),), (text(e.r(4)),)))

    def ctrace(e):
        fmt = text(e.r(4))
        L.rec("CTRACE", (e.r(3), e.r(5) if "%d" in fmt else 0), (fmt,))

    S.internal(0x1006FCBC, ctrace)

    def get_id(e):
        L.rec("GetThingId", (L.lab(e.r(4)),))
        e.write(e.r(3), e.read(e.r(4), 2) if e.r(4) else b"\0\0")

    S.internal(0x1012E1F8, get_id)
    S.internal(0x1012E728, lambda e: L.rec("KillMe", (L.lab(e.r(3)),)))
    S.internal(0x1010D90C, lambda e: L.rec("RequestBreakdown", (L.lab(e.r(3)),)))
    S.internal(0x10107D38, lambda e: L.rec("UpdateEngine", (L.lab(e.r(3)), T.s32(e.r(4)))))
    S.internal(0x100FAC4C, lambda e: L.rec("KillSpriteScript", (L.lab(e.r(3) - 0xC),)))
    S.internal(0x1006A3EC, lambda e: L.rec("DestroyRideInstance", (e.r(3),)))
    S.internal(0x10032584, lambda e: L.rec("StopRidingRideIn1stPerson"))
    S.internal(0x1007ACA4, lambda e: L.rec("ESP_DisableSpriteDraw"))
    S.internal(0x101F11B0, lambda e: L.rec("Bankrupt", (L.lab(e.r(3)), L.lab(e.r(4)))))
    S.internal(0x101F120C, lambda e: L.rec("Congratulations", (L.lab(e.r(3)), L.lab(e.r(4)))))

    def cobject(e):
        L.rec("CObject", (L.newlab(e.r(3)), (e.r(4) >> 16) & 0xFFFF, (e.r(5) >> 16) & 0xFFFF, T.s32(e.r(6)), T.s32(e.r(7))))
        e.write(e.r(3), struct.pack(">H", L.ret("id", 0x1234) & 0xFFFF))

    S.internal(0x10105D74, cobject)

    def person(name):
        return lambda e: L.rec(name, (L.newlab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))

    S.internal(0x1010409C, person("CMechanic"))
    S.internal(0x100FEFD0, person("CHandyman"))
    S.internal(0x100FB094, person("CEntertainer"))
    S.internal(0x10116634, person("CPerson"))

    def valid(e):
        x, y = T.s32(e.r(3)), T.s32(e.r(4))
        e.set_r(3, 1 if (x + y * 0x80 + 1) in c["valid"] else 0)

    S.internal(0x101012F4, valid)

    def cell_of(a):
        return (a - (st.world + CELLS)) // CELL_BYTES + 1

    def cell_query(name, key):
        def fn(e):
            cell = cell_of(e.r(3))
            L.rec(name, (cell,))
            e.set_r(3, 1 if cell in c[key] else 0)
        return fn

    S.internal(0x1009C4DC, cell_query("IsEntrance", "entrance"))
    S.internal(0x1009C44C, cell_query("IsPath", "path"))

    def position(name, base):
        def fn(e):
            which = T.s32(e.r(5))
            L.rec(name, (which,))
            e.write(e.r(3), struct.pack(">H", c["pos"][base + (which & 1)] & 0xFFFF))
        return fn

    S.internal(0x10101AE0, position("GetParkEntrancePos", 0))
    S.internal(0x101019BC, position("GetTicketBoothPos", 2))

    def get_analyser(e):
        L.rec("GetParkAnalyser", (L.lab(e.r(3)),))
        e.set_r(3, st.analyser)

    S.internal(0x1013EBC8, get_analyser)

    def count_rides(e):
        L.rec("NumberOfObjectTypeInPark", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5))))
        e.set_r(3, c["nrides"] & MASK)

    S.internal(0x100EA5D8, count_rides)

    def status(e):
        L.rec("ParkGates_GetStatus", (L.lab(e.r(3)),))
        e.set_r(3, T.u32(L.ret("gates", 1)))

    S.internal(0x1013FFB4, status)
    S.internal(0x1013FC3C, lambda e: L.rec("SetParkOpenOrClosed", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)))))
    S.internal(0x1013DDA8, lambda e: L.rec("DeletePendingThings", (L.lab(e.r(3)),)))

    def first_ride(e):
        L.rec("GetFirstRide", (L.lab(e.r(3)),))
        st.cursor = 0
        e.set_r(3, st.rides[0] if st.rides else 0)

    def next_ride(e):
        L.rec("GetNextRide", (L.lab(e.r(3)),))
        st.cursor += 1
        e.set_r(3, st.rides[st.cursor] if st.cursor < len(st.rides) else 0)

    def ride_info(e):
        ident = (e.r(4) >> 16) & 0xFFFF
        L.rec("GetRideInfo", (L.lab(e.r(3)), ident))
        for addr, r in zip(st.rides, c["rides"]):
            if (r["id"] & 0xFFFF) == ident:
                e.set_r(3, addr)
                return
        e.set_r(3, st.rides[0])

    S.internal(0x101547B4, first_ride)
    S.internal(0x10154800, next_ride)
    S.internal(0x10154868, ride_info)

    def fold(s):
        return "".join(chr(ord(ch) + 32) if "A" <= ch <= "Z" else ch for ch in s)

    def wcsicmp(e):
        a, b = CL.wide_text(e, e.r(3)), CL.wide_text(e, e.r(4))
        L.rec("wcsicmp", (), (a, b))
        e.set_r(3, 0 if fold(wide_read(e, e.r(3))) == fold(wide_read(e, e.r(4))) else 1)

    S.imp("_wcsicmp__10NS_MacDozeFPCwPCw", wcsicmp)

    def op_new(e):
        size = e.r(3)
        if size >= 0x100:
            L.rec("new", (size,))
        e.set_r(3, e.alloc(size, b"\xa5" * size))

    S.imp("__nw__FUl", op_new)

    def op_delete(e):
        p = e.r(3)
        if p:
            st.deletes += 1
            if L.lab(p) > 0:
                L.rec("delete", (L.lab(p),))
                L.forget(p)

    S.imp("__dl__FPv", op_delete)
    S.imp("__ct__26TbDynamicStringTemplate<w>FRC26TbDynamicStringTemplate<w>", lambda e: s_write(e, e.r(3), s_read(e, e.r(4), True), True))
    S.imp("__dt__26TbDynamicStringTemplate<w>Fv", lambda e: None)

    def strpbrk(e):
        s, chars = e.cstr(e.r(3), limit=1000), e.cstr(e.r(4), limit=1000)
        for i, ch in enumerate(s):
            if ch in chars:
                e.set_r(3, e.r(3) + i)
                return
        e.set_r(3, 0)

    def strtol(e):
        a = e.r(3)
        s = e.cstr(a, limit=1000)
        n = 0
        while n < len(s) and s[n].isdigit():
            n += 1
        if e.r(4):
            e.write(e.r(4), struct.pack(">I", (a + n) & MASK))
        e.set_r(3, int(s[:n]) & MASK if n else 0)

    S.imp("strpbrk", strpbrk)
    S.imp("strtol", strtol)
    return S


def ppc_table_report(e, addr_of_block, initial):
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


def ppc_list_report(e):
    head, tail = e.u32(LIST + 4), e.u32(LIST + 8)
    fwd, a = [], head
    while a and len(fwd) < 64:
        fwd.append(struct.unpack(">H", e.read(a, 2))[0])
        a = struct.unpack(">I", e.read(a + 2, 4))[0]
    bwd, a = [], tail
    while a and len(bwd) < 64:
        bwd.append(struct.unpack(">H", e.read(a, 2))[0])
        a = struct.unpack(">I", e.read(a + 6, 4))[0]
    return (T.s32(e.u32(LIST_COUNT)), int(head == 0), int(tail == 0), fwd, bwd)


def ppc(e, c):
    op = c["op"]
    code, entry = OPS[op]
    specs, arg_block, table, heads = plan(c)
    ids = spec_ids(c, specs)
    world = e.alloc(WORLD_SIZE, FILL_WORLD)
    e.write(world + W_START, struct.pack(">I", c["start"] & MASK))
    e.write(world + W_PERSONS, struct.pack(">I", c["persons"] & MASK))
    e.write(world + W_GATES, struct.pack(">H", c["gates"] & 0xFFFF))
    e.write(world + W_STATE, struct.pack(">I", c["state"] & MASK))
    analyser = e.alloc(0x40, b"\xa5" * 0x40)
    blocks = []
    for k, spec in enumerate(specs):
        data = bytearray(FILL_BLOCK)
        struct.pack_into(">H", data, 0, ids[k])
        data[2], data[3] = spec["kind"] & 0xFF, spec["dead"] & 0xFF
        struct.pack_into(">I", data, 0xC, spec["w0c"] & MASK)
        struct.pack_into(">I", data, 0x1C, spec["w1c"] & MASK)
        struct.pack_into(">H", data, 0x2E, spec["h2e"] & 0xFFFF)
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
    # the rides
    rides = []
    for r in c["rides"]:
        rec = bytearray(RIDE_BYTES)
        struct.pack_into(">H", rec, 0, r["id"] & 0xFFFF)
        struct.pack_into(">i", rec, 0x94, T.s32(r["number"]))
        folder = r["folder"].encode("latin-1") + b"\0"
        rec[0x28C:0x28C + len(folder)] = folder
        name = b"".join(struct.pack(">H", ord(ch)) for ch in r["name"]) + b"\0\0"
        rec[0x68C:0x68C + len(name)] = name
        rides.append(e.alloc(RIDE_BYTES, bytes(rec)))
    # the globals
    e.write(WINNING, struct.pack(">H", c["winning"] & 0xFFFF))
    e.write(BITMAP, c["bitmap"])
    s_write(e, END_NAME, c["endname"], True)
    # the pending list
    nodes = []
    for i, ident in enumerate(c["plist"]):
        nodes.append(e.alloc(10, b"\0" * 10))
    for i, ident in enumerate(c["plist"]):
        nxt = nodes[i + 1] if i + 1 < len(nodes) else 0
        prv = nodes[i - 1] if i > 0 else 0
        e.write(nodes[i], struct.pack(">HII", ident & 0xFFFF, nxt, prv))
    e.write(LIST + 4, struct.pack(">II", nodes[0] if nodes else 0, nodes[-1] if nodes else 0))
    e.write(LIST_COUNT, struct.pack(">I", pcount_of(c) & MASK))
    out = e.alloc(OUT_BYTES, b"\xee" * OUT_BYTES)
    fixed = {world: L_WORLD, analyser: L_ANALYSER, LIST: L_LIST, RIDES: L_RIDES, UI: L_UI}
    for k, a in enumerate(blocks):
        fixed[a] = L_BLOCK + k
    L = CL.PpcLog(fixed, c["q"])
    e.cl = L
    st = State(c, world, analyser, rides)
    S = stubs(e, L, st)
    saved = S.activate(e)
    result = {}
    try:
        arg = blocks[arg_block] if arg_block is not None else 0
        args = {
            "populate": [world], "online": [world], "bankrupt": [world], "startwin": [world], "isplay": [world], "endwin": [world],
            "prev": [out + 4, world, arg], "seed": [world, c["a"] & MASK], "rfl": [world], "person": [world],
            "flag": [world, ((c["a"] & 0xFFFF) << 16) | (c["junk"] & 0xFFFF)], "convert": [world, out], "remove": [out + 4, LIST],
            "add": [LIST, ((c["a"] & 0xFFFF) << 16) | (c["junk"] & 0xFFFF)], "restore": [world], "delete": [world],
        }[op]
        r3 = T.call_ppc(e, entry, args)[0]
        if op == "isplay":
            result["ret"] = r3 & 0xFF
        elif op == "person":
            result["ret"] = r3
        elif op in ("prev", "remove"):
            if op == "prev" or c["plist"]:
                result["ret"] = struct.unpack(">H", e.read(out + 4, 2))[0]
            result["outguard"] = int(e.read(out, 4) == b"\xee" * 4 and e.read(out + 6, 10) == b"\xee" * 10)
        elif op == "convert":
            result["ret"] = r3
            raw = e.read(out, OUT_BYTES)
            result["out"] = (struct.unpack(">256H", raw[:0x200]), raw[0x200:])
        rows, hd = ppc_table_report(e, block_addr, initial)
        result["table"] = rows
        result["heads"] = hd
        result["list"] = ppc_list_report(e)
        result["globals"] = (struct.unpack(">H", e.read(WINNING, 2))[0], e.read(BITMAP, 0x20))
        wbytes = bytearray(e.read(world, WORLD_SIZE))
        result["world"] = (struct.unpack(">I", wbytes[W_START:W_START + 4])[0], struct.unpack(">I", wbytes[W_PERSONS:W_PERSONS + 4])[0],
                           struct.unpack(">H", wbytes[W_GATES:W_GATES + 2])[0], struct.unpack(">I", wbytes[W_STATE:W_STATE + 4])[0])
        wbytes[W_START:W_START + 4] = b"\xa5" * 4
        wbytes[W_PERSONS:W_PERSONS + 4] = b"\xa5" * 4
        wbytes[W_GATES:W_GATES + 2] = b"\xa5\xa5"
        wbytes[W_STATE:W_STATE + 4] = b"\xa5" * 4
        result["worldfill"] = int(bytes(wbytes) == FILL_WORLD)
        report = []
        for a in blocks:
            b = e.read(a, BLOCK)
            rest = bytes(b[i] for i in range(BLOCK) if i not in FIELD_BYTES)
            report.append((struct.unpack(">H", b[:2])[0], b[2], b[3], struct.unpack(">I", b[0xC:0x10])[0],
                           struct.unpack(">I", b[0x1C:0x20])[0], struct.unpack(">H", b[0x2E:0x30])[0], int(rest == b"\xa5" * len(rest))))
        result["things"] = report
        result["heap"] = st.deletes
        result["log"] = L.log
    finally:
        CL.Stubs.deactivate(e, saved)
    return result


# ------------------------------------------------------------------ the host side


def bind(lib):
    P32 = C.POINTER(C.c_uint32)
    lib.tpw_wsq_begin.argtypes = [C.c_int32]
    lib.tpw_wsq_ptr.argtypes = [C.c_int32]
    lib.tpw_wsq_ptr.restype = C.c_void_p
    lib.tpw_wsq_thing.argtypes = [C.c_int32] + [C.c_uint32] * 6
    lib.tpw_wsq_get_thing.argtypes = [C.c_int32, P32]
    lib.tpw_wsq_table.argtypes = [P32, C.c_int32, C.c_uint32, C.c_uint32]
    lib.tpw_wsq_get_table.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_wsq_world.argtypes = [C.c_uint32] * 4
    lib.tpw_wsq_get_world.argtypes = [P32]
    lib.tpw_wsq_ride.argtypes = [C.c_int32, C.c_uint32, C.c_int32, C.c_char_p, C.POINTER(C.c_uint16), C.c_int32]
    lib.tpw_wsq_globals.argtypes = [C.c_uint32, C.c_char_p, C.POINTER(C.c_uint16), C.c_int32]
    lib.tpw_wsq_map.argtypes = [P32, C.c_int32, P32, C.c_int32, P32, C.c_int32, P32, C.c_uint32]
    lib.tpw_wsq_get_globals.argtypes = [P32]
    lib.tpw_wsq_list.argtypes = [C.POINTER(C.c_uint16), C.c_int32, C.c_int32]
    lib.tpw_wsq_get_list.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_wsq_call.argtypes = [C.c_int32, C.c_int32, C.c_uint32, C.c_void_p]
    lib.tpw_wsq_call.restype = C.c_int64


def u32s(values):
    return (C.c_uint32 * max(len(values), 1))(*[v & MASK for v in values])


def host(lib, c):
    op = c["op"]
    code, entry = OPS[op]
    bind(lib)
    specs, arg_block, table, heads = plan(c)
    ids = spec_ids(c, specs)
    lib.tpw_wsq_begin(len(specs))
    for k, spec in enumerate(specs):
        lib.tpw_wsq_thing(k, ids[k], spec["kind"] & 0xFF, spec["dead"] & 0xFF, spec["w0c"] & MASK, spec["w1c"] & MASK, spec["h2e"] & 0xFFFF)
    flat = [x for row in table for x in row]
    lib.tpw_wsq_table(u32s(flat), len(table), heads[0], heads[1])
    lib.tpw_wsq_world(c["start"] & MASK, c["persons"] & MASK, c["gates"] & 0xFFFF, c["state"] & MASK)
    for k, r in enumerate(c["rides"]):
        name = [ord(ch) for ch in r["name"]]
        lib.tpw_wsq_ride(k, r["id"] & 0xFFFF, T.s32(r["number"]), r["folder"].encode("latin-1"), (C.c_uint16 * (len(name) + 1))(*name), len(name))
    name = [ord(ch) for ch in c["endname"]]
    lib.tpw_wsq_globals(c["winning"] & 0xFFFF, bytes(c["bitmap"]), (C.c_uint16 * (len(name) + 1))(*name), len(name))
    lib.tpw_wsq_map(u32s(c["valid"]), len(c["valid"]), u32s(c["entrance"]), len(c["entrance"]), u32s(c["path"]), len(c["path"]),
                    u32s(c["pos"]), c["nrides"] & MASK)
    plist = c["plist"]
    lib.tpw_wsq_list((C.c_uint16 * max(len(plist), 1))(*[p & 0xFFFF for p in plist]), len(plist), T.s32(pcount_of(c)))
    fixed = {L_WORLD: lib.tpw_wsq_ptr(L_WORLD), L_ANALYSER: lib.tpw_wsq_ptr(L_ANALYSER), L_LIST: lib.tpw_wsq_ptr(L_LIST),
             L_RIDES: lib.tpw_wsq_ptr(L_RIDES), L_UI: lib.tpw_wsq_ptr(L_UI)}
    for k in range(len(specs)):
        fixed[L_BLOCK + k] = lib.tpw_wsq_ptr(L_BLOCK + k)
    CL.host_begin(lib, fixed, c["q"])
    heap = (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")
    for i in range(4):
        heap[i] = 0
    out = (C.c_uint8 * OUT_BYTES)(*([0xEE] * OUT_BYTES))
    thing_arg = arg_block + 1 if arg_block is not None else 0
    try:
        r = lib.tpw_wsq_call(code, thing_arg, c["a"] & MASK, C.cast(out, C.c_void_p))
    finally:
        log = CL.host_end(lib)
    result = {}
    if op == "isplay":
        result["ret"] = r & 0xFF
    elif op == "person":
        result["ret"] = r & MASK
    elif op in ("prev", "remove"):
        if op == "prev" or c["plist"]:
            result["ret"] = r & 0xFFFF
        result["outguard"] = 1
    elif op == "convert":
        result["ret"] = r & MASK
        raw = bytes(out)
        result["out"] = (struct.unpack("<256H", raw[:0x200]), raw[0x200:])
    lib.tpw_wsq_get_table(HOST_TABLE)
    n = HOST_TABLE[0]
    result["table"] = [tuple(HOST_TABLE[1 + 6 * j + i] for i in range(6)) for j in range(n)]
    result["heads"] = (HOST_TABLE[1 + 6 * n], HOST_TABLE[2 + 6 * n])
    lst = (C.c_int64 * 300)()
    lib.tpw_wsq_get_list(lst)
    nf = lst[3]
    result["list"] = (T.s32(lst[0]), lst[1], lst[2], [lst[4 + i] for i in range(nf)], [lst[5 + nf + i] for i in range(lst[4 + nf])])
    g = (C.c_uint32 * 9)()
    lib.tpw_wsq_get_globals(g)
    result["globals"] = (g[0] & 0xFFFF, bytes(C.cast(g, C.POINTER(C.c_uint8))[4:36]))
    w = (C.c_uint32 * 5)()
    lib.tpw_wsq_get_world(w)
    result["world"] = (w[0], w[1], w[2], w[3])
    result["worldfill"] = w[4]
    report = []
    for k in range(len(specs)):
        t = (C.c_uint32 * 7)()
        lib.tpw_wsq_get_thing(k, t)
        report.append(tuple(t))
    result["things"] = report
    result["heap"] = heap[2]
    result["log"] = log
    return result


# ------------------------------------------------------------------ the cases

END_FOLDERS = ["pk\\ride1.wad", "a\\b\\c7", "x\\1", "dir\\ride123456789", "ab\\007x", "\\9", "p\\q\\r05.wad", "pack\\ride12.wad", "q\\a0b",
               "d\\9z\\8", "c:\\t\\ride65535", "c:\\t\\ride65536", "c:\\t\\ride70000"]


def walk(op, rows, kinds=None, **kw):
    c = make(op, **with_things(rows, kinds, **{k: v for k, v in kw.items() if k in ("dead", "w0c", "w1c", "h2e")}))
    for k, v in kw.items():
        if k not in ("dead", "w0c", "w1c", "h2e"):
            c[k] = v
    return c


def edges_seed():
    for s in WORDS + [0x19660D, 0x3C6EF35F, 0xDEADBEEF]:
        yield make("seed", a=s, start=0x55AA55AA)
        yield make("seed", a=s, start=s)


def edges_person():
    for p in [0, 1, 2, 5, 0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF, 9, 99, 0x12345678]:
        yield make("person", persons=p)


def edges_isplay():
    for w in [0, 1, 2, 5, 0x27FF, 0x8000, 0xFFFF, 0x1234, 0x100, 0xFF00]:
        yield make("isplay", winning=w)


def edges_prev():
    base = with_things([5, 3, 2])
    for ident in (5, 3, 2, 7, 0, 1, 0x27FF):
        yield make("prev", arg=thing(id=ident), **base)
    for idx in IDX_WORDS:
        yield make("prev", arg=thing(id=3), fix={5: dict(index=idx)}, **base)
        yield make("prev", arg=thing(id=5), fix={5: dict(index=idx)}, **base)
    yield make("prev", arg=thing(id=3), fix={3: dict(prev=None)}, **base)
    yield make("prev", arg=thing(id=0x27FF), used=[0x27FF, 4], things={0x27FF: thing(), 4: thing()})
    yield make("prev", arg=thing(id=4), used=[0x27FF, 4], things={0x27FF: thing(), 4: thing()})
    yield make("prev", arg=thing(id=3, kind=3, dead=1, w0c=7, w1c=9, h2e=4), **base)
    yield make("prev", arg=thing(id=3), fix={0: dict(prev=5)}, **base)


def edges_rfl():
    everything = list(range(1, 0x2800))
    yield make("rfl")
    yield make("rfl", things={1: thing()}, alias=everything)
    yield make("rfl", things={1: thing()}, alias=everything, free=[4, 5, 6])
    yield make("rfl", things={1: thing()}, alias=range(1, 0x2800, 2))
    yield make("rfl", things={1: thing()}, alias=range(2, 0x2800, 2))
    yield make("rfl", things={1: thing()}, alias=range(1, 0x27FF))
    yield make("rfl", things={2: thing()}, alias=range(2, 0x2800))
    yield make("rfl", things={0: thing()}, free=[5, 6, 7], heads=(None, 9))
    yield make("rfl", things={3: thing(), 5: thing(), 9: thing()}, free=[2, 4, 6, 8], heads=(None, 4))
    yield make("rfl", things={3: thing()}, free=[0x27FF, 1], fix={7: dict(nfree=3), 0x27FF: dict(nfree=2)})
    yield make("rfl", things={r: thing() for r in range(1, 30)})
    yield make("rfl", things={r: thing() for r in range(1, 30)}, alias=range(100, 140), free=[200, 201], heads=(5, 200))
    yield make("rfl", things={r: thing() for r in (1, 2, 0x27FE)}, fix={0: dict(nfree=7)})


def edges_flag():
    rs = [ride(1, 0), ride(2, 1), ride(3, 7), ride(4, 8), ride(5, 9), ride(6, 0x100), ride(7, 0x41), ride(8, 255), ride(9, 16), ride(10, 17)]
    for k in range(1, 11):
        yield make("flag", a=k, rides=rs)
    for k in (1, 2, 4):
        yield make("flag", a=k, rides=rs, bitmap=b"\xff" * 32)
        yield make("flag", a=k, rides=rs, bitmap=bytes([0x55] * 32), junk=0xFFFF)
    yield make("flag", a=0x99, rides=rs)                 # an unknown id answers ride 0
    yield make("flag", a=0x99, rides=[ride(1, 3)])
    yield make("flag", a=3, rides=rs, bitmap=bytes(range(32)), junk=0x1234)
    yield make("flag", a=0xFFFF, rides=rs + [ride(0xFFFF, 0x80)])
    yield make("flag", a=0, rides=[ride(0, 5)], q={})


def edges_convert():
    rs = [ride(i, i, folder=END_FOLDERS[(i - 1) % len(END_FOLDERS)]) for i in range(1, 14)]

    def bits(*numbers):
        b = bytearray(32)
        for n in numbers:
            b[(n - 1) >> 3] |= 1 << ((n - 1) & 7)
        return bytes(b)

    yield make("convert", rides=rs)
    yield make("convert", rides=rs, bitmap=bits(1))
    yield make("convert", rides=rs, bitmap=bits(1, 2, 3))
    yield make("convert", rides=rs, bitmap=bits(8, 9))
    yield make("convert", rides=rs, bitmap=bits(13, 5, 7))
    yield make("convert", rides=rs, bitmap=bits(*range(1, 14)))
    yield make("convert", rides=rs + [ride(20, 256, folder="z\\t99")], bitmap=bits(256))
    yield make("convert", rides=rs + [ride(20, 255, folder="z\\t98")], bitmap=bits(255, 256, 3))
    yield make("convert", rides=rs + [ride(20, 128, folder="z\\t97"), ride(21, 129, folder="z\\t96")], bitmap=bits(128, 129))
    yield make("convert", rides=rs, bitmap=bits(14))             # no ride: the assertion
    yield make("convert", rides=rs, bitmap=bits(14, 2, 200))
    yield make("convert", rides=[ride(1, 1)], bitmap=bits(1))
    yield make("convert", rides=[ride(3, 3, folder="a\\b\\c\\d0")], bitmap=bits(3))
    yield make("convert", rides=[ride(3, 3, folder="a\\b\\5c\\x9")], bitmap=bits(3))
    yield make("convert", rides=rs, bitmap=b"\x80" * 32)
    yield make("convert", rides=rs, bitmap=b"\xff" * 32)


def edges_startwin():
    rs = [ride(11, 1, name="Other"), ride(12, 2, name="End"), ride(13, 3, name="End")]
    yield make("startwin", rides=rs)
    yield make("startwin", rides=rs, q={"id": [0x77]})
    yield make("startwin", rides=rs, q={"id": [0]})
    yield make("startwin", rides=rs, q={"id": [0xFFFF]}, winning=0x55)
    yield make("startwin", rides=rs, endname="end")
    yield make("startwin", rides=rs, endname="END")
    yield make("startwin", rides=rs, endname="eNd")
    yield make("startwin", rides=rs, endname="En d")
    yield make("startwin", rides=rs, endname="")
    yield make("startwin", rides=[ride(5, 1, name="end")], endname="END", q={"id": [0x1234]})
    yield make("startwin", rides=[ride(0xFFFF, 1, name="End")])
    yield make("startwin", rides=[ride(0, 1, name="End")])
    yield make("startwin", rides=rs[:1])                      # not found: the assertion
    yield make("startwin", rides=rs[:1], winning=0x66)
    yield make("startwin", rides=[ride(1, 1, name="Endx"), ride(2, 2, name="xEnd")])
    yield make("startwin", rides=[ride(1, 1, name="Endless")], endname="Endless")
    yield make("startwin", rides=[ride(1, 1, name="Zzz")], endname="A longer feature name")
    yield make("startwin", rides=rs, winning=0x42)


def edges_endwin():
    base = with_things([5, 3, 9])
    for w in (5, 3, 9, 7, 1, 0x27FF):
        yield make("endwin", winning=w, **base)
    yield make("endwin", winning=0, **base)
    yield make("endwin", winning=0, **with_things([0, 5]))
    yield make("endwin", winning=5, **with_things([0, 5]))
    yield make("endwin", winning=5, things={5: thing(kind=3, dead=1, w0c=3, w1c=4, h2e=4)}, used=[5])
    yield make("endwin", winning=0x27FF, things={0x27FF: thing()}, used=[0x27FF])
    yield make("endwin", winning=0, things={0: thing()})


def edges_bankrupt():
    base = with_things([3, 5, 7], [3, 3, 1], h2e=4)
    yield make("bankrupt")
    yield make("bankrupt", **base)
    for g in (0, 3, 5, 9, 0x27FF):
        yield make("bankrupt", gates=g, **base)
    for st in STATES:
        yield make("bankrupt", state=st, gates=5, **base)
    for flags in (0, 4, 5, 0xFFFB, 0xFFFF, 0x8004, 1, 2, 8, 0x10, 0x100):
        for kind in (1, 3, 4):
            yield make("bankrupt", **with_things([3, 5], [kind, 3], h2e=flags))
    for dead in (1, 2, 0xFF):
        yield make("bankrupt", **with_things([3, 5], [3, 3], dead=dead, h2e=4))
    yield make("bankrupt", used=[3, 5, 7], things={3: thing(3, h2e=4), 7: thing(3, h2e=4)})
    yield make("bankrupt", used=[3, 5, 7], things={3: thing(3, id=5, h2e=4), 5: thing(3, h2e=4), 7: thing(3, h2e=4)})
    yield make("bankrupt", used=[3], things={0: thing(), 3: thing(3, h2e=4)})
    yield make("bankrupt", used=[3], things={0: thing(), 3: thing(3, h2e=4)}, gates=3)
    yield make("bankrupt", gates=3, used=[3], things={3: thing(3, h2e=4)})
    yield make("bankrupt", **with_things(list(range(1, 12)), KINDS[:11], h2e=4), gates=2)
    yield make("bankrupt", used=[0x27FF, 1], things={0x27FF: thing(3, h2e=4), 1: thing(3, h2e=4)}, gates=0x27FF)
    yield make("bankrupt", used=[3, 5], things={3: thing(3, h2e=4), 5: thing(3, h2e=4)}, fix={3: dict(index=0x10003), 5: dict(index=0x10005)})
    yield make("bankrupt", used=[3, 5], things={3: thing(3, h2e=4), 5: thing(3, h2e=4)}, fix={5: dict(index=0)})


def edges_restore():
    yield make("restore")
    for kind in KINDS:
        yield make("restore", **with_things([3, 5], [kind, 3]))
    for dead in (1, 2, 0x80, 0xFF):
        yield make("restore", **with_things([3, 5, 7], [3, 3, 3], dead=dead))
    yield make("restore", **with_things([3, 5, 7], [3, 1, 3]))
    yield make("restore", used=[3, 5, 7], things={3: thing(3), 7: thing(3)})
    yield make("restore", used=[3, 5, 7], things={3: thing(3, id=5), 5: thing(3), 7: thing(3)})
    yield make("restore", used=[3], things={0: thing(), 3: thing(3)})
    yield make("restore", used=[0x27FF, 1], things={0x27FF: thing(3), 1: thing(3)})
    yield make("restore", used=[3, 5], things={3: thing(3), 5: thing(3)}, fix={3: dict(index=0x10003), 5: dict(index=0x20005)})
    yield make("restore", used=[3, 5], things={3: thing(3), 5: thing(3)}, fix={5: dict(index=0)})
    yield make("restore", used=[3, 5], things={3: thing(3), 5: thing(3)}, heads=(None, None))


def edges_delete():
    yield make("delete")
    for kind in KINDS:
        for w0c in (0, 0x1234):
            yield make("delete", **with_things([3, 5], [kind, 3], w0c=w0c, w1c=0x5555))
    for dead in (1, 2, 0x80, 0xFF):
        yield make("delete", **with_things([3, 5, 7], [3, 1, 4], dead=dead, w0c=1, w1c=2))
    for w in WORDS:
        yield make("delete", **with_things([3, 5], [3, 1], w0c=w, w1c=w))
    yield make("delete", **with_things([3, 5, 7, 9, 11], [1, 4, 5, 6, 7], w0c=0x10))
    yield make("delete", **with_things([3, 5, 7, 9], [8, 0x12, 0x11, 0x13], w0c=0x10, w1c=0x20))
    yield make("delete", used=[3, 5, 7], things={3: thing(3, w1c=1), 7: thing(1, w0c=4)})
    yield make("delete", used=[3, 5, 7], things={3: thing(3, id=5, w1c=1), 5: thing(1, w0c=2), 7: thing(3, w1c=3)})
    yield make("delete", used=[3], things={0: thing(), 3: thing(3, w1c=9)})
    yield make("delete", used=[0x27FF, 1], things={0x27FF: thing(3, w1c=9), 1: thing(1, w0c=9)})
    yield make("delete", used=[3, 5], things={3: thing(3, w1c=9), 5: thing(3, w1c=8)}, fix={3: dict(index=0x10003), 5: dict(index=0x20005)})
    yield make("delete", used=[3, 5], things={3: thing(3, w1c=9), 5: thing(3, w1c=8)}, fix={5: dict(index=0)})


def edges_populate():
    cells = [1, 2, 129, 130, 16384, 8000, 77, 300]
    for g in (0, 2, -1, 5, 1):
        yield make("populate", q={"gates": [g]}, start=7)
    yield make("populate", start=7, valid=[5], entrance=[5])
    yield make("populate", start=1, valid=[5], entrance=[5])
    yield make("populate", start=2, valid=[5], path=[5])
    yield make("populate", start=0x80000000, valid=[5, 9], entrance=[5, 9], path=[5, 9])
    yield make("populate", start=0xFFFFFFFF, valid=cells, entrance=cells[:3], path=cells[3:6])
    yield make("populate", start=3, valid=cells, entrance=cells, path=cells)
    yield make("populate", start=3, valid=[], entrance=cells, path=cells)
    yield make("populate", start=3, valid=[5, 130], entrance=[7, 130], path=[9, 130])
    yield make("populate", start=0x7FFFFFFF, valid=[16384, 16383, 129, 128, 1], entrance=[16384, 129], path=[16383, 128, 1])
    yield make("populate", start=5, valid=[1, 2, 3], path=[1, 2, 3], entrance=[3])


def edges_online():
    pos = [0x1111, 0x2222, 0x3333, 0x4444]
    for n in (0, 1, 2, 3, -1, -5):
        yield make("online", nrides=n, pos=pos, start=9)
    for s in (0, 1, 0x80000000, 0xFFFFFFFF, 0x7FFFFFFF, 0x12345678):
        yield make("online", nrides=2, pos=pos, start=s)
    yield make("online", nrides=1, pos=[0, 0, 0, 0], start=5)
    yield make("online", nrides=1, pos=[0xFFFF, 0xFFFE, 0x8000, 0x7FFF], start=6)
    yield make("online", nrides=4, pos=[1, 2, 3, 4], start=0x55555555, state=3, persons=2)


def edges_remove():
    yield make("remove")
    yield make("remove", pcount=0)
    yield make("remove", pcount=5)
    for ids in ([7], [7, 8], [1, 2, 3, 4, 5], [0], [0xFFFF, 0, 1]):
        yield make("remove", plist=ids)
        yield make("remove", plist=ids, pcount=len(ids) + 3)
    for cnt in (0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x12345678):
        yield make("remove", plist=[4, 5], pcount=cnt)
        yield make("remove", plist=[4], pcount=cnt)


def edges_add():
    yield make("add", a=5)
    for ids in ([7], [7, 8], [1, 2, 3, 4, 5]):
        for a in (0, 1, 0xFFFF, 0x1234):
            yield make("add", a=a, plist=ids, junk=0xBEEF)
            yield make("add", a=a, plist=ids, pcount=len(ids) + 2)
    for cnt in (0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x12345678):
        yield make("add", a=9, plist=[4, 5], pcount=cnt)
        yield make("add", a=9, pcount=cnt)


# ---- random cases

def rand_thing(r, kinds):
    return thing(kind=r.choice(kinds), dead=r.choice([0, 0, 0, 1, 2, 0x80, 0xFF]),
                 w0c=r.choice([0, 0, 1, 0x12345678, 0xFFFFFFFF, 0x80000000, r.getrandbits(32)]),
                 w1c=r.choice([0, 0x55, r.getrandbits(32), 0xFFFFFFFF]),
                 h2e=r.choice([0, 4, 5, 0xFFFB, 0xFFFF, 1, 0x8004, 0x100, r.getrandbits(16)]))


def rand_rows(r, n):
    pool = list(range(1, 40)) + [0x27FF, 0x27FE, 0x100, 0x80]
    return r.sample(pool, n)


def rand_table(r, op, kinds):
    n = r.randrange(0, 9)
    rows = rand_rows(r, n)
    c = make(op)
    c["used"] = list(rows)
    for pos, row in enumerate(rows):
        if r.random() < 0.9:
            t = rand_thing(r, kinds)
            if r.random() < 0.12 and pos + 1 < len(rows):
                t["id"] = r.choice(rows[pos + 1:])      # later in the list only: the walk cannot loop
            c["things"][row] = t
        if r.random() < 0.08:
            c["fix"].setdefault(row, {})["index"] = r.choice([row | 0x10000, 0])
        if r.random() < 0.05:
            c["fix"].setdefault(row, {})["next"] = None
    if r.random() < 0.15:
        c["things"][0] = rand_thing(r, kinds)
    c["gates"] = r.choice(rows + [0, 3, 0x27FF]) if rows else r.choice([0, 3])
    c["state"] = r.choice(STATES + [r.getrandbits(32)])
    return c


def rand_case(op, r):
    if op in ("restore", "delete", "bankrupt"):
        return rand_table(r, op, [3, 3, 1, 4, 5, 6, 7, 8, 0x12, 0, 2, 9, 0x13, 0xFF])
    if op == "prev":
        c = rand_table(r, op, [1, 3, 4])
        ident = r.choice(c["used"] + [0, 3, 0x27FF]) if c["used"] else r.choice([0, 3])
        c["arg"] = thing(kind=r.choice([1, 3]), id=ident, dead=r.choice([0, 1]))
        for row in c["used"]:
            if r.random() < 0.1:
                c["fix"].setdefault(row, {})["index"] = r.choice(IDX_WORDS)
        return c
    if op == "endwin":
        c = rand_table(r, op, [1, 3, 4])
        rows = sorted(c["things"])
        c["winning"] = r.choice(rows + [0]) if rows else 0
        return c
    if op == "rfl":
        c = make(op)
        rows = rand_rows(r, r.randrange(0, 9))
        c["things"] = {row: rand_thing(r, [1, 3]) for row in rows}
        if r.random() < 0.5:
            c["things"].setdefault(1, rand_thing(r, [1]))
            lo = r.randrange(1, 0x2800)
            c["alias"] = [x for x in range(lo, 0x2800) if r.random() < 0.7]
        if r.random() < 0.3:
            c["things"][0] = rand_thing(r, [1])
        c["free"] = r.sample(range(1, 0x2800), r.randrange(0, 5))
        if r.random() < 0.3:
            c["heads"] = (None, r.choice(c["free"] or [3]))
        return c
    if op == "seed":
        return make(op, a=r.choice(WORDS + [r.getrandbits(32)] * 5), start=r.getrandbits(32))
    if op == "person":
        return make(op, persons=r.choice([0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32), r.randrange(0, 1000)]))
    if op == "isplay":
        return make(op, winning=r.choice([0, 1, r.getrandbits(16), 0xFFFF]))
    if op in ("add", "remove"):
        ids = [r.getrandbits(16) for _ in range(r.randrange(0, 7))]
        return make(op, plist=ids, a=r.getrandbits(16), junk=r.getrandbits(16),
                    pcount=r.choice([None, None, r.getrandbits(32), len(ids) + 1, 0]))
    if op == "flag":
        n = r.randrange(1, 6)
        rs = [ride(r.randrange(1, 300), r.choice([0, 1, 7, 8, 9, 0x100, r.randrange(0, 0x101)]), name="x") for _ in range(n)]
        rs[0]["id"] = 1
        bitmap = bytes(r.choice([0, 0, 0xFF, 0x55, r.getrandbits(8)]) for _ in range(32))
        return make(op, rides=rs, a=r.choice([x["id"] for x in rs] + [r.getrandbits(16)]), bitmap=bitmap, junk=r.getrandbits(16))
    if op == "convert":
        rs, numbers = [], r.sample(range(1, 0x101), r.randrange(0, 6))
        for k, num in enumerate(numbers):
            folder = r.choice(["pk\\ride%d.wad", "a\\b%d", "x\\%d", "d\\y\\z%dq", "\\%d", "p\\0%d"]) % r.randrange(0, 70000)
            rs.append(ride(k + 1, num, folder=folder))
        bitmap = bytearray(32)
        for num in numbers:
            if r.random() < 0.8:
                bitmap[(num - 1) >> 3] |= 1 << ((num - 1) & 7)
        if r.random() < 0.15:
            extra = r.randrange(1, 0x101)
            if extra not in numbers:
                bitmap[(extra - 1) >> 3] |= 1 << ((extra - 1) & 7)
        return make(op, rides=rs or [ride(1, 300)], bitmap=bytes(bitmap))
    if op == "startwin":
        rs = [ride(r.randrange(0, 0x10000), 0, name=r.choice(["End", "end", "END", "Other", "Endx", "", "eNd"])) for _ in range(r.randrange(0, 5))]
        return make(op, rides=rs, endname=r.choice(["End", "End", "end", "Other", "ENDX", "", "xyz"]), winning=r.choice([0, 0x42]),
                    q={"id": [r.choice([0x1234, 0, 0xFFFF, r.getrandbits(16)])]})
    if op == "populate":
        c = make(op, start=r.choice([0, 1, 0x80000000, 0xFFFFFFFF, r.getrandbits(32)]), q={"gates": [r.choice([1, 1, 0, 2, -1])]})
        if c["q"]["gates"][0] == 1 and r.random() < 0.9:
            c["q"]["gates"] = [r.choice([0, 2])]
        if c["q"]["gates"][0] == 1:
            valid = r.sample(range(1, 16385), r.randrange(0, 8)) + [1, 16384][:r.randrange(0, 3)]
            valid = list(dict.fromkeys(valid))
            c["valid"] = valid
            c["entrance"] = [x for x in valid + [r.randrange(1, 16385)] if r.random() < 0.4]
            c["path"] = [x for x in valid + [r.randrange(1, 16385)] if r.random() < 0.5]
        return c
    if op == "online":
        return make(op, start=r.choice([0, 0x80000000, r.getrandbits(32)]), nrides=r.choice([0, 1, 2, 3, 4, -1, -9]),
                    pos=[r.getrandbits(16) for _ in range(4)], state=r.getrandbits(32), persons=r.getrandbits(32))
    raise ValueError(op)


def tests():
    names = {
        "populate": "CWorld::PopulatePark", "online": "CWorld::StartingPopulationForOnline", "bankrupt": "CWorld::StartBankruptcySequence",
        "startwin": "CWorld::StartWinningSequence", "isplay": "CWorld::IsPlayingWinningAnim", "endwin": "CWorld::EndWinningSequence",
        "prev": "CWorld::GetPrevUsedThing", "seed": "CWorld::SeedRand", "rfl": "CWorld::ReconstructFreeList",
        "person": "CWorld::GetNewPersonIndex", "flag": "CWorld::FlagItemAsSelected", "convert": "CWorld::ConvertDLRidesUsedToIndices",
        "remove": "TbTList::RemoveHead", "add": "TbTList::AddHead", "restore": "CWorld::RestoreEngineObjects",
        "delete": "CWorld::DeleteEngineObjects",
    }
    edges = {
        "populate": edges_populate, "online": edges_online, "bankrupt": edges_bankrupt, "startwin": edges_startwin, "isplay": edges_isplay,
        "endwin": edges_endwin, "prev": edges_prev, "seed": edges_seed, "rfl": edges_rfl, "person": edges_person, "flag": edges_flag,
        "convert": edges_convert, "remove": edges_remove, "add": edges_add, "restore": edges_restore, "delete": edges_delete,
    }
    return [FnTest(names[op], ppc, host, edge=edges[op], rnd=lambda r, op=op: rand_case(op, r)) for op in names]


TESTS = tests()
