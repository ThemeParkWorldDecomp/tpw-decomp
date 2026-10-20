"""Unit v5089, the left mouse button going down on a cell, against libtpw_unit_LLCM_LeftMouseButtonDown.dylib
(tests/unit_LLCM_LeftMouseButtonDown.cpp), main app ("SimTheme Park"), src/LLCM_LeftMouseButtonDown.cpp:
  LLCM_LeftMouseButtonDown 0x10084a00

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (MouseMoved,
GetIllegalPlacement, both RecordAction overloads, SystemIsMode, SetCurrentObject, AddToClickStack, DrawLine, DoMovePylon,
CoasterAcceptLeftMouseClick) are fakes on both sides that record their arguments and answer from the case; DrawLine also rewrites
the x and y it is given by address.

The world of a case: the recorder's recording word, six cell system words and two bytes, and a zeroed block standing for the world
(gGameWorld points at it). What is compared: the event log, the words and bytes written, and the whole world block (the word DoMovePylon's
answer lands in). No fused operations. Not tested: coordinates outside 0..127 (the world block is sized for them).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCM_LeftMouseButtonDown"
MASK = 0xFFFFFFFF

FN = 0x10084A00
SIZES = {FN: 624}
OWN = set(SIZES)

CALLEES = dict(moved=0x10083180, illegal=0x10088970, rec2=0x10143FF8, rec3=0x101440E0, ismode=0x10091568,
               setcur=0x10088590, addclick=0x10091E5C, drawline=0x1009C0D0, domove=0x100579CC, coaster=0x10057F80)

ADDR = dict(rec=0x1037EC68, c214=0x102DC214, e0dc=0x102DE0DC, db34=0x102DDB34, db50=0x102DDB50, db54=0x102DDB54,
            e090=0x102DE090, f14=0x1029DF14, world=0x1037EBFC)
HOST_LAYOUT = [64]   # mRecording on the host (the host's three files are larger than the Mac's); the Mac's is 0x24
WORLD_SIZE = 0x200000
ZERO = bytes(WORLD_SIZE)
WORLD = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_moved(e):
    e.cl.rec("MouseMoved", (s32(e.r(3)), s32(e.r(4)), e.r(5) & 0xFFFF))


def st_illegal(e):
    e.cl.rec("GetIllegalPlacement", ())
    e.set_r(3, e.cl.ret("Illegal") & MASK)


def st_rec2(e):
    e.cl.rec("Rec2", (s32(e.r(4)), e.r(5) & 0xFF, e.r(6) & 0xFF))


def st_rec3(e):
    e.cl.rec("Rec3", (s32(e.r(4)), e.r(5) & 0xFF, e.r(6) & 0xFF, e.r(7) & 0xFFFF))


def st_ismode(e):
    m = s32(e.r(3))
    e.cl.rec("IsMode", (m,))
    e.set_r(3, e.cl.ret("IsMode:%d" % m) & MASK)


def st_setcur(e):
    e.cl.rec("SetCurrentObject", (e.r(3) & 0xFF,))


def st_addclick(e):
    e.cl.rec("AddToClickStack", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, 1)


def st_drawline(e):
    px, py = e.r(6), e.r(7)
    e.cl.rec("DrawLine", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), e.s32(px), e.s32(py)))
    nx = s32(e.cl.ret("DrawX"))
    ny = s32(e.cl.ret("DrawY"))
    if nx != 999:
        e.write(px, struct.pack(">i", nx))
    if ny != 999:
        e.write(py, struct.pack(">i", ny))


def st_domove(e):
    e.cl.rec("DoMovePylon", (e.r(3) & MASK, e.r(4) & MASK))
    e.set_r(3, e.cl.ret("DoMovePylon") & MASK)


def st_coaster(e):
    e.cl.rec("CoasterAcceptLeftMouseClick", (s32(e.r(3)),))


for key, fn in (("moved", st_moved), ("illegal", st_illegal), ("rec2", st_rec2), ("rec3", st_rec3), ("ismode", st_ismode),
                ("setcur", st_setcur), ("addclick", st_addclick), ("drawline", st_drawline), ("domove", st_domove),
                ("coaster", st_coaster)):
    STUBS.internal(CALLEES[key], fn)


def setup(runner):
    T.install_std_stubs(runner.emu)
    WORLD["ppc"] = T.persist_alloc(runner.emu, WORLD_SIZE)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def world(self):
        return WORLD["ppc"]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, x, y, shift, flag):
        T.call_ppc(self.e, FN, [x & MASK, y & MASK, shift & 0xFFFF, flag & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_llb_addr(name.encode())

    def world(self):
        if "host" not in WORLD:
            WORLD["host"] = self.lib.tpw_llb_alloc(WORLD_SIZE)
        return WORLD["host"]

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, x, y, shift, flag):
        self.lib.tpw_llb_down(s32(x), s32(y), shift & 0xFFFF, s32(flag))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_llb_addr.restype, lib.tpw_llb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_llb_alloc.restype, lib.tpw_llb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_llb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_llb_down.restype, lib.tpw_llb_down.argtypes = None, [C.c_int32, C.c_int32, C.c_uint32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_llb_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    w = s.world()
    s.write(w, ZERO)
    s.words(s.g("world"), [w] if s.fmt == ">I" else [w & MASK, w >> 32])   # the host's pointer is 8 bytes
    s.words(s.g("rec"), [case["rec"]])
    s.words(s.g("c214"), [case["c214"]])
    s.words(s.g("e0dc"), [case["e0dc"]])
    s.words(s.g("db50"), [case["db50"]])
    s.words(s.g("db54"), [case["db54"]])
    s.words(s.g("f14"), [case["f14"]])
    s.write(s.g("db34"), bytes([case["db34"]]))
    s.write(s.g("e090"), bytes([case["e090"]]))


def execute(s, case):
    s.call(*case["args"])


def observe(s, case):
    return {"log": s.log(), "c214": s.rd_words(s.g("c214"), 1), "f14": s.rd_words(s.g("f14"), 1),
            "db34": s.read(s.g("db34"), 1), "e090": s.read(s.g("e090"), 1), "world": [(i * 4, v) for i, v in enumerate(s.rd_words(s.world(), WORLD_SIZE // 4)) if v]}


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


def mk(r, rec=None, e0dc=None, e090=None, illegal=None, m38=None, m0=None, m16=None, shift=None, flag=None):
    c = {}
    c["rec"] = r.choice([0, 1, 5]) if rec is None else rec
    c["c214"] = r.choice([0, 7])
    c["e0dc"] = r.choice([0, 3]) if e0dc is None else e0dc
    c["db50"] = r.randrange(0, 128)
    c["db54"] = r.randrange(0, 128)
    c["f14"] = r.choice([0, 5, MASK])
    c["db34"] = r.choice([0, 1, 9])
    c["e090"] = r.choice([0, 1, 2]) if e090 is None else e090
    pick = lambda v: r.choice([0, 1]) if v is None else v
    c["queues"] = {
        "Illegal": [pick(illegal), pick(illegal)],
        "IsMode:56": [pick(m38)], "IsMode:0": [pick(m0)], "IsMode:22": [pick(m16)],
        "DrawX": [r.choice([999, r.randrange(0, 128)]) for _ in range(4)],
        "DrawY": [r.choice([999, r.randrange(0, 128)]) for _ in range(4)],
        "DoMovePylon": [r.choice([0, 1, 0x12345678, MASK])],
    }
    x = r.choice([0, 127, r.randrange(0, 128)])
    y = r.choice([0, 127, r.randrange(0, 128)])
    sh = r.choice([0, 1, 0xFFFF, r.randrange(0, 65536)]) if shift is None else shift
    fl = r.choice([0, 1, -1]) if flag is None else flag
    c["args"] = [x, y, sh, fl]
    return c


def edges():
    r = random.Random(89)
    for rec in (0, 1):
        for e0dc in (0, 3):
            for shift in (0, 5):
                for flag in (0, 1):
                    for illegal in (0, 1):
                        yield mk(r, rec=rec, e0dc=e0dc, shift=shift, flag=flag, illegal=illegal, m38=1, m0=0, m16=1, e090=1)
    for m38 in (0, 1):
        for illegal in (0, 1):
            for m0 in (0, 1):
                for m16 in (0, 1):
                    for e090 in (0, 1):
                        yield mk(r, m38=m38, illegal=illegal, m0=m0, m16=m16, e090=e090)


def rnd(r):
    return mk(r)


TESTS = [
    FnTest("LLCM_LeftMouseButtonDown", ppc, host, edge=edges, rnd=rnd),
]
