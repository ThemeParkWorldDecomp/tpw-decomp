"""Differential tests for the lobby's small leaves, part 3 (unit w59), against libtpw_unit_LobbyLeaves3.dylib
(tests/unit_LobbyLeaves3.cpp):

  src/RideDB_names.cpp    CRideDatabase::GetTypeName(id) 0x10154a10, GetSignNameA 0x10154c24, GetSignNameB 0x10154d68
  src/Game_theme.cpp      CGame::GetLocalisedThemeGateNames 0x1014a768 (and the four theme strings it compares with)
  src/hlsprite_flies.cpp  FLY_Close 0x100783ec
  src/rsse_close.cpp      RSSE_Close 0x100d3708

Six groups of cases, one operation per case:

  names     a ride database of 0 to 6 entries over a pool of 8 ride records (ids that repeat, a cache that hits, misses or points at a
            record no longer in the table, type names of 0 to 0x71 characters, sign pointers NULL or set) and one of the three
            members. The sign members first ask the name builder's table (real on both sides: the ride type numbers of the original's
            table hit it, anything else misses it), whose strings come from a fake string table.
  gate      the theme text against the four theme strings, the gate strings' starting texts, and the texts the gate ride types give.
  fly       FLY_Close with the flies initialised or not.
  rsse      RSSE_Close with the scripts system open or not, a script list and the queue of answers of KillScript.

Real on both sides: the unit's functions, the name builder (CRideNameDB::GetNameA and GetNameB), TPUniStrEq, the 16-bit C library
(wcsncpy: the original's MSL one is a Python stand-in, the host's the port's loop). The bullfrog strings are not runnable on the
PowerPC side (they are imports), so the gate strings and the theme text are objects whose virtual operator=(const Char*) is a fake that
records the call and stores the text; on the host they are the real strings with the same recording in a subclass. Fakes recording the
same events on both sides: the string table's FormatString, _wcsicmp, LbMem_Free, FLY_RemoveBase, KillScript and GAME_ASSERTMSG. The
gate function's local narrow string (constructed and destroyed, never used) shows as a count of constructor and destructor calls on
the PowerPC side and as the blocks the host's string took and gave back around the call.
"""
import ctypes as C
import struct

import calllog as CL
import test_FSSystem as FS
import test_IslandLobby as IL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyLeaves3"

MASK = 0xFFFFFFFF
TYPE_NAME, SIGN_A, SIGN_B = 0x10154A10, 0x10154C24, 0x10154D68
NAME_A, NAME_B = 0x10157250, 0x1015734C
GATE, UNI_STR_EQ, PTR_GLUE = 0x1014A768, 0x1000548C, 0x1023BAA8
FLY_CLOSE, FLY_REMOVE_BASE = 0x100783EC, 0x100781F4
RSSE_CLOSE, KILL_SCRIPT, ASSERT = 0x100D3708, 0x100D2ECC, 0x1000DDE8

RIDE_DB = 0x1037F6C8
TEXT_TABLE_POINTER = 0x1037FF18 + 0x59C
THEME_STRINGS = {"fantasy": 0x1037F10C, "hallow": 0x1037F0EC, "jungle": 0x1037F0CC, "space": 0x1037F0AC}
GAME = 0x1037F134
FLY_ACTIVE, FLY_INIT, FLY_TABLE, FLY_BASES = 0x102DAAF4, 0x102DAAF8, 0x102DAAFC, 0x102DA964
RSSE_STATE, RSSE_MALLOCS = 0x102F7BEC, 0x102F7BDC
ASSERT_TEXT = "RSSE: Arrooga - Something malloced but not freed - bad boy - fix it !"

POOL = 8                      # ride records of a case
BUFFER_UNITS = 0x118          # the sign members' output buffer (a case asks for at most 0x100)
LAB_TABLE, LAB_GATE1, LAB_GATE2, LAB_SCRIPT, LAB_SIN = 80, 1, 2, 10, 20
GATE_TYPES = {"fantasy": 4601, "hallow": 2601, "jungle": 1601, "space": 3601}

