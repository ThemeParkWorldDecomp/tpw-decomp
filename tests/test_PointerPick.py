"""Differential test for src/TPRide_pointer.cpp (main app 0x10065f50, unit w134): CalculatePointerWorldPosn, the pointer's world
position from the camera ray through the height field and the rides, against libtpw_unit_PointerPick.dylib.

The four callees are fakes on both sides and record their arguments (FindPointedRide in two events, because an event holds twelve words):
FindPointedRide 0x10065b24 (answers a queue of ride labels, fills gPointedRidePosition from a queue when it answers a ride),
SetPotentiallyHighlightedRide 0x1006cfa4, VectorThroughLMatrixNoWNoTrans 0x100c2040 (replaces the point with scripted words, so the ray
is the test's) and DistanceBetweenPoints 0x100c3198 (answers a queue). The C library calls (sin, cos, atan, sqrt, modf) are Python's math.

A case is a dict of words: the sine table, the camera and pointer globals, the height field (heights, quad flags, header flags), the
scripted vectors, ride answers and distances, which of the three outputs are NULL. Compared: the position, the pickup, the result, the label
of gPointedRide, the once byte and factor, gPointedRidePosition, and the call logs.

Unicorn computes fmadds in double precision and rounds twice; HardwareRounding (a code hook per fused instruction, as in
test_RideAnimInterp.py) gives each the processor's single rounding.

The widescreen pick (unit v5L51): a library built with the port fixes, with the unit space of a 16:9 screen (2730 units), scales the x of the
three corners by 0.75 view centre x / view centre y before they go through the camera matrix, so the ray reaches as far across as the
widened 3D view. "the widescreen pick" runs 16:9 cases on the original as they are and expects the host's log to hold the same corners
with x scaled that way, the rest unchanged. A library built without the port fixes keeps the 4:3 corners; there its own log is scaled the
same way, so the test is the 4:3 comparison again.

Not tested: a ray that never ends (the original loops while the height is at or above the limit, so the cases keep the ray descending or
make FindPointedRide answer a ride in the end); NaN and infinite inputs.
"""
import ctypes as C
import math
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_PointerPick"
ENTRY = 0x10065F50
SIZE = 4332
MASK = 0xFFFFFFFF
NAN_BITS = 0x7FC00000
A = dict(
    gTPGlobals=0x103B74C8, gCameraMatrix=0x102B5B74, gCameraViewPoint=0x102B5BC0, gPointerScreenXY=0x102B5A94,
    gCameraFov=0x1029CC84, gViewCentreX=0x102B2B6C, gViewCentreY=0x102B2B68, gHeightfieldHeader=0x102C6C7C,
    gPointedRide=0x102C6CE4, gPointedRidePosition=0x102C6CCC, gPointerFovOnce=0x102C6CA0, gPointerFovFactor=0x102C6CA4)
SENTINEL = [0xA5A5A501, 0xA5A5A502, 0xA5A5A503, 0xA5A5A504, 0xA5A5A505, 0xA5A5A506, 0x5A5A5A5A]


def f32(x):
    return T.f32(x)


def fb(x):
    """The bit pattern of x as a single."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon(word):
    if (word & 0x7F800000) == 0x7F800000 and (word & 0x7FFFFF):
        return NAN_BITS
    return word


# ------------------------------------------------------------------ the original's C library calls


def modf(e):
    """modf(double, double*): the fraction in f1, the whole part stored through r5."""
    fraction, whole = math.modf(e.f(1))
    e.write(e.r(5), struct.pack(">d", whole))
    e.set_f1(fraction)


# ------------------------------------------------------------------ the emulator's fused multiply-adds

LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """fmadds, fmsubs, fnmadds and fnmsubs with the processor's single rounding (see test_RideAnimInterp.py)."""

    changed = 0
    seen = 0

    def __init__(self, e, ranges):
        self.e, self.pending, self.hooks, self.ranges = e, None, [], ranges

    def __enter__(self):
        for begin, end in self.ranges:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            target, value = self.pending
            self.pending = None
            corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
            if fpr(e, target) != corrected and not (value != value and fpr_double(e, target) != fpr_double(e, target)):
                HardwareRounding.changed += 1
            uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        HardwareRounding.seen += 1
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        for x in (a, c, b):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        subtract = kind in (28, 30)
        value = float(LIBM.fmaf(a, c, -b if subtract else b))
        if kind in (30, 31):
            value = -value
        self.pending = (rd, value)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ------------------------------------------------------------------ the cases


