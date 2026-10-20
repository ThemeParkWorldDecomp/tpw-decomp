"""Unit v5963, six bullfrog functions, against libtpw_unit_RegisterBullfrogWindowClass_2.dylib
(tests/unit_RegisterBullfrogWindowClass_2.cpp), main app ("SimTheme Park"), src/CreateDirectDrawObject_RegisterBullfrogWindowClass_2.cpp:
  RegisterBullfrogWindowClass 0x1006a824, _ScreenFPUHandler::~_ScreenFPUHandler 0x1006aa8c, __sinit_TbSystemInfo_cpp 0x10043d04,
  LbRect_Contains 0x10029bcc, LbRect_Bounding 0x10029a04, bfl::compress::CRefpack::MemCompress 0x1000d53c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (LbApp_GetHInstance, LbApp_GetWindowIcon, ZeroMemory, LoadIcon, GetStockObject, RegisterClass, CPU::detect, TbFPUFlag::Reset,
operator delete, operator new[] / delete[], the virtual CompressWorkspaceSize through __ptr_glue) are fakes on both sides that record their
arguments and answer from the case's queues. sprintf and memcpy run for real on both sides (their effect is compared in the memory).

The world of a case: the registration guard and flag, the class name buffer; the _ScreenFPUHandler global; the CPU global and the four
halfwords after it; rectangles; a source buffer with padding, a destination filled with 0xA5, an optional workspace. What is compared: the
answer, the globals and blocks read back after the call (the whole destination, the source and the whole 0xC0000 byte workspace), the event log.

No fused operations. Not tested: _ScreenFPUHandler::~_ScreenFPUHandler with this == 0xfffffffc (the Mac skips the stores for it; no real
object lives there), a source whose three bytes after the end are unreadable (the original reads past the end; the buffers are padded).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_RegisterBullfrogWindowClass_2"
MASK = 0xFFFFFFFF

FN_REG, FN_DTOR, FN_SINIT, FN_CONTAINS, FN_BOUNDING, FN_COMPRESS = (
    0x1006a824, 0x1006aa8c, 0x10043d04, 0x10029bcc, 0x10029a04, 0x1000d53c)
SIZES = {FN_REG: 272, FN_DTOR: 124, FN_SINIT: 72, FN_CONTAINS: 112, FN_BOUNDING: 148, FN_COMPRESS: 1584}
OWN = set(SIZES)

# ---- the callees faked on both sides
GETHINST, GETICON = 0x10013f44, 0x10014308
ZEROMEM, LOADICON, GETSTOCK, REGCLASS, SPRINTF = 0x1006bd3c, 0x1006bfdc, 0x1006bff4, 0x1006c00c, 0x1006b16c
CPUDETECT, FPURESET, DELETE = 0x100156b0, 0x1001611c, 0x1006b2d4
PTRGLUE, NEWA, DELA, MEMCPY = 0x1006c12c, 0x1006b334, 0x1006b364, 0x1006b4fc

ADDR = dict(guard=0x100750f4, done=0x100750f8, classname=0x10075240, fpu=0x100750fc, fpu4=0x10075100,
            vtA=0x10073a84, vtB=0x10073a90, cpu=0x100748ac, f1c=0x100748c8)
# _ScreenFPUHandler: mF00, mVtable, mF08, mF0A, mF0C, sizeof; sizeof(bullfrog::CPU)
HOST_LAYOUT = [0, 8, 16, 18, 20, 24, 0x1c]
WORKSIZE = 0xC0000
CUR = {}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def stub_gethinst(e):
    e.cl.rec("GetHInstance", (0,))
    e.set_r(3, e.cl.ret("hinst") & MASK)


def stub_geticon(e):
    e.cl.rec("GetWindowIcon", (0,))
    e.set_r(3, e.cl.ret("icon") & 0xFFFF)


def stub_zeromem(e):
    n = e.r(4)
    e.cl.rec("ZeroMemory", (n,))
    e.write(e.r(3), bytes(n))


def stub_loadicon(e):
    e.cl.rec("LoadIcon", (e.r(3), e.r(4)))
    e.set_r(3, e.cl.ret("loadicon") & MASK)


def stub_getstock(e):
    e.cl.rec("GetStockObject", (e.r(3),))
    e.set_r(3, e.cl.ret("stock") & MASK)


def stub_regclass(e):
    w = struct.unpack(">10I", e.read(e.r(3), 0x28))
    e.cl.rec("RegisterClass", (w[0], int(w[1] == 0x10070e24), w[2], w[3], w[4], w[5], w[6], w[7], w[8], int(w[9] == 0x10075240)))
    e.set_r(3, e.cl.ret("regclass") & MASK)


def stub_sprintf(e):
    e.write(e.r(3), e.cstr(e.r(4)).encode("latin-1") + b"\0" if isinstance(e.cstr(e.r(4)), str) else e.cstr(e.r(4)) + b"\0")


def stub_detect(e):
    e.cl.rec("CPU::detect", (e.cl.lab(e.r(3)),))


def stub_reset(e):
    e.cl.rec("TbFPUFlag::Reset", (e.cl.lab(e.r(3)),))


def stub_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def stub_ptrglue(e):
    e.cl.rec("CompressWorkspaceSize", (int(e.r(3) == CUR.get("obj")),))
    e.set_r(3, e.cl.ret("wsize") & MASK)


def stub_newa(e):
    size = e.r(3)
    e.cl.rec("new[]", (size,))
    CUR["work"] = e.alloc(max(size, WORKSIZE))
    e.set_r(3, CUR["work"])


def stub_dela(e):
    e.cl.rec("delete[]", (int(e.r(3) == CUR.get("work")),))


def stub_memcpy(e):
    n = e.r(5)
    if n:
        e.write(e.r(3), e.read(e.r(4), n))


def glue(fn):
    """A call through the import glue: the caller reloads its TOC from 0x14(r1) afterwards, where the real glue saved it."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return wrapped


