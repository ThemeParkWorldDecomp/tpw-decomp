"""Unit v5365, DestroyTrackBrace and PlaceEntrancePylons, against libtpw_unit_DestroyTrackBrace.dylib (tests/unit_DestroyTrackBrace.cpp), main app
("SimTheme Park"), src/TPCoaster_instances_DestroyTrackBrace.cpp:
  DestroyTrackBrace 0x1004094c, PlaceEntrancePylons 0x10040acc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian, 4-byte
pointers) for the original, and in the host library's memory (little-endian, 8-byte pointers, the host's offsets from the layout export) for the
translation. The callees outside the unit (DestroyRideInstance, MReleaseFreeF, Get/SetCurSelectedTrackBrace, FindDataNodeIndexByID, the float
to cell function 0x10237cd8, NewPylon, NewTrackBrace, SetupBraceAnimFrameData, SetPotentialPylonArea, FlashEndPylons, FlagGenerateTrack) are
fakes on both sides that record every argument (pointers as labels, the point NewPylon is given by its three floats) and answer from the case.

The world of a case: for DestroyTrackBrace a brace with its ride word, four blocks, pylon data flags and an optional pylon control; for
PlaceEntrancePylons an instance, its type (flags), the editor's instance, a ride anim set with a mesh, header, embedded table of four records
(blocks or NULL) and the hoarding node. What is compared: the log, and the brace words after the call.

No fused operations. Not tested: a node index outside the four records (the original reads past the table).
"""
import ctypes as C
import itertools
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DestroyTrackBrace"
MASK = 0xFFFFFFFF

FN_DTB, FN_PEP = 0x1004094C, 0x10040ACC
SIZES = {FN_DTB: 308, FN_PEP: 764}
OWN = set(SIZES)

C_DESTROY_RIDE, C_MREL, C_GETCUR, C_SETCUR = 0x1006A3EC, 0x10064C54, 0x10052290, 0x100522CC
C_FIND, C_NEWPYLON, C_NEWBRACE, C_SETUP = 0x101FFDC8, 0x1003D610, 0x10040304, 0x1003BB44
C_POTENTIAL, C_FLASH, C_FLAGGEN, C_CELL = 0x10041A58, 0x10052158, 0x10040F08, 0x10237CD8

ADDR = dict(edit=0x102B68B0, sets=0x102C79A4)

NAMES = ["b_ride", "b_7c", "b_80", "b_88", "b_8c", "b_data", "d_flags", "i_type", "i_entr", "i_f40", "t_pyl", "s_heads", "m_header",
         "m_f28", "h_f78", "e_records", "r_size", "r_block", "p_x", "p_y", "p_z", "z_brace", "z_inst", "z_type", "z_pylon"]
MAC = dict(zip(NAMES, [0x70, 0x7C, 0x80, 0x88, 0x8C, 0x100, 4, 4, 0x10, 0x40, 0xA8, 8, 4, 0x28, 0x78, 4, 0x14, 4, 0x1C, 0x20, 0x24,
                       0, 0, 0, 0]))
HL = {}
BLK = 0x1000

W = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_destroy_ride(e):
    e.cl.rec("DestroyRideInstance", (e.r(3) & MASK,))


def st_mrel(e):
    e.cl.rec("MReleaseFreeF", (e.cl.lab(e.r(3)),))
    e.set_r(3, 0)


def st_getcur(e):
    e.cl.rec("GetCur", ())
    e.set_r(3, W["self"] if e.cl.ret("Cur") else 0x5A5A)


def st_setcur(e):
    e.cl.rec("SetCur", (e.cl.lab(e.r(3)),))


def st_find(e):
    L = e.cl
    L.rec("Find", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))
    e.set_r(3, L.ret("Find") & MASK)


def st_cell(e):
    L = e.cl
    bits = T.f64_bits(e.f(1)) & 0xFFFFFFFFFFFFFFFF
    if bits >= 1 << 63:
        bits -= 1 << 64
    L.rec("Cell", (bits,))
    e.set_r(3, L.ret("Cell") & MASK)


def st_newpylon(e):
    L = e.cl
    point = e.r(8)
    vals = (0, 0, 0)
    if point:
        vals = tuple(canon_f(w) for w in struct.unpack(">3I", e.read(point, 12)))
    L.rec("NewPylon", (e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK, s32(e.r(6)), L.lab(e.r(7)), L.lab(point)) + vals)
    e.set_r(3, W["pylon"])


def st_newbrace(e):
    L = e.cl
    extra = e.u32(e.r(1) + 0x38)
    L.rec("NewTrackBrace", (L.lab(e.r(3)), canon_f(fb(e.f(1))), canon_f(fb(e.f(2))), canon_f(fb(e.f(3))), e.r(7) & MASK, L.lab(e.r(8)),
                            e.r(9) & MASK, e.r(10) & MASK, extra))
    e.set_r(3, W["bret"])


