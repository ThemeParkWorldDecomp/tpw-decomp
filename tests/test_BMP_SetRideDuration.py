"""Unit v51036, the ride commands on the bump world, against libtpw_unit_BMP_SetRideDuration.dylib (tests/unit_BMP_SetRideDuration.cpp), main app
("SimTheme Park"), src/BMP_FixRide_BMP_CountTrackSections_BMP_SetRideDuration.cpp:
  BMP_SetRideDuration 0x10026b14, BMP_RideWorn 0x100263f4, BMP_GetCameraMesh 0x1002cf30, BMP_StartBump 0x1002eaac,
  BMP_AddBuoyToRide 0x1002ebd0, BMP_StartBuoy 0x1002ec84, __sinit_HoardingsBalance_cpp 0x10021074

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. TRACE (text and
the duration the original leaves in r4 for the %d text), CBalanceTable::Init and __register_global_object are fakes on both sides that
record their arguments.

The world of a case: the list at 0x102b56c0 (magic, ride records, bumps, buoy table, counts), 256 ride records of 0xd0 bytes, 256 bumps
of 0xac, 1024 buoys of 0x1c, a ride record with a buoy chain, a camera mesh; the filler words are random. What is compared: the answer
(pointers as an index into their table), the log, the list's words and every block read back word by word, Ptr32 fields as tokens.

No fused operations. Not tested: a ride id of 0 on a dead world (the original stores through NULL); the return values of the void
functions (BMP_SetRideDuration, BMP_RideWorn leave what TRACE leaves); the vtable words the sinit stores (the host builds its objects
in the static initialisation).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BMP_SetRideDuration"
MASK = 0xFFFFFFFF
KM = 0x4A454647

F_DUR, F_WORN, F_MESH, F_BUMP, F_ADDBUOY, F_BUOY, F_SINIT = 0x10026B14, 0x100263F4, 0x1002CF30, 0x1002EAAC, 0x1002EBD0, 0x1002EC84, 0x10021074
SIZES = {F_DUR: 164, F_WORN: 168, F_MESH: 156, F_BUMP: 252, F_ADDBUOY: 124, F_BUOY: 200, F_SINIT: 136}
OWN = set(SIZES)

TRACE_ADDR, INIT_ADDR = 0x1006FCFC, 0x1001EA28

ADDR = dict(world=0x102B56C0, t434C=0x102B434C, t4130=0x102B4130, dtors=0x10265FA0, node434C=0x102B455C, node4130=0x102B4340)
HOST_LAYOUT = [0xD0, 0xAC]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_trace(e):
    text = e.cstr(e.r(3))
    arg = T.s32(e.r(4)) if "%d" in text else 0
    e.cl.rec("TRACE", (arg,), (text,))


def stub_init(e):
    e.cl.rec("Init", (e.cl.lab(e.r(3)),))


def stub_register(e):
    L = e.cl
    L.rec("register", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))


STUBS.internal(TRACE_ADDR, stub_trace)
STUBS.internal(INIT_ADDR, stub_init)
STUBS.imp("__register_global_object", stub_register)


def setup(runner):
    T.install_std_stubs(runner.emu)


def blk(r, n):
    return list(struct.unpack(">%dI" % n, r.getrandbits(32 * n).to_bytes(4 * n, "big")))


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def token(self, w):
        """A Ptr32 word as a name that is the same on both sides (None: not one of the world's blocks)."""
        for name, base, size, stride in self.regions:
            d = w - self.off[name]
            if 0 <= d < size and d % stride == 0:
                return (name, d // stride)
        return None

    def fix(self, words, positions):
        out = list(words)
        for p in positions:
            if out[p]:
                t = self.token(out[p])
                if t is not None:
                    out[p] = t
        return out

    def rel(self, ans, name, stride):
        if ans == 0:
            return None
        d = ans - self.addrs[name]
        return d // stride if 0 <= d and d % stride == 0 else ("bad", d)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["t434C"]: 1, ADDR["t4130"]: 2, ADDR["dtors"]: 3, ADDR["node434C"]: 4, ADDR["node4130"]: 5},
                                  case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def toword(self, a):
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        with_args = [a & MASK for a in args]
        return T.call_ppc(self.e, fn, with_args)[0] & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("t434C"), 2: self.g("t4130"), 3: self.g("dtors"), 4: self.g("node434C"), 5: self.g("node4130")},
                      case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bmpsd_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_bmpsd_alloc(n)

    def toword(self, a):
        return self.lib.tpw_bmpsd_off(a)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        L = self.lib
        a = [T.s32(x) for x in args]
        if fn == F_DUR:
            return L.tpw_bmpsd_setdur(a[0], a[1]) & MASK
        if fn == F_WORN:
            L.tpw_bmpsd_worn(a[0])
            return 0
        if fn == F_MESH:
            return L.tpw_bmpsd_mesh(a[0])
        if fn == F_BUMP:
            return L.tpw_bmpsd_bump(a[0], a[1], a[2])
        if fn == F_BUOY:
            return L.tpw_bmpsd_buoy(a[0], a[1])
        if fn == F_ADDBUOY:
            return L.tpw_bmpsd_addbuoy(args[0], a[1], a[2])
        L.tpw_bmpsd_sinit()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_bmpsd_"
    f = getattr(lib, p + "addr")
    f.restype, f.argtypes = C.c_uint64, [C.c_char_p]
    f = getattr(lib, p + "alloc")
    f.restype, f.argtypes = C.c_uint64, [C.c_uint32]
    f = getattr(lib, p + "off")
    f.restype, f.argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    i32 = C.c_int32
    for name, res, args in (("setdur", C.c_int32, [i32, i32]), ("worn", None, [i32]), ("mesh", C.c_uint32, [i32]),
                            ("bump", C.c_uint64, [i32, i32, i32]), ("buoy", C.c_uint64, [i32, i32]),
                            ("addbuoy", C.c_uint64, [C.c_uint64, i32, i32]), ("sinit", None, [])):
        f = getattr(lib, p + name)
        f.restype, f.argtypes = res, args
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

def build(s, case):
    r = random.Random(case["seed"])
    rides, bumps, buoys, ride, mesh = s.alloc(256 * 0xD0), s.alloc(256 * 0xAC), s.alloc(1024 * 0x1C), s.alloc(0xD0), s.alloc(0x20)
    s.addrs = dict(rides=rides, bumps=bumps, buoys=buoys, ride=ride, mesh=mesh)
    s.off = {k: s.toword(v) for k, v in s.addrs.items()}
    s.regions = [("rides", rides, 256 * 0xD0, 0xD0), ("bumps", bumps, 256 * 0xAC, 0xAC), ("buoys", buoys, 1024 * 0x1C, 0x1C), ("ride", ride, 1, 1), ("mesh", mesh, 1, 1)]

    # ride records
    rw = blk(r, 256 * 52)
    for k in range(256):
        rw[k * 52 + 22] |= 1      # filler in a pointer field is odd: never a block address (the blocks are aligned), so never a token
    rw[case["idx"] * 52] = case["kind"] & MASK
    rw[case["idx"] * 52 + 22] = 0 if case["mesh_null"] else s.off["mesh"]
    s.words(rides, rw)
    mw = blk(r, 8)
    mw[0] = (mw[0] | 0x400000) if case["mesh_flag"] else (mw[0] & ~0x400000)
    s.words(mesh, mw)

    # bumps
    p = case["p"] if case["op"] == "bump" else 256
    bw = blk(r, 256 * 43)
    for k in range(min(p + 1, 256)):
        bw[k * 43] = (bw[k * 43] | 1) if k < p else (bw[k * 43] & ~1)
    s.words(bumps, bw)

    # buoys and the ride's chain
    p = case["p"] if case["op"] in ("buoy", "addbuoy") else 1024
    uw = blk(r, 1024 * 7)
    for k in range(min(p + 1, 1024)):
        uw[k * 7] = (uw[k * 7] | 1) if k < p else (uw[k * 7] & ~1)
    for k in range(1024):
        uw[k * 7 + 4] |= 1
        uw[k * 7 + 5] |= 1
    rr = blk(r, 52)
    rr[42] |= 1
    chain = r.sample(range(p), min(case["n"], p)) if case["op"] == "addbuoy" else []
    for i, k in enumerate(chain):
        uw[k * 7 + 5] = s.off["buoys"] + chain[i + 1] * 0x1C if i + 1 < len(chain) else 0
    rr[42] = s.off["buoys"] + chain[0] * 0x1C if chain else 0
    rr[43] = len(chain) + case["rcount"]
    s.words(buoys, uw)
    s.words(ride, rr)

    ww = [0] * 16
    ww[0] = case["magic"]
    ww[2] = s.off["rides"] if False else s.toword(rides)
    ww[4] = 0 if case["bumps_null"] else s.toword(bumps)
    ww[5] = case["bumpcount"]
    ww[8] = 0 if case["buoys_null"] else s.toword(buoys)
    ww[9] = case["buoycount"]
    s.words(s.g("world"), ww)
    s.addrs["rides"] = rides
    s.off["rides"] = s.toword(rides)


def execute(s, case):
    op = case["op"]
    if op == "dur":
        s.call(F_DUR, case["args"])
        return None
    if op == "worn":
        s.call(F_WORN, case["args"])
        return None
    if op == "mesh":
        return s.call(F_MESH, case["args"])
    if op == "bump":
        return s.rel(s.call(F_BUMP, case["args"]), "bumps", 0xAC)
    if op == "buoy":
        return s.rel(s.call(F_BUOY, case["args"]), "buoys", 0x1C)
    if op == "addbuoy":
        return s.rel(s.call(F_ADDBUOY, [s.addrs["ride"]] + case["args"]), "buoys", 0x1C)
    s.call(F_SINIT, [])
    return None


def observe(s, case, answer):
    A = s.addrs
    world = s.fix(s.rd_words(s.g("world"), 16), [2, 4, 8])
    rides = s.fix(s.rd_words(A["rides"], 256 * 52), [i * 52 + 22 for i in range(256)])
    bumps = s.rd_words(A["bumps"], 256 * 43)
    buoys = s.fix(s.rd_words(A["buoys"], 1024 * 7), [k * 7 + j for k in range(1024) for j in (4, 5)])
    ride = s.fix(s.rd_words(A["ride"], 52), [42])
    return {"answer": answer, "log": s.log(), "world": world, "rides": rides, "bumps": bumps, "buoys": buoys, "ride": ride,
            "mesh": s.rd_words(A["mesh"], 8)}


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


# ---------------------------------------------------------------------------------------------------------------- cases

KINDS = [0, 1, -1, 5, 0x7FFFFF, 0x800000, 0x1000000, -0x800000]


def rint(r):
    return r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, r.randrange(-1000, 1000), r.getrandbits(32) - 0x80000000])


