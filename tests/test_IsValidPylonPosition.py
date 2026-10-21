"""Unit v5551, IsValidPylonPosition, against libtpw_unit_IsValidPylonPosition.dylib (tests/unit_IsValidPylonPosition.cpp), main app
("SimTheme Park"), src/game/rides/CoasterPylon.cpp:
  IsValidPylonPosition 0x100411e4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers, the host's offsets from the library's layout export)
for the translation; the same code reads both back. The callee outside the unit, XZAngleBetweenPylons (0x10041018), is a fake on both sides
that records its six arguments and answers float bits from the case's queue; sqrt is the C library's on both sides; the function itself
runs as the original where it calls itself.

The world of a case: the height field (header, field, copy with the quads' low bits), the editor's coaster and two others on the list, the
coaster type with its two pylon controls, six braces and six pylon records linked at random (the record lists, the instance, the control
and the brace of each), the dragged brace, the input flags and the clamp bounds. What is compared: the answer and the event log (the
function writes nothing else).

Fused operations: the lerp of the angle bounds is two fmadds (HardwareRounding over the function). Not tested: a brace without next brace
reaching the angle call when the pointer is over a brace (the original passes two registers it never set; the translation passes zero).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_IsValidPylonPosition"
MASK = 0xFFFFFFFF

FN = 0x100411E4
SIZES = {FN: 2108}
OWN = set(SIZES)

XZ = 0x10041018

ADDR = dict(edit=0x102B68B0, insts=0x102B68B4, mv=0x102C177C, w1790=0x102C1790, inflags=0x102C1794, hhdr=0x102C6C7C, hcopy=0x102C6C80,
            cmax=0x1029CD48, cmin=0x1029CD4C)

# the Mac's offsets and sizes; the host's come from tpw_ivp_layout in the same order
NAMES = ["inst.mNext", "inst.mBlock3C", "inst.mType", "inst.mPylons", "inst.mLastBrace", "inst.mBraces", "inst.mPreview", "inst.mBrace64",
         "inst.size",
         "brace.mNumber", "brace.mFlags", "brace.mPrev", "brace.mNext", "brace.mData", "brace.size",
         "pyl.mFlags", "pyl.mIndex", "pyl.mCellX", "pyl.mCellZ", "pyl.mInstance", "pyl.mControl", "pyl.mNext", "pyl.mCount", "pyl.mBraces",
         "pyl.size",
         "type.mPylonsA", "type.mPylonsB", "type.size",
         "ctl.mF9C", "ctl.mFA0", "ctl.mFA4", "ctl.mFA8", "ctl.mAngles", "ctl.mFCC"]
MAC = dict(zip(NAMES, [0x18, 0x3C, 0x04, 0x4C, 0x54, 0x58, 0x60, 0x64, 0x150,
                       0x00, 0x08, 0x64, 0x68, 0x100, 0x110,
                       0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x28, 0x30, 0x34, 0x48,
                       0xA8, 0x1C8, 0x400,
                       0x9C, 0xA0, 0xA4, 0xA8, 0x74, 0xCC]))


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_xz(e):
    """float XZAngleBetweenPylons(x1, z1, x2, z2, x3, z3): records the six words, answers the queue's float bits."""
    L = e.cl
    L.rec("XZ", tuple(e.r(i) & MASK for i in range(3, 9)))
    e.set_f1(bitsf(L.ret("XZ")))


STUBS.internal(XZ, stub_xz)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def put(self, a, fmt, v):
        self.write(a, struct.pack(self.e_ + fmt, v))

    def u32(self, a, v):
        self.put(a, "I", v & MASK)

    def half(self, a, v):
        self.put(a, "H", v & 0xFFFF)

    def off(self, name):
        return MAC[name] if self.mac else self.lay[name]

    def size(self, name):
        return max(MAC[name], self.lay.get(name, 0)) if not self.mac else MAC[name]

    def field_u32(self, base, name, v):
        self.u32(base + self.off(name), v)

    def field_ptr(self, base, name, v):
        self.ptr(base + self.off(name), v)


class Ppc(Side):
    e_ = ">"
    mac = True

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
        self.put(a, "I", v & MASK)

    def p32(self, a, v):
        self.put(a, "I", v & MASK)

    def log(self):
        return self.L.log

    def call(self, args):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN, [v & MASK for v in args])
        return s32(r3)


