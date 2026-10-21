"""The walk planner (unit w196), against libtpw_unit_WallhugPlanner.dylib (tests/unit_WallhugPlanner.cpp), main app ("SimTheme Park"),
src/game/park/LineOfSight.cpp:
  wallhug_continue_trivial 0x10136f04, wallhug_trivial 0x10137d04

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for
the original, and in the host library's memory (little-endian) for the translation. Real on both sides: the planner's two functions and w195's
bresenham_start, line_of_sight and line_of_sight_cleanup (src/game/park/LineOfSight.cpp on the host, the original code on the PowerPC side). Fakes on both
sides that record every argument: MAP_wall_in_way 0x10101ca4 (answers from the case's WALL TABLE: a fill mask for every square plus single
squares' masks; bit d of a mask is a wall in direction d; bit 8 of the fill word silences the wall calls in the log for the long walks, the log
holds 4096 events) and SamsUtilities::UDebug 0x10000e40. labs and abs are Python's.

The world of a case: the six planner globals and w195's seven at random values (and the step table, compared), the wall table, a 0x208-byte path of
random bytes with the case's count and start and end squares, and the call: wallhug_trivial(path, mode) or wallhug_continue_trivial(path, start
word, limit) with the step counter preset. What is compared: the answer, the thirteen globals, the step table, all 0x208 bytes of the path and
the event log. The walls come from shapes (bars, L and U shapes, rooms with a door, closed rooms around the end, mazes, wrap at the map's byte
edge) with consistent walls on both sides of an edge, and from random single bits.

No fused operations. A waypoint is passed as a word whose low half is random (the original reads only the top half). Not tested: a direction
outside 0 to 3 (UDebug; bresenham_start never produces one). The count on entry goes up to 0xff for wallhug_continue_trivial (the buffer
holds 257 points, so the appends stay inside the 0x208 bytes and the count wraps at 256).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_WallhugPlanner"
MASK = 0xFFFFFFFF

# ---- the unit's own functions and w195's, all run as the original
CONT, TRIV = 0x10136F04, 0x10137D04
BRES, LOS, LOSC = 0x101360AC, 0x1013623C, 0x10136498
SIZES = {CONT: 3500, TRIV: 96}
OWN = {CONT, TRIV, BRES, LOS, LOSC}

# ---- the callees faked on both sides
WALL, UDEBUG = 0x10101CA4, 0x10000E40

# ---- the Mac's addresses of the globals and their sizes; the host library's come from tpw_whp_addr(name)
ADDR = dict(xmajor=0x1034CAB8, xdir=0x1034CABC, ydir=0x1034CAC0, err=0x1034CAC4, dy=0x1034CAC8, dx=0x1034CACC, mode=0x1034CAD0,
            step=0x102A45A4, rec=0x1034CAA8, hitstep=0x1034CAAC, hitdir=0x1034CAB0, hitside=0x1034CAB1, hitcount=0x1034CAB2,
            steps=0x1034CAB4)
SIZE = dict(xmajor=4, xdir=4, ydir=4, err=4, dy=4, dx=4, mode=4, rec=4, hitstep=4, hitdir=1, hitside=1, hitcount=1, steps=4)
GLOBALS = ["xmajor", "xdir", "ydir", "err", "dy", "dx", "mode", "rec", "hitstep", "hitdir", "hitside", "hitcount", "steps"]
# ---- the host's sizes the module relies on, checked once against tpw_whp_layout (catches a header that moved)
HOST_LAYOUT = [0x208, 6]
PATH_SIZE = 0x208
MODES = [0, 1, 2, 7, -1, 0x1234]
NOWALK = 0x70000000
DX = (0, 1, 0, -1)
DY = (-1, 0, 1, 0)


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_wall(e):
    """MAP_wall_in_way(x, z, dir, mode): records the four arguments, answers from the case's wall table."""
    x, z, d, mode = e.r(3) & 0xFF, e.r(4) & 0xFF, e.r(5) & 0xFF, s32(e.r(6))
    if not e.quiet:
        e.cl.rec("MAP_wall_in_way", (x, z, d, mode))
    mask = e.walls.get((x, z), e.fill)
    e.set_r(3, (mask >> d) & 1 if d < 8 else 0)