STRINGS = {}                  # the string table of the case in hand (number -> text)
BASE_STRINGS = {}
TABLE_IDS = []                # the ride type numbers the name builder's table has (the rows before its terminator)
TABLE_ROWS = {}

LONG = "Long text " + "y" * 120
TEXTS = ["", "a", "Roller", "Twenty-nine characters long tex", "x" * 31, "x" * 32, "x" * 33, LONG, "é€ñ mixed", "-"]
TYPE_TEXTS = ["", "a", "Roller Coaster", "t" * 0x70, "u" * 0x71, "v" * 5, "w" * 0x20]
IDS = [0, 1, 2, 3, 5, 0x7FFF, 0x8000, 0xFFFF, 0x1234, 5000]
SIZES = [0, 1, 5, 0x20, 0x100]


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def w16(e, a, v):
    e.write(a, struct.pack(">H", v & 0xFFFF))


def wide(text):
    return IL.wide_bytes(text)


def units(text):
    return (C.c_uint16 * max(len(text), 1))(*[ord(ch) for ch in text])


def setup(runner):
    e = runner.emu
    IL.load_name_table(e)
    BASE_STRINGS.update(IL.base_strings())
    STRINGS.update(BASE_STRINGS)
    TABLE_IDS[:] = []
    TABLE_ROWS.clear()
    for row in IL.NAME_TABLE:
        if row[0] == 0:
            break
        TABLE_IDS.append(row[0])
        TABLE_ROWS[row[0]] = row
    lib = runner.lib
    lib.tpw_ll_string.argtypes = [C.c_int32, C.c_void_p, C.c_int32]
    for sid, text in BASE_STRINGS.items():
        lib.tpw_ll_string(sid, units(text), len(text))
    T.persist_alloc(e, 16)
    # the gate strings' vtable: operator=(const Char*) is slot 0x20 (a fake that records and stores the text)
    vt = T.persist_alloc(e, 0x30)

    def assign(emu):
        this, text = emu.r(3), FS.wide_read(emu, emu.r(4), 4000)
        emu.cl.rec("Assign", (emu.cl.lab(this),), (CL.narrow(text),))
        CL.wds_set(emu, this, text)
        w32(emu, this, vt)

    w32(e, vt + 0x20, T.fake_function(e, assign))
    e.ll_gate_vtable = vt


def host_strings(lib, overrides):
    """Makes the host's string table the case's: the base strings with the case's own over them (and back afterwards)."""
    for sid, text in overrides.items():
        lib.tpw_ll_string(sid, units(text), len(text))


def host_strings_restore(lib, overrides):
    for sid in overrides:
        text = BASE_STRINGS.get(sid, "")
        lib.tpw_ll_string(sid, units(text), len(text))


def case_strings(overrides):
    STRINGS.clear()
    STRINGS.update(BASE_STRINGS)
    STRINGS.update(overrides)


# ======================================================================= fakes on the PowerPC side


def string_table_stubs(S):
    def wcsncpy(e):
        dst, src, count = e.r(3), e.r(4), e.r(5)
        text = FS.wide_read(e, src, 4000)[:count]
        if count:
            e.write(dst, wide(text)[:-2].ljust(2 * count, b"\0"))
        e.set_r(3, dst)

    def format_string(e):
        L = e.cl
        this, out, size, sid = e.r(3), e.r(4), e.r(5), e.r(6)
        L.rec("FormatString", (L.lab(this), size, sid))
        text = STRINGS.get(sid, "") if sid < 2048 else STRINGS[0]
        if size == 0:
            e.set_r(3, 0)
            return
        kept = text[:size - 1]
        e.write(out, wide(kept))
        e.set_r(3, len(kept))

    S.imp("wcsncpy", wcsncpy)
    S.imp("FormatString__8TbITableCFPwUiUle", format_string)


def icmp(a, b):
    a = "".join(chr(ord(c) + 32) if "A" <= c <= "Z" else c for c in a)
    b = "".join(chr(ord(c) + 32) if "A" <= c <= "Z" else c for c in b)
    return 0 if a == b else (-1 if a < b else 1)