for _a, _f in ((GETHINST, stub_gethinst), (GETICON, stub_geticon), (CPUDETECT, stub_detect), (FPURESET, stub_reset)):
    STUBS.internal(_a, _f)
for _a, _f in ((ZEROMEM, stub_zeromem), (LOADICON, stub_loadicon), (GETSTOCK, stub_getstock), (REGCLASS, stub_regclass),
               (SPRINTF, stub_sprintf), (DELETE, stub_delete), (PTRGLUE, stub_ptrglue), (NEWA, stub_newa), (DELA, stub_dela),
               (MEMCPY, stub_memcpy)):
    STUBS.internal(_a, glue(_f))


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def put(self, a, code, v):
        self.write(a, struct.pack(self.fmt[0] + code, v))

    def get(self, a, code):
        return struct.unpack(self.fmt[0] + code, self.read(a, struct.calcsize(self.fmt[0] + code)))[0]

    def set_vt(self, tag):
        self.put(self.g("fpu") + self.OFF[1], self.PTR, {0: 0x12345678, 1: self.g("vtA"), 2: self.g("vtB")}[tag])

    def get_vt(self):
        v = self.get(self.g("fpu") + self.OFF[1], self.PTR)
        return {self.g("vtA"): "A", self.g("vtB"): "B"}.get(v, v)


