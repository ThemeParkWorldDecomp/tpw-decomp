"""Unit v5144, the editor's pylon drag toggles, against libtpw_unit_ToggleLoopTurnPylonState.dylib (tests/unit_ToggleLoopTurnPylonState.cpp),
main app ("SimTheme Park"), src/game/rides/CoasterEdit.cpp:
  ToggleLoopTurnPylonState 0x10052e5c, ToggleRotatePylonState 0x100530a8, ToggleBankInclinePylonState 0x100532f4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments: CActionRecorder::RecordAction (the float overloads at 0x10144704 and
0x10144868) and LockPointedAtRide (0x10067288).

The world of a case: the input flags word, the editor instance (flags, the word at 0x40), the dragged brace (number, flags, the angles and
wobbles, its pylon record with flags, its control with flags and the angle at 0x94), the pointer position, and a random fill of every
global the functions write. What is compared: the globals written (floats, pointers as the brace field they point at), the instance's and
brace's words, the event log, and on the PowerPC side that r3 is 1 (the host functions are void in the header).

No fused operations (fsubs, fadds only). Not tested: a NULL brace when the drag flag is set (the original reads through it
unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ToggleLoopTurnPylonState"
MASK = 0xFFFFFFFF

FN_LOOP, FN_ROT, FN_BANK = 0x10052E5C, 0x100530A8, 0x100532F4
SIZES = {FN_LOOP: 540, FN_ROT: 540, FN_BANK: 620}
OWN = set(SIZES)

REC4 = 0x10144868
REC3 = 0x10144704
LOCK = 0x10067288

ADDR = dict(
    gCoasterEditInstance=0x102B68B0, gCoasterInputFlags=0x102C1794, gCoasterDragBrace=0x102C178C, gPointerScreenXY=0x102B5A94,
    gCoasterDragLastDX=0x102C16E4, gCoasterDragLastDY=0x102C16E8, gCoasterDragAnchor=0x102C16EC, gCoasterDragScaleX=0x102C16F4,
    gCoasterDragScaleY=0x102C16F8, gCoasterHorizontalBase2=0x102C1714, gCoasterHorizontalBase1=0x102C1718,
    gCoasterHorizontalMax1=0x102C1720, gCoasterHorizontalMin1=0x102C1728, gCoasterVerticalBase1=0x102C1748,
    gCoasterVerticalMax1=0x102C1750, gCoasterVerticalMin1=0x102C1758, gCoasterHorizontalTarget2=0x102C175C,
    gCoasterHorizontalTarget1=0x102C1760, gCoasterVerticalTarget2=0x102C1764, gCoasterVerticalTarget1=0x102C1768,
    gActionRecorder=0x1037EC44,
)
FLOAT_GLOBALS = ["gCoasterDragLastDX", "gCoasterDragLastDY", "gCoasterDragScaleX", "gCoasterDragScaleY", "gCoasterHorizontalBase2",
                 "gCoasterHorizontalBase1", "gCoasterHorizontalMax1", "gCoasterHorizontalMin1", "gCoasterVerticalBase1",
                 "gCoasterVerticalMax1", "gCoasterVerticalMin1"]
PTR_GLOBALS = ["gCoasterHorizontalTarget2", "gCoasterHorizontalTarget1", "gCoasterVerticalTarget2", "gCoasterVerticalTarget1"]

OFF_KEYS = ["inst_flags", "inst_f40", "br_number", "br_flags", "br_fd4", "br_angle", "br_fd8", "br_fdc", "br_ff8", "br_data",
            "dt_flags", "dt_control", "ct_flags", "ct_ang8"]
MAC_OFF = dict(zip(OFF_KEYS, [0, 0x40, 0, 8, 0xD4, 0xE0, 0xD8, 0xDC, 0xF8, 0x100, 4, 0x18, 0, 0x94]))
BRACE_FIELDS = ["br_number", "br_flags", "br_fd4", "br_angle", "br_fd8", "br_fdc", "br_ff8"]
HOST_OFF = {}


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


def stub_rec3(e):
    """RecordAction(RecordedAction, float, unsigned short): the float takes r5's slot, the id is in r6."""
    L = e.cl
    L.rec("RecordAction3", (L.lab(e.r(3)), s32(e.r(4)), canon_f(fb(e.f(1))), e.r(6) & 0xFFFF))


def stub_rec4(e):
    """RecordAction(RecordedAction, float, float, unsigned short): the floats take r5 and r6, the id is in r7."""
    L = e.cl
    L.rec("RecordAction4", (L.lab(e.r(3)), s32(e.r(4)), canon_f(fb(e.f(1))), canon_f(fb(e.f(2))), e.r(7) & 0xFFFF))


