"""Unit v5157, the track editor's new brace and the undoing of the preview brace, against libtpw_unit_NewTrackBrace.dylib
(tests/unit_NewTrackBrace.cpp), main app ("SimTheme Park"), src/NewTrackBrace.cpp:
  NewTrackBrace 0x10040304, UnLinkPreviewBrace 0x1003fedc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers, the host's own offsets from tpw_ntb_layout) for the
translation. The callees outside the unit (MReleaseAllocF, CorrectBraceNumbering, InstanceRide, RestoreRideInstance, SetNewAnim,
SetupBraceInfo, Assign3DObjectToAIWorld, InsertBraceIntoPylon, FlagCollisionUpdateTrackAroundBrace, RemoveBraceFromCollisionSystem,
UnLinkBrace, RestorePylonDetails, UpdatePylonStackMeshes) are fakes on both sides that record every argument; MReleaseAllocF hands out two
blocks of the world, the ride functions answer from the case's queues. GetTempFixedRideParams is inlined by the original: its two statics
(0x102b5bf8, 0x102b5c08) are read back on both sides.

The world of a case: the edit coaster and its list of braces and pylons, the pylon, the pylon control, three ride anim sets with their time
controls, the globals gLinkFoundPylon and gPylonStackSkip. What is compared: the answer, the call log, every named field of every object
(pointers as the object's name and the offset in it), the two statics.

No fused operations (the float work is fmuls and fadds). Not tested: a NULL control with a pylon of flag 0 (the original reads it), a
preview brace without a next in a case that has none to renumber is tested; a pylon count of 0 with gPylonStackSkip 0 (the count becomes
0xffffffff and the original walks four billion records), a pylon with more than 5 braces (the record holds 5).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_NewTrackBrace"
MASK = 0xFFFFFFFF

FN_NEW, FN_UNLINK = 0x10040304, 0x1003fedc
SIZES = {FN_NEW: 1532, FN_UNLINK: 1000}
OWN = set(SIZES)

MALLOC, CORRECT, RESTORE_RIDE, INSTANCE_RIDE = 0x10064BD4, 0x100564AC, 0x1006A140, 0x10069B24
SET_NEW_ANIM, SETUP_BRACE, ASSIGN, INSERT = 0x100C60FC, 0x1003DF2C, 0x1005F53C, 0x1003EF78
FLAG_COLL, REMOVE_COLL, UNLINK_BRACE, RESTORE_PYLON, UPDATE_STACK = 0x10040FA4, 0x10048FC4, 0x1003E5C0, 0x1003EC00, 0x1003D958

ADDR = dict(pair=0x102B5C08, params=0x102B5BF8, edit=0x102B68B0, found=0x102B6F00, skip=0x102B68A0, rides=0x102C79A4)

# (key, the Mac's offset or size, kind) in the order tpw_ntb_layout writes the host's.  u32, f float bits, p pointer (4 bytes Mac, 8 host)
ALL = [
    ("brace.num", 0x00, "u"), ("brace.flags", 0x08, "u"), ("brace.x4c", 0x4C, "f"), ("brace.l50", 0x50, "f"), ("brace.z54", 0x54, "f"),
    ("brace.prev", 0x64, "p"), ("brace.next", 0x68, "p"), ("brace.f6c", 0x6C, "f"), ("brace.w70", 0x70, "u"), ("brace.f74", 0x74, "p"),
    ("brace.cnt78", 0x78, "u"), ("brace.blk7c", 0x7C, "p"), ("brace.blk80", 0x80, "p"), ("brace.cnt84", 0x84, "u"), ("brace.blk88", 0x88, "p"),
    ("brace.blk8c", 0x8C, "p"), ("brace.f90", 0x90, "p"), ("brace.data", 0x100, "p"), ("brace.ff8", 0xF8, "f"), ("brace.fe8", 0xE8, "f"),
    ("inst.flags", 0x00, "u"), ("inst.blk3c", 0x3C, "u"), ("inst.f40", 0x40, "u"), ("inst.cnt44", 0x44, "u"), ("inst.f48", 0x48, "f"),
    ("inst.tail50", 0x50, "p"), ("inst.last54", 0x54, "p"), ("inst.braces58", 0x58, "p"), ("inst.b5c", 0x5C, "p"), ("inst.prev60", 0x60, "p"),
    ("inst.b64", 0x64, "p"), ("inst.pyl68", 0x68, "p"), ("inst.b6c", 0x6C, "p"), ("inst.f118", 0x118, "f"), ("inst.cobj", 0x28, "p"),
    ("pyl.num", 0x00, "u"), ("pyl.flags", 0x04, "u"), ("pyl.cellx", 0x0C, "u"), ("pyl.cellz", 0x10, "u"), ("pyl.control", 0x18, "p"),
    ("pyl.next", 0x28, "p"), ("pyl.prev", 0x2C, "p"), ("pyl.count", 0x30, "u"), ("pyl.braces", 0x34, "p"),
    ("ctl.flags", 0x00, "u"), ("ctl.f30", 0x30, "u"), ("ctl.f44", 0x44, "u"), ("ctl.fb4", 0xB4, "u"), ("ctl.fb8", 0xB8, "u"),
    ("ctl.fd0", 0xD0, "f"), ("ctl.ride", 0x14, "u"),
    ("set.flags", 0x04, "u"), ("set.tc", 0x10, "p"), ("set.groups", 0x18, "u"), ("set.gstride", 12, "n"), ("set.braceblk", 0xD4, "p"),
    ("tc.f1c", 0x1C, "f"), ("tc.f20", 0x20, "f"), ("tc.size", 0x38, "n"),
    ("fp.x", 0x00, "u"), ("fp.z", 0x04, "u"), ("fp.rot", 0x08, "u"), ("fp.ptr", 0x0C, "p"),
]
MACV = {k: v for k, v, _ in ALL}
KIND = {k: kd for k, _, kd in ALL}
HL = {}

BRACE_KEYS = [k for k, _, _ in ALL if k.startswith("brace.")]
INST_KEYS = [k for k, _, _ in ALL if k.startswith("inst.")]
PYL_KEYS = [k for k, _, _ in ALL if k.startswith("pyl.")]

BRACES = ["brace", "prev", "prevprev", "old1", "old2", "pb", "nxt", "b64", "bn0", "bn1", "bn2", "bn3", "bn4", "other"]
PYLONS = ["pylon", "pylon2", "rec", "recprev"]
CTLS = ["control", "pctl", "pctl2"]
RIDES = ["ride1", "ride2", "ride3"]
TCS = ["tc1", "tc2", "tc3"]
NAMES = ["inst"] + BRACES + PYLONS + CTLS + RIDES + TCS + ["blk", "cobj"]
OBJ_SIZE = 0x400
LABEL = {n: i + 1 for i, n in enumerate(NAMES)}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)

PAIR = ADDR["pair"]


def rec_params(e, p):
    L = e.cl
    if p == 0:
        L.rec("RideParams", (0, 0, 0, 0, 0, 0))
        return
    ptr = e.u32(p + 12)
    L.rec("RideParams", (s32(e.u32(p)), s32(e.u32(p + 4)), s32(e.u32(p + 8)), 1 if ptr == PAIR else (2 if ptr else 0),
                         s32(e.u32(PAIR)), s32(e.u32(PAIR + 4))))


def stub_malloc(e):
    L = e.cl
    addr = L.allocs.pop(0) if L.allocs else 0
    # the brace: the Mac asks for 0x104, the host for its own brace size; both sides record -1 for it
    size = -1 if e.r(3) == 0x104 else s32(e.r(3))
    L.rec("MReleaseAllocF", (size, s32(e.r(4)), L.lab(addr)))
    e.set_r(3, addr)


def stub_correct(e):
    e.cl.rec("CorrectBraceNumbering", (e.cl.lab(e.r(3)),))


def stub_instance_ride(e):
    L = e.cl
    sp = e.r(1)
    L.rec("InstanceRide", (s32(e.r(3)), s32(e.r(4)), s32(e.r(6)), s32(e.r(7)), L.lab(e.r(8)), s32(e.r(9)), s32(e.r(10)),
                           s32(e.u32(sp + 0x38)), s32(e.u32(sp + 0x3C))))
    rec_params(e, e.r(5))
    e.set_r(3, L.ret("InstanceRide") & MASK)


def stub_restore_ride(e):
    L = e.cl
    sp = e.r(1)
    L.rec("RestoreRideInstance", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(7)), L.lab(e.r(8)), s32(e.r(9)), s32(e.r(10)),
                                  s32(e.u32(sp + 0x38))))
    rec_params(e, e.r(6))
    e.set_r(3, L.ret("RestoreRideInstance") & MASK)


def stub_set_new_anim(e):
    L = e.cl
    s = e.r(3)
    base = e.u32(s + 0x10)
    L.rec("SetNewAnim", (L.lab(s), s32(e.r(4)), s32(e.r(5)), (s32(e.r(6)) - s32(base)) // 0x38, s32(e.r(7)), canon_f(fb(e.f(1)))))


def stub_setup_brace(e):
    L = e.cl
    L.rec("SetupBraceInfo", (L.lab(e.r(3)), s32(e.r(4) - e.r(3)), s32(e.r(5) - e.r(6)), L.lab(e.r(6))))


def stub_assign(e):
    L = e.cl
    L.rec("Assign3DObjectToAIWorld", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))


def stub_insert(e):
    L = e.cl
    sp = e.r(1)
    L.rec("InsertBraceIntoPylon", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5)), s32(e.r(6)), canon_f(fb(e.f(1))), L.lab(e.r(8)), s32(e.r(9)),
                                   s32(e.r(10)), s32(e.u32(sp + 0x38)), s32(e.u32(sp + 0x3C))))


def stub_flag(e):
    e.cl.rec("FlagCollisionUpdateTrackAroundBrace", (e.cl.lab(e.r(3)),))


def stub_remove(e):
    L = e.cl
    L.rec("RemoveBraceFromCollisionSystem", (L.lab(e.r(3)), L.lab(e.r(4))))


def stub_unlink_brace(e):
    L = e.cl
    L.rec("UnLinkBrace", (L.lab(e.r(3)), L.lab(e.r(4) - 0x3C)))


def stub_restore_pylon(e):
    L = e.cl
    L.rec("RestorePylonDetails", (L.lab(e.r(3)), L.lab(e.r(4))))


def stub_update_stack(e):
    L = e.cl
    L.rec("UpdatePylonStackMeshes", (L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5)), s32(e.r(6))))


for _a, _f in ((MALLOC, stub_malloc), (CORRECT, stub_correct), (INSTANCE_RIDE, stub_instance_ride), (RESTORE_RIDE, stub_restore_ride),
               (SET_NEW_ANIM, stub_set_new_anim), (SETUP_BRACE, stub_setup_brace), (ASSIGN, stub_assign), (INSERT, stub_insert),
               (FLAG_COLL, stub_flag), (REMOVE_COLL, stub_remove), (UNLINK_BRACE, stub_unlink_brace), (RESTORE_PYLON, stub_restore_pylon),
               (UPDATE_STACK, stub_update_stack)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def off(self, key):
        return MACV[key] if self.mac else HL[key]

    def rd(self, a, n, signed=False):
        return int.from_bytes(self.read(a, n), self.order)

    def wr(self, a, v, n):
        self.write(a, (v & ((1 << (8 * n)) - 1)).to_bytes(n, self.order))

    def addr_of(self, name):
        return 0 if name is None else self.o[name]

    def set_(self, base, key, v, index=0):
        """One field; v is an int (u32 / float bits) or an object name / None for a pointer."""
        kind = KIND[key]
        if key == "pyl.braces":
            a = base + self.off(key) + index * self.psz
        elif key == "ctl.fd0":
            a = base + self.off(key) + 4 * index
        elif key == "set.groups":
            a = base + self.off(key) + index * self.off("set.gstride")
        else:
            a = base + self.off(key)
        if kind == "p":
            self.wr(a, self.addr_of(v) if (v is None or isinstance(v, str)) else v, self.psz)
        else:
            self.wr(a, v, 4)

    def get_(self, base, key, index=0):
        kind = KIND[key]
        if key == "pyl.braces":
            a = base + self.off(key) + index * self.psz
        elif key == "ctl.fd0":
            a = base + self.off(key) + 4 * index
        elif key == "set.groups":
            a = base + self.off(key) + index * self.off("set.gstride")
        else:
            a = base + self.off(key)
        return self.rd(a, self.psz if kind == "p" else 4)

    def label(self, v):
        """A pointer value as (object name, offset in it); the host's field offsets mapped back to the Mac's."""
        if v == 0:
            return 0
        for n in NAMES:
            base = self.o[n]
            if base <= v < base + OBJ_SIZE:
                off = v - base
                if not self.mac and n in BRACES:
                    off = {HL["brace.cnt78"]: 0x78, HL["brace.cnt84"]: 0x84}.get(off, off)
                return [n, off]
        if v == self.g("pair"):
            return ["pair", 0]
        return ["?", v]

    def fill(self, name, d):
        base = self.o[name]
        for key, v in d.items():
            if isinstance(v, list):
                for i, x in enumerate(v):
                    self.set_(base, key, x, i)
            else:
                self.set_(base, key, v)

    def setup_objects(self):
        self.o = {n: self.alloc(OBJ_SIZE) for n in NAMES}

    def dump(self, name):
        base = self.o[name]
        out = []
        if name in BRACES:
            for k in BRACE_KEYS:
                out.append(self.field(base, k))
            out.append(self.label(self.rd(base + 0x1C, self.psz)))
        elif name == "inst":
            for k in INST_KEYS:
                out.append(self.field(base, k))
        elif name in PYLONS:
            for k in PYL_KEYS:
                if k == "pyl.braces":
                    for i in range(5):
                        out.append(self.label(self.get_(base, k, i)))
                else:
                    out.append(self.field(base, k))
        elif name in CTLS:
            out.append(self.get_(base, "ctl.flags"))
        elif name in RIDES:
            out.append(self.get_(base, "set.flags"))
            out.append(self.label(self.get_(base, "set.braceblk")))
        elif name in TCS:
            out.append(canon_f(self.get_(base, "tc.f1c")))
            out.append(canon_f(self.get_(base, "tc.f20")))
        elif name == "blk":
            out.append(self.label(self.rd(base + 0x10, self.psz)))
        return out

    def field(self, base, key):
        v = self.get_(base, key)
        kind = KIND[key]
        if kind == "p":
            return self.label(v)
        if kind == "f":
            return canon_f(v)
        return v


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"
    order = "big"
    psz = 4
    mac = True

    def __init__(self, e, case):
        self.e = e
        self.setup_objects()
        self.L = e.cl = CL.PpcLog({self.o[n]: LABEL[n] for n in NAMES}, case["queues"])
        self.L.allocs = [self.o["brace"], self.o["blk"]]
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

    def call_new(self, a):
        r3, _, _ = T.call_ppc(self.e, FN_NEW, [self.addr_of(a["pylon"]), T.F(bitsf(a["x"])), T.F(bitsf(a["z"])), T.F(bitsf(a["loft"])),
                                               a["flags"] & MASK, self.addr_of(a["control"]), a["loops"] & MASK, a["level"] & MASK,
                                               a["extra"] & MASK])
        return r3 & MASK

    def call_unlink(self):
        T.call_ppc(self.e, FN_UNLINK, [self.o["inst"]])
        return 0


class Host(Side):
    """The same on the host library."""
    fmt = "<I"
    order = "little"
    psz = 8
    mac = False

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.setup_objects()
        CL.host_begin(lib, {LABEL[n]: self.o[n] for n in NAMES}, case["queues"])
        lib.tpw_ntb_set_alloc(self.o["brace"], self.o["blk"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ntb_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ntb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_new(self, a):
        return self.lib.tpw_ntb_new(self.addr_of(a["pylon"]), bitsf(a["x"]), bitsf(a["z"]), bitsf(a["loft"]), a["flags"] & MASK,
                                    self.addr_of(a["control"]), a["loops"] & MASK, a["level"] & MASK, a["extra"] & MASK) & 0xFFFFFFFFFFFFFFFF

    def call_unlink(self):
        self.lib.tpw_ntb_unlink(self.o["inst"])
        return 0


def bind(lib):
    """The host library's entry points, typed once; the host offsets are read here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_ntb_addr.restype, lib.tpw_ntb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ntb_alloc.restype, lib.tpw_ntb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ntb_set_alloc.restype, lib.tpw_ntb_set_alloc.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_ntb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ntb_layout.restype = C.c_int32
    lib.tpw_ntb_new.restype = C.c_uint64
    lib.tpw_ntb_new.argtypes = [C.c_uint64, C.c_float, C.c_float, C.c_float, C.c_uint32, C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint32]
    lib.tpw_ntb_unlink.restype, lib.tpw_ntb_unlink.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 128)()
    n = lib.tpw_ntb_layout(out)
    assert n == len(ALL), (n, len(ALL))
    for (key, _, _), v in zip(ALL, list(out)[:n]):
        HL[key] = v
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.write(s.g("pair"), b"\0" * 8)
    for key in ("fp.x", "fp.z", "fp.rot", "fp.ptr"):
        s.set_(s.g("params"), key, 0)
    s.wr(s.g("edit"), s.addr_of("inst"), s.psz)
    s.wr(s.g("found"), s.addr_of(case["found"]), s.psz)
    s.wr(s.g("skip"), case["skip"], 4)
    for k, nm in enumerate(RIDES, 1):
        s.wr(s.g("rides") + k * s.psz, s.addr_of(nm), s.psz)
    for name, d in case["fill"].items():
        s.fill(name, d)


