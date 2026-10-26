"""Unit v51028, SaveRides 0x1006acf8, against libtpw_unit_SaveRides_2.dylib (tests/unit_SaveRides_2.cpp), main app
("SimTheme Park"), src/game/rides/TPRide.cpp.

A case is a world and a call. The world (the ride anim sets with their meshes, headers, nodes, embedded data and time controls, the three
counters and the zero word) is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian, 4-byte
pointers) for the original, and in the host library's memory (little-endian, native pointers where the headers have them, Ptr32 offsets
where the file images have them). The callees outside the unit are fakes on both sides: MReleaseAllocF, MReleaseFreeF, CTRACE,
OutputAnimTimeStats, LbFile_Write (which also keeps the bytes it was given) and UDebug. The USwapInplace functions run as the original
on the PowerPC side and are the host's identity swaps on the host.

What is compared: the answer, the event log and every byte written to the file. The original's one word it never swaps (the
time control's +0x14 word) is stored in the machine's own order, so the PowerPC bytes of that word are reversed before the compare
(the host is little-endian: the file's order). Fused operations: none. Not tested: a set with more nodes in the "nodes" part than
the header's all-node count (the original never reads them), a NULL mesh or header (read unconditionally).

The port fix ridepreviewsave (src/game/rides/TPRide.cpp) writes an instance whose type number no loaded ride type carries (the status
panels' ride preview) as an empty slot, with the counts and the first free slot as DestroyRideInstance would leave them. The host's
type table (gRideTypes, which the Mac's SaveRides never reads) is given every slot's set as a type, except the slots a case names in
"unknown". The SaveRides_portfix test runs the host on such a case and the original on the case with those slots emptied and the counts
adjusted the same way (derived); with the fixes off both sides run the derived case.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SaveRides_2"
MASK = 0xFFFFFFFF

SAVE_RIDES = 0x1006acf8
CALLEE_CTRACE = 0x1006fcbc
CALLEE_OATS = 0x1006aa70
CALLEE_ALLOC = 0x10064bd4
CALLEE_FREE = 0x10064c54
CALLEE_UDEBUG = 0x10000e40
CALLEE_WRITE = 0x10238878
OWN = {SAVE_RIDES: 2308}

ADDR = dict(sets=0x102c79a4, A=0x102d19a8, B=0x102d19ac, ff=0x102d19a4, zero=0x1029d050)
FILE = 0x1230

# Mac offsets of the fields the module writes; the host's come from tpw_sr_layout in the same order.
HOST_FIELDS = [
    ("set", "mFlags", 0x04), ("set", "mHeads", 0x08), ("set", "mF0C", 0x0c), ("set", "mTimeCount", 0x0e),
    ("set", "mTimeControls", 0x10), ("set", "mHoardingLevel", 0xb8), ("set", "mFD4", 0xd4), ("set", "mFE4", 0xe4),
    ("set", "mFE8", 0xe8), ("set", "mFEC", 0xec), ("set", "mFF0", 0xf0), ("set", "mFF4", 0xf4), ("set", "mFF8", 0xf8),
    ("mesh", "mHeader", 0x04), ("mesh", "mF10", 0x10), ("mesh", "mF28", 0x28),
    ("eset", "mFlags", 0x00), ("eset", "mRecords", 0x04), ("eset", "mF08", 0x08),
    ("rec", "mFlags", 0x00), ("rec", "mF0C", 0x0c),
]
HOST_SIZES = ["set", "mesh", "eset", "rec", "hdr"]
MAC_SIZES = dict(set=0x100, mesh=0x40, eset=0x14, rec=0x14, hdr=0xc0)
EXPECT_TAIL = [0x42, 0x44, 0x48, 0x70, 0x74, 0xa0, 0x58, 0x38]


def mac_layout():
    f = {"%s.%s" % (a, b): c for a, b, c in HOST_FIELDS}
    f.update({"size." + k: v for k, v in MAC_SIZES.items()})
    return f


MAC = mac_layout()
HOST = None


def host_layout(lib):
    global HOST
    if HOST is None:
        out = (C.c_int64 * 64)()
        n = lib.tpw_sr_layout(out)
        vals = list(out)[:n]
        nf = len(HOST_FIELDS)
        assert vals[nf + len(HOST_SIZES) + 0 - 0 + 0:][:0] == []
        assert vals[nf + len(HOST_SIZES):] == EXPECT_TAIL, vals[nf + len(HOST_SIZES):]
        HOST = {"%s.%s" % (a, b): vals[i] for i, (a, b, _) in enumerate(HOST_FIELDS)}
        HOST.update({"size." + k: vals[nf + i] for i, k in enumerate(HOST_SIZES)})
    return HOST


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def sets_scan(e):
    count = (e.u32(ADDR["A"]) + e.u32(ADDR["B"])) & MASK
    return [e.u32(ADDR["sets"] + 4 * i) for i in range(count + 1)]


def stub_alloc(e):
    size, flags = e.r(3), e.r(4)
    e.cl.rec("alloc", (size, flags))
    e.sr_last = e.alloc(max(size, 4), bytes(max(size, 4)))
    e.set_r(3, e.sr_last)


def stub_free(e):
    e.cl.rec("free", (1 if e.r(3) == getattr(e, "sr_last", None) else 0,))


def stub_oats(e):
    control, set_ = e.r(3), e.r(4)
    table = sets_scan(e)
    idx = table.index(set_, 1) if set_ in table[1:] else -1
    base = e.u32(set_ + MAC["set.mTimeControls"])
    e.cl.rec("oats", (idx, (control - base) // 0x38))


def stub_write(e):
    L = e.cl
    ret, file, buf, n, wp = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
    L.rec("write", (file, n, 1 if wp else 0))
    err = L.ret("werr") & MASK
    e.write(ret, struct.pack(">I", err))
    if wp:
        e.write(wp, struct.pack(">I", L.ret("wn") & MASK))
    e.sr_writes.append(e.read(buf, n))


STUBS.internal(CALLEE_ALLOC, stub_alloc)
STUBS.internal(CALLEE_FREE, stub_free)
STUBS.internal(CALLEE_OATS, stub_oats)
STUBS.internal(CALLEE_WRITE, stub_write)
STUBS.internal(CALLEE_CTRACE, CL.recorder("CTRACE", "us"))
STUBS.internal(CALLEE_UDEBUG, CL.recorder("UDebug", "s"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def f(self, key):
        return self.F[key]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psize = 4

    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, case["queues"])
        e.sr_writes = []
        self.saved = STUBS.activate(e)
        self.F = MAC

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        n = max(n, 4)
        return self.e.alloc(n, bytes(n))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def p32(self, v):
        return v

    def log(self):
        return self.e.cl.log

    def types(self, addrs):
        pass        # the Mac's SaveRides never reads the type table

    def writes(self, case):
        out = [bytearray(b) for b in self.e.sr_writes]
        if len(out) > 1:
            for off in unswapped_offsets(case):
                if off + 4 <= len(out[1]):
                    out[1][off:off + 4] = out[1][off:off + 4][::-1]
        return [bytes(b) for b in out]

    def call(self, case):
        r3, _, _ = T.call_ppc(self.e, SAVE_RIDES, [FILE])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.F = host_layout(lib)
        lib.tpw_sr_reset()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sr_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sr_alloc(max(n, 4))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def p32(self, v):
        return self.lib.tpw_sr_off(v) if v else 0

    def log(self):
        return CL.host_end(self.lib)

    def types(self, addrs):
        table = self.g("types")
        for i, addr in enumerate(addrs):
            self.ptr(table + self.psize * (i + 1), addr)
        self.words(self.g("typeCount"), [len(addrs)])

    def writes(self, case):
        out = []
        for i in range(self.lib.tpw_sr_nwrites()):
            n = self.lib.tpw_sr_wlen(i)
            buf = C.create_string_buffer(max(n, 1))
            self.lib.tpw_sr_wdata(i, buf)
            out.append(buf.raw[:n])
        return out

    def call(self, case):
        return self.lib.tpw_sr_save(FILE) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sr_addr.restype, lib.tpw_sr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sr_alloc.restype, lib.tpw_sr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sr_off.restype, lib.tpw_sr_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_sr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sr_nwrites.restype = C.c_int32
    lib.tpw_sr_wlen.restype, lib.tpw_sr_wlen.argtypes = C.c_int32, [C.c_int32]
    lib.tpw_sr_wdata.argtypes = [C.c_int32, C.c_void_p]
    lib.tpw_sr_save.restype, lib.tpw_sr_save.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_sr_portfix.restype, lib.tpw_sr_portfix.argtypes = C.c_int32, []
    lib.port_fix = lib.tpw_sr_portfix() == 1
    lib.bound = True


def unswapped_offsets(case):
    """Offsets in the block of the time controls' +0x14 words (the one word the original does not swap)."""
    off = 0xc
    res = []
    for d in case["slots"]:
        off += 1
        if d is None:
            continue
        off += 0x36
        if d["embedded"] is not None:
            off += d["ec"] * 8
        off += d["n_all"] * 4
        for _ in d["tcs"]:
            res.append(off + 0x10)
            off += 0x2c
    return res


