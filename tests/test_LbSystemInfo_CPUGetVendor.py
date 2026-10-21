"""Unit v51071, thirty small bullfrog functions (TbSystemInfo, LbDraw, WinScrn, LbMaths, TbJPEGSaver, Draw, LbSCBack), against
libtpw_unit_LbSystemInfo_CPUGetVendor.dylib (tests/unit_LbSystemInfo_CPUGetVendor.cpp), src/bullfrog/Draw.cpp, src/bullfrog/LbApp.cpp, src/bullfrog/LbMaths.cpp, src/bullfrog/LbRect.cpp, src/bullfrog/LbTime.cpp, src/bullfrog/TbJPEGSaver.cpp, src/bullfrog/TbSystemInfo.cpp, src/bullfrog/WinScrn.cpp.

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian at the Mac's addresses, host
library little-endian). Callees outside the unit are fakes on both sides recording their arguments: CPU::clocks_per_millisec,
NS_MacDoze::Sleep, IDirectDraw::WaitForVerticalBlank.

The world: the CPU description, the screen globals, the JPEG masks, the draw contexts, pi, the window name and icon, and a 64 byte block
holding two rects, a size, a point and a 16 byte output buffer. Compared after every call: the answer (pointer answers as "is the expected
pointer"), the event log, the whole block and every global above (pointers as the symbol they equal). No fused operations. Not tested: nothing.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbSystemInfo_CPUGetVendor"
MASK = 0xFFFFFFFF

# name, address, argument kinds, answer kind ("v" void, "r" plain word, "copy" dest pointer, "name" the name pointer)
# argument kinds: A B S P V = block offsets, i = int, u = unsigned
OPS = [
    ("LbSystemInfo_CPUGetVendor", 0x10043bcc, "V", "v"),
    ("LbSystemInfo_CPUGetFamily", 0x10043b7c, "", "r"),
    ("LbSystemInfo_CPUhasMMX", 0x10043c28, "", "r"),
    ("LbSystemInfo_CPUhasRDTSC", 0x10043c68, "", "r"),
    ("LbSystemInfo_CPUGetTickRate", 0x10043ca4, "u", "r"),
    ("LbTime_Delay", 0x10046a54, "u", "v"),
    ("LbSize_Area", 0x10029f50, "S", "r"),
    ("LbSize_Set", 0x10029f18, "Suu", "v"),
    ("LbSize_IsNull", 0x10029ec4, "S", "r"),
    ("LbRect_Intersects", 0x10029e18, "AB", "r"),
    ("LbRect_IsNormal", 0x10029db8, "A", "r"),
    ("LbRect_IsEmpty", 0x10029c74, "A", "r"),
    ("LbRect_Offset", 0x10029928, "Aii", "v"),
    ("LbRect_Copy", 0x100298d0, "AB", "copy"),
    ("LbRect_SetWindow", 0x10029880, "Aiiii", "v"),
    ("LbRect_SetFromSize", 0x10029824, "AS", "v"),
    ("LbRect_Set", 0x100297e4, "Aiiii", "v"),
    ("LbPoint_Offset", 0x10029798, "Pii", "v"),
    ("LbPoint_Set", 0x10029760, "Pii", "v"),
    ("__sinit_LbMaths_cpp", 0x1002971c, "", "v"),
    ("__sinit_TbJPEGSaver_cpp", 0x100249c0, "", "v"),
    ("__sinit_Draw_cpp", 0x100661a8, "", "v"),
    ("LbScreen_StopOpenFadePalette", 0x10068710, "", "v"),
    ("LbScreen_SetDirectDrawGuid", 0x10068754, "u", "v"),
    ("LbScreen_IsInitialised", 0x10068c24, "", "r"),
    ("LbScreen_IsActive", 0x10068e20, "", "r"),
    ("LbScreen_ModeFindEnd", 0x100690bc, "A", "v"),
    ("LbScreen_WaitVbi", 0x1006a400, "", "v"),
    ("LbApp_GetWindowName", 0x10014340, "", "name"),
    ("LbApp_GetWindowIcon", 0x10014308, "", "r"),
]
OPID = {o[0]: i for i, o in enumerate(OPS)}
INFO = {o[0]: o for o in OPS}

CLOCKS = 0x1001577c      # bullfrog::CPU::clocks_per_millisec
SLEEP = 0x1006bc1c       # glue NS_MacDoze::Sleep
WAITVBI = 0x1006bd54     # glue IDirectDraw::WaitForVerticalBlank

ADDR = dict(cpu=0x100748ac, guid=0x1007511c, active=0x10075124, init=0x100750f0, ddslot=0x10075234, dd=0x10075240, fade=0x100748e8,
            jpeg=0x100747dc, draw32=0x100748d0, draw24=0x100748d4, draw16=0x100748d8, draw8=0x100748dc,
            t32=0x100738c0, t24=0x1007390c, t16=0x10073958, t8=0x100739a4, atan=0x10074810, pi=0x1007561c,
            name=0x10072600, icon=0x10073dfa)
HOST_LAYOUT = [0, 4, 8, 0xc, 0x1c, 16, 8, 8]
OFF = dict(A=0, B=16, S=32, P=40, V=48)
TABS = ["t32", "t24", "t16", "t8"]

STUBS = CL.Stubs()
STUBS.real.update(o[1] for o in OPS)


def stub_clocks(e):
    L = e.cl
    L.rec("clocks", (L.lab(e.r(3)), T.s32(e.r(4))))
    e.set_r(3, L.ret("clocks") & MASK)


def stub_sleep(e):
    e.cl.rec("Sleep", (e.r(3) & MASK,))


def stub_wait(e):
    L = e.cl
    L.rec("WaitVbi", (L.lab(e.r(3)), e.r(4) & MASK, L.lab(e.r(5))))
    e.set_r(3, 0)


STUBS.internal(CLOCKS, stub_clocks)
STUBS.internal(SLEEP, stub_sleep)
STUBS.internal(WAITVBI, stub_wait)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def f64(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "d", v))

    def rd_f64bits(self, a):
        return struct.unpack(">Q", struct.pack(">d", struct.unpack(self.fmt[0] + "d", self.read(a, 8))[0]))[0]

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["cpu"]: 1, ADDR["dd"]: 2}, case["queues"])
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

    def ptr(self, a, v):
        self.words(a, [v])

    def log(self):
        return self.L.log

    def call(self, name, args):
        r3, _, _ = T.call_ppc(self.e, INFO[name][1], [x & MASK for x in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("cpu"), 2: self.g("dd")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lsi_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_lsi_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, name, args):
        a = [x & 0xFFFFFFFFFFFFFFFF for x in args] + [0] * (5 - len(args))
        return self.lib.tpw_lsi_call(OPID[name], *a) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lsi_addr.restype, lib.tpw_lsi_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lsi_alloc.restype, lib.tpw_lsi_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lsi_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lsi_call.restype, lib.tpw_lsi_call.argtypes = C.c_uint64, [C.c_int32] + [C.c_uint64] * 5
    out = (C.c_int64 * 32)()
    n = lib.tpw_lsi_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("cpu"), case["cpu"][:3])
    s.write(s.g("cpu") + 12, struct.pack(">4I", *case["cpu"][3:]))      # the name is bytes, not words
    s.words(s.g("guid"), [case["guid"]])
    s.words(s.g("active"), [case["active"]])
    s.words(s.g("init"), [case["init"]])
    s.ptr(s.g("ddslot"), s.g("dd") if case["dd"] else 0)
    s.words(s.g("fade"), [case["fade"]])
    s.words(s.g("jpeg"), case["jpeg"])
    for n in ("draw32", "draw24", "draw16", "draw8"):
        s.words(s.g(n), [case[n]])
    s.f64(s.g("atan"), case["atan"])
    s.f64(s.g("pi"), case["pi"])
    s.write(s.g("name"), case["wname"])
    s.half(s.g("icon"), case["icon"])
    s.blk = s.alloc(64)
    s.words(s.blk, case["blk"][:12])
    s.write(s.blk + 48, struct.pack(">4I", *case["blk"][12:]))      # the output buffer is bytes


def sym(s, w, names):
    for n in names:
        if s.g(n) & MASK == w:
            return n
    return ("raw", w)


def execute(s, case):
    name = case["op"]
    _, _, spec, kind = INFO[name]
    ints = iter(case["ints"])
    args = []
    for k in spec:
        args.append(s.blk + OFF[k] if k in OFF else next(ints))
    r = s.call(name, args)
    if kind == "v":
        return 0
    if kind == "copy":
        return int(r == (s.blk & MASK))
    if kind == "name":
        return int(r == (s.g("name") & MASK))
    return r


def observe(s, case, answer):
    names = ["dd"]
    return {
        "answer": answer, "log": s.log(),
        "blk": s.rd_words(s.blk, 12), "outbuf": s.read(s.blk + 48, 16),
        "cpu": s.rd_words(s.g("cpu"), 3), "cpuname": s.read(s.g("cpu") + 12, 16),
        "guid": s.rd_word(s.g("guid")), "active": s.rd_word(s.g("active")), "init": s.rd_word(s.g("init")),
        "dd": sym(s, s.rd_word(s.g("ddslot")), names),
        "fade": s.rd_word(s.g("fade")), "jpeg": s.rd_words(s.g("jpeg"), 6),
        "draw": [sym(s, s.rd_word(s.g(n)), TABS) for n in ("draw32", "draw24", "draw16", "draw8")],
        "atan": s.rd_f64bits(s.g("atan")), "pi": s.rd_f64bits(s.g("pi")),
        "wname": s.read(s.g("name"), 64), "icon": s.rd_words(s.g("icon") & ~3, 1)[0] if False else s.read(s.g("icon"), 2)[::1 if s.fmt[0] == ">" else -1],
    }


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


PIS = [3.141592653589793, 3.0, 0.5, -2.0, 0.0, -0.0, 1e-300, 1e300, 1024.0]
SMALL = [-3, -2, -1, 0, 1, 2, 3]
WORDS = [0, 1, 2, 3, 7, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFE]


def base(r, op):
    mtype = r.choice([0, 0x600, 0x500, 0xF00, 0x400, 0x12345, 0xFFFFFFFF, r.getrandbits(32)])
    c = {"op": op, "queues": {"clocks": [r.choice([0, 1, 5000, 0x7FFFFFFF, 0x80000000])]}}
    c["cpu"] = [mtype, r.choice([0, 0x800000, 0x7FFFFF, 0xFF800000, r.getrandbits(32)]), r.choice([0, 1, r.getrandbits(32)])] + \
        [r.getrandbits(32) for _ in range(4)]
    c["guid"] = r.getrandbits(32)
    c["active"] = r.choice([0, 1, r.getrandbits(32)])
    c["init"] = r.choice([0, 1, r.getrandbits(32)])
    c["dd"] = r.choice([0, 1])
    c["fade"] = r.choice([0, 1, r.getrandbits(32)])
    c["jpeg"] = [r.getrandbits(32) for _ in range(6)]
    for n in ("draw32", "draw24", "draw16", "draw8"):
        c[n] = r.getrandbits(32)
    c["atan"] = r.choice([1.0, 0.25, r.uniform(-9, 9)])
    c["pi"] = r.choice(PIS + [r.uniform(-10, 10)])
    c["wname"] = bytes(r.getrandbits(8) for _ in range(64))
    c["icon"] = r.choice([0, 1, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16)])
    sm = lambda: r.choice(SMALL) & MASK
    c["blk"] = [sm() for _ in range(8)] + [r.choice(WORDS) for _ in range(2)] + [r.choice(WORDS) for _ in range(2)] + \
        [r.getrandbits(32) for _ in range(4)]
    c["ints"] = [r.choice(WORDS + [r.getrandbits(32)] + [x & MASK for x in SMALL]) for _ in range(4)]
    return c


def make_test(name, seed):
    def edges():
        r = random.Random(seed)
        for i in range(40):
            c = base(r, name)
            if i == 0:
                c["cpu"][0] = 0
            if i == 1:
                c["blk"][:8] = [1, 2, 3, 4, 1, 2, 3, 4]
            if i == 2:
                c["blk"][:8] = [1, 2, 3, 4, 3, 4, 5, 6]
            if i == 3:
                c["blk"][:8] = [3, 4, 5, 6, 1, 2, 3, 4]
            yield c

    def rnd(r):
        return base(r, name)

    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


TESTS = [make_test(o[0], 100 + i) for i, o in enumerate(OPS)]