def wcsicmp_stub(e):
    a, b = FS.wide_read(e, e.r(3), 4000), FS.wide_read(e, e.r(4), 4000)
    e.cl.rec("_wcsicmp", (), (CL.narrow(a), CL.narrow(b)))
    e.set_r(3, T.u32(icmp(a, b)))


# ======================================================================= names


def rec_text(r):
    return r["t"]


def lookup(c):
    """The record (pool number) the database's lookup gives, or None: the model the generator uses to keep to cases the original
    can run (a record that is not there is read at address 0)."""
    if c["id"] == c["cache"][0]:
        return c["cache"][1] if c["cache"][1] >= 0 else None
    for k in range(c["count"]):
        if c["recs"][c["order"][k]]["id"] == c["id"]:
            return c["order"][k]
    return None


def runnable(c):
    if c["op"] == 0:
        return True
    if c["id"] in TABLE_ROWS:
        return True
    k = lookup(c)
    if k is None:
        return False
    sign = c["recs"][k]["a" if c["op"] == 1 else "b"]
    return sign is not None or c["size"] == 0


def make_recs(r):
    return [{"id": r.choice(IDS + TABLE_IDS[:6]), "t": r.choice(TYPE_TEXTS + [LONG[:r.randrange(0x72)]]),
             "a": r.choice([None] + TEXTS), "b": r.choice([None] + TEXTS)} for _ in range(POOL)]


def names_case(op, id, size, recs, count, order, cache, strings=None):
    return {"op": op, "id": id, "size": size, "recs": recs, "count": count, "order": order, "cache": cache,
            "strings": strings or {}}


def fixed_recs(ids, a="Sign A", b="Sign B"):
    recs = []
    for k in range(POOL):
        rid = ids[k % len(ids)] if ids else 0
        recs.append({"id": rid, "t": "Type %d" % k, "a": a, "b": b})
    return recs


def names_rnd(op):
    def rnd(r):
        recs = make_recs(r)
        count = r.randrange(7)
        order = [r.randrange(POOL) for _ in range(6)]
        roll = r.random()
        if roll < 0.35:
            cache = (0, -1)
        elif roll < 0.7:
            cache = (r.choice(IDS), r.randrange(-1, POOL))
        else:
            k = r.randrange(POOL)
            cache = (recs[k]["id"], k if r.random() < 0.7 else r.randrange(POOL))
        roll = r.random()
        if roll < 0.5 and count:
            id = recs[order[r.randrange(count)]]["id"]
        elif roll < 0.75:
            id = r.choice(IDS)
        elif roll < 0.9:
            id = r.choice(TABLE_IDS)
        else:
            id = r.getrandbits(16)
        size = r.choice(SIZES) if r.random() < 0.7 else r.randrange(0x101)
        c = names_case(op, id, size, recs, count, order, cache)
        for _ in range(8):
            if runnable(c):
                return c
            if c["count"] and r.random() < 0.7:
                c["id"] = recs[order[r.randrange(c["count"])]]["id"]
            else:
                c["size"] = 0
        c["id"] = r.choice(TABLE_IDS)
        return c
    return rnd


