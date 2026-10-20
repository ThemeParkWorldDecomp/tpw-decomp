"""The world's construction (unit w113, main app): CGame::InitWorld, the CWorld constructor, Allocate, AllocateThingHeap,
InitThingHeap, the staff database and staff record constructors, the park time constructor and Reset, the arrival constructor, the
three cell constructors, the two grid registrations, ObjectControl's empty constructor and CBlockSave::RegisterSaveBlock.

The functions run as the original on the PowerPC side and as src/ on the host. Everything they call outside the unit is a fake that
records the same event on both sides: operator new (InitWorld; the case can make it fail), TbTimeStamp::SetTime (the stamp as an
offset from the object) and NS_MacDoze::GetLocalTime (it fills the case's eight 16-bit values). CExitError::SetError is real. The
element constructors of the world's arrays run as the original on both sides (the PowerPC side through the standard
__construct_array stub, which calls the original constructor descriptors).

Each case builds the object over fill bytes (0x00, 0xa5, 0xff or random) with guard bytes after it, and fills every global the
functions might touch (the world pointers, the heads of the thing lists, the save table, the grid registry, the exit error, the whole
thing table) with random words. After the call the test compares the answer, the call log, the global state, the thing table, the
bytes the function writes (by value: big-endian words on the Mac, native on the host) and the cell arrays, and checks that every
other byte of the object and its guard still holds the fill. The Mac's pointers are 32-bit words and the host's are 64-bit, so a
pointer is reported as 0, an offset into the object, a thing table index or its raw low 32 bits.

The 49,152 element constructor calls of one CWorld::CWorld are slow on the PowerPC side: the edge cases construct every element, the
random cases construct a sample of each big array (the first and last few and some random ones) and compare those elements.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from ppcemu import ImportCalled
from tpwtest import FnTest

SHIM_LIBRARY = "unit_WorldInit"
MAIN = "SimTheme Park"

# The functions of the unit, by op number (the host's tpw_wi_call uses the same numbers).
(INIT_WORLD, CWORLD, ALLOCATE, ALLOC_HEAP, INIT_HEAP, STAFFDB, STAFFINFO, PARKTIME, RESET, ARRIVAL, MAPCELL, TRACKCELL, REG_MAP,
 REG_TRACK, REGION, OBJCTRL, SAVEBLOCK) = range(17)
ADDR = [0x10147E34, 0x10139F70, 0x1013A1C0, 0x1013AADC, 0x1013AB48, 0x10128ED8, 0x10128390, 0x10111F14, 0x1011275C, 0x100F1FF4,
        0x10102948, 0x1012FB08, 0x100F4780, 0x100F47E0, 0x10120C10, 0x1013A10C, 0x100F46EC]
OWN = set(ADDR)

WORLD_SIZE = 0x1DA748
BLOCK_SIZE = WORLD_SIZE + 0x100          # the host's gBlock; the world ops use all of it
GUARD = 0x40
SMALL = {STAFFDB: 0x2A0, STAFFINFO: 0x14, PARKTIME: 0x24, RESET: 0x24, ARRIVAL: 0x14, MAPCELL: 0x44, TRACKCELL: 0x28, REGION: 10,
         OBJCTRL: 0x20}
WORLD_OPS = {INIT_WORLD, CWORLD, ALLOCATE, ALLOC_HEAP, INIT_HEAP}
TABLE_OPS = WORLD_OPS                    # the ops that fill the thing table and compare it afterwards
RETURNS = {INIT_WORLD, ALLOCATE, ALLOC_HEAP}

TABLE = 0x1034CBFC
TABLE_ENTRIES = 0x2800
TABLE_BYTES = TABLE_ENTRIES * 0x14
WORLD_TAG, TABLE_TAG = 1 << 40, 2 << 40

# The state words, in the order of tpw_wi_set_state: (PowerPC address, kind) with kind p (pointer) or w (word).
STATE = [(0x1037EBFC, "p"), (0x1037EC0C, "p"), (0x1037EC08, "p"), (0x1034CBF4, "p"), (0x1034CBF8, "p"),
         (0x1034C1AC, "p"), (0x1034C1B0, "w"), (0x1034C1B4, "p"), (0x1034C1B8, "w"),
         (0x1034C1D0, "p"), (0x1034C1CC, "p"), (0x1034C1C0, "w"),
         (0x1034C1C8, "p"), (0x1034C1C4, "p"), (0x1034C1BC, "w"),
         (0x1037F1C0, "w"), (0x1037F1C4, "w"), (0x1037F1C8, "w")]
EXIT_TEXT, EXIT_TEXT_SIZE = 0x1037F1CC, 0x50

EDGE_WORDS = [0, 1, 2, 5, 8, 0x28, 0x44, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x3FFF, 0x4000, 0x3FFFF, 0x40000,
              0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFE, 0xFFFFFFFF]
FILLS = [0x00, 0xA5, 0xFF, "rand"]


def setup(runner):
    T.install_std_stubs(runner.emu)


# ------------------------------------------------------------------ the objects and their checked bytes (Mac offsets)


def world_fields():
    """(offset, width) of every scalar the world's construction writes."""
    f = []
    for i in range(0x20):
        b = i * 0x14
        f += [(b, 4), (b + 9, 1), (b + 10, 1), (b + 11, 1), (b + 12, 4)]
    f += [(0x294, 4), (0x298, 1)] + [(0x299 + i, 1) for i in range(5)]
    f += [(0x2A0 + 4 * i, 4) for i in range(8)]
    f += [(0x2C4, 4), (0x2C8, 4), (0x2CC, 4), (0x2D0, 4), (0x2D4, 1), (0x2D5, 1)]
    f += [(0x1DA710, 4)] + [(0x1DA73C + 2 * i, 2) for i in range(6)]
    return f