def make(r, **kw):
    """A case from the random source r; keyword arguments replace parts of it."""
    c = {}
    c["mask"] = r.choice([15, 63, 255, 4095])
    c["scale"] = fb(f32(r.uniform(1.0, 800.0)))
    c["table"] = [fb(f32(r.uniform(-1.0, 1.0))) for _ in range(c["mask"] + 1)]
    c["fov"] = fb(f32(1.5707964 if r.random() < 0.4 else r.uniform(0.5, 2.5)))
    c["cx"], c["cy"] = fb(f32(r.uniform(200, 1000))), fb(f32(r.uniform(200, 800)))
    cxf, cyf = struct.unpack(">f", struct.pack(">I", c["cx"]))[0], struct.unpack(">f", struct.pack(">I", c["cy"]))[0]
    c["px"], c["py"] = fb(f32(r.uniform(0, 2 * cxf))), fb(f32(r.uniform(0, 2 * cyf)))
    c["once"] = r.choice([0, 1, 1, 0x80, 0xFF])
    c["factor"] = fb(f32(r.uniform(0.5, 1.2)))
    # the field
    w, d = r.randint(2, 6), r.randint(2, 6)
    ox, oz = f32(r.uniform(-30, 10)), f32(r.uniform(-30, 10))
    sx, sz = f32(r.uniform(4, 14)), f32(r.uniform(4, 14))
    if r.random() < 0.3:
        sz = sx
    heights = [f32(r.uniform(0, 12)) for _ in range((w + 1) * (d + 1))]
    if r.random() < 0.15:
        heights = [f32(r.choice([0.0, 3.0, 6.0])) for _ in heights]
    low, high = min(heights), max(heights)
    if r.random() < 0.15:
        low, high = f32(r.uniform(-5, 5)), f32(r.uniform(5, 20))
    c["w"], c["d"] = w, d
    c["field"] = [fb(v) for v in (ox, oz, sx, sz, low, high)]
    c["heights"] = [fb(v) for v in heights]
    c["quads"] = [r.choice([0, 2, 1, 3, r.getrandbits(16)]) for _ in range(w * d)]
    roll = r.random()
    if roll < 0.08:
        c["mode"], c["flags"] = 0, 0
    elif roll < 0.14:
        c["mode"], c["flags"] = 1, r.choice([0, 1, 4, 0xFFFFFFFD])
    elif roll < 0.22:
        c["mode"], c["flags"] = 2, r.choice([0, 1, 4, 0xFFFFFFFD])
    else:
        c["mode"], c["flags"] = 2, r.choice([2, 2, 3, 6, 0xFFFFFFFF])
    cam = (f32(ox + sx * w * r.uniform(0.0, 1.0)), f32(r.uniform(8, 30)), f32(oz + sz * d * r.uniform(0.0, 1.0)))
    c["cam"] = [fb(v) for v in cam]
    # the ray: three points the matrix fake hands back
    up = r.random() < 0.08
    ax, az = r.uniform(-0.3, 0.3), r.uniform(-0.3, 0.3)
    ay = r.uniform(0.5, 1.0) if up else r.uniform(-1.0, -0.5)
    a = (ax, ay, az)
    b = tuple(v + r.uniform(-0.15, 0.15) for v in a)
    cc = tuple(v + r.uniform(-0.15, 0.15) for v in a)
    c["vec"] = [fb(f32(v)) for v in a + b + cc]
    # the rides
    queue = [0] * r.choice([0, 0, 1, 2, 5, 12])
    if up or r.random() < 0.3:
        queue.append(r.choice([1, 2]))
    c["rides"] = queue or [0]
    c["ridepos"] = [fb(f32(r.uniform(-50, 100))) for _ in range(6)]
    c["ridepos0"] = [fb(f32(r.uniform(-50, 100))) for _ in range(3)]
    c["ride0"] = r.choice([0, 0, 1, 2])
    base = r.uniform(0, 100)
    c["dist"] = [fb(f32(base)), fb(f32(base if r.random() < 0.2 else r.uniform(0, 100)))]
    c["nulls"] = r.choice([0, 0, 0, 0, 0, 1, 2, 4, 3, 5, 6, 7])
    c["in_pos"] = SENTINEL[0:3]
    c["in_pick"] = SENTINEL[3:6]
    c["in_res"] = SENTINEL[6]
    c.update(kw)
    return c