def names_edges(op):
    def edges():
        # a database of the ids 1, 2, 2, 7 over the pool, with the cache at its start
        recs = fixed_recs([1, 2, 2, 7, 0x7FFF, 0x8000, 0xFFFF, 0])
        order = [0, 1, 2, 3, 4, 5]
        for id in IDS + [7, 2]:
            for count in (0, 1, 2, 4, 6):
                for cache in ((0, -1), (2, 1), (2, 7), (1, 0), (id, -1)):
                    c = names_case(op, id, 0x20, recs, count, order, cache)
                    if runnable(c):
                        yield c
        # type names of 0, 1 and 0x71 characters; sign texts shorter, equal and longer than the size; every size
        for t in TYPE_TEXTS:
            recs2 = fixed_recs([5000, 5001], "Sign A", "Sign B")
            recs2[0]["t"] = t
            yield names_case(op, 5000, 0x20, recs2, 2, [0, 1, 0, 0, 0, 0], (0, -1))
        for text in TEXTS:
            for size in SIZES + [2, 31, 32, 33]:
                recs3 = fixed_recs([5000], text, text)
                c = names_case(op, 5000, size, recs3, 1, [0, 0, 0, 0, 0, 0], (0, -1))
                yield c
        # NULL sign pointers: only a size of 0 may be asked
        recs4 = fixed_recs([5000])
        recs4[0]["a"] = None
        recs4[0]["b"] = None
        yield names_case(op, 5000, 0, recs4, 1, [0] * 6, (0, -1))
        # the name table: a hit, a miss, ids that are in it with an empty second string, id 0 (which ends it)
        for id in TABLE_IDS[:5] + TABLE_IDS[-3:] + [2601, 4601, 3601, 1601]:
            for size in (0, 1, 0x20):
                yield names_case(op, id, size, recs, 4, order, (0, -1))
        # triage 2026-10-13a (doubtful 2): a name table string that ends with a hyphen: GetSignNameA asks the table without the trim
        # (the hyphen stays in the sign text), GetSignNameB asks for the second string
        for id in TABLE_IDS[:5] + [4601, 2601, 3601, 1601]:
            row = TABLE_ROWS[id]
            for text in ("Roller-", "-", "a-", "Ends--", "x", "-x"):
                for size in (0x40, 3):
                    yield names_case(op, id, size, recs, 4, order, (0, -1), strings={row[1]: text, row[2]: text[::-1]})
                    yield names_case(op, id, size, recs, 4, order, (0, -1), strings={row[1]: text, row[2]: text})
        # a cache that points at a record no longer in the table
        yield names_case(op, 9, 5, recs, 0, order, (9, 6))
        yield names_case(op, 9, 5, [dict(x, id=9) for x in recs], 3, order, (9, 6))
        yield names_case(op, 9, 5, [dict(x, id=9) for x in recs], 3, order, (3, 6))
        yield names_case(op, 7, 5, recs, 6, order, (7, 3))
    return edges


ZERO_DB = bytes(0x738)


def ppc_names(e, c):
    case_strings(c["strings"])
    e.cl = L = CL.PpcLog({}, {})
    S = CL.Stubs()
    S.real.update({TYPE_NAME, SIGN_A, SIGN_B, NAME_A, NAME_B})
    string_table_stubs(S)
    table = e.alloc(16)
    L.labels[table] = LAB_TABLE
    recs = []
    for k, rec in enumerate(c["recs"]):
        addr = e.alloc(0x800, b"\xa5" * 0x800)
        w16(e, addr, rec["id"])
        e.write(addr + 0x70C, wide(rec["t"]).ljust(0xE4, b"\0"))
        sign_a = e.alloc(2 * len(rec["a"]) + 2, wide(rec["a"])) if rec["a"] is not None else 0
        sign_b = e.alloc(2 * len(rec["b"]) + 2, wide(rec["b"])) if rec["b"] is not None else 0
        w32(e, addr + 0x7F4, sign_a)
        w32(e, addr + 0x7F8, sign_b)
        recs.append(addr)
    saved = (e.read(RIDE_DB, 0x738), e.read(TEXT_TABLE_POINTER, 4))
    e.write(RIDE_DB, ZERO_DB)
    for i in range(6):
        w32(e, RIDE_DB + 4 * i, recs[c["order"][i]])   # (the entries after the count are stale pointers)
    w32(e, RIDE_DB + 0x528, c["count"])
    w16(e, RIDE_DB + 0x730, c["cache"][0])
    w32(e, RIDE_DB + 0x734, recs[c["cache"][1]] if c["cache"][1] >= 0 else 0)
    w32(e, TEXT_TABLE_POINTER, table)
    buffer = e.alloc(2 * BUFFER_UNITS, b"\xa5" * (2 * BUFFER_UNITS))
    saved_stubs = S.activate(e)
    try:
        junk = 0xBEEF
        arg = ((c["id"] & 0xFFFF) << 16) | junk
        info, text = [-1, 0, 0], ""
        if c["op"] == 0:
            answer = T.call_ppc(e, TYPE_NAME, [RIDE_DB, arg])[0]
            if answer == 0:
                info[0] = -1
            else:
                info[0] = next((k for k, a in enumerate(recs) if a + 0x70C == answer), -2)
                text = FS.wide_read(e, answer, 0x72)
        else:
            T.call_ppc(e, SIGN_A if c["op"] == 1 else SIGN_B, [RIDE_DB, arg, buffer, c["size"]])
        info[1] = struct.unpack(">H", e.read(RIDE_DB + 0x730, 2))[0]
        cached = e.u32(RIDE_DB + 0x734)
        info[2] = -1 if cached == 0 else next((k for k, a in enumerate(recs) if a == cached), -2)
        out = list(struct.unpack(">%dH" % BUFFER_UNITS, e.read(buffer, 2 * BUFFER_UNITS)))
        return {"info": info, "text": text, "buffer": out, "log": L.log}
    finally:
        CL.Stubs.deactivate(e, saved_stubs)
        e.write(RIDE_DB, saved[0])
        e.write(TEXT_TABLE_POINTER, saved[1])


