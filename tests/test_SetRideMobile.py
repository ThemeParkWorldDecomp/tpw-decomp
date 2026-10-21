"""Unit v5306, SetRideMobile, against libtpw_unit_SetRideMobile.dylib (tests/unit_SetRideMobile.cpp), main app, src/game/rides/TPRide.cpp:
  SetRideMobile 0x10068280

A case is a world and a call. Three anim sets (indexes 1..3 of gRideAnimSets), each with its own mesh, header and node, are built by ONE piece
of Python code in emulator memory (big-endian, 4-byte pointers at the Mac's offsets) and in host memory (little-endian, 8-byte native pointers
at the host's offsets, Ptr32 words as arena offsets). No callees.

Compared: the answer-less call's effects: each set's flags word, each header's bytes 0..0x78 (flags at 0x30), each node's first words.
No fused operations. Not tested: a NULL set, mesh or node (the original reads them unconditionally), an index past the table.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SetRideMobile"
MASK = 0xFFFFFFFF

FN = 0x10068280
SIZES = {FN: 84}
OWN = set(SIZES)
ADDR = dict(sets=0x102C79A4)
MAC = dict(set_flags=4, set_heads=8, mesh_header=4, hdr_flags=0x30, hdr_f78=0x78, ps=4)
HOST = {}
NSETS = 3

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def put_ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    lay = MAC

    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, {})
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def p32(self, a):
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def call(self, ride):
        T.call_ppc(self.e, FN, [ride])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.lay = HOST
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sr_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sr_alloc(n)

    def p32(self, a):
        return self.lib.tpw_sr_p32(a)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def call(self, ride):
        self.lib.tpw_sr_set_ride_mobile(ride)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sr_addr.restype, lib.tpw_sr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sr_alloc.restype, lib.tpw_sr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sr_p32.restype, lib.tpw_sr_p32.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_sr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sr_set_ride_mobile.restype, lib.tpw_sr_set_ride_mobile.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sr_layout(out)
    v = list(out)[:n]
    assert v[3:5] == [0x30, 0x78], v
    HOST.update(set_flags=v[0], set_heads=v[1], mesh_header=v[2], hdr_flags=v[3], hdr_f78=v[4], ps=v[5])
    lib.bound = True


def build(s, case):
    L = s.lay
    s.objs = []
    s.ridebox = s.alloc(8)
    s.words(s.ridebox, [case["ride"]])
    for k in range(NSETS):
        c = case["sets"][k]
        st = s.alloc(0x200)
        mesh = s.alloc(0x40)
        hdr = s.alloc(0xB0)
        node = s.alloc(0x20)
        s.words(st + L["set_flags"], [c["setflags"]])
        s.put_ptr(st + L["set_heads"], mesh)
        s.put_ptr(mesh + L["mesh_header"], hdr)
        s.words(hdr, c["hdr"])
        s.words(hdr + L["hdr_flags"], [c["hdrflags"]])
        s.words(hdr + L["hdr_f78"], [s.p32(node)])
        s.words(node, c["node"])
        s.put_ptr(s.g("sets") + (k + 1) * L["ps"], st)
        s.objs.append((st, hdr, node))


def observe(s, case):
    L = s.lay
    out = {}
    for k, (st, hdr, node) in enumerate(s.objs):
        out["flags%d" % k] = s.rd(st + L["set_flags"], 1)
        out["hdr%d" % k] = s.rd(hdr, 0x78 // 4)
        out["node%d" % k] = s.rd(node, 8)
    return out


def run(s, case):
    try:
        build(s, case)
        s.call(s.ridebox)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, ride):
    sets = []
    for _ in range(NSETS):
        sets.append(dict(setflags=r.choice([0, MASK, 0x20, 0x1000000, r.getrandbits(32)]),
                         hdrflags=r.choice([0, MASK, 0x20, r.getrandbits(32)]),
                         hdr=[r.getrandbits(32) for _ in range(12)],
                         node=[r.choice([0, MASK, 0x80800, r.getrandbits(32)]) for _ in range(8)]))
    return {"ride": ride, "sets": sets}


def edges():
    r = random.Random(5306)
    for ride in (1, 2, 3, 1, 2, 3):
        yield base(r, ride)


def rnd(r):
    return base(r, r.randrange(1, NSETS + 1))


TESTS = [
    FnTest("SetRideMobile", ppc, host, edge=edges, rnd=rnd),
]