def execute(s, case):
    if case["op"] == "new":
        r = s.call_new(case["args"])
        return s.label(r)
    return s.call_unlink()


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log()}
    for n in NAMES:
        out["o:" + n] = s.dump(n)
    pb = s.g("params")
    out["params"] = [s.get_(pb, "fp.x"), s.get_(pb, "fp.z"), s.get_(pb, "fp.rot"), s.label(s.get_(pb, "fp.ptr")),
                     s.rd(s.g("pair"), 4), s.rd(s.g("pair") + 4, 4)]
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

SMALL = [0, 0x80000000, 0x3F000000, 0x3F800000, 0xBF800000, 0x40000000, 0xC0000000, 0x7F800000, 0xFF800000, 0x7FC00000, 0x00000001,
         0x3F800001, 0x42C80000]


def rf(r):
    k = r.random()
    if k < 0.25:
        return r.choice(SMALL)
    if k < 0.65:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def u32(r):
    return r.choice([0, 1, 0xFFFFFFFF, 0x80000000, r.getrandbits(32), r.getrandbits(32), r.randrange(0, 20)])


def fl(r):
    """Flag words: random bits, now and then none or all."""
    return r.choice([0, 0xFFFFFFFF, r.getrandbits(32), r.getrandbits(32), r.getrandbits(32)])