def host_names(lib, c):
    lib.tpw_ll_ride.argtypes = [C.c_int32, C.c_int32, C.c_void_p, C.c_int32, C.c_int32, C.c_void_p, C.c_int32, C.c_int32,
                                C.c_void_p, C.c_int32]
    lib.tpw_ll_db.argtypes = [C.c_int32, C.c_void_p, C.c_int32, C.c_int32]
    lib.tpw_ll_names.argtypes = [C.c_int32, C.c_int32, C.c_void_p, C.c_uint32, C.c_void_p, C.c_void_p]
    CL.host_begin(lib, {})
    lib.tpw_ll_reset()
    host_strings(lib, c["strings"])
    try:
        for k, rec in enumerate(c["recs"]):
            a, b = rec["a"], rec["b"]
            lib.tpw_ll_ride(k, rec["id"], units(rec["t"]), len(rec["t"]), int(a is not None), units(a or ""), len(a or ""),
                            int(b is not None), units(b or ""), len(b or ""))
        order = (C.c_int32 * 6)(*c["order"])
        lib.tpw_ll_db(c["count"], order, c["cache"][0], c["cache"][1])
        buffer = (C.c_uint16 * BUFFER_UNITS)(*([0xA5A5] * BUFFER_UNITS))
        text = (C.c_uint16 * 0x72)()
        info = (C.c_int32 * 3)()
        lib.tpw_ll_names(c["op"], c["id"], buffer, c["size"], text, info)
        log = CL.host_end(lib)
    finally:
        host_strings_restore(lib, c["strings"])
    chars = []
    for unit in text:
        if unit == 0:
            break
        chars.append(chr(unit))
    return {"info": list(info), "text": "".join(chars), "buffer": list(buffer), "log": log}


# ======================================================================= gate


def gate_case(theme, g1, g2, strings=None):
    return {"theme": theme, "g1": g1, "g2": g2, "strings": strings or {}}


def gate_strings(r):
    out = {}
    for name, type_id in GATE_TYPES.items():
        row = TABLE_ROWS[type_id]
        for sid in row[1:]:
            if sid > 1 and r.random() < 0.5:
                out[sid] = r.choice(TEXTS + ["q" * r.randrange(0, 50)])
    return out


def toggle_case(r, text):
    return "".join(ch.upper() if r.random() < 0.5 else ch.lower() for ch in text)


def gate_rnd(r):
    roll = r.random()
    base = r.choice(list(GATE_TYPES))
    if roll < 0.55:
        theme = toggle_case(r, base)
    elif roll < 0.7:
        theme = base + r.choice(["s", " ", "x", "é"])
    elif roll < 0.8:
        theme = base[:r.randrange(len(base))]
    elif roll < 0.9:
        theme = r.choice(["", "lobby", "alpha", "Hollywood", "space2", "ā"])
    else:
        theme = "".join(chr(r.randrange(0x20, 0x7F)) for _ in range(r.randrange(8)))
    return gate_case(theme, r.choice(TEXTS), r.choice(TEXTS), gate_strings(r) if r.random() < 0.6 else None)


