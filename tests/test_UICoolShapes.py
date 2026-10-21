"""Differential tests for the region shapes' virtual members, against libtpw_unit_UICoolShapes.dylib
(tests/unit_UICoolShapes.cpp):

  src/game/ui/CoolRegion.cpp   CoolCircle, CoolEllipse and CoolPoly: PointIn, Shift, Adjust and the destructor (12 functions)

Real on both sides: those and under them the constructors (src/game/ui/CoolRegion.cpp, src/game/ui/InterfaceKnob.cpp, src/game/ui/InterfaceListBox.cpp, src/game/ui/InterfaceListControl2.cpp). Only the allocator is a fake (it counts).

A case is a script of operations on three shapes in slots: make one (the constructor), PointIn, Shift, Adjust, destroy it with the
Mac's flag (positive also deletes the object), and poke a polygon's count or array. After each operation the answer, the live
shapes' fields and the heap traffic are compared. Radii of 0 are not tested for the ellipse (a division by zero, undefined in the
original); a polygon's count is only poked to 0 or with no array (it would read past the points otherwise).
"""
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UICoolShapes"

MINUS = 0xFFFFFFFF
SLOTS = 3

CTOR = {1: 0x101D5BB0, 2: 0x101D5CE4, 3: 0x101D5E2C}
POINT_IN = {1: 0x101D5BFC, 2: 0x101D5D34, 3: 0x101D5FAC}
SHIFT = {1: 0x101D5C6C, 2: 0x101D5DB0, 3: 0x101D60CC}
ADJUST = {1: 0x101D5CB4, 2: 0x101D5DF8, 3: 0x101D6150}
DTOR = {1: 0x101D6200, 2: 0x101D6180, 3: 0x101D5F04}
OBJECT_SIZE = 0x40

SMALL = [0, 0, 1, -1, 2, -3, 7, -8, 20, -33, 100, -250, 0x7FFF, -0x8000, 0x8000, 0xFFFF, 0x12345, -0x12345]


def s16(v):
    return T.s16(v & 0xFFFF)


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MINUS))


class World:
    """One case on the PowerPC side."""

    def __init__(self, e):
        self.e = e
        self.heap = [0, 0, 0, 0]
        e.us = self
        e.cl = CL.PpcLog({})
        self.objects = [0] * SLOTS
        self.kinds = [0] * SLOTS

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def dump(self, slot):
        e, o, kind = self.e, self.objects[slot], self.kinds[slot]
        if kind == 1:
            return list(struct.unpack(">3h", e.read(o + 4, 6)))
        if kind == 2:
            return list(struct.unpack(">4h", e.read(o + 4, 8)))
        count = struct.unpack(">h", e.read(o + 4, 2))[0]
        points = e.u32(o + 8)
        out = [count, int(points != 0)]
        if points:
            out += list(struct.unpack(">%dh" % (2 * count), e.read(points, 4 * count))) if count > 0 else []
        return out

    def op(self, code, a):
        e = self.e
        if code == 1:   # make: slot, kind, values, points
            slot, kind, values, points = a
            if self.objects[slot]:
                return None
            data = e.alloc(4 * max(len(points), 1) + 8, struct.pack(">%dh" % len(points), *[s16(v) for v in points]))
            block = e.alloc(OBJECT_SIZE, b"\xa5" * OBJECT_SIZE)
            if kind == 1:
                self.call(CTOR[1], block, *[s16(v) for v in values[:3]])
            elif kind == 2:
                self.call(CTOR[2], block, *[s16(v) for v in values[:4]])
            else:
                self.call(CTOR[3], block, s16(values[0]), data)
            self.objects[slot], self.kinds[slot] = block, kind
            self.heap = [0, 0, 0, 0]
            return [0]
        slot = a[0]
        if not self.objects[slot]:
            return None
        o, kind = self.objects[slot], self.kinds[slot]
        if code == 2:
            return [T.s32(self.call(POINT_IN[kind], o, s16(a[1]), s16(a[2])))]
        if code == 3:
            self.call(SHIFT[kind], o, s16(a[1]), s16(a[2]))
            return [0]
        if code == 4:
            self.call(ADJUST[kind], o, *[s16(v) for v in a[1:5]])
            return [0]
        if code == 5:
            self.call(DTOR[kind], o, a[1])
            self.objects[slot] = 0
            return [0]
        if code == 6 and kind == 3:   # poke: slot, count, no array
            e.write(o + 4, struct.pack(">h", s16(a[1])))
            if a[2]:
                w32(e, o + 8, 0)
            return [0]
        return None


