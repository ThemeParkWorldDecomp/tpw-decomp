"""Unit v5920, the coaster editor's button functions, against libtpw_unit_GetCoasterEditDefaultButtons.dylib
(tests/unit_GetCoasterEditDefaultButtons.cpp), main app, src/game/rides/CoasterEdit.cpp:
  HandleCoasterButtonDown 0x10056d7c, GetCoasterEditDefaultButtons 0x10057120, GetCoasterEditAvailableButtons 0x10057278

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian at the Mac offsets,
host memory little-endian at the host offsets of the header's structs). DisableCurrentPylonEditState, FlashEndPylons and SetPylonType
are fakes on both sides that record their arguments.

The world of a case: the editor's coaster (flags, type word, block flags, the brace list head and the preview brace with their flags),
the six globals. What is compared: the out word, the globals, the coaster's flags word, the event log.

No fused operations. Not tested: a NULL editor coaster (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_GetCoasterEditDefaultButtons"
MASK = 0xFFFFFFFF

HANDLE, DEFAULT, AVAILABLE = 0x10056D7C, 0x10057120, 0x10057278
SIZES = {HANDLE: 696, DEFAULT: 288, AVAILABLE: 1160}
OWN = set(SIZES)

DISABLE, FLASH, SETTYPE = 0x10053B58, 0x10052158, 0x100562D4

ADDR = dict(edit=0x102b68b0, enabled=0x102c16d4, down=0x102c16d8, w16E0=0x102c16e0, w1790=0x102c1790, input=0x102c1794)
GLOBALS = ["enabled", "down", "w16E0", "w1790", "input"]
MAC_OFF = dict(flags=0, type=4, blk=0x3c, braces=0x58, prev=0x60, brflags=8)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def stub_disable(e):
    e.cl.rec("DisableCurrentPylonEditState", ())


def stub_flash(e):
    e.cl.rec("FlashEndPylons", (e.cl.lab(e.r(3)), e.r(4) & MASK))


def stub_settype(e):
    e.cl.rec("SetPylonType", (e.r(3) & MASK,))


STUBS.internal(DISABLE, stub_disable)
STUBS.internal(FLASH, stub_flash)
STUBS.internal(SETTYPE, stub_settype)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    off = MAC_OFF

    def __init__(self, e, case):
        self.e = e
        self.inst = e.alloc(0x200)
        self.L = e.cl = CL.PpcLog({self.inst: 1}, case["queues"])
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

    def call(self, op, arg, out):
        if op == "handle":
            T.call_ppc(self.e, HANDLE, [arg & MASK])
        elif op == "default":
            T.call_ppc(self.e, DEFAULT, [out])
        else:
            T.call_ppc(self.e, AVAILABLE, [out])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lay = lib.lay
        self.off = dict(flags=lay[0], type=lay[1], blk=lay[2], braces=lay[3], prev=lay[4], brflags=lay[6])
        self.inst = lib.tpw_cebtn_alloc(lay[5] + 64)
        CL.host_begin(lib, {1: self.inst}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cebtn_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cebtn_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, arg, out):
        if op == "handle":
            self.lib.tpw_cebtn_handle(arg & MASK)
        elif op == "default":
            self.lib.tpw_cebtn_default(out)
        else:
            self.lib.tpw_cebtn_available(out)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, "tpw_cebtn_addr").restype, getattr(lib, "tpw_cebtn_addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, "tpw_cebtn_alloc").restype, getattr(lib, "tpw_cebtn_alloc").argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cebtn_layout.restype, lib.tpw_cebtn_layout.argtypes = C.c_int32, [C.POINTER(C.c_int64)]
    lib.tpw_cebtn_handle.argtypes = [C.c_uint32]
    lib.tpw_cebtn_handle.restype = None
    lib.tpw_cebtn_default.argtypes = [C.c_uint64]
    lib.tpw_cebtn_default.restype = None
    lib.tpw_cebtn_available.argtypes = [C.c_uint64]
    lib.tpw_cebtn_available.restype = None
    out = (C.c_int64 * 32)()
    n = lib.tpw_cebtn_layout(out)
    lib.lay = list(out)[:n]
    assert lib.lay[0] == 0 and lib.lay[6] == 8, lib.lay
    lib.bound = True


def build(s, case):
    o = s.off
    typ = s.alloc(8)
    s.words(typ, [case["type"]])
    b_head = s.alloc(0x20)
    s.words(b_head + o["brflags"], [case["head"]])
    b_prev = s.alloc(0x20)
    s.words(b_prev + o["brflags"], [case["prev"]])
    s.words(s.inst + o["flags"], [case["iflags"]])
    s.ptr(s.inst + o["type"], typ)
    s.words(s.inst + o["blk"], [case["blk"]])
    s.ptr(s.inst + o["braces"], b_head)
    s.ptr(s.inst + o["prev"], b_prev)
    s.ptr(s.g("edit"), s.inst)
    for n in GLOBALS:
        s.words(s.g(n), [case[n]])
    s.out = s.alloc(8)
    s.words(s.out, [case["out"]])


def observe(s, case):
    r = {"log": s.log(), "flags": s.rd_word(s.inst + s.off["flags"]), "out": s.rd_word(s.out)}
    for n in GLOBALS:
        r[n] = s.rd_word(s.g(n))
    return r


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"], case["arg"], 0 if case["null"] else s.out)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    return {"op": op, "queues": {}, "arg": 0, "null": False, "type": r.getrandbits(12), "head": r.getrandbits(32),
            "prev": r.getrandbits(32), "iflags": r.getrandbits(32), "blk": r.getrandbits(2), "enabled": r.getrandbits(32),
            "down": r.choice([0, r.getrandbits(32), 1 << r.randrange(9)]), "w16E0": r.getrandbits(32),
            "w1790": r.choice([0, 1, 1, 2, r.getrandbits(32)]), "input": r.getrandbits(32), "out": r.getrandbits(32)}


def gen(op, seed):
    def edges():
        r = random.Random(seed)
        for blk in range(4):
            for t in (0, 2, 0x80, 0x180, 0x82, 0x182, 0x100):
                for h in (0, 0x80):
                    for p in (0, 0x80):
                        for w in (0, 1, 2):
                            c = base(r, op)
                            c.update(blk=blk, type=t, head=h, prev=p, w1790=w)
                            if op == "handle":
                                for b in (0, 1, 2, 3, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x80000000, 0xFFFFFFFF):
                                    d = dict(c)
                                    d["arg"] = b
                                    yield d
                            elif op == "available":
                                for dn in (0, 1, 4, 0x20, 0x100, 0x200):
                                    for nl in (False, True):
                                        d = dict(c)
                                        d.update(down=dn, null=nl)
                                        yield d
                            else:
                                yield c

    def rnd(r):
        c = base(r, op)
        c["blk"] = r.getrandbits(2)
        if op == "handle":
            c["arg"] = r.choice([1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, r.getrandbits(32), r.getrandbits(10)])
        if op == "available":
            c["null"] = r.random() < 0.2
        return c

    return edges, rnd


TESTS = []
for _name, _op, _seed in (("HandleCoasterButtonDown", "handle", 11), ("GetCoasterEditDefaultButtons", "default", 12),
                          ("GetCoasterEditAvailableButtons", "available", 13)):
    _e, _r = gen(_op, _seed)
    TESTS.append(FnTest(_name, ppc, host, edge=_e, rnd=_r))
