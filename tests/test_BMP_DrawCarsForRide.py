"""Unit v5672, BMP_DrawCarsForRide (the bumper cars of one ride are placed), against libtpw_unit_BMP_DrawCarsForRide.dylib
(tests/unit_BMP_DrawCarsForRide.cpp), main app ("SimTheme Park"), src/game/rides/BumperRide.cpp:
  BMP_DrawCarsForRide 0x10028030

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit are fakes on both sides that record their arguments: GetMeshPtrAtHFieldCell (answers an anim set by index from the
queue "mesh"), SetRideSpritePosnFocus (the set index of its mesh), GetRideHeightAtPoint (writes the height from the queue "height",
the word 1 leaves it), SetLMatrixIdentityNoTrans and RotateLMatrixXYZNoTrans (the set index of the matrix, the three angles),
PAR_InitEmitter. atan2 is the PowerPC side's Python math.atan2 and the host's libm.

The world of a case: the list at 0x102b56c0 (magic, ride records of 0xd0 bytes, bumps of 0xac bytes, their count), six ride anim sets
(each a set -> mesh -> header -> node chain with a random matrix) in gRideAnimSets, the sine table, the clamp globals, the char flag
and the scale. What is compared: the event log, the bumps (flags, car height), the six matrices, the globals.

Fused multiply-adds and fctiwz are hooked by HardwareRounding over the function. Not tested: a ride id whose magic fails with ride 0 and
a live bump (the original dereferences a null ride), more live bumps than the count names, more than 64 cars (the Mac's stack buffer).
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_BMP_DrawCarsForRide"
MASK = 0xFFFFFFFF

FN = 0x10028030
SIZES = {FN: 3156}
OWN = set(SIZES)

CALLEE_MESH, CALLEE_FOCUS, CALLEE_HEIGHT = 0x1005C1B0, 0x1005CC10, 0x1005D198
CALLEE_EMIT, CALLEE_ROT, CALLEE_IDENT = 0x100BEC30, 0x100C174C, 0x100C0EAC

ADDR = dict(world=0x102B56C0, sets=0x102C79A4, sine=0x102B4EC0, angles=0x1029CA9C, lo=0x1029CAA8, hi=0x1029CAAC,
            init=0x102B4EB8, scale=0x102B4EBC)
HOST_LAYOUT = [16, 8, 0x78, 0x10, 8, 0xAC, 0xD0]
K = 6                      # ride anim sets 1..K
NREC = 8                   # ride records
FLAGS = [0x1000000, 0x800, 0x400, 0x1000, 0x4000000, 0x2000, 0x2000000]
SINE = [int(256 * math.sin(3.14159 * 2 * i / 512)) & MASK for i in range(512)]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = {"sets": {}, "heads": {}, "mats": {}}     # the PowerPC side's anim sets: index -> address, heads address -> index, matrix address -> index


def stub_mesh(e):
    L = e.cl
    L.rec("Mesh", (e.r(3) & MASK, e.r(4) & MASK))
    e.set_r(3, CUR["sets"].get(L.ret("mesh"), 0))


def stub_focus(e):
    e.cl.rec("Focus", (CUR["heads"].get(e.r(3) & MASK, -1),))


def stub_height(e):
    L = e.cl
    L.rec("Height", (canon_f(fb(e.f(1))), canon_f(fb(e.f(2)))))
    v = L.ret("height") & MASK
    if v != 1:
        e.write(e.r(5) & MASK, struct.pack(">I", v))


def stub_ident(e):
    e.cl.rec("Identity", (CUR["mats"].get(e.r(3) & MASK, -1),))


def stub_rot(e):
    w = struct.unpack(">3I", e.read(e.r(5) & MASK, 12))
    e.cl.rec("Rotate", (CUR["mats"].get(e.r(3) & MASK, -1), s32(e.r(4)), canon_f(w[0]), canon_f(w[1]), canon_f(w[2])))


def stub_emit(e):
    L = e.cl
    L.rec("Emitter", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("emitter") & MASK)


def stub_atan2(e):
    e.set_f1(math.atan2(e.f(1), e.f(2)))


STUBS.internal(CALLEE_MESH, stub_mesh)
STUBS.internal(CALLEE_FOCUS, stub_focus)
STUBS.internal(CALLEE_HEIGHT, stub_height)
STUBS.internal(CALLEE_IDENT, stub_ident)
STUBS.internal(CALLEE_ROT, stub_rot)
STUBS.internal(CALLEE_EMIT, stub_emit)
STUBS.imp("atan2", stub_atan2)


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
    psz = 4
    o_heads, o_header = 8, 4

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        CUR["sets"], CUR["heads"], CUR["mats"] = {}, {}, {}

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

    def p32(self, a):
        return a

    def ptr(self, slot, target):
        self.words(slot, [target])

    def register(self, i, setp, heads, node):
        CUR["sets"][i] = setp
        CUR["heads"][heads] = i
        CUR["mats"][node + 0x10] = i

    def log(self):
        return self.L.log

    def call(self, ride, step):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN, [ride & MASK, T.F(bitsf(step))])


class Host(Side):
    fmt = "<I"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.o_heads, self.o_header = 16, 8
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_dcr_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_dcr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def p32(self, a):
        return self.lib.tpw_dcr_off(a)

    def ptr(self, slot, target):
        self.write(slot, struct.pack("<Q", target))

    def register(self, i, setp, heads, node):
        self.lib.tpw_dcr_register(i, setp)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, ride, step):
        self.lib.tpw_dcr_run(s32(ride), bitsf(step))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_dcr_addr.restype, lib.tpw_dcr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_dcr_alloc.restype, lib.tpw_dcr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_dcr_off.restype, lib.tpw_dcr_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_dcr_register.restype, lib.tpw_dcr_register.argtypes = None, [C.c_uint32, C.c_uint64]
    lib.tpw_dcr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_dcr_run.restype, lib.tpw_dcr_run.argtypes = None, [C.c_int32, C.c_float]
    out = (C.c_int64 * 32)()
    n = lib.tpw_dcr_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

BUMP_WORDS = (0xAC // 4)


def build(s, case):
    s.words(s.g("sine"), SINE)
    s.words(s.g("angles"), case["angles"])
    s.words(s.g("lo"), [fb(-1.0)])
    s.words(s.g("hi"), [fb(1.0)])
    s.write(s.g("init"), bytes([case["init"]]))
    s.words(s.g("scale"), [case["scale"]])
    s.nodes = {}
    for i in range(1, K + 1):
        setp, heads, header, node = s.alloc(0x200), s.alloc(0x80), s.alloc(0x100), s.alloc(0x100)
        s.ptr(setp + s.o_heads, heads)
        s.ptr(heads + s.o_header, header)
        s.words(header + 0x78, [s.p32(node)])
        s.words(node + 0x10, case["mats"][i - 1])
        s.ptr(s.g("sets") + s.psz * i, setp)
        s.register(i, setp, heads, node)
        s.nodes[i] = node
    rides = s.alloc(NREC * 0xD0)
    for i in range(NREC):
        s.words(rides + i * 0xD0, [case["kinds"][i]])
        s.words(rides + i * 0xD0 + 8, [case["w8"][i]])
    nb = len(case["bumps"])
    s.nslots = nb
    s.bumps = bumps = s.alloc((nb + 2) * 0xAC)
    for i, b in enumerate(case["bumps"]):
        base = bumps + i * 0xAC
        for off, v in b["w"].items():
            s.words(base + off, [v])
        owner = b["owner"]
        s.words(base + 0x9C, [s.p32(rides + owner * 0xD0) if owner >= 0 else 0])
    s.words(s.g("world"), [case["magic"], 0, s.p32(rides), 0, s.p32(bumps), case["count"]])


def execute(s, case):
    s.call(case["ride"], case["step"])


def observe(s, case):
    out = {"log": s.log()}
    words = s.rd_words(s.bumps, s.nslots * BUMP_WORDS)
    for i in range(s.nslots):
        words[i * BUMP_WORDS + 0x9C // 4] = 0
        j = i * BUMP_WORDS + 0xA0 // 4
        words[j] = canon_f(words[j])
    out["bumps"] = words
    for i in range(1, K + 1):
        out["mat%d" % i] = [canon_f(w) for w in s.rd_words(s.nodes[i] + 0x10, 16)]
    out["angles"] = s.rd_words(s.g("angles"), 3)
    out["init"] = s.read(s.g("init"), 1)
    out["scale"] = s.rd_words(s.g("scale"), 1)
    return out


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rflt(r, lo, hi):
    return fb(r.uniform(lo, hi))


def mkbump(r, owner, extra, live=True):
    w = {}
    flags = 1 if live else (r.randrange(0, 1 << 28) & ~1)
    for f in extra:
        flags |= f
    w[0] = flags
    w[8] = r.randrange(1, K + 1)
    w[0xC] = r.randrange(1, K + 1)
    w[0x34] = r.choice([r.randrange(-30000, 30000), r.randrange(-1000000, 1000000), 0])
    w[0x38] = r.choice([r.randrange(-30000, 30000), r.randrange(-1000000, 1000000), 0])
    w[0x3C] = r.randrange(-80, 80)
    w[0x40] = r.randrange(-80, 80)
    w[0x4C] = r.randrange(-300, 300)
    w[0x54] = r.choice([r.randrange(0, 512), r.randrange(-2000, 2000)])
    w[0x5C] = r.choice([r.randrange(-40, 40), 0])
    w[0x60] = r.randrange(-40, 40)
    w[0x90] = r.choice([r.randrange(-600, 600), 0, 1, 512, 513])
    w[0xA0] = rflt(r, -4, 4)
    return {"w": w, "owner": owner}


def mkcase(r, nbumps=None, flags=None, step=None, magic=True, mismatch=False):
    kinds = [r.choice([-1, -2, -5, -14, 0, 3, r.randrange(-50, 50)]) for _ in range(NREC)]
    idx = r.randrange(NREC)
    ride = s32(((kinds[idx] << 8) & MASK) | idx)
    if mismatch:
        ride = s32(ride + r.choice([1, -1, 256, 0x10000]))
    nb = r.randrange(0, 7) if nbumps is None else nbumps
    bumps = []
    live = 0
    for _ in range(nb):
        isl = r.random() < 0.8
        owner = idx if r.random() < 0.75 else r.choice([-1, (idx + 1) % NREC])
        if flags is None:
            extra = [f for f in FLAGS if r.random() < 0.4]
        else:
            extra = flags
        bumps.append(mkbump(r, owner, extra, isl))
        live += isl
    count = live
    if live and r.random() < 0.1:
        count = live - 1
    if step is None:
        step = r.choice([fb(r.uniform(-2, 2)), fb(r.uniform(-1, 1)), fb(r.uniform(-1, 1)), 0, fb(1.0), fb(-1.0), fb(0.5)])
    case = {
        "ride": ride, "step": step, "magic": 0x4A454647 if magic else r.choice([0, 0x4A454646]),
        "kinds": [k & MASK for k in kinds], "w8": [r.choice([r.randrange(0, 400), r.randrange(0, 400), 0, r.randrange(-50, 50)]) & MASK for _ in range(NREC)],
        "bumps": bumps, "count": count, "init": r.choice([0, 1, 0x80, 0xFF, 0]), "scale": rflt(r, 1000, 60000),
        "angles": [r.randrange(0, 1 << 32) for _ in range(3)],
        "mats": [[rflt(r, -5, 5) for _ in range(16)] for _ in range(K)],
        "queues": {
            "mesh": [r.choice([0, 1, 1, 2, 2, 3, K]) for _ in range(48)],
            "height": [r.choice([fb(-100.0), fb(-100.0), 1, rflt(r, -5, 5), rflt(r, -5, 5), fb(0.0)]) for _ in range(48)],
            "emitter": [r.randrange(0, 5)],
        },
    }
    return case


def edges():
    r = random.Random(672)
    # a bad magic with ride 0 and nothing to draw; a bad magic with another ride
    c = mkcase(r, nbumps=0, magic=False)
    c["ride"] = 0
    c["count"] = 0
    yield c
    c = mkcase(r, magic=False)
    c["ride"] = 5
    yield c
    yield mkcase(r, mismatch=True)
    yield mkcase(r, nbumps=0)
    # every flag alone, with one and two live bumps
    for f in FLAGS + [0]:
        for nb in (1, 2, 3):
            c = mkcase(r, nbumps=nb, flags=[f])
            for b in c["bumps"]:
                b["w"][0] |= 1
                b["owner"] = 0 if False else b["owner"]
            yield c
    # every step: the clamp ends, zero, negative zero, nan, inf
    for st in (0, 0x80000000, fb(1.0), fb(-1.0), fb(2.0), fb(-2.0), fb(0.999), fb(-0.999), 0x7FC00000, 0x7F800000, 0xFF800000):
        yield mkcase(r, nbumps=2, step=st)
    # the mesh answers all equal, all null
    for q in ([0], [1], [2], [1, 2, 1, 2], [0, 1, 0, 1, 0, 1, 0, 1]):
        c = mkcase(r, nbumps=2)
        c["queues"]["mesh"] = q
        yield c
    # the height unchanged, and exactly -100.0
    for q in ([1], [fb(-100.0)], [fb(3.0)]):
        c = mkcase(r, nbumps=2)
        c["queues"]["height"] = q
        yield c


def rnd(r):
    return mkcase(r)


TESTS = [
    FnTest("BMP_DrawCarsForRide", ppc, host, edge=edges, rnd=rnd),
]
