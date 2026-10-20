"""Unit v5372, LoadCoasters (reads the coasters of a park file), against libtpw_unit_LoadCoasters.dylib (tests/unit_LoadCoasters.cpp), main app
("SimTheme Park"), src/LoadCoasters.cpp:
  LoadCoasters 0x10044d68

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian, 4-byte
pointers) for the original, and in the host library's memory (little-endian, 8-byte pointers, the host structs' offsets) for the translation. The
callees outside the unit are fakes on both sides that record their arguments: LbFile_Read (hands out the case's file bytes and answers how many it
gave), UDebug (the text), InstanceCoasterTrains, PlaceEntrancePylons, AllocateObjectInstanceToTable (the table's three counters), SetPlaceLoop,
PlacePylon (writes the pylon and brace it answers from the case's queues), SetLoft/Rotate/WobblePylon, RecalcTrackParams,
UpdatePylonStackMeshes, PlaceEndPylons, SetCoasterEditState, SetCoasterState, GetTrackBrace (answers from a queue), LoadTrainsAndCars,
SetTrainCapacity, ClearCoasterButtons and FlashEndPylons. The swaps run as the original on the PowerPC side (the file is little-endian) and are no-ops on the host.

The world of a case: the file bytes (cut short for the read failures), 1 to 3 coaster instances (list, type, brace chain, pylon list with braces
and tables), a pool of braces PlacePylon answers, the ride sets' heights, the object table's counters, the action recorder's flag. What is compared:
the answer, the event log, the counters, the globals, every instance word the function writes, every brace (flags, count, words, the pointers it
writes as offsets inside their object).

No fused operations. Not tested: a record whose entrance matches no instance (the original reads address 0), brace word counts above 10 (the host
brace array holds 11 words).
"""
import ctypes as C
import random
import struct
import types

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LoadCoasters"
MASK = 0xFFFFFFFF

FN = 0x10044D68
SIZES = {FN: 2848}
OWN = set(SIZES)

LBREAD, UDEBUG, INSTTR, PLACEENT, ALLOC = 0x10238848, 0x10000E40, 0x10042084, 0x10040ACC, 0x101EF1F4
SETLOOP, PLACEPYL, LOFT, ROT, WOB = 0x10054554, 0x10041C5C, 0x10055E0C, 0x10055F38, 0x1005608C
RECALC, UPDATE, PLACEEND, EDITST, COASTST = 0x10050F88, 0x1003D958, 0x100424F0, 0x10053DE8, 0x10042BB8
GETBRACE, LOADTR, TRAINCAP, CLEARBTN, FLASH = 0x1004DE40, 0x10043E8C, 0x1004A328, 0x10053D88, 0x10052158

ADDR = dict(skip=0x102B68A0, cur=0x102B68A4, edit=0x102B68B0, list=0x102B68B4, table=0x102B6F04, lowest=0x102B7304,
            first=0x102B7308, count=0x102B730C, sets=0x102C79A4, rec=0x1037EC44 + 0x24)

# The Mac offsets, in the order of tpw_LC_layout's host offsets.
NAMES = ["i_flags", "i_type", "i_state", "i_entrance", "i_objid", "i_next", "i_f2c", "i_block", "i_pylons", "i_last", "i_braces",
         "i_prev60", "i_b64", "i_ff0", "i_trains", "i_f140", "i_size",
         "b_number", "b_flags", "b_prev", "b_fb0", "b_fb4", "b_f70", "b_f90", "b_f74", "b_data", "b_size",
         "y_flags", "y_control", "y_next", "y_count", "y_braces", "y_size",
         "d_table", "d_size", "t_f2f4", "t_size", "rs_heads", "mesh_hdr", "hdr_f78"]
MAC = dict(zip(NAMES, [0, 4, 8, 0x10, 0x14, 0x18, 0x2C, 0x3C, 0x4C, 0x54, 0x58, 0x60, 0x64, 0xF0, 0xF4, 0x140, 0x14C,
                       0, 8, 0x64, 0xB0, 0xB4, 0x70, 0x90, 0x74, 0x100, 0x120,
                       4, 0x18, 0x28, 0x30, 0x34, 0x48,
                       0x18, 0x40, 0x2F4, 0x360, 8, 4, 0x78]))