def gate_edges():
    for theme in ["fantasy", "Fantasy", "FANTASY", "hallow", "HALLOW", "jungle", "Jungle", "space", "SPACE", "", "spaces", "fantas",
                  "lobby", "alpha", "spac", "fantasyx", "hallo w", "ā"]:
        for g1, g2 in (("", ""), ("start", "end"), (LONG, "x"), ("y", LONG), ("x" * 32, "z" * 33)):
            yield gate_case(theme, g1, g2)
    for name, type_id in GATE_TYPES.items():
        row = TABLE_ROWS[type_id]
        for text in ("", "short", "x" * 31, "x" * 32, "x" * 40, "é€"):
            yield gate_case(name, "", "", {row[1]: text, row[2]: text[::-1]})


def ppc_gate(e, c):
    case_strings(c["strings"])
    e.cl = L = CL.PpcLog({}, {})
    counts = [0, 0]
    S = CL.Stubs()
    S.real.update({GATE, UNI_STR_EQ, SIGN_A, SIGN_B, NAME_A, NAME_B, PTR_GLUE})
    string_table_stubs(S)
    S.imp("_wcsicmp__10NS_MacDozeFPCwPCw", wcsicmp_stub)
    S.imp("__ct__26TbDynamicStringTemplate<c>Fv", lambda emu: counts.__setitem__(0, counts[0] + 1))
    S.imp("__dt__26TbDynamicStringTemplate<c>Fv", lambda emu: counts.__setitem__(1, counts[1] + 1))
    table = e.alloc(16)
    L.labels[table] = LAB_TABLE
    theme = e.alloc(20)
    CL.wds_set(e, theme, c["theme"])
    gates = []
    for text, label in ((c["g1"], LAB_GATE1), (c["g2"], LAB_GATE2)):
        obj = e.alloc(20)
        CL.wds_set(e, obj, text)
        w32(e, obj, e.ll_gate_vtable)
        L.labels[obj] = label
        gates.append(obj)
    saved = {a: e.read(a, 20) for a in list(THEME_STRINGS.values())}
    saved_db = (e.read(RIDE_DB, 0x738), e.read(TEXT_TABLE_POINTER, 4))
    for text, a in THEME_STRINGS.items():
        CL.wds_set(e, a, text)
    e.write(RIDE_DB, ZERO_DB)
    w32(e, TEXT_TABLE_POINTER, table)
    # the function's frame is not cleared: the test clears the stack under it, as the host's scrub does
    sp = T.ppcemu.STACK_BASE + T.ppcemu.STACK_SIZE - 0x1000 - 0x400
    saved_stack = e.read(sp - 0x2000, 0x2000)
    e.write(sp - 0x2000, bytes(0x2000))
    saved_stubs = S.activate(e)
    try:
        T.call_ppc(e, GATE, [GAME, theme, gates[0], gates[1]])
        out = {"log": L.log, "ctor_dtor": counts}
        for name, obj in (("g1", gates[0]), ("g2", gates[1])):
            out[name] = FS.wide_read(e, e.u32(obj + 8), 4000)
            out[name + "_length"] = T.s32(e.u32(obj + 0xC))   # (the fake string keeps its length there)
        return out
    finally:
        CL.Stubs.deactivate(e, saved_stubs)
        e.write(sp - 0x2000, saved_stack)
        e.write(RIDE_DB, saved_db[0])
        e.write(TEXT_TABLE_POINTER, saved_db[1])
        for a, data in saved.items():
            e.write(a, data)