STUBS.internal(WALL, stub_wall)
STUBS.internal(UDEBUG, lambda e: e.cl.rec("UDebug", (), (e.cstr(e.r(3))[:159],)))
STUBS.imp("labs", lambda e: e.set_r(3, T.u32(abs(s32(e.r(3))))))
STUBS.imp("abs", lambda e: e.set_r(3, T.u32(abs(s32(e.r(3))))))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def put(self, name, v):
        self.write(self.g(name), (v & ((1 << (8 * SIZE[name])) - 1)).to_bytes(SIZE[name], self.order))

    def get(self, name):
        return int.from_bytes(self.read(self.g(name), SIZE[name]), self.order)


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    order = "big"

    def __init__(self, e, case):
        self.e = e
        e.walls = {(x, y): m for x, y, m in case["walls"][1]}
        e.fill = case["walls"][0] & 255
        e.quiet = bool(case["walls"][0] & 256)
        self.L = e.cl = CL.PpcLog({}, {})
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        return T.call_ppc(self.e, entry, [a & MASK for a in args])[0] & MASK


class Host(Side):
    """The same on the host library."""
    order = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, {})
        fill, entries = case["walls"]
        packed = (C.c_uint32 * (len(entries) + 1))(*[(x << 16) | (y << 8) | m for x, y, m in entries])
        lib.tpw_whp_setup(fill, packed, len(entries))

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_whp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_whp_alloc(n)       # zeroed, 32-bit addressable (platform/arena.cpp)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        lib = self.lib
        if entry == CONT:
            return lib.tpw_whp_continue(args[0], args[1] & MASK, args[2] & MASK) & MASK
        return lib.tpw_whp_trivial(args[0], s32(args[1])) & MASK


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_whp_addr.restype, lib.tpw_whp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_whp_alloc.restype, lib.tpw_whp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_whp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_whp_setup.argtypes = [C.c_uint32, C.POINTER(C.c_uint32), C.c_int32]
    lib.tpw_whp_continue.restype, lib.tpw_whp_continue.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32, C.c_uint32]
    lib.tpw_whp_trivial.restype, lib.tpw_whp_trivial.argtypes = C.c_int32, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_whp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's globals and path through Side: the same code for both memories."""
    for name in GLOBALS:
        s.put(name, case["g"][name])
    s.path = s.alloc(PATH_SIZE)
    s.write(s.path, case["path"])


def execute(s, case):
    """The call."""
    if case["op"] == "cont":
        return s.call(CONT, [s.path, case["start"], case["limit"]])
    return s.call(TRIV, [s.path, case["mode"]])


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides."""
    out = {"answer": answer, "log": s.log(), "step": s.read(s.g("step"), 8), "path": s.read(s.path, PATH_SIZE)}
    for name in GLOBALS:
        out[name] = s.get(name)
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the walls


class Walls:
    """A wall table: edges are blocked on both sides (bit d at the square, bit d + 2 at its neighbour); single bits one-sided."""

    def __init__(self):
        self.m = {}

    def bit(self, x, y, d):
        k = (x & 255, y & 255)
        self.m[k] = self.m.get(k, 0) | (1 << d)

    def edge(self, x, y, d):
        self.bit(x, y, d)
        self.bit(x + DX[d], y + DY[d], (d + 2) & 3)

    def hbar(self, x0, x1, y):
        """A wall under row y from column x0 to x1."""
        for x in range(x0, x1 + 1):
            self.edge(x, y, 2)

    def vbar(self, x, y0, y1):
        """A wall right of column x from row y0 to y1."""
        for y in range(y0, y1 + 1):
            self.edge(x, y, 1)

    def room(self, x0, y0, x1, y1, door=None):
        """The border of the rectangle's squares; door = (side, offset) leaves one edge open."""
        gap = None
        if door is not None:
            side, off = door
            gap = [(x0 + off, y0, 0), (x1, y0 + off, 1), (x0 + off, y1, 2), (x0, y0 + off, 3)][side]
        for x in range(x0, x1 + 1):
            for d, y in ((0, y0), (2, y1)):
                if gap != (x, y, d):
                    self.edge(x, y, d)
        for y in range(y0, y1 + 1):
            for d, x in ((3, x0), (1, x1)):
                if gap != (x, y, d):
                    self.edge(x, y, d)

    def maze(self, r, x0, y0, x1, y1, p):
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                for d in (1, 2):
                    if r.random() < p:
                        self.edge(x, y, d)

    def sprinkle(self, r, x0, y0, x1, y1, p):
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                for d in range(4):
                    if r.random() < p:
                        self.bit(x, y, d)

    def table(self, quiet, fill=0):
        return (fill | (256 if quiet else 0), [(x, y, m) for (x, y), m in sorted(self.m.items()) if m != fill])