def make_stubs(e):
    S = CL.Stubs()
    S.real.update(list(CTOR.values()) + list(POINT_IN.values()) + list(SHIFT.values()) + list(ADJUST.values()) + list(DTOR.values()))
    us = lambda e: e.us   # noqa: E731

    def counted(index, allocate):
        def stub(e):
            if allocate:
                us(e).heap[index] += 1
                e.set_r(3, e.alloc(max(e.r(3), 4)))
            elif e.r(3):
                us(e).heap[index] += 1
        return stub

    S.imp("__nw__FUl", counted(0, True))
    S.imp("__nwa__FUl", counted(1, True))
    S.imp("__dl__FPv", counted(2, False))
    S.imp("__dla__FPv", counted(3, False))
    return S


RAN = {}
STUBS = None


def shapes_ppc(e, case):
    saved = STUBS.activate(e)
    try:
        world = World(e)
        steps, ran = [], []
        for code, a in case["ops"]:
            answer = world.op(code, a)
            if answer is None:
                continue
            ran.append((code, a))
            live = [world.dump(s) if world.objects[s] else [] for s in range(SLOTS)]
            steps.append(dict(answer=answer, live=live, heap=list(world.heap)))
        RAN[id(case)] = ran
        return dict(steps=steps)
    finally:
        CL.Stubs.deactivate(e, saved)


