"""Unit w123b (main app), against libtpw_unit_WorldGetters.dylib (tests/unit_WorldGetters.cpp):

  src/game/World.cpp      CWorld::GetParkAnalyser const (0x1013eb38) and non-const (0x1013ebc8), GetResearchLab const (0x1013ec58) and non-const
                             (0x1013ece8), GetWeather (0x1013ed74), GetBankAccount (0x1013edfc), GetChallengeHQ (0x1013ee88)
  src/game/people/ParkTime.cpp     CParkTime::GetFunnyTime (0x101127d0)
  src/game/park/analyser.cpp  CParkAnalyser::Init (0x100e4a44)
  src/game/park/ChallengeHQ.cpp   CChallengeHQ::Init (0x100f6904)

The ten run as the original on the PowerPC side (the signed 64 bit divide helper 0x10237e1c too) and as src/ on the host. Every other callee is a fake that
records the same event on both sides: the assertion and the trace, the world accessor, TbTimeStamp::operator+ and GetTime, the message centre (Subscribe,
operator new, the header block), the analyser cell, the challenge constructor, AddChallenge, ClearInvalidFollowups and the balance table's Init, Load, UnInit.

The world is not a block of its Mac size: the 0x80 bytes at world + 0x1da6f0 are real memory and the world pointer is that block's address minus 0x1da6f0
on both sides. Memory the code owns is built from VALUES (a fill of random bytes, with the fields the code reads or writes packed in each side's byte
order), compared by value, and every other byte must still hold its fill. Pointers are compared as call-log labels.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_WorldGetters"

MASK = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF
TABLE, MAIN, G_CENTRE = 0x1034CBFC, 0x102B4568, 0x1037F6C4
ASSERT, HELPER = 0x1000DDE8, 0x10237E1C
FUNNY, ANALYSER, HQ = 0x101127D0, 0x100E4A44, 0x100F6904
GET_ADDR = [0x1013EB38, 0x1013EBC8, 0x1013EC58, 0x1013ECE8, 0x1013ED74, 0x1013EDFC, 0x1013EE88]
GET_NAMES = ["CWorld::GetParkAnalyser const", "CWorld::GetParkAnalyser", "CWorld::GetResearchLab const", "CWorld::GetResearchLab",
             "CWorld::GetWeather", "CWorld::GetBankAccount", "CWorld::GetChallengeHQ"]
GET_FIELD = [0x1DA720, 0x1DA720, 0x1DA722, 0x1DA722, 0x1DA724, 0x1DA726, 0x1DA736]
OWN = set(GET_ADDR) | {FUNNY, ANALYSER, HQ, HELPER}
TAIL0 = 0x1DA6F0                       # the world byte the tail block starts at
TURN_WORD = (0x1DA70C - TAIL0) // 4    # the world's turn, as a word of the tail
ROW_TEXT = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"
TRACE_TEXT = "!!!!!\nLoading challenges.sam went horribly wrong! Shout at Bjarne!\n!!!!!"
L_WORLD, L_CLOCK, L_BLOCK, L_CENTRE, L_THING, L_CELL = 1, 3, 4, 5, 10, 30

ROWS = [0, 1, 2, 3, 4, 5, 6, 7, 0x400, 0x13FF, 0x1400, 0x27FE, 0x27FF]    # the thing table rows a case sets
ID_FIELDS = list(range(0x1DA718, 0x1DA738, 2))                              # the world's sixteen halfwords of thing ids
U32S = [0, 1, 2, 3, 4, 5, 15000, 0x7FFFFFFF, 0x80000000, MASK]
TICKS = [0, 1, MASK, 0x100000000, 0x7FFFFFFFFFFFFFFF, 0x8000000000000000, M64, 0x01B21DD213814000]
FILLS = [0, 0xA5, 0xFF, "rand"]
ANALYSER_SIZE = 0x21C48 + 0x40
HQ_SIZE = 0x3E4 + 0x40


def s32(v):
    return T.s32(v)


def s64(v):
    v &= M64
    return v - (1 << 64) if v & (1 << 63) else v


def qn(q):
    """The queues as signed 64 bit numbers (the host's queues are int64; the PowerPC side masks what it uses)."""
    return {k: [s64(v) for v in vs] for k, vs in q.items()}


def setup(runner):
    T.install_std_stubs(runner.emu)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def be64(e, a):
    return int.from_bytes(e.read(a, 8), "big")


def hash_values(values):
    h = 2166136261
    for v in values:
        h = ((h ^ (v & MASK)) * 16777619) & MASK
    return h


def lib_addr(lib, which):
    lib.tpw_wg_addr.restype = C.c_void_p
    return lib.tpw_wg_addr(which)


# ------------------------------------------------------------------ blocks built from values


def make_fill(seed, kind, size):
    r = random.Random(seed)
    return bytearray(r.randbytes(size)) if kind == "rand" else bytearray([kind]) * size


def sided(fill, fields, init, little):
    """The block of a side: the common fill, with each field (offset, width) holding its value (init, or the PowerPC reading of the fill)."""
    buf = bytearray(fill)
    for o, w in fields:
        v = init.get(o)
        if v is None:
            v = int.from_bytes(fill[o:o + w], "big")
        buf[o:o + w] = (v & ((1 << (8 * w)) - 1)).to_bytes(w, "little" if little else "big")
    return bytes(buf)


def decode(buf, fields, little):
    return [int.from_bytes(buf[o:o + w], "little" if little else "big") for o, w in fields]


def gaps(fields, size):
    """The byte ranges outside the fields."""
    out, at = [], 0
    for o, w in sorted(fields):
        if o > at:
            out.append((at, o))
        at = max(at, o + w)
    if at < size:
        out.append((at, size))
    return out


def moved(buf, fill, ranges):
    """The ranges outside the fields that no longer hold the fill."""
    return [a for a, b in ranges if bytes(buf[a:b]) != bytes(fill[a:b])]


class Base(FnTest):
    """A case builds both sides from the same description; the answer, the observed state and the call logs must agree."""

    def run_case(self, runner, case):
        runner.emu.reset_scratch()
        try:
            p = self.ppc(runner.emu, case)
        except ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = self.host(runner.lib, case)
        difference = CL.first_difference(p.pop("log"), h.pop("log"))
        errors = T.same_values(p, h, "", False)
        for side, report in (("ppc", p), ("host", h)):
            if report.get("untouched"):
                errors.append(f"{side}: a byte outside the stores of the function changed: {report['untouched'][:4]}")
        if difference:
            errors.append(difference)
        return errors


# ------------------------------------------------------------------ the seven getters


def tail_bytes(c, k, little):
    r = random.Random(c["seed"] * 2 + k)
    halves = [r.getrandbits(16) for _ in range(64)]
    for off, v in c["ids"][k].items():
        halves[(off - TAIL0) // 2] = v
    return struct.pack(("<" if little else ">") + "64H", *halves)


def mk_get(op, r, **kw):
    blocks = list(range(1, 9))
    r.shuffle(blocks)
    spare = r.sample(ROWS, 8)
    rows = {row: 0 for row in ROWS}
    rows.update({row: blocks[i] for i, row in enumerate(spare)})
    if r.random() < 0.5:
        rows[0] = 0
    ids = [{off: r.choice(ROWS) for off in ID_FIELDS} for _ in range(2)]
    c = dict(op=op, seed=r.getrandbits(32), world=r.randrange(2), ids=ids, rows=rows,
             f04={row: [r.getrandbits(32) for _ in range(4)] for row in ROWS}, mut=None)
    c.update(kw)
    return c


def mutation(r, c, with_id=True, with_rows=True):
    """The assertion's change of the world: another id in the field the getter reads, and other things in some rows."""
    read = c["ids"][c["world"]][GET_FIELD[c["op"]]]
    rows = []
    if with_rows:
        rows = [(read if r.random() < 0.7 else r.choice(ROWS), r.randrange(0, 9)) for _ in range(r.randrange(1, 4))]
    return dict(id=r.choice(ROWS) if with_id else read, rows=rows)


def get_queues(c):
    m = c["mut"]
    if m is None:
        return {}
    return {"amut": [1], "aoff": [GET_FIELD[c["op"]]], "aid": [m["id"]], "arows": [len(m["rows"])],
            "arow": [row for row, _ in m["rows"]] or [0], "ablk": [blk for _, blk in m["rows"]] or [0]}


class GetterTest(Base):
    def __init__(self, op):
        super().__init__(GET_NAMES[op], self.ppc_case, self.host_case, edge=lambda: edges_get(op), rnd=lambda r: rnd_get(op, r))
        self.op = op

    def ppc_case(self, e, c):
        L = CL.PpcLog({}, qn(get_queues(c)))
        e.cl = L
        tails = [e.alloc(0x80, tail_bytes(c, k, False)) for k in range(2)]
        things = [e.alloc(0x20, b"\xa5" * 0x20) for _ in range(8)]
        for i, a in enumerate(things):
            L.labels[a] = L_THING + i
        for k, a in enumerate(tails):
            L.labels[a - TAIL0] = L_WORLD + k
        for row in ROWS:
            b = c["rows"][row]
            e.write(TABLE + 0x14 * row, struct.pack(">5I", things[b - 1] if b else 0, *c["f04"][row]))

        def assertion(emu):
            L.rec("GAME_ASSERTMSG", [s32(emu.r(3))], [emu.cstr(emu.r(4))])
            if L.ret("amut", 0):
                e.write(tails[c["world"]] + (L.ret("aoff") - TAIL0), struct.pack(">H", L.ret("aid") & 0xFFFF))
                for _ in range(L.ret("arows")):
                    row, blk = L.ret("arow"), L.ret("ablk")
                    w32(e, TABLE + 0x14 * row, things[blk - 1] if blk else 0)

        S = CL.Stubs()
        S.real = OWN
        S.internal(ASSERT, assertion)
        saved = S.activate(e)
        try:
            r3, _, _ = T.call_ppc(e, GET_ADDR[self.op], [tails[c["world"]] - TAIL0])
            out = {"result": L.lab(r3)}
            out["rows"] = [[L.lab(e.u32(TABLE + 0x14 * row))] + [e.u32(TABLE + 0x14 * row + 4 * i) for i in range(1, 5)] for row in ROWS]
            out["tails"] = [list(struct.unpack(">64H", e.read(a, 0x80))) for a in tails]
        finally:
            CL.Stubs.deactivate(e, saved)
        out["log"] = L.log
        return out

    def host_case(self, lib, c):
        tails = [lib_addr(lib, k) for k in range(2)]
        for k in range(2):
            C.memmove(tails[k], tail_bytes(c, k, True), 0x80)
        lib.tpw_wg_row.argtypes = [C.c_int32, C.c_int32] + [C.c_uint32] * 4
        for row in ROWS:
            lib.tpw_wg_row(row, c["rows"][row], *c["f04"][row])
        lib.tpw_wg_world.restype = C.c_void_p
        fixed = {L_WORLD + k: lib.tpw_wg_world(k) for k in range(2)}
        fixed.update({L_THING + i: lib_addr(lib, 10 + i) for i in range(8)})
        CL.host_begin(lib, fixed, qn(get_queues(c)))
        try:
            lib.tpw_wg_getter.argtypes = [C.c_int32, C.c_int32]
            result = lib.tpw_wg_getter(self.op, c["world"])
        finally:
            log = CL.host_end(lib)
        rows = []
        lib.tpw_wg_rowinfo.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
        for row in ROWS:
            info = (C.c_int64 * 5)()
            lib.tpw_wg_rowinfo(row, info)
            rows.append(list(info))
        return {"result": result, "rows": rows, "tails": [list(struct.unpack("<64H", C.string_at(a, 0x80))) for a in tails], "log": log}


def edges_get(op):
    r = random.Random(7000 + op)
    out = []
    # Every row of the table as the selected one, with row 0 free; then with row 0 holding a thing (the assertion's condition alone differs).
    for row in ROWS:
        for zero in (True, False):
            c = mk_get(op, r)
            c["ids"][c["world"]][GET_FIELD[op]] = row
            c["rows"][row] = r.randrange(1, 9) if row else (0 if zero else r.randrange(1, 9))
            if row and not zero:
                c["rows"][0] = r.randrange(1, 9)
            elif row:
                c["rows"][0] = 0
            out.append(c)
    # A selected row that is free, with the other fields pointing at things.
    for row in ROWS[1:]:
        c = mk_get(op, r)
        c["ids"][c["world"]][GET_FIELD[op]] = row
        c["rows"][row] = 0
        out.append(c)
    # The assertion changes the id (the saved one is used) and the rows (the fresh row is read).
    for _ in range(12):
        c = mk_get(op, r)
        c["mut"] = mutation(r, c)
        out.append(c)
    for _ in range(6):
        c = mk_get(op, r)
        c["mut"] = mutation(r, c, with_id=True, with_rows=False)
        out.append(c)
    for _ in range(6):
        c = mk_get(op, r)
        c["mut"] = mutation(r, c, with_id=False, with_rows=True)
        out.append(c)
    return out


def rnd_get(op, r):
    c = mk_get(op, r)
    if r.random() < 0.3:
        c["mut"] = mutation(r, c)
    return c


# ------------------------------------------------------------------ GetFunnyTime

CLOCK_AT = 0x40
CLOCK_SIZE = CLOCK_AT + 0x24 + 0x40
CLOCK_FIELDS = [(CLOCK_AT, 8), (CLOCK_AT + 0x1C, 4)]
CELL_FIELDS = [(4 * i, 4) for i in range(12)]
TAIL_WORDS = [(4 * i, 4) for i in range(32)]
OUT_PATTERNS = [[-1] * 8] + [[k if i == k else -1 for i in range(8)] for k in range(8)] + [
    list(range(8)), [0] * 8, [0, -1, 2, -1, 4, -1, 6, -1], [-1, 1, -1, 3, -1, 5, -1, 7], [0, 0, 1, 1, 2, 2, 3, 3], list(range(7, -1, -1)),
    [3, 2, 3, 2, 3, 2, 3, 2]]


def clock_init(c):
    return {CLOCK_AT: c["start"] & M64, CLOCK_AT + 0x1C: c["rate"]}


def clock_bytes(c, little):
    fill = make_fill(c["seed"] + 1, "rand", CLOCK_SIZE)
    buf = bytearray(fill)
    order = "little" if little else "big"
    buf[CLOCK_AT:CLOCK_AT + 8] = (c["start"] & M64).to_bytes(8, order)
    buf[CLOCK_AT + 0x1C:CLOCK_AT + 0x20] = (c["rate"] & MASK).to_bytes(4, order)
    return bytes(buf), bytes(fill)


def tail_words(c, k, little):
    r = random.Random(c["seed"] * 3 + k)
    words = [r.getrandbits(32) for _ in range(32)]
    words[TURN_WORD] = c["turns"][k]
    return struct.pack(("<" if little else ">") + "32I", *words)


def mk_funny(r, **kw):
    q = {"plus": [r.choice(TICKS) if r.random() < 0.5 else r.getrandbits(64)],
         "gtout": [r.getrandbits(32) for _ in range(8)], "gterr": [r.choice([0, 1, M64, 0x7FFFFFFF, 0x80000000, r.getrandbits(32)])],
         "gtmut": [r.randrange(2) if r.random() < 0.3 else 0], "gtnew": [r.getrandbits(64)],
         "wsel": [r.randrange(2)], "wmut": [1 if r.random() < 0.2 else 0], "wturn": [r.getrandbits(32)], "wrate": [r.getrandbits(32)],
         "wstart": [r.getrandbits(64)]}
    c = dict(seed=r.getrandbits(32), rate=r.choice(U32S) if r.random() < 0.5 else r.getrandbits(32),
             turns=[r.choice(U32S) if r.random() < 0.5 else r.getrandbits(32) for _ in range(2)],
             start=r.choice(TICKS) if r.random() < 0.5 else r.getrandbits(64), outs=r.choice(OUT_PATTERNS), q=q)
    c.update(kw)
    for key, value in c.pop("override", {}).items():
        c["q"][key] = value
    return c


class FunnyTest(Base):
    def __init__(self):
        super().__init__("CParkTime::GetFunnyTime", self.ppc_case, self.host_case, edge=edges_funny, rnd=lambda r: mk_funny(r))

    def ppc_case(self, e, c):
        L = CL.PpcLog({}, qn(c["q"]))
        e.cl = L
        clock_data, clock_fill = clock_bytes(c, False)
        clock_block = e.alloc(CLOCK_SIZE, clock_data)
        clock = clock_block + CLOCK_AT
        tails = [e.alloc(0x80, tail_words(c, k, False)) for k in range(2)]
        cells = e.alloc(48, b"".join(struct.pack(">I", v) for v in cell_initial(c)))
        result = e.alloc(0x20, b"\x5a" * 0x20)
        L.labels.update({tails[0] - TAIL0: 1, tails[1] - TAIL0: 2, clock: L_CLOCK})
        L.labels.update({cells + 4 * i: L_CELL + i for i in range(8)})

        def getworld(emu):
            L.rec("GetWorld", [L.lab(emu.r(3))])
            sel = L.ret("wsel", 0)
            if L.ret("wmut", 0):
                w32(e, tails[sel] + 4 * TURN_WORD, L.ret("wturn"))
                w32(e, clock + 0x1C, L.ret("wrate"))
                e.write(clock, (L.ret("wstart") & M64).to_bytes(8, "big"))
            emu.set_r(3, tails[sel] - TAIL0)

        def plus(emu):
            L.rec("TimeStamp+", [L.lab(emu.r(4)), s64(be64(emu, emu.r(4))), s64(be64(emu, emu.r(5)))])
            emu.write(emu.r(3), (L.ret("plus", 0) & M64).to_bytes(8, "big"))

        def gettime(emu):
            stamp = emu.r(4)
            outs = [emu.r(i) for i in range(5, 11)] + [emu.u32(emu.r(1) + 0x38), emu.u32(emu.r(1) + 0x3C)]
            L.rec("GetTime", [s64(be64(emu, stamp))] + [L.lab(o) for o in outs])
            for o in outs:
                if o:
                    w32(e, o, L.ret("gtout", 0))
            if L.ret("gtmut", 0):
                emu.write(stamp, (L.ret("gtnew", 0) & M64).to_bytes(8, "big"))
            w32(e, emu.r(3), L.ret("gterr", 0))

        S = CL.Stubs()
        S.real = OWN
        S.internal(0x1014205C, getworld)
        S.imp("__pl__11TbTimeStampCFRC10TbTimeDiff", plus)
        S.imp("GetTime__11TbTimeStampCFPiPiPiPiPiPiPiPi", gettime)
        saved = S.activate(e)
        try:
            args = [result + 8, clock] + [cells + 4 * s if s >= 0 else 0 for s in c["outs"]]
            T.call_ppc(e, FUNNY, args)
            ticks = s64(be64(e, result + 8))
            guard = bytes(e.read(result, 8)) + bytes(e.read(result + 16, 0x10))
            out = {"ticks": ticks, "guard": guard, "cells": decode(e.read(cells, 48), CELL_FIELDS, False),
                   "clock": decode(e.read(clock_block, CLOCK_SIZE), CLOCK_FIELDS, False),
                   "tails": [decode(e.read(a, 0x80), TAIL_WORDS, False) for a in tails]}
            out["untouched"] = moved(e.read(clock_block, CLOCK_SIZE), clock_fill, gaps(CLOCK_FIELDS, CLOCK_SIZE))
        finally:
            CL.Stubs.deactivate(e, saved)
        out["log"] = L.log
        return out

    def host_case(self, lib, c):
        clock_data, clock_fill = clock_bytes(c, True)
        block = lib_addr(lib, 2)
        C.memmove(block, clock_data, CLOCK_SIZE)
        tails = [lib_addr(lib, k) for k in range(2)]
        for k in range(2):
            C.memmove(tails[k], tail_words(c, k, True), 0x80)
        cells = lib_addr(lib, 3)
        C.memmove(cells, b"".join(struct.pack("<I", v) for v in cell_initial(c)), 48)
        lib.tpw_wg_world.restype = C.c_void_p
        fixed = {L_WORLD + k: lib.tpw_wg_world(k) for k in range(2)}
        fixed[L_CLOCK] = block + CLOCK_AT
        fixed.update({L_CELL + i: cells + 4 * i for i in range(8)})
        CL.host_begin(lib, fixed, qn(c["q"]))
        try:
            lib.tpw_wg_funny.restype = C.c_int64
            lib.tpw_wg_funny.argtypes = [C.POINTER(C.c_int32)]
            ticks = lib.tpw_wg_funny((C.c_int32 * 8)(*c["outs"]))
        finally:
            log = CL.host_end(lib)
        data = C.string_at(block, CLOCK_SIZE)
        return {"ticks": ticks, "guard": b"\x5a" * 0x18, "cells": decode(C.string_at(cells, 48), CELL_FIELDS, True),
                "clock": decode(data, CLOCK_FIELDS, True), "tails": [decode(C.string_at(a, 0x80), TAIL_WORDS, True) for a in tails],
                "untouched": moved(data, clock_fill, gaps(CLOCK_FIELDS, CLOCK_SIZE)), "log": log}


def cell_initial(c):
    r = random.Random(c["seed"] + 5)
    return [r.getrandbits(32) for _ in range(12)]


def edges_funny():
    r = random.Random(7100)
    out = []
    # The product of every rate with every turn crosses bit 63 (0xffffffff * 0xffffffff), approaches it (0x80000000 * 0xffffffff) and hits all
    # remainders of the division by 4.
    for rate in U32S:
        for turn in U32S:
            out.append(mk_funny(r, rate=rate, turns=[turn, r.getrandbits(32)], override={"wsel": [0], "wmut": [0]}))
    for rate in (0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 15000, 3, 5, 1):
        for turn in (0xFFFFFFFE, 0xFFFFFFFD, 0x80000001, 0x7FFFFFFF, 0x55555555, 0xAAAAAAAA, 0x40000000):
            out.append(mk_funny(r, rate=rate, turns=[r.getrandbits(32), turn], override={"wsel": [1], "wmut": [0]}))
    # Every start and every answer of the addition.
    for start in TICKS:
        for plus in TICKS:
            out.append(mk_funny(r, start=start, override={"plus": [plus]}))
    # The eight outputs: none, each alone, all distinct, all the same, alternating, aliases.
    for pattern in OUT_PATTERNS:
        for _ in range(2):
            out.append(mk_funny(r, outs=pattern))
    # An error from GetTime (negative, positive, zero) never changes the answer; a GetTime that changes its stamp does.
    for err in (0, 1, M64, 0x80000000):
        for mut in (0, 1):
            out.append(mk_funny(r, override={"gterr": [err], "gtmut": [mut], "gtnew": [r.getrandbits(64)]}))
    # The world accessor changes the answered world's turn, the clock's rate and the clock's start before they are read.
    for sel in (0, 1):
        for _ in range(3):
            out.append(mk_funny(r, override={"wsel": [sel], "wmut": [1]}))
    return out


# ------------------------------------------------------------------ CParkAnalyser::Init


def analyser_fields():
    fields = [(o, 4) for o in range(0xC, 0x20, 4)]
    for i in range(0x400):
        fields += [(0x20 + 0x7C * i + 4 * k, 4) for k in range(4)]
    return fields


ANALYSER_FIELDS = analyser_fields()
ANALYSER_GAPS = gaps(ANALYSER_FIELDS, ANALYSER_SIZE)
GRIDS = [(0, 0), (0, 7), (7, 0), (1, 1), (2, 3), (3, 2), (1, 5), (5, 1), (4, 4), (8, 8), (16, 16), (32, 32), (32, 1), (1, 32), (33, 31), (31, 33), (1, 1024),
         (-2, 3), (3, -2), (-2, -3), (-1, 5), (5, -1), (-1, -1), (65536, 65536), (0x40000000, 4), (0x7FFFFFFF, 2), (0x7FFFFFFF, 0x7FFFFFFF)]
SQUARES = [4, 1, 0, -1, 3, 2, -4, 7, 0x10000, 0x7FFFFFFF, -0x80000000]


def mk_ana(r, **kw):
    gx, gy = r.choice(GRIDS)
    c = dict(seed=r.getrandbits(32), fill=r.choice(FILLS), squares=r.choice(SQUARES), gx=gx, gy=gy, mx=r.randrange(-20, 300), my=r.randrange(-20, 300),
             centre=r.randrange(2), q={"newnull": [r.randrange(2) for _ in range(3)], "centreclear": [r.randrange(2) for _ in range(3)],
                                       "cret": [r.choice([0, 1, 2, M64])]})
    c.update(kw)
    return c


def extents(squares, g):
    """Map extents around the cell edges of a grid of g cells."""
    s = s32(squares * g) if abs(g) < 0x10000 else 0
    return [0, 1, -1, squares, s32(squares * 2), s, s32(s - 1), s32(s + 1), 128, 127, 129, 0x7FFFFFFF, -0x80000000]


class AnalyserTest(Base):
    def __init__(self):
        super().__init__("CParkAnalyser::Init", self.ppc_case, self.host_case, edge=edges_analyser, rnd=self.random_case_)

    @staticmethod
    def random_case_(r):
        while True:
            c = mk_ana(r)
            if r.random() < 0.7:
                c["gx"], c["gy"] = r.randrange(-40, 41), r.randrange(-30, 31)
                if c["gx"] * c["gy"] > 1024:
                    continue
            c["squares"] = r.choice(SQUARES) if r.random() < 0.5 else r.randrange(-8, 10)
            c["mx"] = r.choice(extents(c["squares"], c["gx"])) if r.random() < 0.6 else r.randrange(-20, 300)
            c["my"] = r.choice(extents(c["squares"], c["gy"])) if r.random() < 0.6 else r.randrange(-20, 300)
            if r.random() < 0.15 and c["gx"] * c["gy"] > 0:
                c["q"]["cmut"] = [1 if r.random() < 0.5 else 0 for _ in range(6)]
                c["q"].update({"cs": [r.randrange(-8, 10)], "cgx": [r.choice([1, -1, 3, 7, 40])], "cgy": [r.randrange(-3, 40)],
                               "cmx": [r.randrange(-20, 300)], "cmy": [r.randrange(-20, 300)]})
            return c

    @staticmethod
    def init(c):
        return {0xC: c["squares"], 0x10: c["gx"], 0x14: c["gy"], 0x18: c["mx"], 0x1C: c["my"]}

    def ppc_case(self, e, c):
        L = CL.PpcLog({}, qn(c["q"]))
        e.cl = L
        fill = make_fill(c["seed"], c["fill"], ANALYSER_SIZE)
        data = sided(fill, ANALYSER_FIELDS, self.init(c), False)
        block = e.alloc(len(data), data)
        centre = e.alloc(0x1D0, b"\xa5" * 0x1D0)
        L.labels.update({block: L_BLOCK, centre: L_CENTRE})
        allocations = []

        def subscribe(emu):
            L.rec("Subscribe", [L.lab(emu.r(3)), L.lab(emu.r(4)), s32(emu.r(5))])
            if L.ret("centreclear", 0):
                w32(e, G_CENTRE, 0)

        def cell(emu):
            p = emu.r(3)
            L.rec("CellInit", [p - block] + [s32(emu.r(i)) for i in range(4, 8)])
            for i in range(4):
                w32(e, p + 4 * i, emu.r(4 + i))
            if L.ret("cmut", 0):
                for off, key, dflt in ((0xC, "cs", 0), (0x10, "cgx", 1), (0x14, "cgy", 0), (0x18, "cmx", 0), (0x1C, "cmy", 0)):
                    w32(e, block + off, L.ret(key, dflt))
            emu.set_r(3, L.ret("cret", 1) & MASK)

        def op_new(emu):
            size = emu.r(3)
            fail = bool(L.ret("newnull", 0))
            L.rec("new", [size, int(fail)])
            p = 0 if fail else emu.alloc(size, b"\xa5" * size)
            if p:
                allocations.append(p)
            emu.set_r(3, p)

        def construct(emu, call):
            p, ctor, dtor, size, count = (emu.r(i) for i in range(3, 8))
            if ctor == 0x102657E8:
                L.rec("CMessageCentre", [L.newlab(p), int(ctor == 0x102657E8), int(dtor == 0x10267288), size, count])

        S = CL.Stubs()
        S.real = OWN
        S.internal(0x10152388, subscribe)
        S.internal(0x100E6054, cell)
        S.imp("__nw__FUl", op_new)
        saved = S.activate(e)
        try:
            T.stub_cb(e, "__construct_array", construct)
            w32(e, G_CENTRE, centre if c["centre"] else 0)
            r3, _, _ = T.call_ppc(e, ANALYSER, [block], max_insns=200_000_000, timeout_us=60_000_000)
            buf = e.read(block, len(data))
            out = {"ret": r3, "fields": decode(buf, ANALYSER_FIELDS, False), "untouched": moved(buf, data, ANALYSER_GAPS),
                   "globals": [L.lab(e.u32(G_CENTRE)), len(allocations)]}
        finally:
            CL.Stubs.deactivate(e, saved)
        out["log"] = L.log
        return out

    def host_case(self, lib, c):
        fill = make_fill(c["seed"], c["fill"], ANALYSER_SIZE)
        data = sided(fill, ANALYSER_FIELDS, self.init(c), True)
        block, centre = lib_addr(lib, 4), lib_addr(lib, 5)
        C.memmove(block, data, len(data))
        lib.tpw_wg_prepare.argtypes = [C.c_int32]
        lib.tpw_wg_prepare(c["centre"])
        CL.host_begin(lib, {L_BLOCK: block, L_CENTRE: centre}, qn(c["q"]))
        try:
            ret = lib.tpw_wg_analyser()
        finally:
            log = CL.host_end(lib)
        state = (C.c_int64 * 2)()
        lib.tpw_wg_globals.argtypes = [C.POINTER(C.c_int64)]
        lib.tpw_wg_globals(state)
        buf = C.string_at(block, len(data))
        return {"ret": ret, "fields": decode(buf, ANALYSER_FIELDS, True), "untouched": moved(buf, data, ANALYSER_GAPS), "globals": list(state), "log": log}


def edges_analyser():
    r = random.Random(7200)
    out = []
    for gx, gy in GRIDS:
        for squares in (4, 1, 0, -1, 3):
            limit = s32(gx * gy)
            if limit > 1024:
                continue
            for _ in range(2):
                mx = r.choice(extents(squares, gx))
                my = r.choice(extents(squares, gy))
                out.append(mk_ana(r, gx=gx, gy=gy, squares=squares, mx=mx, my=my))
    # The map exactly at, one less and one more than the last cell's edge (strict compare), in X and Y separately and together.
    for squares in (4, 3, 1, -2):
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                gx, gy = 5, 4
                out.append(mk_ana(r, gx=gx, gy=gy, squares=squares, mx=s32(squares * gx + dx), my=s32(squares * gy + dy)))
                out.append(mk_ana(r, gx=gx, gy=gy, squares=squares, mx=s32(squares * 2 + dx), my=s32(squares * 3 + dy)))
    # The normal park: 32 x 32 cells of 4 squares in a 128 x 128 map (starts 0..124, sizes 4).
    for fill in FILLS:
        for centre in (0, 1):
            out.append(mk_ana(r, fill=fill, centre=centre, gx=32, gy=32, squares=4, mx=128, my=128))
    # Every allocation outcome of the three lazy centre blocks, with Subscribe leaving the centre or clearing it.
    for centre in (0, 1):
        for nulls in ([0], [1, 0], [1, 1, 0], [1, 1, 1], [0, 1, 0], [0, 0, 1], [1, 0, 1]):
            for clears in ([0], [1], [0, 1], [1, 0, 1], [0, 0, 1], [1, 1, 0]):
                out.append(mk_ana(r, centre=centre, gx=2, gy=2, squares=4, mx=8, my=8, q={"newnull": nulls, "centreclear": clears, "cret": [1]}))
    # The cell changes the analyser: the loop limit was taken once, every other number is read afresh.
    for _ in range(24):
        gx, gy = r.choice([(4, 4), (8, 3), (3, 8), (6, 6), (5, 2)])
        q = {"cmut": [r.randrange(2) for _ in range(4)] + [1], "cs": [r.randrange(-3, 9)], "cgx": [r.choice([1, 2, 3, 7, -1, -2])],
             "cgy": [r.randrange(-3, 12)], "cmx": [r.randrange(0, 60)], "cmy": [r.randrange(0, 60)], "newnull": [0], "centreclear": [0]}
        out.append(mk_ana(r, gx=gx, gy=gy, squares=r.choice([1, 2, 4]), mx=r.randrange(0, 40), my=r.randrange(0, 40), q=q))
    # Each answer of the cell is ignored.
    for ans in (0, 1, 2, M64, 0x80000000):
        out.append(mk_ana(r, gx=3, gy=3, squares=4, mx=11, my=11, q={"cret": [ans], "newnull": [0], "centreclear": [0]}))
    return out


# ------------------------------------------------------------------ CChallengeHQ::Init

HQ_FIELDS = [(0xC + 0x30 * i, 4) for i in range(20)] + [(0x3CC, 4)]
HQ_GAPS = gaps(HQ_FIELDS, HQ_SIZE)
IDS = [0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x12345678, M64, 0xFFFF0000, 0x1FFFF]
FLAGS = [0, 1, 2, M64, 0x80000000, 0x100]
ROW_SPECIAL = [0, 1, 2, M64, 0x7FFFFFFF, 0x80000000, 100, 0x10000, 0xFFFF]


def hq_state(types, current):
    return hash_values(list(types) + [current])


def mk_table(r, rows=None):
    """Slots 1..601 (index 0 is slot 1): sixty rows of ten columns, then the row count."""
    table = []
    for j in range(60):
        row = [r.choice(ROW_SPECIAL) if r.random() < 0.5 else r.getrandbits(32) for _ in range(10)]
        row[4], row[5] = r.choice(IDS) if r.random() < 0.7 else row[4], r.choice(IDS) if r.random() < 0.7 else row[5]
        row[8], row[9] = r.choice(FLAGS), r.choice(FLAGS)
        table += row
    table.append(r.choice([0, 5, 60, 61, M64, r.getrandbits(32)]))
    return table


def mk_hq(r, **kw):
    choices = []
    for _ in range(20):
        t = r.random()
        choices.append(0 if t < 0.3 else r.choice([M64, 0x80000000, 0x80000001]) if t < 0.4 else r.randrange(1, 60))
    c = dict(seed=r.getrandbits(32), fill=r.choice(FILLS), choices=choices, count=r.choice([0, 1, 20, 60, M64, r.getrandbits(32)]),
             after=None, table=mk_table(r), centre=0,
             q={"load": [r.choice([0, 1, 1, 1])]})
    if r.random() < 0.15:
        c["after"] = [r.choice([0, 1, 2, 59, M64]) for _ in range(20)]
    if r.random() < 0.2:
        c["q"].update({"addmut": [r.randrange(2)], "addslot": [r.randrange(20)], "addtype": [r.getrandbits(32)]})
    if r.random() < 0.2:
        c["q"].update({"clrmut": [1], "clrval": [r.getrandbits(32)]})
    c.update(kw)
    return c


class HqTest(Base):
    def __init__(self):
        super().__init__("CChallengeHQ::Init", self.ppc_case, self.host_case, edge=edges_hq, rnd=lambda r: mk_hq(r))

    def ppc_case(self, e, c):
        L = CL.PpcLog({}, qn(c["q"]))
        e.cl = L
        fill = make_fill(c["seed"], c["fill"], HQ_SIZE)
        data = sided(fill, HQ_FIELDS, {}, False)
        block = e.alloc(len(data), data)
        L.labels[block] = L_BLOCK
        for i in range(20):
            w32(e, MAIN + 0x794 + 4 * i, c["choices"][i])
        w32(e, MAIN + 0x794 + 0x50, c["count"])

        def state(emu):
            return hash_values([emu.u32(block + 0xC + 0x30 * i) for i in range(20)] + [emu.u32(block + 0x3CC)])

        def table_init(emu):
            L.rec("BalanceInit", [L.newlab(emu.r(3)), state(emu)])

        def table_load(emu):
            t = emu.r(3)
            L.rec("BalanceLoad", [L.lab(t), s32(emu.r(5)), state(emu)], [emu.cstr(emu.r(4))])
            for k in range(1, 602):
                w32(e, t + 8 + 4 * k, c["table"][k - 1])
            if c["after"] is not None:
                for i in range(20):
                    w32(e, MAIN + 0x794 + 4 * i, c["after"][i])
            emu.set_r(3, L.ret("load", 1) & MASK)

        def table_uninit(emu):
            L.rec("BalanceUnInit", [L.lab(emu.r(3)), state(emu)])

        def trace(emu):
            L.rec("CTRACE", [emu.r(3)], [emu.cstr(emu.r(4))])

        def challenge(emu):
            p, sp = emu.r(3), emu.r(1)
            label = L.lab(p)
            if label < 0:
                label = L.newlab(p)
            ids = (emu.r(10) >> 16, emu.u32(sp + 0x38) >> 16)
            staff, independent = s32(emu.u32(sp + 0x3C)), emu.u32(sp + 0x40) & 0xFF
            L.rec("ChallengeCtor", [label, s32(emu.r(4)), s32(emu.r(5)), s32(emu.r(6)), s32(emu.r(7)), emu.r(8) & 0xFF, s32(emu.r(9)),
                                    ids[0], ids[1], staff, independent])
            for off, value in ((0, emu.r(4)), (4, emu.r(5)), (8, emu.r(9)), (0x10, staff), (0x14, emu.r(6)), (0x18, emu.r(7)),
                               (0x20, 0), (0x24, 0), (0x28, 0)):
                w32(e, p + off, value)
            e.write(p + 0xC, struct.pack(">HH", *ids))
            e.write(p + 0x1C, bytes([emu.r(8) & 0xFF, independent]))
            e.write(p + 0x2C, b"\0")
            e.write(p + 0x2E, b"\0\0")

        def add(emu):
            this, p = emu.r(3), emu.r(4)
            fields = [s32(e.u32(p)), s32(e.u32(p + 4)), s32(e.u32(p + 8)), int.from_bytes(e.read(p + 0xC, 2), "big"),
                      int.from_bytes(e.read(p + 0xE, 2), "big"), s32(e.u32(p + 0x10)), s32(e.u32(p + 0x14)), s32(e.u32(p + 0x18)),
                      e.read(p + 0x1C, 1)[0], e.read(p + 0x1D, 1)[0]]
            L.rec("AddChallenge", [L.lab(this), L.lab(p)] + fields)
            if L.ret("addmut", 0):
                w32(e, this + 0xC + 0x30 * L.ret("addslot", 0), L.ret("addtype", 0))

        def clear(emu):
            L.rec("ClearFollowups", [L.lab(emu.r(3)), state(emu)])
            if L.ret("clrmut", 0):
                w32(e, emu.r(3) + 0x3CC, L.ret("clrval", 0))

        S = CL.Stubs()
        S.real = OWN
        S.internal(0x1001EA28, table_init)
        S.internal(0x10020134, table_load)
        S.internal(0x1001ED40, table_uninit)
        S.internal(0x1006FCBC, trace)
        S.internal(0x100F5A78, challenge)
        S.internal(0x100F7498, add)
        S.internal(0x100F680C, clear)
        saved = S.activate(e)
        try:
            T.call_ppc(e, HQ, [block], max_insns=200_000_000, timeout_us=60_000_000)
            buf = e.read(block, len(data))
            out = {"fields": decode(buf, HQ_FIELDS, False), "untouched": moved(buf, data, HQ_GAPS),
                   "main": [e.u32(MAIN + 0x794 + 4 * i) for i in range(21)]}
        finally:
            CL.Stubs.deactivate(e, saved)
        out["log"] = L.log
        return out

    def host_case(self, lib, c):
        fill = make_fill(c["seed"], c["fill"], HQ_SIZE)
        data = sided(fill, HQ_FIELDS, {}, True)
        block = lib_addr(lib, 4)
        C.memmove(block, data, len(data))
        lib.tpw_wg_settable.argtypes = [C.POINTER(C.c_int32)]
        lib.tpw_wg_settable((C.c_int32 * 602)(0, *[s32(v) for v in c["table"]]))
        lib.tpw_wg_setmain.argtypes = [C.POINTER(C.c_int32), C.c_int32, C.POINTER(C.c_int32), C.c_int32]
        after = c["after"] if c["after"] is not None else [0] * 20
        lib.tpw_wg_setmain((C.c_int32 * 20)(*[s32(v) for v in c["choices"]]), s32(c["count"]), (C.c_int32 * 20)(*[s32(v) for v in after]),
                           int(c["after"] is not None))
        CL.host_begin(lib, {L_BLOCK: block}, qn(c["q"]))
        try:
            lib.tpw_wg_hq()
        finally:
            log = CL.host_end(lib)
        main = (C.c_int32 * 21)()
        lib.tpw_wg_mainnow.argtypes = [C.POINTER(C.c_int32)]
        lib.tpw_wg_mainnow(main)
        buf = C.string_at(block, len(data))
        return {"fields": decode(buf, HQ_FIELDS, True), "untouched": moved(buf, data, HQ_GAPS), "main": [v & MASK for v in main], "log": log}


def edges_hq():
    r = random.Random(7300)
    out = []

    def table(**kw):
        t = mk_table(r)
        for (j, col), v in kw.get("set", {}).items():
            t[10 * j + col] = v
        return t

    # No choice is positive (zero, negative, the signed minimum): nothing is built but Clear and UnInit still run; Load failing traces.
    for load in (0, 1):
        for choices in ([0] * 20, [M64] * 20, [0x80000000] * 20, [0x80000001, M64, 0, 0x80000000] * 5):
            out.append(mk_hq(r, choices=choices, q={"load": [load]}))
    # One positive choice at the first and at the last slot, at the first and at the last row of the table.
    for slot in (0, 1, 18, 19):
        for row in (1, 2, 30, 58, 59):
            for load in (1, 0):
                choices = [0] * 20
                choices[slot] = row
                out.append(mk_hq(r, choices=choices, q={"load": [load]}))
    # Duplicates, out of order, all twenty enabled in both directions.
    out.append(mk_hq(r, choices=[5] * 20, q={"load": [1]}))
    out.append(mk_hq(r, choices=list(range(1, 21)), q={"load": [1]}))
    out.append(mk_hq(r, choices=list(range(59, 39, -1)), q={"load": [1]}))
    out.append(mk_hq(r, choices=[7, 0, 3, M64, 3, 59, 1, 0x80000000, 2, 0, 9, 0, 0, 8, 1, 1, 4, 0, 6, 5], q={"load": [1]}))
    # The loaded count (slot 601) and the level's count (slot 503) never bound the twenty slots.
    for tcount in (0, 1, 5, 60, M64):
        for mcount in (0, 1, 20, 0x7FFFFFFF):
            t = mk_table(r)
            t[600] = tcount
            out.append(mk_hq(r, table=t, count=mcount, choices=[3, 0, 59, 1] * 5, q={"load": [1]}))
    # Object ids (truncated to 16 bits, not sign extended) and flags (!= 0), one pinned column at a time.
    for ident in IDS:
        for other in (IDS[1], IDS[4]):
            t = table(set={(3, 4): ident, (3, 5): other, (4, 4): other, (4, 5): ident})
            out.append(mk_hq(r, table=t, choices=[3, 4] + [0] * 18, q={"load": [1]}))
    for flag in FLAGS:
        for other in (0, 1):
            t = table(set={(2, 8): flag, (2, 9): other, (6, 8): other, (6, 9): flag})
            out.append(mk_hq(r, table=t, choices=[2, 6] + [0] * 18, q={"load": [1]}))
    # Load changes the main balance's choices; AddChallenge and ClearInvalidFollowups change the HQ.
    for _ in range(8):
        out.append(mk_hq(r, after=[r.choice([0, 1, 2, 59, M64, 7]) for _ in range(20)], choices=[r.randrange(0, 60) for _ in range(20)], q={"load": [r.randrange(2)]}))
    for slot in (0, 5, 19):
        out.append(mk_hq(r, choices=[2] * 20, q={"load": [1], "addmut": [1], "addslot": [slot], "addtype": [r.getrandbits(32)],
                                                   "clrmut": [1], "clrval": [r.getrandbits(32)]}))
    # Every fill, including an HQ that holds types already (cleared before the table is made).
    for fill in FILLS:
        out.append(mk_hq(r, fill=fill, choices=[1, 0, 2] + [0] * 17, q={"load": [1]}))
    return out


TESTS = [GetterTest(op) for op in range(7)] + [FunnyTest(), AnalyserTest(), HqTest()]