def shape(r, w, sx, sy, ex, ey, kind):
    """Adds an obstacle of the kind between the start and the end squares."""
    mx, my = (sx + ex) // 2, (sy + ey) // 2
    ln = r.choice([1, 2, 3, 5, 9, 16])
    if kind == "hbar":
        w.hbar(mx - ln, mx + ln, my)
    elif kind == "vbar":
        w.vbar(mx, my - ln, my + ln)
    elif kind == "bar_near_start":
        k = r.choice([0, 1, 2])
        w.hbar(sx - ln, sx + ln, sy + k) if r.random() < 0.5 else w.vbar(sx + k, sy - ln, sy + ln)
    elif kind == "L":
        w.hbar(mx - ln, mx + ln, my)
        w.vbar(mx + ln * r.choice([-1, 1]), my - ln, my)
    elif kind == "U":
        a = r.choice([1, 2, 4, 7])
        w.hbar(mx - a, mx + a, my + a)
        w.vbar(mx - a, my, my + a)
        w.vbar(mx + a, my, my + a)
    elif kind == "room_start":
        a, b = r.choice([1, 2, 3, 5]), r.choice([1, 2, 3, 5])
        w.room(sx - a, sy - b, sx + a, sy + b, (r.randrange(4), r.randrange(0, 2 * min(a, b) + 1)) if r.random() < 0.7 else None)
    elif kind == "room_end":
        a, b = r.choice([1, 2, 3, 5]), r.choice([1, 2, 3, 5])
        w.room(ex - a, ey - b, ex + a, ey + b, (r.randrange(4), r.randrange(0, 2 * min(a, b) + 1)) if r.random() < 0.5 else None)
    elif kind == "closed_end":
        a = r.choice([0, 1, 2])
        w.room(ex - a, ey - a, ex + a, ey + a)
    elif kind == "closed_rough":
        # a closed room around the end with one-sided bits inside and on the ring: the two huggers' loops differ in length
        a = r.choice([1, 2, 3, 4])
        w.room(ex - a, ey - a, ex + a, ey + a)
        w.sprinkle(r, ex - a - 1, ey - a - 1, ex + a + 1, ey + a + 1, r.choice([0.03, 0.08, 0.15]))
    elif kind == "stairs":
        # a long staircase wall through the line's middle: many turns, so a hugger's buffer fills
        x, y = mx - 70, my - 70
        for i in range(r.choice([40, 130, 140])):
            w.edge(x + i, y + i, 2)
            w.edge(x + i, y + i + 1, 1)
    elif kind == "maze":
        pad = 2
        w.maze(r, min(sx, ex) - pad, min(sy, ey) - pad, max(sx, ex) + pad, max(sy, ey) + pad, r.choice([0.1, 0.25, 0.4]))
    elif kind == "sprinkle":
        pad = 2
        w.sprinkle(r, min(sx, ex) - pad, min(sy, ey) - pad, max(sx, ex) + pad, max(sy, ey) + pad, r.choice([0.02, 0.08, 0.2]))
    elif kind == "two_bars":
        w.hbar(mx - ln, mx + ln, my)
        w.vbar(mx + ln + 1, my - ln, my + ln)
        w.hbar(mx - 2 * ln, mx + ln + 1, my - ln)


KINDS = ["hbar", "vbar", "bar_near_start", "L", "U", "room_start", "room_end", "closed_end", "maze", "sprinkle", "two_bars", "closed_rough", "stairs", "none"]


def pick_squares(r, wrap):
    """A start and an end square: a short or a long line, anywhere (wrap puts them at the byte edge)."""
    n = r.choice([3, 6, 10, 10, 20, 40])
    if wrap:
        sx = r.choice([0, 1, 2, 253, 254, 255])
        sy = r.choice([0, 1, 2, 253, 254, 255])
        ex = min(255, max(0, sx + r.randint(-n, n)))
        ey = min(255, max(0, sy + r.randint(-n, n)))
        return sx, sy, ex, ey
    sx, sy = r.randrange(45, 210), r.randrange(45, 210)
    k = r.random()
    dx, dy = r.randint(-n, n), r.randint(-n, n)
    if k < 0.1:
        dx = 0
    elif k < 0.2:
        dy = 0
    elif k < 0.3:
        dy = dx if r.random() < 0.5 else -dx
    return sx, sy, sx + dx, sy + dy