class Ppc(Side):
    fmt = ">I"
    PTR = "I"
    OFF = (0, 4, 8, 0xa, 0xc)

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({0x100750fc: 1, 0x10075100: 2, 0x100748ac: 3}, case["queues"])
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
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    PTR = "Q"
    OFF = (0, 8, 16, 18, 20)

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("fpu"), 2: self.g("fpu4"), 3: self.g("cpu")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rb2_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_rb2_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        l = self.lib
        if entry == FN_REG:
            l.tpw_rb2_register()
            return 0
        if entry == FN_DTOR:
            return l.tpw_rb2_dtor(args[0], s32(args[1]) if False else C.c_int16(args[1] & 0xFFFF).value)
        if entry == FN_SINIT:
            l.tpw_rb2_sinit()
            return 0
        if entry == FN_CONTAINS:
            return l.tpw_rb2_contains(args[0], args[1])
        if entry == FN_BOUNDING:
            l.tpw_rb2_bounding(args[0], args[1])
            return 0
        return l.tpw_rb2_compress(args[1], args[2], args[3] & MASK, args[4]) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rb2_addr.restype, lib.tpw_rb2_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rb2_alloc.restype, lib.tpw_rb2_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rb2_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rb2_register.restype, lib.tpw_rb2_register.argtypes = None, []
    lib.tpw_rb2_dtor.restype, lib.tpw_rb2_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int16]
    lib.tpw_rb2_sinit.restype, lib.tpw_rb2_sinit.argtypes = None, []
    lib.tpw_rb2_contains.restype, lib.tpw_rb2_contains.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    lib.tpw_rb2_bounding.restype, lib.tpw_rb2_bounding.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_rb2_compress.restype, lib.tpw_rb2_compress.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rb2_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def gen_data(case):
    r = random.Random(case["seed"])
    n, kind = case["len"], case["kind"]
    if kind == "zero":
        return bytes(n)
    if kind == "rand":
        return r.randbytes(n)
    if kind == "window":
        d = case["dist"]
        a = r.randbytes(0x300)
        return a + r.randbytes(d - 0x300) + a + r.randbytes(0x100)
    out = bytearray()
    alpha = case["alpha"]
    while len(out) < n:
        if len(out) > 3 and r.random() < 0.6:
            d = r.randint(1, min(len(out), case["dmax"]))
            for _ in range(r.choice([3, 4, 5, 8, 10, 11, 20, 67, 68, 70, 200, 300, 600, 1100])):
                out.append(out[-d])
        else:
            for _ in range(r.randint(1, 12)):
                out.append(r.randrange(alpha))
    return bytes(out[:n])


def build(s, case):
    op = case["op"]
    if op == "reg":
        s.put(s.g("guard"), "B", case["guard"])
        s.put(s.g("done"), "I", case["done"])
        s.write(s.g("classname"), b"X" * 32)
    elif op == "dtor":
        a = s.g("fpu")
        s.put(a + s.OFF[0], "B", case["f00"])
        s.set_vt(case["vt"])
        s.put(a + s.OFF[2], "H", case["f08"])
        s.put(a + s.OFF[3], "H", case["f0a"])
        s.put(a + s.OFF[4], "H", case["f0c"])
    elif op == "sinit":
        s.write(s.g("cpu"), bytes(case["cpu"]))
        s.write(s.g("f1c"), struct.pack(s.fmt[0] + "4H", *case["f1c"]))
    elif op in ("contains", "bounding"):
        a = s.alloc(16)
        b = a if case["alias"] else s.alloc(16)
        s.words(a, case["a"])
        if not case["alias"]:
            s.words(b, case["b"])
        s.st = (a, b)
    else:
        data = gen_data(case)
        n = len(data)
        s.obj = s.alloc(8)
        s.words(s.obj, [s.alloc(0x20) & MASK])
        s.src = s.alloc(n + 0x440)
        s.write(s.src, data + bytes((i * 37 + 11) & 0xFF for i in range(0x440)))
        s.dstsize = 2 * n + 64
        s.dst = s.alloc(s.dstsize)
        s.write(s.dst, b"\xa5" * s.dstsize)
        s.work = s.alloc(WORKSIZE) if case["work"] else 0
        s.n = n
        CUR["obj"] = s.obj
        CUR["work"] = None


def execute(s, case):
    op = case["op"]
    if op == "reg":
        s.call(FN_REG, [])   # void: r3 is whatever the last callee left
        return 0
    if op == "dtor":
        a = s.g("fpu")
        r = s.call(FN_DTOR, [a if case["this"] else 0, case["flag"]])
        return 0 if r == 0 else (1 if r == a else 2)
    if op == "sinit":
        s.call(FN_SINIT, [])
        return 0
    if op == "contains":
        return s.call(FN_CONTAINS, [s.st[0], s.st[1]])
    if op == "bounding":
        s.call(FN_BOUNDING, [s.st[0], s.st[1]])
        return 0
    return s.call(FN_COMPRESS, [s.obj, s.dst, s.src, s.n, s.work])