BIG = 0x400


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def regs(e, spec):
    """The arguments by PowerOpen placement: a float takes an FPR and also a GPR slot."""
    L = e.cl
    g, fp, out = 3, 1, []
    for c in spec:
        if c == "f":
            out.append(canon_f(fb(e.f(fp))))
            fp += 1
            g += 1
        elif c == "L":
            out.append(L.lab(e.r(g) & MASK))
            g += 1
        elif c == "P":
            out.append(L.lab((e.r(g) - 0x3C) & MASK))
            g += 1
        elif c == "u":
            out.append(e.r(g) & MASK)
            g += 1
        elif c == "i":
            out.append(s32(e.r(g)))
            g += 1
        elif c == "b":
            out.append(e.r(g) & 0xFF)
            g += 1
    return tuple(out)


def simple(name, spec, ret=None):
    def stub(e):
        e.cl.rec(name, regs(e, spec))
        if ret:
            e.set_r(3, e.cl.ret(ret) & MASK)
    return stub


def stub_read(e):
    L = e.cl
    # the import glue saves the caller's TOC at 20(r1); the caller reloads r2 from there
    e.write(e.r(1) + 20, struct.pack(">I", e.toc))
    ret, file, buf, length, tr = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
    L.rec("LbFile_Read", (L.lab(file & MASK), length & MASK))
    n = min(length & MASK, len(L.file) - L.fpos)
    if n:
        e.write(buf, L.file[L.fpos:L.fpos + n])
    L.fpos += n
    e.write(tr, struct.pack(">I", n))
    e.write(ret, struct.pack(">I", 0))


def stub_alloc(e):
    L = e.cl
    L.rec("AllocateObjectInstanceToTable", (e.u32(e.r(3)), e.u32(e.r(4)), e.u32(e.r(5)), e.r(6) & MASK,
                                             1 if (e.r(7) & MASK) == ADDR["table"] else 0, L.lab(e.r(8) & MASK), e.r(9) & MASK))
    e.set_r(3, L.ret("Alloc") & MASK)


def stub_place(e):
    L = e.cl
    L.rec("PlacePylon", regs(e, "uuibu")[:3] + (e.r(8) & 0xFF, e.r(9) & MASK))
    e.write(e.r(6), struct.pack(">I", L.ret("PlacePylonP") & MASK))
    e.write(e.r(7), struct.pack(">I", L.ret("PlacePylonB") & MASK))
    e.set_r(3, 1)


def stub_loadtr(e):
    e.cl.rec("LoadTrainsAndCars", regs(e, "LL"))
    e.set_r(3, 1)


def stub_setloop(e):
    e.cl.rec("SetPlaceLoop", ())
    e.set_r(3, 1)