def world(r, kind=None, wrap=False, squares=None):
    """(start, end, walls, quiet) of a case: an obstacle of the kind (random when None) between two squares."""
    sx, sy, ex, ey = squares if squares is not None else pick_squares(r, wrap)
    kind = kind or r.choice(KINDS)
    w = Walls()
    shape(r, w, sx, sy, ex, ey, kind)
    if wrap and r.random() < 0.5:
        # walls on the byte edge itself
        for t in range(0, 6):
            w.edge(r.choice([0, 255]), r.randrange(256), r.randrange(4))
            w.edge(r.randrange(256), r.choice([0, 255]), r.randrange(4))
    big = wrap or max(abs(ex - sx), abs(ey - sy)) > 8 or kind in ("maze", "room_start", "room_end", "U", "two_bars", "sprinkle", "closed_end", "closed_rough", "stairs")
    return (sx, sy), (ex, ey), w, big


# ---------------------------------------------------------------------------------------------------------------- the cases


def random_globals(r):
    g = {name: r.choice([0, 1, 5, 0xFFFFFFFF, r.randrange(1 << 32)]) for name in GLOBALS}
    g["rec"] = r.choice([0, 0, 1, 1, 0x100, 0xFFFFFFFF])
    return g


def make_path(r, count, start, end):
    """A path buffer: random bytes everywhere, then the count and the start and end squares."""
    data = bytearray(r.randrange(256) for _ in range(PATH_SIZE))
    data[0] = count & 255
    data[2], data[3], data[4], data[5] = start[0] & 255, start[1] & 255, end[0] & 255, end[1] & 255
    return bytes(data)


def trivial_case(r, kind=None, wrap=False, squares=None, mode=None, rec=None, quiet=None):
    """wallhug_trivial on a world."""
    s, e, w, big = world(r, kind, wrap, squares)
    c = {"op": "triv", "g": random_globals(r), "mode": r.choice(MODES) if mode is None else mode}
    if rec is not None:
        c["g"]["rec"] = rec
    c["path"] = make_path(r, r.randrange(256), s, e)
    c["walls"] = w.table(big if quiet is None else quiet)
    return c


def cont_case(r, kind=None, wrap=False, squares=None, limit=None, count=None, steps=None, rec=None, quiet=None):
    """wallhug_continue_trivial called directly: any limit, count on entry, step counter and start square (not always the path's)."""
    s, e, w, big = world(r, kind, wrap, squares)
    c = {"op": "cont", "g": random_globals(r)}
    c["limit"] = r.choice([0, 1, 2, 3, 7, 50, 50, 200, 500, 60000]) if limit is None else limit
    count = r.choice([0, 0, 0, 5, 0xF9, 0xFA, 0xFB]) if count is None else count
    c["g"]["steps"] = r.choice([0, 0, 0, 1, 2, 40]) if steps is None else steps
    c["g"]["mode"] = r.choice(MODES) & MASK
    if rec is not None:
        c["g"]["rec"] = rec
    begin = s if r.random() < 0.8 else (s[0] + r.randint(-3, 3), s[1] + r.randint(-3, 3))
    c["start"] = (begin[0] & 255) << 24 | (begin[1] & 255) << 16 | r.randrange(0x10000)
    c["path"] = make_path(r, count, s, e)
    c["walls"] = w.table((big or c["limit"] > 60) if quiet is None else quiet)
    return c


