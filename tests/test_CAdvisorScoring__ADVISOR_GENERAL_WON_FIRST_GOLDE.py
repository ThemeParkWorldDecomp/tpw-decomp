"""Unit v51073, 30 scorers that return 0 or one balance word, against libtpw_unit_CAdvisorScoring__ADVISOR_GENERAL_WON_FIRST_GOLDE.dylib.

A case fills a 0x700-byte object with random words (PowerPC: Mac offsets; host: the balance slots from 0x14 on) and calls one scorer.
The answer is compared. No callees, no fused operations."""
import ctypes as C
import random

import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_GENERAL_WON_FIRST_GOLDE"
MASK = 0xFFFFFFFF

FUNCS = [
    ("_ADVISOR_GENERAL_WON_FIRST_GOLDEN_TICKET", 0x1001adf8, 0x5b4), ("_ADVISOR_WELCOME_NAMEENTRY", 0x1001adac, None),
    ("_ADVISOR_GENERAL_BLOWUP_ADVISOR_1", 0x1001ae50, None), ("_ADVISOR_WEATHER_FUNNY_STORM", 0x1001ad60, None),
    ("_ADVISOR_GENERAL_BLOWUP_ADVISOR_2", 0x1001aea4, None), ("_ADVISOR_WEATHER_FUNNY_SNOW", 0x1001ad14, None),
    ("_ADVISOR_WEATHER_FUNNY_RAIN", 0x1001acc8, None), ("_ADVISOR_RIDE_EXIT_BLOCKED", 0x1001ac7c, None),
    ("_ADVISOR_GOLD_TICKET_CLUES_FOR_REMAINDER", 0x1001affc, 0x5c0), ("_ADVISOR_OBJECTIVE_MET_OFFERDOUBLE", 0x1001b444, 0x62c),
    ("_ADVISOR_OBJECTIVE_MET_OFFERDOUBLE_2", 0x1001b498, 0x62c), ("_ADVISOR_CONGRAT_GOLDEN_KEY_ANYPARK", 0x1001b4ec, None),
    ("_ADVISOR_RIDE_CONDEMNED", 0x1001a6e0, 0x578), ("_ADVISOR_CASH_LOW", 0x1001b774, None),
    ("_ADVISOR_CASH_LOW_TAKE_LOAN", 0x1001b7b8, None), ("_ADVISOR_PEEPS_STUCK_BREAKDOWN", 0x1001b804, None),
    ("_ADVISOR_PEEPS_STUCK_CONDEMNED", 0x1001b854, None), ("_ADVISOR_PREBUILT_PARK", 0x1001b8a4, 0x5f8),
    ("_ADVISOR_CHALLENGE", 0x1001bf28, 0x62c), ("_ADVISOR_WELCOME_LOST_WORLD", 0x10019258, None),
    ("_ADVISOR_WELCOME_SPACE", 0x10019210, None), ("_ADVISOR_WELCOME_HALLOWEEN", 0x100191c4, None),
    ("_ADVISOR_WELCOME_FANTASY", 0x1001917c, None), ("_ADVISOR_WELCOME_VETERAN", 0x10019134, None),
    ("_ADVISOR_WELCOME_ENTER_NAME", 0x100190e8, None), ("_ADVISOR_WELCOME_NO_NAME", 0x100190a0, None),
    ("_ADVISOR_WELCOME_NEWBIE", 0x10019058, None), ("_ADVISOR_BUY_MORE_LAND", 0x10019010, None),
    ("_ADVISOR_BANKRUPTED", 0x10016734, 0x350), ("_ADVISOR_TUTORIAL_MESSAGE", 0x10016114, 0x328),
]
SIZE = 0x700
NW = (SIZE - 0x14) // 4


def make(idx, addr):
    def ppc(e, case):
        obj = e.alloc(SIZE)
        e.write(obj + 0x14, b"".join((w & MASK).to_bytes(4, "big") for w in case["w"]))
        r3, _, _ = T.call_ppc(e, addr, [obj])
        return {"answer": r3 & MASK}

    def host(lib, case):
        f = lib.tpw_v51073_call
        f.restype, f.argtypes = C.c_uint32, [C.c_int32, C.POINTER(C.c_uint32), C.c_int32]
        arr = (C.c_uint32 * NW)(*[w & MASK for w in case["w"]])
        return {"answer": f(idx, arr, NW) & MASK}

    def gen(r):
        return {"w": [r.choice([0, MASK, 0x80000000, r.getrandbits(32)]) for _ in range(NW)]}

    def edges():
        r = random.Random(idx)
        for _ in range(4):
            yield gen(r)
        yield {"w": [0] * NW}
        yield {"w": [MASK] * NW}

    return ppc, host, edges, gen


def setup(runner):
    T.install_std_stubs(runner.emu)


TESTS = []
for i, (name, addr, off) in enumerate(FUNCS):
    p, h, ed, g = make(i, addr)
    TESTS.append(FnTest(name, p, h, edge=ed, rnd=g))