def host_gate(lib, c):
    lib.tpw_ll_gate.argtypes = [C.c_void_p, C.c_int32, C.c_void_p, C.c_int32, C.c_void_p, C.c_int32, C.c_int32, C.c_void_p,
                                C.c_void_p, C.c_void_p]
    CL.host_begin(lib, {})
    lib.tpw_ll_reset()
    host_strings(lib, c["strings"])
    try:
        out1 = (C.c_uint16 * 0x200)()
        out2 = (C.c_uint16 * 0x200)()
        out = (C.c_int32 * 4)()
        lib.tpw_ll_gate(units(c["theme"]), len(c["theme"]), units(c["g1"]), len(c["g1"]), units(c["g2"]), len(c["g2"]), 1, out1,
                        out2, out)
        log = CL.host_end(lib)
    finally:
        host_strings_restore(lib, c["strings"])
    return {"log": log, "ctor_dtor": [out[2], out[3]], "g1": "".join(chr(u) for u in out1[:out[0]]),
            "g2": "".join(chr(u) for u in out2[:out[1]]), "g1_length": out[0], "g2_length": out[1]}


# ======================================================================= flies


def fly_case(init, table, active, seed):
    return {"init": init, "table": table, "active": active, "seed": seed}


def fly_edges():
    for init in (0, 1, -1, 2, 0x100, 0x7FFFFFFF, -0x80000000):
        for table in (0, 1):
            for active in (0, 1, 7):
                yield fly_case(init, table, active, 0x1000)


def fly_rnd(r):
    return fly_case(r.choice([0, 1, r.getrandbits(32) - 0x80000000, r.randrange(-3, 4)]), r.randrange(2),
                    r.choice([0, 1, r.getrandbits(32) - 0x80000000]), r.getrandbits(20))


def ppc_fly(e, c):
    e.cl = L = CL.PpcLog({}, {})
    S = CL.Stubs()
    S.real.update({FLY_CLOSE})
    S.internal(FLY_REMOVE_BASE, CL.recorder("FLY_RemoveBase", "i"))
    S.imp("LbMem_Free__FPv", CL.recorder("LbMem_Free", "L"))
    table = e.alloc(16)
    L.labels[table] = LAB_SIN
    saved = e.read(FLY_BASES, FLY_TABLE + 4 - FLY_BASES)
    w32(e, FLY_INIT, c["init"])
    w32(e, FLY_TABLE, table if c["table"] else 0)
    w32(e, FLY_ACTIVE, c["active"])
    e.write(FLY_BASES, b"".join(struct.pack(">I", (c["seed"] + i) & MASK) for i in range(100)))
    saved_stubs = S.activate(e)
    try:
        T.call_ppc(e, FLY_CLOSE, [])
        words = list(struct.unpack(">100I", e.read(FLY_BASES, 400)))
        return {"log": L.log, "state": [T.s32(e.u32(FLY_INIT)), L.lab(e.u32(FLY_TABLE)), T.s32(e.u32(FLY_ACTIVE))],
                "bases": [T.s32(w) for w in words]}
    finally:
        CL.Stubs.deactivate(e, saved_stubs)
        e.write(FLY_BASES, saved)


def host_fly(lib, c):
    lib.tpw_ll_fly.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_void_p]
    CL.host_begin(lib, {})
    out = (C.c_int32 * 103)()
    lib.tpw_ll_fly(c["init"], c["table"], c["active"], c["seed"], out)
    return {"log": CL.host_end(lib), "state": list(out[:3]), "bases": list(out[3:])}


# ======================================================================= scripts


def rsse_case(init, f04, f08, count, head, queue, mallocs):
    return {"init": init, "f04": f04, "f08": f08, "count": count, "head": head, "queue": queue, "mallocs": mallocs}


def rsse_edges():
    for init in (0, 1, 2, -1):
        for mallocs in (0, 1, -1, 0x10000):
            yield rsse_case(init, 0, 1, 3, 0, [1, 2, -1], mallocs)
    for head, queue in ((-1, [-1]), (0, [-1]), (0, [1, -1]), (0, [1, 2, -1]), (3, [1, 0, 2, -1]), (7, [-1]), (2, [2, 2, -1])):
        for mallocs in (0, 5):
            yield rsse_case(1, 0, 1, len(queue), head, queue, mallocs)
    yield rsse_case(0, 9, 9, 4, 0, [1, -1], 0)
    yield rsse_case(1, 5, 6, 0, 0, [], 0)


