"""Differential tests for the key tables' KeyDown and KeyUp (src/game/keyboard.cpp, unit w50), against
libtpw_unit_KeyTables.dylib (tests/unit_KeyTables.cpp).

  CKeyTable::KeyDown 0x1014fdec, CKeyTable::KeyUp 0x1014fef0

The Mac table (0x18 bytes) and its entries (0x14 bytes: id, key, modifiers, state, a word, the up action, the down action) are
built by hand in emulated memory; the host's are the structs of include/keyboard.h (wider, with native action pointers). An entry's
actions are one of four: none, A, B, or "move" (which copies the entries to a second block and points the table at it, to show that
the state is stored through the table after the call). A and B and "move" are fakes on both sides that record (which, argument) and
answer from the case's queue; the PowerPC ones are stubs at three unused code addresses behind TVectors. A case is a table and a
script of KeyDown and KeyUp calls; after each, the answer, the block in use, the count and enabled words and both blocks' records
(the Mac's second block is filled with 0xa5 bytes, like the host's) are compared, and the call log at the end. The key and modifiers
reach the PowerPC side with junk in the upper half of the register (the function sign-extends the low half).
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_KeyTables"

KEY_DOWN, KEY_UP = 0x1014FDEC, 0x1014FEF0
# unused code addresses (InterfaceWindow's empty virtuals) that stand in for the three actions
ACTION_CODE = {1: 0x1018A270, 2: 0x1018A2A8, 3: 0x1018A9D4}
MINUS = 0xFFFFFFFF
BLOCK_ENTRIES = 8
ENTRY = 0x14
WORDS = 7


def s16(v):
    return T.s16(v & 0xFFFF)


def ppc(e, case):
    queue = {"Action": case["answers"]}
    e.cl = CL.PpcLog({}, queue)
    S = CL.Stubs()
    total = len(case["entries"])
    blocks = [e.alloc(BLOCK_ENTRIES * ENTRY, b"\xa5" * (BLOCK_ENTRIES * ENTRY)) for _ in range(2)]
    table = e.alloc(0x18, b"\xa5" * 0x18)
    descriptor = {}
    for action, code in ACTION_CODE.items():
        descriptor[action] = e.alloc(8, struct.pack(">II", code, e.toc))
    by_descriptor = {v: k for k, v in descriptor.items()}

    def make_stub(action):
        def stub(emu):
            emu.cl.rec("Action", (action, T.s32(emu.r(3))))
            emu.set_r(3, T.u32(emu.cl.ret("Action")))
            if action == 3:
                emu.write(blocks[1], bytes(emu.read(blocks[0], BLOCK_ENTRIES * ENTRY)))
                emu.write(table, struct.pack(">I", blocks[1]))
        return stub

    for action, code in ACTION_CODE.items():
        S.internal(code, make_stub(action))
    for i, w in enumerate(case["entries"]):
        at = blocks[0] + ENTRY * i
        e.write(at, struct.pack(">4hI", s16(w[0]), s16(w[1]), s16(w[2]), s16(w[3]), w[4] & MINUS))
        e.write(at + 0xC, struct.pack(">II", descriptor.get(w[5], 0), descriptor.get(w[6], 0)))
    e.write(table, struct.pack(">I2h", blocks[0], s16(case["count"]), s16(case["enabled"])))
    out = []
    saved = S.activate(e)
    try:
        for op, key, mods, junk in case["calls"]:
            r3 = T.call_ppc(e, KEY_DOWN if op == 0 else KEY_UP, [table, (key & 0xFFFF) | (junk << 16), (mods & 0xFFFF) | (junk << 16 if junk else 0)])[0]
            out.append(T.s32(r3))
            current = e.u32(table)
            out.append(1 if current == blocks[1] else 0)
            count, enabled = struct.unpack(">2h", e.read(table + 4, 4))
            out += [count, enabled]
            for block in blocks:
                for i in range(total):
                    at = block + ENTRY * i
                    ident, key_, mods_, state, word = struct.unpack(">4hI", e.read(at, 12))
                    up, down = struct.unpack(">II", e.read(at + 0xC, 8))
                    out += [ident, key_, mods_, state, T.s32(word), by_descriptor.get(up, 0), by_descriptor.get(down, 0)]
        return out, e.cl.log
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    queues = {"Action": case["answers"]}
    CL.host_begin(lib, {}, queues)
    total = len(case["entries"])
    flat = [v for w in case["entries"] for v in w]
    calls = [v for op, key, mods, _ in case["calls"] for v in (op, s16(key), s16(mods))]
    per_call = 4 + 2 * total * WORDS
    out = (C.c_int32 * (per_call * len(case["calls"]) + 1))()
    lib.tpw_kt_run.argtypes = [C.c_int32] * 3 + [C.POINTER(C.c_int32), C.c_int32, C.POINTER(C.c_int32), C.POINTER(C.c_int32)]
    lib.tpw_kt_run(case["enabled"], case["count"], total, (C.c_int32 * (len(flat) + 1))(*flat), len(case["calls"]),
                   (C.c_int32 * (len(calls) + 1))(*calls), out)
    return list(out)[:per_call * len(case["calls"])], CL.host_end(lib)


# ---- the cases

KEYS = [0x41, 0x42, 0, -1, 0x7FFF, -0x8000, 0x100, 0x1234, 0x20, 0xFF]
MODS = [0, 1, 2, 3, 0x10, -1, 0x7FFF, -0x8000, 0x100]
ANSWERS = [0, 7, -1, 0x12345678, -0x80000000, 1]


def entry(key, mods, state=0, up=0, down=0, ident=None, word=None):
    return (ident if ident is not None else (key * 3 + mods) & 0x7FFF, key, mods, state, 0x0A0B0C0D if word is None else word, up, down)


def case(enabled, count, entries, calls, answers=(0,)):
    return dict(enabled=enabled, count=count, entries=entries, calls=calls, answers=list(answers))


def edges():
    # enabled or not, hits and misses, both ways, every action
    for enabled in (0, 1, -1, 2, 0x100):
        for table in ([], [entry(0x41, 0, 0, 1, 2)], [entry(0x41, 0, 1, 1, 2), entry(0x42, 1, 0, 2, 1), entry(0x43, 2, 1, 0, 0)]):
            count = len(table)
            calls = [(0, 0x41, 0, 0), (1, 0x41, 0, 0), (0, 0x42, 1, 0), (1, 0x42, 1, 0), (0, 0x43, 2, 0), (1, 0x43, 2, 0), (0, 0x41, 1, 0),
                     (1, 0x42, 0, 0), (0, 0x44, 0, 0), (1, 0x44, 0, 0)]
            yield case(enabled, count, table, calls, ANSWERS)
    # an action that answers 0, 7 and others; none; both slots set apart; state before 0, 1 and 7
    for state in (0, 1, 7, -1):
        for up in range(4):
            for down in range(4):
                for answer in (0, 7, -3):
                    table = [entry(0x41, 0, state, up, down)]
                    yield case(1, 1, table, [(0, 0x41, 0, 0), (1, 0x41, 0, 0), (0, 0x41, 0, 0), (1, 0x41, 0, 0)], [answer])
    # a hit at index 0 and at 2, a miss by key and by modifiers, the first of two equal entries wins
    base = [entry(0x41, 0, 5, 1, 2), entry(0x42, 0, 5, 2, 1), entry(0x43, 4, 5, 1, 1), entry(0x43, 4, 6, 2, 2)]
    for count in (0, 1, 2, 3, 4, 5, -1, -0x8000, 0x7FFF):
        for key, mods in ((0x41, 0), (0x42, 0), (0x43, 4), (0x43, 0), (0x41, 4), (0x44, 4), (0x42, 4), (0x41, 1)):
            yield case(1, count, base, [(0, key, mods, 0), (1, key, mods, 0), (0, key, mods, 0)], [3, 4, 5])
    # entries past the count that would match are not seen; a matching entry before a non-matching key-only one
    yield case(1, 2, [entry(0x41, 1), entry(0x42, 0), entry(0x43, 0, 0, 1, 1)], [(0, 0x43, 0, 0), (1, 0x43, 0, 0), (0, 0x41, 0, 0)])
    yield case(1, 3, [entry(0x41, 1, 0, 1, 1), entry(0x41, 0, 0, 2, 2), entry(0x41, 0, 0, 1, 1)], [(0, 0x41, 0, 0), (1, 0x41, 0, 0)])
    # the key and modifiers are 16-bit values: junk in the upper half of the registers is ignored; extreme values match
    for key in KEYS:
        for mods in MODS:
            for junk in (0, 0x5A5A, 0xFFFF, 0x8000):
                yield case(1, 2, [entry(key, mods, 0, 1, 2), entry(0x55, 0, 0, 2, 1)], [(0, key, mods, junk), (1, key, mods, junk)])
    # the move action: the state is stored in the block the table points at after the call
    for action in (1, 2, 3):
        yield case(1, 3, [entry(0x41, 0, 0, 3, 3), entry(0x42, 0, 1, action, 3), entry(0x43, 0, 1, 3, action)],
                   [(0, 0x42, 0, 0), (1, 0x42, 0, 0), (0, 0x41, 0, 0), (1, 0x41, 0, 0), (0, 0x43, 0, 0), (1, 0x43, 0, 0)], [1, 2, 3])
    yield case(1, 8, [entry(k, 0, 1, 3, 3) for k in range(8)], [(0, k, 0, 0) for k in range(8)] + [(1, k, 0, 0) for k in range(8)])


def rnd(r):
    n = r.randint(0, BLOCK_ENTRIES)
    keys = [r.choice(KEYS) for _ in range(3)]
    mods = [r.choice(MODS) for _ in range(3)]
    table = [entry(r.choice(keys), r.choice(mods), r.choice([0, 1, 7, -1]), r.choice([0, 0, 1, 2, 3]), r.choice([0, 0, 1, 2, 3]),
                   r.getrandbits(15), r.getrandbits(32)) for _ in range(n)]
    count = r.choice([n, n, n, r.randint(-2, BLOCK_ENTRIES)])
    calls = [(r.randrange(2), r.choice(keys + [0x99]), r.choice(mods + [0x77]), r.choice([0, 0, 0, r.getrandbits(16)])) for _ in range(r.randint(1, 8))]
    return case(r.choice([0, 1, 1, 1, 1, r.randint(-5, 5)]), count, table, calls, [r.choice(ANSWERS) for _ in range(r.randint(1, 4))])


TESTS = [FnTest("CKeyTable::KeyDown, CKeyTable::KeyUp", ppc, host, edge=edges, rnd=rnd)]