class Host(Side):
    e_ = "<"
    mac = False

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.lay = lib.lay
        self.base = lib.tpw_ivp_base()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ivp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ivp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.put(a, "Q", v)

    def p32(self, a, v):
        self.put(a, "I", (v - self.lib.tpw_ivp_base()) & MASK if v else 0)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, args):
        x, z, a, b, c, d, e = args
        return self.lib.tpw_ivp_call(x & MASK, z & MASK, a, b & MASK, c & MASK, d & MASK, e & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ivp_addr.restype, lib.tpw_ivp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ivp_alloc.restype, lib.tpw_ivp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ivp_base.restype, lib.tpw_ivp_base.argtypes = C.c_uint64, []
    lib.tpw_ivp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ivp_call.restype = C.c_int32
    lib.tpw_ivp_call.argtypes = [C.c_uint32, C.c_uint32, C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 64)()
    n = lib.tpw_ivp_layout(out)
    assert n == len(NAMES), n
    lib.lay = dict(zip(NAMES, list(out)[:n]))
    for k in ("ctl.mF9C", "ctl.mFA0", "ctl.mFA4", "ctl.mFA8", "ctl.mAngles", "ctl.mFCC"):
        assert lib.lay[k] == MAC[k], (k, lib.lay[k])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    w, h = c["w"], c["h"]
    # the height field: header -> field (+0x6c), copy -> quads (+0x2c)
    hdr, field, copy = s.alloc(0xC0), s.alloc(0x40), s.alloc(0x40)
    quads = s.alloc(4 * w * h + 8)
    s.p32(hdr + 0x6C, field)
    s.u32(field + 0x18, w)
    s.u32(field + 0x1C, h)
    s.p32(copy + 0x2C, quads)
    for i, bit in enumerate(c["quad"]):
        s.half(quads + 4 * i, bit)
    s.ptr(s.g("hhdr"), hdr)
    s.ptr(s.g("hcopy"), copy)

    typ = s.alloc(s.size("type.size"))
    ctls = [typ + s.off("type.mPylonsA"), typ + s.off("type.mPylonsB")]
    for ctl, p in zip(ctls, c["ctl"]):
        s.u32(ctl + 0x9C, p["f9c"])
        s.u32(ctl + 0xA0, p["fa0"])
        s.u32(ctl + 0xA4, fb(p["fa4"]))
        s.u32(ctl + 0xA8, fb(p["fa8"]))
        for i, v in enumerate(p["ang"]):
            s.u32(ctl + 0x74 + 4 * i, fb(v))
        s.u32(ctl + 0xCC, p["fcc"])

    insts = [s.alloc(s.size("inst.size")) for _ in range(3)]
    braces = [s.alloc(s.size("brace.size")) for _ in range(6)]
    pyls = [s.alloc(s.size("pyl.size")) for _ in range(6)]

    def P(lst, i):
        return lst[i] if i >= 0 else 0

    for i, b in enumerate(c["braces"]):
        a = braces[i]
        s.field_u32(a, "brace.mNumber", b["number"])
        s.field_u32(a, "brace.mFlags", b["flags"])
        s.field_ptr(a, "brace.mPrev", P(braces, b["prev"]))
        s.field_ptr(a, "brace.mNext", P(braces, b["next"]))
        s.field_ptr(a, "brace.mData", pyls[b["data"]])
    for i, p in enumerate(c["pyls"]):
        a = pyls[i]
        s.field_u32(a, "pyl.mFlags", p["flags"])
        s.field_u32(a, "pyl.mIndex", p["index"])
        s.field_u32(a, "pyl.mCellX", p["cx"])
        s.field_u32(a, "pyl.mCellZ", p["cz"])
        s.field_ptr(a, "pyl.mInstance", insts[p["inst"] - 1] if p["inst"] else 0)
        s.field_ptr(a, "pyl.mControl", ctls[p["ctl"]])
        s.field_ptr(a, "pyl.mNext", P(pyls, p["next"]))
        s.field_u32(a, "pyl.mCount", p["count"])
        s.field_ptr(a, "pyl.mBraces", braces[p["b0"]])
    e = c["edit"]
    for k, ia in enumerate(insts):
        s.field_ptr(ia, "inst.mType", typ)
        s.field_ptr(ia, "inst.mPylons", P(pyls, c["heads"][k]))
    E = insts[0]
    s.field_u32(E, "inst.mBlock3C", c["f3c"])
    s.field_ptr(E, "inst.mLastBrace", braces[5])
    s.field_ptr(E, "inst.mBraces", braces[e["braces"]])
    s.field_ptr(E, "inst.mPreview", braces[e["preview"]])
    s.field_ptr(E, "inst.mBrace64", braces[e["b64"]])
    order = c["order"]
    for j, k in enumerate(order):
        s.field_ptr(insts[k], "inst.mNext", insts[order[j + 1]] if j + 1 < len(order) else 0)
    s.ptr(s.g("insts"), insts[order[0]] if order else 0)
    s.ptr(s.g("edit"), E)
    s.ptr(s.g("mv"), braces[c["mv"]])
    s.u32(s.g("w1790"), c["w1790"])
    s.u32(s.g("inflags"), c["inflags"])
    s.u32(s.g("cmax"), fb(c["cmax"]))
    s.u32(s.g("cmin"), fb(c["cmin"]))
    return braces


def run(s, c):
    try:
        braces = build(s, c)
        a = braces[c["a"]] if c["a"] >= 0 else 0
        x, z, b, cc, d, e = c["args"]
        return {"answer": s.call([x, z, a, b, cc, d, e]), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def case(r, **force):
    w, h = r.randint(3, 8), r.randint(3, 8)
    x = force.get("x")
    if x is None:
        x = w if r.random() < 0.05 else r.randrange(w)
    z = force.get("z")
    if z is None:
        z = h if r.random() < 0.05 else r.randrange(h)
    a = force.get("a")
    if a is None:
        a = -1 if r.random() < 0.5 else r.randrange(6)
    in100 = force.get("in100")
    if in100 is None:
        in100 = r.random() < 0.5
    inflags = (0x100 if in100 else 0) | r.choice([0, 0, 0x40, 0x1])
    f3c = force.get("f3c")
    if f3c is None:
        f3c = r.choice([0, 1, 2, 3])
    cell = x + z * w
    pf = lambda p: r.choice([0.0, 1.0, 1.5, 2.0, r.uniform(0, 3)])

    def ctl():
        fa4 = pf(0)
        return dict(f9c=r.choice([0, 0, 1, 2, 4]), fa0=r.choice([2, 5, 20, 100, 100]), fa4=fa4,
                    fa8=fa4 + r.choice([0.0, 1.0, 2.0, 3.5, -1.0, 1.0]), ang=[r.uniform(0, 3.4) for _ in range(10)],
                    fcc=r.choice([0, 1, 2, 3, 4, 5]))

    nxt_forced = in100 and a < 0
    braces = []
    for i in range(6):
        fl = 0
        for bit in (0x10000, 0x20000, 0x40, 0x200, 0x400, 0x8):
            if r.random() < 0.25:
                fl |= bit
        nx = r.randrange(6) if (nxt_forced or r.random() < 0.8) else -1
        pv = r.randrange(6) if (i == 5 or r.random() < 0.8) else -1
        braces.append(dict(number=r.choice([0, 1, 2, 3]), flags=fl, prev=pv, next=nx, data=r.randrange(6)))
    pyls = []
    for i in range(6):
        fl = 0
        for bit in (1, 2, 4, 8):
            if r.random() < 0.3:
                fl |= bit
        pyls.append(dict(flags=fl, index=r.choice([cell, cell, r.randrange(w * h)]), cx=r.choice([x, r.randrange(w + 1)]),
                         cz=r.choice([z, r.randrange(h + 1)]), inst=r.choice([1, 1, 1, 2, 0]), ctl=r.choice([0, 1]),
                         next=r.choice([-1] + list(range(i + 1, 6))), count=r.randrange(0, 5), b0=r.randrange(6)))
    order = r.choice([[0, 1, 2], [1, 0, 2], [1, 2, 0], [1, 2], [0], [2, 1, 0]])
    heads = [r.randrange(6) if r.random() < 0.9 else -1 for _ in range(3)]
    return {
        "queues": {"XZ": [fb(r.uniform(-4, 4)) for _ in range(3)] + [r.choice([0, 0x80000000, fb(3.14159), fb(1.0)])]},
        "w": w, "h": h, "quad": [1 if r.random() < 0.06 else 0 for _ in range(w * h)],
        "ctl": [ctl(), ctl()], "braces": braces, "pyls": pyls, "heads": heads, "order": order,
        "edit": dict(braces=r.randrange(6), preview=r.randrange(6), b64=r.randrange(6)),
        "f3c": f3c, "mv": r.randrange(6), "w1790": r.choice([0, 1, 2]), "inflags": inflags, "a": a,
        "cmax": r.choice([1.0, 1.0, 1.0, 0.5, 2.0]), "cmin": r.choice([0.0, 0.0, 0.0, -1.0, 0.25]),
        "args": [x, z] + [r.randrange(0, 7) for _ in range(4)],
    }


def edges():
    r = random.Random(5551)
    combos = [(a, i, f) for a in (-1, 0, 3) for i in (False, True) for f in (0, 1, 2, 3)]
    for _ in range(12):
        for a, i, f in combos:
            yield case(r, a=a, in100=i, f3c=f)
    for _ in range(20):
        yield case(r, x=99)
        yield case(r, z=99)


def rnd(r):
    return case(r)


TESTS = [
    FnTest("IsValidPylonPosition", ppc, host, edge=edges, rnd=rnd),
]