WORLD_ARRAYS = [(0x2D8, 0x44, 0x4000, "map"), (0x1102D8, 0x28, 0x4000, "track"), (0x1B13DC, 10, 0x4000, "region")]


def spec_for(op):
    """(fields, arrays): the scalars the function writes and its cell arrays; every other byte must keep the fill."""
    if op in (INIT_WORLD, CWORLD):
        return world_fields(), WORLD_ARRAYS
    if op == STAFFDB:
        return [(i * 0x14 + o, w) for i in range(0x20) for o, w in ((0, 4), (9, 1), (10, 1), (11, 1), (12, 4))] + \
            [(0x294, 4), (0x298, 1)] + [(0x299 + i, 1) for i in range(5)], []
    if op == STAFFINFO:
        return [(0, 4), (9, 1), (10, 1), (11, 1), (12, 4)], []
    if op == PARKTIME:
        return [(4 * i, 4) for i in range(8)], []
    if op == ARRIVAL:
        return [(0, 4), (4, 4), (8, 4), (12, 4), (0x10, 1), (0x11, 1)], []
    if op == MAPCELL:
        return [(0, 2)] + [(i, 1) for i in range(2, 0x44)], []
    if op == TRACKCELL:
        return [(0, 2)] + [(i, 1) for i in range(2, 0x28)], []
    if op == REGION:
        return [(2 * i, 2) for i in range(5)], []
    return [], []       # Reset, ObjectControl, the registrations: no object byte is written


def block_length(op):
    return BLOCK_SIZE if op in WORLD_OPS else SMALL.get(op, 0) + GUARD


def cell_key(kind, element, order):
    if kind == "region":
        return element.hex()
    return "%04x:%s" % (int.from_bytes(element[:2], order), element[2:].hex())


def block_report(buf, fill, op, little, sample, built=True):
    """The scalars by value, the first byte that differs from the fill outside the written ranges (-1: none) and the distinct cells.
    built False: the function writes nothing in this case (InitWorld whose allocation failed), so the whole object must keep the fill."""
    fields, arrays = spec_for(op) if built else ([], [])
    order = "little" if little else "big"
    values = [int.from_bytes(buf[o:o + w], order) for o, w in fields]
    a, b = bytearray(buf), bytearray(fill)
    for o, w in fields:
        a[o:o + w] = bytes(w)
        b[o:o + w] = bytes(w)
    for o, size, count, _ in arrays:
        a[o:o + size * count] = bytes(size * count)
        b[o:o + size * count] = bytes(size * count)
    first = -1
    if a != b:
        first = next(i for i in range(len(a)) if a[i] != b[i])
    cells = []
    for o, size, count, kind in arrays:
        indexes = range(count) if sample is None else sample
        cells.append(sorted({cell_key(kind, bytes(buf[o + i * size:o + (i + 1) * size]), order) for i in indexes}))
    return {"fields": values, "untouched": first, "cells": cells}


# ------------------------------------------------------------------ the case data both sides share


def safe(word):
    """A random pointer word that is not an address either side gives out (the program image and the emulator's scratch)."""
    if 0x10000000 <= word < 0x10400000 or 0x7D000000 <= word < 0x7D400000:
        word ^= 0x08000000
    return word