def case_for(r, op, **o):
    c = dict(op=op, seed=r.getrandbits(32), queues={}, idx=r.choice([0, 1, 255, r.randrange(256)]), kind=r.choice(KINDS + [r.getrandbits(32)]),
             mesh_null=r.random() < 0.2, mesh_flag=r.random() < 0.6, p=r.choice([0, 1, 2, r.randrange(256), 255, 256, 1022, 1023, 1024]),
             n=r.randrange(0, 5), rcount=r.randrange(0, 4), bumpcount=r.randrange(0, 300), buoycount=r.randrange(0, 1100),
             bumps_null=r.random() < 0.1, buoys_null=r.random() < 0.1, match=r.random() < 0.6,
             magic=KM if r.random() < 0.85 else r.choice([0, KM + 1, KM - 1, KM ^ 0x10000, r.getrandbits(32)]))
    c.update(o)
    if op in ("dur", "worn", "mesh"):
        ident = ((c["kind"] & MASK) << 8 | c["idx"]) & MASK
        arg = ident if c["match"] else ident ^ (1 << r.randrange(8, 32))
        if c["magic"] != KM and arg == 0:
            arg = 1
        c["args"] = [arg] + ([rint(r) & MASK] if op == "dur" else [])
    elif op == "bump":
        c["args"] = [rint(r) & MASK, rint(r) & MASK, rint(r) & MASK]
    elif op == "buoy":
        c["args"] = [rint(r) & MASK, rint(r) & MASK]
    elif op == "addbuoy":
        c["args"] = [rint(r) & MASK, rint(r) & MASK]
    else:
        c["args"] = []
    return c


