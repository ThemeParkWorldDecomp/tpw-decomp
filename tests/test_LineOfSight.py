"""The walk planner's line of sight and path smoothing (unit w195), against libtpw_unit_LineOfSight.dylib (tests/unit_LineOfSight.cpp), main app
("SimTheme Park"), src/World_sight.cpp:
  bresenham_start 0x101360ac, line_of_sight 0x1013623c, line_of_sight_cleanup 0x10136498, wallhug_cleanup 0x101365cc, wallhug_tricky 0x10136e7c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for
the original, and in the host library's memory (little-endian) for the translation. Real on both sides: the five functions (and so each
calls the next as the original does). Fakes on both sides that record every argument: MAP_wall_in_way 0x10101ca4 (answers from the case's WALL
TABLE: a fill mask for every square plus single squares' masks; bit d of a mask is a wall in direction d), wallhug_trivial 0x10137d04
(plans a real walk of adjacent squares, x first, y first or alternating, or answers the queues "wh_count", "wh_pt", "wh_ret" as
tests/test_MapDelta.py does; a planned walk longer than the case's limit answers 0x70000000) and SamsUtilities::UDebug 0x10000e40. labs and abs
are Python's, memmove and memcpy tpwtest's standard stubs (none of the four is logged on either side).

The world of a case: the seven globals (the six Bresenham words and the nav mode) at random values, the wall table, the planner, and for the
path functions a 0x208-byte path. What is compared: the answer, the seven globals, gWallhugStep (the bytes the binary holds against the
translation's table), all 0x208 bytes of the path and the event log.

No fused operations. A waypoint is passed as a word whose low half is random (the original reads only the top half). Not tested: a direction
outside 0 to 3 in line_of_sight (UDebug; bresenham_start never produces one), a path count of 253 or more (the buffer holds 253 points).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LineOfSight"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
BRES, LOS, LOSC, WCL, TRICKY = 0x101360AC, 0x1013623C, 0x10136498, 0x101365CC, 0x10136E7C
SIZES = {BRES: 324, LOS: 532, LOSC: 248, WCL: 2168, TRICKY: 76}
OWN = set(SIZES)

# ---- the callees faked on both sides
WALL, WALLHUG, UDEBUG = 0x10101CA4, 0x10137D04, 0x10000E40

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_los_addr(name)
ADDR = dict(xmajor=0x1034CAB8, xdir=0x1034CABC, ydir=0x1034CAC0, err=0x1034CAC4, dy=0x1034CAC8, dx=0x1034CACC, mode=0x1034CAD0,
            step=0x102A45A4)
GLOBALS = ["xmajor", "xdir", "ydir", "err", "dy", "dx", "mode"]
# ---- the host's sizes the module relies on, checked once against tpw_los_layout (catches a header that moved)
HOST_LAYOUT = [0x208, 6]
PATH_SIZE = 0x208
MODES = [0, 1, 2, 7, -1, 0x1234]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def plan(kind, limit, sx, sy, ex, ey):
    """The planner's walk (the same rule as tests/unit_LineOfSight.cpp): adjacent squares from the start to the end, or None past `limit`."""
    pts = []
    x, y = sx, sy
    while (x, y) != (ex, ey):
        if len(pts) >= limit:
            return None
        if kind == 0:
            step_x = x != ex
        elif kind == 1:
            step_x = y == ey
        else:
            step_x = (len(pts) % 2 == 0 and x != ex) or y == ey
        if step_x:
            x += 1 if ex > x else -1
        else:
            y += 1 if ey > y else -1
        pts.append((x, y))
    return pts


def stub_wall(e):
    """MAP_wall_in_way(x, z, dir, mode): records the four arguments, answers from the case's wall table."""
    x, z, d, mode = e.r(3) & 0xFF, e.r(4) & 0xFF, e.r(5) & 0xFF, s32(e.r(6))
    if not e.quiet:
        e.cl.rec("MAP_wall_in_way", (x, z, d, mode))
    mask = e.walls.get((x, z), e.fill)
    e.set_r(3, (mask >> d) & 1 if d < 8 else 0)


def expand(queues):
    """The harness holds 64 values per queue: each of the three fake queues becomes keys "name:chunk" of 32 values, padded with its last value
    to 512 (the original queue repeats its last value forever)."""
    out = {}
    for name, vals in queues.items():
        vals = list(vals) + [vals[-1]] * (512 - len(vals))
        for k in range(16):
            out["%s:%d" % (name, k)] = vals[32 * k:32 * k + 32]
    return out


