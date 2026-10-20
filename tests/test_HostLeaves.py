"""Four leaves of the main app that the effect manager calls (original Advisor.cpp and TPAdvisor.cpp ranges):
RealTimePauseAdjusted 0x10007c68, HOST_Sin 0x100218f4, HOST_Cos 0x10021928 and BMP_GenerateSinTab 0x100259d0
(src/host_leaves.cpp, library unit_HostLeaves).

A case is a script run on both sides over the same starting state: the table of 512 sines (main app data
0x102b4ec0) and the three words of the advisor's state the clock keeps its pauses in (0x102b354c + 0x11c, 0x120 and
0x124). The game clock's two reads (CClock::ReadFixed 0x1014733c, GetPauseFlag 0x101474b8) are fakes on both sides that
record and answer from the case's queues. After every operation the answer, the three words and the whole table are
compared, and the call logs at the end. sin is the C library's on both sides.
"""
import ctypes as C
import math
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_HostLeaves"

RTPA, HOST_SIN, HOST_COS, GEN = 0x10007C68, 0x100218F4, 0x10021928, 0x100259D0
READ_FIXED, PAUSE_FLAG = 0x1014733C, 0x101474B8
TABLE, STATE, CLOCK = 0x102B4EC0, 0x102B354C, 0x1037F048
PAUSED, START, TOTAL = STATE + 0x11C, STATE + 0x120, STATE + 0x124


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc(e, case):
    e.cl = CL.PpcLog({CLOCK: 1}, case["q"])
    st = CL.Stubs()
    st.real = {RTPA, HOST_SIN, HOST_COS, GEN}
    st.internal(READ_FIXED, CL.recorder("ReadFixed", "L", ret="i"))
    st.internal(PAUSE_FLAG, CL.recorder("GetPauseFlag", "L", ret="i"))
    saved_table = e.read(TABLE, 0x800)
    saved_words = [e.u32(a) for a in (PAUSED, START, TOTAL)]
    e.write(TABLE, struct.pack(">512i", *case["table"]))
    for a, v in zip((PAUSED, START, TOTAL), case["words"]):
        e.write(a, struct.pack(">i", v))
    old = st.activate(e)
    results, states = [], []
    try:
        for op, arg in case["script"]:
            entry = {1: RTPA, 2: HOST_SIN, 3: HOST_COS, 4: GEN}[op]
            r3 = T.call_ppc(e, entry, [arg] if op in (2, 3) else [])[0]
            results.append(r3 if op == 1 else T.s32(r3) if op in (2, 3) else 0)
            states.append([T.s32(e.u32(a)) for a in (PAUSED, START, TOTAL)] + list(struct.unpack(">512i", e.read(TABLE, 0x800))))
    finally:
        CL.Stubs.deactivate(e, old)
        e.write(TABLE, saved_table)
        for a, v in zip((PAUSED, START, TOTAL), saved_words):
            e.write(a, struct.pack(">I", v))
    return dict(results=results, states=states, log=e.cl.log)


def host(lib, case):
    lib.tpw_hl_clock.restype = C.c_void_p
    CL.host_begin(lib, {1: lib.tpw_hl_clock()}, case["q"])
    lib.tpw_hl_reset.argtypes = [C.POINTER(C.c_int32), C.POINTER(C.c_int32)]
    lib.tpw_hl_reset((C.c_int32 * 512)(*case["table"]), (C.c_int32 * 3)(*case["words"]))
    lib.tpw_hl_op.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_hl_op.restype = C.c_int64
    lib.tpw_hl_state.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_hl_state.restype = C.c_int32
    out = (C.c_int64 * 515)()
    results, states = [], []
    try:
        for op, arg in case["script"]:
            r = lib.tpw_hl_op(op, T.s32(arg))
            results.append(r & 0xFFFFFFFF if op == 1 else r if op in (2, 3) else 0)
            lib.tpw_hl_state(out)
            states.append(list(out))
    finally:
        log = CL.host_end(lib)
    return dict(results=results, states=states, log=log)


# ----------------------------------------------------------------- cases

# A table the leaves can be told from the generated one: a value per entry that shows its position.
MARKED = [(i * 7919 + 13) - 1000 for i in range(512)]
GENERATED = [int(256 * math.sin(3.14159 * 2 * i * 0.001953125)) for i in range(512)]

ANGLES = [0, 1, 2, 127, 128, 129, 255, 256, 257, 383, 384, 385, 510, 511, 512, 513, 1023, 1024, -1, -2, -127, -128, -129,
          -255, -256, -511, -512, -513, 0x7FFFFF7F, 0x7FFFFF80, 0x7FFFFF81, 0x7FFFFFFF, -0x80000000, -0x7FFFFFFF, 0x12345678,
          -0x12345678, 0xFFFF, 0x10000, -0x10000]