def edges():
    import random
    r = random.Random(1234)
    yield make(r, mode=0, flags=0, ride0=1, nulls=0)
    yield make(r, mode=0, flags=0, ride0=0, nulls=0)
    yield make(r, mode=1, flags=4, ride0=2, nulls=0)
    yield make(r, mode=2, flags=0, ride0=1, nulls=2)
    yield make(r, mode=2, flags=2, nulls=0, rides=[0], once=0)
    yield make(r, mode=2, flags=2, nulls=1, rides=[0], once=1)
    yield make(r, mode=2, flags=2, nulls=2, rides=[0, 2], once=0x80)
    yield make(r, mode=2, flags=2, nulls=4, rides=[0])
    yield make(r, mode=2, flags=2, nulls=7)
    yield make(r, mode=2, flags=2, nulls=0, ride0=0, rides=[1])
    yield make(r, mode=2, flags=2, nulls=0, rides=[0, 0, 0, 1])
    for i in range(6):
        c = make(r, mode=2, flags=2, nulls=0)
        c["dist"] = [c["dist"][0], c["dist"][0]]
        yield c


def rnd(r):
    return make(r)


# ------------------------------------------------------------------ the original (PowerPC side)


def ppc(e, case):
    ride_addr = [0, e.alloc(16), e.alloc(16)]
    camera, matrix = A["gCameraViewPoint"], A["gCameraMatrix"]
    e.cl = CL.PpcLog({ride_addr[1]: 1, ride_addr[2]: 2, camera: 3, matrix: 4},
                     {"FindPointedRide": case["rides"], "RidePos": case["ridepos"], "Vec": case["vec"], "Dist": case["dist"]})
    st = CL.Stubs()

    def find(e):
        cell_x, cell_z, axis = e.r(3), e.r(4), e.r(5)
        x, y, z, y_step, step = [fb(e.f(i)) for i in (1, 2, 3, 4, 5)]
        direction = e.r(10)
        cam = e.u32(e.r(1) + 0x38)
        e.cl.rec("FindPointedRide", [cell_x, cell_z, axis, x, y, z, y_step])
        e.cl.rec("FindPointedRide2", [e.u32(direction), e.u32(direction + 4), e.u32(direction + 8), e.cl.lab(cam), step])
        answer = e.cl.ret("FindPointedRide")
        if answer:
            e.write(A["gPointedRidePosition"], struct.pack(">III", e.cl.ret("RidePos"), e.cl.ret("RidePos"), e.cl.ret("RidePos")))
        e.set_r(3, ride_addr[answer])

    def highlight(e):
        e.cl.rec("SetPotentiallyHighlightedRide", [e.cl.lab(e.r(3))])

    def vector(e):
        p = e.r(4)
        e.cl.rec("VectorThroughLMatrixNoWNoTrans", [e.cl.lab(e.r(3)), e.u32(p), e.u32(p + 4), e.u32(p + 8)])
        e.write(p, struct.pack(">III", e.cl.ret("Vec"), e.cl.ret("Vec"), e.cl.ret("Vec")))

    def distance(e):
        a, b = e.r(3), e.r(4)
        e.cl.rec("DistanceBetweenPoints", [e.u32(a + 4 * i) for i in range(3)] + [e.u32(b + 4 * i) for i in range(3)])
        e.set_f1(struct.unpack(">f", struct.pack(">I", e.cl.ret("Dist")))[0])

    st.internal(0x10065B24, find)
    st.internal(0x1006CFA4, highlight)
    st.internal(0x100C2040, vector)
    st.internal(0x100C3198, distance)
    st.imp("modf", modf)

    def words(addr, ws):
        e.write(addr, struct.pack(">" + "I" * len(ws), *[v & MASK for v in ws]))

    g = A["gTPGlobals"]
    words(g + 4, [case["mask"], case["scale"]])
    words(g + 0xC, case["table"])
    words(A["gCameraFov"], [case["fov"]])
    words(A["gViewCentreX"], [case["cx"]])
    words(A["gViewCentreY"], [case["cy"]])
    words(A["gPointerScreenXY"], [case["px"], case["py"]])
    e.write(A["gPointerFovOnce"], bytes([case["once"]]))
    words(A["gPointerFovFactor"], [case["factor"]])
    words(camera, case["cam"])
    words(A["gPointedRidePosition"], case["ridepos0"])
    words(A["gPointedRide"], [ride_addr[case["ride0"]]])
    if case["mode"] == 0:
        words(A["gHeightfieldHeader"], [0])
    else:
        header = e.alloc(0x100)
        words(header + 0x30, [case["flags"]])
        words(A["gHeightfieldHeader"], [header])
        if case["mode"] == 2:
            field = e.alloc(0x30)
            heights = e.alloc(4 * len(case["heights"]) + 16)
            quads = e.alloc(4 * len(case["quads"]) + 16)
            words(header + 0x6C, [field])
            words(field + 0x08, case["field"][0:4])
            words(field + 0x18, [case["w"], case["d"]])
            words(field + 0x20, case["field"][4:6])
            words(field + 0x28, [heights, quads])
            words(heights, case["heights"])
            for i, q in enumerate(case["quads"]):
                e.write(quads + 4 * i, struct.pack(">HH", q, 0))
    nulls = case["nulls"]
    pos, pick, res = e.alloc(16), e.alloc(16), e.alloc(8)
    words(pos, case["in_pos"])
    words(pick, case["in_pick"])
    words(res, [case["in_res"]])
    old = st.activate(e)
    try:
        with HardwareRounding(e, [(ENTRY, ENTRY + SIZE)]):
            T.call_ppc(e, ENTRY, [0 if nulls & 1 else pos, T.F(3.5), 0 if nulls & 2 else pick, 0 if nulls & 4 else res])
    finally:
        CL.Stubs.deactivate(e, old)
    out = [[e.u32(pos + 4 * i) for i in range(3)], [e.u32(pick + 4 * i) for i in range(3)], [e.u32(res)],
           [e.cl.lab(e.u32(A["gPointedRide"])), e.read(A["gPointerFovOnce"], 1)[0], e.u32(A["gPointerFovFactor"])]
           + [e.u32(A["gPointedRidePosition"] + 4 * i) for i in range(3)]]
    return [[canon(w) for w in part] for part in out], e.cl.log


