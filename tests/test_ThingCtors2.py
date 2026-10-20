"""Unit w122b: three thing constructors, eight histories, two cached values, cell, challenge and admission fee.

The original element constructors run through __construct_array. External calls are recording fakes.
Every written scalar compares by value; all other object bytes and the guard must retain the original fill.
pow arguments compare as double bits. Its result comes from host libm or a queue on both sides, while the
PowerPC unsigned conversion helper runs for real. This does not compare the original Mac libm implementation.
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ThingCtors2"
ADDR = [0x100E2A4C, 0x100F29F4, 0x100F6640, 0x100F03C4, 0x100F0708, 0x10110FA0,
        0x100EFB70, 0x100EFE40, 0x10111514, 0x100EF230, 0x100E2F3C, 0x100EF460,
        0x100EF5F8, 0x100F0ABC, 0x100F5A34, 0x100F3A40]
NAMES = ["CParkAnalyser::CParkAnalyser", "CBankAccount::CBankAccount", "CChallengeHQ::CChallengeHQ",
         "CHistory_float_144", "CHistory_int_144", "CHistory_int_30", "CHistory_int_5", "CHistory_int_50",
         "CHistory_uint_30", "CParkAnalyserVal_int", "CParkAnalyserCell::CParkAnalyserCell",
         "CParkAnalyserVal_float", "CHistory_uchar_50", "CHistory_object_3", "CChallenge::CChallenge",
         "CBankAccount::SetAdmissionFee"]
SIZE = [0x21C48, 0x128, 0x3E4, 0x250, 0x250, 0x88, 0x24, 0xD8, 0x88, 0xC, 0x7C, 0xC, 0x40, 0x14, 0x30, 0x128]
OWN = set(ADDR) | {0x100E2EF0, 0x100F1D7C, 0x10237CD8}
MASK = 0xFFFFFFFF
FILLS = [0, 0xA5, 0xFF, "rand"]
U32S = [0, 1, 9999, 10000, 10001, 0x7FFFFFFF, 0x80000000, MASK]
APRS = [0, 1, 5, 100, 0x7FFFFFFF, MASK]
MONTHS = [0, 1, 12, 24, 0x7FFFFFFF, 0x80000000, MASK]
TICKS = [0, 1, MASK, 0x100000000, 0x7FFFFFFFFFFFFFFF, 0x8000000000000000, 0xFFFFFFFFFFFFFFFF]
POWERS = [math.nan, math.inf, -math.inf, 0.0, -0.0, -0.5, 1.0, 2147483647.75,
          2147483648.0, 4294967295.5, 4294967296.0, 1e300]
G_WORLD, G_CENTRE, BALANCE = 0x1037EC0C, 0x1037F6C4, 0x102B4568


def s64(value):
    value &= 0xFFFFFFFFFFFFFFFF
    return value - (1 << 64) if value & (1 << 63) else value


def setup(runner):
    T.install_std_stubs(runner.emu)
    # A full analyser constructs more than nine thousand parts; use forty random cases for it.
    run = runner.run_test
    runner.run_test = lambda test, n_random=200, seed=1: run(test, 40 if test.name == NAMES[0] else n_random, seed)


def history(temp, current, count, flag):
    return [(0, temp), (current, 4), (count, 4), (flag, 1)]


def shifted(fields, base):
    return [(base + o, width) for o, width in fields]


VALUE = [(0, 4), (4, 4), (8, 1)]
CELL = [field for i in range(9) for field in shifted(VALUE, 0x10 + 0xC * i)]
CHALLENGE = [(o, 4) for o in (0, 0x20, 0x24, 0x28)] + [(0x2C, 1)]
HISTORY = {3: history(4, 0x244, 0x248, 0x24C), 4: history(4, 0x244, 0x248, 0x24C),
           5: history(4, 0x7C, 0x80, 0x84), 6: history(4, 0x18, 0x1C, 0x20),
           7: history(4, 0xCC, 0xD0, 0xD4), 8: history(4, 0x7C, 0x80, 0x84),
           12: history(1, 0x34, 0x38, 0x3C), 13: history(2, 8, 0xC, 0x10)}


def fields_of(op):
    if op in HISTORY:
        return HISTORY[op]
    if op in (9, 11):
        return VALUE
    if op == 10:
        return CELL
    if op == 14:
        return CHALLENGE
    if op == 15:
        return [(0x118, 4)]
    f = [(i, 1) for i in range(0xC)]  # base fake stamps bytes, no endian conversion
    if op == 0:
        f += [(o, 4) for o in range(0xC, 0x20, 4)]
        for i in range(0x400):
            f += shifted(CELL, 0x20 + 0x7C * i)
        for i in range(4):
            f += shifted(HISTORY[13], 0x1F068 + 0x14 * i)
        for i in range(6):
            f += shifted(VALUE, 0x1F0B8 + 0xC * i)
        for b in [0x1F100 + 0x250 * i for i in range(13)] + [0x20F10, 0x21160, 0x213B0]:
            f += shifted(HISTORY[4], b)
        f += shifted(HISTORY[7], 0x21600) + shifted(HISTORY[6], 0x216D8)
        for i in range(20):
            f += shifted(HISTORY[12], 0x216FC + 0x40 * i)
        f += [(o, 4) for o in range(0x21BFC, 0x21C0C, 4)]
        f += [(o, 8) for o in range(0x21C0C, 0x21C3C, 8)]
        f += [(o, 4) for o in range(0x21C3C, 0x21C48, 4)]
    elif op == 1:
        f += [(o, 4) for o in range(0xC, 0x128, 4)]
    elif op == 2:
        for i in range(20):
            f += shifted(CHALLENGE, 0xC + 0x30 * i)
        f += [(0x3CC, 4), (0x3D0, 8), (0x3D8, 1), (0x3D9, 1), (0x3DA, 1), (0x3DC, 4), (0x3E0, 2)]
    return f


FIELDS = [fields_of(op) for op in range(16)]
UNTOUCHED = [sorted(set(range(SIZE[op] + 0x40)) - {i for o, w in fields for i in range(o, o + w)})
             for op, fields in enumerate(FIELDS)]


def block_report(buf, fill, op, little):
    order = "little" if little else "big"
    values = [int.from_bytes(buf[o:o + w], order) for o, w in FIELDS[op]]
    changed = [(i, buf[i]) for i in UNTOUCHED[op] if buf[i] != fill[i]]
    return {"fields": values, "untouched": changed}


def hash_values(values):
    h = 2166136261
    for v in values:
        h = ((h ^ v) * 16777619) & MASK
    return h


def bits(x):
    return struct.unpack(">q", struct.pack(">d", x))[0]


def power(x, y):
    try:
        return math.pow(x, y)
    except OverflowError:
        return math.inf


def prepare(c):
    r = random.Random(c["seed"])
    n = SIZE[c["op"]] + 0x40
    return r.randbytes(n) if c["fill"] == "rand" else bytes([c["fill"]]) * n


def w32(e, a, value):
    e.write(a, struct.pack(">I", value & MASK))


def ppc_case(e, c, fill):
    L = CL.PpcLog({}, c["q"])
    e.cl = L
    block = e.alloc(len(fill), fill)
    worlds = [e.alloc(0x300, b"\xa5" * 0x300) for _ in range(2)]
    centre = e.alloc(0x1D0, b"\xa5" * 0x1D0)
    L.labels.update({worlds[0]: 1, block: 2, centre: 3, worlds[1]: 4})
    allocations = []
    pow_index = 0

    def byte_hash(a, n):
        return hash_values(e.read(a, n))

    def analyser_state(tail):
        values = [e.u32(block + o) for o in range(0xC, 0x20, 4)]
        values += [e.u32(block + o) for o in range(0x21BFC, 0x21C0C, 4)]
        values += [e.u32(block + o) for o in range(0x21C0C, 0x21C3C, 4)]
        if tail:
            values += [e.u32(block + o) for o in range(0x21C3C, 0x21C48, 4)]
        return hash_values(values)

    def base(emu):
        p = emu.r(3)
        L.rec("CThing", [L.lab(p), emu.r(4) >> 16, T.s32(emu.r(5)), byte_hash(p, 0x40)])
        emu.write(p, b"\x5a" * 0xC)

    def subscribe(emu):
        L.rec("Subscribe", [L.lab(emu.r(3)), L.lab(emu.r(4)), T.s32(emu.r(5))])
        if L.ret("centreclear", 0):
            w32(e, G_CENTRE, 0)

    def funny(emu):
        world = emu.u32(G_WORLD)
        args = [int(emu.r(i) != 0) for i in range(5, 11)]
        args += [int(emu.u32(emu.r(1) + o) != 0) for o in (0x38, 0x3C)]
        L.rec("GetFunnyTime", [L.lab(world), (emu.r(4) - world) & MASK] + args + [analyser_state(False)])
        emu.write(emu.r(3), (L.ret("funny", 0) & 0xFFFFFFFFFFFFFFFF).to_bytes(8, "big"))
        if L.ret("worldswap", 0):
            w32(e, G_WORLD, worlds[1] if world == worlds[0] else worlds[0])

    def challenge_init(emu):
        p = emu.r(3)
        tick = s64(int.from_bytes(emu.read(p + 0x3D0, 8), "big"))
        flags = list(emu.read(p + 0x3D8, 3))
        L.rec("CChallengeHQ::Init", [L.lab(p), emu.u32(p + 0x3CC), tick] + flags +
              [emu.u32(p + 0x3DC), int.from_bytes(emu.read(p + 0x3E0, 2), "big")])

    def trace(emu):
        L.rec("CTRACE", [emu.r(3), emu.r(5), byte_hash(block + 0x118, 4)], [emu.cstr(emu.r(4))])

    def pow_stub(emu):
        nonlocal pow_index
        x, y = emu.f(1), emu.f(2)
        offsets = [0xC, 0x10, 0x114, 0x118, 0x11C, 0x120, 0x124]
        offsets += list(range(0x14, 0x14 + 0x20 * pow_index, 4))
        offsets += [0x14 + 0x20 * pow_index + o for o in (0, 4, 8, 0xC, 0x14, 0x18, 0x1C)]
        L.rec("pow", [bits(x), bits(y), hash_values(emu.u32(block + o) for o in offsets)])
        pow_index += 1
        answer = struct.unpack(">d", struct.pack(">q", s64(L.ret("powbits", 0))))[0] if L.ret("powoverride", 0) else power(x, y)
        emu.set_f(1, answer)

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
            return
        for i in range(count):
            call(ctor, p + i * size, 1)

    S = CL.Stubs()
    S.real = OWN
    S.internal(0x1012DE58, base)
    S.internal(0x10152388, subscribe)
    S.internal(0x100E4A44, lambda emu: L.rec("CParkAnalyser::Init", [L.lab(emu.r(3)), analyser_state(True)]))
    S.internal(0x100F6904, challenge_init)
    S.internal(0x101127D0, funny)
    S.internal(0x1006FCBC, trace)
    S.imp("pow", pow_stub)
    S.imp("__nw__FUl", op_new)
    saved = S.activate(e)
    try:
        T.stub_cb(e, "__construct_array", construct)
        w32(e, G_WORLD, worlds[c["world"]])
        w32(e, G_CENTRE, centre if c["centre"] else 0)
        for i, value in enumerate(c["balance"]):
            w32(e, BALANCE + 0x198 + 4 * i, value)
        T.call_ppc(e, ADDR[c["op"]], [block, c["fee"]], max_insns=200_000_000, timeout_us=60_000_000)
        out = block_report(e.read(block, len(fill)), fill, c["op"], False)
        out["globals"] = [L.lab(e.u32(G_WORLD)), L.lab(e.u32(G_CENTRE)), len(allocations)]
    finally:
        CL.Stubs.deactivate(e, saved)
    out["log"] = L.log
    return out


def host_case(lib, c, fill):
    lib.tpw_tc2_load.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_tc2_load(fill, len(fill))
    lib.tpw_tc2_block.restype = C.c_void_p
    lib.tpw_tc2_world.restype = C.c_void_p
    lib.tpw_tc2_centre.restype = C.c_void_p
    block = lib.tpw_tc2_block()
    worlds = [lib.tpw_tc2_world(i) for i in range(2)]
    centre = lib.tpw_tc2_centre()
    balance = (C.c_uint32 * 42)(*c["balance"])
    lib.tpw_tc2_prepare.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_uint32)]
    lib.tpw_tc2_prepare(c["world"], c["centre"], balance)
    heap = (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")
    for i in range(4):
        heap[i] = 0
    CL.host_begin(lib, {1: worlds[0], 2: block, 3: centre, 4: worlds[1]}, c["q"])
    try:
        lib.tpw_tc2_call.argtypes = [C.c_int32, C.c_uint32]
        lib.tpw_tc2_call(c["op"], c["fee"])
    finally:
        log = CL.host_end(lib)
    state = (C.c_int64 * 3)()
    lib.tpw_tc2_globals.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_tc2_globals(state)
    buf = C.create_string_buffer(len(fill))
    lib.tpw_tc2_dump.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_tc2_dump(buf, len(fill))
    out = block_report(buf.raw, fill, c["op"], True)
    out.update(globals=list(state), log=log)
    return out


class ConstructorTest(FnTest):
    def run_case(self, runner, case):
        runner.emu.reset_scratch()
        fill = prepare(case)
        p = self.ppc(runner.emu, case, fill)
        h = self.host(runner.lib, case, fill)
        difference = CL.first_difference(p.pop("log"), h.pop("log"))
        errors = T.same_values(p, h, "", False)
        if p["untouched"] or h["untouched"]:
            errors.append("a byte outside the constructor's store table changed")
        if difference:
            errors.append(difference)
        return errors


def mk(op, r, **kw):
    balance = [r.getrandbits(32), r.getrandbits(32)]
    for _ in range(10):
        balance += [r.choice(U32S) if r.random() < 0.5 else r.getrandbits(32),
                    r.choice(APRS) if r.random() < 0.5 else r.getrandbits(32),
                    r.choice(MONTHS) if r.random() < 0.5 else r.getrandbits(32), r.getrandbits(32)]
    ticks = [s64(r.choice(TICKS) if r.random() < 0.5 else r.getrandbits(64)) for _ in range(6)]
    c = {"op": op, "seed": r.getrandbits(32), "fill": r.choice(FILLS), "world": r.randrange(2),
         "centre": r.randrange(2), "balance": balance, "fee": r.getrandbits(32),
         "q": {"funny": ticks, "worldswap": [r.randrange(2) for _ in range(6)],
               "newnull": [r.randrange(2) for _ in range(3)], "centreclear": [r.randrange(2) for _ in range(3)]}}
    c.update(kw)
    return c


def edges(op):
    def cases():
        r = random.Random(12200 + op)
        result = [mk(op, r, fill=fill) for fill in FILLS]
        if op in (1, 2):
            for fill in FILLS:
                for centre in (0, 1):
                    for nulls in ([0], [1, 0], [1, 1, 0], [1, 1, 1]):
                        for clears in ([0], [1]):
                            result.append(mk(op, r, fill=fill, centre=centre,
                                             q={"newnull": nulls, "centreclear": clears}))
        if op == 0:
            for tick in TICKS:
                for world in (0, 1):
                    result.append(mk(op, r, q={"funny": [s64(tick)], "worldswap": [1]}, world=world))
        if op == 1:
            # Cross every unsigned amount/APR/period boundary; all eight loan slots read the same pinned entry.
            for amount in U32S:
                for apr in APRS:
                    for months in MONTHS:
                        result.append(mk(op, r, balance=[amount, apr] + [amount, apr, months, MASK] * 10))
            for value in POWERS:
                for amount, months in ((0, 0), (1, 1), (2, 1), (MASK, 12)):
                    result.append(mk(op, r, balance=[MASK, MASK] + [amount, 5, months, 1] * 10,
                                     q={"powoverride": [1], "powbits": [bits(value)], "newnull": [0]}))
        if op == 15:
            result += [mk(op, r, fee=fee) for fee in U32S]
        return result
    return cases


TESTS = [ConstructorTest(name, ppc_case, host_case, edge=edges(op), rnd=lambda r, op=op: mk(op, r))
         for op, name in enumerate(NAMES)]