def edges_for(op, seed):
    def gen():
        r = random.Random(seed)
        if op in ("dur", "worn", "mesh"):
            for magic in (KM, 0, KM + 1):
                for match in (True, False):
                    for idx in (0, 255):
                        for mn in (True, False):
                            for mf in (True, False):
                                yield case_for(r, op, magic=magic, match=match, idx=idx, mesh_null=mn, mesh_flag=mf)
        elif op == "bump":
            for p in (0, 1, 255, 256):
                for bn in (False, True):
                    yield case_for(r, op, p=p, bumps_null=bn, magic=KM)
        elif op in ("buoy", "addbuoy"):
            for p in (0, 1, 2, 1023, 1024):
                for bn in (False, True):
                    for n in (0, 1, 3):
                        yield case_for(r, op, p=p, buoys_null=bn, n=n)
        else:
            yield case_for(r, op)
    return gen


def rnd_for(op):
    return lambda r: case_for(r, op)


def test(op, name):
    return FnTest(name, ppc, host, edge=edges_for(op, sum(map(ord, op)) * 31), rnd=rnd_for(op))


TESTS = [
    test("dur", "BMP_SetRideDuration"),
    test("worn", "BMP_RideWorn"),
    test("sinit", "__sinit_HoardingsBalance_cpp"),
    test("mesh", "BMP_GetCameraMesh"),
    test("bump", "BMP_StartBump"),
    test("addbuoy", "BMP_AddBuoyToRide"),
    test("buoy", "BMP_StartBuoy"),
]