STUBS.internal(LBREAD, stub_read)
STUBS.internal(UDEBUG, CL.recorder("UDebug", "s"))
STUBS.internal(INSTTR, simple("InstanceCoasterTrains", "LLi"))
STUBS.internal(PLACEENT, simple("PlaceEntrancePylons", "Luufi"))
STUBS.internal(ALLOC, stub_alloc)
STUBS.internal(SETLOOP, stub_setloop)
STUBS.internal(PLACEPYL, stub_place)
STUBS.internal(LOFT, simple("SetLoftPylon", "fuii"))
STUBS.internal(ROT, simple("SetRotatePylon", "fuii"))
STUBS.internal(WOB, simple("SetWobblePylon", "ffuii"))
STUBS.internal(RECALC, simple("RecalcTrackParams", "L"))
STUBS.internal(UPDATE, simple("UpdatePylonStackMeshes", "LLii"))
STUBS.internal(PLACEEND, simple("PlaceEndPylons", "Lf"))
STUBS.internal(EDITST, simple("SetCoasterEditState", "u"))
STUBS.internal(COASTST, simple("SetCoasterState", "Lu"))
STUBS.internal(GETBRACE, simple("GetTrackBrace", "Pu", ret="GetTrackBrace"))
STUBS.internal(LOADTR, stub_loadtr)
STUBS.internal(TRAINCAP, simple("SetTrainCapacity", "Luuu"))
STUBS.internal(CLEARBTN, simple("ClearCoasterButtons", ""))
STUBS.internal(FLASH, simple("FlashEndPylons", "Lu"))


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

    def wptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.PW))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    PW = 4
    base = 0

    def __init__(self, e, case):
        self.e = e
        self.off = MAC
        self.W = make_world(self, case)
        fixed = {a: l for l, a in self.W.labels.items()}
        self.L = e.cl = CL.PpcLog(fixed, self.W.queues)
        self.L.file = self.W.blob
        self.L.fpos = 0
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

    def call(self, file):
        r3, _, _ = T.call_ppc(self.e, FN, [file & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    PW = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = lib.hostoff
        lib.tpw_LC_alloc(8)   # the arena starts with its first allocation
        self.base = lib.tpw_LC_arenabase()
        self.W = make_world(self, case)
        fa = self.alloc(max(8, len(self.W.blob)))
        self.write(fa, self.W.blob)
        lib.tpw_LC_setfile(fa, len(self.W.blob))
        CL.host_begin(lib, dict(self.W.labels), self.W.queues)

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_LC_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_LC_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, file):
        return self.lib.tpw_LC_run(file) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_LC_addr.restype, lib.tpw_LC_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_LC_alloc.restype, lib.tpw_LC_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_LC_arenabase.restype, lib.tpw_LC_arenabase.argtypes = C.c_uint64, []
    lib.tpw_LC_setfile.restype, lib.tpw_LC_setfile.argtypes = None, [C.c_uint64, C.c_uint32]
    lib.tpw_LC_run.restype, lib.tpw_LC_run.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_LC_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 64)()
    n = lib.tpw_LC_layout(out)
    assert n == len(NAMES), n
    lib.hostoff = dict(zip(NAMES, list(out)[:n]))
    for k in ("i_size", "b_size", "y_size", "d_size", "t_size"):
        assert lib.hostoff[k] <= BIG, (k, lib.hostoff[k])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def make_world(s, case):
    """Builds the memory through Side; answers the labels, the queues and the objects to observe."""
    W = types.SimpleNamespace(labels={}, braces=[], insts=[], blob=case["blob"][:case["cut"]] if case["cut"] is not None else case["blob"])
    o = s.off
    PW = s.PW
    W.labels[90] = s.alloc(16)

    def blk():
        return s.alloc(BIG)

    # the ride sets: set -> mesh -> header -> node, a float at node + 0x44
    for ident in range(1, 8):
        node = blk()
        s.words(node + 0x44, [case["heights"][ident]])
        hdr = blk()
        s.words(hdr + o["hdr_f78"], [(node - s.base) & MASK])
        mesh = blk()
        s.wptr(mesh + o["mesh_hdr"], hdr)
        rs = blk()
        s.wptr(rs + o["rs_heads"], mesh)
        s.wptr(s.g("sets") + ident * PW, rs)

    def mk_brace(spec, label, data=False):
        b = blk()
        s.words(b + o["b_number"], [spec["number"]])
        s.words(b + o["b_flags"], [spec["flags"]])
        s.words(b + o["b_fb0"], [spec["fb0"]])
        s.words(b + o["b_fb4"], spec["fb4"])
        table = None
        if data:
            d = blk()
            table = blk()
            s.words(table, [spec["tflags"]])
            s.wptr(d + o["d_table"], table)
            s.wptr(b + o["b_data"], d)
        W.labels[label] = b
        W.braces.append((b, table, label))
        return b

    pickable = []
    prev_inst = 0
    insts = []
    for k, spec in enumerate(case["insts"]):
        inst = blk()
        typ = blk()
        W.labels[1 + k] = inst
        W.labels[11 + k] = typ
        s.words(typ + o["t_f2f4"], [spec["f2f4"]])
        s.wptr(inst + o["i_type"], typ)
        for key, nm in (("flags", "i_flags"), ("state", "i_state"), ("ent", "i_entrance"), ("objid", "i_objid"), ("f2c", "i_f2c"),
                        ("block", "i_block"), ("ff0", "i_ff0"), ("trains", "i_trains"), ("f140", "i_f140")):
            s.words(inst + o[nm], [spec[key]])
        chain = [mk_brace(bs, 100 + 10 * k + i) for i, bs in enumerate(spec["chain"])]
        for i in range(len(chain) - 1):
            s.wptr(chain[i] + o["b_prev"], chain[i + 1])
        s.wptr(inst + o["i_last"], chain[0])
        if spec["head"] >= 0:
            s.wptr(inst + o["i_braces"], chain[spec["head"]])
        pickable += chain
        nodes = []
        for j, ps in enumerate(spec["pyl"]):
            node = blk()
            W.labels[200 + 10 * k + j] = node
            ctl = blk()
            W.labels[300 + 10 * k + j] = ctl
            s.words(node + o["y_flags"], [ps["flags"]])
            s.wptr(node + o["y_control"], ctl)
            s.words(node + o["y_count"], [len(ps["braces"])])
            for i, bs in enumerate(ps["braces"]):
                b = mk_brace(bs, 400 + 100 * k + 10 * j + i, True)
                s.wptr(node + o["y_braces"] + i * PW, b)
            nodes.append(node)
        for j in range(len(nodes) - 1):
            s.wptr(nodes[j] + o["y_next"], nodes[j + 1])
        if nodes:
            s.wptr(inst + o["i_pylons"], nodes[0])
        insts.append(inst)
        W.insts.append(inst)
    for k in range(len(insts) - 1):
        s.wptr(insts[k] + o["i_next"], insts[k + 1])
    s.wptr(s.g("list"), insts[0])
    pool = [mk_brace(bs, 150 + i) for i, bs in enumerate(case["pool"])]
    pickable += pool

    g = case["globals"]
    s.words(s.g("count"), [g["count"]])
    s.words(s.g("first"), [g["first"]])
    s.words(s.g("lowest"), [g["lowest"]])
    s.words(s.g("skip"), [g["skip"]])
    s.words(s.g("rec"), [g["rec"]])
    s.wptr(s.g("edit"), 0)
    s.wptr(s.g("cur"), 0)
    W.queues = {
        "PlacePylonB": [pickable[i % len(pickable)] for i in case["pick"]],
        "PlacePylonP": list(case["pp"]),
        "Alloc": list(case["alloc"]),
        "GetTrackBrace": list(case["gtb"]),
    }
    W.addr2lab = {a: l for l, a in W.labels.items()}
    return W