def new_case(r, **o):
    def pick(k, f):
        return o[k] if k in o else f()

    flags = pick("flags", lambda: r.choice([r.getrandbits(10), r.getrandbits(10), r.getrandbits(10), r.randrange(0, 8)]))
    pyl1 = pick("pyl1", lambda: r.random() < 0.4)
    blk2 = pick("blk2", lambda: r.random() < 0.4)
    has_ctl = pick("has_ctl", lambda: True if not pyl1 else r.random() < 0.7)
    if not pyl1:
        has_ctl = True
    ctl_flags = pick("ctl_flags", lambda: (r.choice([0, 8]) | r.choice([0, 0x4000]) | (r.getrandbits(5) << 5) | r.choice([0, r.getrandbits(4)])))
    loops = pick("loops", lambda: r.choice([0, 1, 2, u32(r)]))
    cnt44 = pick("cnt44", lambda: r.choice([0, 1, 2, 3, 0xFFFFFFFF, 0x1000001, 0x1000003, 0x80000001, r.randrange(1, 1000)]))
    ride_k = r.choice([1, 2, 3])
    fill = {}
    blk3c = (2 if blk2 else 0) | (r.getrandbits(4) & ~2 & 0xF) | (r.getrandbits(28) << 4 if r.random() < 0.3 else 0)
    inst = {"inst.flags": fl(r), "inst.blk3c": blk3c, "inst.f40": u32(r), "inst.cnt44": cnt44, "inst.f48": rf(r), "inst.tail50": None,
            "inst.last54": r.choice([None, "old2"]), "inst.braces58": r.choice([None, "old1"]), "inst.b5c": r.choice([None, "old2"]),
            "inst.prev60": "prev" if (blk2 or r.random() < 0.5) else None, "inst.b64": r.choice([None, "other"]),
            "inst.pyl68": None, "inst.b6c": r.choice([None, "old2"]), "inst.f118": rf(r), "inst.cobj": r.choice([None, "cobj"])}
    fill["inst"] = inst
    for n in BRACES:
        fill[n] = {"brace.flags": fl(r), "brace.num": u32(r), "brace.prev": None, "brace.next": None}
    fill["prev"]["brace.prev"] = "prevprev"
    fill["prev"]["brace.next"] = r.choice([None, "old1"])
    fill["prevprev"]["brace.next"] = "prev"
    fill["brace"] = {"brace.flags": r.choice([0, fl(r)]), "brace.fe8": rf(r)}
    fill["pylon"] = {"pyl.flags": (1 if pyl1 else 0) | (r.getrandbits(8) << 1) & ~1 | 0, "pyl.cellx": u32(r), "pyl.cellz": u32(r),
                     "pyl.control": r.choice([None, "pctl"])}
    fill["pctl"] = {"ctl.flags": r.choice([0, 0x4000, fl(r)])}
    ctl = {"ctl.flags": ctl_flags, "ctl.ride": u32(r), "ctl.fb4": u32(r), "ctl.fb8": u32(r), "ctl.fd0": [rf(r) for _ in range(4)]}
    if r.random() < 0.3:
        ctl["ctl.fd0"] = [r.choice(SMALL) for _ in range(4)]
    fill["control"] = ctl
    for i, k in enumerate(RIDES):
        fill[k] = {"set.flags": fl(r), "set.tc": TCS[i], "set.groups": [r.choice([0, 1, 2, 3, 4, 7, r.getrandbits(32)]) for _ in range(12)],
                   "set.braceblk": None}
        fill[TCS[i]] = {"tc.f1c": rf(r), "tc.f20": rf(r)}
    case = {"op": "new", "found": None, "skip": 0, "fill": fill,
            "queues": {"InstanceRide": [ride_k], "RestoreRideInstance": [r.choice([1, 2, 3])]},
            "args": {"pylon": "pylon", "x": rf(r), "z": rf(r), "loft": rf(r), "flags": flags, "control": "control" if has_ctl else None,
                     "loops": loops, "level": u32(r), "extra": u32(r)}}
    return case