def st_setup(e):
    e.cl.rec("Setup", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def st_potential(e):
    e.cl.rec("SetPotential", ())


def st_flash(e):
    e.cl.rec("Flash", (e.cl.lab(e.r(3)), e.r(4) & MASK))


def st_flaggen(e):
    e.cl.rec("FlagGen", (e.cl.lab(e.r(3)), s32(e.r(4))))


for _a, _f in ((C_DESTROY_RIDE, st_destroy_ride), (C_MREL, st_mrel), (C_GETCUR, st_getcur), (C_SETCUR, st_setcur), (C_FIND, st_find),
               (C_CELL, st_cell), (C_NEWPYLON, st_newpylon), (C_NEWBRACE, st_newbrace), (C_SETUP, st_setup), (C_POTENTIAL, st_potential),
               (C_FLASH, st_flash), (C_FLAGGEN, st_flaggen)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def w32(self, a, v):
        self.write(a, struct.pack(self.fmt, v & MASK))

    def r32(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def wptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]

    def g(self, name):
        return ADDR[name]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.lay = MAC

    def begin(self, labels, pylon, bret, selfb):
        W.update(pylon=pylon, bret=bret, self=selfb)
        self.L = self.e.cl = CL.PpcLog(dict(labels), self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if hasattr(self, "saved"):
            CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def off(self, a):
        return a

    def call_dtb(self, brace, ctl):
        T.call_ppc(self.e, FN_DTB, [brace, ctl])

    def call_pep(self, inst, x, z, h, flag):
        T.call_ppc(self.e, FN_PEP, [inst, x, z, T.F(h), flag & MASK])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)
        self.lay = HL

    def begin(self, labels, pylon, bret, selfb):
        self.lib.tpw_dt_arm(pylon, bret, selfb)
        CL.host_begin(self.lib, {n: a for a, n in labels.items()}, self.case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_dt_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def off(self, a):
        return self.lib.tpw_dt_off(a)

    def g(self, name):
        return self.lib.tpw_dt_addr(name.encode())

    def call_dtb(self, brace, ctl):
        self.lib.tpw_dt_destroy(brace, ctl)

    def call_pep(self, inst, x, z, h, flag):
        self.lib.tpw_dt_place(inst, x, z, h, flag)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_dt_addr.restype, lib.tpw_dt_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_dt_alloc.restype, lib.tpw_dt_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_dt_off.restype, lib.tpw_dt_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_dt_arm.argtypes = [C.c_uint64] * 3
    lib.tpw_dt_destroy.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_dt_place.argtypes = [C.c_uint64, C.c_uint32, C.c_uint32, C.c_float, C.c_int32]
    lib.tpw_dt_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 64)()
    n = lib.tpw_dt_layout(out)
    assert n == len(NAMES), n
    HL.update(zip(NAMES, list(out)[:n]))
    assert all(HL[k] < BLK for k in ("z_brace", "z_inst", "z_type", "z_pylon")), HL
    lib.bound = True


def lbl(labels, v):
    return 0 if v == 0 else labels.get(v, -1)


# ---------------------------------------------------------------------------------------------------------------- DestroyTrackBrace


def run_dtb(s, case):
    try:
        L = s.lay
        brace, data, bret, pylon = s.alloc(BLK), s.alloc(BLK), s.alloc(BLK), s.alloc(BLK)
        ctl = s.alloc(BLK) if case["ctl"] is not None else 0
        blocks = [s.alloc(16) if present else 0 for present in case["blocks"]]
        s.w32(brace + L["b_ride"], case["ride"])
        for name, b in zip(("b_7c", "b_80", "b_88", "b_8c"), blocks):
            s.wptr(brace + L[name], b)
        s.wptr(brace + L["b_data"], data)
        s.w32(data + L["d_flags"], case["dflag"])
        if ctl:
            s.w32(ctl, case["ctl"])
        labels = {brace: 1, data: 7}
        if ctl:
            labels[ctl] = 2
        for i, b in enumerate(blocks):
            if b:
                labels[b] = 3 + i
        s.begin(labels, pylon, bret, brace)
        s.call_dtb(brace, ctl)
        return {"log": s.log(), "ride": s.r32(brace + L["b_ride"]),
                "ptrs": [lbl(labels, s.rptr(brace + L[n])) for n in ("b_7c", "b_80", "b_88", "b_8c")],
                "dflag": s.r32(data + L["d_flags"]), "ctl": s.r32(ctl) if ctl else 0}
    finally:
        s.done()


def dtb_case(r, ride, ctl, dflag, cur, blocks):
    return {"queues": {"Cur": [cur]}, "ride": ride, "ctl": ctl, "dflag": dflag, "blocks": blocks}


def dtb_edges():
    r = random.Random(11)
    for ride, ctl, dflag, cur in itertools.product((0, 5), (None, 8, 0, 0x108, 0xF7), (0, 1, 0xE, 0xF), (0, 1)):
        yield dtb_case(r, ride, ctl, dflag, cur, [True, True, True, True])
    for mask in range(16):
        yield dtb_case(r, 3, 8, 0, 1, [bool(mask & 1), bool(mask & 2), bool(mask & 4), bool(mask & 8)])
        yield dtb_case(r, 0, None, 1, 0, [bool(mask & 1), bool(mask & 2), bool(mask & 4), bool(mask & 8)])


def dtb_random(r):
    return dtb_case(r, r.choice([0, r.randrange(1, 1 << 32)]), r.choice([None, r.randrange(0, 1 << 32), 8 | r.randrange(0, 1 << 32)]),
                    r.randrange(0, 1 << 32), r.choice([0, 1]), [r.random() < 0.8 for _ in range(4)])


# ---------------------------------------------------------------------------------------------------------------- PlaceEntrancePylons


def run_pep(s, case):
    try:
        L = s.lay
        inst, typ, edit, etyp, sset = (s.alloc(BLK) for _ in range(5))
        mesh, header, eset, node, pylon, bret = (s.alloc(BLK) for _ in range(6))
        recs = s.alloc(BLK)
        blocks = [s.alloc(0x40) if rec is not None else 0 for rec in case["recs"]]
        s.wptr(inst + L["i_type"], typ)
        s.w32(inst + L["i_entr"], case["eid"])
        s.w32(inst + L["i_f40"], case["f40"])
        s.w32(typ, case["tflag"])
        s.wptr(edit + L["i_type"], etyp)
        s.wptr(s.g("edit"), edit)
        s.wptr(s.g("sets") + s.psz * case["eid"], sset)
        s.wptr(sset + L["s_heads"], mesh)
        s.wptr(mesh + L["m_header"], header)
        s.wptr(mesh + L["m_f28"], eset)
        s.w32(header + L["h_f78"], s.off(node))
        s.w32(node + 0x44, case["hy"])
        s.wptr(eset + L["e_records"], recs)
        for i, (rec, b) in enumerate(zip(case["recs"], blocks)):
            s.wptr(recs + i * L["r_size"] + L["r_block"], b)
            if rec is not None:
                for k in range(3):
                    s.w32(b + 0x30 + 4 * k, rec[k])
        for k, name in enumerate(("p_x", "p_y", "p_z")):
            s.w32(pylon + L[name], case["pyl"][k])
        labels = {inst: 1, typ + L["t_pyl"]: 2, etyp: 3, sset: 4, pylon: 5, bret: 6, edit: 7}
        s.begin(labels, pylon, bret, 0)
        s.call_pep(inst, case["x"], case["z"], struct.unpack(">f", struct.pack(">I", case["h"]))[0], case["flag"])
        return {"log": s.log()}
    finally:
        s.done()


def rfl(r):
    k = r.random()
    if k < 0.1:
        return r.choice([0, 0x80000000, fb(10.0), fb(-10.0)])
    if k < 0.7:
        return fb(r.uniform(-500, 500))
    if k < 0.9:
        return fb(r.uniform(-1, 1) * 10 ** r.uniform(-6, 6))
    return fb(r.uniform(-1, 1) * 1e30)


def pep_case(r, tflag=None, f40=None, flag=None, finds=None, eid=None):
    finds = finds if finds is not None else [r.choice([0, 1, 2, 3, 0xFFFFFFFF]) for _ in range(3)]
    return {"queues": {"Find": finds, "Cell": [r.randrange(-5, 5000) for _ in range(6)]},
            "tflag": r.choice([0x20, 0x20, 0, 0x21, 0xFFFFFFDF, r.randrange(0, 1 << 32)]) if tflag is None else tflag,
            "f40": r.choice([0, 0, 0, 1, 0x40]) if f40 is None else f40,
            "flag": r.choice([0, 1, -1, 7]) if flag is None else flag,
            "eid": r.choice([1, 2, 3]) if eid is None else eid,
            "x": r.randrange(0, 1 << 32), "z": r.randrange(0, 1 << 32), "h": rfl(r), "hy": rfl(r),
            "pyl": [rfl(r), rfl(r), rfl(r)],
            "recs": [None if r.random() < 0.25 else [rfl(r), rfl(r), rfl(r)] for _ in range(4)]}


def pep_edges():
    r = random.Random(12)
    for tflag, f40, flag, eid in itertools.product((0x20, 0), (0, 5), (0, 1, -3), (1, 3)):
        for finds in ([0, 1, 2], [3, 0xFFFFFFFF, 1], [0xFFFFFFFF] * 3, [2, 2, 0xFFFFFFFF]):
            yield pep_case(r, tflag, f40, flag, finds, eid)


def pep_random(r):
    return pep_case(r)


def ppc(run):
    return lambda e, case: run(Ppc(e, case), case)


def host(run):
    return lambda lib, case: run(Host(lib, case), case)


TESTS = [
    FnTest("DestroyTrackBrace", ppc(run_dtb), host(run_dtb), edge=dtb_edges, rnd=dtb_random),
    FnTest("PlaceEntrancePylons", ppc(run_pep), host(run_pep), edge=pep_edges, rnd=pep_random),
]
