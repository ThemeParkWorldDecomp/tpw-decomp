"""Unit v5062, the advisor's bump (ride launch) functions, against libtpw_unit_SetupHeadIDs.dylib (tests/unit_SetupHeadIDs.cpp), main app
("SimTheme Park"), src/SetupHeadIDs.cpp:
  SetupHeadIDs 0x1002cddc, BMP_AddJoiningListToBump 0x1002cc70, BMP_LAUNCH 0x1002cff4, BMP_Collision 0x1002c7ec

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (FindDataNodeIndexByID, EnableRideHead, PAR_InitEmitter, SetControlType, PointInColObject, BMP_StartBump, GetRideInfo, InstanceRide,
SetCurRideAnim) are fakes on both sides that record their arguments (pointers into the world block as offsets) and answer from the case's
queues or the world; labs, sqrt and atan2 are the C library on both sides.

The world of a case: the list global (0x102b56c0: magic, ride records, bumps, collision objects), 4 ride records (0xd0 bytes), 6 bumps (0xac
bytes), 4 collision objects, 4 head nodes (a chain), 3 ride anim sets with their mesh, header and head node (the host's own layout for the
anim set and mesh, the file layout for the header), the ride info (sub ride types), gRideAnimSets, the sine table. What is compared: the
answer, the event log, and every word of the ride records, bumps, collision objects and head nodes (addresses as offsets in the block).

Fused operations: BMP_Collision's two fnmsub (fused on the host with fma); the fctiwz of SetupHeadIDs and BMP_Collision run under the
HardwareRounding hook. Not tested: a list that is not live with the id 0 (the original reads through a null ride), a ride index past the
4 records, a bump count above the live bumps (the original reads past the array), a ride size 0 (divw by 0), floats past 2^31 / 1024 and
a NaN in the head node (fctiwz saturation differs in Unicorn).
"""
import math
import random
import struct
import ctypes as C

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_SetupHeadIDs"
MASK = 0xFFFFFFFF

FN_SETUP, FN_ADD, FN_LAUNCH, FN_COLL = 0x1002CDDC, 0x1002CC70, 0x1002CFF4, 0x1002C7EC
SIZES = {FN_SETUP: 296, FN_ADD: 316, FN_LAUNCH: 808, FN_COLL: 1112}
OWN = set(SIZES)

CALLEES = dict(find=0x101FFDC8, enable=0x10200230, par=0x100BEC30, startbump=0x1002EAAC, point=0x1002BC34, rideinfo=0x10154868,
               instance=0x10069B24, setanim=0x100C6670, setctrl=0x1002D404)

ADDR = dict(list=0x102B56C0, sets=0x102C79A4, sine=0x102B4EC0)
RDB = 0x1037F6C8
JUMP_TABLE = 0x1029CB70

# the world block (offsets)
WSIZE = 0x4000
OFF = {"R": (0x000, 0xD0), "B": (0x400, 0xAC), "C": (0x840, 0x20), "N": (0x900, 0x14)}
INFO, SETS, MESHES, HDRS, FNODES = 0x3000, 0x1400, 0x2000, 0x2400, 0x2800


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


CUR = {"W": 0, "start": 0, "info": 0}


