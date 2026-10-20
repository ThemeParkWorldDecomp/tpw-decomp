"""TbRect::Intersection (bullfrog 0x10029f8c, src/bfl_tbrect.cpp; unit w45a), against libtpw_unit_TbRectIntersection.dylib
(tests/unit_TbRectIntersection.cpp). The ltms font render method calls it (through the library's export) to cut a glyph's
rectangle to the clip rectangle.

The function takes two rectangles (this and the argument, in that order) and answers a new one by value (a hidden result
pointer on the PowerPC side): left and top the larger of the two, right and bottom the smaller, then a right below the left
and a bottom below the top are raised to them, so the answer is never inverted. Both orders of the pair are tested (the
function is symmetric, which a swapped-argument mistake would not show on its own) over rectangles of every shape: empty,
inverted, disjoint on either axis, touching, nested, single points and the ends of the 32-bit range.
"""
import ctypes as C
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_TbRectIntersection"
INTERSECTION = 0x10029F8C
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF

# Intervals along one axis (low, high).
INTERVALS = [(0, 4), (2, 6), (4, 8), (5, 7), (0, 0), (3, 3), (6, 2), (-2, 1), (INT_MIN, INT_MAX), (INT_MAX, INT_MIN), (-1, 0),
             (0, 1), (INT_MIN, INT_MIN), (INT_MAX, INT_MAX), (1, 0), (9, 3), (-5, -3), (7, 7)]


def ppc(e, case):
    a = e.alloc(16, struct.pack(">4i", *case[0]))
    b = e.alloc(16, struct.pack(">4i", *case[1]))
    out = e.alloc(16, b"\xa5" * 16)
    T.call_ppc(e, INTERSECTION, [out, a, b])
    return {"result": list(struct.unpack(">4i", e.read(out, 16))), "this": list(struct.unpack(">4i", e.read(a, 16))),
            "other": list(struct.unpack(">4i", e.read(b, 16)))}


def host(lib, case):
    a, b, out = (C.c_int32 * 4)(*case[0]), (C.c_int32 * 4)(*case[1]), (C.c_int32 * 4)(*([-0x5a5a5a5b] * 4))
    lib.tpw_tr_intersection(a, b, out)
    return {"result": list(out), "this": list(a), "other": list(b)}


def edges():
    fixed = [(INTERVALS[0], INTERVALS[1]), (INTERVALS[0], INTERVALS[0]), (INTERVALS[6], INTERVALS[3]), (INTERVALS[9], INTERVALS[8]),
             (INTERVALS[1], INTERVALS[2]), (INTERVALS[15], INTERVALS[16])]
    for x in INTERVALS:
        for x2 in INTERVALS:
            for y, y2 in fixed:
                yield [[x[0], y[0], x[1], y[1]], [x2[0], y2[0], x2[1], y2[1]]]
    # the same on the other axis, so that left and top, right and bottom are told apart
    for y in INTERVALS:
        for y2 in INTERVALS:
            for x, x2 in fixed[:4]:
                yield [[x[0], y[0], x[1], y[1]], [x2[0], y2[0], x2[1], y2[1]]]


def coord(r, span):
    k = r.random()
    if k < 0.6:
        return r.randint(-span, span)
    if k < 0.8:
        return r.randint(-1000, 1000)
    return r.choice([INT_MIN, INT_MAX, -1, 0, 1, INT_MIN + 1, INT_MAX - 1])


def rnd(r):
    return [[coord(r, 6) for _ in range(4)], [coord(r, 6) for _ in range(4)]]


TESTS = [FnTest("TbRect::Intersection", ppc, host, edge=edges, rnd=rnd)]