def lab_of(W, v):
    return 0 if v == 0 else W.addr2lab.get(v, ("raw", v))


def observe(s, case, answer):
    W, o = s.W, s.off
    out = {"answer": answer, "log": s.log()}
    for nm in ("skip", "rec"):
        out[nm] = s.rd_word(s.g(nm))
    for i, nm in enumerate(("count", "first", "lowest")):
        v = s.rd_word(s.g(nm))
        # A short read is never swapped: its bytes are the same on both sides, only the PowerPC word reads them the other way round.
        # (An unread global keeps four equal bytes, which no swap changes.)
        if len(W.blob) < 8 + 4 * i and s.fmt == ">I":
            v = int.from_bytes(v.to_bytes(4, "big"), "little")
        out[nm] = v
    out["edit"] = lab_of(W, s.rd_ptr(s.g("edit")))
    cur = s.rd_ptr(s.g("cur"))
    out["cur"] = next((("inst", i) for i, a in enumerate(W.insts) if cur == a + o["i_block"]), 0 if cur == 0 else ("raw", 1))
    for k, inst in enumerate(W.insts):
        d = {}
        for nm in ("i_flags", "i_state", "i_entrance", "i_objid", "i_block", "i_ff0", "i_trains", "i_f140"):
            d[nm] = s.rd_word(inst + o[nm])
        d["i_f2c"] = canon_f(s.rd_word(inst + o["i_f2c"]))
        d["i_prev60"] = lab_of(W, s.rd_ptr(inst + o["i_prev60"]))
        d["i_b64"] = lab_of(W, s.rd_ptr(inst + o["i_b64"]))
        out["inst%d" % k] = d
    lg = out["log"]
    # A short read of the first brace's count (right after the allocation) is not swapped: same bytes, read the other way round on the PowerPC.
    short_fb0 = (answer == 0 and len(lg) >= 3 and "AllocateObjectInstanceToTable" in str(lg[-3]) and "LbFile_Read" in str(lg[-2])
                 and "UDebug" in str(lg[-1]))
    # (the records of a case match different instances, so that brace still holds its initial 0 before the short read)
    short_label = 100 + 10 * (lg[-4][1][0] - 1) + 2 if short_fb0 and "PlaceEntrancePylons" in str(lg[-4]) else None
    for b, table, label in W.braces:
        d = {nm: s.rd_word(b + o[nm]) for nm in ("b_number", "b_flags", "b_fb0")}
        if label == short_label and s.fmt == ">I":
            d["b_fb0"] = int.from_bytes(d["b_fb0"].to_bytes(4, "big"), "little")
        d["fb4"] = s.rd_words(b + o["b_fb4"], 11)
        d["prev"] = lab_of(W, s.rd_ptr(b + o["b_prev"]))
        v = s.rd_ptr(b + o["b_f90"])
        d["f90"] = 0 if v == 0 else v - b - o["b_f70"]
        v = s.rd_ptr(b + o["b_f74"])
        d["f74"] = 0 if v == 0 else (v - table if table else ("raw", v))
        out["brace%d" % label] = d
    return out


