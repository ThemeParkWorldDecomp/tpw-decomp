"""Unit v5266, InterfaceVaryBox::ChangeValue and SetMinMaxValues, against libtpw_unit_InterfaceVaryBox_ChangeValue.dylib
(tests/unit_InterfaceVaryBox_ChangeValue.cpp), main app, src/ui_whatami_InterfaceVaryBox_ChangeValue.cpp:
  ChangeValue 0x101d9080, SetMinMaxValues 0x101d8e44

A case is an object (a zeroed block) with the range, value, old value, force flag, id and parent set, built by one piece of code in both
memories (the host's offsets come from the layout export, the Mac's are fixed). InterfaceWindow::SetValue and UI_PostMessage are fakes on
both sides recording every argument. Compared: the answer, the five fields and the event log. No fused operations. Not tested: nothing.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InterfaceVaryBox_ChangeValue"
MASK = 0xFFFFFFFF

FN_CHANGE, FN_MINMAX = 0x101D9080, 0x101D8E44
SIZES = {FN_CHANGE: 168, FN_MINMAX: 104}
OWN = set(SIZES)
SETVALUE, POSTMSG = 0x101DFE84, 0x101C9400

MAC = dict(min=0x138, max=0x13C, val=0x140, old=0x144, force=0x154, parent=0x5C, id=0x88, size=0x158)
HOST = {}
HOST_KEYS = ["min", "max", "val", "old", "force", "parent", "id", "size"]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def stub_setvalue(e):
    e.cl.rec("SetValue", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, 0)


def stub_post(e):
    e.cl.rec("UI_PostMessage", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


STUBS.internal(SETVALUE, stub_setvalue)
STUBS.internal(POSTMSG, stub_post)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def prep(self):
        self.obj = self.alloc(self.off["size"] + 16)
        self.parent = self.alloc(16)


class Ppc(Side):
    fmt = ">I"
    off = MAC

    def __init__(self, e, case):
        self.e = e
        self.prep()
        self.L = e.cl = CL.PpcLog({self.obj: 1, self.parent: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def setptr(self, a, p):
        self.words(a, [p])

    def log(self):
        return self.L.log

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = HOST
        self.prep()
        CL.host_begin(lib, {1: self.obj, 2: self.parent}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_vb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setptr(self, a, p):
        self.write(a, struct.pack("<Q", p))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        if entry == FN_CHANGE:
            return self.lib.tpw_vb_ChangeValue(args[0], s32(args[1])) & MASK
        self.lib.tpw_vb_SetMinMaxValues(args[0], args[1], args[2])
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_vb_alloc.restype, lib.tpw_vb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_vb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_vb_ChangeValue.restype, lib.tpw_vb_ChangeValue.argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    lib.tpw_vb_SetMinMaxValues.restype, lib.tpw_vb_SetMinMaxValues.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_vb_layout(out)
    got = list(out)[:n]
    for k, v in zip(HOST_KEYS, got):
        HOST[k] = v
    assert HOST["max"] == HOST["min"] + 4 and HOST["val"] == HOST["min"] + 8 and HOST["old"] == HOST["min"] + 12, got
    lib.bound = True


def build(s, case):
    o, off = s.obj, s.off
    s.words(o + off["min"], [case["min"]])
    s.words(o + off["max"], [case["max"]])
    s.words(o + off["val"], [case["val"]])
    s.words(o + off["old"], [case["old"]])
    s.write(o + off["force"], bytes([case["force"]]))
    s.words(o + off["id"], [case["id"]])
    s.setptr(o + off["parent"], s.parent)
    case["ptrs"] = []
    for v in (case.get("pmin"), case.get("pmax")):
        if v is None:
            s.ptr_arg = None
            case["ptrs"].append(0)
        else:
            a = s.alloc(8)
            s.words(a, [v])
            case["ptrs"].append(a)


def execute(s, case):
    if case["op"] == "change":
        return s.call(FN_CHANGE, [s.obj, case["arg"] & MASK])
    s.call(FN_MINMAX, [s.obj] + case["ptrs"])       # void: r3 is not an answer
    return 0


def observe(s, case, answer):
    o, off = s.obj, s.off
    return {"answer": answer, "log": s.log(),
            "min": s.rd_word(o + off["min"]), "max": s.rd_word(o + off["max"]), "val": s.rd_word(o + off["val"]),
            "old": s.rd_word(o + off["old"]), "force": s.read(o + off["force"], 1)[0]}


def run(s, case):
    try:
        # pointers differ per side: build sets case["ptrs"], so work on a copy
        c = dict(case)
        build(s, c)
        return observe(s, c, execute(s, c))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rv(r):
    k = r.random()
    if k < 0.3:
        return r.choice([0, 1, -1, 5, -5, 10, 0x7FFFFFFF, -0x80000000, 0x9708CD])
    return r.randrange(-20, 21)


def base(r, op):
    return {"op": op, "queues": {}, "min": rv(r) & MASK, "max": rv(r) & MASK, "val": rv(r) & MASK, "old": rv(r) & MASK,
            "force": r.choice([0, 0, 1, 7]), "id": rv(r) & MASK, "pmin": None, "pmax": None, "arg": rv(r)}


def change_case(r, mn=None, mx=None, val=None, arg=None, force=None):
    c = base(r, "change")
    if mn is not None:
        c["min"] = mn & MASK
    if mx is not None:
        c["max"] = mx & MASK
    if val is not None:
        c["val"] = val & MASK
    if arg is not None:
        c["arg"] = arg
    if force is not None:
        c["force"] = force
    return c


def change_edges():
    r = random.Random(5266)
    for mn, mx in ((0, 10), (-5, 5), (3, 3), (10, 0)):
        for arg in (mn - 1, mn, mx, mx + 1, (mn + mx) // 2, mn + 1):
            for val in (arg, arg + 1, 0x9708CD):
                for force in (0, 1):
                    yield change_case(r, mn, mx, val, arg, force)


def change_random(r):
    return change_case(r)


def mm_case(r, pmin="r", pmax="r"):
    c = base(r, "minmax")
    c["pmin"] = rv(r) & MASK if pmin == "r" else pmin
    c["pmax"] = rv(r) & MASK if pmax == "r" else pmax
    return c


def mm_edges():
    r = random.Random(8844)
    for pmin in (None, "r"):
        for pmax in (None, "r"):
            for _ in range(12):
                yield mm_case(r, pmin, pmax)
    for mn, mx, val, pa, pb in ((0, 10, -1, 0, 10), (0, 10, 11, 0, 10), (0, 10, 5, 0, 10), (0, 10, 0, 0, 10), (0, 10, 10, 0, 10),
                                (5, 1, 3, 5, 1), (5, 1, 0, 5, 1), (5, 1, 9, 5, 1)):
        c = mm_case(r)
        c["val"], c["pmin"], c["pmax"] = val & MASK, pa & MASK, pb & MASK
        yield c


def mm_random(r):
    return mm_case(r, r.choice([None, "r"]), r.choice([None, "r"]))


TESTS = [
    FnTest("InterfaceVaryBox::ChangeValue", ppc, host, edge=change_edges, rnd=change_random),
    FnTest("InterfaceVaryBox::SetMinMaxValues", ppc, host, edge=mm_edges, rnd=mm_random),
]