# ------------------------------------------------------------------ the translation (host side)


def host(lib, case):
    lib.tpw_pp_unitwidth.restype, lib.tpw_pp_unitwidth.argtypes = C.c_int32, [C.c_int32]
    lib.tpw_pp_unitwidth(case.get("units", 0x800))
    CL.host_begin(lib, {}, {"FindPointedRide": case["rides"], "RidePos": case["ridepos"], "Vec": case["vec"], "Dist": case["dist"]})
    lib.tpw_pp_labels()

    def arr(ws):
        return (C.c_uint32 * (len(ws) + 1))(*[v & MASK for v in ws])

    lib.tpw_pp_table.argtypes = [C.c_void_p, C.c_int32]
    lib.tpw_pp_table(arr(case["table"]), len(case["table"]))
    lib.tpw_pp_field.argtypes = [C.c_int32, C.c_uint32, C.c_void_p, C.c_uint32, C.c_uint32, C.c_void_p, C.c_void_p]
    lib.tpw_pp_field(case["mode"], case["flags"] & MASK, arr(case["field"]), case["w"], case["d"], arr(case["heights"]), arr(case["quads"]))
    g = [case["mask"], case["scale"], case["fov"], case["cx"], case["cy"], case["px"], case["py"], case["once"], case["factor"]]
    g += case["cam"] + case["ridepos0"] + [case["ride0"]]
    io = arr(case["in_pos"] + case["in_pick"] + [case["in_res"]] + [0] * 6)
    lib.tpw_pp_run.argtypes = [C.c_void_p, C.c_int32, C.c_void_p]
    lib.tpw_pp_run(arr(g), case["nulls"], io)
    lib.tpw_pp_field(0, 0, arr([0] * 6), 0, 0, arr([0]), arr([0]))
    v = list(io)
    out = [v[0:3], v[3:6], [v[6]], v[7:13]]
    return [[canon(w) for w in part] for part in out], CL.host_end(lib)