def stub_lock(e):
    e.cl.rec("LockPointedAtRide", (s32(e.r(3)),))


STUBS.internal(REC3, stub_rec3)
STUBS.internal(REC4, stub_rec4)
STUBS.internal(LOCK, stub_lock)


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

    def put(self, base, key, v):
        self.words(base + self.off[key], [v])

    def get(self, base, key):
        return self.rd_word(base + self.off[key])


class Ppc(Side):
    fmt = ">I"
    off = MAC_OFF

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["gActionRecorder"]: 1}, case["queues"])
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

    def rd_ptr(self, a):
        return self.rd_word(a)

    def log(self):
        return self.L.log

    def call(self, fn):
        r3, _, _ = T.call_ppc(self.e, fn, [])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = HOST_OFF
        CL.host_begin(lib, {1: self.g("gActionRecorder")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_tlp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_tlp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn):
        name = {FN_LOOP: "loop", FN_ROT: "rotate", FN_BANK: "bank"}[fn]
        return getattr(self.lib, "tpw_tlp_" + name)() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_tlp_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    for n in ("loop", "rotate", "bank"):
        getattr(lib, p + n).restype, getattr(lib, p + n).argtypes = C.c_uint32, []
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    vals = list(out)[:n]
    assert n == len(OFF_KEYS) + 3, vals
    for k, v in zip(OFF_KEYS, vals):
        HOST_OFF[k] = v
    assert vals[len(OFF_KEYS)] <= 0x200 and vals[len(OFF_KEYS) + 1] <= 0x400 and vals[len(OFF_KEYS) + 2] <= 0x200, vals
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    for name, v in case["pre"].items():
        if name in PTR_GLOBALS:
            s.ptr(s.g(name), v)
        elif name == "gCoasterDragAnchor":
            s.words(s.g(name), v)
        else:
            s.words(s.g(name), [v])
    s.words(s.g("gPointerScreenXY"), case["pxy"])
    s.words(s.g("gCoasterInputFlags"), [case["flags"]])
    s.inst = 0
    if case["inst"] is not None:
        s.inst = s.alloc(0x200)
        s.put(s.inst, "inst_flags", case["inst"][0])
        s.put(s.inst, "inst_f40", case["inst"][1])
    s.ptr(s.g("gCoasterEditInstance"), s.inst)
    s.brace = 0
    b = case["brace"]
    if b is not None:
        s.brace = s.alloc(0x400)
        data = s.alloc(0x80)
        ctl = s.alloc(0x200)
        for k in BRACE_FIELDS:
            s.put(s.brace, k, b[k])
        s.ptr(s.brace + s.off["br_data"], data)
        s.put(data, "dt_flags", b["dflags"])
        s.ptr(data + s.off["dt_control"], ctl)
        s.put(ctl, "ct_flags", b["cflags"])
        s.put(ctl, "ct_ang8", b["ang8"])
    s.ptr(s.g("gCoasterDragBrace"), s.brace)


def ptr_name(s, p):
    if s.brace and 0 <= p - s.brace < 0x400:
        inv = {v: k for k, v in s.off.items() if k.startswith("br_")}
        return ("brace", inv.get(p - s.brace, p - s.brace))
    return ("raw", p)


def observe(s, case, answer):
    o = {"answer": answer, "log": s.log(), "flags": s.rd_word(s.g("gCoasterInputFlags")),
         "anchor": [canon_f(w) for w in s.rd_words(s.g("gCoasterDragAnchor"), 2)]}
    for n in FLOAT_GLOBALS:
        o[n] = canon_f(s.rd_word(s.g(n)))
    for n in PTR_GLOBALS:
        o[n] = ptr_name(s, s.rd_ptr(s.g(n)))
    if s.inst:
        o["inst"] = [s.get(s.inst, "inst_flags"), s.get(s.inst, "inst_f40")]
    if s.brace:
        o["brace"] = [canon_f(s.get(s.brace, k)) if k not in ("br_number", "br_flags") else s.get(s.brace, k) for k in BRACE_FIELDS]
    return o


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["fn"]) if isinstance(s, Ppc) else s.call(case["fn"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

FLOATS = [0.0, 0.5, 0.5000001, 0.4999999, 0.75, 0.25, 1.5, -0.25, -1.0, 2.0, 3.14]


def rfl(r):
    if r.random() < 0.5:
        return fb(r.choice(FLOATS))
    return fb(r.uniform(-8, 8))


def make_brace(r, cflags=None, dflags=None, bflags=None, ff8=None):
    return {
        "number": r.choice([0, 1, 0x12345, 0xFFFF, 0x10000, r.getrandbits(32)]),
        "flags": r.getrandbits(32) if bflags is None else bflags,
        "fd4": rfl(r), "angle": rfl(r), "fd8": rfl(r), "fdc": rfl(r),
        "ff8": rfl(r) if ff8 is None else fb(ff8),
        "dflags": r.choice([0, 2, 4, 1, 3, r.getrandbits(32)]) if dflags is None else dflags,
        "cflags": r.getrandbits(32) if cflags is None else cflags,
        "ang8": rfl(r),
    }


def finish(r, fn, flags, brace, inst):
    """Packs a case; the dragged brace exists whenever the original dereferences it."""
    low = 2 if fn == FN_ROT else 4
    if (flags & low) and brace is None:
        brace = make_brace(r)
    pre = {n: r.getrandbits(32) for n in FLOAT_GLOBALS}
    pre["gCoasterDragAnchor"] = [r.getrandbits(32), r.getrandbits(32)]
    for n in PTR_GLOBALS:
        pre[n] = 0x1000 + 4 * r.randrange(4)
    b = None
    if brace is not None:
        b = {"number": brace["number"], "br_number": brace["number"], "br_flags": brace["flags"], "br_fd4": brace["fd4"],
             "br_angle": brace["angle"], "br_fd8": brace["fd8"], "br_fdc": brace["fdc"], "br_ff8": brace["ff8"],
             "dflags": brace["dflags"], "cflags": brace["cflags"], "ang8": brace["ang8"]}
    return {"fn": fn, "queues": {}, "flags": flags & MASK, "brace": b, "inst": inst, "pre": pre,
            "pxy": [rfl(r), rfl(r)]}


def rflags(r):
    k = r.random()
    if k < 0.15:
        return 0
    if k < 0.5:
        return r.getrandbits(6) | r.choice([0, 0x200, 0x400, 0x2000])
    if k < 0.7:
        return r.choice([2, 4, 6, 0x2004, 0x2002, 0x606, 0x2000, 0x200, 0x400])
    return r.getrandbits(32)


def rinst(r, fn):
    if fn == FN_ROT and r.random() < 0.2:
        return None if r.random() < 0.5 else (r.getrandbits(32), 0)
    return (r.getrandbits(32) | r.choice([0, 0x2000]), r.choice([1, 0x40, r.getrandbits(32) | 1]))


def random_for(fn):
    def gen(r):
        flags = rflags(r)
        brace = None if r.random() < 0.2 else make_brace(r)
        if brace and r.random() < 0.5:
            brace["cflags"] |= r.choice([0x200, 0x40, 0x100, 0x80, 0x180, 0x1C0])
        return finish(r, fn, flags, brace, rinst(r, fn))
    return gen


def edges_for(fn):
    def gen():
        r = random.Random(fn)
        for flags in (0, 1, 2, 4, 6, 8, 0x3F, 0x2004, 0x2002, 0x2000, 0x606, 0x14, 0x12):
            for cflags in (0, 0x200, 0x40, 0x100, 0x80, 0x180, 0x1C0, 0xFFFFFFFF):
                for dflags in (0, 2):
                    for isnull in (False, True):
                        for ff8, bflags in ((0.5, 0x80), (0.75, 0x80), (0.75, 0), (0.4999999, 0x80), (0.5000001, 0x80)):
                            if flags & 0x3F and not isnull and (ff8, bflags) != (0.75, 0x80) and r.random() < 0.7:
                                continue
                            brace = None if isnull else make_brace(r, cflags, dflags, bflags | r.choice([0, 0x40, 1, 6]), ff8)
                            for inst in ((0x2000 | 0x10, 0x40), (0xFFFFFFFF, 0), None) if fn == FN_ROT else ((0x2010, 1),):
                                yield finish(r, fn, flags, brace, inst)
    return gen


TESTS = [
    FnTest("ToggleLoopTurnPylonState", ppc, host, edge=edges_for(FN_LOOP), rnd=random_for(FN_LOOP)),
    FnTest("ToggleRotatePylonState", ppc, host, edge=edges_for(FN_ROT), rnd=random_for(FN_ROT)),
    FnTest("ToggleBankInclinePylonState", ppc, host, edge=edges_for(FN_BANK), rnd=random_for(FN_BANK)),
]