def run(s, case):
    try:
        return observe(s, case, s.call(s.W.labels[90]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def r32(r):
    return r.choice([0, 0xFFFFFFFF, r.getrandbits(32), r.getrandbits(32), r.getrandbits(8)])


def rep(r):
    """A word of four equal bytes: a short read leaves the old bytes behind, and the two sides keep the word's bytes in opposite orders."""
    return r.getrandbits(8) * 0x01010101


def mk_brace(r, data=False):
    return dict(number=r.choice([2, 2, 0, 1, 3, r.getrandbits(32)]), flags=r32(r), fb0=r.randint(0, 10), tflags=r.choice([8, 0, r32(r)]),
                fb4=[r32(r) for _ in range(11)])


def mk_inst(r, ent):
    nchain = r.randint(3, 5)
    pyl = []
    for _ in range(r.randint(0, 3)):
        count = r.randint(0, 5)
        pyl.append(dict(flags=r.choice([0, 1, r.getrandbits(8)]), braces=[mk_brace(r, True) for _ in range(count)]))
    return dict(ent=ent, flags=r32(r), state=r32(r), objid=r32(r), f2c=rf(r), block=r.choice([0, 1, 2, 3, r32(r)]), ff0=r32(r), trains=r32(r),
                f140=r32(r), f2f4=r32(r), chain=[dict(mk_brace(r), fb0=0) for _ in range(nchain)], head=r.choice([-1] + list(range(nchain))), pyl=pyl)


def mk_pylon(r, flags=None, cnt=None):
    return dict(flags=r.getrandbits(12) if flags is None else flags, extra=r32(r), x=r.getrandbits(16), z=r.getrandbits(16),
                level=r.getrandbits(8), f=[rf(r) for _ in range(5)], cnt=r.randint(0, 10) if cnt is None else cnt)


def mk_rec(r, ents, flags=None, pylons=None):
    pyl = [mk_pylon(r) for _ in range(r.randint(0, 3))] if pylons is None else pylons
    return dict(flags=r.getrandbits(9) if flags is None else flags, state=r32(r), ent=r.choice(ents), extra=r.getrandbits(16),
                x=r.getrandbits(16), z=r.getrandbits(16), f=rf(r), trains=r.getrandbits(16), word=r.getrandbits(16), last=r32(r),
                h8=r.getrandbits(16), b0=r.randint(0, 10), pyl=pyl)


def make_blob(r, recs, count=None, objs=None):
    """The file's bytes (little-endian) and the end of each read."""
    parts = []
    n = len(recs) if count is None else count
    objs = objs or (r32(r), r32(r), r32(r))
    parts.append(struct.pack("<I", n))
    for w in objs:
        parts.append(struct.pack("<I", w))
    bounds = [4, 8, 12, 16]
    pos = 16

    def add(b, ends):
        nonlocal pos
        parts.append(b)
        for e in ends:
            bounds.append(pos + e)
        pos += len(b)

    for rec in recs:
        add(struct.pack("<IIHHHHHIHHHI", rec["flags"], rec["state"], rec["h8"], rec["ent"], rec["extra"], rec["x"], rec["z"],
                        rec["f"], len(rec["pyl"]), rec["trains"], rec["word"], rec["last"]), [0x20])
        halves = [r.getrandbits(16) for _ in range(rec["b0"])]
        add(struct.pack("<I", rec["b0"]) + b"".join(struct.pack("<H", h) for h in halves), [4] + [4 + 2 * (i + 1) for i in range(rec["b0"])])
        for p in rec["pyl"]:
            add(struct.pack("<IIHHBIIIIIB", p["flags"], p["extra"], p["x"], p["z"], p["level"], *p["f"], p["cnt"]), [0x22])
            hs = b"".join(struct.pack("<H", r.getrandbits(16)) for _ in range(p["cnt"]))
            add(hs, [2 * (i + 1) for i in range(p["cnt"])])
    return b"".join(parts), sorted(set(bounds))


def base(r, recs=None, ninst=None, cut=None, count=None, cutfn=None):
    ninst = r.randint(1, 3) if ninst is None else ninst
    nrec = r.randint(0, 3) if recs is None else len(recs)
    ninst = max(ninst, nrec)
    ents = r.sample(range(1, 8), ninst)   # distinct: each record meets a fresh instance
    recs = [mk_rec(r, [ents[i]]) for i in range(nrec)] if recs is None else [mk_rec(r, [ents[i]], **kw) for i, kw in enumerate(recs)]
    blob, bounds = make_blob(r, recs, count)
    if cutfn is not None:
        cut = cutfn(bounds)
    elif cut == "rand":
        cut = r.choice([None, None, r.randrange(len(blob) + 1), r.choice(bounds), r.choice(bounds) - 1])
    # A NULL pylon skips that pylon's brace words, so a later coaster record would be read out of step (an unmatched entrance reads address 0).
    pp = [r.choice([1, 1, 1, 0]) for _ in range(40)]
    # (and the skipped brace words would be read as the next pylon, with a brace count above the 11 words the host brace holds)
    if not (len(recs) == 1 and (count is None or count == 1) and len(recs[0]["pyl"]) <= 1):
        pp = [1] * 40
    return {"insts": [mk_inst(r, e) for e in ents], "pool": [mk_brace(r) for _ in range(3)], "heights": {i: rf(r) for i in range(1, 8)},
            "globals": dict(count=rep(r), first=rep(r), lowest=rep(r), skip=r.choice([0, 1, r32(r)]), rec=r.choice([0, 0, 1, r32(r)])),
            "pick": [r.randrange(64) for _ in range(40)], "pp": pp,
            "alloc": [r32(r) for _ in range(6)], "gtb": [r.getrandbits(32) for _ in range(40)], "blob": blob, "cut": cut, "bounds": bounds}


def edges():
    r = random.Random(5372)
    yield base(r, recs=[], ninst=1)
    # the header's reads and every read of a rich file cut short (before, at and after each end)
    probe = random.Random(77)
    rich = [dict(flags=0x1FF, pylons=[mk_pylon(probe, 0xFFF, 2), mk_pylon(probe, 0, 0)]), dict(flags=0, pylons=[mk_pylon(probe, 0x10, 1)])]
    n = len(base(random.Random(78), recs=rich, ninst=2)["bounds"])
    for i in range(n):
        for delta in (0, -1):
            def cutfn(bounds, i=i, delta=delta):
                return max(0, bounds[i] + delta)
            yield base(random.Random(78), recs=rich, ninst=2, cutfn=cutfn)
    yield base(r, recs=rich, ninst=2)
    # a count that promises more records than the file holds
    yield base(r, recs=[dict()], ninst=2, count=3)
    # the coaster flags, one at a time
    for bit in range(10):
        yield base(r, recs=[dict(flags=1 << bit, pylons=[mk_pylon(r, 0, 1)])], ninst=2)
    yield base(r, recs=[dict(flags=0, pylons=[])], ninst=1)
    yield base(r, recs=[dict(flags=0x1FF, pylons=[])], ninst=1)
    # the pylon flags, one at a time and all
    for bit in range(12):
        yield base(r, recs=[dict(flags=r.choice([0, 2, 3]), pylons=[mk_pylon(r, 1 << bit, r.choice([0, 1, 10])), mk_pylon(r, 0, 3)])], ninst=2)
    yield base(r, recs=[dict(flags=3, pylons=[mk_pylon(r, 0xFFF, 10), mk_pylon(r, 0xFFF, 0)])], ninst=1)
    yield base(r, recs=[dict(flags=0, pylons=[mk_pylon(r, 0, 10), mk_pylon(r, 0, 0)])], ninst=3)


def rnd(r):
    return base(r, cut="rand")


TESTS = [
    FnTest("LoadCoasters", ppc, host, edge=edges, rnd=rnd),
]