def wl(p):
    p &= MASK
    return p - CUR["W"] if CUR["W"] <= p < CUR["W"] + WSIZE else p


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_find(e):
    L = e.cl
    L.rec("Find", (wl(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, L.ret("Find") & MASK)


def st_enable(e):
    e.cl.rec("Enable", (wl(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def st_par(e):
    e.cl.rec("Emitter", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


def st_setctrl(e):
    e.cl.rec("SetControlType", (wl(e.r(3)), wl(e.r(4))))


def st_point(e):
    L = e.cl
    L.rec("Point", (s32(e.r(3)), s32(e.r(4)), wl(e.r(5))))
    e.set_r(3, L.ret("Point") & MASK)


def st_startbump(e):
    e.cl.rec("StartBump", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, CUR["start"] & MASK)


def st_rideinfo(e):
    e.cl.rec("GetRideInfo", (1 if e.r(3) == RDB else 0, (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, CUR["info"] & MASK)


def st_instance(e):
    L = e.cl
    sp = e.r(1)
    L.rec("InstanceRide", (s32(e.r(3)), s32(e.r(4)), wl(e.r(5)), s32(e.r(6)), s32(e.r(7)), wl(e.r(8)), s32(e.r(9)), s32(e.r(10)),
                           s32(e.u32(sp + 0x38)), s32(e.u32(sp + 0x3C))))
    e.set_r(3, L.ret("Instance") & MASK)


def st_setanim(e):
    e.cl.rec("SetCurRideAnim", (wl(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), fb(e.f(1)), s32(e.r(8))))


for key, fn in (("find", st_find), ("enable", st_enable), ("par", st_par), ("startbump", st_startbump), ("point", st_point),
                ("rideinfo", st_rideinfo), ("instance", st_instance), ("setanim", st_setanim), ("setctrl", st_setctrl)):
    STUBS.internal(CALLEES[key], fn)


def imp_labs(e):
    e.set_r(3, abs(s32(e.r(3))) & MASK)


def imp_atan2(e):
    e.set_f1(math.atan2(e.f(1), e.f(2)))


def imp_sqrt(e):
    x = e.f(1)
    e.set_f1(math.sqrt(x) if x >= 0 else float("nan"))


STUBS.imp("labs", imp_labs)
STUBS.imp("atan2", imp_atan2)
STUBS.imp("sqrt", imp_sqrt)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    PW = 4
    base = 0

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        self.off = {"heads": 8, "header": 4, "sub": 0x7B8}

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

    def setg(self, name, v):
        CUR[name] = v

    def ptr(self, a, v):
        self.words(a, [v])

    def call(self, fn, args):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.PW = lib.layout[7]
        self.off = {"heads": lib.layout[0], "header": lib.layout[1], "sub": lib.layout[2]}
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    @property
    def base(self):
        return self.lib.tpw_sh_base()      # set by the first allocation: read it after

    def g(self, name):
        return self.lib.tpw_sh_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sh_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setg(self, name, v):
        self.write(self.g(name), struct.pack("<Q", v))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def call(self, fn, args):
        lib = self.lib
        if fn == FN_SETUP:
            lib.tpw_sh_SetupHeadIDs(args[0], s32(args[1]))
            return 0
        if fn == FN_ADD:
            return lib.tpw_sh_BMP_AddJoiningListToBump(s32(args[0])) & MASK
        if fn == FN_LAUNCH:
            return lib.tpw_sh_BMP_LAUNCH(s32(args[0]), s32(args[1])) & MASK
        lib.tpw_sh_BMP_Collision(args[0])
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sh_addr.restype, lib.tpw_sh_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sh_base.restype, lib.tpw_sh_base.argtypes = C.c_uint64, []
    lib.tpw_sh_alloc.restype,lib.tpw_sh_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sh_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sh_SetupHeadIDs.restype, lib.tpw_sh_SetupHeadIDs.argtypes = None, [C.c_uint64, C.c_int32]
    lib.tpw_sh_BMP_AddJoiningListToBump.restype, lib.tpw_sh_BMP_AddJoiningListToBump.argtypes = C.c_uint32, [C.c_int32]
    lib.tpw_sh_BMP_LAUNCH.restype, lib.tpw_sh_BMP_LAUNCH.argtypes = C.c_uint32, [C.c_int32, C.c_int32]
    lib.tpw_sh_BMP_Collision.restype, lib.tpw_sh_BMP_Collision.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sh_layout(out)
    lay = list(out)[:n]
    assert n == 8 and lay[2] + 0x20 <= 0xF00 and lay[6] == 0x20 and lay[3] <= 0x400 and lay[4] <= 0x100 and lay[5] <= 0x100 and lay[7] in (4, 8), lay
    lib.layout = lay
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    W = s.alloc(WSIZE)
    s.W = W
    Wr = s.Wr = W - s.base      # the words that hold addresses hold offsets from the arena base (0 on the Mac)
    s.setg("W", W)
    CUR["W"] = W

    def val(v):
        if isinstance(v, tuple):
            base, stride = OFF[v[0]]
            return Wr + base + stride * v[1]
        return v & MASK

    for kind, key in (("R", "rides"), ("B", "bumps"), ("C", "cols"), ("N", "nodes")):
        base, stride = OFF[kind]
        for k, d in enumerate(case[key]):
            for off, v in d.items():
                s.words(W + base + stride * k + off, [val(v)])
    s.words(s.g("list"), [case["magic"], 0, Wr + OFF["R"][0], 0, Wr + OFF["B"][0], case["count"], Wr + OFF["C"][0], case["ncols"]])
    s.setg("start", W + val(case["start"]) - Wr if case["start"] is not None else 0)
    s.setg("info", W + INFO)
    s.words(W + INFO + s.off["sub"], case["subs"])
    for k in range(3):
        st, me, hd, fn = W + SETS + 0x400 * k, W + MESHES + 0x100 * k, W + HDRS + 0x100 * k, W + FNODES + 0x100 * k
        s.ptr(st + s.off["heads"], me)
        s.ptr(me + s.off["header"], hd)
        s.words(hd + 0x78, [fn - s.base])
        s.words(fn + 0x40, case["fl"][k])
    for i in range(4):
        s.ptr(s.g("sets") + i * s.PW, W + SETS + 0x400 * (i % 3))
    s.words(s.g("sine"), case["sine"])


def nm(w, W):
    """W is the block's offset in the words (an address on the Mac, an arena offset on the host)."""
    return -(1 << 40) - (w - W) if W <= w < W + WSIZE else w


def execute(s, case):
    W = s.W
    op = case["op"]
    if op == "setup":
        s.call(FN_SETUP, [W + OFF["B"][0] + OFF["B"][1] * case["arg"], case["flag"]])
        return 0      # void
    if op == "add":
        return s.call(FN_ADD, [case["arg"]])
    if op == "launch":
        return s.call(FN_LAUNCH, [case["arg"], case["flag"]])
    s.call(FN_COLL, [W + OFF["B"][0] + OFF["B"][1] * case["arg"]])
    return 0      # void


def observe(s, case, answer):
    W, Wr = s.W, s.Wr
    out ={"answer": nm(answer, Wr), "log": s.log()}
    ptrs = {"R": (0x58, 0xC4), "B": (0x30, 0x98, 0x9C), "C": (), "N": (0x10,)}
    for kind, name, n in (("R", "rides", 4 * 0xD0), ("B", "bumps", 6 * 0xAC), ("C", "cols", 4 * 0x20), ("N", "nodes", 4 * 0x14)):
        stride = OFF[kind][1]
        out[name] = [nm(w, Wr) if (4 * i) % stride in ptrs[kind] else w for i, w in enumerate(s.rd_words(W + OFF[kind][0], n // 4))]
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

KINDS = [-5, -8, -12, -13, -14, -3, -4, -7, -10, -1, 0, 1, -16, -2, -6, -9, -11, -15]
FLOATS = [0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 1.0 / 1024, -1.0 / 1024, 1.5 / 1024, 0.9999, 123.456, -987.654, 2000.0, -2000.0, 3.0 / 2048]


def rfl(r):
    k = r.random()
    if k < 0.3:
        return fb(r.choice(FLOATS))
    if k < 0.8:
        return fb(r.uniform(-3, 3))
    return fb(r.uniform(-1000, 1000))


def mk_ride(r):
    d = {0: r.choice(KINDS), 4: r.getrandbits(32), 8: r.randrange(0, 20), 0x10: r.getrandbits(32), 0x14: r.randrange(0, 4),
         0x18: r.randrange(1, 5), 0x2C: r.choice([0, 256, 1024, 1024, 1500, 2047, r.randrange(0, 2200)]),
         0x50: r.choice([0, 1, 1, 1]), 0x58: r.choice([0, ("B", r.randrange(6))]), 0x60: r.getrandbits(32),
         0x74: r.randrange(-1000, 1000), 0x78: r.randrange(-1000, 1000), 0xC4: r.choice([0, 0, ("N", r.randrange(4))])}
    if r.random() < 0.7:
        d[0x5C], d[0x64] = r.randrange(0, 4), r.randrange(5, 9)
    else:
        d[0x5C] = r.choice([0, 1, 2, 3, 0x3F, 0x40, 0x41, r.randrange(0, 70)])
        d[0x64] = r.choice([0, 1, 2, 5, 8, 0x40, 0x41, r.randrange(0, 80)])
    return d


def mk_bump(r, k):
    flags = 1 if r.random() < 0.88 else 0
    for bit in (0x4000, 0x80000, 0x100000, 0x200000, 0x200, 0x400, 0x8000):
        if r.random() < (0.12 if bit == 0x200 else 0.25):
            flags |= bit
    wide = r.random() < 0.08
    big = lambda: r.choice([r.randrange(-(1 << 30), 1 << 30), -(1 << 31), (1 << 31) - 1, r.randrange(-70000, 70000)])
    d = {0: flags, 4: r.getrandbits(32), 8: r.randrange(0, 4), 0x10: r.getrandbits(32), 0x14: r.randrange(0, 8),
         0x30: r.choice([0, 0, ("N", r.randrange(4))]), 0x34: big() if wide else r.randrange(-120, 120),
         0x38: big() if wide else r.randrange(-120, 120), 0x3C: r.randrange(-500, 500), 0x40: r.randrange(-500, 500),
         0x44: r.choice([r.randrange(-200, 200), r.randrange(-3000, 3000), r.randrange(-20000, 20000), big()]),
         0x48: r.choice([r.randrange(-200, 200), r.randrange(-3000, 3000), r.randrange(-20000, 20000), big()]),
         0x64: (r.choice([0x7FFFFFFF, big() & 0x7FFFFFFF]) if wide else r.randrange(20, 130)),
         0x7C: r.getrandbits(32), 0x80: r.getrandbits(32), 0x88: r.getrandbits(32),
         0x98: ("C", r.randrange(4)), 0x9C: ("R", r.randrange(4))}
    return d


def mk_col(r):
    f = 0
    for bit, pr in ((1, 0.85), (0x40, 0.25), (0x80, 0.2), (0x20, 0.3)):
        if r.random() < pr:
            f |= bit
    return {0: f, 4: r.randrange(-100, 100), 8: r.randrange(-100, 100), 0xC: r.randrange(1, 90), 0x10: r.randrange(1, 90)}


def mk_node(r, k):
    return {0: r.getrandbits(32), 4: r.getrandbits(32), 8: r.getrandbits(32), 0xC: r.getrandbits(32),
            0x10: r.choice([0, ("N", k + 1)]) if k < 3 else 0}


def world(r, op):
    rides = [mk_ride(r) for _ in range(4)]
    bumps = [mk_bump(r, k) for k in range(6)]
    live = sum(1 for b in bumps if b[0] & 1)
    cols = [mk_col(r) for _ in range(4)]
    livec = sum(1 for c in cols if c[0] & 1)
    if r.random() < 0.5:
        sine = [int(256 * math.sin(math.pi * 2 * i / 512)) for i in range(512)]
    else:
        sine = [r.randrange(-256, 257) for _ in range(512)]
    return {"op": op, "magic": 0x4A454647 if r.random() < 0.95 else r.getrandbits(32), "rides": rides, "bumps": bumps,
            "cols": cols, "nodes": [mk_node(r, k) for k in range(4)],
            "count": r.randrange(0, live + 1) if r.random() < 0.3 else live, "ncols": r.randrange(0, livec + 1),
            "start": ("B", r.randrange(6)) if r.random() < 0.9 else None, "subs": [r.getrandbits(32) for _ in range(8)],
            "fl": [[rfl(r) for _ in range(3)] for _ in range(3)], "sine": sine, "arg": 0, "flag": 0,
            "queues": {"Find": [r.choice([MASK, MASK, 0, 1, 2, 3, 5, r.getrandbits(32)]) for _ in range(4)],
                       "Point": [r.choice([0, 0, 1]) for _ in range(4)],
                       "Instance": [r.choice([0, 1, 2, 3, 3]) for _ in range(3)]}}


def ride_arg(c, k, mismatch=0):
    return (((c["rides"][k][0] << 8) | k) + mismatch) & MASK


def setup_case(r, mod=None):
    c = world(r, "setup")
    c["arg"] = r.randrange(6)
    c["flag"] = r.choice([0, 1, 1, 5, -1])
    b = c["bumps"][c["arg"]]
    b[0x30] = r.choice([("N", 0), ("N", 0), ("N", 1), ("N", 2), 0])
    b[8] = r.randrange(0, 4)
    if mod:
        mod(c)
    return c


def add_case(r, mod=None):
    c = world(r, "add")
    k = r.randrange(4)
    c["rides"][k][0x50] = r.choice([1, 1, 1, 0])
    for b in c["bumps"]:
        if r.random() < 0.5:
            b[0x9C] = ("R", k)
    for b in c["bumps"]:
        if b[0] & 1 and r.random() < 0.5:
            b[0x30] = 0
    c["arg"] = ride_arg(c, k, r.choice([0, 0, 0, 0, 0x100, 1, -1, 0x10000]))
    if c["magic"] != 0x4A454647 and c["arg"] == 0:
        c["arg"] = 1
    if mod:
        mod(c)
    return c


def launch_case(r, mod=None):
    c = world(r, "launch")
    k = r.randrange(4)
    c["arg"] = ride_arg(c, k, r.choice([0, 0, 0, 0, 0x100, 1, -1]))
    if c["magic"] != 0x4A454647 and c["arg"] == 0:
        c["arg"] = 1
    c["flag"] = r.choice([0, 0, 0, 0x100, 0x20, r.getrandbits(32)])
    if mod:
        mod(c)
    return c


def coll_case(r, mod=None):
    c = world(r, "coll")
    c["arg"] = r.randrange(6)
    if r.random() < 0.7:
        for b in c["bumps"]:
            b[0] &= ~0x200
            b[0x44] = r.randrange(-250, 250)
            b[0x48] = r.randrange(-250, 250)
    if r.random() < 0.5:
        for b in c["bumps"]:
            b[0] &= ~0x200000
    if mod:
        mod(c)
    return c


def edges(gen, seed, mods):
    r = random.Random(seed)
    for m in mods:
        for _ in range(3):
            yield gen(r, m)


def md(**kw):
    """A modifier: sets ride fields {k: value} etc. kw: ride0={...}, bump0={...}, top={...}"""
    def f(c):
        for key, d in kw.items():
            if key.startswith("ride"):
                c["rides"][int(key[4:])].update(d)
            elif key.startswith("bump"):
                c["bumps"][int(key[4:])].update(d)
            elif key.startswith("col"):
                c["cols"][int(key[3:])].update(d)
            elif key.startswith("node"):
                c["nodes"][int(key[4:])].update(d)
            elif key == "top":
                c.update(d)
            elif key == "q":
                c["queues"].update(d)
    return f


def setup_edges():
    mods = [None,
            md(bump0={0x30: 0}, bump1={0x30: 0}, bump2={0x30: 0}, bump3={0x30: 0}, bump4={0x30: 0}, bump5={0x30: 0}),
            md(q={"Find": [MASK]}, top={"flag": 1}),
            md(q={"Find": [0]}, top={"flag": 1}),
            md(q={"Find": [1, MASK, 2, MASK]}, top={"flag": 0}),
            md(q={"Find": [3, 4, 5, 6]}, top={"flag": -1}),
            md(top={"fl": [[fb(0.0), fb(-0.0), fb(1.0 / 1024)]] * 3}),
            md(top={"fl": [[fb(-1.0 / 1024), fb(1.5 / 1024), fb(0.9999)]] * 3}),
            md(top={"fl": [[fb(1500.5), fb(-1500.5), fb(2000.0)]] * 3}),
            md(top={"fl": [[fb(1.0 / 1024 - 1e-7), fb(1.0 / 1024 + 1e-7), fb(-1.0 / 1024 + 1e-7)]] * 3})]
    r = random.Random(5)
    for m in mods:
        for k in range(2):
            def both(c, m=m, k=k):
                c["arg"] = [0, 3][k]
                c["bumps"][c["arg"]][0x30] = ("N", 0) if m is None or k == 0 else ("N", 2)
                if m:
                    m(c)
            yield setup_case(r, both)


def add_edges():
    r = random.Random(6)
    allb = {f"bump{i}": {0: 1, 0x9C: ("R", 0), 0x30: 0} for i in range(6)}
    def A(**kw):
        d = {k: dict(v) for k, v in allb.items()}
        for k, v in kw.items():
            if k in d:
                d[k].update(v)
            else:
                d[k] = v
        return md(**d)
    mods = [None,
            md(top={"magic": 0x12345678, "arg": 5}),
            md(top={"magic": 0x4A454647}, ride0={0x50: 0}),
            A(top={"count": 6}, ride0={0x50: 1, 0: 0}),
            A(top={"count": 6}, bump0={0x30: 5}, ride0={0x50: 1, 0: 0}),
            A(top={"count": 6}, bump0={0x30: 5}, bump1={0x30: 5}, ride0={0x50: 1, 0: 0}),
            A(top={"count": 0}, ride0={0x50: 1, 0: 0}),
            A(top={"count": 1}, bump0={0: 0}, ride0={0x50: 1, 0: 0}),
            A(top={"count": 2}, bump0={0: 0}, bump1={0x9C: ("R", 1)}, ride0={0x50: 1, 0: 0}),
            md(top={"arg": 0x100}, ride0={0: 1, 0x50: 1})]
    for m in mods:
        for k in range(3):
            def both(c, m=m, k=k):
                c["arg"] = ride_arg(c, 0, [0, 0x100, 1][k] if m is None else 0)
                if m:
                    m(c)
            yield add_case(r, both)


def launch_edges():
    r = random.Random(7)
    base = dict(ride0={0: 3, 0x5C: 1, 0x64: 5, 0x50: 1, 0x18: 3, 0x14: 2, 0x58: 0, 0xC4: ("N", 0)})

    def L(**kw):
        d = {k: dict(v) for k, v in base.items()}
        for k, v in kw.items():
            if k in d:
                d[k].update(v)
            else:
                d[k] = v
        return md(**d)
    mods = [L(),
            L(top={"magic": 0x1}),
            L(top={"arg": 0x301}),
            L(ride0={0x5C: 5}),
            L(ride0={0x5C: 6}),
            L(ride0={0x5C: 0x40, 0x64: 0x80}),
            L(ride0={0x5C: 0x3F, 0x64: 0x80}),
            L(ride0={0x5C: 0x3E, 0x64: 0x3F}),
            L(ride0={0x50: 0}),
            L(top={"start": None}),
            L(top={"flag": 0x40}),
            L(top={"flag": 0}, ride0={0x58: ("B", 2)}),
            L(top={"flag": 0, "ncols": 4}, col0={0: 1}, col1={0: 0x41}, col2={0: 0x80}, col3={0: 1}, q={"Point": [1]}),
            L(top={"flag": 0, "ncols": 4}, col0={0: 1}, col1={0: 0x41}, col2={0: 1}, col3={0: 1}, q={"Point": [0, 1, 1]}),
            L(top={"flag": 0, "ncols": 4}, col0={0: 1}, col1={0: 1}, col2={0: 1}, col3={0: 1}, q={"Point": [0]}),
            L(top={"flag": 0, "ncols": 2}, col0={0: 0}, col1={0: 0x40}, q={"Point": [1]}),
            L(top={"flag": 0, "ncols": 0}),
            L(top={"flag": 0}, q={"Instance": [0]}),
            L(top={"flag": 0}, q={"Instance": [2]}, node0={0x10: 0}),
            L(top={"flag": 0}, q={"Instance": [1], "Find": [MASK, 2, MASK]}),
            L(top={"flag": 0}, q={"Instance": [3], "Find": [4, 5, 6]}),
            L(top={"flag": 0}, ride0={0xC4: 0}),
            L(top={"flag": 0}, ride0={0x5C: 7, 0x64: 9, 0x18: 4, 0x14: 3}),
            L(top={"flag": 0}, ride0={0x5C: 2, 0x18: 1, 0x14: 0}),
            ]
    for m in mods:
        for k in range(2):
            def both(c, m=m, k=k):
                c["arg"] = None
                m(c)
                if c["arg"] is None:
                    c["arg"] = ride_arg(c, 0)
            yield launch_case(r, both)


def coll_edges():
    r = random.Random(8)
    near = lambda i, x, y, dx, dy, rad, kind_ride: {0: 1, 0x34: x, 0x38: y, 0x44: dx, 0x48: dy, 0x64: rad, 0x98: ("C", 0), 0x9C: ("R", kind_ride)}
    mods = []
    for kind in range(-16, 2):
        for dirs in ((-100, 50, 100, -50), (100, 50, -100, -50), (100, 0, 100, 0), (60, 80, -60, -80)):
            mods.append((kind, dirs))

    def make(kind, dirs):
        def f(c):
            c["arg"] = 0
            c["magic"] = 0x4A454647
            for i in range(6):
                c["bumps"][i][0] = 1 if i < 2 else 0
            c["count"] = 2
            c["cols"][0][0] = 1
            c["bumps"][0].update(near(0, 0, 0, dirs[0], dirs[1], 50, 0))
            c["bumps"][1].update(near(1, 30, 20, dirs[2], dirs[3], 50, 1))
            c["rides"][0][0] = kind
            c["rides"][1][0x2C] = 1024
            c["rides"][0][0x2C] = 1024
            c["bumps"][0][0x3C] = 400
            c["bumps"][0][0x40] = -300
        return f
    for kind, dirs in mods:
        yield coll_case(r, make(kind, dirs))
    # the exits
    ex = [md(bump0={0: 1, 0x98: ("C", 1)}, col1={0: 0x80}), md(bump0={0: 0x201}), md(bump1={0: 0x201}, top={"count": 6}),
          md(bump1={0x98: ("C", 2)}, col2={0: 0x80}, top={"count": 6}), md(top={"count": 0}),
          md(bump1={0x34: 1000, 0x38: 1000}, bump0={0x34: 0, 0x38: 0}, top={"count": 6}),
          md(bump1={0x34: 0, 0x38: 0}, bump0={0x34: 0, 0x38: 0}, top={"count": 6}),
          md(bump1={0x34: 100, 0x38: 0, 0x64: 0}, bump0={0x34: 0, 0x38: 0, 0x64: 0}, top={"count": 6}),
          md(bump1={0x34: 1 << 20, 0x38: 1 << 20, 0x64: 1 << 22}, bump0={0x34: 0, 0x38: 0, 0x64: 1 << 22}, top={"count": 6}),
          md(bump1={0x34: -(1 << 31), 0x38: 5, 0x64: 100}, bump0={0x34: 0, 0x38: 0, 0x64: 100}, top={"count": 6}),
          md(bump1={0x34: 0x8000, 0x38: 0x7FFF, 0x64: 0x10000}, bump0={0x34: 0, 0x38: 0, 0x64: 0x10000}, top={"count": 6}),
          md(bump1={0x34: 0x8001, 0x38: 0x7FFF, 0x64: 0x10000}, bump0={0x34: 0, 0x38: 0, 0x64: 0x10000}, top={"count": 6}),
          md(bump1={0x34: 0x7FFF, 0x38: 0x8000, 0x64: 0x10000}, bump0={0x34: 0, 0x38: 0, 0x64: 0x10000}, top={"count": 6}),
          md(bump1={0x34: 60, 0x38: 0, 0x64: 60}, bump0={0x34: 0, 0x38: 0, 0x64: 60}, top={"count": 6})]
    for m in ex:
        def both(c, m=m):
            c["arg"] = 0
            c["magic"] = 0x4A454647
            for i in range(6):
                c["bumps"][i][0] = 1
                c["bumps"][i][0x98] = ("C", 0)
            c["cols"][0][0] = 1
            c["cols"][1][0] = 1
            c["cols"][2][0] = 1
            m(c)
        yield coll_case(r, both)


TESTS = [
    FnTest("SetupHeadIDs", ppc, host, edge=setup_edges, rnd=lambda r: setup_case(r)),
    FnTest("BMP_AddJoiningListToBump", ppc, host, edge=add_edges, rnd=lambda r: add_case(r)),
    FnTest("BMP_LAUNCH", ppc, host, edge=launch_edges, rnd=lambda r: launch_case(r)),
    FnTest("BMP_Collision", ppc, host, edge=coll_edges, rnd=lambda r: coll_case(r)),
]