def ptr_word(r):
    return 0 if r.random() < 0.125 else safe(r.getrandbits(32))


def make_fill(mode, rng, n):
    return rng.randbytes(n) if mode == "rand" else bytes([mode]) * n


def table_words(mode, rng):
    n = TABLE_ENTRIES * 5
    if mode == "a5":
        return [0xA5A5A5A5] * n
    if mode == "zero":
        return [0] * n
    if mode == "ones":
        return [0xFFFFFFFF] * n
    words = [rng.getrandbits(32) for _ in range(n)]
    for i in range(0, n, 5):
        for k in (0, 2, 3, 4):
            words[i + k] = safe(words[i + k])
    return words


def light_sample(rng):
    picks = set(range(4)) | set(range(0x4000 - 4, 0x4000)) | {rng.randrange(0x4000) for _ in range(24)}
    return sorted(picks)


def built_in(c):
    """False for an InitWorld whose operator new answers NULL: nothing is constructed."""
    return not (c["op"] == INIT_WORLD and c["q"].get("newnull", [0])[0] != 0)


def prepare(c):
    rng = random.Random(c["seed"])
    op = c["op"]
    d = {"fill": make_fill(c["fill"], rng, block_length(op))}
    state = []
    for _, kind in STATE:
        state.append(ptr_word(rng) if kind == "p" else (rng.choice(EDGE_WORDS) if rng.random() < 0.4 else rng.getrandbits(32)))
    d["state"] = state
    d["text"] = rng.randbytes(EXIT_TEXT_SIZE)
    d["time"] = [rng.choice((0, 0xFFFF, 0x7FFF, 0x8000, 1)) if rng.random() < 0.4 else rng.getrandbits(16) for _ in range(8)]
    d["table"] = table_words(c["table"], rng) if op in TABLE_OPS else None
    d["sample"] = light_sample(rng) if c["sample"] == "light" else None
    return d


# ------------------------------------------------------------------ the PowerPC side


def canon_ppc(word, block):
    if word == 0:
        return 0
    if block <= word < block + BLOCK_SIZE:
        return WORLD_TAG + word - block
    off = word - TABLE
    if 0 <= off < TABLE_BYTES:
        return TABLE_TAG + off // 0x14 if off % 0x14 == 0 else (3 << 40) + off
    return word


def ppc_case(e, c, d):
    op = c["op"]
    L = CL.PpcLog({}, c.get("q"))
    e.cl = L
    fill = d["fill"]
    block = e.alloc(len(fill), fill)
    st = CL.Stubs()
    st.real = set(OWN)

    def op_new(emu):
        fail = L.ret("newnull", 0) != 0
        L.rec("new", [emu.r(3), int(fail)])
        emu.set_r(3, 0 if fail else block)

    def set_time(emu):
        # r3 is the hidden TbError buffer, r4 the stamp, r5 to r10 the first six values, the seventh is on the stack.
        L.rec("SetTime", [emu.r(4) - block] + [T.s32(emu.r(i)) for i in range(5, 11)] + [T.s32(emu.u32(emu.r(1) + 0x38))])

    def get_local_time(emu):
        L.rec("GetLocalTime")
        emu.write(emu.r(3), b"".join(v.to_bytes(2, "big") for v in d["time"]))

    st.imp("__nw__FUl", op_new)
    st.imp("SetTime__11TbTimeStampFiiiiiii", set_time)
    st.imp("GetLocalTime__10NS_MacDozeFPQ210NS_MacDoze11_SYSTEMTIME", get_local_time)
    saved = st.activate(e)
    out = {}
    try:
        sample = d["sample"]

        def construct(emu, call):
            base, ctor, size, count = emu.r(3), emu.r(4), emu.r(6), emu.r(7)
            for i in (sample if sample is not None and count == 0x4000 else range(count)):
                call(ctor, base + i * size, 1)
        T.stub_cb(e, "__construct_array", construct)
        for (addr, _), word in zip(STATE, d["state"]):
            e.write(addr, word.to_bytes(4, "big"))
        e.write(EXIT_TEXT, d["text"])
        if d["table"] is not None:
            e.write(TABLE, b"".join(w.to_bytes(4, "big") for w in d["table"]))
        a, b, cc, dd = (list(c["args"]) + [0, 0, 0, 0])[:4]
        if op == SAVEBLOCK:
            args = [0x1034C1AC, a, b, cc, dd]
        elif op in (REG_MAP, REG_TRACK):
            args = [a, b]
        elif op == INIT_WORLD:
            args = [a]
        else:
            args = [block]
        r3 = T.call_ppc(e, ADDR[op], args, max_insns=2_000_000_000, timeout_us=3_000_000_000)[0]
        out["result"] = r3 & 0xFFFFFFFF if op in RETURNS else 0
        out["state"] = [canon_ppc(e.u32(addr), block) if kind == "p" else e.u32(addr) for addr, kind in STATE]
        out["text"] = list(e.read(EXIT_TEXT, EXIT_TEXT_SIZE))
        if d["table"] is not None:
            raw = e.read(TABLE, TABLE_BYTES)
            words = [int.from_bytes(raw[i:i + 4], "big") for i in range(0, TABLE_BYTES, 4)]
            for i in range(0, len(words), 5):
                words[i] = canon_ppc(words[i], block)
                for k in (2, 3, 4):
                    words[i + k] = canon_ppc(words[i + k], block)
            out["table"] = words
        out.update(block_report(e.read(block, len(fill)), fill, op, False, sample, built_in(c)))
    finally:
        CL.Stubs.deactivate(e, saved)
    out["log"] = L.log
    return out