def observe(s, case, answer):
    op = case["op"]
    d = {"answer": answer, "log": s.log()}
    if op == "reg":
        d["guard"] = s.get(s.g("guard"), "B")
        d["done"] = s.get(s.g("done"), "I")
        d["classname"] = s.read(s.g("classname"), 32)
    elif op == "dtor":
        a = s.g("fpu")
        d["fields"] = (s.get(a + s.OFF[0], "B"), s.get(a + s.OFF[2], "H"), s.get(a + s.OFF[3], "H"), s.get(a + s.OFF[4], "H"))
        d["vt"] = s.get_vt()
    elif op == "sinit":
        d["cpu"] = s.read(s.g("cpu"), 0x1c)
        d["f1c"] = s.read(s.g("f1c"), 8)
    elif op in ("contains", "bounding"):
        d["a"] = s.rd_words(s.st[0], 4)
        d["b"] = s.rd_words(s.st[1], 4)
    else:
        d["dst"] = s.read(s.dst, s.dstsize)
        d["src"] = s.read(s.src, s.n + 0x440)
        if case["work"]:
            d["work"] = s.rd_words(s.work, WORKSIZE // 4)
    return d


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

INTS = [0, 1, -1, 2, -2, 0x7FFFFFFF, -0x80000000, 5, -5]


def reg_case(r, guard=None, done=None, icons=None):
    icons = icons if icons is not None else [r.choice([0, 0, 1, 0x8000, 0xFFFF, r.randrange(1, 0xFFFF)]) for _ in range(2)]
    return {"op": "reg", "guard": r.choice([0, 1]) if guard is None else guard, "done": r.choice([0, 1, 2, 0x100]) if done is None else done,
            "queues": {"hinst": [r.randrange(1 << 32), r.randrange(1 << 32)], "icon": icons, "loadicon": [r.randrange(1 << 32)],
                       "stock": [r.randrange(1 << 32)], "regclass": [r.randrange(1 << 16)]}}


def reg_edges():
    r = random.Random(11)
    for guard in (0, 1):
        for done in (0, 1, 7):
            for icons in ([0, 0], [5, 5], [5, 0], [0, 9], [0x8000, 0x8000], [0xFFFF, 0xFFFF], [1, 0x7FFF]):
                yield reg_case(r, guard, done, icons)


def reg_random(r):
    return reg_case(r)


def dtor_case(r, this=None, flag=None):
    return {"op": "dtor", "this": r.choice([0, 1, 1]) if this is None else this,
            "flag": r.choice([0, 1, -1, 5, 0x7FFF, 0x8000, 0x10000, 0x18001, 0xFFFF, r.randrange(1 << 16)]) if flag is None else flag,
            "f00": r.randrange(256), "vt": r.choice([0, 1]), "f08": r.randrange(1 << 16), "f0a": r.randrange(1 << 16),
            "f0c": r.randrange(1 << 16), "queues": {}}


def dtor_edges():
    r = random.Random(12)
    for this in (0, 1):
        for flag in (0, 1, 2, -1, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x18000, 0x18001, 0xFFFF8000):
            yield dtor_case(r, this, flag)


def dtor_random(r):
    return dtor_case(r)


def sinit_case(r):
    return {"op": "sinit", "cpu": [r.randrange(256) for _ in range(0x1c)], "f1c": [r.randrange(1, 1 << 16) for _ in range(4)], "queues": {}}


def sinit_edges():
    r = random.Random(13)
    yield sinit_case(r)
    yield {"op": "sinit", "cpu": [0] * 0x1c, "f1c": [0, 0, 0, 0], "queues": {}}
    yield {"op": "sinit", "cpu": [255] * 0x1c, "f1c": [0xFFFF] * 4, "queues": {}}


def sinit_random(r):
    return sinit_case(r)


def rect_case(op, a, b, alias=False):
    return {"op": op, "a": [x & MASK for x in a], "b": [x & MASK for x in b], "alias": alias, "queues": {}}


def rect_edges(op):
    r = random.Random(14 if op == "contains" else 15)
    base = [10, 20, 30, 40]
    for i in range(4):
        for da in (-1, 0, 1):
            for db in (-1, 0, 1):
                a, b = list(base), list(base)
                a[i] += da
                b[i] += db
                yield rect_case(op, a, b)
    for x in (0x7FFFFFFF, -0x80000000, -1, 0):
        for y in (0x7FFFFFFF, -0x80000000, -1, 0, 1):
            for i in range(4):
                a, b = list(base), list(base)
                a[i], b[i] = x, y
                yield rect_case(op, a, b)
    yield rect_case(op, base, base, True)
    yield rect_case(op, [3, 4, 5, 6], [3, 4, 5, 6])
    yield rect_case(op, [0, 0, 100, 100], [10, 10, 20, 20])
    yield rect_case(op, [10, 10, 20, 20], [0, 0, 100, 100])
    yield rect_case(op, [0, 0, 100, 100], [10, 10, 200, 20])
    yield rect_case(op, [0, 0, 100, 100], [10, 10, 20, 200])
    yield rect_case(op, [0, 0, 100, 100], [-5, 10, 20, 20])
    yield rect_case(op, [0, 0, 100, 100], [10, -5, 20, 20])


def rect_random(op):
    def gen(r):
        def v():
            return r.choice([r.randint(-3, 3), r.randint(-3, 3), r.choice(INTS), r.randrange(-(1 << 31), 1 << 31)])
        return rect_case(op, [v() for _ in range(4)], [v() for _ in range(4)], r.random() < 0.05)
    return gen


def comp_case(r, length, kind=None, work=None, **kw):
    kind = kind or r.choice(["lz", "lz", "lz", "rand", "zero"])
    c = {"op": "compress", "len": length, "kind": kind, "seed": r.randrange(1 << 30), "alpha": r.choice([2, 4, 16, 256]),
         "dmax": r.choice([8, 300, 3000, 30000]), "work": r.choice([True, False]) if work is None else work,
         "queues": {"wsize": [r.choice([0, 1, 0x1000, WORKSIZE, 0x123456])]}}
    c.update(kw)
    return c


def comp_edges():
    r = random.Random(16)
    for length in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 16, 17, 20, 40, 100, 0x70, 0x71, 0x74, 0x80, 300, 0x403, 0x404, 0x405, 0x406, 0x500, 1000, 2500):
        for kind in ("zero", "lz"):
            yield comp_case(r, length, kind, work=bool(length & 1))
    yield comp_case(r, 5000, "rand", work=False)
    yield comp_case(r, 6000, "lz", work=True, dmax=30000, alpha=2)
    yield comp_case(r, 20000, "lz", work=True, dmax=30000, alpha=16)
    for dist in (0x1FFFF, 0x20000):
        d = dist
        yield comp_case(r, d + 0x400, "window", work=True, dist=d)


def comp_random(r):
    k = r.random()
    if k < 0.3:
        length = r.randrange(0, 40)
    elif k < 0.7:
        length = r.randrange(40, 400)
    elif k < 0.95:
        length = r.randrange(400, 4000)
    else:
        length = r.randrange(4000, 20000)
    return comp_case(r, length)


TESTS = [
    FnTest("RegisterBullfrogWindowClass", ppc, host, edge=reg_edges, rnd=reg_random),
    FnTest("_ScreenFPUHandler::~_ScreenFPUHandler", ppc, host, edge=dtor_edges, rnd=dtor_random),
    FnTest("__sinit_TbSystemInfo_cpp", ppc, host, edge=sinit_edges, rnd=sinit_random),
    FnTest("LbRect_Contains", ppc, host, edge=lambda: rect_edges("contains"), rnd=rect_random("contains")),
    FnTest("LbRect_Bounding", ppc, host, edge=lambda: rect_edges("bounding"), rnd=rect_random("bounding")),
    FnTest("bfl::compress::CRefpack::MemCompress", ppc, host, edge=comp_edges, rnd=comp_random),
]