def new_edges():
    r = random.Random(1157)
    bits = [1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200]
    for fl_ in [0] + bits + [0x3FF, 3, 5, 0x203, 0x201, 0x206, 0x210, 0x214]:
        for pyl1 in (True, False):
            for blk2 in (False, True):
                yield new_case(r, flags=fl_, pyl1=pyl1, blk2=blk2)
    for cf in (0, 8, 0x4000, 0x4008, 0x20, 0x40, 0x80, 0x100, 0x200, 0x3E0, 0x43E8):
        for loops in (0, 1):
            for fl_ in (0, 1, 2, 0x200):
                yield new_case(r, ctl_flags=cf, loops=loops, pyl1=False, flags=fl_, blk2=False)
    for c in (0, 1, 2, 3, 0xFFFFFFFF, 0x1000001, 0x1000003, 0x80000001, 0x7FFFFFFF, 0x1000002):
        yield new_case(r, cnt44=c, flags=0, pyl1=False, blk2=False)
        yield new_case(r, cnt44=c, flags=4, pyl1=True, blk2=False)
    for _ in range(40):
        c = new_case(r, pyl1=False, flags=r.choice([0, 1, 4]), blk2=False)
        c["fill"]["control"]["ctl.fd0"] = [r.choice(SMALL) for _ in range(4)]
        c["fill"]["brace"]["brace.fe8"] = r.choice(SMALL)
        yield c