# ------------------------------------------------------------------ the host side


def host_case(lib, c, d):
    op = c["op"]
    fill = d["fill"]
    lib.tpw_wi_load.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_wi_load(fill, len(fill))
    lib.tpw_wi_set_time.argtypes = [C.POINTER(C.c_uint16)]
    lib.tpw_wi_set_time((C.c_uint16 * 8)(*d["time"]))
    lib.tpw_wi_set_state.argtypes = [C.POINTER(C.c_uint32), C.c_char_p]
    lib.tpw_wi_set_state((C.c_uint32 * len(STATE))(*d["state"]), d["text"])
    if d["table"] is not None:
        lib.tpw_wi_set_table.argtypes = [C.POINTER(C.c_uint32)]
        lib.tpw_wi_set_table((C.c_uint32 * len(d["table"]))(*d["table"]))
    a, b, cc, dd = (list(c["args"]) + [0, 0, 0, 0])[:4]
    out = {}
    CL.host_begin(lib, {}, c.get("q"))
    try:
        lib.tpw_wi_call.restype = C.c_int64
        lib.tpw_wi_call.argtypes = [C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32]
        r = lib.tpw_wi_call(op, a, b, cc, dd)
        out["result"] = r & 0xFFFFFFFF if op in RETURNS else 0
    finally:
        log = CL.host_end(lib)
    state = (C.c_int64 * len(STATE))()
    text = C.create_string_buffer(EXIT_TEXT_SIZE)
    lib.tpw_wi_get_state.argtypes = [C.POINTER(C.c_int64), C.c_char_p]
    lib.tpw_wi_get_state(state, text)
    out["state"] = list(state)
    out["text"] = list(text.raw)
    if d["table"] is not None:
        table = (C.c_int64 * (TABLE_ENTRIES * 5))()
        lib.tpw_wi_get_table.argtypes = [C.POINTER(C.c_int64)]
        lib.tpw_wi_get_table(table)
        out["table"] = list(table)
    buf = C.create_string_buffer(len(fill))
    lib.tpw_wi_dump.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_wi_dump(buf, len(fill))
    out.update(block_report(buf.raw, fill, op, True, d["sample"], built_in(c)))
    out["log"] = log
    return out


class WorldTest(FnTest):
    """One function of the unit: the case data is made once and handed to both sides."""

    def run_case(self, runner, case):
        emu, lib = runner.emu, runner.lib
        emu.reset_scratch()
        data = prepare(case)
        try:
            p = self.ppc(emu, case, data)
        except ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = self.host(lib, case, data)
        return T.same_values(p, h, "", False)


# ------------------------------------------------------------------ CWorld::Allocate with a fake AllocateThingHeap (library unit_WorldAlloc)

ALLOC_LIBRARY = "unit_WorldAlloc"
ALLOC_STATE = [0x1034C1AC, 0x1034C1B0, 0x1034C1B4, 0x1034C1B8, 0x1037F1C0, 0x1037F1C4, 0x1037F1C8]   # save table, exit error
ALLOC_POINTERS = {0, 2}


def alloc_prepare(c):
    rng = random.Random(c["seed"])
    words = [ptr_word(rng) if i in ALLOC_POINTERS else (rng.choice(EDGE_WORDS) if rng.random() < 0.4 else rng.getrandbits(32))
             for i in range(len(ALLOC_STATE))]
    return {"state": words, "text": rng.randbytes(EXIT_TEXT_SIZE)}