def rsse_rnd(r):
    queue = [r.randrange(-1, 8) for _ in range(r.randrange(0, 6))] + [-1]
    return rsse_case(r.choice([0, 1, 1, r.randrange(-3, 4), r.getrandbits(32) - 0x80000000]), r.choice([0, r.getrandbits(32) - 0x80000000]),
                     r.choice([1, r.getrandbits(32) - 0x80000000]), r.randrange(0, 6), r.randrange(-1, 8), queue,
                     r.choice([0, 0, 1, r.getrandbits(32) - 0x80000000]))


def ppc_rsse(e, c):
    scripts = [e.alloc(16) for _ in range(8)]
    fixed = {a: LAB_SCRIPT + k for k, a in enumerate(scripts)}
    e.cl = L = CL.PpcLog(fixed, {"KillScript": c["queue"]})
    S = CL.Stubs()
    S.real.update({RSSE_CLOSE})

    def kill(emu):
        emu.cl.rec("KillScript", (emu.cl.lab(emu.r(3)), T.s32(emu.r(4))))
        v = emu.cl.ret("KillScript", -1)
        emu.set_r(3, scripts[v] if v >= 0 else 0)

    S.internal(KILL_SCRIPT, kill)

    def assert_stub(emu):
        emu.cl.rec("GAME_ASSERTMSG", (T.s32(emu.r(3)), T.s32(emu.u32(RSSE_STATE))), (emu.cstr(emu.r(4))[:159],))

    S.internal(ASSERT, assert_stub)
    saved = e.read(RSSE_MALLOCS, RSSE_STATE + 0x14 - RSSE_MALLOCS)
    w32(e, RSSE_STATE, c["init"])
    w32(e, RSSE_STATE + 4, c["f04"])
    w32(e, RSSE_STATE + 8, c["f08"])
    w32(e, RSSE_STATE + 0xC, c["count"])
    w32(e, RSSE_STATE + 0x10, scripts[c["head"]] if c["head"] >= 0 else 0)
    w32(e, RSSE_MALLOCS, c["mallocs"])
    saved_stubs = S.activate(e)
    try:
        T.call_ppc(e, RSSE_CLOSE, [])
        state = [T.s32(e.u32(RSSE_STATE + 4 * i)) for i in range(4)] + [L.lab(e.u32(RSSE_STATE + 0x10)), T.s32(e.u32(RSSE_MALLOCS))]
        return {"log": L.log, "state": state}
    finally:
        CL.Stubs.deactivate(e, saved_stubs)
        e.write(RSSE_MALLOCS, saved)


def host_rsse(lib, c):
    lib.tpw_ll_rsse.argtypes = [C.c_int32] * 6 + [C.c_void_p]
    CL.host_begin(lib, {}, {"KillScript": c["queue"]})
    out = (C.c_int32 * 6)()
    lib.tpw_ll_rsse(c["init"], c["f04"], c["f08"], c["count"], c["head"], c["mallocs"], out)
    return {"log": CL.host_end(lib), "state": list(out)}


TESTS = [
    FnTest("CRideDatabase::GetTypeName(id) 0x10154a10", ppc_names, host_names, edge=names_edges(0), rnd=names_rnd(0)),
    FnTest("CRideDatabase::GetSignNameA 0x10154c24", ppc_names, host_names, edge=names_edges(1), rnd=names_rnd(1)),
    FnTest("CRideDatabase::GetSignNameB 0x10154d68", ppc_names, host_names, edge=names_edges(2), rnd=names_rnd(2)),
    FnTest("CGame::GetLocalisedThemeGateNames 0x1014a768", ppc_gate, host_gate, edge=gate_edges, rnd=gate_rnd),
    FnTest("FLY_Close 0x100783ec", ppc_fly, host_fly, edge=fly_edges, rnd=fly_rnd),
    FnTest("RSSE_Close 0x100d3708", ppc_rsse, host_rsse, edge=rsse_edges, rnd=rsse_rnd),
]