TIMES = [0, 1, 2, 100, 5000, 0x7FFFFFFF, -0x80000000, -1, -2, 0x12345678, -0x12345678, 0x40000000, 0x7FFFFF00, -0x7FFFFF00]
FLAGS = [0, 1, 2, -1, 0x80000000, 0x100]
WORDS = [0, 1, 2, -1, 0x7FFFFFFF, -0x80000000, 77, 100000, 0x12345678]


def case(script, table=MARKED, words=(0, 0, 0), reads=(), flags=()):
    return dict(script=script, table=list(table), words=list(words), q=dict(ReadFixed=list(reads), GetPauseFlag=list(flags)))


def edges():
    # The table lookups at every edge angle, on a marked table and on the generated one.
    for table in (MARKED, GENERATED):
        yield case([(2, a) for a in ANGLES] + [(3, a) for a in ANGLES], table=table)
    # Both lookups at every index of the marked table.
    yield case([(2, a) for a in range(-600, 1200, 7)] + [(3, a) for a in range(-600, 1200, 7)])
    yield case([(op, a) for a in range(0, 512) for op in (2, 3)])
    # The table is generated: from a marked one, and on top of itself.
    yield case([(4, 0), (2, 0), (3, 0), (2, 128), (3, 128), (2, 256), (3, 384), (4, 0)])
    yield case([(4, 0)] + [(op, a) for a in range(0, 512, 5) for op in (2, 3)])
    # The clock: unpaused with no pause in progress, with one that has ended, paused with none and with one in progress,
    # every combination of the time and the three words at edge values.
    for flag in (0, 1, 2, -1):
        for paused in (0, 1, 2, -1):
            for now in TIMES:
                for start, total in ((0, 0), (1000, 10), (-5, 7), (0x7FFFFFFF, 0x7FFFFFFF), (-0x80000000, 1), (123, -456)):
                    yield case([(1, 0), (1, 0)], words=(paused, start, total), reads=[now, now + 50], flags=[flag, flag])
    # A pause begins, goes on, ends and begins again.
    yield case([(1, 0)] * 8, reads=[100, 200, 250, 400, 450, 500, 600, 650], flags=[0, 1, 1, 0, 0, 1, 1, 0])
    yield case([(1, 0)] * 6, words=(1, 90, 40), reads=[100, 200, 300, 400, 500, 600], flags=[1, 1, 0, 0, 1, 0])
    # The clock wrapping (the game counts the millisecond clock as an unsigned difference, the leaf as signed words).
    yield case([(1, 0)] * 5, words=(0, 0, 5), reads=[0x7FFFFFF0, 0x7FFFFFFF, 0x80000001, 0xFFFFFFFE & 0x7FFFFFFF, 1],
               flags=[0, 1, 1, 0, 0])
    yield case([(1, 0)] * 4, words=(1, 0x7FFFFFF0, 3), reads=[-0x7FFFFFF0, 5, 7, 9], flags=[0, 0, 1, 1])
    # The queue running dry repeats its last value; an empty one answers 0.
    yield case([(1, 0)] * 3, reads=[], flags=[])
    yield case([(1, 0)] * 3, reads=[77], flags=[1])


def random_case(r):
    script = []
    for _ in range(r.randrange(1, 30)):
        op = r.choice([1, 1, 1, 2, 2, 3, 3, 4])
        if op in (2, 3):
            script.append((op, r.choice(ANGLES + [r.randint(-0x80000000, 0x7FFFFFFF), r.randint(-2000, 2000), r.randint(-0x80000000, 0x7FFFFFFF)])))
        else:
            script.append((op, 0))
    reads = [r.choice(TIMES + [r.randint(-0x80000000, 0x7FFFFFFF)]) for _ in range(r.randrange(0, 12))]
    flags = [r.choice(FLAGS) for _ in range(r.randrange(0, 12))]
    table = r.choice([MARKED, GENERATED, [r.randint(-0x80000000, 0x7FFFFFFF) for _ in range(512)]])
    words = tuple(r.choice(WORDS + [r.randint(-0x80000000, 0x7FFFFFFF)]) for _ in range(3))
    return case(script, table=table, words=words, reads=reads, flags=flags)


TESTS = [FnTest("effect manager leaves: RealTimePauseAdjusted, HOST_Sin, HOST_Cos, BMP_GenerateSinTab", ppc, host, edge=edges,
                rnd=random_case)]