def alloc_ppc(e, c, d):
    L = CL.PpcLog({}, c.get("q"))
    e.cl = L
    block = e.alloc(16, b"\xa5" * 16)
    L.labels[block] = 1
    st = CL.Stubs()
    st.real = {ADDR[ALLOCATE], ADDR[SAVEBLOCK]}

    def heap(emu):
        L.rec("AllocateThingHeap", [L.lab(emu.r(3))])
        emu.set_r(3, 1 if L.ret("heap", 1) != 0 else 0)

    st.internal(ADDR[ALLOC_HEAP], heap)
    saved = st.activate(e)
    out = {}
    try:
        for addr, word in zip(ALLOC_STATE, d["state"]):
            e.write(addr, word.to_bytes(4, "big"))
        e.write(EXIT_TEXT, d["text"])
        out["result"] = T.call_ppc(e, ADDR[ALLOCATE], [block])[0] & 0xFF
        out["state"] = [(1 if e.u32(a) == block else e.u32(a)) if i in ALLOC_POINTERS else e.u32(a) for i, a in enumerate(ALLOC_STATE)]
        out["text"] = list(e.read(EXIT_TEXT, EXIT_TEXT_SIZE))
    finally:
        CL.Stubs.deactivate(e, saved)
    out["log"] = L.log
    return out


def alloc_host(lib, c, d):
    lib.tpw_wa_dummy.restype = C.c_void_p
    lib.tpw_wa_set_state.argtypes = [C.POINTER(C.c_uint32), C.c_char_p]
    lib.tpw_wa_set_state((C.c_uint32 * len(ALLOC_STATE))(*d["state"]), d["text"])
    out = {}
    CL.host_begin(lib, {1: lib.tpw_wa_dummy()}, c.get("q"))
    try:
        out["result"] = lib.tpw_wa_call() & 0xFF
    finally:
        log = CL.host_end(lib)
    state = (C.c_int64 * len(ALLOC_STATE))()
    text = C.create_string_buffer(EXIT_TEXT_SIZE)
    lib.tpw_wa_get_state.argtypes = [C.POINTER(C.c_int64), C.c_char_p]
    lib.tpw_wa_get_state(state, text)
    out["state"] = list(state)
    out["text"] = list(text.raw)
    out["log"] = log
    return out


class AllocTest(FnTest):
    """Allocate in the library that has a fake AllocateThingHeap (the PowerPC side hooks the original at its address)."""

    def run_case(self, runner, case):
        emu = runner.emu
        emu.reset_scratch()
        data = alloc_prepare(case)
        p = self.ppc(emu, case, data)
        h = self.host(runner.shim(ALLOC_LIBRARY), case, data)
        return T.same_values(p, h, "", False)


def alloc_case(r, heap=None):
    return {"op": ALLOCATE, "seed": r.getrandbits(32), "q": {"heap": [r.choice((0, 1)) if heap is None else heap]}}


def alloc_edges():
    r = random.Random(6000)
    return [alloc_case(r, heap) for heap in (1, 0, 1, 0, 0, 1)]


def alloc_random(r):
    return alloc_case(r)


# ------------------------------------------------------------------ cases


def mk(op, r, fill=None, args=(), q=None, sample=None, table=None):
    return {"op": op, "seed": r.getrandbits(32), "fill": r.choice(FILLS) if fill is None else fill, "args": tuple(args),
            "q": q or {}, "sample": sample, "table": table or r.choice(("a5", "rand", "zero"))}


def edge_fills(op, r, **kw):
    for fill in FILLS:
        yield mk(op, r, fill=fill, **kw)


def edges_of(op, **kw):
    def edges():
        r = random.Random(1000 + op)
        return list(edge_fills(op, r, **kw))
    return edges


def rnd_of(op, sample=None, args=None):
    def rnd(r):
        return mk(op, r, sample=sample, args=args(r) if args else ())
    return rnd


def cgame(r):
    return (safe(r.getrandbits(32)),)


# CWorld::CWorld and CGame::InitWorld: the edge cases build every element, the random cases a sample.
def world_edges(op):
    def edges():
        r = random.Random(2000 + op)
        cases = []
        for table in ("a5", "rand"):
            for fill in FILLS:
                if op == INIT_WORLD:
                    for fail in (0, 1):
                        cases.append(mk(op, r, fill=fill, args=cgame(r), q={"newnull": [fail]}, table=table))
                else:
                    cases.append(mk(op, r, fill=fill, table=table))
        return cases
    return edges


