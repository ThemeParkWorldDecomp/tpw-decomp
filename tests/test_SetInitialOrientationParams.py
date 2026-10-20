"""Unit v51074, 23 small coaster, heightfield and mesh helpers, against libtpw_unit_SetInitialOrientationParams.dylib
(tests/unit_SetInitialOrientationParams.cpp), main app ("SimTheme Park"),
src/DestroyCoasterPylons_SetupBraceBezierCtrlParams_SetInitialOrientationParams.cpp:
  SetInitialOrientationParams 0x1005092c, SetInitialCoasterInterpParams 0x100509bc, RecalcTrackParams 0x10050f88,
  GetWrappedTrackPosition 0x1005158c, GetCurSelectedTrackBrace 0x10052290, SetCurSelectedTrackBrace 0x100522cc,
  IncCoasterConst2 0x100526d4, IsTrackClosed 0x10052780, GetCoasterInstanceFromCObject 0x100527e0, GetTrackBrace 0x1004de40,
  ClearCoasterButtons 0x10053d88, OutputCoasterValues 0x100545d4, IsCoasterLoftMode 0x10058b30, SetPlaceNormalPylon 0x10058b68,
  SetPlaceSpecialPylon 0x10058bb8, LayoutDeleteLastTrackBrace 0x10058f4c, LayoutRecordCoasterSpeed 0x100590a0,
  GetHFieldMaxVertexIndices 0x1005c250, DeActivateHoardings 0x100617cc, RestoreLevelHeightfield 0x10063cc0,
  GetLightsourceHandle 0x100647ec, SetMeshPosition 0x10065588, ScaleMesh 0x100655d8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers) for the translation; the structures that hold pointers
have different offsets on the host, which the library exports (tpw_sip_layout) and the shared code looks up by name. The callees outside
the unit (BezierPosition, CTRACE, FlagGenerateTrack, FindCoasterInstanceFromEntranceID, DisableCurrentPylonEditState, SetPylonType,
DeleteLastTrackBrace, CActionRecorder::RecordAction, RestoreHFieldDataFromCopy, ScaleLMatrixNoTrans) are fakes on both sides that record
their arguments (objects as labels, floats as bits) and answer from the case.

What is compared: the answer (pointers as the identity of the object they name), the globals and every block the function may write,
and the event log. No fused operations. Not tested: a NULL the original reads unconditionally (GetTrackBrace past the chain,
GetWrappedTrackPosition with a NULL brace, the heightfield header NULL, GetLightsourceHandle with a NULL record array and an index above 0),
the words of a structure that the function does not touch but whose host layout differs from the Mac's (compared by field instead).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SetInitialOrientationParams"
MASK = 0xFFFFFFFF

FUNCS = {
    "SetInitialOrientationParams": 0x1005092C, "SetInitialCoasterInterpParams": 0x100509BC, "RecalcTrackParams": 0x10050F88,
    "GetWrappedTrackPosition": 0x1005158C, "GetCurSelectedTrackBrace": 0x10052290, "SetCurSelectedTrackBrace": 0x100522CC,
    "IncCoasterConst2": 0x100526D4, "IsTrackClosed": 0x10052780, "GetCoasterInstanceFromCObject": 0x100527E0,
    "GetTrackBrace": 0x1004DE40, "ClearCoasterButtons": 0x10053D88, "OutputCoasterValues": 0x100545D4,
    "IsCoasterLoftMode": 0x10058B30, "SetPlaceNormalPylon": 0x10058B68, "SetPlaceSpecialPylon": 0x10058BB8,
    "LayoutDeleteLastTrackBrace": 0x10058F4C, "LayoutRecordCoasterSpeed": 0x100590A0, "GetHFieldMaxVertexIndices": 0x1005C250,
    "DeActivateHoardings": 0x100617CC, "RestoreLevelHeightfield": 0x10063CC0, "GetLightsourceHandle": 0x100647EC,
    "SetMeshPosition": 0x10065588, "ScaleMesh": 0x100655D8,
}
OWN = set(FUNCS.values())

ADDR = dict(gCoasterEditInstance=0x102B68B0, gCoasterWord7310=0x102B7310, gCoasterEditEnabled=0x102C16D4,
            gCoasterEditDown=0x102C16D8, gCoasterDragBrace=0x102C178C, gCoasterInputFlags=0x102C1794,
            gHeightfieldHeader=0x102C6C7C, gHeightfieldCopy=0x102C6C80, gRideAnimSets=0x102C79A4, gActionRecorder=0x1037EC44)

# name: (Mac offset, index into the host's layout export or None when the offset is the same)
OFF = dict(brace_f90=(0x90, 0), brace_prev=(0x64, 1), ctl_brace=(4, 2), spl_a=(4, 3), bez=(0x58, 4), par_last=(0x18, 5),
           set_flags=(4, 6), set_hoard=(0xB0, 7), set_speed=(0xBC, 8), set_heads=(8, 9), set_size=(0x100, 10),
           mesh_f28=(0x28, 11), mesh_hdr=(4, 12), emb_rec=(4, 13), rec_size=(0x14, 14), rec_f08=(8, 15), ptr=(4, 16),
           inst_type=(4, 17), inst_3c=(0x3C, 18), inst_oid=(0x14, 19), hdr_hf=(0x6C, 20), hdr_f78=(0x78, 21), hf_18=(0x18, 22),
           node_mat=(0x10, 23))
LAY = []


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


def canon_d(hi, lo):
    if (hi & 0x7FF00000) == 0x7FF00000 and ((hi & 0xFFFFF) or lo):
        return 0x7FF80000, 0
    return hi, lo


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_bezier(e):
    L = e.cl
    L.rec("BezierPosition", (L.lab(e.r(3)), L.lab(e.r(4)), canon_f(fb(e.f(1))), L.lab(e.r(6))))


def stub_ctrace(e):
    L = e.cl
    fmt = e.cstr(e.r(4))
    total = 0
    for ch in fmt.encode("latin-1") if isinstance(fmt, str) else bytes(fmt):
        total = (total * 31 + ch) & MASK
    h1, l1 = canon_d(e.r(5) & MASK, e.r(6) & MASK)
    h2, l2 = canon_d(e.r(7) & MASK, e.r(8) & MASK)
    L.rec("CTRACE", (e.r(3) & MASK, total, h1, l1, h2, l2))


def stub_flag(e):
    e.cl.rec("FlagGenerateTrack", (e.cl.lab(e.r(3)), s32(e.r(4))))


def stub_find(e):
    L = e.cl
    L.rec("FindCoasterInstanceFromEntranceID", (e.r(3) & MASK,))
    e.set_r(3, L.ret("FindCoasterInstanceFromEntranceID") & MASK)


def stub_disable(e):
    e.cl.rec("DisableCurrentPylonEditState", ())


def stub_settype(e):
    e.cl.rec("SetPylonType", (e.r(3) & MASK,))


def stub_deletelast(e):
    e.cl.rec("DeleteLastTrackBrace", ())


def stub_record(e):
    L = e.cl
    L.rec("RecordAction", (L.lab(e.r(3)), s32(e.r(4)), e.r(5) & 0xFFFF, e.r(6) & 0xFFFF))


def stub_restore(e):
    L = e.cl
    L.rec("RestoreHFieldDataFromCopy", (L.lab(e.r(3)), L.lab(e.r(4))))


def stub_scale(e):
    L = e.cl
    L.rec("ScaleLMatrixNoTrans", (L.lab(e.r(3)), canon_f(fb(e.f(1))), canon_f(fb(e.f(2))), canon_f(fb(e.f(3)))))


STUBS.internal(0x1004DA98, stub_bezier)
STUBS.internal(0x1006FCBC, stub_ctrace)
STUBS.internal(0x10040F08, stub_flag)
STUBS.internal(0x10043380, stub_find)
STUBS.internal(0x10053B58, stub_disable)
STUBS.internal(0x100562D4, stub_settype)
STUBS.internal(0x10056A80, stub_deletelast)
STUBS.internal(0x1014430C, stub_record)
STUBS.internal(0x1005C050, stub_restore)
STUBS.internal(0x100C1D0C, stub_scale)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides

# argument kinds: p pointer (word), u unsigned, i signed, f float bits; results: p/u words, i int, None
SIG = {
    "SetInitialOrientationParams": ("", "ppi"), "SetInitialCoasterInterpParams": ("", "p"), "RecalcTrackParams": ("", "p"),
    "GetWrappedTrackPosition": ("", "ppfp"), "GetCurSelectedTrackBrace": ("p", ""), "SetCurSelectedTrackBrace": ("", "p"),
    "IncCoasterConst2": ("i", ""), "IsTrackClosed": ("i", "p"), "GetCoasterInstanceFromCObject": ("p", "p"),
    "GetTrackBrace": ("p", "pu"), "ClearCoasterButtons": ("", ""), "OutputCoasterValues": ("i", ""), "IsCoasterLoftMode": ("i", ""),
    "SetPlaceNormalPylon": ("", ""), "SetPlaceSpecialPylon": ("", ""), "LayoutDeleteLastTrackBrace": ("", ""),
    "LayoutRecordCoasterSpeed": ("", "pu"), "GetHFieldMaxVertexIndices": ("", "pp"), "DeActivateHoardings": ("", "p"),
    "RestoreLevelHeightfield": ("", ""), "GetLightsourceHandle": ("p", "uu"), "SetMeshPosition": ("", "pfff"), "ScaleMesh": ("", "pp"),
}


class Side:
    started = False

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def junk(self, a, n, case, skip=0):
        self.words(a, [case["junk"][(i + skip) % len(case["junk"])] for i in range(n)])

    def tag(self, a, ident):
        self.ids[a] = ident

    def norm(self, v):
        return 0 if v == 0 else self.ids.get(v, -1)

    def p32(self, a, v):
        """A Ptr32 word (an offset from the arena base on the host)."""
        self.words(a, [v - self.base() if v else 0])


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.ids = {}

    def base(self):
        return 0

    def o(self, name):
        return OFF[name][0]

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptrw(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_word(a)

    def start(self, labels):
        self.L = self.e.cl = CL.PpcLog(dict(labels), self.case["queues"])
        self.saved = STUBS.activate(self.e)
        self.started = True

    def log(self):
        return self.L.log

    def done(self):
        if self.started:
            CL.Stubs.deactivate(self.e, self.saved)

    def call(self, name, *args):
        ints, floats = [], []
        out = []
        for kind, a in zip(SIG[name][1], args):
            if kind == "f":
                out.append(T.F(bitsf(a)))
            else:
                out.append(a & MASK)
        r3, _, _ = T.call_ppc(self.e, FUNCS[name], out)
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        self.ids = {}
        bind(lib)

    def base(self):
        return self.lib.tpw_sip_base()

    def o(self, name):
        mac, idx = OFF[name]
        return mac if idx is None else LAY[idx]

    def g(self, name):
        return self.lib.tpw_sip_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sip_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptrw(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def start(self, labels):
        CL.host_begin(self.lib, {lab: a for a, lab in labels.items()}, self.case["queues"])
        self.started = True

    def log(self):
        return CL.host_end(self.lib)

    def done(self):
        pass

    def call(self, name, *args):
        fn = getattr(self.lib, "tpw_sip_" + name)
        r = fn(*[bitsf(a) if k == "f" else a for k, a in zip(SIG[name][1], args)])
        return r or 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_sip_"
    lib.tpw_sip_addr.restype, lib.tpw_sip_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sip_base.restype, lib.tpw_sip_base.argtypes = C.c_uint64, []
    lib.tpw_sip_alloc.restype, lib.tpw_sip_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sip_layout.argtypes = [C.POINTER(C.c_int64)]
    kinds = {"p": C.c_uint64, "u": C.c_uint32, "i": C.c_int32, "f": C.c_float}
    for name, (res, args) in SIG.items():
        fn = getattr(lib, p + name)
        fn.restype = {"": None, "p": C.c_uint64, "i": C.c_int32}[res]
        fn.argtypes = [kinds[k] for k in args]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sip_layout(out)
    LAY[:] = list(out)[:n]
    # the words the Mac and the host lay out the same way
    assert LAY[4] == 0x58 and LAY[20] == 0x6C and LAY[21] == 0x78 and LAY[22] == 0x18 and LAY[23] == 0x10, LAY
    assert LAY[16] == 8, LAY
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the operations
# Each takes the side and the case, builds the world, starts the log, calls and observes.

def label_map(s, pairs):
    return {a: lab for a, lab in pairs if a}


def op_SetInitialOrientationParams(s, c):
    blk = s.alloc(0x40)
    s.junk(blk, 16, c)
    s.start({})
    s.call("SetInitialOrientationParams", blk, c["p2"], c["a"])
    return {"blk": s.rd_words(blk, 16), "log": s.log()}


def op_SetInitialCoasterInterpParams(s, c):
    blk = s.alloc(0x80)
    s.junk(blk, 32, c)
    s.start({})
    s.call("SetInitialCoasterInterpParams", blk)
    return {"blk": s.rd_words(blk, 32), "log": s.log()}


def op_RecalcTrackParams(s, c):
    blk = s.alloc(0x100)
    s.junk(blk, 32, c)
    s.start({})
    s.call("RecalcTrackParams", blk)
    return {"blk": s.rd_words(blk, 32), "log": s.log()}


def op_GetWrappedTrackPosition(s, c):
    n1, n2 = c["n1"], c["n2"]
    arr1 = s.alloc(s.o("bez") * 8)
    arr2 = s.alloc(s.o("bez") * 8)
    spl1, spl2 = s.alloc(0x40), s.alloc(0x40)
    s.ptrw(spl1 + s.o("spl_a"), arr1)
    s.ptrw(spl2 + s.o("spl_a"), arr2)
    br1, br2 = s.alloc(0x400), s.alloc(0x400)
    s.ptrw(br1 + s.o("brace_f90"), spl1)
    s.ptrw(br2 + s.o("brace_f90"), spl2)
    ctl = s.alloc(0x40)
    s.words(ctl, [c["idx"]])
    s.ptrw(ctl + s.o("ctl_brace"), br1)
    pt = s.alloc(16)
    labels = {arr1: 1, arr1 + c["idx"] * s.o("bez"): 2, arr2: 3, pt: 4}
    labels.setdefault(arr1 + c["idx"] * s.o("bez"), 2)
    s.start(labels)
    s.call("GetWrappedTrackPosition", ctl, br2, c["t"], pt)
    return {"log": s.log(), "ctl": s.rd_word(ctl)}


def objs(s, n):
    blocks = [s.alloc(0x400) for _ in range(n)]
    for i, b in enumerate(blocks):
        s.tag(b, i + 1)
    return blocks


def op_GetCurSelectedTrackBrace(s, c):
    a, b = objs(s, 2)
    s.ptrw(s.g("gCoasterDragBrace"), [0, a, b][c["sel"]])
    s.start({})
    r = s.call("GetCurSelectedTrackBrace")
    return {"answer": s.norm(r), "log": s.log(), "g": s.norm(s.rd_ptr(s.g("gCoasterDragBrace")))}


def op_SetCurSelectedTrackBrace(s, c):
    a, b = objs(s, 2)
    s.ptrw(s.g("gCoasterDragBrace"), [0, a, b][c["old"]])
    s.start({})
    s.call("SetCurSelectedTrackBrace", [0, a, b][c["sel"]])
    return {"log": s.log(), "g": s.norm(s.rd_ptr(s.g("gCoasterDragBrace")))}


def op_IncCoasterConst2(s, c):
    inst = s.alloc(0x400)
    typ = s.alloc(0x40)
    s.words(typ + 8, [c["f1"], c["f2"]])
    if c["has_type"]:
        s.ptrw(inst + s.o("inst_type"), typ)
    s.ptrw(s.g("gCoasterEditInstance"), inst if c["has_inst"] else 0)
    s.start({inst: 1})
    r = s.call("IncCoasterConst2")
    return {"answer": r, "log": s.log(), "g": s.rd_ptr(s.g("gCoasterEditInstance")) == (inst if c["has_inst"] else 0)}


def op_IsTrackClosed(s, c):
    inst = s.alloc(0x400)
    s.words(inst + s.o("inst_3c"), [c["w"]])
    s.start({})
    return {"answer": s.call("IsTrackClosed", inst), "log": s.log()}


def op_GetCoasterInstanceFromCObject(s, c):
    obj = s.alloc(0x40)
    s.words(obj + 0x1C, [c["w"]])
    s.start({})
    return {"answer": s.call("GetCoasterInstanceFromCObject", obj if c["has_obj"] else 0) & MASK, "log": s.log()}


def op_GetTrackBrace(s, c):
    n = c["n"]
    chain = objs(s, n)
    for i, b in enumerate(chain):
        s.ptrw(b + s.o("brace_prev"), chain[i + 1] if i + 1 < n else 0)
    params = s.alloc(0x80)
    s.ptrw(params + s.o("par_last"), chain[0] if n else 0)
    s.start({})
    r = s.call("GetTrackBrace", params, c["id"])
    return {"answer": s.norm(r), "log": s.log()}


def op_ClearCoasterButtons(s, c):
    s.words(s.g("gCoasterEditEnabled"), [c["a"]])
    s.words(s.g("gCoasterEditDown"), [c["b"]])
    s.start({})
    s.call("ClearCoasterButtons")
    return {"log": s.log(), "en": s.rd_word(s.g("gCoasterEditEnabled")), "dn": s.rd_word(s.g("gCoasterEditDown"))}


def op_OutputCoasterValues(s, c):
    inst = s.alloc(0x400)
    s.ptrw(s.g("gCoasterEditInstance"), inst if c["has_inst"] else 0)
    s.words(s.g("gCoasterWord7310"), [c["w"]])
    s.start({inst: 1})
    r = s.call("OutputCoasterValues")
    return {"answer": r, "log": s.log(), "w": s.rd_word(s.g("gCoasterWord7310"))}


def op_IsCoasterLoftMode(s, c):
    s.words(s.g("gCoasterInputFlags"), [c["w"]])
    s.start({})
    return {"answer": s.call("IsCoasterLoftMode"), "log": s.log()}


def op_plain(name):
    def f(s, c):
        s.start({})
        s.call(name)
        return {"log": s.log()}
    return f


def op_LayoutRecordCoasterSpeed(s, c):
    inst = s.alloc(0x400)
    s.words(inst + s.o("inst_oid"), [c["oid"]])
    s.start({s.g("gActionRecorder"): 1, inst: 2})
    s.call("LayoutRecordCoasterSpeed", inst, c["speed"])
    return {"log": s.log()}


def op_GetHFieldMaxVertexIndices(s, c):
    hdr = s.alloc(0x100)
    hf = s.alloc(0x40)
    s.p32(hdr + s.o("hdr_hf"), hf)
    s.words(hf + s.o("hf_18"), [c["w"], c["h"]])
    s.ptrw(s.g("gHeightfieldHeader"), hdr)
    out = s.alloc(16)
    s.junk(out, 4, c)
    s.start({})
    s.call("GetHFieldMaxVertexIndices", out, out + 8)
    return {"out": s.rd_words(out, 4), "log": s.log()}


def op_DeActivateHoardings(s, c):
    st = s.alloc(0x400)
    s.words(st + s.o("set_flags"), [c["flags"]])
    s.words(st + s.o("set_speed"), [c["speed"]])
    s.ptrw(st + s.o("set_hoard"), st if c["geom"] else 0)
    s.start({})
    s.call("DeActivateHoardings", st)
    return {"flags": s.rd_word(st + s.o("set_flags")), "speed": s.rd_word(st + s.o("set_speed")),
            "geom": s.rd_ptr(st + s.o("set_hoard")) == (st if c["geom"] else 0), "log": s.log()}


def op_RestoreLevelHeightfield(s, c):
    hdr, cp = s.alloc(0x100), s.alloc(0x40)
    s.ptrw(s.g("gHeightfieldHeader"), hdr if c["has_h"] else 0)
    s.ptrw(s.g("gHeightfieldCopy"), cp if c["has_c"] else 0)
    s.start({hdr: 1, cp: 2})
    s.call("RestoreLevelHeightfield")
    return {"log": s.log()}


def op_GetLightsourceHandle(s, c):
    ride, idx = c["ride"], c["idx"]
    st = s.alloc(0x400)
    mesh = s.alloc(0x100)
    emb = s.alloc(0x40)
    recs = s.alloc(s.o("rec_size") * (idx + 2)) if c["has_recs"] else 0
    s.ptrw(s.g("gRideAnimSets") + ride * s.o("ptr"), st)
    s.ptrw(st + s.o("set_heads"), mesh)
    s.ptrw(mesh + s.o("mesh_f28"), emb)
    s.ptrw(emb + s.o("emb_rec"), recs)
    if recs:
        for i in range(idx + 2):
            s.words(recs + i * s.o("rec_size") + s.o("rec_f08"), [c["junk"][(i + 3) % len(c["junk"])]])
    s.start({})
    return {"answer": s.call("GetLightsourceHandle", ride, idx) & MASK, "log": s.log()}


def mesh_world(s, c):
    mesh = s.alloc(0x100)
    hdr = s.alloc(0x100)
    node = s.alloc(0x100)
    s.junk(node, 64, c)
    s.ptrw(mesh + s.o("mesh_hdr"), hdr)
    s.p32(hdr + s.o("hdr_f78"), node)
    return mesh, node


def op_SetMeshPosition(s, c):
    mesh, node = mesh_world(s, c)
    s.start({})
    s.call("SetMeshPosition", mesh, c["x"], c["y"], c["z"])
    w = s.rd_words(node, 0x20)
    for i in (0x40 // 4, 0x44 // 4, 0x48 // 4):
        w[i - 0] = canon_f(w[i]) if i < len(w) else 0
    return {"node": w + s.rd_words(node + 0x80, 8), "log": s.log()}


def op_ScaleMesh(s, c):
    mesh, node = mesh_world(s, c)
    pt = s.alloc(16)
    s.words(pt, [c["x"], c["y"], c["z"]])
    s.start({node + 0x10: 1, pt: 2})
    s.call("ScaleMesh", mesh, pt)
    return {"node": s.rd_words(node, 0x20), "pt": s.rd_words(pt, 3), "log": s.log()}


OPS = {n: globals().get("op_" + n) for n in FUNCS}
for _n in ("SetPlaceNormalPylon", "SetPlaceSpecialPylon", "LayoutDeleteLastTrackBrace"):
    OPS[_n] = op_plain(_n)


def run(s, case):
    try:
        return OPS[case["op"]](s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000, 0x7F7FFFFF]


def rf(r):
    k = r.random()
    if k < 0.15:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-8, 8))
    return fb(r.uniform(-300, 300))


def rw(r):
    return r.choice([0, 1, 2, 3, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, r.getrandbits(32), r.getrandbits(32), r.getrandbits(8)])


def pick(r, e, choices, d=1):
    return r.choice(choices) if e is None else choices[(e // d) % len(choices)]


def base(r, op):
    return {"op": op, "junk": [r.getrandbits(32) for _ in range(64)],
            "queues": {"FindCoasterInstanceFromEntranceID": [r.choice([0, 0x2000, 0x12345678])]}}


def g_SetInitialOrientationParams(r, e):
    c = base(r, "SetInitialOrientationParams")
    c["p2"] = pick(r, e, [0, 0x1234], 1)
    c["a"] = pick(r, e, [0, 1, -1, 5], 2) & MASK
    return c


def g_SetInitialCoasterInterpParams(r, e):
    return base(r, "SetInitialCoasterInterpParams")


def g_RecalcTrackParams(r, e):
    return base(r, "RecalcTrackParams")


def g_GetWrappedTrackPosition(r, e):
    c = base(r, "GetWrappedTrackPosition")
    c["idx"] = pick(r, e, [0, 1, 2, 5, 7], 1)
    c["n1"], c["n2"] = 0, 0
    c["t"] = pick(r, e, SPECIAL, 5) if e is not None else rf(r)
    return c


def g_GetCurSelectedTrackBrace(r, e):
    c = base(r, "GetCurSelectedTrackBrace")
    c["sel"] = pick(r, e, [0, 1, 2])
    return c


def g_SetCurSelectedTrackBrace(r, e):
    c = base(r, "SetCurSelectedTrackBrace")
    c["old"] = pick(r, e, [0, 1, 2])
    c["sel"] = pick(r, e, [0, 1, 2], 3)
    return c


def g_IncCoasterConst2(r, e):
    c = base(r, "IncCoasterConst2")
    c["has_inst"] = pick(r, e, [0, 1, 1, 1], 1)
    c["has_type"] = pick(r, e, [0, 1], 4)
    c["f1"] = pick(r, e, SPECIAL, 8) if e is not None else rf(r)
    c["f2"] = pick(r, e, SPECIAL, 88) if e is not None else rf(r)
    return c


def g_IsTrackClosed(r, e):
    c = base(r, "IsTrackClosed")
    c["w"] = pick(r, e, [0, 1, 2, 3, 0xFFFFFFFF, 0xFFFFFFFE, 0x80000001, 0x80000003, 0x1C], 1) if e is not None else rw(r)
    return c


def g_GetCoasterInstanceFromCObject(r, e):
    c = base(r, "GetCoasterInstanceFromCObject")
    c["has_obj"] = pick(r, e, [0, 1, 1], 1)
    c["w"] = pick(r, e, [0, 1, 0xFFFFFFFF, 0x12345], 3) if e is not None else rw(r)
    c["queues"]["FindCoasterInstanceFromEntranceID"] = [pick(r, e, [0, 0x2000, 0xCAFE0000], 12)]
    return c


def g_GetTrackBrace(r, e):
    c = base(r, "GetTrackBrace")
    c["n"] = pick(r, e, [0, 1, 2, 3, 5], 1)
    c["id"] = r.randrange(0, c["n"] + 1) if e is None else (e // 5) % (c["n"] + 1)
    return c


def g_ClearCoasterButtons(r, e):
    c = base(r, "ClearCoasterButtons")
    c["a"], c["b"] = pick(r, e, [0, 1, 0xFFFFFFFF, 0x55], 1) if e is not None else rw(r), rw(r)
    return c


def g_OutputCoasterValues(r, e):
    c = base(r, "OutputCoasterValues")
    c["has_inst"] = pick(r, e, [0, 1])
    c["w"] = pick(r, e, [0, 1, 0xFFFFFFFF], 2) if e is not None else rw(r)
    return c


def g_IsCoasterLoftMode(r, e):
    c = base(r, "IsCoasterLoftMode")
    c["w"] = pick(r, e, [0, 1, 2, 3, 0xFFFFFFFF, 0xFFFFFFFE, 0x80000001], 1) if e is not None else rw(r)
    return c


def g_plain(name):
    return lambda r, e: base(r, name)


def g_LayoutRecordCoasterSpeed(r, e):
    c = base(r, "LayoutRecordCoasterSpeed")
    c["oid"] = pick(r, e, [0, 1, 0xFFFF, 0x10000, 0x12345678, 0xFFFFFFFF], 1) if e is not None else rw(r)
    c["speed"] = pick(r, e, [0, 1, 0xFFFF, 0x10000, 0x87654321, 0xFFFFFFFF], 6) if e is not None else rw(r)
    return c


def g_GetHFieldMaxVertexIndices(r, e):
    c = base(r, "GetHFieldMaxVertexIndices")
    c["w"], c["h"] = pick(r, e, [0, 1, 255, 0xFFFFFFFF], 1) if e is not None else rw(r), rw(r)
    return c


def g_DeActivateHoardings(r, e):
    c = base(r, "DeActivateHoardings")
    c["geom"] = pick(r, e, [0, 1], 1)
    c["flags"] = pick(r, e, [0, 0x20, 0x40, 0x60, 0x80, 0xE0, 0xFFFFFFFF, 0xFFFFFF9F, 0x1F, 0x100], 2) if e is not None else (rw(r) if r.random() < 0.5 else r.choice([0x20, 0x60, 0xE0]) | r.getrandbits(32))
    c["speed"] = rf(r)
    return c


def g_RestoreLevelHeightfield(r, e):
    c = base(r, "RestoreLevelHeightfield")
    c["has_h"] = pick(r, e, [0, 1], 1)
    c["has_c"] = pick(r, e, [0, 1], 2)
    return c


def g_GetLightsourceHandle(r, e):
    c = base(r, "GetLightsourceHandle")
    c["ride"] = pick(r, e, [1, 2, 3, 7], 1)
    c["has_recs"] = pick(r, e, [0, 1, 1, 1], 4)
    c["idx"] = 0 if not c["has_recs"] else pick(r, e, [0, 1, 2, 5], 16)
    if not c["has_recs"]:
        c["has_recs"] = 0
    return c


def g_SetMeshPosition(r, e):
    c = base(r, "SetMeshPosition")
    c["x"] = pick(r, e, SPECIAL, 1) if e is not None else rf(r)
    c["y"] = pick(r, e, SPECIAL, 11) if e is not None else rf(r)
    c["z"] = pick(r, e, SPECIAL, 111) if e is not None else rf(r)
    return c


def g_ScaleMesh(r, e):
    c = base(r, "ScaleMesh")
    c["x"] = pick(r, e, SPECIAL, 1) if e is not None else rf(r)
    c["y"] = pick(r, e, SPECIAL, 11) if e is not None else rf(r)
    c["z"] = pick(r, e, SPECIAL, 111) if e is not None else rf(r)
    return c


GEN = {n: globals().get("g_" + n) for n in FUNCS}
for _n in ("SetPlaceNormalPylon", "SetPlaceSpecialPylon", "LayoutDeleteLastTrackBrace"):
    GEN[_n] = g_plain(_n)

EDGE_COUNT = 40


def make_test(name):
    gen = GEN[name]
    seed = 51074 + sum(map(ord, name))

    def edges():
        r = random.Random(seed)
        for e in range(EDGE_COUNT):
            yield gen(r, e)

    def rnd(r):
        return gen(r, None)

    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


TESTS = [make_test(n) for n in FUNCS]
