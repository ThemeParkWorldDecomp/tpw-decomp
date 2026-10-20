"""Unit v5890, LoadRides (the rides module of a save), against libtpw_unit_LoadRides_2.dylib (tests/unit_LoadRides_2.cpp), main app
("SimTheme Park"), src/RestoreAnimTime_LoadRides_2.cpp:
  LoadRides 0x1006b878

A case is a world and a call. The world is built twice by ONE piece of Python code (the shared build): in emulator memory at the Mac's
addresses (big-endian) for the original, and in the host library's memory for the translation (the host's ride sets have native pointers, so
its blocks are made by the library's helpers from the same specs). The callees outside the unit are fakes on both sides that record their
arguments: LbFile_Read (answers from the case's file image and error queue), MReleaseAllocF, MReleaseFreeF, TickRideAnims, GetRideInfo,
InstanceRide (two events; answers an instance number from the queue), ERRORBox, CTRACE, UDebug, ReactivateHoardings, RestoreAnimTime (two
events: the place and the 11 saved words), OutputAnimTimeStats, StaticifyCreateOnlyAnimNodes, OverrideActiveChannels, GetNumActiveChannels.
The swap functions (USwapInplace32 ...) run as the original on the PowerPC side; the save file is little endian, so the Mac swaps every word
and the host (little endian) leaves them: the case's file holds little-endian numbers and both sides see the same values. The one word the
original does not swap (the save's +0x10 of a time control) is read byte-reversed on the PowerPC side for the log, so it names the same number.

The world of a case: the three ride count globals, the type table (gRideTypes, gRideTypeCount), three ride instances in gRideAnimSets with
their meshes, headers, node arrays, embedded sets and time controls, two ride database records, the TbError zero word, the fallback shape
words, the file image. What is compared: the answer, the event log, the counts, the shape words and every word of each instance the call may
change.

No fused operations. Not tested: a record count past the end of the file's buffer (both sides read zeros then; the markers are 0).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LoadRides_2"
MASK = 0xFFFFFFFF

FN = 0x1006B878
SIZES = {FN: 2200}
OWN = set(SIZES)

ADDR = dict(A=0x102D19A8, B=0x102D19AC, FF=0x102D19A4, TC=0x102D1ED8, types=0x102D19B0, sets=0x102C79A4,
            shape=0x102C6D04, zero=0x1029D054, db=0x1037F6C8)
HOST_LAYOUT = [0x38, 0x2C]
K = 3          # instances 1 .. K


def s32(v):
    return T.s32(v)


def bswap(v):
    return struct.unpack("<I", struct.pack(">I", v & MASK))[0]


ST = {}


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def slot(addr):
    return ST["slots"].get(addr, 0)


def stub_read(e):
    L = e.cl
    out, fh, buf, n, tp = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
    take = ST["file"][ST["pos"]:ST["pos"] + n]
    ST["pos"] += len(take)
    if take:
        e.write(buf, take)
    if tp:
        e.write(tp, struct.pack(">I", len(take)))
    L.rec("LbFile_Read", (fh, n, 1 if tp else 0, len(take)))
    err = L.ret("LbFile_Read")
    e.write(out, struct.pack(">i", s32(err)))
    e.set_r(3, out)


def stub_alloc(e):
    size, flags = e.r(3), e.r(4)
    addr = e.alloc(size + 0x400)
    e.write(addr, bytes(size + 0x400))
    ST["block"] = addr
    e.cl.rec("MReleaseAllocF", (size, flags))
    e.set_r(3, addr)


def stub_free(e):
    e.cl.rec("MReleaseFreeF", (1 if e.r(3) == ST["block"] else 0,))
    e.set_r(3, 0)


def stub_getinfo(e):
    L = e.cl
    L.rec("GetRideInfo", (L.lab(e.r(3)), e.r(4) >> 16))
    k = L.ret("GetRideInfo")
    e.set_r(3, ST["infos"][k - 1] if 1 <= k <= 2 else 0)


def stub_instance(e):
    L = e.cl
    sp = e.r(1)
    p = e.r(5)
    x, z, rot, shape = e.u32(p), e.u32(p + 4), e.u32(p + 8), e.u32(p + 12)
    L.rec("InstanceRide", (e.r(3), e.r(4), s32(x), s32(z), s32(rot), e.r(6), s32(e.r(7)), e.r(8)))
    if shape == ADDR["shape"]:
        sh, s0, s1 = -1, s32(e.u32(shape)), s32(e.u32(shape + 4))
    else:
        sh, s0, s1 = shape, 0, 0
    L.rec("InstanceRide2", (s32(e.r(9)), s32(e.r(10)), e.u32(sp + 0x38), s32(e.u32(sp + 0x3C)), sh, s0, s1))
    e.set_r(3, L.ret("InstanceRide") & MASK)


def stub_reactivate(e):
    e.cl.rec("ReactivateHoardings", (slot(e.r(3)),))


def stub_restore(e):
    L = e.cl
    st, ctl, sv = e.r(3), e.r(4), e.r(5)
    L.rec("RestoreAnimTime", (slot(st), (ctl - e.u32(st + 0x10)) // 0x38, sv - ST["block"]))
    w = list(struct.unpack(">11I", e.read(sv, 0x2C)))
    w[4] = bswap(w[4])          # the word the original does not swap
    L.rec("RestoreAnimTimeSave", tuple(w))
    e.set_r(3, 1 if L.ret("RestoreAnimTime") else 0)


def stub_output(e):
    st, ctl = e.r(4), e.r(3)
    e.cl.rec("OutputAnimTimeStats", ((ctl - e.u32(st + 0x10)) // 0x38, slot(st)))


def one_slot(name):
    def fn(e):
        e.cl.rec(name, (slot(e.r(3)),))
    return fn


STUBS.internal(0x10238848, stub_read)
STUBS.internal(0x10064BD4, stub_alloc)
STUBS.internal(0x10064C54, stub_free)
STUBS.internal(0x100C6964, CL.recorder("TickRideAnims"))
STUBS.internal(0x10154868, stub_getinfo)
STUBS.internal(0x10069B24, stub_instance)
STUBS.internal(0x1006FD3C, CL.recorder("ERRORBox", "ss"))
STUBS.internal(0x1006FCBC, CL.recorder("CTRACE", "is"))
STUBS.internal(0x10000E40, CL.recorder("UDebug", "s"))
STUBS.internal(0x10061840, stub_reactivate)
STUBS.internal(0x1006B6F4, stub_restore)
STUBS.internal(0x1006AA70, stub_output)
STUBS.internal(0x100C6B54, one_slot("StaticifyCreateOnlyAnimNodes"))
STUBS.internal(0x100C609C, one_slot("OverrideActiveChannels"))
STUBS.internal(0x100C5FD4, one_slot("GetNumActiveChannels"))


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


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        ST.clear()
        ST.update(slots={}, infos=[], file=case["file"], pos=0, block=0)
        self.L = e.cl = CL.PpcLog({ADDR["db"]: 1}, case["queues"])
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

    def set_globals(self, c):
        self.words(ADDR["A"], [c["A0"]])
        self.words(ADDR["B"], [c["B0"]])
        self.words(ADDR["FF"], [c["FF0"]])
        self.words(ADDR["TC"], [len(c["types"])])
        self.words(ADDR["zero"], [c["zero"]])
        self.words(ADDR["shape"], [c["sh0"], c["sh1"]])

    def make_set(self, sp):
        a, mesh, hdr = self.alloc(0x100), self.alloc(0x40), self.alloc(0xB8)
        nodes = self.alloc(max(sp["nodes"], 1) * 0xA0)
        minis = self.alloc(max(sp["allN"] - sp["nodes"], 1) * 0x58)
        ctl = self.alloc(max(sp["tc"], 1) * 0x38)
        es = recs = 0
        if sp["hasEmbed"]:
            es = self.alloc(0x14)
            recs = self.alloc(max(sp["emb"], 1) * 0x14)
            self.words(es, [sp["ehdr"][0], recs, sp["ehdr"][1]])
            for i, (x, y) in enumerate(sp["erecs"]):
                self.words(recs + i * 0x14, [x])
                self.words(recs + i * 0x14 + 0xC, [y])
        self.words(a + 4, [sp["flags"], mesh, (sp["f0c"] << 16) | sp["tc"], ctl])
        self.words(a + 0xB8, [sp["level"]])
        self.words(a + 0xF8, [sp["ff8"]])
        self.words(mesh + 4, [hdr])
        self.words(mesh + 0x28, [es])
        self.write(hdr + 0x18, sp["name"].encode() + b"\0")
        self.write(hdr + 0x42, struct.pack(">H", sp["allN"]))
        self.write(hdr + 0x44, struct.pack(">H", sp["nodes"]))
        self.write(hdr + 0x48, struct.pack(">H", sp["emb"]))
        self.words(hdr + 0x70, [nodes, minis])
        for i, v in enumerate(sp["nflags"]):
            self.words(nodes + i * 0xA0 if i < sp["nodes"] else minis + (i - sp["nodes"]) * 0x58, [v])
        for i, v in enumerate(sp["f04"]):
            self.words(ctl + i * 0x38 + 4, [v])
        return dict(addr=a, sp=sp, nodes=nodes, minis=minis, es=es, recs=recs, ctl=ctl)

    def install(self, kind, index, obj):
        if kind == 0:
            ST["slots"][obj["addr"]] = index
            self.words(ADDR["sets"] + 4 * index, [obj["addr"]])
        else:
            self.words(ADDR["types"] + 4 * index, [obj["addr"]])

    def set_info(self, k, shape):
        a = self.alloc(0x100)
        self.words(a + 0x18, [shape])
        ST["infos"].append(a)

    def set_file(self, data):
        pass

    def dump(self, obj):
        sp, a = obj["sp"], obj["addr"]
        out = [self.rd_word(a + 4), struct.unpack(">H", self.read(a + 0xC, 2))[0], self.rd_word(a + 0xB8)]
        for i in range(sp["allN"]):
            out.append(self.rd_word(obj["nodes"] + i * 0xA0 if i < sp["nodes"] else obj["minis"] + (i - sp["nodes"]) * 0x58))
        if sp["hasEmbed"]:
            out += [self.rd_word(obj["es"]), self.rd_word(obj["es"] + 8)]
            for i in range(sp["emb"]):
                out += [self.rd_word(obj["recs"] + i * 0x14), self.rd_word(obj["recs"] + i * 0x14 + 0xC)]
        for i in range(sp["tc"]):
            out.append(self.rd_word(obj["ctl"] + i * 0x38 + 4))
        return out

    def globals_out(self):
        return [self.rd_word(ADDR[k]) for k in ("A", "B", "FF")] + self.rd_words(ADDR["shape"], 2)

    def run(self, fh):
        with_r3 = T.call_ppc(self.e, FN, [fh])[0]
        return s32(with_r3 & MASK)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: lib.tpw_ld_addr(b"db")}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_ld_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_globals(self, c):
        self.lib.tpw_ld_globals(c["A0"], c["B0"], c["FF0"], len(c["types"]), s32(c["zero"]), s32(c["sh0"]), s32(c["sh1"]))

    def make_set(self, sp):
        L = self.lib
        a = L.tpw_ld_make_set(sp["allN"], sp["nodes"], sp["emb"], 1 if sp["hasEmbed"] else 0, sp["tc"], sp["name"].encode(),
                              sp["flags"], sp["f0c"], sp["level"], sp["ff8"])
        for i, v in enumerate(sp["nflags"]):
            L.tpw_ld_node_flags(a, i, v)
        if sp["hasEmbed"]:
            L.tpw_ld_embed(a, -1, sp["ehdr"][0], sp["ehdr"][1])
            for i, (x, y) in enumerate(sp["erecs"]):
                L.tpw_ld_embed(a, i, x, y)
        for i, v in enumerate(sp["f04"]):
            L.tpw_ld_control(a, i, v)
        return dict(addr=a, sp=sp)

    def install(self, kind, index, obj):
        self.lib.tpw_ld_install(kind, index, obj["addr"])

    def set_info(self, k, shape):
        self.lib.tpw_ld_info(k, shape)

    def set_file(self, data):
        self.lib.tpw_ld_file(data, len(data))

    def dump(self, obj):
        out = (C.c_uint32 * 256)()
        n = self.lib.tpw_ld_dump(obj["addr"], out)
        return list(out)[:n]

    def globals_out(self):
        out = (C.c_uint32 * 8)()
        self.lib.tpw_ld_globals_out(out)
        return list(out)[:5]

    def run(self, fh):
        return self.lib.tpw_ld_run(fh)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, u32, i32, vp = C.c_uint64, C.c_uint32, C.c_int32, C.c_void_p
    sig = {
        "addr": (u64, [C.c_char_p]), "alloc": (u64, [u32]), "layout": (i32,[C.POINTER(C.c_int64)]),
        "make_set": (u64, [i32, i32, i32, i32, i32, C.c_char_p, u32, u32, u32, u32]),
        "node_flags": (None, [u64, i32, u32]), "embed": (None, [u64, i32, u32, u32]), "control": (None, [u64, i32, u32]),
        "install": (None, [i32, i32, u64]), "globals": (None, [u32, u32, u32, u32, i32, i32, i32]), "info": (None, [i32, u64]),
        "file": (None, [C.c_char_p, u32]), "dump": (u32, [u64, C.POINTER(C.c_uint32)]),
        "globals_out": (None, [C.POINTER(C.c_uint32)]), "run": (i32, [u64]),
    }
    for name, (res, args) in sig.items():
        f = getattr(lib, "tpw_ld_" + name)
        f.restype, f.argtypes = res, args
    out = (C.c_int64 * 32)()
    n = lib.tpw_ld_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.set_globals(case)
    objs = []
    for k, sp in enumerate(case["sets"], 1):
        obj = s.make_set(sp)
        s.install(0, k, obj)
        objs.append(obj)
    for i, ff8 in enumerate(case["types"], 1):
        s.install(1, i, s.make_set(dict(allN=0, nodes=0, emb=0, hasEmbed=False, tc=0, name="t", flags=0, f0c=0, level=0, ff8=ff8,
                                        nflags=[], ehdr=(0, 0), erecs=[], f04=[])))
    for k, shape in enumerate(case["infos"], 1):
        s.set_info(k, shape)
    s.set_file(case["file"])
    return objs


def run(s, case):
    try:
        objs = build(s, case)
        answer = s.run(case["fh"])
        return {"answer": answer, "log": s.log(), "globals": s.globals_out(), "sets": [s.dump(o) for o in objs]}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rword(r):
    return r.choice([0, 0xFFFFFFFF, 0x80000000, 1, r.getrandbits(32), r.getrandbits(32), r.getrandbits(12)])


def rlevel(r):
    return r.choice([0, 0x80000000, 0x3F800000, 0x7F800000, T.f32_bits(r.uniform(-3, 3)), T.f32_bits(r.uniform(-1, 1))])


def rand_spec(r, tc=None, has_embed=None):
    allN = r.randrange(0, 5)
    nodes = r.randrange(0, allN + 1)
    emb = r.randrange(0, 4)
    tc = r.randrange(0, 4) if tc is None else tc
    return dict(allN=allN, nodes=nodes, emb=emb, hasEmbed=(r.random() < 0.7) if has_embed is None else has_embed, tc=tc,
                name=r.choice(["Ride", "Carousel", "X" * 0x12, ""]), flags=r.choice([0, 0xFFFFFFFF, 0x1000000, rword(r), 0x400000 | 0x20]),
                f0c=r.getrandbits(16), level=rlevel(r), ff8=0, nflags=[rword(r) for _ in range(allN)],
                ehdr=(rword(r), rword(r)), erecs=[(rword(r), rword(r)) for _ in range(emb)],
                f04=[r.choice([0, 0xB, 0xC, 0xD, 5, 0x10]) for _ in range(tc)])


def record(r, c, inst, typ, match):
    sp = c["sets"][inst - 1]
    allN, emb = (sp["allN"], sp["emb"]) if match else (r.randrange(0, 5), r.randrange(0, 4))
    b = bytes([1]) + struct.pack("<I", typ)
    b += struct.pack("<iiiiIiI", r.choice([0, -5, 77, r.getrandbits(31)]), r.choice([0, 9, -1]), r.choice([0, 3, -7]),
                     r.choice([0, 4, -2]), rword(r), r.choice([0, 1, -1, 99]), r.choice([0, 0x1FF, 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100,
                                                                                         rword(r) & 0x1FF]))
    b += struct.pack("<HI", r.getrandbits(16), rlevel(r))
    assert len(b) == 1 + 0x26
    b += struct.pack("<IHHII", rword(r), allN, emb, rword(r), rword(r))
    b += b"".join(struct.pack("<II", rword(r), rword(r)) for _ in range(emb))
    b += b"".join(struct.pack("<I", r.choice([0, 0xFFFFFFFF, rword(r), r.getrandbits(12)])) for _ in range(allN))
    for _ in range(sp["tc"]):
        w = [rword(r) for _ in range(11)]
        w[1] = r.choice([0xC, 0, 5, 0xB, 0xD, rword(r)])
        b += struct.pack("<11I", *w)
    return b


def make_case(r, n=None, err=None, zero=None, short=None, match=None, has_embed=None, tc=None, restore=None, markers=None, size=None,
              counts=None, tcount=None):
    c = {}
    c["sets"] = [rand_spec(r, tc, has_embed) for _ in range(K)]
    nt = r.randrange(0, 4) if tcount is None else tcount
    c["types"] = [r.choice([1, 2, 3, 0x10005, 0xFFF0, 0x20009, r.getrandbits(32)]) for _ in range(nt)]
    c["infos"] = [r.choice([0, 0x7001, 0x7FFF0, r.randrange(1, 1 << 30)]) for _ in range(2)]
    c["A0"], c["B0"], c["FF0"] = rword(r), rword(r), rword(r)
    c["sh0"], c["sh1"] = rword(r), rword(r)
    c["zero"] = r.choice([0, 0, 1, 0x100]) if zero is None else zero
    c["fh"] = 0x5100 + r.randrange(8)
    n = r.choice([0, 1, 2, 2, 3, 4]) if n is None else n
    recs = b""
    inst_q = []
    for i in range(n):
        m = r.choice([1, 1, 1, 1, 0, 2, 255]) if markers is None else markers[i % len(markers)]
        if m != 1:
            recs += bytes([m])
            continue
        inst = r.randrange(1, K + 1)
        inst_q.append(inst)
        typ = r.choice(c["types"] + [r.getrandbits(32), 0x10005]) if c["types"] else r.choice([1, 0x10005, r.getrandbits(32)])
        recs += record(r, c, inst, typ, (r.random() < 0.75) if match is None else match)
    a = r.randrange(0, n + 1)
    cb, ca = n - a, a
    if counts is not None:
        cb, ca = counts
    body = struct.pack("<III", cb, ca, rword(r)) + recs
    sz = len(body) if size is None else size
    c["file"] = struct.pack("<I", sz) + body
    if short is not None:
        c["file"] = c["file"][:short]
    e2 = r.choice([0, 0, -1, 5, 0x7FFFFFFF, -0x80000000]) if err is None else err
    q = {"LbFile_Read": [r.choice([0, -1, 3]), e2], "InstanceRide": inst_q or [1],
         "GetRideInfo": [r.choice([0, 1, 2]) for _ in range(4)],
         "RestoreAnimTime": [r.choice([0, 1]) for _ in range(3)] if restore is None else restore}
    c["queues"] = q
    return c


def edges():
    r = random.Random(5890)
    for short in (0, 1, 3):
        yield make_case(r, short=short)
    yield make_case(r, n=0, size=0)
    for err in (0, -1, 1, 5):
        for zero in (0, 1):
            yield make_case(r, n=1, err=err, zero=zero, markers=[1])
    for m in (0, 2, 255):
        yield make_case(r, n=3, markers=[m, 1, m])
    yield make_case(r, n=2, counts=(0xFFFFFFFF, 1), markers=[1])
    yield make_case(r, n=3, markers=[1], match=True, err=0, zero=0, restore=[0])
    yield make_case(r, n=3, markers=[1], match=True, err=0, zero=0, restore=[1])
    yield make_case(r, n=2, markers=[1], match=False, err=0, zero=0)
    yield make_case(r, n=2, markers=[1], match=True, tc=0, err=0, zero=0)
    yield make_case(r, n=2, markers=[1], match=True, tc=2, err=0, zero=0)
    yield make_case(r, n=2, markers=[1], match=True, has_embed=False, err=0, zero=0)
    yield make_case(r, n=2, markers=[1], match=True, has_embed=True, err=0, zero=0)
    yield make_case(r, n=2, markers=[1], tcount=0, err=0, zero=0)
    yield make_case(r, n=3, markers=[1], size=8, err=0, zero=0)
    for _ in range(30):
        yield make_case(r, n=r.randrange(1, 4), markers=[1], err=0, zero=0)


def rnd(r):
    return make_case(r, err=r.choice([0, 0, 0, 0, -1, 5]), zero=r.choice([0, 0, 0, 0, 1]))


TESTS = [
    FnTest("LoadRides", ppc, host, edge=edges, rnd=rnd),
]