def make_set(s, d):
    F = s.F
    hdr = s.alloc(F["size.hdr"])
    nodes = s.alloc(max(d["n_nodes"], 1) * 0xa0)
    minis = s.alloc(max(d["n_all"] - d["n_nodes"], 1) * 0x58)
    for i in range(d["n_nodes"]):
        s.words(nodes + 0xa0 * i, [d["nodeflags"][i]])
    for i in range(d["n_nodes"], d["n_all"]):
        s.words(minis + 0x58 * (i - d["n_nodes"]), [d["nodeflags"][i]])
    s.half(hdr + 0x42, d["n_all"])
    s.half(hdr + 0x44, d["n_nodes"])
    s.half(hdr + 0x48, d["ec"])
    s.words(hdr + 0x70, [s.p32(nodes)])
    s.words(hdr + 0x74, [s.p32(minis)])
    eset = 0
    if d["embedded"] is not None:
        e = d["embedded"]
        eset = s.alloc(F["size.eset"])
        recs = s.alloc(max(d["ec"], 1) * F["size.rec"])
        s.words(eset + F["eset.mFlags"], [e["flags"]])
        s.ptr(eset + F["eset.mRecords"], recs)
        s.words(eset + F["eset.mF08"], [e["f08"]])
        for i, (fl, f0c) in enumerate(e["recs"]):
            s.words(recs + i * F["size.rec"] + F["rec.mFlags"], [fl])
            s.words(recs + i * F["size.rec"] + F["rec.mF0C"], [f0c])
    mesh = s.alloc(F["size.mesh"])
    s.ptr(mesh + F["mesh.mHeader"], hdr)
    s.words(mesh + F["mesh.mF10"], [d["mesh10"]])
    s.ptr(mesh + F["mesh.mF28"], eset)
    tcs = s.alloc(max(len(d["tcs"]), 1) * 0x38)
    for i, words in enumerate(d["tcs"]):
        s.words(tcs + 0x38 * i, words)
    st = s.alloc(F["size.set"])
    s.words(st + F["set.mFlags"], [d["flags"]])
    s.ptr(st + F["set.mHeads"], mesh)
    s.half(st + F["set.mF0C"], d["f0c"])
    s.half(st + F["set.mTimeCount"], len(d["tcs"]))
    s.ptr(st + F["set.mTimeControls"], tcs)
    s.words(st + F["set.mHoardingLevel"], [d["level"]])
    for name in ("mFD4", "mFE4", "mFE8", "mFEC", "mFF0", "mFF4", "mFF8"):
        s.words(st + F["set." + name], [d[name]])
    return st