def init_world_random(r):
    return mk(INIT_WORLD, r, args=cgame(r), q={"newnull": [r.choice((0, 0, 1))]}, sample="light")


def cworld_random(r):
    return mk(CWORLD, r, sample="light")


def table_edges(op):
    def edges():
        r = random.Random(3000 + op)
        return [mk(op, r, fill=fill, table=table) for table in ("a5", "rand", "zero", "ones") for fill in (0xA5, "rand")]
    return edges


def table_random(op):
    def rnd(r):
        return mk(op, r)
    return rnd


GRID_SIZES = [0, 1, 0x44, 0x28, 0x3FFF, 0x4000, 0x3FFFF, 0x40000, 0x40001, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x10000, 0x20000]


def grid_args(r, size=None):
    base = r.choice((0, 0xFFFFFFFF, 0x80000000, 0xFFFFC000, 0xFFFF0000)) if r.random() < 0.3 else safe(r.getrandbits(32))
    size = size if size is not None else (r.choice(GRID_SIZES) if r.random() < 0.6 else r.getrandbits(32))
    return (base, size)


def grid_edges(op):
    def edges():
        r = random.Random(4000 + op)
        return [mk(op, r, args=grid_args(r, size)) for size in GRID_SIZES]
    return edges


def grid_random(op):
    def rnd(r):
        return mk(op, r, args=grid_args(r))
    return rnd


def save_args(r, block=None):
    return (r.choice((0, 1)) if block is None else block, safe(r.getrandbits(32)), r.choice(EDGE_WORDS) if r.random() < 0.5 else r.getrandbits(32),
            r.getrandbits(32))


def save_edges():
    r = random.Random(5000)
    return [mk(SAVEBLOCK, r, args=save_args(r, block)) for block in (0, 1, 0, 1)]


def save_random(r):
    return mk(SAVEBLOCK, r, args=save_args(r))


def test(name, op, edges, rnd):
    return WorldTest(name, ppc_case, host_case, edge=edges, rnd=rnd)


TESTS = [
    test("CGame::InitWorld", INIT_WORLD, world_edges(INIT_WORLD), init_world_random),
    test("CWorld::CWorld", CWORLD, world_edges(CWORLD), cworld_random),
    test("CWorld::Allocate", ALLOCATE, table_edges(ALLOCATE), table_random(ALLOCATE)),
    AllocTest("CWorld::Allocate (fake heap)", alloc_ppc, alloc_host, edge=alloc_edges, rnd=alloc_random),
    test("CWorld::AllocateThingHeap", ALLOC_HEAP, table_edges(ALLOC_HEAP), table_random(ALLOC_HEAP)),
    test("CWorld::InitThingHeap", INIT_HEAP, table_edges(INIT_HEAP), table_random(INIT_HEAP)),
    test("CStaffDB::CStaffDB", STAFFDB, edges_of(STAFFDB), rnd_of(STAFFDB)),
    test("CStaffInfo::CStaffInfo", STAFFINFO, edges_of(STAFFINFO), rnd_of(STAFFINFO)),
    test("CParkTime::CParkTime", PARKTIME, edges_of(PARKTIME), rnd_of(PARKTIME)),
    test("CParkTime::Reset", RESET, edges_of(RESET), rnd_of(RESET)),
    test("CArrival::CArrival", ARRIVAL, edges_of(ARRIVAL), rnd_of(ARRIVAL)),
    test("CMapCell::CMapCell", MAPCELL, edges_of(MAPCELL), rnd_of(MAPCELL)),
    test("CTrackCell::CTrackCell", TRACKCELL, edges_of(TRACKCELL), rnd_of(TRACKCELL)),
    test("CCellBase::RegisterMapGrid", REG_MAP, grid_edges(REG_MAP), grid_random(REG_MAP)),
    test("CCellBase::RegisterTrackGrid", REG_TRACK, grid_edges(REG_TRACK), grid_random(REG_TRACK)),
    test("CRegionEffectCell::CRegionEffectCell", REGION, edges_of(REGION), rnd_of(REGION)),
    test("ObjectControl::ObjectControl", OBJCTRL, edges_of(OBJCTRL), rnd_of(OBJCTRL)),
    test("CBlockSave::RegisterSaveBlock", SAVEBLOCK, save_edges, save_random),
]