def qret(e, name):
    n = e.qn.get(name, 0)
    e.qn[name] = n + 1
    return e.cl.ret("%s:%d" % (name, min(n // 32, 15)), 0)


def stub_wallhug(e):
    """wallhug_trivial(path, mode): records the mode and the start and end bytes; writes the planned walk (count, then the points)."""
    L = e.cl
    p = e.r(3)
    b = e.read(p + 2, 4)
    L.rec("wallhug_trivial", [s32(e.r(4))] + list(b))
    if e.kind < 3:
        pts = plan(e.kind, e.limit, b[0], b[1], b[2], b[3])
        if pts is None:
            e.set_r(3, 0x70000000)
            return
        e.write(p, bytes([len(pts)]))
        if pts:
            e.write(p + 6, bytes(v for pt in pts for v in pt))
        e.set_r(3, T.u32(qret(e, "wh_ret")))
        return
    n = qret(e, "wh_count") & 0xFF
    pts = [qret(e, "wh_pt") & 0xFF for _ in range(2 * n)]
    e.write(p, bytes([n]))
    if pts:
        e.write(p + 6, bytes(pts))
    e.set_r(3, T.u32(qret(e, "wh_ret")))


STUBS.internal(WALL, stub_wall)
STUBS.internal(WALLHUG, stub_wallhug)
STUBS.internal(UDEBUG, lambda e: e.cl.rec("UDebug", (), (e.cstr(e.r(3))[:159],)))
STUBS.imp("labs", lambda e: e.set_r(3, T.u32(abs(s32(e.r(3))))))
STUBS.imp("abs", lambda e: e.set_r(3, T.u32(abs(s32(e.r(3))))))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.walls = {(x, y): m for x, y, m in case["walls"][1]}
        e.fill = case["walls"][0] & 255
        e.quiet = bool(case["walls"][0] & 256)
        e.kind, e.limit = case["plan"]
        e.qn = {}
        self.L = e.cl = CL.PpcLog({}, expand(case["queues"]))
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
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, expand(case["queues"]))
        fill, entries = case["walls"]
        packed = (C.c_uint32 * (len(entries) + 1))(*[(x << 16) | (y << 8) | m for x, y, m in entries])
        lib.tpw_los_setup(case["plan"][0], case["plan"][1], fill, packed, len(entries))

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_los_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_los_alloc(n)       # zeroed, 32-bit addressable (platform/arena.cpp)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        lib = self.lib
        a = [v & MASK for v in args]
        if entry in (LOSC, WCL, TRICKY):
            a[0] = args[0]      # the host's path pointer is a 64-bit address
        if entry == BRES:
            lib.tpw_los_bresenham(a[0], a[1])
            return 0
        if entry == LOS:
            return lib.tpw_los_sight(a[0], a[1]) & MASK
        if entry == LOSC:
            return lib.tpw_los_cleanup(a[0], a[1]) & MASK
        if entry == WCL:
            return lib.tpw_los_wallhug_cleanup(a[0], a[1]) & MASK
        return lib.tpw_los_tricky(a[0], s32(a[1])) & MASK


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_los_addr.restype, lib.tpw_los_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_los_alloc.restype, lib.tpw_los_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_los_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_los_setup.argtypes = [C.c_int32, C.c_int32, C.c_uint32, C.POINTER(C.c_uint32), C.c_int32]
    lib.tpw_los_bresenham.argtypes = [C.c_uint32, C.c_uint32]
    lib.tpw_los_sight.restype, lib.tpw_los_sight.argtypes = C.c_int32, [C.c_uint32, C.c_uint32]
    lib.tpw_los_cleanup.restype, lib.tpw_los_cleanup.argtypes = C.c_int32, [C.c_uint64, C.c_uint32]
    lib.tpw_los_wallhug_cleanup.restype, lib.tpw_los_wallhug_cleanup.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32]
    lib.tpw_los_tricky.restype, lib.tpw_los_tricky.argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_los_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's globals and path through Side: the same code for both memories."""
    for name, v in zip(GLOBALS, case["g"]):
        s.words(s.g(name), [v])
    s.path = 0
    if "path" in case:
        s.path = s.alloc(PATH_SIZE)
        s.write(s.path, case["path"])


def execute(s, case):
    """The call."""
    op = case["op"]
    if op == "bres":
        s.call(BRES, case["args"])      # void: r3 is whatever the last labs left
        return 0
    if op == "los":
        return s.call(LOS, case["args"])
    if op == "losc":
        return s.call(LOSC, [s.path, case["first"]])
    if op == "wcl":
        return s.call(WCL, [s.path, case["result"]])
    return s.call(TRICKY, [s.path, case["mode"]])


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides."""
    out = {"answer": answer, "log": s.log(), "step": s.read(s.g("step"), 8)}
    for name in GLOBALS:
        out[name] = s.rd_word(s.g(name))
    if s.path:
        out["path"] = s.read(s.path, PATH_SIZE)
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


# ---------------------------------------------------------------------------------------------------------------- case generators


def wp(r, x, y):
    """A waypoint as a register: x in bits 24 to 31, y in 16 to 23, the low half random (the original ignores it)."""
    return (x & 0xFF) << 24 | (y & 0xFF) << 16 | r.randrange(0x10000)


def base(r, op):
    """The world every case starts from: random globals, no walls, a random planner."""
    return {"op": op, "g": [r.choice([0, 1, 5, 0xFFFFFFFF, r.randrange(1 << 32)]) for _ in GLOBALS], "queues": {}, "walls": (0, []),
            "plan": (r.choice([0, 1, 2, 3]), r.choice([8, 40, 300]))}


def sprinkle(r, squares, density):
    """Wall entries for the squares: each direction bit set with the probability `density`."""
    out = []
    for x, y in sorted(squares):
        m = 0
        for d in range(4):
            if r.random() < density:
                m |= 1 << d
        if m:
            out.append((x, y, m))
    return out


def around(points, rad=2):
    return {((x + dx) & 255, (y + dy) & 255) for x, y in points for dx in range(-rad, rad + 1) for dy in range(-rad, rad + 1)}


def boxed(points, rad=2):
    """The squares of the points' bounding box grown by rad (or just around each point when the box is large)."""
    xs, ys = [p[0] for p in points], [p[1] for p in points]
    if (max(xs) - min(xs) + 2 * rad + 1) * (max(ys) - min(ys) + 2 * rad + 1) > 6000:
        return around(points, rad)
    return {(x & 255, y & 255) for x in range(min(xs) - rad, max(xs) + rad + 1) for y in range(min(ys) - rad, max(ys) + rad + 1)}


def walls_for(r, points, density=None, rad=2):
    d = r.choice([0, 0.02, 0.1, 0.4]) if density is None else density
    return (0, sprinkle(r, boxed(points, rad), d))


# ---- bresenham_start
def bres_case(r, a=None, b=None):
    c = base(r, "bres")
    if a is None:
        a = (r.randrange(256), r.randrange(256))
    if b is None:
        k = r.random()
        if k < 0.1:
            b = (a[0], r.randrange(256))
        elif k < 0.2:
            b = (r.randrange(256), a[1])
        elif k < 0.3:
            b = a
        elif k < 0.5:
            b = (min(255, max(0, a[0] + r.randint(-6, 6))), min(255, max(0, a[1] + r.randint(-6, 6))))
        else:
            b = (r.randrange(256), r.randrange(256))
    c["args"] = [wp(r, *a), wp(r, *b)]
    return c


def bres_edges():
    r = random.Random(101)
    for fx, fy in ((10, 10), (0, 0), (255, 255), (250, 3), (3, 250)):
        for dx in (-9, -3, -1, 0, 1, 3, 9):
            for dy in (-9, -3, -1, 0, 1, 3, 9):
                yield bres_case(r, (fx, fy), (min(255, max(0, fx + dx)), min(255, max(0, fy + dy))))
    for a, b in (((0, 0), (255, 255)), ((255, 255), (0, 0)), ((0, 255), (255, 0)), ((250, 5), (3, 5)), ((5, 250), (5, 3)), ((250, 250), (3, 3)),
                 ((1, 1), (1, 1)), ((0, 0), (0, 0)), ((255, 0), (255, 0)), ((128, 128), (129, 127))):
        yield bres_case(r, a, b)


# ---- line_of_sight
def los_case(r, a=None, b=None, density=None):
    c = base(r, "los")
    if a is None:
        a = (r.randrange(2, 250), r.randrange(2, 250))
    if b is None:
        n = r.choice([3, 8, 20, 60])
        k = r.random()
        dx, dy = r.randint(-n, n), r.randint(-n, n)
        if k < 0.1:
            dx = 0
        elif k < 0.2:
            dy = 0
        elif k < 0.3:
            dy = dx if r.random() < 0.5 else -dx
        elif k < 0.35:
            dx = dy = 0
        b = (a[0] + dx, a[1] + dy)
        b = (min(255, max(0, b[0])), min(255, max(0, b[1])))
    c["g"][6] = r.choice(MODES) & MASK
    c["args"] = [wp(r, *a), wp(r, *b)]
    c["walls"] = walls_for(r, [a, b], density)
    return c


def los_edges():
    r = random.Random(102)
    # no walls: every octant, the axes and from == to
    for dx in (-6, -2, 0, 2, 6):
        for dy in (-6, -2, 0, 2, 6):
            yield los_case(r, (50, 50), (50 + dx, 50 + dy), 0)
    yield los_case(r, (250, 4), (255, 9), 0)
    yield los_case(r, (4, 250), (0, 255), 0)
    yield los_case(r, (255, 255), (0, 0), 0)
    yield los_case(r, (0, 0), (255, 255), 0)
    yield los_case(r, (250, 5), (3, 5), 0)
    yield los_case(r, (5, 250), (5, 3), 0)
    # one wall bit on one square of the box, for a few lines: every first-step test and every corner test
    for a, b in (((20, 20), (24, 22)), ((20, 20), (22, 24)), ((30, 30), (26, 27)), ((30, 30), (27, 25)), ((10, 10), (14, 10)), ((10, 10), (10, 15)),
                 ((40, 40), (43, 43)), ((40, 40), (37, 43))):
        for x, y in sorted(boxed([a, b], 1)):
            for d in range(4):
                c = los_case(r, a, b, 0)
                c["walls"] = (0, [(x, y, 1 << d)])
                yield c
    # a wall on the first step only; a wall everywhere
    c = los_case(r, (20, 20), (26, 23), 0)
    c["walls"] = (15, [])
    yield c
    c = los_case(r, (20, 20), (20, 20), 0)
    c["walls"] = (15, [])
    yield c


def los_random(r):
    return los_case(r)


# ---- paths
def make_path(r, count, start=None, kind=None, lo=8, hi=240):
    """A path buffer: random bytes everywhere, then the header and `count` points on a walk of the given kind."""
    data = bytearray(r.randrange(256) for _ in range(PATH_SIZE))
    x, y = start if start is not None else (r.randrange(lo, hi), r.randrange(lo, hi))
    kind = kind or r.choice(["straight", "zigzag", "random", "diag", "mixed"])
    sx, sy = x, y
    pts = []
    dirx, diry = r.choice([-1, 0, 1]), r.choice([-1, 0, 1])
    for i in range(count):
        if kind == "straight":
            sdx, sdy = dirx, diry if (dirx or diry) else 1
        elif kind == "zigzag":
            sdx, sdy = (1, 0) if i % 2 == 0 else (0, 1)
            sdx, sdy = sdx * dirx or sdx, sdy * diry or sdy
        elif kind == "diag":
            sdx, sdy = r.choice([(1, 1), (-1, 1), (1, -1), (-1, -1)])
        elif kind == "random":
            sdx, sdy = r.choice([(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (2, 1), (1, 3), (3, 3), (3, 5), (2, 2), (4, 6), (-3, 1)])
        else:
            sdx, sdy = r.choice([(1, 0), (0, 1), (-1, 0), (0, -1), (1, 1), (-1, 1), (1, -1), (-1, -1), (3, 3), (5, 1), (1, 3), (3, 5), (2, 4), (0, 2)])
        x = min(255, max(0, x + sdx))
        y = min(255, max(0, y + sdy))
        pts.append((x, y))
    data[0] = count
    data[2], data[3] = sx, sy
    data[4], data[5] = (pts[-1] if pts else (r.randrange(256), r.randrange(256)))
    for i, (px, py) in enumerate(pts):
        data[6 + 2 * i], data[7 + 2 * i] = px, py
    return bytes(data), [(sx, sy)] + pts


def queue_plan(r, c, n_legs, legs_real, ret_choices=(0,)):
    """The wallhug_trivial queues: legs of 0 to 6 points, random squares, answers from ret_choices."""
    counts, pts, rets = [], [], []
    for _ in range(n_legs):
        n = r.choice([0, 1, 2, 3, 4, 6])
        counts.append(n)
        x, y = r.randrange(5, 250), r.randrange(5, 250)
        for _ in range(n):
            pts += [x, y]
            x += r.choice([-1, 0, 1])
            y += r.choice([-1, 0, 1])
        rets.append(r.choice(ret_choices))
    c["queues"] = {"wh_count": counts or [0], "wh_pt": pts or [0], "wh_ret": rets or [0]}


# ---- line_of_sight_cleanup
def quiet(c, count, limit=100):
    """Paths of `limit` points or more would overflow the 4096-event log with MAP_wall_in_way events: bit 8 of the fill word turns that one event off
    on both sides (the answers, the path and every other event are still compared)."""
    if count >= limit:
        c["walls"] = (c["walls"][0] | 256, c["walls"][1])
    return c


def losc_case(r, count=None, first=None, density=None, kind=None):
    c = base(r, "losc")
    count = r.choice([0, 1, 2, 5, 5, 12, 30]) if count is None else count
    data, pts = make_path(r, count, kind=kind)
    c["path"] = data
    if first is None:
        first = r.choice([0, 0, 1, 2, max(0, count // 2), max(0, count - 1), count, count + 3, r.randrange(count + 4)])
    c["first"] = first
    c["g"][6] = r.choice(MODES) & MASK
    c["walls"] = walls_for(r, pts, density)
    return quiet(c, count)


def losc_edges():
    r = random.Random(103)
    for count in (0, 1, 2, 5, 30, 200):
        for first in (0, 1, count // 2, max(0, count - 1), count, count + 3):
            for kind in ("straight", "zigzag"):
                for density in ((0, 0.1) if count < 100 else (0,)):
                    yield losc_case(r, count, first, density, kind)
    c = losc_case(r, 200, 0, 0, "random")
    c["walls"] = (15 | 256, [])
    yield c
    c = losc_case(r, 200, 3, 0, "mixed")
    c["walls"] = (15 | 256, [])
    yield c


def losc_random(r):
    return losc_case(r)


# ---- wallhug_cleanup
def wcl_case(r, count=None, kind=None, density=None, result=None, plan_kind=None, fill=0):
    c = base(r, "wcl")
    count = r.choice([0, 1, 2, 3, 4, 6, 8, 12, 20, 30, 40]) if count is None else count
    data, pts = make_path(r, count, kind=kind)
    c["path"] = data
    c["result"] = r.choice([0, 1, 0x70000000, 0xFFFFFFFF, r.randrange(1 << 32)]) if result is None else result
    if plan_kind is not None:
        c["plan"] = (plan_kind, c["plan"][1])
    c["g"][6] = r.choice(MODES) & MASK
    c["walls"] = (fill, sprinkle(r, boxed(pts, 2), r.choice([0, 0.02, 0.1, 0.4]) if density is None else density) if fill == 0 else [])
    if c["plan"][0] == 3:
        queue_plan(r, c, 3 * count + 3, True, (0, 0, 0, 0x70000000, 5))
    return quiet(c, count, 5)


def diag_case(r, quad, bits, count, fill=0):
    """A one-square diagonal in a quadrant with the eight wall bits of its two squares set by `bits` (bit 0 to 3: directions of the square the
    step reaches, bit 4 to 7: directions of the start square), every other square `fill`. The planner answers 0x70000000 so the diagonal
    pass is reached."""
    sx, sy = quad
    x0, y0 = r.randrange(10, 240), r.randrange(10, 240)
    data = bytearray(r.randrange(256) for _ in range(PATH_SIZE))
    pts = [(x0 + sx, y0 + sy)]
    for i in range(count - 1):
        pts.append((min(255, max(0, pts[-1][0] + r.choice([-1, 0, 1]))), min(255, max(0, pts[-1][1] + r.choice([-1, 0, 1])))))
    data[0] = count
    data[2], data[3] = x0, y0
    data[4], data[5] = pts[-1]
    for i, (px, py) in enumerate(pts):
        data[6 + 2 * i], data[7 + 2 * i] = px, py
    c = base(r, "wcl")
    c["path"] = bytes(data)
    c["result"] = r.choice([0, 1, 0xFFFFFFFF])
    c["g"][6] = r.choice(MODES) & MASK
    c["plan"] = (3, 8)
    c["queues"] = {"wh_count": [0], "wh_pt": [0], "wh_ret": [0x70000000]}
    c["walls"] = (fill, [(x0 + sx, y0 + sy, bits & 15), (x0, y0, bits >> 4 & 15)])
    return quiet(c, count)


def wcl_edges():
    r = random.Random(104)
    # one-square diagonals: every quadrant with every pattern of the eight wall bits of its blocks (256 patterns for 4 quadrants is a lot:
    # all 16 patterns of the four bits each quadrant reads, spread over the two squares, then 40 random patterns)
    for quad in ((1, 1), (-1, 1), (1, -1), (-1, -1)):
        for bits in range(256):
            if bits < 64 or r.random() < 0.25:
                yield diag_case(r, quad, bits, 1)
        for bits in (0, 15, 240, 255, 5, 10, 160, 80):
            yield diag_case(r, quad, bits, 3)
    for count in (0, 1, 2, 3):
        for result in (0, 1, 0x70000000, 0xFFFFFFFF):
            yield wcl_case(r, count, "mixed", 0, result)
    # the splice limits: legs of the queue with 54, 55 and 56 points on paths of 200 points; 250, 251 and 252 points and the diagonal limit
    for count in (200,):
        for legn in (53, 54, 55, 56):
            c = wcl_case(r, count, "straight", 0, 0, 3, 15)
            c["queues"] = {"wh_count": [legn, 0], "wh_pt": [100, 100] * legn + [0], "wh_ret": [0]}
            yield c
    for count in (249, 250, 251, 252):
        for quad, bits in (((1, 1), 1), ((-1, -1), 4), ((1, -1), 16), ((-1, 1), 0)):
            yield diag_case(r, quad, bits, count, 15)
    # a planner that always fails; a planner of real walks over straight and zig-zag paths without walls
    for pk in (0, 1, 2):
        for kind in ("straight", "zigzag", "mixed", "random"):
            for count in (3, 8, 20):
                yield wcl_case(r, count, kind, 0, 1, pk)
    c = wcl_case(r, 40, "mixed", 0, 1, 0)
    c["plan"] = (0, 0)
    yield c


def wcl_random(r):
    return wcl_case(r)


# ---- wallhug_tricky
def tricky_case(r, mode=None, count=None):
    c = base(r, "tricky")
    data, pts = make_path(r, 0)
    data = bytearray(data)
    a = (r.randrange(10, 240), r.randrange(10, 240))
    b = (a[0] + r.randint(-12, 12), a[1] + r.randint(-12, 12))
    data[2], data[3], data[4], data[5] = a[0], a[1], b[0] & 255, b[1] & 255
    c["path"] = bytes(data)
    c["mode"] = r.choice(MODES) if mode is None else mode
    c["walls"] = walls_for(r, [a, b], None, 3)
    if c["plan"][0] == 3:
        queue_plan(r, c, 6, False, (0, 1, 0x70000000, 0xFFFFFFFF, 7))
        c["queues"]["wh_count"] = [r.choice([2, 3, 4, 6, 0])] + c["queues"]["wh_count"]
        c["queues"]["wh_pt"] = [a[0] + 1, a[1], b[0], b[1]] * 2 + c["queues"]["wh_pt"]
    else:
        c["queues"]["wh_ret"] = [r.choice([0, 1, 0x70000000, 0xFFFFFFFF, 7])]
    return c


def tricky_edges():
    r = random.Random(105)
    for mode in MODES:
        for pk in (0, 1, 2, 3):
            c = tricky_case(r, mode)
            c["plan"] = (pk, 40)
            if pk == 3:
                c["queues"] = {"wh_count": [4, 0, 0], "wh_pt": [20, 20, 21, 20, 22, 21, 23, 22], "wh_ret": [5]}
            else:
                c["queues"] = {"wh_ret": [r.choice([0, 1, 0xFFFFFFFF])]}
            yield c
    c = tricky_case(r, 2)
    c["plan"] = (0, 0)
    yield c


def tricky_random(r):
    return tricky_case(r)


TESTS = [
    FnTest("bresenham_start", ppc, host, edge=bres_edges, rnd=bres_case),
    FnTest("line_of_sight", ppc, host, edge=los_edges, rnd=los_random),
    FnTest("line_of_sight_cleanup", ppc, host, edge=losc_edges, rnd=losc_random),
    FnTest("wallhug_cleanup", ppc, host, edge=wcl_edges, rnd=wcl_random),
    FnTest("wallhug_tricky", ppc, host, edge=tricky_edges, rnd=tricky_random),
]
