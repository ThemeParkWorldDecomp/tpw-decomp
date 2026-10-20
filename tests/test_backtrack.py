"""Unit v5545, the backtrack key, against libtpw_unit_backtrack.dylib (tests/unit_backtrack.cpp), main app ("SimTheme Park"),
src/turn_object_counterclockwise_TestAdvisor3_backtrack.cpp:
  backtrack 0x1014c010

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the mouse mode
constructor and AssignMouseMode, the mouse mode's GetMode virtual, LLCC_SystemIsMode / GetMode / SetMode / Backtrack, CMapCell::IsPath,
LLCM_LeftMouseButtonUp, SOUND_SendEvent) are fakes on both sides that record their arguments and answer from the case.

The world of a case: gMouse (null or set), the two mouse words, gMouseMode (null or an object whose virtual GetMode answers from the case),
the selected cell id (gIntrface +0x24), the sound banks, gGameWorld. What is compared: the answer, gMouse set or not, the two mouse
words, the event log (the cell address as an offset from the world).

No fused operations; the float conversions are exact for the cell coordinates. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_backtrack"
MASK = 0xFFFFFFFF

FN = 0x1014C010
OWN = {FN}

ADDR = dict(gIntrface=0x102DC1AC, w1e0=0x102DC1E0, w1e4=0x102DC1E4, mouseMode=0x102DC1F0, mouse=0x102DC1F4,
            world=0x1037EBFC, banks=0x102F7C94)
HOST_LAYOUT = [48]

CUR = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)

STUBS.internal(0x1007EA78, CL.recorder("CMMDefault", "N"))
STUBS.internal(0x1007E2E0, CL.recorder("AssignMouseMode", "LL"))
STUBS.internal(0x10091568, CL.recorder("IsMode", "i", ret="i"))
STUBS.internal(0x100914F4, CL.recorder("SysGetMode", "", ret="i"))
STUBS.internal(0x100DD9FC, CL.recorder("SendEvent", "iiiiii"))
STUBS.internal(0x10090E18, CL.recorder("SetMode", "ib"))
STUBS.internal(0x10084CA4, CL.recorder("LeftUp", "iiHi"))
STUBS.internal(0x10092280, CL.recorder("Backtrack", "Lb"))


def stub_ispath(e):
    e.cl.rec("IsPath", (s32(e.r(3) - CUR["world"]),))
    e.set_r(3, e.cl.ret("IsPath") & MASK)


def stub_ptr_glue(e):
    """The virtual call through the TVector in r12: GetMode of the case's mode object."""
    assert e.r(12) == CUR["desc"] and e.r(3) == CUR["obj"], (hex(e.r(12)), hex(e.r(3)))
    e.cl.rec("ModeGetMode", ())
    e.set_r(3, e.cl.ret("ModeGetMode") & MASK)


STUBS.internal(0x1009C44C, stub_ispath)
STUBS.internal(0x1023BAA8, stub_ptr_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    f24 = 0x24

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    def is_set(self, a):
        return int(any(self.read(a, 4)))

    def mode_obj(self):
        vt = self.alloc(0x40)
        desc = self.alloc(8)
        obj = self.alloc(8)
        self.words(desc, [0x10000000, 0x10260000])
        self.words(vt + 0x2C, [desc])
        self.words(obj, [vt])
        CUR["desc"], CUR["obj"] = desc, obj
        return obj

    def log(self):
        return self.L.log

    def call(self):
        r3, _, _ = T.call_ppc(self.e, FN, [])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    f24 = 48       # the host's CIntrface has native pointers in front of mF24

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bt_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_bt_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def is_set(self, a):
        return int(any(self.read(a, 8)))

    def mode_obj(self):
        return self.lib.tpw_bt_addr(b"modeObj")

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        return self.lib.tpw_bt_backtrack() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bt_addr.restype, lib.tpw_bt_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bt_alloc.restype, lib.tpw_bt_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bt_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bt_backtrack.restype, lib.tpw_bt_backtrack.argtypes = C.c_uint32, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_bt_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    world = s.alloc(0x400)
    CUR["world"] = world
    s.ptr(s.g("world"), world)
    s.ptr(s.g("mouse"), s.alloc(16) if case["mouse"] else 0)
    s.ptr(s.g("mouseMode"), s.mode_obj() if case["mode"] else 0)
    s.words(s.g("w1e0"), [case["w1e0"]])
    s.words(s.g("w1e4"), [case["w1e4"]])
    s.write(s.g("gIntrface") + s.f24, struct.pack(s.fmt[0] + "H", case["id"]))
    s.words(s.g("banks"), case["banks"])


def execute(s, case):
    return s.call()


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "mouse": s.is_set(s.g("mouse")), "w1e0": s.rd_words(s.g("w1e0"), 1),
            "w1e4": s.rd_words(s.g("w1e4"), 1)}


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


def mk(r, mouse, mode, modeval, ismode, ispath, cid):
    return {"mouse": mouse, "mode": mode, "id": cid, "w1e0": r.getrandbits(32), "w1e4": r.getrandbits(32),
            "banks": [r.getrandbits(32) for _ in range(8)],
            "queues": {"IsMode": [ismode], "ModeGetMode": [modeval & MASK], "IsPath": [ispath],
                       "SysGetMode": [r.getrandbits(32)]}}


def edges():
    r = random.Random(5545)
    for mouse in (0, 1):
        for mode, mv in ((0, 0), (1, 0), (1, 1), (1, 2), (1, 3), (1, 4), (1, -1)):
            for ismode in (0, 1):
                for ispath in (0, 1):
                    yield mk(r, mouse, mode, mv, ismode, ispath, r.choice([1, 2, 5, 128, 129, 0x3FFF]))
    for cid in (0, 1, 127, 128, 129, 255, 256, 0x4000, 0xFFFF):
        yield mk(r, 1, 1, 0, 1, 1, cid)


def rnd(r):
    return mk(r, r.choice([0, 1]), r.choice([0, 1, 1]), r.choice([0, 1, 2, 3, 3, 4, -1]), r.choice([0, 1, 1]),
              r.choice([0, 1]), r.choice([r.randrange(0, 0x10000), r.randrange(0, 0x4000)]))


TESTS = [
    FnTest("backtrack", ppc, host, edge=edges, rnd=rnd),
]
