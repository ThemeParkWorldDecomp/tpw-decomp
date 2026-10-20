"""Unit v5707, the coaster editor's button loops and the pylon type, against libtpw_unit_UIHandleEndLoop.dylib (tests/unit_UIHandleEndLoop.cpp), main app
("SimTheme Park"), src/LayoutSetRotatePylon_DeleteLastTrackBrace_UIHandleEndLoop.cpp:
  UIHandleEndLoop 0x100565cc, UIHandleMidLoop 0x10056548, SetPylonType 0x100562d4

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory at the Mac's addresses, big-endian; the host
library's memory, little-endian, with the host's own offsets and pointer width). The callees outside the unit (GetCoasterEditAvailableButtons,
SetCoasterButtonState, UIInterface::ChangeCoasterButtonsEnabled / Down, CActionRecorder::RecordAction, SetPotentialPylonArea) are fakes on both
sides recording their arguments.

The world of a case: the editor's coaster (flags, type word, the +0x3c block, the first brace and the preview brace with their flags), the globals
(button masks, the three pylon words). What is compared: the event log, the three pylon words, the editor coaster's flags.

No fused operations. Not tested: a NULL editor coaster (the original reads its flags unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UIHandleEndLoop"
MASK = 0xFFFFFFFF

FN_END, FN_MID, FN_PYLON = 0x100565CC, 0x10056548, 0x100562D4
SIZES = {FN_END: 108, FN_MID: 92, FN_PYLON: 436}
OWN = set(SIZES)

C_GETBTN, C_SETBTN, C_ENABLED, C_DOWN, C_RECORD, C_AREA = 0x10057278, 0x10057064, 0x101F0F94, 0x101F0F2C, 0x10143F48, 0x10041A58

ADDR = dict(edit=0x102B68B0, enabled=0x102C16D4, down=0x102C16D8, w16dc=0x102C16DC, w1790=0x102C1790, input=0x102C1794,
            rec=0x1037EC44, ui=0x10396440)
PPC_OFF = dict(flags=0, type=4, b3c=0x3C, braces=0x58, prev=0x60, size=0x200, bflags=8, ptr=4)
HOST_OFF = {}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk(name, nregs=()):
    def stub(e):
        e.cl.rec(name, tuple(f(e) for f in nregs))
    return stub


STUBS.internal(C_GETBTN, lambda e: e.cl.rec("GetCoasterEditAvailableButtons", (e.cl.lab(e.r(3)),)))
STUBS.internal(C_SETBTN, lambda e: e.cl.rec("SetCoasterButtonState", (e.r(3) & MASK, e.r(4) & MASK)))
STUBS.internal(C_ENABLED, lambda e: e.cl.rec("ChangeCoasterButtonsEnabled", (e.cl.lab(e.r(3)), e.r(4) & MASK)))
STUBS.internal(C_DOWN, lambda e: e.cl.rec("ChangeCoasterButtonsDown", (e.cl.lab(e.r(3)), e.r(4) & MASK)))
STUBS.internal(C_RECORD, lambda e: e.cl.rec("RecordAction", (e.cl.lab(e.r(3)), T.s32(e.r(4)))))
STUBS.internal(C_AREA, lambda e: e.cl.rec("SetPotentialPylonArea", ()))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def wptr(self, a, p):
        self.write(a, struct.pack(self.pfmt, p))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    off = PPC_OFF

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["ui"]: 1, ADDR["rec"]: 2}, {})
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

    def call(self, op, args):
        entry = {"end": FN_END, "mid": FN_MID, "pylon": FN_PYLON}[op]
        T.call_ppc(self.e, entry, [a & MASK for a in args])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = HOST_OFF
        CL.host_begin(lib, {1: self.g("ui"), 2: self.g("rec")}, {})

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_u7_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_u7_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        if op == "end":
            self.lib.tpw_u7_end()
        elif op == "mid":
            self.lib.tpw_u7_mid()
        else:
            self.lib.tpw_u7_pylon(args[0] & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_u7_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_u7_pylon.argtypes = [C.c_uint32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    v = list(out)[:n]
    HOST_OFF.update(flags=v[0], type=v[1], b3c=v[2], braces=v[3], prev=v[4], size=v[5], bflags=v[6], ptr=8)
    assert v[0] == 0 and v[6] == 8, v
    lib.bound = True


def build(s, case):
    o = s.off
    s.words(s.g("enabled"), [case["enabled"]])
    s.words(s.g("down"), [case["down"]])
    s.words(s.g("w16dc"), [case["w16dc"]])
    s.words(s.g("w1790"), [case["w1790"]])
    s.words(s.g("input"), [case["input"]])
    inst = s.alloc(o["size"])
    typ = s.alloc(16)
    s.words(typ, [case["tflag"]])
    br = s.alloc(32)
    pv = s.alloc(32)
    s.words(br + o["bflags"], [case["bf"]])
    s.words(pv + o["bflags"], [case["pf"]])
    s.words(inst + o["flags"], [case["flags"]])
    s.wptr(inst + o["type"], typ)
    s.words(inst + o["b3c"], [case["b3c"]])
    s.wptr(inst + o["braces"], br)
    s.wptr(inst + o["prev"], pv)
    s.wptr(s.g("edit"), inst)
    s.inst = inst


def observe(s, case):
    return {"log": s.log(), "w16dc": s.rd_words(s.g("w16dc"), 1), "w1790": s.rd_words(s.g("w1790"), 1),
            "input": s.rd_words(s.g("input"), 1), "flags": s.rd_words(s.inst + s.off["flags"], 1)}


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"], case["args"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mkcase(r, op, args, **kw):
    c = {"op": op, "args": args, "enabled": r.getrandbits(32), "down": r.getrandbits(32), "w16dc": r.choice([0, 1, 2, 3, r.getrandbits(32)]),
         "w1790": r.choice([0, 1, 2, 3]), "input": r.choice([0, 0x40, r.getrandbits(32)]), "flags": r.choice([0, 0x2000, r.getrandbits(32)]),
         "tflag": r.choice([0, 2, 0xFD, r.getrandbits(32)]), "b3c": r.choice([0, 1, 2, 3, r.getrandbits(32)]),
         "bf": r.choice([0, 0x80, r.getrandbits(32)]), "pf": r.choice([0, 0x80, r.getrandbits(32)])}
    c.update(kw)
    return c


def loop_edges(op):
    def gen():
        r = random.Random(5)
        for _ in range(8):
            yield mkcase(r, op, [])
    return gen


def pylon_edges():
    r = random.Random(7)
    for typ in (2, 5):
        for tflag in (0, 2):
            for b3c in range(4):
                for bf in (0, 0x80):
                    for pf in (0, 0x80):
                        for inp in (0, 0x40):
                            for w16 in (2, 0):
                                for w17 in (2, 0):
                                    yield mkcase(r, "pylon", [typ], tflag=tflag, b3c=b3c, bf=bf, pf=pf, input=inp, w16dc=w16, w1790=w17)


def rnd_pylon(r):
    return mkcase(r, "pylon", [r.choice([2, 2, 0, 1, 3, r.getrandbits(32)])])


TESTS = [
    FnTest("UIHandleEndLoop", ppc, host, edge=loop_edges("end"), rnd=lambda r: mkcase(r, "end", [])),
    FnTest("UIHandleMidLoop", ppc, host, edge=loop_edges("mid"), rnd=lambda r: mkcase(r, "mid", [])),
    FnTest("SetPylonType", ppc, host, edge=pylon_edges, rnd=rnd_pylon),
]
