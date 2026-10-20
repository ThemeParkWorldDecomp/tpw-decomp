"""Unit v51031, a cell line walk, a globe destructor and a download globe's message handler, against libtpw_unit_LLCC_DrawLine_2.dylib
(tests/unit_LLCC_DrawLine_2.cpp), main app ("SimTheme Park"), src/MapCell_kinds_LLCC_DrawLine_2.cpp:
  LLCC_DrawLine 0x1009c0d0, CLBGlobe::~CLBGlobe 0x100ac534, CLBScreenComponentDownloadGlobe::WindowMessagesFn 0x100ae374

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments: CMap::IsXYValid (x, y; answers from a queue), CMapCell::Modify (the cell as an
index from the map's first cell, mode, a, b and the two globals at that moment; answers from a queue), the online globe's WindowMessagesFn
(object label, message, a, b). The virtual calls (__ptr_glue on the Mac; slots 0x24 Show, 0x28 Hide, 0x3c) land on screen objects whose vtable
words are tokens on the Mac and on a probe class on the host; both log (object, slot, argument).

The world of a case: gCalcNeighboursJoinEntry, gValidTypeChangeLock, the world pointer (a fake address, never read: only cell addresses are
computed), the end point words, the screen system and the selection pointer; the globe, screen, download globe and city flag objects. What is
compared: the answer, those globals, the end point words, the selection pointer and the event log.

No fused operations. Not tested: the destructor's delete storage flag above 0 (the compiler's operator delete on the host; the host side calls
the complete destructor) and a NULL screen in a globe (the original reads it unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCC_DrawLine_2"
MASK = 0xFFFFFFFF

FN_LINE, FN_DTOR, FN_WMF = 0x1009C0D0, 0x100AC534, 0x100AE374
SIZES = {FN_LINE: 768, FN_DTOR: 112, FN_WMF: 172}
OWN = set(SIZES)

CALLEE_VALID, CALLEE_MODIFY, CALLEE_ONLINE_WMF, GLUE_PTR = 0x101012F4, 0x10097538, 0x100AD54C, 0x1023BAA8

ADDR = dict(lock=0x102E4834, join=0x102E4838, world=0x1037EBFC, screens=0x102E49B8, sel=0x102E4864)
WORLD = 0x30000000
TOKEN = 0x70000000

PPC_OFF = dict(mF70=0x70, dl_size=0x80, isl=8, sys_size=0x20, gscreen=8, globe_size=0x18, city=0x0C, flag_size=0x10, base=0)
OFF_NAMES = ["mF70", "dl_size", "isl", "sys_size", "gscreen", "globe_size", "city", "flag_size", "base"]

NAMES = ["globe", "screen", "dl", "isl", "sys", "flag", "ex", "ey", "vt"]
LABELS = {"globe": 1, "screen": 2, "dl": 3, "isl": 4}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_valid(e):
    """CMap::IsXYValid(long x, long y)."""
    L = e.cl
    L.rec("IsXYValid", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("IsXYValid") & MASK)


def stub_modify(e):
    """CMapCell::Modify(cell, mode, a, b): the cell as its index from the first cell of the map at WORLD + 0x2d8."""
    L = e.cl
    index = (e.r(3) - WORLD - 0x2D8) // 0x44
    L.rec("Modify", (index, s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.u32(ADDR["join"])), s32(e.u32(ADDR["lock"]))))
    e.set_r(3, L.ret("Modify") & MASK)


def stub_online_wmf(e):
    L = e.cl
    L.rec("OnlineWMF", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))


def stub_glue(e):
    """__ptr_glue: r12 holds a vtable token (TOKEN + slot offset); the argument is only meaningful for slot 0x3c."""
    L = e.cl
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))      # the glue saves the caller's TOC where the code reloads it
    slot = (e.r(12) & MASK) - TOKEN
    L.rec("virt", (L.lab(e.r(3)), slot, L.lab(e.r(4)) if slot == 0x3C else 0))


STUBS.internal(CALLEE_VALID, stub_valid)
STUBS.internal(CALLEE_MODIFY, stub_modify)
STUBS.internal(CALLEE_ONLINE_WMF, stub_online_wmf)
STUBS.internal(GLUE_PTR, stub_glue)
STUBS.imp("abs", lambda e: e.set_r(3, abs(s32(e.r(3))) & MASK))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def make_objects(self):
        sizes = {"vt": 0x80, "sys": max(self.off["sys_size"], 0x40), "dl": max(self.off["dl_size"], 0x100),
                 "globe": max(self.off["globe_size"], 0x40), "flag": max(self.off["flag_size"], 0x40)}
        self.obj = {n: self.alloc(sizes.get(n, 0x100 if n == "screen" or n == "isl" else 8)) for n in NAMES}


class Ppc(Side):
    fmt = ">I"
    off = PPC_OFF

    def __init__(self, e, case):
        self.e = e
        self.make_objects()
        fixed = {self.obj[n]: lab for n, lab in LABELS.items()}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def ptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_word(a)

    def vtable(self):
        self.words(self.obj["vt"], [TOKEN + i for i in range(0, 0x80, 4)])

    def object(self, name):
        """A screen object: the vtable of tokens."""
        self.vtable()
        self.ptr(self.obj[name], self.obj["vt"])

    def globe(self, screen):
        self.vtable()
        self.words(self.obj["globe"], [0x11111111])
        self.ptr(self.obj["globe"] + self.off["gscreen"], screen)

    def call_line(self, mode, x, y):
        return T.call_ppc(self.e, FN_LINE, [mode & MASK, x & MASK, y & MASK, self.obj["ex"], self.obj["ey"]])[0] & MASK

    def call_dtor(self, this, flag):
        r3 = T.call_ppc(self.e, FN_DTOR, [this, flag & MASK])[0] & MASK
        return 1 if (this != 0 and r3 == this) else 0

    def call_wmf(self, m, a, b):
        T.call_ppc(self.e, FN_WMF, [self.obj["dl"], m & MASK, a & MASK, b & MASK])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = lib.off
        self.make_objects()
        out = (C.c_int64 * 32)()
        lib.tpw_ldl_layout(out)      # again: the arena base is only known once something was allocated
        self.off = dict(zip(OFF_NAMES, list(out)))
        CL.host_begin(lib, {lab: self.obj[n] for n, lab in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ldl_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ldl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0] & MASK

    def object(self, name):
        self.lib.tpw_ldl_probe(self.obj[name])

    def globe(self, screen):
        self.lib.tpw_ldl_globe(self.obj["globe"])
        self.ptr(self.obj["globe"] + self.off["gscreen"], screen)

    def call_line(self, mode, x, y):
        return self.lib.tpw_ldl_drawline(s32(mode), s32(x), s32(y), self.obj["ex"], self.obj["ey"]) & MASK

    def call_dtor(self, this, flag):
        return self.lib.tpw_ldl_dtor(this)

    def call_wmf(self, m, a, b):
        self.lib.tpw_ldl_wmf(self.obj["dl"], s32(m), s32(a), s32(b))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ldl_addr.restype, lib.tpw_ldl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ldl_alloc.restype, lib.tpw_ldl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ldl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ldl_probe.argtypes = [C.c_uint64]
    lib.tpw_ldl_globe.argtypes = [C.c_uint64]
    lib.tpw_ldl_drawline.restype = C.c_int32
    lib.tpw_ldl_drawline.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_uint64, C.c_uint64]
    lib.tpw_ldl_dtor.restype, lib.tpw_ldl_dtor.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_ldl_wmf.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ldl_layout(out)
    assert n == len(OFF_NAMES), n
    lib.off = dict(zip(OFF_NAMES, list(out)[:n]))
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o = s.obj
    s.words(s.g("join"), [case["join"]])
    s.words(s.g("lock"), [case["lock"]])
    s.ptr(s.g("world"), WORLD)
    s.ptr(s.g("sel"), case["sel0"])
    s.ptr(s.g("screens"), o["sys"])
    s.ptr(o["sys"] + s.off["isl"], o["isl"])
    s.object("isl")
    s.object("screen")
    s.object("dl")
    s.words(o["ex"], [case.get("ex", 0)])
    s.words(o["ey"], [case.get("ey", 0)])
    s.globe(o["screen"])
    flag_off = (o["flag"] - s.off["base"]) & MASK
    s.ptr(o["flag"] + s.off["city"], case["city"])
    s.words(o["dl"] + s.off["mF70"], [flag_off if case.get("flag", False) else 0])


def execute(s, case):
    op = case["op"]
    if op == "line":
        return s.call_line(*case["args"])
    if op == "dtor":
        return s.call_dtor(0 if case["null"] else s.obj["globe"], case["flag"])
    return s.call_wmf(*case["args"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "join": s.rd_word(s.g("join")), "lock": s.rd_word(s.g("lock")),
            "ex": s.rd_word(s.obj["ex"]), "ey": s.rd_word(s.obj["ey"]), "sel": s.rd_ptr(s.g("sel"))}


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

QUEUES = [[1], [0], [1, 1, 0], [1, 0, 1], [0, 1], [1, 1, 1, 0], [0, 0, 1]]
MODES = [0x15, 0x83, 0x81, 0x87, 0x11F, 0x13A, -1, 0, 2]


def base(r, op):
    return {"op": op, "join": r.choice([0, 1, 2]), "lock": r.choice([0, 1, 5]), "sel0": r.randrange(1, 1 << 31), "city": r.randrange(1, 1 << 31),
            "queues": {"IsXYValid": r.choice(QUEUES + [[r.randrange(2) for _ in range(r.randrange(1, 8))]]),
                       "Modify": r.choice(QUEUES + [[r.randrange(2) for _ in range(r.randrange(1, 8))]])}}


def line_case(r, dx=None, dy=None, x=None, y=None, vq=None, mq=None):
    c = base(r, "line")
    if x is None:
        x = r.choice([r.randrange(-5, 135), r.randrange(0, 128), 0, 127, 65535, -1, r.randrange(-70000, 70000)])
    if y is None:
        y = r.choice([r.randrange(-5, 135), r.randrange(0, 128), 0, 127, 511, -1])
    if dx is None:
        dx = r.choice([r.randrange(-8, 9), r.randrange(-40, 41), 0])
    if dy is None:
        dy = r.choice([r.randrange(-8, 9), r.randrange(-40, 41), 0])
    c["ex"], c["ey"] = (x + dx) & MASK, (y + dy) & MASK
    c["args"] = [r.choice(MODES), x, y]
    if vq is not None:
        c["queues"]["IsXYValid"] = vq
    if mq is not None:
        c["queues"]["Modify"] = mq
    return c


def line_edges():
    r = random.Random(77)
    for dx, dy in [(0, 0), (1, 0), (0, 1), (-1, 0), (0, -1), (3, 3), (-3, -3), (4, 3), (-4, 3), (3, -4), (5, -2), (-5, 2), (2, 5), (-2, -5),
                   (3, -3), (-3, 3)]:
        for vq, mq in [([1], [1]), ([0], [1]), ([1], [0]), ([1, 1, 0, 1], [1, 1, 0]), ([1, 0], [1, 1, 1, 0]), ([1, 1, 1, 1, 1], [1, 1, 1, 0])]:
            yield line_case(r, dx, dy, r.randrange(0, 100), r.randrange(0, 100), vq, mq)
    yield line_case(r, 2, 0, 65535, 3, [1], [1])
    yield line_case(r, 0, 2, 0xFFFF - 128, 0x1FF, [1], [1])


def line_random(r):
    return line_case(r)


def dtor_case(r, flag=None, null=None):
    c = base(r, "dtor")
    if flag is None:
        low = r.choice([0, 0xFFFF, 0x8000, 0xFFFF, r.randrange(0x8000, 0x10000), 0])
        flag = (r.randrange(0, 0x10000) << 16 | low) & MASK
    c["flag"] = flag
    c["null"] = (r.random() < 0.1) if null is None else null
    return c


def dtor_edges():
    r = random.Random(78)
    for flag in (0, MASK, 0x10000, 0xFFFF8000, 0x8000, 0xFFFF, 0x20000, 0xABCD0000, 0x7FFF8001):
        for null in (False, True):
            yield dtor_case(r, flag, null)


def dtor_random(r):
    return dtor_case(r)


def wmf_case(r, m=None, a=None, flag=None):
    c = base(r, "wmf")
    c["args"] = [r.choice([0x10005] * 5 + [0x10004, 0x10006, 0, -1, 5, r.randrange(-1 << 31, 1 << 31)]) if m is None else m,
                 r.choice([0, 0, 0, 1, -1, r.randrange(-1 << 31, 1 << 31)]) if a is None else a, r.choice([0, 1, r.randrange(-1 << 31, 1 << 31)])]
    c["flag"] = (r.random() < 0.7) if flag is None else flag
    return c


def wmf_edges():
    r = random.Random(79)
    for m in (0x10005, 0x10004, 0x10006, 0, -1, 0x20005, 5):
        for a in (0, 1, -1):
            for flag in (False, True):
                yield wmf_case(r, m, a, flag)


def wmf_random(r):
    return wmf_case(r)


TESTS = [
    FnTest("LLCC_DrawLine", ppc, host, edge=line_edges, rnd=line_random),
    FnTest("CLBGlobe::~CLBGlobe", ppc, host, edge=dtor_edges, rnd=dtor_random),
    FnTest("CLBScreenComponentDownloadGlobe::WindowMessagesFn", ppc, host, edge=wmf_edges, rnd=wmf_random),
]