# ------------------------------------------------------------------ the widescreen pick (unit v5L51)

WIDE_UNITS = 2730   # the unit space of a 16:9 screen (3840 x 2160, or 2730 x 1536)


def wide_scale(case):
    """UI_PickAcrossScale (include/UI_Base.h): 0.75 times the view centre x, over the view centre y, each step a single."""
    cx, cy = (struct.unpack(">f", struct.pack(">I", case[k]))[0] for k in ("cx", "cy"))
    return f32(f32(0.75 * cx) / cy)


def widen(log, case):
    """The log with the x of each point sent through the camera matrix scaled as the widescreen pick scales it."""
    k = wide_scale(case)
    out = []
    for name, ints, texts in log:
        if name == "VectorThroughLMatrixNoWNoTrans":
            x = struct.unpack(">f", struct.pack(">I", ints[1] & MASK))[0]
            ints = (ints[0], fb(f32(x * k))) + tuple(ints[2:])
        out.append((name, ints, texts))
    return out


def wide_case(r, **kw):
    """A case on a 16:9 view: the view centre y as make draws it, x 16/9 of it, the pointer anywhere across."""
    cy = f32(r.uniform(200, 800))
    cx = f32(cy * 16.0 / 9.0)
    kw.setdefault("px", fb(f32(r.uniform(0, 2 * cx))))
    return make(r, cx=fb(cx), cy=fb(cy), units=WIDE_UNITS, **kw)


def wide_edges():
    import random
    r = random.Random(5151)
    # the pointer at the left edge, the middle and the right edge of a 3840 x 2160 view
    for px in (0.0, 1920.0, 3839.0):
        yield make(r, cx=fb(1920.0), cy=fb(1080.0), px=fb(px), py=fb(1080.0), fov=fb(f32(1.5707964)), factor=fb(f32(0.885)),
                   once=1, mode=2, flags=2, nulls=0, units=WIDE_UNITS)
    for _ in range(8):
        yield wide_case(r, mode=2, flags=2, nulls=0)


def ppc_wide(e, case):
    out, log = ppc(e, case)
    return out, widen(log, case)


def host_wide(lib, case):
    out, log = host(lib, case)
    if lib.tpw_pp_unitwidth(0x800) == 0:
        log = widen(log, case)
    return out, log


TESTS = [
    FnTest("CalculatePointerWorldPosn", ppc, host, edge=edges, rnd=rnd),
    FnTest("the widescreen pick", ppc_wide, host_wide, edge=wide_edges, rnd=wide_case),
]
