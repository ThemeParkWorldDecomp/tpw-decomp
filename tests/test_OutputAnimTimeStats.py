"""Unit v5933, OutputAnimTimeStats and RestoreRideInstance, against libtpw_unit_OutputAnimTimeStats.dylib
(tests/unit_OutputAnimTimeStats.cpp), main app ("SimTheme Park"), src/game/rides/TPRide.cpp:
  OutputAnimTimeStats 0x1006aa70, RestoreRideInstance 0x1006a140

A case is a world and a call, built twice by ONE piece of Python code (Mac memory big-endian with 4-byte pointers for the original, host
memory little-endian with 8-byte pointers and the host struct offsets for the translation). Callees outside the unit are fakes on both
sides: CTRACE (level and text), GetRideInfo (the id, answers a block or NULL), SetCoasterCObject (both arguments); sprintf is the C
library's on both sides. The ride info's Shape slot is written at the Mac's +0x18 (a 4-byte pointer) on the PPC side and at the host field
of the same name (an 8-byte pointer) on the host side (tests/ridelayout.py).

The world: the ride tables (instances, types, counts), set blocks, a params block, a ride info block, a time control block. Compared:
the answer, the log, the flags word of every set. No fused operations. Not tested: NaN or infinite floats (the two C libraries print
them differently); an instance number whose slot is NULL (the original dereferences it); a type with no block.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ridelayout
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_OutputAnimTimeStats"
MASK = 0xFFFFFFFF

FN_OUT, FN_RESTORE = 0x1006AA70, 0x1006A140
SIZES = {FN_OUT: 568, FN_RESTORE: 608}
OWN = set(SIZES)
CTRACE = 0x1006FCBC
GETRIDEINFO = 0x10154868
SETCOASTER = 0x10043480

ADDR = dict(sets=0x102C79A4, countA=0x102D19A8, countB=0x102D19AC, types=0x102D19B0, typeCount=0x102D1ED8)
KEYS = ["self", "flags", "heads", "count", "tc", "fe4", "fe8", "fec", "ff0", "ff4", "ff8", "hdr", "f10", "f0c"]
MAC = dict(zip(KEYS, [0, 4, 8, 0xE, 0x10, 0xE4, 0xE8, 0xEC, 0xF0, 0xF4, 0xF8, 4, 0x10, 0xC]))

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_get(e):
    L = e.cl
    L.rec("GetRideInfo", ((e.r(4) >> 16) & 0xFFFF,))
    e.set_r(3, L.info if L.ret("GetRideInfo") else 0)


def stub_coaster(e):
    e.cl.rec("SetCoasterCObject", (e.r(3) & MASK, e.r(4) & MASK))


STUBS.internal(GETRIDEINFO, stub_get)
STUBS.internal(SETCOASTER, stub_coaster)
STUBS.internal(CTRACE, CL.recorder("CTRACE", "us"))


def stub_sprintf(e):
    """sprintf(buf, fmt, ...) with the arguments in r5 up (a double takes two words); %d %s %.2f only."""
    import re
    fmt = e.cstr(e.r(4))
    reg = [5]
    out = []

    def nxt():
        v = e.r(reg[0]) & MASK
        reg[0] += 1
        return v

    pos = 0
    for m in re.finditer(r"%(\.2f|d|s)", fmt):
        out.append(fmt[pos:m.start()])
        pos = m.end()
        k = m.group(1)
        if k == "d":
            out.append("%d" % s32(nxt()))
        elif k == "s":
            out.append(e.cstr(nxt()))
        else:
            hi = nxt()
            lo = nxt()
            out.append("%.2f" % struct.unpack(">d", struct.pack(">II", hi, lo))[0])
    out.append(fmt[pos:])
    text = "".join(out).encode() + b"\0"
    e.write(e.r(3), text)
    e.set_r(3, len(text) - 1)


STUBS.imp("sprintf", stub_sprintf)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def h16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def put(self, base, key, v):
        self.words(base + self.off[key], [v])

    def get(self, base, key):
        return self.rd_word(base + self.off[key])


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    off = MAC

    def __init__(self, e, case):
        self.e = e
        self.info = e.alloc(64)
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.info = self.info
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return self.info if name == "info" else ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def rec(self, mac):
        return mac

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def out(self, control, st):
        with_args = [control & MASK, st & MASK]
        T.call_ppc(self.e, FN_OUT, with_args)
        return 0

    def restore(self, a):
        return T.call_ppc(self.e, FN_RESTORE, [x & MASK for x in a])[0] & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = lib.hoff
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ots_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ots_alloc(n)

    def rec(self, mac):
        return ridelayout.host_offset(self.lib, mac)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def out(self, control, st):
        return self.lib.tpw_ots_out(control, st) & MASK

    def restore(self, a):
        return self.lib.tpw_ots_restore(a[0] & MASK, a[1] & MASK, a[2] & MASK, a[3], a[4] & MASK, a[5], s32(a[6]), s32(a[7]), s32(a[8])) & MASK


def s32(v):
    return T.s32(v)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ots_addr.restype, lib.tpw_ots_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ots_alloc.restype, lib.tpw_ots_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ots_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ots_out.restype, lib.tpw_ots_out.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    lib.tpw_ots_restore.restype = C.c_uint32
    lib.tpw_ots_restore.argtypes = [C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint64, C.c_uint32, C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ots_layout(out)
    assert n == len(KEYS), n
    lib.hoff = dict(zip(KEYS, list(out)[:n]))
    ridelayout.bind(lib)
    lib.bound = True


SETSZ = 0x200
FAKE_C = 0x5000


def mk_set(s, d):
    """A set block from a description."""
    a = s.alloc(SETSZ)
    s.put(a, "flags", d["flags"])
    s.put(a, "fe4", d["fe4"])
    s.put(a, "fe8", d["x"])
    s.put(a, "fec", d["z"])
    s.put(a, "ff0", d["w"])
    s.put(a, "ff4", d["dp"])
    s.put(a, "ff8", d["ff8"])
    mesh = s.alloc(0x80)
    s.put(mesh, "f10", d["rot"])
    s.ptr(a + s.off["heads"], mesh)
    return a


def build_restore(s, case):
    types = []
    ta = s.g("types")
    for k, ff8 in enumerate(case["types"]):
        a = mk_set(s, dict(flags=0, fe4=0, x=0, z=0, w=0, dp=0, ff8=ff8, rot=0))
        types.append(a)
        s.ptr(ta + s.psz * (k + 1), a)
    for k, src in enumerate(case["dups"]):
        if src:
            s.ptr(ta + s.psz * (k + 1), types[src - 1])
            types[k] = types[src - 1]
    s.ptr(ta, 0)
    s.words(s.g("typeCount"), [len(types)])
    s.sets = []
    sa = s.g("sets")
    for j, d in enumerate(case["sets"]):
        if d is None:
            s.ptr(sa + s.psz * (j + 1), 0)
            s.sets.append(0)
            continue
        a = mk_set(s, d)
        t = d["tidx"]
        s.ptr(a + s.off["self"], types[t - 1] if 1 <= t <= len(types) else 0x7777)
        s.ptr(sa + s.psz * (j + 1), a)
        s.sets.append(a)
    s.words(s.g("countA"), [case["countA"]])
    s.words(s.g("countB"), [len(case["sets"]) - case["countA"]])
    p = 0
    pair = 0
    if case["params"] is not None:
        pr = case["params"]
        p = s.alloc(0x20)
        s.words(p, [pr["x"], pr["z"], pr["rot"]])
        if pr["f0c"]:
            pair = s.alloc(16)
            s.words(pair, [pr["w"], pr["dp"]])
            s.ptr(p + s.off["f0c"], pair)
    s.ptr(s.g("info") + s.rec(0x18), pair if case["infomatch"] else case["infoword"])
    return p


def exec_restore(s, case):
    p = build_restore(s, case)
    a = case["args"]
    return s.restore([a["inst"], a["type"], a["flags"], p, a["a"], a["c"], a["d"], a["e"], a["f"]])


def obs_restore(s, case, ans):
    return {"answer": ans, "log": s.log(), "flags": [s.get(x, "flags") if x else None for x in s.sets]}


def exec_out(s, case):
    o = case["out"]
    blk = s.alloc(0x38 * 12)
    tcs = blk + 0x38 * 4
    st = s.alloc(SETSZ)
    hdr = s.alloc(0x100)
    s.write(hdr + 0x18, o["name"] + b"\0")
    mesh = s.alloc(0x80)
    s.ptr(mesh + s.off["hdr"], hdr)
    s.ptr(st + s.off["heads"], mesh)
    s.ptr(st + s.off["tc"], tcs)
    s.h16(st + s.off["count"], o["count"])
    control = tcs + o["offset"]
    s.words(control, o["words"])
    return s.out(control, st)


def obs_out(s, case, ans):
    return {"answer": ans, "log": s.log()}


def run(s, case):
    try:
        if case["op"] == "out":
            return obs_out(s, case, exec_out(s, case))
        r = exec_restore(s, case)
        return obs_restore(s, case, r)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------- cases

def fl(r):
    return T.f32_bits(r.choice([0.0, 1.0, -1.5, r.uniform(-300, 300), r.uniform(-1, 1) * 1e5]))


def out_case(r, kind=None, offset=None):
    words = [r.choice([0, 1, 0x40, 0xFFFFFFFF, r.getrandbits(32)]) for _ in range(14)]
    words[1] = kind if kind is not None else r.choice([0, 1, 0xC, 5, r.getrandbits(32)])
    words[7] = fl(r)
    words[8] = fl(r)
    name = bytes(r.randrange(97, 123) for _ in range(r.randrange(0, 20)))
    return {"op": "out", "queues": {}, "out": dict(words=words, offset=offset if offset is not None else r.choice([0, 0x38, 0x38 * 3, -0x38, -0x38 * 2, 5, 0x38 * 3 + 7, -9, 0x38 * 6 + 1]),
                                                   count=r.choice([0, 1, 12, 0xFFFF, r.getrandbits(16)]), name=name)}


def out_edges():
    r = random.Random(11)
    yield out_case(r, kind=0xC)
    for off in (0, 0x38, 0x38 * 7, -0x38, -0x38 * 4, 1, 55, 56, 57, -1, -55, -57, 0x38 * 3 + 27, 111):
        yield out_case(r, kind=1, offset=off)


def out_random(r):
    return out_case(r)


def restore_case(r, mode=None):
    mode = mode or r.choice(["inst", "none", "f0c_info", "f0c_noinfo", "nof0c", "plain", "plain"])
    T_ = r.randrange(1, 4)
    types = [r.choice([0, 1, 0x1234, 0xFFFF, 0x10005, r.getrandbits(32)]) for _ in range(T_)]
    dups = [0] * T_
    if T_ > 1 and r.random() < 0.15:
        dups[T_ - 1] = 1
    n = r.randrange(0, 6) if mode != "inst" else r.randrange(1, 6)
    sets = []
    for _ in range(n):
        if r.random() < 0.15:
            sets.append(None)
            continue
        sets.append(dict(tidx=r.choice([0, 1, 2, 3, 1 + r.randrange(T_)]), flags=r.choice([0, 0x10000, 0x40000, 0x50000, r.getrandbits(32), 0x20]),
                         fe4=r.choice([0, 1, 0x33A, 0x101]), x=r.randrange(-3, 4), z=r.randrange(-3, 4), rot=r.randrange(-2, 3),
                         w=r.randrange(0, 4), dp=r.randrange(0, 4), ff8=r.getrandbits(16)))
    live = [d for d in sets if d is not None]
    tgt = r.choice(live) if live and r.random() < 0.8 else None
    type_ = r.randrange(1, T_ + 1)
    flags = r.choice([0, 1, 0x33A, 0x101])
    pr = dict(x=r.randrange(-3, 4), z=r.randrange(-3, 4), rot=r.randrange(-2, 3), f0c=r.random() < 0.5, w=r.randrange(0, 4), dp=r.randrange(0, 4))
    if tgt is not None:
        tgt["flags"] &= ~0x10000 if r.random() < 0.8 else MASK
        tgt["tidx"] = type_
        tgt["fe4"] = flags
        if r.random() < 0.9:
            pr.update(x=tgt["x"], z=tgt["z"], rot=tgt["rot"], w=tgt["w"], dp=tgt["dp"])
        else:
            pr["x"] += r.choice([0, 1])
        if r.random() < 0.3:
            pr["w"] += 1
    if mode in ("none", "inst"):
        pr = None
    elif mode == "nof0c":
        pr["f0c"] = False
    elif mode.startswith("f0c"):
        pr["f0c"] = True
    inst = 0
    if mode == "inst" and live:
        inst = 1 + sets.index(r.choice(live))
    d = r.choice([0, 0, 2, -1])
    e = r.choice([0, 0, 3, -2])
    ret_info = 1 if mode == "f0c_info" else (0 if mode == "f0c_noinfo" else r.choice([0, 1]))
    return {"op": "restore", "queues": {"GetRideInfo": [ret_info]},
            "types": types, "dups": dups, "sets": sets, "countA": r.randrange(0, n + 1), "params": pr,
            "infomatch": r.random() < 0.6, "infoword": r.getrandbits(32),
            "args": dict(inst=inst, type=type_, flags=flags, a=r.getrandbits(32), c=r.choice([0, FAKE_C]), d=d, e=e, f=r.getrandbits(32))}


def restore_edges():
    r = random.Random(77)
    for mode in ("inst", "none", "f0c_info", "f0c_noinfo", "nof0c", "plain"):
        for _ in range(12):
            yield restore_case(r, mode)


def restore_random(r):
    return restore_case(r)


TESTS = [
    FnTest("OutputAnimTimeStats", ppc, host, edge=out_edges, rnd=out_random),
    FnTest("RestoreRideInstance", ppc, host, edge=restore_edges, rnd=restore_random),
]