def trivial_edges():
    r = random.Random(201)
    # open fields: every octant and the axes, start == end
    for dx in (-7, -2, 0, 2, 7):
        for dy in (-7, -2, 0, 2, 7):
            yield trivial_case(r, "none", squares=(60, 60, 60 + dx, 60 + dy), quiet=False)
    for kind in KINDS:
        for rec in (0, 1):
            for wrap in (False, True):
                yield trivial_case(r, kind, wrap, rec=rec)
    # one bar across a short line, every offset: the first hit, both ways round
    for dx in (-5, 5):
        for dy in (-3, 0, 3):
            for k in range(3):
                c = trivial_case(r, "none", squares=(60, 60, 60 + dx, 60 + dy), rec=1, quiet=False)
                w = Walls()
                w.hbar(60 + dx // 2 - 1 - k, 60 + dx // 2 + 1 + k, 60 + dy // 2)
                c["walls"] = w.table(False)
                yield c
    # rough closed rooms around the end (the huggers meet after loops of different lengths) and long staircases (the buffers fill)
    for k in range(40):
        yield trivial_case(r, "closed_rough", squares=(60, 60, 70 + k % 5, 66 + k % 3), quiet=True)
    for k in range(6):
        yield trivial_case(r, "stairs", squares=(60, 60, 130, 120 + k), quiet=True)
    # an enclosed end, an enclosed start, walls everywhere
    for a in (0, 1, 2, 3):
        w = Walls()
        w.room(80 - a, 70 - a, 80 + a, 70 + a)
        c = trivial_case(r, "none", squares=(60, 60, 80, 70), quiet=True)
        c["walls"] = w.table(True)
        yield c
    c = trivial_case(r, "none", squares=(60, 60, 64, 62), quiet=False)
    c["walls"] = (15, [])
    yield c
    c = trivial_case(r, "none", squares=(60, 60, 60, 60), quiet=False)
    c["walls"] = (15, [])
    yield c
    c = trivial_case(r, "none", squares=(0, 0, 255, 255), quiet=True)
    yield c
    c = trivial_case(r, "none", squares=(255, 255, 0, 0), quiet=True)
    yield c


def cont_edges():
    r = random.Random(202)
    # the limits and the counts on entry, in an open field and behind a bar
    for limit in (0, 1, 2, 50, 500, 60000):
        for count in (0, 5, 0xF9, 0xFA, 0xFB):
            for kind in ("none", "hbar"):
                yield cont_case(r, kind, squares=(60, 60, 66, 63), limit=limit, count=count, steps=0, quiet=False)
    # the limit cuts the walk at each step, with and without a hit
    for limit in range(0, 14):
        for kind in ("none", "U"):
            yield cont_case(r, kind, squares=(60, 60, 64, 66), limit=limit, count=0, steps=0, rec=1, quiet=False)
    # the step counter above the limit, equal to it
    for steps, limit in ((5, 5), (6, 5), (0, 0), (1, 0)):
        yield cont_case(r, "hbar", squares=(60, 60, 60, 70), limit=limit, steps=steps, quiet=False)
    for dx in (-7, -2, 0, 2, 7):
        for dy in (-7, -2, 0, 2, 7):
            yield cont_case(r, "none", squares=(60, 60, 60 + dx, 60 + dy), limit=60000, count=0, steps=0, quiet=False)
    for kind in KINDS:
        for rec in (0, 1):
            yield cont_case(r, kind, rec=rec, limit=500, count=0)
            yield cont_case(r, kind, True, rec=rec, limit=500, count=0)
    # the corner squares near the path's end: mCount 0xf9, 0xfa, 0xfb on entry with turns in the line
    for count in (0xF8, 0xF9, 0xFA, 0xFB):
        for k in range(4):
            yield cont_case(r, "hbar", squares=(50, 50, 60 + k, 66), limit=50, count=count, steps=0, quiet=False)
            yield cont_case(r, "sprinkle", squares=(50, 50, 58 + k, 62), limit=40, count=count, steps=0, quiet=True)
    # a count on entry past the limits of the checks (0xfc to 0xff): the splices refuse, the corner appends wrap the count
    for count in (0xFC, 0xFD, 0xFE, 0xFF):
        for kind in ("none", "hbar", "U", "sprinkle"):
            yield cont_case(r, kind, squares=(50, 50, 58, 62), limit=500, count=count, steps=0, quiet=(kind == "sprinkle"))


def trivial_random(r):
    return trivial_case(r, wrap=r.random() < 0.15)


def cont_random(r):
    return cont_case(r, wrap=r.random() < 0.15)


TESTS = [
    FnTest("wallhug_continue_trivial", ppc, host, edge=cont_edges, rnd=cont_random),
    FnTest("wallhug_trivial", ppc, host, edge=trivial_edges, rnd=trivial_random),
]
