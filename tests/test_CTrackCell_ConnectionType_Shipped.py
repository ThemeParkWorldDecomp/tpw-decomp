"""Proof for candidate 14: tests/test_CTrackCell_ConnectionType.py against a library linked as the shipped game is, see tests/unit_CTrackCell_ConnectionType_Shipped.cpp.

Two more runs use the game's real data: every record of the data files has OverwritePriority 5, and the mouse code's rank is 0, 2, 3 or 5. The pylon cell (the Mac's
0x1029e0b4 and 0x1029e0b8, which the Mac's ConnectionType never touches) is -1 until a coaster pylon has been dragged; after a drag it keeps that pylon's cell. What is compared
includes the pylon words, which the original leaves alone.
"""
import random
import struct

import test_CTrackCell_ConnectionType as B
from test_CTrackCell_ConnectionType import *  # noqa: F401,F403
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CTrackCell_ConnectionType_Shipped"
PYLON = 0x1029e0b4
RIDE_CELL = (4, 9, 10)


def real_data(r, pylon, mode=None, ttype=None):
    c = B.make(r, mode, ttype)
    w = c["w"]
    w["rides"] = [5] * 4
    w["things"] = [(3, obj) for _, obj in w["things"]]
    w["g"]["gLLCCDE0B8"] = r.choice([0, 2, 3, 5])
    w["g"]["gLLCCDE0B4"] = r.choice([0, 5])
    w["mcells"][r.randrange(3)]["type"] = r.choice(RIDE_CELL)
    c["pylon"] = pylon
    return c


def ppc2(e, case):
    x, y = case.get("pylon", (-1, -1))
    e.write(PYLON, struct.pack(">ii", x, y))
    out = B.ppc(e, case)
    out["pylon"] = struct.unpack(">ii", e.read(PYLON, 8))
    return out


def host2(lib, case):
    x, y = case.get("pylon", (-1, -1))
    B.bind(lib)
    lib.tpw_ct_set_pylon(x, y)
    out = B.host(lib, case)
    out["pylon"] = (lib.tpw_ct_get_pylon(0), lib.tpw_ct_get_pylon(1))
    return out


def no_pylon_edges():
    r = random.Random(14001)
    for mode in B.MODES:
        for ttype in B.TYPES:
            yield real_data(r, (-1, -1), mode, ttype)


def no_pylon_rnd(r):
    return real_data(r, (-1, -1))


def after_drag_edges():
    r = random.Random(14002)
    for mode in B.MODES:
        for ttype in B.TYPES:
            yield real_data(r, (r.randrange(0x80), r.randrange(6, 0x80)), mode, ttype)


def after_drag_rnd(r):
    return real_data(r, (r.randrange(0x80), r.randrange(6, 0x80)))


TESTS = [
    FnTest("CTrackCell::ConnectionType", B.ppc, B.host, edge=B.edges, rnd=B.rnd),
    FnTest("CTrackCell::ConnectionType, real data, no pylon dragged yet", ppc2, host2, edge=no_pylon_edges, rnd=no_pylon_rnd),
    FnTest("CTrackCell::ConnectionType, real data, after a pylon drag at z above 5", ppc2, host2, edge=after_drag_edges, rnd=after_drag_rnd),
]