def shapes_host(lib, case):
    import ctypes as C
    lib.tpw_ucs_make.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int16)]
    lib.tpw_ucs_pointin.argtypes = [C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_ucs_pointin.restype = C.c_int64
    lib.tpw_ucs_shift.argtypes = [C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_ucs_adjust.argtypes = [C.c_int32] * 5
    lib.tpw_ucs_destroy.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_ucs_poke.argtypes = [C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_ucs_dump.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_ucs_dump.restype = C.c_int32
    lib.tpw_ucs_heap.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ucs_reset()
    alive = [False] * SLOTS
    steps = []
    for code, a in RAN.pop(id(case)):
        answer = [0]
        if code == 1:
            slot, kind, values, points = a
            args = (C.c_int64 * 4)(*[s16(v) for v in values], *([0] * (4 - len(values))))
            array = (C.c_int16 * max(len(points), 1))(*[s16(v) for v in points])
            lib.tpw_ucs_make(kind, slot, args, array)
            alive[slot] = True
        elif code == 2:
            answer = [lib.tpw_ucs_pointin(a[0], s16(a[1]), s16(a[2]))]
        elif code == 3:
            lib.tpw_ucs_shift(a[0], s16(a[1]), s16(a[2]))
        elif code == 4:
            lib.tpw_ucs_adjust(a[0], *[s16(v) for v in a[1:5]])
        elif code == 5:
            lib.tpw_ucs_destroy(a[0], a[1])
            alive[a[0]] = False
        elif code == 6:
            lib.tpw_ucs_poke(a[0], s16(a[1]), a[2])
        out = (C.c_int64 * 4096)()
        live = []
        for s in range(SLOTS):
            n = lib.tpw_ucs_dump(s, out) if alive[s] else 0
            live.append(list(out[:n]))
        heap = (C.c_int64 * 4)()
        lib.tpw_ucs_heap(heap)
        steps.append(dict(answer=answer, live=live, heap=list(heap)))
    return dict(steps=steps)


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    STUBS = make_stubs(runner.emu)


# ======================================================================= the cases

TESTS = []


def make(slot, kind, values=(), points=()):
    return (1, (slot, kind, tuple(values), tuple(points)))


def circle_edges():
    for centre in ((0, 0), (10, 20), (-10, -20), (0x7FFF, -0x8000), (-0x8000, 0x7FFF)):
        for radius in (0, 1, 5, -5, 100, 0x7FFF, -0x8000):
            ops = [make(0, 1, (centre[0], centre[1], radius))]
            for x in (centre[0], centre[0] + radius, centre[0] - radius, centre[0] + 1, centre[0] - 1, 0, 0x7FFF, -0x8000):
                for y in (centre[1], centre[1] + radius, centre[1] - radius, centre[1] + 1, 0, 0x7FFF, -0x8000):
                    ops.append((2, (0, x, y)))
            yield dict(ops=ops)
    base = [make(0, 1, (10, 20, 5)), make(1, 1, (-0x8000, 0x7FFF, 3))]
    for dx, dy in ((0, 0), (1, 2), (-1, -2), (0x7FFF, 0x7FFF), (-0x8000, -0x8000), (0xFFFF, 0x10001)):
        yield dict(ops=base + [(3, (0, dx, dy)), (3, (1, dx, dy)), (2, (0, 12, 22)), (4, (0, 1, 2, 3, 4)), (4, (1, -1, -2, -3, -4))])
    for flag in (1, 2, 0, -1, 0x7FFF, 0x8000, 0xFFFF):
        yield dict(ops=base + [(5, (0, flag)), (2, (1, 0, 0)), (5, (1, flag))])


def ellipse_edges():
    for centre in ((0, 0), (10, 20), (-10, -20), (0x7FFF, -0x8000)):
        for rx, ry in ((1, 1), (5, 3), (3, 5), (-5, 3), (5, -3), (-5, -3), (100, 1), (1, 100), (0x7FFF, 0x7FFF), (-0x8000, 0x7FFF), (7, -0x8000),
                       (0, 0)):
            if rx == 0:
                continue   # a division by zero: undefined in the original
            ops = [make(0, 2, (centre[0], centre[1], rx, ry))]
            for x in (centre[0], centre[0] + rx, centre[0] - rx, centre[0] + 1, centre[0] - 1, 0, 0x7FFF, -0x8000):
                for y in (centre[1], centre[1] + ry, centre[1] - ry, centre[1] + 1, 0, 0x7FFF, -0x8000):
                    ops.append((2, (0, x, y)))
            yield dict(ops=ops)
    base = [make(0, 2, (10, 20, 5, 4)), make(1, 2, (-0x8000, 0x7FFF, 3, 9))]
    for dx, dy in ((0, 0), (1, 2), (-1, -2), (0x7FFF, 0x7FFF), (-0x8000, -0x8000), (0xFFFF, 0x10001)):
        yield dict(ops=base + [(3, (0, dx, dy)), (3, (1, dx, dy)), (2, (0, 12, 22)), (4, (0, 1, 2, 3, 4)), (4, (1, -1, -2, -3, -4))])
    for flag in (1, 2, 0, -1, 0x7FFF, 0x8000, 0xFFFF):
        yield dict(ops=base + [(5, (0, flag)), (2, (1, 0, 0)), (5, (1, flag))])


SQUARE = [(0, 0), (10, 0), (10, 10), (0, 10)]
TRIANGLE = [(0, 0), (20, 5), (5, 20)]
STAR = [(10, 0), (13, 7), (20, 8), (14, 13), (16, 20), (10, 16), (4, 20), (6, 13), (0, 8), (7, 7)]
BOWTIE = [(0, 0), (10, 10), (10, 0), (0, 10)]
EXTREME = [(0x7FFF, 0x7FFF), (-0x8000, 0x7FFF), (-0x8000, -0x8000), (0x7FFF, -0x8000)]


def flat(points):
    return [v for p in points for v in p]


def polygon_edges():
    shapes = [[], [(3, 4)], [(0, 0), (10, 10)], TRIANGLE, SQUARE, STAR, BOWTIE, EXTREME, [(5, 5)] * 4, [(0, 0), (10, 0), (10, 0), (0, 0)],
              [(0, 5), (10, 5), (10, 5), (0, 5)], [(0, 0), (0, 10), (0, 20)], [(-5, -5), (5, -5), (5, 5), (-5, 5), (0, 0), (0, -9)]]
    for points in shapes:
        ops = [make(0, 3, (len(points),), flat(points))]
        for x in range(-3, 24, 2):
            for y in range(-3, 24, 2):
                ops.append((2, (0, x, y)))
        for x, y in ((0x7FFF, 0x7FFF), (-0x8000, -0x8000), (0, 0x7FFF), (0x7FFF, 0), (-0x8000, 0)):
            ops.append((2, (0, x, y)))
        yield dict(ops=ops)
    # the vertices and the edges themselves
    for points in (SQUARE, TRIANGLE, STAR):
        ops = [make(0, 3, (len(points),), flat(points))]
        for x, y in points + [((a[0] + b[0]) // 2, (a[1] + b[1]) // 2) for a, b in zip(points, points[1:] + points[:1])]:
            ops.append((2, (0, x, y)))
        yield dict(ops=ops)
    # a count with no array, a count of 0 with an array
    yield dict(ops=[make(0, 3, (3,), flat(TRIANGLE)), (6, (0, 3, 1)), (2, (0, 5, 5)), (3, (0, 1, 1)), (2, (0, 5, 5)), (5, (0, 1))])
    yield dict(ops=[make(0, 3, (3,), flat(TRIANGLE)), (6, (0, 0, 0)), (2, (0, 5, 5)), (3, (0, 1, 1)), (5, (0, 0))])
    # shifting
    for points in (TRIANGLE, SQUARE, EXTREME, []):
        for dx, dy in ((0, 0), (1, 2), (-1, -2), (0x7FFF, 0x7FFF), (-0x8000, -0x8000), (0xFFFF, 0x10001)):
            yield dict(ops=[make(0, 3, (len(points),), flat(points)), (3, (0, dx, dy)), (2, (0, 5, 5)), (4, (0, 1, 2, 3, 4)), (3, (0, dx, dy))])
    # destroying: the array goes, and the object with a positive flag
    for points in (TRIANGLE, []):
        for flag in (1, 2, 0, -1, 0x7FFF, 0x8000, 0xFFFF):
            yield dict(ops=[make(0, 3, (len(points),), flat(points)), make(1, 3, (3,), flat(TRIANGLE)), (5, (0, flag)), (2, (1, 5, 5)), (5, (1, flag))])
    yield dict(ops=[make(0, 3, (3,), flat(TRIANGLE)), (6, (0, 3, 1)), (5, (0, 1))])


def rnd_polygon(r):
    count = r.choice([0, 1, 2, 3, 4, 5, 8, 12])
    return count, flat([(r.choice(SMALL + [r.randint(-30, 30)]), r.choice(SMALL + [r.randint(-30, 30)])) for _ in range(count)])


def shapes_random(r):
    ops = []
    for slot in range(SLOTS):
        kind = r.choice([1, 2, 3])
        if kind == 1:
            ops.append(make(slot, 1, (r.choice(SMALL), r.choice(SMALL), r.choice(SMALL + [r.randint(0, 40)]))))
        elif kind == 2:
            ops.append(make(slot, 2, (r.choice(SMALL), r.choice(SMALL), r.choice([v for v in SMALL if v != 0] + [r.randint(1, 40)]),
                                      r.choice(SMALL + [r.randint(0, 40)]))))
        else:
            count, points = rnd_polygon(r)
            ops.append(make(slot, 3, (count,), points))
    for _ in range(r.randint(3, 25)):
        k = r.random()
        slot = r.randrange(SLOTS)
        if k < 0.55:
            ops.append((2, (slot, r.choice(SMALL + [r.randint(-30, 30)]), r.choice(SMALL + [r.randint(-30, 30)]))))
        elif k < 0.75:
            ops.append((3, (slot, r.choice(SMALL), r.choice(SMALL))))
        elif k < 0.85:
            ops.append((4, (slot, r.choice(SMALL), r.choice(SMALL), r.choice(SMALL), r.choice(SMALL))))
        elif k < 0.92:
            ops.append((6, (slot, r.choice([0, 3]), 1)))
        else:
            ops.append((5, (slot, r.choice([1, 0, -1, 2]))))
    return dict(ops=ops)


for name, edges in (("CoolCircle", circle_edges), ("CoolEllipse", ellipse_edges), ("CoolPoly", polygon_edges)):
    TESTS.append(FnTest(name + " PointIn, Shift, Adjust, destructor", shapes_ppc, shapes_host, edge=edges, rnd=shapes_random))
