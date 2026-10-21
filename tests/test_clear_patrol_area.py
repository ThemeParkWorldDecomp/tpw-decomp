"""Unit v5341, four key actions, against libtpw_unit_clear_patrol_area.dylib (tests/unit_clear_patrol_area.cpp), main app
("SimTheme Park"), src/game/keyboard.cpp:
  clear_patrol_area 0x1014cfe4, wheres_my_script 0x1014d114, start_strike 0x1014ceac, abort_all_requesters 0x1014cb8c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit (UIInterface::GetSelectedPeep and RemoveLocators, CStaff::CancelPatrolArea, CWorld::GetStaffHQ, CStaffHQ::ForceStrike,
the CMsgEndOfMonth constructor, CMessageCentre::PostMessage, __construct_array (the 29 header constructors), CMMDefault::CMMDefault,
CMouse::AssignMouseMode, the mode's GetMode through __ptr_glue, LLCC_SystemSetMode, SCR_GetScriptPosition, CTRACE) are fakes on both
sides that record their arguments and answer from the case.

The world of a case: the selected person (kind byte), the pointed thing (kind byte, script handle), the world, message centre, mouse,
mouse mode (a fake whose GetMode answers from a queue) and the two mouse words. What is compared: the answer, the globals the functions
may write (mouse, message centre, the two words, as labels) and the event log (CTRACE's format as a checksum and its three doubles as
words).

No fused operations. Not tested: a NULL game world in start_strike (the original reads through it unconditionally), operator new
returning NULL (it never does).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_clear_patrol_area"
MASK = 0xFFFFFFFF

FN = {"clear_patrol_area": 0x1014CFE4, "wheres_my_script": 0x1014D114, "start_strike": 0x1014CEAC, "abort_all_requesters": 0x1014CB8C}
SIZES = {0x1014CFE4: 264, 0x1014D114: 388, 0x1014CEAC: 276, 0x1014CB8C: 540}
OWN = set(SIZES)

ADDR = dict(ui=0x10396440, intr=0x102DC1AC, mw0=0x102DC1E0, mw1=0x102DC1E4, mmode=0x102DC1F0, mouse=0x102DC1F4, world=0x1037EBFC,
            centre=0x1037F6C4)
HOST_LAYOUT = [16, 0x1D0]
OBJS = ["world", "hq", "peep", "mouse", "fakemode", "thing", "centre"]
LABELS = {name: i + 2 for i, name in enumerate(OBJS)}


def s32(v):
    return T.s32(v)


def u8s(b):
    return b.encode() if isinstance(b, str) else bytes(b)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def q_ret(key):
    return lambda e, log: e.cl.ret(key) & MASK


STUBS.internal(0x101EFF4C, CL.recorder("GetSelectedPeep", "L", ret=q_ret("peep")))
STUBS.internal(0x101F0340, CL.recorder("RemoveLocators", "L", ret=q_ret("RemoveLocators")))
STUBS.internal(0x10127A24, CL.recorder("CancelPatrolArea", "L"))
STUBS.internal(0x1013E99C, CL.recorder("GetStaffHQ", "L", ret=q_ret("hq")))
STUBS.internal(0x1012C04C, CL.recorder("ForceStrike", "Li"))
STUBS.internal(0x1007EA78, CL.recorder("CMMDefault", "N"))
STUBS.internal(0x1007E2E0, CL.recorder("AssignMouseMode", "LL"))
STUBS.internal(0x10090E18, CL.recorder("LLCC_SystemSetMode", "ib"))


def stub_msg_ctor(e):
    L = e.cl
    L.rec("MsgEOM", (L.lab(e.r(3)),))
    e.write(e.r(3) + 4, struct.pack(">I", 0x55))


def stub_post(e):
    L = e.cl
    L.rec("PostMessage", (L.lab(e.r(3)), L.lab(e.r(4)), e.u32(e.r(4) + 4)))
    e.set_r(3, 0)


def stub_scr(e):
    L = e.cl
    L.rec("SCR_GetScriptPosition", (e.r(3) & MASK,))
    for reg, key in ((4, "px"), (5, "py"), (6, "pz"), (7, "pm")):
        e.write(e.r(reg), struct.pack(">I", L.ret(key) & MASK))


def stub_ctrace(e):
    L = e.cl
    text = u8s(e.cstr(e.r(4)))
    total = sum(c * (i + 1) for i, c in enumerate(text))
    ws = [e.r(r) & MASK for r in (5, 6, 7, 8, 9, 10)]
    L.rec("CTRACE", (e.r(3) & MASK, total, *ws, e.u32(e.r(1) + 0x38)))


def stub_construct_array(e):
    assert (e.r(4), e.r(5), e.r(6), e.r(7)) == (0x102657E8, 0x10267288, 0x10, 0x1D), "construct_array arguments"
    for _ in range(0x1D):
        e.cl.rec("hdr_ctor", ())


def stub_glue(e):
    """__ptr_glue calling the mode's GetMode: r3 the mode; the caller reloads r2 from 0x14(r1)."""
    L = e.cl
    L.rec("GetMode", (L.lab(e.r(3)),))
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
    e.set_r(3, L.ret("GetMode") & MASK)