def new_random(r):
    return new_case(r)


def unl_case(r, **o):
    def pick(k, f):
        return o[k] if k in o else f()

    count = pick("count", lambda: r.choice([0, 1, 2, 3, 4, 5, r.randrange(0, 6)]))
    skip = pick("skip", lambda: 1 if count == 0 else r.choice([0, 0, 1]))
    if count == 0:
        skip = 1
    found = pick("found", lambda: r.choice([None, "pylon"]))
    iflag8 = pick("iflag8", lambda: r.random() < 0.5)
    pb8000 = pick("pb8000", lambda: r.random() < 0.5)
    rec8 = pick("rec8", lambda: r.random() < 0.6)
    has_pyl = pick("has_pyl", lambda: r.random() < 0.85)
    fill = {}
    fill["inst"] = {"inst.flags": (fl(r) & ~8) | (8 if iflag8 else 0), "inst.blk3c": fl(r), "inst.f40": r.choice([0, 1, 5, u32(r)]),
                    "inst.tail50": r.choice([None, "other"]), "inst.pyl68": "rec", "inst.b6c": "pb", "inst.prev60": "prev",
                    "inst.b64": "b64", "inst.braces58": None, "inst.last54": None}
    for n in BRACES:
        fill[n] = {"brace.flags": fl(r), "brace.num": u32(r), "brace.prev": None, "brace.next": None, "brace.f74": None, "brace.f90": None,
                   "brace.data": r.choice(["pylon", "pylon", "pylon2"])}
    fill["pb"]["brace.flags"] = (fl(r) & ~0x8000) | (0x8000 if pb8000 else 0)
    fill["pb"]["brace.next"] = r.choice([None, "nxt", "nxt"])
    fill["pb"]["brace.data"] = "pylon" if has_pyl and r.random() < 0.8 else ("pylon2" if has_pyl else None)
    fill["pylon"] = {"pyl.flags": fl(r), "pyl.count": count, "pyl.control": "pctl", "pyl.braces": ["bn0", "bn1", "bn2", "bn3", "bn4"]}
    fill["pylon2"] = {"pyl.flags": fl(r), "pyl.count": r.choice([1, 2, 3, 5]), "pyl.control": "pctl2",
                      "pyl.braces": ["bn4", "bn3", "bn2", "bn1", "bn0"]}
    fill["rec"] = {"pyl.flags": (fl(r) & ~8) | (8 if rec8 else 0), "pyl.num": u32(r), "pyl.prev": r.choice([None, "recprev"]),
                   "pyl.next": r.choice([None, "recprev"])}
    fill["recprev"] = {"pyl.flags": fl(r), "pyl.next": "rec", "pyl.num": u32(r)}
    fill["pctl"] = {"ctl.flags": fl(r)}
    fill["pctl2"] = {"ctl.flags": fl(r)}
    return {"op": "unlink", "found": found, "skip": skip, "fill": fill, "queues": {}, "args": {}}


def unl_edges():
    r = random.Random(1158)
    for found in (None, "pylon"):
        for iflag8 in (False, True):
            for pb8000 in (False, True):
                for rec8 in (False, True):
                    for count in (0, 1, 2, 3, 5):
                        yield unl_case(r, found=found, iflag8=iflag8, pb8000=pb8000, rec8=rec8, count=count)
    for count in (1, 2, 3, 4, 5):
        for skip in (0, 1):
            for _ in range(6):
                yield unl_case(r, found="pylon", iflag8=False, count=count, skip=skip)
                yield unl_case(r, found=None, count=count, skip=skip)
    yield unl_case(r, found="pylon", iflag8=False, has_pyl=False)
    yield unl_case(r, found=None, has_pyl=False)


def unl_random(r):
    return unl_case(r)


TESTS = [
    FnTest("NewTrackBrace", ppc, host, edge=new_edges, rnd=new_random),
    FnTest("UnLinkPreviewBrace", ppc, host, edge=unl_edges, rnd=unl_random),
]
