"""Differential tests for unit w125, the frame readers of the things (src/game/park/analyser.cpp, src/game/ui/InterfaceTextObjectSprite.cpp). See difftest.py and tpwtest.py.

CParkAnalyser::HowBusyIsPark, NumberOfVisitorsInPark and CountNeighboursOfType, and the Change of InterfaceNumberTextObject and
InterfaceCashAndGTickTextObject. Every callee is a fake that records the same event on both sides (GAME_ASSERTMSG, the world accessor and its used-thing
walk, CPeep::IsInsidePark, the analyser value's Get and Set, GetVisitorsInPark, the sprite's Change, swprintf, the string table's FormatString); the answers
come from queues. The thing table, the map, the world's turn word and the analyser value are built from the same description on both sides, and a case
compares the answer and the whole call log. CTimeSig::SecondsSinceLastUpdate (0x1015f124) runs as the original on the PowerPC side, over the world accessor's fake.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ParkReaders"

MASK = 0xFFFFFFFF
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF
TABLE, G_MAP, G_TEXT = 0x1034CBFC, 0x1037EC08, 0x1037FF18
ASSERT, GETWORLD, HEAD, NEXT = 0x1000DDE8, 0x1014205C, 0x1013ADB0, 0x1013AE08
INSIDE, VALGET, VALSET, GVIP, SPRITE = 0x10115A80, 0x100EF3C8, 0x100EF400, 0x100E5414, 0x1018D798
HOWBUSY, VISITORS, COUNT, NUMCHG, CASHCHG, SECONDS = 0x100E5944, 0x100E7278, 0x100E6A6C, 0x1018DB00, 0x1018E36C, 0x1015F124
OWN = {HOWBUSY, VISITORS, COUNT, NUMCHG, CASHCHG, SECONDS}
TAIL0 = 0x1DA6F0
VAL_AT = 0x1F0B8
ANALYSER_SIZE = VAL_AT + 0x40
CELLS = 0x4000
L_ANALYSER, L_VAL, L_WORLD, L_TABLE, L_OBJ, L_CHANGE, L_THING = 1, 2, 3, 4, 5, 6, 10
SLOTS = 8
MAXID = 15
ASSERT_TEXT = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"


def s32(v):
    return T.s32(v)


def s64(v):
    v &= (1 << 64) - 1
    return v - (1 << 64) if v & (1 << 63) else v


def setup(runner):
    T.install_std_stubs(runner.emu)


def lib_addr(lib, which):
    lib.tpw_pr_addr.restype = C.c_void_p
    lib.tpw_pr_addr.argtypes = [C.c_int32]
    return lib.tpw_pr_addr(which)


def wide(e, a, limit=200):
    out = []
    for _ in range(limit):
        ch = struct.unpack(">H", e.read(a, 2))[0]
        if ch == 0:
            break
        out.append(chr(ch))
        a += 2
    return "".join(out)


def wbytes(text):
    return b"".join(struct.pack(">H", ord(c)) for c in text) + b"\0\0"


class Base(FnTest):
    def run_case(self, runner, case):
        runner.emu.reset_scratch()
        try:
            p = self.ppc(runner.emu, case)
        except ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = self.host(runner.lib, case)
        difference = CL.first_difference(p.pop("log"), h.pop("log"))
        errors = T.same_values(p, h, "", False)
        if difference:
            errors.append(difference)
        return errors


# ------------------------------------------------------------------ the world of a case (both sides built from the description)


def qn(q):
    return {k: [s64(v) for v in vs] for k, vs in q.items()}


def stubs_for(e, L, world, c):
    """The PowerPC fakes."""
    def assertion(emu):
        L.rec("GAME_ASSERTMSG", [s32(emu.r(3))], [emu.cstr(emu.r(4))])

    def getworld(emu):
        L.rec("GetWorld", [L.lab(emu.r(3))])
        emu.set_r(3, world)

    def head(emu):
        L.rec("GetUsedThingHead", [L.lab(emu.r(4))])
        e.write(emu.r(3), struct.pack(">H", L.ret("head", 0) & 0xFFFF))

    def nxt(emu):
        L.rec("GetNextUsedThing", [L.lab(emu.r(4)), L.lab(emu.r(5))])
        e.write(emu.r(3), struct.pack(">H", L.ret("next", 0) & 0xFFFF))

    def inside(emu):
        L.rec("IsInsidePark", [L.lab(emu.r(3))])
        emu.set_r(3, T.u32(L.ret("inside", 0)))

    def valget(emu):
        L.rec("ValGet", [L.lab(emu.r(3))])
        emu.set_r(3, T.u32(L.ret("get", 0)))

    def valset(emu):
        L.rec("ValSet", [L.lab(emu.r(3)), s32(emu.r(4))])

    def gvip(emu):
        L.rec("GetVisitorsInPark", [L.lab(emu.r(3)), emu.r(4)])
        emu.set_r(3, T.u32(L.ret("gvip", 0)))

    def sprite(emu):
        this, flags, change = emu.r(3), emu.r(4), emu.r(5)
        ints = [L.lab(this), flags, L.lab(change)]
        if flags == 0x100:
            L.rec("SpriteChange", ints, [CL.narrow(wide(e, emu.u32(emu.u32(change + 0x10))))])
        else:
            L.rec("SpriteChange", ints)

    def hexword(prefix, value, digits):
        return prefix + format(value & ((1 << (4 * digits)) - 1), "0%dx" % digits)

    def swprintf(emu):
        out, size, fmt = emu.r(3), emu.r(4), wide(e, emu.r(5))
        if fmt.startswith("%d"):
            value = s32(emu.r(6))
            L.rec("swprintf", [size, value], [CL.narrow(fmt)])
            text = hexword("n", value, 8)
        else:
            bits = (emu.r(6) << 32) | emu.r(7)   # a variadic double is read from the two word slots (r6, r7), as the C library does
            L.rec("swprintf", [size, s64(bits)], [CL.narrow(fmt)])
            text = hexword("f", bits, 16)
        if size:
            e.write(out, wbytes(text[:size - 1]))
        emu.set_r(3, T.u32(len(text) if len(text) <= size - 1 and size else -1))

    def format_string(emu):
        this, out, size, sid, tag = emu.r(3), emu.r(4), emu.r(5), emu.r(6), emu.r(7)
        value = s32(emu.u32(tag + 0xC))
        L.rec("FormatString", [L.lab(this), size, sid, emu.u32(tag), emu.u32(tag + 8), value])
        text = hexword("s", value, 8)
        if size:
            e.write(out, wbytes(text[:size - 1]))
        emu.set_r(3, len(text))

    S = CL.Stubs()
    S.real = OWN
    for addr, fn in ((ASSERT, assertion), (GETWORLD, getworld), (HEAD, head), (NEXT, nxt), (INSIDE, inside), (VALGET, valget),
                     (VALSET, valset), (GVIP, gvip), (SPRITE, sprite)):
        S.internal(addr, fn)
    S.imp("swprintf", swprintf)
    S.imp("FormatString__8TbITableCFPwUiUle", format_string)
    S.imp("labs", lambda emu: emu.set_r(3, T.u32(abs(s32(emu.r(3))))))
    return S


def build_ppc(e, c, queues):
    """The PowerPC memory of a case; returns (log, stubs, addresses)."""
    L = CL.PpcLog({}, qn(queues))
    e.cl = L
    analyser = e.alloc(ANALYSER_SIZE, b"\xa5" * ANALYSER_SIZE)
    tail = e.alloc(0x80, b"\xa5" * 0x80)
    world = tail - TAIL0
    things = [e.alloc(0x400, b"\xa5" * 0x400) for _ in range(SLOTS)]
    table = e.alloc(0x40, b"\xa5" * 0x40)
    obj = e.alloc(0x400, b"\xa5" * 0x400)
    change = e.alloc(0x28, b"\xa5" * 0x28)
    L.labels.update({analyser: L_ANALYSER, analyser + VAL_AT: L_VAL, world: L_WORLD, table: L_TABLE, obj: L_OBJ, change: L_CHANGE})
    L.labels.update({a: L_THING + i for i, a in enumerate(things)})
    if "turn" in c:
        e.write(tail + 0x1C, struct.pack(">I", c["turn"] & MASK))
        e.write(analyser + VAL_AT, struct.pack(">II", c["stamp"] & MASK, c["val"] & MASK) + bytes([c["worked"] & 0xFF]))
    for i, th in enumerate(c.get("things", [])):
        if th is not None:
            e.write(things[i] + 2, bytes([th[0] & 0xFF]))
            e.write(things[i] + 0xA, struct.pack(">H", th[1] & 0xFFFF))
    for row in range(MAXID + 1):
        slot = c.get("rows", {}).get(row, 0)
        e.write(TABLE + 0x14 * row, struct.pack(">I", things[slot - 1] if slot else 0))
    return L, stubs_for(e, L, world, c), dict(analyser=analyser, things=things, table=table, obj=obj, change=change)


def host_setup(lib, c, queues):
    lib.tpw_pr_turn.argtypes = [C.c_uint32]
    lib.tpw_pr_val.argtypes = [C.c_int32, C.c_uint32, C.c_int32]
    lib.tpw_pr_row.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_pr_thing.argtypes = [C.c_int32] * 3
    if "turn" in c:
        lib.tpw_pr_turn(c["turn"] & MASK)
        lib.tpw_pr_val(c["worked"] & 0xFF, c["stamp"] & MASK, c["val"])
    for i, th in enumerate(c.get("things", [])):
        if th is not None:
            lib.tpw_pr_thing(i, th[0] & 0xFF, th[1] & 0xFFFF)
    for row in range(MAXID + 1):
        lib.tpw_pr_row(row, c.get("rows", {}).get(row, 0))
    fixed = {L_ANALYSER: lib_addr(lib, 0), L_VAL: lib_addr(lib, 0) + VAL_AT, L_WORLD: lib_addr(lib, 1), L_TABLE: lib_addr(lib, 2),
             L_OBJ: lib_addr(lib, 3), L_CHANGE: lib_addr(lib, 4)}
    fixed.update({L_THING + i: lib_addr(lib, 10 + i) for i in range(SLOTS)})
    CL.host_begin(lib, fixed, qn(queues))


# ------------------------------------------------------------------ HowBusyIsPark and NumberOfVisitorsInPark

VALS = [-999, -1, -2, -3, 0, 1, 2, 3, 199, 200, 201, 398, 399, 400, 401, INT_MAX, INT_MIN, INT_MIN + 1, -INT_MAX]
KINDS = [0, 1, 1, 1, 2, 3, 4, 0x13, 0x81, 0xFF]
INSIDE_ANSWERS = [0, 1, 2, 0x100, 0x10000, 0x80000000, 0xFFFFFFFF, 0xFFFF0000, 0x7FFFFFFF]


def rand_val(r):
    k = r.random()
    if k < 0.55:
        return r.choice(VALS)
    if k < 0.8:
        return r.randint(-450, 450)
    return s32(r.getrandbits(32))


def analyser_case(r, **kw):
    n = r.choice([0, 1, 1, 2, 3, 4, 5, 6])
    ids = r.sample(range(1, MAXID + 1), n)
    slots = r.sample(range(1, SLOTS + 1), min(n, SLOTS))
    rows = {i: s for i, s in zip(ids, slots)}
    for extra in r.sample(range(0, MAXID + 1), 4):
        rows.setdefault(extra, r.randint(0, SLOTS) if extra == 0 else 0)
    if r.random() < 0.3:
        rows[0] = r.randint(1, SLOTS)
    ids = ids[:len(slots)]
    stamp = r.getrandbits(32)
    turn = (stamp + r.choice([0, 0, 1, 2, 3, 4, 5, 7, 8, 100, 1 << 31, MASK])) & MASK if r.random() < 0.7 else r.getrandbits(32)
    c = dict(worked=r.choice([0, 1, 1, 2, 0x80, 0xFF, 0x100]) if r.random() < 0.9 else r.randrange(256), stamp=stamp, turn=turn, val=rand_val(r),
             rows=rows, things=[(r.choice(KINDS), r.getrandbits(16)) for _ in range(SLOTS)],
             head=ids[0] if ids else 0, nxt=ids[1:] + [0],
             inside=[r.choice(INSIDE_ANSWERS) for _ in range(SLOTS)],
             get=rand_val(r), gvip=[rand_val(r) for _ in range(3)])
    if r.random() < 0.3:
        c["gvip"] = [c["gvip"][0]] * 3
    c["worked"] &= 0xFF
    c.update(kw)
    return c


def analyser_queues(c):
    return {"head": [c["head"]], "next": c["nxt"], "inside": c["inside"], "get": [c["get"]], "gvip": c["gvip"]}


def analyser_edges():
    out = []
    r = random.Random(11)
    for worked in (0, 1, 0x80):
        for secs in (0, 1, 4, 8):
            for get in (-3, -2, -1, 0, 1, 199, 200, 201, INT_MIN, INT_MAX):
                for gvip in ([0, 0, 0], [398, 398, 398], [399, 399, 399], [400, 400, 400], [-1, -1, -1], [-2, -2, -2], [-3, -3, -3], [INT_MIN] * 3,
                             [INT_MAX] * 3, [401, 5, -7], [-3, 600, 1], [200, 1, 399]):
                    stamp = r.getrandbits(32) & ~3
                    out.append(analyser_case(r, worked=worked, stamp=stamp, turn=(stamp + secs * 4) & MASK, get=get, gvip=gvip))
    r2 = random.Random(12)
    out = r2.sample(out, 170)
    # walks of every length, every kind of answer
    for n in range(0, 7):
        ids = list(range(1, n + 1))
        slots = list(range(1, n + 1))
        base = analyser_case(r, worked=0, rows={**{i: s for i, s in zip(ids, slots)}, 0: 0}, head=ids[0] if ids else 0, nxt=ids[1:] + [0])
        for answers in ([0] * 8, [1] * 8, [0x100] * 8, [0x80000000] * 8, [0, 1] * 4, [1, 0] * 4):
            for kinds in ([1] * 8, [0] * 8, [1, 2] * 4, [2, 1] * 4):
                out.append(dict(base, inside=answers, things=[(k, 0) for k in kinds]))
    out.append(analyser_case(r, rows={0: 3, 1: 1, 2: 2}, head=1, nxt=[2, 0], things=[(1, 0)] * 8))      # row 0 taken: the assertions read false
    out.append(analyser_case(r, rows={0: 1}, head=0, nxt=[0], worked=1, turn=0, stamp=0x7FFFFFFC))     # an empty list
    return out


def howbusy_ppc(e, c):
    L, S, a = build_ppc(e, c, analyser_queues(c))
    saved = S.activate(e)
    try:
        r3, _, _ = T.call_ppc(e, HOWBUSY, [T.I(a["analyser"])])
    finally:
        CL.Stubs.deactivate(e, saved)
    return dict(ret=s32(r3), log=L.log)


def howbusy_host(lib, c):
    host_setup(lib, c, analyser_queues(c))
    lib.tpw_pr_howbusy.restype = C.c_int32
    try:
        ret = lib.tpw_pr_howbusy()
    finally:
        log = CL.host_end(lib)
    return dict(ret=ret, log=log)


def visitors_ppc(e, c):
    L, S, a = build_ppc(e, c, analyser_queues(c))
    saved = S.activate(e)
    try:
        r3, _, _ = T.call_ppc(e, VISITORS, [T.I(a["analyser"])])
    finally:
        CL.Stubs.deactivate(e, saved)
    return dict(ret=s32(r3), log=L.log)


def visitors_host(lib, c):
    host_setup(lib, c, analyser_queues(c))
    lib.tpw_pr_visitors.restype = C.c_int32
    try:
        ret = lib.tpw_pr_visitors()
    finally:
        log = CL.host_end(lib)
    return dict(ret=ret, log=log)


# ------------------------------------------------------------------ CountNeighboursOfType


def count_case(r, **kw):
    if "ident" in kw:
        pass
    elif r.random() < 0.85:
        x, y = r.randrange(0, 128), r.randrange(0, 14)
        if r.random() < 0.3:
            x = r.choice([0, 1, 2, 125, 126, 127])
        if r.random() < 0.3:
            y = r.choice([0, 1, 2, 3])
        kw["ident"] = y * 128 + x + 1
    else:
        kw["ident"] = r.choice([0, 1, 128, 129, 0x4000, 0x4001, 0x4080, 0xFFFF, 0x2000, 0x8000, 0x7FFF, 0xC000])
    ident = kw["ident"]
    x, y = ((ident - 1) & 0x7F, ((ident - 1) & MASK) >> 7)
    a = r.choice([0, 0, 0, 1, 1, 2, 3, 4, 5, 8]) if r.random() < 0.9 else r.choice([20, 127, 128, 200, 1000])
    if r.random() < 0.12:
        a = -r.randint(1, 3)
        if x + a < 0:
            a = 0
    a = kw.get("a", a)
    model = r.choice([1, 2, 3]) if r.random() < 0.6 else r.choice([4, 0x13, 0xFF, 0x100, 0x101, -1, 0, INT_MAX, INT_MIN, 0x80000001, 0x81, r.randint(0, 8)])
    model = kw.get("model", model)
    b = r.choice([0, 0, 0, 0, 1, -1, 0x100, INT_MIN, 7])
    # the things: up to eight, each in a cell of the window (or out of it), chained by their next links
    nthings = r.choice([0, 1, 2, 3, 4, 6, 8, 8])
    cells = {}
    ids = r.sample(range(1, SLOTS + 1), nthings)
    pool = list(ids)
    ra = max(a, 0)
    while pool:
        k = r.random()
        if k < 0.65:      # inside the window (the cell itself often)
            cx, cy = min(127, max(0, x + r.randint(-ra, ra))), min(127, max(0, y + r.randint(-ra, ra)))
            if r.random() < 0.4:
                cx, cy = min(x, 127), min(y, 127)
        elif k < 0.85:    # next to the window
            cx, cy = min(127, max(0, x + r.randint(-ra - 1, ra + 1))), min(127, max(0, y + r.randint(-ra - 1, ra + 1)))
        else:
            cx, cy = r.randrange(0, 128), r.randrange(0, 128) if r.random() < 0.3 else r.randrange(0, 16)
        n = r.randint(1, min(3, len(pool)))
        chain, pool = pool[:n], pool[n:]
        cells.setdefault(cy * 128 + cx, []).extend(chain)
    things = [None] * SLOTS
    # chains: the cell's list head is the first id; each thing's next is the following id
    who = {}
    for index, chain in cells.items():
        who[index] = chain[0]
        for i, tid in enumerate(chain):
            things[tid - 1] = [r.choice([model & 0xFF, model & 0xFF, 1, 2, 3, 0xFF]) if model >= 0 else r.choice([1, 2, 3, 0xFF]),
                               chain[i + 1] if i + 1 < len(chain) else 0]
    for tid in range(1, SLOTS + 1):
        if things[tid - 1] is None:
            things[tid - 1] = [r.choice([1, 2, 3]), 0]
    rows = {tid: tid for tid in range(1, SLOTS + 1)}
    rows[0] = r.randint(1, SLOTS) if r.random() < 0.15 else 0
    c = dict(model=model, ident=ident, garbage=r.getrandbits(16), a=a, b=b, who=who, rows=rows, things=[tuple(t) for t in things])
    c.update(kw)
    return c


def count_edges():
    r = random.Random(21)
    out = []
    # a thing of every kind in the start cell, every b, a few radii
    for model in (1, 2, 0xFF, 0x101, -1):
        for b in (0, 1):
            for a in (0, 1, 2):
                for ident in (1, 129, 130, 0x2000 + 5):
                    c = count_case(r, model=model, a=a, b=b, ident=ident)
                    out.append(c)
    for ident in (0, 1, 128, 129, 0x4000, 0x4001, 0x4080, 0xFFFF):
        for a in (0, 1, 3, 127):
            out.append(count_case(r, ident=ident, a=a, b=0, model=1))
    for a in (200, 1000):
        out.append(count_case(r, ident=0x2000 + 64, a=a, b=0, model=1))
        out.append(count_case(r, ident=1, a=a, b=1, model=2))
    return out


def count_args(c, analyser):
    return [T.I(analyser), T.I(c["model"] & MASK), T.I(((c["ident"] & 0xFFFF) << 16) | c["garbage"]), T.I(c["a"] & MASK), T.I(c["b"] & MASK)]


def count_ppc(e, c):
    L, S, a = build_ppc(e, c, {})
    cells = bytearray(CELLS * 0x44)
    for index, w in c["who"].items():
        struct.pack_into(">H", cells, index * 0x44 + 0x24, w)
    block = e.alloc(len(cells), bytes(cells))
    e.write(G_MAP, struct.pack(">I", block))
    saved = S.activate(e)
    try:
        r3, _, _ = T.call_ppc(e, COUNT, count_args(c, a["analyser"]))
    finally:
        CL.Stubs.deactivate(e, saved)
    return dict(ret=s32(r3), log=L.log)


def count_host(lib, c):
    lib.tpw_pr_cells_clear()
    lib.tpw_pr_cell.argtypes = [C.c_int32, C.c_int32]
    for index, w in c["who"].items():
        lib.tpw_pr_cell(index, w)
    lib.tpw_pr_globals.argtypes = [C.c_int32]
    lib.tpw_pr_globals(1)
    host_setup(lib, c, {})
    lib.tpw_pr_count.restype = C.c_int32
    lib.tpw_pr_count.argtypes = [C.c_int32, C.c_uint32, C.c_int32, C.c_int32]
    try:
        ret = lib.tpw_pr_count(s32(c["model"]), c["ident"] & 0xFFFF, s32(c["a"]), s32(c["b"]))
    finally:
        log = CL.host_end(lib)
    return dict(ret=ret, log=log)


# ------------------------------------------------------------------ the two text objects' Change

NUM_FLAGS = [0, 1, 8, 0x80, 0x88, 0x100, 0x108, 0x180, 0xFFFFFFFF, 0x7F, 4, 0x40, 0x10, 0x888, 0x77, 0x180 ^ 0x80]
NUM_VALUES = [0, 1, -1, 2, 10, 99, 100, 1000, 1024, 1023, 12345, 16777216, 16777217, 33554435, 0x7FFFFFFF, INT_MIN, INT_MIN + 1, -1024, -1023, 0x3FFFFFFF,
              0x1000000F, 0x00FFFFFF, 0x02000001, 0x7FFFFF80, 0x7FFFFFC0, 0x7FFFFFBF]
CASH_VALUES = [0, 1, -1, 2, 99, 100, 999999, -999999, 0xEEEEEEE, 0xEEEEEEE - 1, 0xEEEEEEE + 1, 0xEEEEEEE + 2, 0xEEEEEEF, -0xEEEEEEE, -0xEEEEEEE + 1,
               -0xEEEEEEE - 1, -0xEEEEEEE - 2, -0xEEEEEEF, 0x7FFFFFFF, INT_MIN, INT_MIN + 1, INT_MIN + 0xEEEEEEE, INT_MIN + 0xEEEEEEE + 1, 0x1DDDDDDC,
               0x1DDDDDDD, -0x1DDDDDDC, 0x7FFFFFFF - 0xEEEEEEE, 0x10000000, -0x10000000]


def change_case(r, values, flag_pool):
    k = r.random()
    value = r.choice(values) if k < 0.6 else (s32(r.getrandbits(32)) if k < 0.85 else r.randint(-300, 300))
    return dict(flags=r.choice(flag_pool) if r.random() < 0.8 else r.getrandbits(32), value=value, garbage=r.getrandbits(32))


def change_edges(values, flag_pool):
    return lambda: [dict(flags=f, value=v, garbage=0x12345678) for f in flag_pool for v in values]


def change_ppc(kind):
    def fn(e, c):
        L, S, a = build_ppc(e, {}, {})
        e.write(a["change"], struct.pack(">i", c["value"]))
        text = e.alloc(16, wbytes("pass"))
        style = e.alloc(0x20, struct.pack(">I", text) + b"\xa5" * 0x1C)
        e.write(a["change"] + 0x10, struct.pack(">I", style))
        e.write(G_TEXT + 0x564, struct.pack(">I", a["table"]))
        saved = S.activate(e)
        try:
            T.call_ppc(e, NUMCHG if kind == 0 else CASHCHG, [T.I(a["obj"]), T.I(c["flags"]), T.I(a["change"])])
        finally:
            CL.Stubs.deactivate(e, saved)
        return dict(word=s32(e.u32(a["change"])), log=L.log)
    return fn


def change_host(kind):
    def fn(lib, c):
        host_setup(lib, {}, {})
        lib.tpw_pr_globals.argtypes = [C.c_int32]
        lib.tpw_pr_globals(1)
        lib.tpw_pr_change.argtypes = [C.c_int32, C.c_uint32, C.c_int32]
        try:
            lib.tpw_pr_change(kind, c["flags"], c["value"])
        finally:
            log = CL.host_end(lib)
        return dict(word=c["value"], log=log)
    return fn


def make(title, ppc, host, edge, rnd):
    return Base(title, ppc, host, edge=edge, rnd=rnd)


TESTS = [
    make("CParkAnalyser::HowBusyIsPark", howbusy_ppc, howbusy_host, analyser_edges, analyser_case),
    make("CParkAnalyser::NumberOfVisitorsInPark", visitors_ppc, visitors_host, analyser_edges, analyser_case),
    make("CParkAnalyser::CountNeighboursOfType", count_ppc, count_host, count_edges, count_case),
    make("InterfaceNumberTextObject::Change", change_ppc(0), change_host(0), change_edges(NUM_VALUES, NUM_FLAGS),
         lambda r: change_case(r, NUM_VALUES, NUM_FLAGS)),
    make("InterfaceCashAndGTickTextObject::Change", change_ppc(1), change_host(1), change_edges(CASH_VALUES, [0, 0x80, 0x100, 0x180, 0xFFFFFFFF, 0x7F]),
         lambda r: change_case(r, CASH_VALUES, [0, 0x80, 0x80, 0x80, 0x100, 0x180, 0xFFFFFFFF, 0x7F])),
]