STUBS.internal(0x10151C6C, stub_msg_ctor)
STUBS.internal(0x1015256C, stub_post)
STUBS.internal(0x100D93B8, stub_scr)
STUBS.internal(0x1006FCBC, stub_ctrace)
STUBS.internal(0x10238758, stub_construct_array)
STUBS.internal(0x1023BAA8, stub_glue)


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

    def ptr(self, a, v):
        """A pointer global: 4 bytes on the Mac, 8 on the host (arena addresses do not fit 32 bits)."""
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def queues(self, case):
        q = {k: list(v) for k, v in case["queues"].items()}
        q["peep"] = [self.obj["peep"] if case["peep"] else 0]
        q["hq"] = [self.obj["hq"]]
        return q

    def label(self, w):
        for name, a in self.obj.items():
            if a == w:
                return name
        return 0 if w == 0 else "new"


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.obj = {n: e.alloc(0x40) for n in OBJS}
        vt, tv = e.alloc(0x40), e.alloc(8)
        e.write(self.obj["fakemode"], struct.pack(">I", vt))
        e.write(vt + 0x2C, struct.pack(">I", tv))
        fixed = {ADDR["ui"]: 1}
        fixed.update({a: LABELS[n] for n, a in self.obj.items()})
        self.L = e.cl = CL.PpcLog(fixed, self.queues(case))
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op):
        r3, _, _ = T.call_ppc(self.e, FN[op], [])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_cpa_reset()
        self.obj = {n: lib.tpw_cpa_alloc(0x40) for n in OBJS}
        self.obj["fakemode"] = lib.tpw_cpa_fakemode()
        fixed = {1: self.g("ui")}
        fixed.update({LABELS[n]: a for n, a in self.obj.items()})
        CL.host_begin(lib, fixed, self.queues(case))

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cpa_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        return getattr(self.lib, "tpw_cpa_" + op)() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cpa_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "fakemode").restype, getattr(lib, p + "fakemode").argtypes = C.c_uint64, []
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    for op in FN:
        getattr(lib, p + op).restype, getattr(lib, p + op).argtypes = C.c_uint32, []
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o = s.obj
    s.ptr(s.g("world"), o["world"])
    s.ptr(s.g("centre"), o["centre"] if case["centre"] else 0)
    s.ptr(s.g("mouse"), o["mouse"] if case["gmouse"] else 0)
    s.ptr(s.g("mmode"), o["fakemode"] if case["mode"] else 0)
    s.words(s.g("mw0"), [case["w0"]])
    s.words(s.g("mw1"), [case["w1"]])
    s.ptr(s.g("intr"), o["thing"] if case["thing"] else 0)
    s.write(o["thing"] + 2, bytes([case["tkind"]]))
    s.words(o["thing"] + 0xC, [case["handle"]])
    s.write(o["peep"] + 2, bytes([case["kind"]]))


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "mouse": s.label(s.rd_ptr(s.g("mouse"))), "mmode": s.label(s.rd_ptr(s.g("mmode"))),
            "centre": s.label(s.rd_ptr(s.g("centre"))), "w0": s.rd_word(s.g("mw0")), "w1": s.rd_word(s.g("mw1"))}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-8, 8))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def mk(op, **kw):
    c = dict(op=op, peep=0, kind=0, thing=0, tkind=0, handle=0, gmouse=0, mode=0, centre=0, w0=0, w1=0, queues={})
    c.update(kw)
    return c


# clear_patrol_area
def cpa_edges():
    for kind in list(range(0, 0x14)) + [0x80, 0xFF]:
        for peep in (1, 0):
            yield mk("clear_patrol_area", peep=peep, kind=kind)


def cpa_random(r):
    return mk("clear_patrol_area", peep=r.choice([0, 1, 1]), kind=r.choice([r.randrange(256), r.randrange(0, 10)]))


# wheres_my_script
def wms_case(r, thing, tkind):
    return mk("wheres_my_script", thing=thing, tkind=tkind, handle=r.choice([0, 1, MASK, r.randrange(1 << 32)]),
              queues={"px": [rf(r)], "py": [rf(r)], "pz": [rf(r)], "pm": [r.choice([0, 1, MASK, r.randrange(1 << 32)])]})


def wms_edges():
    r = random.Random(5341)
    yield wms_case(r, 0, 1)
    for tkind in list(range(0, 0x15)) + [0x80, 0xFF]:
        yield wms_case(r, 1, tkind)


def wms_random(r):
    return wms_case(r, r.choice([0, 1, 1]), r.choice([r.randrange(256), r.choice([1, 4, 5, 6, 7, 8, 0x12])]))


# start_strike
def ss_edges():
    for centre in (0, 1):
        yield mk("start_strike", centre=centre)


def ss_random(r):
    return mk("start_strike", centre=r.choice([0, 1]))


# abort_all_requesters
def aar_case(r, rl, gmouse, mode, a, b):
    return mk("abort_all_requesters", gmouse=gmouse, mode=mode, w0=r.randrange(1 << 32), w1=r.randrange(1 << 32),
              queues={"RemoveLocators": [rl], "GetMode": [a, b]})


def aar_edges():
    r = random.Random(7)
    for rl in (1, -1, 0):
        yield aar_case(r, rl, 0, 1, 2, 2)
    for gmouse in (0, 1):
        for mode in (0, 1):
            for a, b in ((0, 0), (0, 5), (1, 1), (1, 0), (2, 1), (2, 2), (2, 0), (-1, 1), (-1, -1), (5, 1)):
                yield aar_case(r, 0, gmouse, mode, a, b)


def aar_random(r):
    return aar_case(r, r.choice([0, 0, 0, 1, -3]), r.choice([0, 1]), r.choice([0, 1, 1]), r.choice([0, 1, 2, -1, 9]),
                    r.choice([0, 1, 1, 2, 7]))


TESTS = [
    FnTest("clear_patrol_area", ppc, host, edge=cpa_edges, rnd=cpa_random),
    FnTest("wheres_my_script", ppc, host, edge=wms_edges, rnd=wms_random),
    FnTest("start_strike", ppc, host, edge=ss_edges, rnd=ss_random),
    FnTest("abort_all_requesters", ppc, host, edge=aar_edges, rnd=aar_random),
]