def build(s, case):
    s.words(s.g("A"), [case["A"]])
    s.words(s.g("B"), [case["B"]])
    s.words(s.g("ff"), [case["ff"]])
    s.words(s.g("zero"), [case["zero"]])
    table = s.g("sets")
    types = []
    unknown = set(case.get("unknown", []))
    for i, d in enumerate(case["slots"]):
        addr = make_set(s, d) if d is not None else 0
        s.ptr(table + s.psize * (i + 1), addr)
        if d is not None and i not in unknown:
            types.append(addr)
    s.types(types)


def run(s, case):
    try:
        build(s, case)
        answer = s.call(case)
        return {"answer": answer, "log": s.log(), "writes": s.writes(case)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def derived(case):
    """The case as the port fix writes it: the unknown slots empty, the counts and the first free slot as DestroyRideInstance keeps
    them (the empty slots at the end go, the first free slot is the lowest empty one)."""
    unknown = set(case.get("unknown", []))
    if not unknown:
        return case
    slots = [None if i in unknown else d for i, d in enumerate(case["slots"])]
    total = (case["B"] + case["A"]) & MASK
    b = case["B"] - len(unknown) if len(unknown) < case["B"] else 0
    a = (total - b) & MASK
    while a != 0 and slots[(b + a - 1) & MASK] is None:
        a -= 1
    n = (b + a) & MASK
    ff = next((i for i in range(n) if slots[i] is None), n)
    return dict(case, slots=slots[:n], A=a, B=b, ff=ff, unknown=[])


def fix_ppc(e, case):
    return ppc(e, derived(case))


def fix_host(lib, case):
    bind(lib)
    return host(lib, case if lib.port_fix else derived(case))


SPECIAL = [0, 0x80000000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000, 0xFFFFFFFF]


def rw(r):
    return r.choice(SPECIAL) if r.random() < 0.15 else r.getrandbits(32)


def rf(r):
    k = r.random()
    if k < 0.15:
        return r.choice([0, 0x80000000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0x00800000])
    return fb(r.uniform(-300, 300))


def make_tc(r, kind=None):
    w = [rw(r) for _ in range(14)]
    w[1] = 0xC if (kind == "c" or (kind is None and r.random() < 0.3)) else rw(r)
    for i in (3, 7, 8, 12):
        w[i] = rf(r)
    w[13] = 0
    return w


def make_node_flags(r, n):
    return [r.getrandbits(32) for _ in range(n)]


def make_setdesc(r, flags=None, ntc=None, n_all=None, n_nodes=None, ec=None, embedded=None):
    n_all = r.randrange(0, 5) if n_all is None else n_all
    n_nodes = r.randrange(0, n_all + 1) if n_nodes is None else n_nodes
    ec = r.randrange(0, 4) if ec is None else ec
    ntc = r.randrange(0, 4) if ntc is None else ntc
    if embedded is None:
        embedded = r.random() < 0.5
    d = {
        "flags": r.getrandbits(32) if flags is None else flags,
        "f0c": r.getrandbits(16), "level": rf(r),
        "mFD4": rw(r), "mFE4": rw(r), "mFE8": rw(r), "mFEC": rw(r), "mFF0": rw(r), "mFF4": rw(r), "mFF8": rw(r),
        "mesh10": rw(r), "n_all": n_all, "n_nodes": n_nodes, "ec": ec,
        "nodeflags": make_node_flags(r, n_all),
        "embedded": None, "tcs": [make_tc(r) for _ in range(ntc)],
    }
    if embedded:
        d["embedded"] = {"flags": rw(r), "f08": rw(r), "recs": [(rw(r), rw(r)) for _ in range(ec)]}
    return d


def base(r, slots, err=0, zero=0, wn=4):
    return {
        "op": "save", "A": len(slots) - len(slots) // 2, "B": len(slots) // 2, "slots": slots,
        "ff": rw(r), "zero": zero,
        "queues": {"werr": [err, err], "wn": [wn]},
    }


def random_case(r):
    n = r.randrange(0, 5)
    slots = [None if r.random() < 0.3 else make_setdesc(r) for _ in range(n)]
    return base(r, slots, err=r.choice([0, 0, 0, -1, 5, 0x7FFFFFFF, -0x80000000]), zero=r.choice([0, 0, 0, 1, 0x80000000]),
                wn=r.choice([4, 4, 4, 4, 5, 7, 0, 3, 8, 16]))


def edges():
    r = random.Random(7)
    yield base(r, [])
    yield base(r, [None])
    # every bit of the set flags and the node flags, alone and together
    for fl in [0] + [1 << i for i in range(32)] + [0xFFFFFFFF]:
        d = make_setdesc(r, flags=fl, ntc=1, n_all=2, n_nodes=1, ec=1, embedded=True)
        d["nodeflags"] = [fl, fl]
        yield base(r, [d])
    for embedded in (True, False):
        for ec in (0, 1, 3):
            for n_all, n_nodes in ((0, 0), (3, 3), (3, 0), (3, 1), (2, 2)):
                yield base(r, [make_setdesc(r, ntc=2, n_all=n_all, n_nodes=n_nodes, ec=ec, embedded=embedded), None,
                               make_setdesc(r, ntc=1, n_all=1, n_nodes=1, ec=1, embedded=embedded)])
    # time controls of kind 0xc and not
    d = make_setdesc(r, ntc=0)
    d["tcs"] = [make_tc(r, "c"), make_tc(r, None), make_tc(r, "c")]
    d["tcs"][1][1] = 0
    yield base(r, [d])
    # the writes' answers
    for err in (0, -1, 1):
        for zero in (0, 1, 0xFFFFFFFF):
            for wn in (4, 3, 8, 5, 7, 0):
                yield base(r, [make_setdesc(r)], err=err, zero=zero, wn=wn)


def with_unknown(r, case, unknown):
    """The case with the given slots as instances no loaded type carries the number of (type number 0, the preview's; the others' never 0)."""
    for i, d in enumerate(case["slots"]):
        if d is None:
            continue
        if i in unknown:
            d["mFF8"] = 0
        elif d["mFF8"] == 0:
            d["mFF8"] = r.randrange(1, 1 << 32)
    case["unknown"] = [i for i in unknown if case["slots"][i] is not None]
    return case


def fix_edges():
    r = random.Random(11)

    def slots(pattern):
        return [make_setdesc(r) if ch == "r" else None for ch in pattern]

    # the preview at the end, in the middle, at the start, next to holes, with holes after it, alone, two of them, every slot
    for pattern, unknown in (("rrr", [2]), ("rrr", [1]), ("rrr", [0]), ("r.r.r", [2]), ("rr..", [1]), ("r", [0]), ("rrrr", [1, 3]),
                             ("rr", [0, 1]), ("r.r", [2]), (".r", [1]), ("rrr.r", [4]), ("rrr.r", [3]), ("", [])):
        yield with_unknown(r, base(r, slots(pattern)), unknown)
    # the counts of the file are not tied to the holes in a test: a used count that is 0 and a used count that is all the slots
    c = with_unknown(r, base(r, slots("rr.")), [0])
    c["A"], c["B"] = 3, 0
    yield c
    c = with_unknown(r, base(r, slots("rr.")), [0])
    c["A"], c["B"] = 0, 3
    yield c


def fix_random_case(r):
    case = random_case(r)
    present = [i for i, d in enumerate(case["slots"]) if d is not None]
    unknown = [i for i in present if r.random() < 0.4]
    return with_unknown(r, case, unknown)


TESTS = [
    FnTest("SaveRides", ppc, host, edge=edges, rnd=random_case),
    FnTest("SaveRides_portfix", fix_ppc, fix_host, edge=fix_edges, rnd=fix_random_case),
]
