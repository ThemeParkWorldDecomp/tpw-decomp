"""Unit v5038, the coaster destroyers and the track position control, against libtpw_unit_DestroyCoasterPylons.dylib
(tests/unit_DestroyCoasterPylons.cpp), main app ("SimTheme Park"), src/DestroyCoasterPylons.cpp:
  DestroyCoasterTrack 0x10050df4, DestroyCoasterPylons 0x10050e84, IncrementTrackPosnControl 0x10051300,
  DecrementTrackPosnControl 0x100513fc, GetTrackPosition 0x100514f0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's offsets (big-endian) for
the original, and in the host library's memory (little-endian, native pointers, the host offsets taken from tpw_dcp_layout) for the
translation. The callees outside the unit (DestroyDynamicMeshSetInstance, DestroyTrackBrace, Destroy3DObjectFromAIWorld, DestroyBraceInfo,
MReleaseFreeF, BezierPosition) are fakes on both sides; objects are named in the log by an id word the case writes into them.
BezierPosition's fake writes the two controls' ids and t's bits into the point, so the compare sees which controls were passed.

What is compared: the event log, the lists' head and links as ids, the control's four words (braces as ids), the point.
No fused operations. Not tested: NULL links the original dereferences (a brace list without next or prev).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DestroyCoasterPylons"
MASK = 0xFFFFFFFF

FN_TRACK, FN_PYLONS, FN_INC, FN_DEC, FN_POS = 0x10050DF4, 0x10050E84, 0x10051300, 0x100513FC, 0x100514F0
SIZES = {FN_TRACK: 80, FN_PYLONS: 192, FN_INC: 184, FN_DEC: 176, FN_POS: 84}
OWN = set(SIZES)

C_DTB, C_D3D, C_DBI, C_DDM, C_FREE, C_BEZ = 0x1004094C, 0x1005F3FC, 0x1003E064, 0x10063264, 0x10064C54, 0x1004DA98

KEYS = ["inst_f38", "inst_pylons", "p_flags", "p_id", "p_f0c", "p_f10", "p_ctl", "p_next", "p_count", "p_braces", "set_next", "pc_brace",
        "pc_idxb", "pc_braceb", "b_prev", "b_next", "b_f90", "sp_a"]
MAC = dict(zip(KEYS, [0x38, 0x4C, 4, 8, 0xC, 0x10, 0x18, 0x28, 0x30, 0x34, 0x38, 4, 8, 0xC, 0x64, 0x68, 0x90, 4]))


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_dtb(e):
    e.cl.rec("DestroyTrackBrace", (e.u32(e.r(3)), e.u32(e.r(4) + 4), e.u32(e.r(4))))


def stub_d3d(e):
    e.cl.rec("Destroy3DObjectFromAIWorld", (e.r(3), e.r(4)))


def stub_dbi(e):
    e.cl.rec("DestroyBraceInfo", (e.u32(e.r(3)), (e.r(4) - e.r(3)) & MASK))


def stub_ddm(e):
    e.cl.rec("DestroyDynamicMeshSetInstance", (e.u32(e.r(3)), e.r(4), e.r(5)))


def stub_free(e):
    e.cl.rec("MReleaseFreeF", (e.u32(e.r(3) + 8),))


def stub_bez(e):
    ida, idb, tb = e.u32(e.r(3) + 8), e.u32(e.r(4) + 8), fb(e.f(1))
    e.write(e.r(6), struct.pack(">III", ida, idb, tb))
    e.cl.rec("BezierPosition", (ida, idb, tb))


for _a, _f in ((C_DTB, stub_dtb), (C_D3D, stub_d3d), (C_DBI, stub_dbi), (C_DDM, stub_ddm), (C_FREE, stub_free), (C_BEZ, stub_bez)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]

    def idof(self, p):
        return 0 if p == 0 else self.rd_word(p)


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    o = MAC

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        T.call_ppc(self.e, fn, args)

    def call_pos(self, p, t, out):
        T.call_ppc(self.e, FN_POS, [p, T.F(t), out])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.o = lib.o
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_dcp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        getattr(self.lib, {FN_TRACK: "tpw_dcp_track", FN_PYLONS: "tpw_dcp_pylons", FN_INC: "tpw_dcp_inc", FN_DEC: "tpw_dcp_dec"}[fn])(args[0])

    def call_pos(self, p, t, out):
        self.lib.tpw_dcp_pos(p, t, out)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_dcp_alloc.restype, lib.tpw_dcp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_dcp_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("pylons", "track", "inc", "dec"):
        getattr(lib, "tpw_dcp_" + n).restype, getattr(lib, "tpw_dcp_" + n).argtypes = None, [C.c_uint64]
    lib.tpw_dcp_pos.restype, lib.tpw_dcp_pos.argtypes = None, [C.c_uint64, C.c_float, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_dcp_layout(out)
    lib.o = dict(zip(KEYS, list(out)[:n]))
    lib.bound = True


# ------------------------------------------------------------------------------------------------------------------ the world

def mk_brace(s, bid, count):
    brace = s.alloc(0x400)
    s.words(brace, [bid])
    spline = s.alloc(0x40)
    s.words(spline, [count])
    arr = s.alloc(8 * 0x58)
    for k in range(8):
        s.words(arr + k * 0x58 + 8, [bid * 100 + k + 1])
    s.ptr(spline + s.o["sp_a"], arr)
    s.ptr(brace + s.o["b_f90"], spline)
    return brace


def build_pylons(s, case):
    inst = s.alloc(0x400)
    nodes = []
    for nd in case["nodes"]:
        node = s.alloc(0x100)
        s.words(node + s.o["p_flags"], [nd["flags"]])
        s.words(node + s.o["p_id"], [nd["id"]])
        s.words(node + s.o["p_f0c"], [nd["f0c"]])
        s.words(node + s.o["p_f10"], [nd["f10"]])
        ctl = s.alloc(0x200)
        s.words(ctl, [nd["ctl_flags"], nd["ctl_id"]])
        s.ptr(node + s.o["p_ctl"], ctl)
        s.words(node + s.o["p_count"], [nd["n"]])
        for i, bid in enumerate(nd["braces"]):
            br = s.alloc(0x200)
            s.words(br, [bid])
            s.ptr(node + s.o["p_braces"] + i * s.psz, br)
        nodes.append(node)
    for i, node in enumerate(nodes):
        s.ptr(node + s.o["p_next"], nodes[i + 1] if i + 1 < len(nodes) else 0)
    s.ptr(inst + s.o["inst_pylons"], nodes[0] if nodes else 0)
    return inst


def build_track(s, case):
    inst = s.alloc(0x400)
    sets = []
    for sid in case["sets"]:
        st = s.alloc(0x100)
        s.words(st, [sid])
        sets.append(st)
    for i, st in enumerate(sets):
        s.ptr(st + s.o["set_next"], sets[i + 1] if i + 1 < len(sets) else 0)
    s.ptr(inst + s.o["inst_f38"], sets[0] if sets else 0)
    return inst, sets


def build_pos(s, case):
    braces = [mk_brace(s, i + 1, c) for i, c in enumerate(case["counts"])]
    for i, br in enumerate(braces):
        s.ptr(br + s.o["b_prev"], braces[case["prev"][i]])
        s.ptr(br + s.o["b_next"], braces[case["next"][i]])
    p = s.alloc(0x40)
    s.words(p, [case["ia"]])
    s.ptr(p + s.o["pc_brace"], braces[case["a"]])
    s.words(p + s.o["pc_idxb"], [case["ib"]])
    s.ptr(p + s.o["pc_braceb"], braces[case["b"]])
    return p


def read_pos(s, p):
    return [s.rd_word(p), s.idof(s.rd_ptr(p + s.o["pc_brace"])), s.rd_word(p + s.o["pc_idxb"]), s.idof(s.rd_ptr(p + s.o["pc_braceb"]))]


def run(s, case):
    try:
        op = case["op"]
        if op == "pylons":
            inst = build_pylons(s, case)
            s.call(FN_PYLONS, [inst])
            return {"log": s.log()}
        if op == "track":
            inst, sets = build_track(s, case)
            s.call(FN_TRACK, [inst])
            return {"log": s.log(), "head": s.idof(s.rd_ptr(inst + s.o["inst_f38"])),
                    "next": [s.idof(s.rd_ptr(st + s.o["set_next"])) for st in sets]}
        p = build_pos(s, case)
        if op == "pos":
            out = s.alloc(0x10)
            s.call_pos(p, case["t"], out)
            return {"log": s.log(), "pc": read_pos(s, p), "pt": [s.rd_word(out + 4 * i) for i in range(3)]}
        s.call(FN_INC if op == "inc" else FN_DEC, [p])
        return {"log": s.log(), "pc": read_pos(s, p)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ------------------------------------------------------------------------------------------------------------------ cases

def rid(r):
    return r.getrandbits(32)


def pylon_node(r, flags=None, ctl_flags=None, n=None):
    flags = r.choice([0, 0, 1, 1, 2, 3, 0xFFFFFFFE, 0xFFFFFFFF]) if flags is None else flags
    n = r.randrange(0, 5) if n is None else n
    if flags & 1:
        n = max(n, 1)
    return {"flags": flags, "id": rid(r), "f0c": rid(r), "f10": rid(r), "n": n,
            "ctl_flags": r.choice([0, 4, 0x4004, 0x10, 5, 0xFFFFFFFB, 0xFFFFFFFF]) if ctl_flags is None else ctl_flags,
            "ctl_id": rid(r), "braces": [rid(r) for _ in range(n)]}


def pylons_case(r, nodes):
    return {"op": "pylons", "queues": {}, "nodes": nodes}


def pylons_edges():
    r = random.Random(11)
    yield pylons_case(r, [])
    for flags in (0, 1, 2, 0xFFFFFFFE):
        for cf in (0, 4, 0x4004):
            for n in (0, 1, 3):
                yield pylons_case(r, [pylon_node(r, flags, cf, n)])
    yield pylons_case(r, [pylon_node(r, 0, 4, 2), pylon_node(r, 1, 0, 1), pylon_node(r, 0, 0, 0), pylon_node(r, 0, 4, 4)])


def pylons_random(r):
    return pylons_case(r, [pylon_node(r) for _ in range(r.randrange(0, 6))])


def track_case(r, n):
    return {"op": "track", "queues": {}, "sets": [rid(r) for _ in range(n)]}


def track_edges():
    r = random.Random(12)
    for n in range(0, 5):
        yield track_case(r, n)


def track_random(r):
    return track_case(r, r.randrange(0, 7))


def pos_case(r, op, same, ca, cb, ia, ib):
    counts = [r.choice([0, 1, 2, 3, 5, 8]) for _ in range(4)]
    counts[0] = ca
    counts[1] = cb
    return {"op": op, "queues": {}, "counts": counts, "prev": [r.randrange(4) for _ in range(4)], "next": [r.randrange(4) for _ in range(4)],
            "a": 0, "b": 0 if same else 1, "ia": ia, "ib": ib, "t": bitsf(fb(r.uniform(-4, 4)))}


def pos_edges(op):
    def gen():
        r = random.Random({"inc": 21, "dec": 22, "pos": 23}[op])
        for same in (True, False):
            for ca in (0, 1, 2, 3):
                for cb in (0, 1, 2, 3):
                    if same and ca != cb:
                        continue
                    for ia in (0, 1, 2):
                        for ib in (0, 1, 2, 3):
                            yield pos_case(r, op, same, ca, cb, ia, ib)
    return gen


def pos_random(op):
    def gen(r):
        return pos_case(r, op, r.random() < 0.4, r.randrange(0, 6), r.randrange(0, 6), r.randrange(0, 8), r.randrange(0, 8))
    return gen


TESTS = [
    FnTest("DestroyCoasterTrack", ppc, host, edge=track_edges, rnd=track_random),
    FnTest("DestroyCoasterPylons", ppc, host, edge=pylons_edges, rnd=pylons_random),
    FnTest("IncrementTrackPosnControl", ppc, host, edge=pos_edges("inc"), rnd=pos_random("inc")),
    FnTest("DecrementTrackPosnControl", ppc, host, edge=pos_edges("dec"), rnd=pos_random("dec")),
    FnTest("GetTrackPosition", ppc, host, edge=pos_edges("pos"), rnd=pos_random("pos")),
]
