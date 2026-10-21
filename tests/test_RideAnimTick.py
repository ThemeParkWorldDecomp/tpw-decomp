"""Differential test for src/game/TPEmbed.cpp, src/game/TPMesh.cpp, src/game/rides/RideAnim.cpp (TickRideAnimSet, DoAnimateRideSet,
RAAnimateTextureFrames, AnimateNode, GetAnimFrameData, IsMeshDrawEnabled, TPProcessEmbeddedData and
DoWorldTransformsForEmbeddedData), against libtpw_unit_RideAnimTick.dylib.

The eight functions run as the original on the PowerPC side (and DisableMeshDraw, SetMatrixIdentity and
CombineMatricesNoWDest2nd 0x100c28fc, which the translation links for real as well). Everything else they
call is a fake on both sides that records its name, its arguments and the words of the records it is
given, and some of them write what the real function would write, from the case's queues (SetNewAnim
rewrites its control, the shape interpolators and the orientation helpers write their outputs):
tests/unit_RideAnimTick.cpp is the host's half, the Stubs below the PowerPC's.

The mesh file's records (headers, nodes, animations, keys, shapes, textures) have the file's layout on
both sides, so one description builds them for the two (Side classes: the PowerPC side writes big-endian
words and absolute addresses, the host little-endian words and arena offsets, as the Ptr32 fields hold
them). The ride set, the mesh and the embedded records have native pointers on the host and the Mac's
layout on the PowerPC side; the host builds them from a list of numbers that the PowerPC side lays out
from the same list. After a call the answers, the call log and every word the function could have
changed are compared; pointers are compared as labels, floats as floats (NaNs equal, signed zeros not).

Unicorn computes fmadds in double precision and rounds twice; HardwareRounding (one code hook per fused
instruction, as in test_FrameRest.py) gives each the processor's single rounding.
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_RideAnimTick"

MASK = 0xFFFFFFFF
TPGLOBALS = 0x103B74C8        # +0x400c engine flags, +0x4010 clock, +0x4018 fixed clock, +0x401c, +0x4020

TICK_SET, DO_ANIMATE, TEXTURE_FRAMES, ANIMATE_NODE = 0x100C6A20, 0x100C6C2C, 0x100C3678, 0x100C45D4
FRAME_DATA, IS_DRAW, PROCESS, DO_WORLD = 0x100C34BC, 0x10065364, 0x101FFB0C, 0x101FEF24
DISABLE_MESH, DISABLE_RANGE, SET_IDENTITY, COMBINE_MATRICES = 0x100653A4, 0x1006548C, 0x100C0E28, 0x100C28FC
NAN_BITS = 0x7FC00000


# ------------------------------------------------------------------ small helpers


def ws(e, address, *words):
    """Writes 32-bit words, big-endian, to the emulator's memory."""
    e.write(address, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def fb(x):
    """The bit pattern of a single-precision value (a NaN as one pattern)."""
    if x != x:
        return NAN_BITS
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bf(bits):
    """The single-precision value of a bit pattern."""
    return struct.unpack(">f", struct.pack(">I", bits & MASK))[0]


def canon(bits):
    """A word with every NaN pattern made one (the processors differ in the NaNs they make)."""
    bits &= MASK
    return NAN_BITS if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def f32(x):
    return T.f32(x)


class Globals:
    """Keeps the original's globals a case writes and puts them back when it is over."""

    def __init__(self, e):
        self.e = e
        self.kept = []

    def keep(self, address, size):
        self.kept.append((address, self.e.read(address, size)))

    def close(self):
        for address, data in reversed(self.kept):
            self.e.write(address, data)


LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Unicorn (QEMU 5.0) computes fmadds, fmsubs, fnmadds and fnmsubs in double precision and
    rounds the result to single, which can differ by one unit in the last place from the
    processor's single rounding of the exact result. A code hook looks at each instruction
    before it runs and rewrites its target register just before the next one, with the C
    library's fmaf (one rounding). ranges: the (begin, end) address ranges of the functions
    that use fused instructions (AnimateNode, DoWorldTransformsForEmbeddedData, Combine...)."""

    changed = 0    # results the correction replaced (all cases so far), for the notes
    seen = 0       # fused instructions it looked at

    def __init__(self, e, ranges):
        self.e, self.pending, self.hooks, self.ranges = e, None, [], ranges

    def __enter__(self):
        for begin, end in self.ranges:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            target, value = self.pending
            self.pending = None
            corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
            if fpr(e, target) != corrected and not (value != value and fpr_double(e, target) != fpr_double(e, target)):
                HardwareRounding.changed += 1
            uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        HardwareRounding.seen += 1
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        for x in (a, c, b):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        subtract = kind in (28, 30)
        value = float(LIBM.fmaf(a, c, -b if subtract else b))
        if kind in (30, 31):
            value = -value
        self.pending = (rd, value)


def function_range(e, address):
    """(begin, end) of the function at `address` in the program's function table."""
    for f in e.functions:
        if f["entry"] == address:
            return address, address + f["size"]
    raise KeyError(hex(address))


# ------------------------------------------------------------------ the two sides' memory


class Ref:
    """A pointer field's value: the address of the object `name` (None: null)."""

    def __init__(self, name):
        self.name = name


class Obj:
    """An object of the case: a size, a label for the call log (-1: the next free one) and the
    words to write. sub(offset, label): a label for a place inside it (the records of an array)."""

    def __init__(self, size, label=-1):
        self.size, self.label, self.writes, self.subs = size, label, [], []

    def w(self, offset, fmt, *values):
        self.writes.append((offset, fmt, values))
        return self

    def sub(self, offset, label=-1):
        self.subs.append((offset, label))
        return self


AUTO_LABEL = 100    # the first of the labels that objects get without asking for one


class PpcSide:
    """The PowerPC's memory: absolute addresses, big-endian words."""

    def __init__(self, e, log):
        self.e, self.log, self.addr = e, log, {}

    def new(self, name, size, label):
        self.addr[name] = self.e.alloc(size, b"\xa5" * size)
        if label:
            self.log.labels[self.addr[name]] = label

    def absolute(self, name):
        return self.addr[name] if name is not None else 0

    pointer = absolute

    def name_place(self, name, offset, label):
        self.log.labels[self.addr[name] + offset] = label

    def w(self, name, offset, fmt, *values):
        self.e.write(self.addr[name] + offset, struct.pack(">" + fmt, *values))

    def r(self, name, offset, fmt):
        return struct.unpack(">" + fmt, self.e.read(self.addr[name] + offset, struct.calcsize(">" + fmt)))

    def label(self, field):
        return self.log.lab(field) & MASK


class HostSide:
    """The host's memory: arena blocks, little-endian words, Ptr32 offsets from the arena's base."""

    def __init__(self, lib):
        self.lib, self.addr = lib, {}
        lib.tpw_rat_base.restype = C.c_uint64
        lib.tpw_rat_alloc.restype = C.c_uint64
        lib.tpw_rat_alloc.argtypes = [C.c_uint32]
        lib.tpw_cl_lab.argtypes = [C.c_void_p]
        lib.tpw_cl_lab.restype = C.c_int32
        lib.tpw_rat_alloc(16)
        self.base = lib.tpw_rat_base()

    def new(self, name, size, label):
        a = self.lib.tpw_rat_alloc(size)
        C.memset(a, 0xA5, size)
        self.addr[name] = a
        if label:
            self.lib.tpw_cl_label(label, C.c_void_p(a))

    def absolute(self, name):
        return self.addr[name] if name is not None else 0

    def pointer(self, name):
        return self.addr[name] - self.base if name is not None else 0

    def name_place(self, name, offset, label):
        self.lib.tpw_cl_label(label, C.c_void_p(self.addr[name] + offset))

    def w(self, name, offset, fmt, *values):
        data = struct.pack("<" + fmt, *values)
        C.memmove(self.addr[name] + offset, data, len(data))

    def r(self, name, offset, fmt):
        return struct.unpack("<" + fmt, C.string_at(self.addr[name] + offset, struct.calcsize("<" + fmt)))

    def label(self, field):
        return (self.lib.tpw_cl_lab(C.c_void_p(self.base + field)) if field else 0) & MASK


def build(side, objs):
    """Allocates the objects (poisoned with 0xa5, labelled) and writes their fields."""
    auto = AUTO_LABEL
    for name, o in objs.items():
        label = o.label
        if label == -1:
            label, auto = auto, auto + 1
        side.new(name, o.size, label)
        for offset, sub_label in o.subs:
            if sub_label == -1:
                sub_label, auto = auto, auto + 1
            side.name_place(name, offset, sub_label)
    for name, o in objs.items():
        for offset, fmt, values in o.writes:
            side.w(name, offset, fmt, *[side.pointer(v.name) if isinstance(v, Ref) else (0 if v is None else v) for v in values])


def dump(side, items):
    """The words of the objects that a function may have changed. items: (name, offset, kind,
    count) with kind I (32-bit words), H (16-bit), F (floats: NaNs equal, signed zeros not) or P
    (pointer fields, as labels)."""
    out = []
    for name, offset, kind, n in items:
        if kind == "I":
            out += list(side.r(name, offset, "%dI" % n))
        elif kind == "H":
            out += list(side.r(name, offset, "%dH" % n))
        elif kind == "F":
            out += [float("nan") if v != v else v for v in side.r(name, offset, "%df" % n)]
        elif kind == "P":
            out += [side.label(v) for v in side.r(name, offset, "%dI" % n)]
        else:
            raise ValueError(kind)
    return out


CONTROL_DUMP = [(0, "I", 3), (0xC, "F", 1), (0x10, "I", 3), (0x1C, "F", 2), (0x24, "I", 3), (0x30, "F", 1), (0x34, "P", 1)]


def control_dump(name):
    """All fourteen words of a time control (floats as floats, the animation it applied as a label)."""
    return [(name, offset, kind, n) for offset, kind, n in CONTROL_DUMP]


# ------------------------------------------------------------------ the PowerPC's fakes


def stack_word(e, n):
    """The n-th argument word after the eight in registers (a callee's view at its entry)."""
    return e.u32(e.r(1) + 0x38 + 4 * n)


def words_at(e, address, n):
    return list(struct.unpack(">" + "I" * n, e.read(address, 4 * n)))


def rec_matrix(e, name, address):
    words = [canon(w) for w in words_at(e, address, 16)]
    e.cl.rec(name, words[:8])
    e.cl.rec(name, words[8:])


def node_label(e, matrix):
    """The fakes name a matrix by the node it is the member of."""
    return e.cl.lab(matrix - 0x10) & MASK


def make_stubs():
    """The fakes of everything the eight functions call, on the PowerPC side. Each records the
    event the host's fake of the same name records."""
    S = CL.Stubs()
    S.real.update([TICK_SET, DO_ANIMATE, TEXTURE_FRAMES, ANIMATE_NODE, FRAME_DATA, IS_DRAW, PROCESS, DO_WORLD,
                   DISABLE_MESH, DISABLE_RANGE, SET_IDENTITY, COMBINE_MATRICES])

    def queued(e, key, default=0):
        return e.cl.ret(key, default) & MASK

    # the engine's map, as in test_MeshDraw.py
    def engine_link(e):
        return e.engine_link

    def single(name):
        def stub(e):
            e.cl.rec(name, [int(e.r(4) == engine_link(e)), T.s32(e.r(5))])
            e.set_r(3, 0)
        return stub

    def bounds(name):
        def stub(e):
            ints = [T.s32(e.r(i)) for i in (4, 5, 6, 7)] + [int(e.r(8) == engine_link(e))]
            if name == "LinkRange":
                ints.append(T.s32(e.r(9)))
            e.cl.rec(name, ints)
            e.set_r(3, 0)
        return stub

    def local_matrix(e):
        e.cl.rec("SetLocalMatrix", [int(e.r(3) == e.engine_object), T.s32(e.r(4))])
        rec_matrix(e, "Matrix", e.r(5))

    S.imports["Link__7CMapWhoFP12CMapLinkBasei"] = single("Link")
    S.imports["Unlink__7CMapWhoFP12CMapLinkBasei"] = single("Unlink")
    S.imports["Link__7CMapWhoFiiiiP12CMapLinkBasei"] = bounds("LinkRange")
    S.imports["Unlink__7CMapWhoFiiiiP12CMapLinkBase"] = bounds("UnlinkRange")
    S.imports["SetLocalMatrix__5CMeshFiR7sMatrix"] = local_matrix

    def fmod(e):
        x, y = e.f(1), e.f(2)
        if math.isnan(x) or math.isnan(y) or math.isinf(x) or y == 0.0:
            e.set_f1(math.nan)
        else:
            e.set_f1(math.fmod(x, y))

    S.imports["fmod"] = fmod

    # starting an animation: records the control it is given, then rewrites it from the queues
    def control_events(e, control):
        words = [canon(w) for w in words_at(e, control, 14)]
        words[13] = e.cl.lab(words[13]) & MASK
        e.cl.rec("Control", words[:7])
        e.cl.rec("Control", words[7:])

    def set_new_anim(e):
        control = e.r(6)
        e.cl.rec("SetNewAnim", [e.cl.lab(e.r(3)) & MASK, e.r(4), e.r(5), e.cl.lab(control) & MASK, e.r(7), fb(e.f(1))])
        control_events(e, control)
        special = e.r(4) in (0xD, 0xE)
        ws(e, control + 4, queued(e, "NewKind", e.u32(control + 4) if special else e.r(4)),
           queued(e, "NewIndex", e.u32(control + 8) if special else e.r(5)))
        keep = queued(e, "FlagsAnd", 0xFFFFFFFF)
        ws(e, control, (e.u32(control) & keep) | queued(e, "FlagsOr"))
        ws(e, control + 0x1C, queued(e, "NewLength", e.u32(control + 0x1C)))
        ws(e, control + 0x10, queued(e, "NewStart", e.u32(control + 0x10)))

    S.internal(0x100C60FC, set_new_anim)

    def decompress_morph(e):
        e.cl.rec("RADecompressMorph", [e.cl.lab(e.r(3)) & MASK, e.cl.lab(e.r(4)) & MASK, fb(e.f(1)), e.r(6), T.s32(e.r(7))])

    S.internal(0x100C40DC, decompress_morph)

    def calc_orientation(e):
        e.cl.rec("RACalcOrientation", [e.cl.lab(e.r(3)) & MASK, e.r(4), fb(e.f(1))])
        ws(e, e.r(6), *[queued(e, "Axis") for _ in range(9)])

    S.internal(0x100C8054, calc_orientation)

    def combine_axis(e):
        e.cl.rec("CombineAxis", [node_label(e, e.r(4))] + [canon(w) for w in words_at(e, e.r(3), 9)])
        rec_matrix(e, "CombineDestination", e.r(4))

    S.internal(0x100C37A4, combine_axis)

    def set_orientation(e):
        x, y, z = e.r(4), e.r(5), e.r(6)
        words = [canon(w) for w in words_at(e, x, 3) + words_at(e, y, 3) + words_at(e, z, 3)]
        e.cl.rec("SetLMatrixOrientation", [node_label(e, e.r(3))] + words + [int(y == x + 12), int(z == y + 12)])
        rec_matrix(e, "SetLMatrixOrientationMatrix", e.r(3))

    S.internal(0x100C1EAC, set_orientation)

    def set_scale(e):
        values = [canon(e.u32(e.r(i))) for i in (4, 5, 6)]
        e.cl.rec("SetLMatrixScale", [node_label(e, e.r(3))] + values)
        rec_matrix(e, "SetLMatrixScaleMatrix", e.r(3))

    S.internal(0x100C1AB8, set_scale)

    def set_xz(e):
        e.cl.rec("SetLMatrixXZ", [node_label(e, e.r(3))] + [canon(w) for w in words_at(e, e.r(4), 3)])
        rec_matrix(e, "SetLMatrixXZMatrix", e.r(3))

    S.internal(0x100C2EE8, set_xz)

    def interp_posn(e):
        e.cl.rec("RAInterpPosnAtFrame", [e.cl.lab(e.r(3)) & MASK, e.r(4), fb(e.f(1)), T.s32(e.r(7))])
        ws(e, e.r(6), queued(e, "Place"))

    S.internal(0x100C8CA8, interp_posn)

    def shape_fake(name):
        def stub(e):
            e.cl.rec(name, [e.cl.lab(e.r(3)) & MASK, e.cl.lab(e.r(4)) & MASK, e.r(5), fb(e.f(1))])
            ws(e, e.r(7), queued(e, "Point"), queued(e, "Point"), queued(e, "Point"))
        return stub

    S.internal(0x100C84EC, shape_fake("RABezierInter"))
    S.internal(0x100C86E8, shape_fake("RABezierTangent"))
    S.internal(0x100C88E0, shape_fake("RALinearInter"))
    S.internal(0x100C89C8, shape_fake("RALinearTangent"))
    S.internal(0x100C8A8C, shape_fake("RACatmulInter"))
    S.internal(0x100C82D0, CL.recorder("RAAnimateTextureCoords", "LLi"))

    def calc_orient_track(e):
        points = [canon(w) for w in words_at(e, e.r(3), 9)]
        position = [canon(w) for w in words_at(e, e.r(4), 3)]
        normal = [canon(w) for w in words_at(e, e.r(5), 3)]
        morph = e.r(6)
        morph_words = [e.u16(morph) | (e.u16(morph + 2) << 16)] + words_at(e, morph + 4, 3)
        e.cl.rec("CalculateOrient", [e.cl.lab(morph) & MASK] + points)
        e.cl.rec("CalculateOrientMore", position + normal + morph_words)
        rec_matrix(e, "CalculateOrientMatrix", e.r(7))
        matrix = e.r(7)
        for row in range(3):
            ws(e, matrix + 16 * row, *[queued(e, "Orient") for _ in range(3)])

    S.internal(0x101FEC54, calc_orient_track)
    S.internal(0x100C7224, CL.recorder("RecalculateInvalidNodeFaceNormals", "L"))
    return S


STUBS = None


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    e.u16 = lambda a: struct.unpack(">H", e.read(a, 2))[0]
    e.engine_object = e.engine_link = 0
    STUBS = make_stubs()


# ------------------------------------------------------------------ running a case on both sides


def ppc_run(e, case, entry, args, results, ranges=()):
    """Builds the case's objects, calls `entry` and returns what `results(side, r3)` gives with the
    call log and TPGlobals' five words."""
    g = Globals(e)
    g.keep(TPGLOBALS + 0x400C, 0x18)
    saved = STUBS.activate(e)
    try:
        e.cl = CL.PpcLog({}, case.get("queues"))
        side = PpcSide(e, e.cl)
        build(side, case.get("objs", {}))
        engine = case.get("engine")
        if engine is not None:
            e.engine_object, e.engine_link = engine, engine + 0x154
        g0 = case.get("globals", (0, 0, 0))
        ws(e, TPGLOBALS + 0x400C, g0[0], g0[1])
        ws(e, TPGLOBALS + 0x4018, g0[2], 0x5A5A5A5A, 0x6B6B6B6B)
        e.set_r(30, 0)     # (DoAnimateRideSet's held end with no animation takes the animation from r30)
        with HardwareRounding(e, ranges):
            r3 = T.call_ppc(e, entry, args(side))[0]
        out = {"result": results(side, r3), "log": e.cl.log,
               "globals": [e.u32(TPGLOBALS + o) for o in (0x400C, 0x4010, 0x4018, 0x401C, 0x4020)]}
        return out
    finally:
        CL.Stubs.deactivate(e, saved)
        g.close()


def host_run(lib, case, function, args, results):
    """The host's half of ppc_run."""
    lib.tpw_rat_reset()
    CL.host_begin(lib, {}, case.get("queues"))
    side = HostSide(lib)
    build(side, case.get("objs", {}))
    g0 = case.get("globals", (0, 0, 0))
    lib.tpw_rat_set_globals(g0[0], g0[1], g0[2], 0x5A5A5A5A, 0x6B6B6B6B)
    fn = getattr(lib, function)
    result = fn(*args(side))
    out = {"result": results(side, result)}
    log = CL.host_end(lib)
    values = (C.c_uint32 * 5)()
    lib.tpw_rat_get_globals(values)
    out["log"] = log
    out["globals"] = list(values)
    return out


# ------------------------------------------------------------------ IsMeshDrawEnabled


def ppc_is_draw(e, case):
    saved = STUBS.activate(e)
    try:
        mesh = e.alloc(0x40, b"\xa5" * 0x40)
        ws(e, mesh, case)
        return {"result": [T.s32(T.call_ppc(e, IS_DRAW, [mesh])[0])]}
    finally:
        CL.Stubs.deactivate(e, saved)


def host_is_draw(lib, case):
    fn = lib.tpw_rat_is_mesh_draw_enabled
    fn.argtypes = [C.POINTER(C.c_uint32)]
    fn.restype = C.c_int32
    return {"result": [fn((C.c_uint32 * 1)(case))]}


def is_draw_edges():
    for word in (0, 1, 2, 3, 0x80000000, 0x80000001, 0xFFFFFFFF, 0xFFFFFFFE, 0x10, 0x11, 0x7FFFFFFF):
        yield word


# ------------------------------------------------------------------ GetAnimFrameData


def frame_objs(case):
    keys = Obj(0x80, 4)
    stride = {1: 4, 2: 0x14, 4: 0x10}.get(case["type"] & MASK, 4)
    for i, (frame, extra) in enumerate(case["keys"]):
        keys.w(i * stride, "HH", frame, extra)
    control = Obj(0x38, 3).w(0x1C, "ff", case["length"], case["frame"])
    return {"control": control, "keys": keys, "out": Obj(12, 5).w(0, "IIf", 0x11111111, 0x22222222, -1234.5)}


def frame_dump(side, result):
    return [T.s32(result) if isinstance(result, int) else result] + \
        dump(side, [("out", 0, "I", 2), ("out", 8, "F", 1), ("control", 0x1C, "F", 2), ("keys", 0, "H", 16)])


def ppc_frame(e, case):
    return ppc_run(e, dict(case, objs=frame_objs(case)), FRAME_DATA,
                   lambda s: [s.addr["control"], s.addr["keys"], case["count"], case["type"], case["wrap"],
                              s.addr["out"], s.addr["out"] + 4, s.addr["out"] + 8],
                   frame_dump)


def host_frame(lib, case):
    return host_run(lib, dict(case, objs=frame_objs(case)), "tpw_rat_frame_data",
                    lambda s: [C.c_uint64(s.addr["control"]), C.c_uint64(s.addr["keys"]), C.c_uint32(case["count"]),
                               C.c_uint32(case["type"]), C.c_int32(T.s32(case["wrap"])), C.c_uint64(s.addr["out"]),
                               C.c_uint64(s.addr["out"] + 4), C.c_uint64(s.addr["out"] + 8)],
                    frame_dump)


def frame_case(keys, frame, count=None, type=1, wrap=0, length=100.0):
    return {"keys": keys, "frame": f32(frame), "count": len(keys) if count is None else count, "type": type,
            "wrap": wrap, "length": f32(length), "globals": (0, 0, 0)}


def frame_edges():
    for type in (1, 2, 4, 0, 3, 5, 6, 0xFFFFFFFF, 0x80000001, 0x80000000, 0x7FFFFFFF):
        yield frame_case([(0, 7), (4, 8), (9, 9)], 5.5, type=type)
    keys = [(0, 1), (3, 2), (3, 3), (10, 4)]
    for frame in (-1.0, -0.0, 0.0, 0.5, 2.9999, 3.0, 3.5, 9.99, 10.0, 11.0, 1e9, 4294967295.0, 4294967296.0, 8e9,
                  math.nan, math.inf, -math.inf, 65535.0, 65536.0):
        for wrap in (0, 1, 0xFFFFFFFF):
            yield frame_case(keys, frame, wrap=wrap, type=2)
    for count in range(0, 5):
        yield frame_case([(2, 1), (2, 1), (2, 1), (5, 5)], 2.0, count=count)
        yield frame_case([(2, 1), (2, 1), (2, 1), (5, 5)], 7.0, count=count, wrap=1)
    yield frame_case([(0xFFFF, 1)], 70000.0, wrap=0)
    yield frame_case([(0xFFFF, 1)], 70000.0, wrap=1)
    yield frame_case([(7, 1), (7, 1)], 7.0, wrap=1)
    yield frame_case([(7, 1), (7, 1)], 7.0, wrap=0, type=4)


def frame_random(r):
    count = r.randrange(0, 7)
    keys = sorted((r.choice([0, 0, 1, 2, 5, 10, 100, r.randrange(0, 65536)]), r.randrange(0, 65536)) for _ in range(count))
    if r.random() < 0.1:
        keys = [(r.randrange(0, 65536), 3) for _ in range(count)]
    frame = r.choice([T.rfloat(r), r.uniform(-2, 120), float(r.choice(keys)[0]) if keys else 1.0,
                      (keys[-1][0] + 0.5) if keys else 3.0, r.choice(T.FLOAT_EDGES)])
    return frame_case(keys, frame, count=count if r.random() < 0.9 else r.randrange(0, 7),
                      type=r.choice([1, 2, 4, 4, 2, 1, 0, 3, 5, r.getrandbits(32)]),
                      wrap=r.choice([0, 1, 2, 0xFFFFFFFF, r.getrandbits(32)]), length=r.uniform(0, 100))


# ------------------------------------------------------------------ TickRideAnimSet


CONTROL_COUNT = 3


def control_words(flags=0, kind=0, index=0, rate=1.0, start=0, current=0, held=0, length=10.0, frame=0.0,
                  queued_kind=0xC, queued_index=0, queued_flags=0, speed=0.0):
    """The fourteen words of a time control (the last, the animation applied last, is poisoned)."""
    return [flags & MASK, kind & MASK, index & MASK, fb(rate), start & MASK, current & MASK, held & MASK, fb(length),
            fb(frame), queued_kind & MASK, queued_index & MASK, queued_flags & MASK, fb(speed), 0xA5A5A5A5]


def tick_words(c):
    out = [int(c["present"]), c["flags"], c["animations"], c["count"]]
    for control in c["controls"]:
        out += control
    return out


def ppc_tick_set(e, case):
    objs = {"set": Obj(0xFC, 10), "controls": Obj(CONTROL_COUNT * 0x38)}
    objs["set"].w(4, "I", case["flags"]).w(0xE, "H", case["count"]).w(0x10, "I", Ref("controls")).w(0xA8, "I", case["animations"])
    for i, words in enumerate(case["controls"]):
        objs["controls"].w(0x38 * i, "14I", *words)

    def results(side, r3):
        out = [T.s32(r3)]
        for i in range(CONTROL_COUNT):
            out += dump(side, [("controls", 0x38 * i + off, kind, n) for off, kind, n in CONTROL_DUMP])
        return out

    return ppc_run(e, dict(case, objs=objs), TICK_SET,
                   lambda s: [s.addr["set"] if case["present"] else 0], results)


def host_tick_set(lib, case):
    CL.host_begin(lib, {}, case.get("queues"))
    g0 = case["globals"]
    lib.tpw_rat_set_globals(g0[0], g0[1], g0[2], 0x5A5A5A5A, 0x6B6B6B6B)
    fn = lib.tpw_rat_tick_set
    fn.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    fn.restype = C.c_int32
    out = (C.c_int64 * 64)()
    words = tick_words(case)
    n = fn((C.c_int64 * len(words))(*words), out)
    raw = list(out[:n])
    # the host returns the raw words: the dump of the PowerPC side has floats as floats and the
    # animation applied last as a label (nothing is applied here: the poison, which is no label)
    result = [T.s32(raw[0] & MASK)]
    for i in range(CONTROL_COUNT):
        w = [x & MASK for x in raw[1 + 14 * i:15 + 14 * i]]
        result += w[0:3] + [nan_float(bf(w[3]))] + w[4:7] + [nan_float(bf(w[7])), nan_float(bf(w[8]))] + w[9:12] + \
            [nan_float(bf(w[12])), MASK if w[13] == 0xA5A5A5A5 else w[13]]
    log = CL.host_end(lib)
    values = (C.c_uint32 * 5)()
    lib.tpw_rat_get_globals(values)
    return {"result": result, "log": log, "globals": list(values)}


def nan_float(x):
    return float("nan") if x != x else x


def tick_case(controls, flags=0, animations=1, count=3, present=True, clock=1000, fixed=2000):
    return {"present": present, "flags": flags & MASK, "animations": animations, "count": count,
            "controls": controls, "globals": (0, clock & MASK, fixed & MASK)}


def tick_edges():
    plain = control_words(start=0, rate=1.0, length=10.0)
    busy = control_words(kind=3, start=100, rate=2.0, length=5.0)
    done = control_words(kind=0xC)
    fixed = control_words(flags=0x40, kind=1, start=0, rate=1.0, length=1.0)
    for present in (True, False):
        yield tick_case([plain, busy, fixed], present=present)
    for flags in (0, 0x8000, 0x8004, 4, 0x7FFF):
        for animations in (0, 1, 5):
            yield tick_case([busy, busy, busy], flags=flags, animations=animations)
    for count in range(4):
        yield tick_case([busy, done, fixed], count=count)
        yield tick_case([done, done, done], count=count)
    # past the length / not / exactly on it
    for clock in (100, 1000, 1100, 1140, 1150, 1151, 100000, 0, 0xFFFFFFFF):
        yield tick_case([control_words(kind=1, start=1000, rate=1.0, length=5.0)] * 3, clock=clock)
    # a rate that is zero, negative, NaN, infinite
    for rate in (0.0, -1.0, math.nan, math.inf, 1e-30, 1e30):
        yield tick_case([control_words(kind=1, start=0, rate=rate, length=5.0)] * 3)
    # the length NaN / negative
    for length in (math.nan, -1.0, 0.0, -0.0, math.inf):
        yield tick_case([control_words(kind=1, start=0, rate=1.0, length=length)] * 3)


def tick_random(r):
    controls = []
    for _ in range(CONTROL_COUNT):
        controls.append(control_words(
            flags=r.choice([0, 0x40, 2, 4, 6, 0x42, r.getrandbits(32)]),
            kind=r.choice([0, 1, 5, 11, 0xC, 0xC, r.getrandbits(32)]), index=r.randrange(0, 4),
            rate=r.choice([1.0, 0.5, 2.0, r.uniform(-3, 3), T.rfloat(r)]),
            start=r.choice([0, 100, 1000, 0xFFFFFFF0, r.getrandbits(32)]), current=r.getrandbits(32),
            held=r.getrandbits(32), length=r.choice([0.0, 1.0, 5.0, 100.0, r.uniform(-5, 50), T.rfloat(r)]),
            frame=T.rfloat(r), queued_kind=r.choice([0xC, 3]), queued_index=r.randrange(4), queued_flags=r.getrandbits(8),
            speed=r.choice([0.0, 1.0, T.rfloat(r)])))
    return tick_case(controls, flags=r.choice([0, 0, 0x8000, 4, 0x8004, r.getrandbits(32)]),
                     animations=r.choice([0, 1, 1, 2]), count=r.randrange(0, 4), present=r.random() < 0.95,
                     clock=r.choice([0, 1000, 2000, 1500, r.getrandbits(32)]),
                     fixed=r.choice([0, 1000, 5000, r.getrandbits(32)]))


# ------------------------------------------------------------------ RAAnimateTextureFrames


TEXTURES, ANIMATED, PAIRS = 3, 3, 4


def texture_objs(c):
    textures = Obj(8 * TEXTURES, 6)
    for i, (flags, frame) in enumerate(c["textures"]):
        textures.w(8 * i, "HH", flags, frame)
    animated = Obj(8 * ANIMATED, 7)
    objs = {"textures": textures, "animated": animated}
    for i, (index, pairs) in enumerate(list(c["animated"]) + [(0, [])] * (ANIMATED - len(c["animated"]))):
        pair_obj = Obj(max(4 * PAIRS, 4), 20 + i)
        for k, (frame, value) in enumerate(pairs):
            pair_obj.w(4 * k, "HH", frame, value)
        objs["pairs%d" % i] = pair_obj
        animated.w(8 * i, "HH", index, len(pairs)).w(8 * i + 4, "I", Ref("pairs%d" % i))
    objs["header"] = Obj(0xB8, 5).w(0x50, "I", Ref("textures"))
    objs["animation"] = Obj(0x48, 8).w(0x18, "H", c["count"]).w(0x30, "I", Ref("animated"))
    objs["control"] = Obj(0x38, 4).w(0x20, "f", c["frame"])
    return objs


def texture_dump(side, result):
    return dump(side, [("textures", 0, "H", 2 * TEXTURES), ("control", 0x20, "F", 1)])


def ppc_textures(e, case):
    return ppc_run(e, dict(case, objs=texture_objs(case)), TEXTURE_FRAMES,
                   lambda s: [s.addr["header"], s.addr["animation"], s.addr["control"]], texture_dump)


def host_textures(lib, case):
    return host_run(lib, dict(case, objs=texture_objs(case)), "tpw_rat_texture_frames",
                    lambda s: [C.c_uint64(s.addr["header"]), C.c_uint64(s.addr["animation"]), C.c_uint64(s.addr["control"])],
                    texture_dump)


def texture_case(frame, animated, textures=None, count=None):
    return {"frame": f32(frame), "animated": animated, "count": len(animated) if count is None else count,
            "textures": textures or [(0x0100, 5), (0x0200, 6), (0x0300, 7)], "globals": (0, 0, 0)}


def texture_edges():
    pairs = [(0, 10), (5, 11), (9, 12)]
    for frame in (-1.0, 0.0, 4.9, 5.0, 8.9, 9.0, 100.0, math.nan, math.inf, 4294967296.0):
        yield texture_case(frame, [(0, pairs)])
    yield texture_case(5.0, [(0, pairs), (1, pairs), (2, pairs)])
    yield texture_case(5.0, [(0, pairs), (0, [(0, 99)]), (0, [])])
    yield texture_case(5.0, [(0, [(0, 5)])])           # the texture already shows 5
    yield texture_case(5.0, [(1, [(0, 6)])])
    yield texture_case(5.0, [(0, pairs), (1, pairs), (2, pairs)], count=0)
    yield texture_case(5.0, [(0, pairs), (1, pairs), (2, pairs)], count=2)
    yield texture_case(5.0, [(0, [])])
    yield texture_case(5.0, [(2, [(7, 1), (6, 2), (5, 3), (4, 4)])])   # frames out of order
    yield texture_case(5.0, [(2, [(5, 0x7FFF), (5, 0xFFFF)])], textures=[(0xFFFF, 0), (0, 0xFFFF), (0x400, 0xFFFF)])


def texture_random(r):
    animated = []
    for _ in range(r.randrange(0, ANIMATED + 1)):
        frames = sorted(r.randrange(0, 12) for _ in range(r.randrange(0, PAIRS + 1)))
        if r.random() < 0.2:
            r.shuffle(frames)
        animated.append((r.randrange(0, TEXTURES), [(f, r.choice([r.randrange(0, 4), r.getrandbits(16)])) for f in frames]))
    textures = [(r.choice([0, 0x400, r.getrandbits(16)]), r.choice([0, 1, 2, 3, r.getrandbits(16)])) for _ in range(TEXTURES)]
    return texture_case(r.choice([r.uniform(-1, 14), float(r.randrange(0, 12)), T.rfloat(r), math.nan]), animated, textures,
                        count=len(animated) if r.random() < 0.9 else r.randrange(0, ANIMATED + 1))


# ------------------------------------------------------------------ node animations (AnimateNode)


NA_QUAT, NA_SCALE, NA_ENABLES, NA_PAIRS = 5, 5, 6, 4
KIND_BITS = (0x20000, 0x1000, 0x4000, 8, 0x80, 0x200, 1, 0x400, 0x10000)


def na_objs(objs, p, c, array=None, index=0):
    """The objects of one node animation `p` (spec c): the record, the key arrays, the enable list,
    the weights, its own shape and path, the morph, and the node it names with its parent. The record
    is the `index`-th of the object `array` when that is given (an animation's array of them)."""
    if array is None:
        array = p + ".na"
        objs[array] = Obj(0x40, -1)
    na = objs[array]
    base = 0x40 * index
    na.w(base + 4, "I", c["flags"]).w(base + 0x10, "HHHH", len(c["quat"]), len(c["scale"]), 0, len(c["enables"]))
    na.w(base + 0x18, "I", Ref(p + ".shape")).w(base + 0x1C, "I", Ref(p + ".quat")).w(base + 0x20, "I", Ref(p + ".scale"))
    na.w(base + 0x24, "I", Ref(p + ".path")).w(base + 0x28, "I", Ref(p + ".morph")).w(base + 0x30, "I", Ref(p + ".enables"))
    na.w(base + 0x34, "I", Ref(p + ".weights")).w(base + 0x3C, "I", Ref(p + ".node"))
    quat = Obj(NA_QUAT * 0x14, -1)
    for i, (frame, index, values) in enumerate(c["quat"]):
        quat.w(0x14 * i, "HH4f", frame, index, *values)
    objs[p + ".quat"] = quat
    scale = Obj(NA_SCALE * 0x10, -1)
    for i, (frame, extra, values) in enumerate(c["scale"]):
        scale.w(0x10 * i, "HH3f", frame, extra, *values)
    objs[p + ".scale"] = scale
    objs[p + ".enables"] = Obj(2 * NA_ENABLES, -1).w(0, "%dh" % len(c["enables"]), *c["enables"]) if c["enables"] else Obj(2 * NA_ENABLES, -1)
    objs[p + ".weights"] = Obj(24, -1).w(0, "24B", *c["weights"])
    shape = c["shape"]
    objs[p + ".shape"] = Obj(0x10, -1).w(0, "I", shape["flags"]).w(4, "HH", shape["points"], len(shape["pairs"])).w(
        8, "I", None).w(0xC, "I", Ref(p + ".shapewords"))
    words = Obj(4 * NA_PAIRS, -1)
    for i, (frame, value) in enumerate(shape["pairs"]):
        words.w(4 * i, "HH", frame, value)
    objs[p + ".shapewords"] = words
    objs[p + ".path"] = Obj(0x10, -1).w(0, "II", c["path"][0], c["path"][1]).w(0xC, "I", Ref(p + ".pathwords"))
    objs[p + ".pathwords"] = Obj(16, -1)
    objs[p + ".morph"] = Obj(0x30, -1)
    node = Obj(0xA0, -1)
    node.w(0, "I", c["node_flags"]).w(4, "I", Ref(p + ".parent") if c["parent"] is not None else None)
    node.w(8, "II", None, None).w(0x10, "16f", *c["matrix"]).w(0x50, "HH", 0, 0)
    objs[p + ".node"] = node
    if c["parent"] is not None:
        objs[p + ".parent"] = Obj(0xA0, -1).w(0, "I", c["parent"][0]).w(0x52, "H", c["parent"][1])


def header_shapes(objs, shapes):
    """The header's array of three shapes (flags and point count each), a place for each."""
    array = Obj(0x30, 0)
    for i, (flags, points) in enumerate(shapes):
        array.sub(0x10 * i, -1)
        array.w(0x10 * i, "I", flags).w(0x10 * i + 4, "HH", points, 0)
    objs["hshapes"] = array


def animate_node_objs(c):
    objs = {"control": Obj(0x38, 4).w(0x1C, "ff", c["length"], c["frame"]),
            "header": Obj(0xB8, 5).w(0x30, "I", c["header_flags"]).w(0xAC, "I", Ref("hshapes")),
            "animation": Obj(0x48, 6).w(8, "I", c["animation_f08"])}
    header_shapes(objs, c["shapes"])
    na_objs(objs, "a", c["na"])
    return objs


NODE_DUMP = [("a.node", 0, "I", 1), ("a.node", 0x10, "F", 16), ("header", 0x30, "I", 1)]


def animate_node_dump(side, result):
    return [T.s32(result)] + dump(side, NODE_DUMP)


def ppc_animate_node(e, case):
    return ppc_run(e, dict(case, objs=animate_node_objs(case)), ANIMATE_NODE,
                   lambda s: [s.addr["control"], s.addr["header"], s.addr["animation"], s.addr["a.node"], s.addr["a.na"]],
                   animate_node_dump, [function_range(e, ANIMATE_NODE)])


def host_animate_node(lib, case):
    return host_run(lib, dict(case, objs=animate_node_objs(case)), "tpw_rat_animate_node",
                    lambda s: [C.c_uint64(s.addr["control"]), C.c_uint64(s.addr["header"]), C.c_uint64(s.addr["animation"]),
                               C.c_uint64(s.addr["a.node"]), C.c_uint64(s.addr["a.na"])],
                    animate_node_dump)


def na_spec(flags=0, quat=(), scale=(), enables=(), weights=None, shape=None, path=(0, 10), node_flags=0, matrix=None,
            parent=None):
    return {"flags": flags, "quat": list(quat), "scale": list(scale), "enables": list(enables),
            "weights": list(weights) if weights is not None else [i * 11 % 256 for i in range(24)],
            "shape": shape or {"flags": 0, "points": 4, "pairs": []}, "path": path, "node_flags": node_flags,
            "matrix": matrix or [f32(0.25 * i - 1.0) for i in range(16)], "parent": parent}


def quat_key(frame, index=0xFFFF, values=(1.0, 0.5, 0.25, 0.125)):
    return (frame, index, [f32(v) for v in values])


def scale_key(frame, values=(1.0, 2.0, 3.0)):
    return (frame, 0x1234, [f32(v) for v in values])


def node_case(na, frame=3.5, length=10.0, header_flags=0, shapes=None, engine_flags=0, animation_f08=0x12345678,
              queues=None):
    return {"na": na, "frame": f32(frame), "length": f32(length), "header_flags": header_flags,
            "shapes": shapes or [(0, 8), (2, 9), (8, 10)], "animation_f08": animation_f08,
            "globals": (engine_flags, 0, 0), "queues": queues or {}}


def node_queues(r):
    """Answers for the fakes: nine axis words, a place, and points (floats of moderate size)."""
    def value():
        return fb(f32(r.choice([r.uniform(-10, 10), r.uniform(-1, 1), 0.0, 1.0, T.rfloat(r)])))
    return {"Axis": [value() for _ in range(9)], "Place": [fb(f32(r.choice([r.uniform(-50, 150), r.uniform(0, 100), 0.0, 1e30])))],
            "Point": [value() for _ in range(12)]}


def node_edges():
    keys = [quat_key(0, 0), quat_key(4, 1), quat_key(8, 0xFFFF), quat_key(12, 2)]
    scales = [scale_key(0), scale_key(5, (4.0, 5.0, 6.0)), scale_key(9, (7.0, 8.0, 9.0))]
    # past the length and not
    yield node_case(na_spec(flags=8, quat=keys), frame=10.5, length=10.0)
    yield node_case(na_spec(flags=8, quat=keys), frame=10.0, length=10.0)
    yield node_case(na_spec(flags=8, quat=keys), frame=math.nan, length=10.0)
    # the enable list
    for enables in ([], [5], [-5], [0], [-32768], [32767], [3, -6, 9], [-1, 4, 2, 7], [-32768, 3]):
        for frame in (0.0, 2.0, 3.0, 5.0, 6.0, 8.0, 100.0, 4294967295.0, math.nan):
            yield node_case(na_spec(flags=0x20000, enables=enables), frame=frame, length=1e10)
    # an enable entry of -32768 is a frame of 0xffff8000 (the negation wraps in 16 bits)
    for frame in (32767.0, 32768.0, 40000.0, 65535.0, 2147483648.0, 4294967040.0, 4294967295.0):
        yield node_case(na_spec(flags=0x20000, enables=[-32768]), frame=frame, length=1e10)
        yield node_case(na_spec(flags=0x20000, enables=[7, -32768, 32767]), frame=frame, length=1e10)
    # the morph
    for flags in (0x1000, 0x1000 | 0x4000, 0x4000):
        for header in (0, 4):
            yield node_case(na_spec(flags=flags), header_flags=header)
    # rotation: every segment of the weight table, the fraction near 1, without a table
    for frame in (0.0, 1.0, 3.99, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 11.0, 12.0, 20.0):
        for header in (0, 4):
            yield node_case(na_spec(flags=8, quat=keys), frame=frame, length=30.0, header_flags=header, queues={})
    for quat_frames in ((0, 4), (0, 100), (5, 6)):
        k = [quat_key(f, i % 2) for i, f in enumerate(quat_frames)]
        for frame in (0.0, 1.0, 50.0):
            yield node_case(na_spec(flags=8, quat=k), frame=frame, length=200.0)
    # scale
    for frame in (0.0, 2.5, 5.0, 8.0, 9.0, 20.0):
        for header in (0, 4):
            yield node_case(na_spec(flags=0x80, scale=scales), frame=frame, length=30.0, header_flags=header)
    # position along the node animation's own shape
    for flags in (0, 2, 8, 10, 1, 3):
        for header in (0, 4):
            for tangent in (0, 0x400):
                shape = {"flags": flags, "points": 12, "pairs": [(0, 1), (3, 2), (6, 3)]}
                yield node_case(na_spec(flags=1 | tangent, shape=shape), frame=4.0, header_flags=header,
                                queues={"Point": [fb(1.0), fb(2.0), fb(2.0), fb(0.5), fb(0.25), fb(0.125)] * 4})
    yield node_case(na_spec(flags=1 | 0x400 | 8, shape={"flags": 0, "points": 12, "pairs": [(0, 1)]}), frame=4.0)
    yield node_case(na_spec(flags=1, shape={"flags": 0, "points": 12, "pairs": [(5, 1)]}), frame=4.0)   # no pair yet
    # position along a path, under a moving parent
    parent = (8, 1)
    for start, count, frame in ((0, 10, 0.0), (0, 10, 10.0), (0, 10, 10.5), (4, 10, 3.5), (4, 10, 4.0), (4, 10, 14.0),
                                (4, 0, 4.0), (0xFFFFFFFF, 5, 1.0), (0x80000000, 100, 5.0)):
        for shape_flags in (0, 1, 2, 3, 8, 9):
            shapes = [(0, 8), (shape_flags, 31), (shape_flags, 10000)]
            for tangent in (0, 0x400):
                yield node_case(na_spec(flags=0x200 | tangent, path=(start, count), parent=parent), frame=frame,
                                length=1e10, shapes=shapes, header_flags=0,
                                queues={"Place": [fb(37.5)], "Point": [fb(3.0), fb(0.0), fb(4.0)]})
    yield node_case(na_spec(flags=0x200, path=(0, 10), parent=(0, 1)), frame=5.0)       # the parent not moving
    yield node_case(na_spec(flags=0x200, path=(0, 10), parent=None), frame=5.0)         # no parent
    yield node_case(na_spec(flags=0x200 | 1, path=(0, 10), parent=parent, shape={"flags": 0, "points": 4, "pairs": [(0, 0)]}), frame=5.0)
    # the tangent's length: zero, tiny, NaN
    for point in ((0.0, 0.0, 0.0), (1e-30, 0.0, 0.0), (math.nan, 1.0, 1.0), (1e30, 1e30, 1e30), (3.0, 4.0, 12.0)):
        yield node_case(na_spec(flags=1 | 0x400, shape={"flags": 0, "points": 12, "pairs": [(0, 1)]}), frame=4.0,
                        queues={"Point": [fb(v) for v in point]})
    # texture coordinates
    for engine in (0, 0x10):
        for node_flags in (0, 0x4000):
            for header in (0, 4):
                for flags in (0, 0x10000):
                    yield node_case(na_spec(flags=flags, node_flags=node_flags), header_flags=header, engine_flags=engine)


def node_random(r):
    length = r.choice([r.uniform(1, 60), 10.0, 30.0, 1e10])
    quat_frames = sorted(r.randrange(0, 40) for _ in range(r.randrange(0, NA_QUAT + 1)))
    quat = [quat_key(f, r.choice([0xFFFF, 0, 1, 2]), [r.uniform(-2, 2) for _ in range(4)]) for f in quat_frames]
    scale_frames = sorted(r.randrange(0, 40) for _ in range(r.randrange(0, NA_SCALE + 1)))
    scale = [scale_key(f, [r.uniform(-3, 3) for _ in range(3)]) for f in scale_frames]
    enables = [r.choice([r.randrange(-40, 40), 0, -32768, 32767, -1, 1]) for _ in range(r.randrange(0, NA_ENABLES + 1))]
    pair_frames = sorted(r.randrange(0, 40) for _ in range(r.randrange(0, NA_PAIRS + 1)))
    shape = {"flags": r.choice([0, 1, 2, 3, 8, 9, 10, 11, r.getrandbits(8)]),
             "points": r.choice([0, 1, 3, 4, 7, 12, 31, 100, 65535, r.randrange(0, 65536)]),
             "pairs": [(f, r.randrange(0, 100)) for f in pair_frames]}
    flags = 0
    for bit in KIND_BITS:
        if r.random() < 0.45:
            flags |= bit
    if r.random() < 0.15:
        flags |= r.getrandbits(32)
    parent = None if r.random() < 0.15 else (r.choice([0, 8, 8, 0xFFFFFFFF, r.getrandbits(32)]), r.randrange(0, 3))
    matrix = [f32(r.choice([r.uniform(-5, 5), 0.0, 1.0, T.rfloat(r)])) for _ in range(16)]
    frame = r.choice([r.uniform(-1, length + 2), float(r.randrange(0, 40)), r.uniform(0, 40), r.uniform(0, 12)])
    return node_case(
        na_spec(flags=flags, quat=quat, scale=scale, enables=enables, weights=[r.randrange(256) for _ in range(24)],
                shape=shape, path=(r.choice([0, 3, 4, 10, r.randrange(0, 50), r.getrandbits(32)]),
                                   r.choice([0, 5, 10, 50, r.randrange(0, 100)])),
                node_flags=r.choice([0, 0x4000, 0x10, r.getrandbits(32)]), matrix=matrix, parent=parent),
        frame=frame, length=length, header_flags=r.choice([0, 4, 4, r.getrandbits(32)]),
        shapes=[(r.choice([0, 1, 2, 3, 8, 9, r.getrandbits(4)]), r.choice([1, 4, 12, 100, r.randrange(0, 65536)]))
                for _ in range(3)],
        engine_flags=r.choice([0, 0, 0x10, r.getrandbits(32)]), animation_f08=r.getrandbits(32), queues=node_queues(r))


# ------------------------------------------------------------------ DoAnimateRideSet


ANIMS, GROUPS, ENTRIES = 3, 12, 3


def anim_objs(objs, name, spec):
    """One animation header with its node animations (an array of records) and animated textures."""
    anim = Obj(0x48, -1).w(0, "I", spec["flags"]).w(8, "I", spec["f08"])
    nas = spec["nas"]
    anim.w(0x12, "H", len(nas))
    if nas:
        objs[name + ".nas"] = Obj(0x40 * len(nas), 0)
        for k, na in enumerate(nas):
            objs[name + ".nas"].sub(0x40 * k, -1)
            na_objs(objs, "%s.n%d" % (name, k), na, array=name + ".nas", index=k)
        anim.w(0x2C, "I", Ref(name + ".nas"))
    animated = Obj(8 * ANIMATED, -1)
    for i, (index, pairs) in enumerate(spec["textures"]):
        pair_obj = Obj(4 * PAIRS, -1)
        for k, (frame, value) in enumerate(pairs):
            pair_obj.w(4 * k, "HH", frame, value)
        objs["%s.pairs%d" % (name, i)] = pair_obj
        animated.w(8 * i, "HH", index, len(pairs)).w(8 * i + 4, "I", Ref("%s.pairs%d" % (name, i)))
    objs[name + ".animated"] = animated
    anim.w(0x18, "H", len(spec["textures"])).w(0x30, "I", Ref(name + ".animated"))
    objs[name] = anim


def do_animate_objs(c):
    textures = Obj(8 * TEXTURES, -1)
    for i, (flags, frame) in enumerate(c["textures"]):
        textures.w(8 * i, "HH", flags, frame)
    objs = {"control": Obj(0x38, 4).w(0, "14I", *c["control"]),
            "header": Obj(0xB8, 5).w(0x30, "I", c["header_flags"]).w(0x50, "I", Ref("textures")).w(0xAC, "I", Ref("hshapes")),
            "textures": textures}
    header_shapes(objs, [(0, 1)] * 3)
    for i, spec in enumerate(c["anims"]):
        anim_objs(objs, "anim%d" % i, spec)
    return objs


def do_animate_dump_items(c):
    items = control_dump("control") + [("header", 0x30, "I", 1), ("textures", 0, "H", 2 * TEXTURES)]
    for i, spec in enumerate(c["anims"]):
        for k in range(len(spec["nas"])):
            items += [("anim%d.n%d.node" % (i, k), 0, "I", 1), ("anim%d.n%d.node" % (i, k), 0x10, "F", 16)]
    return items


def do_animate_numbers(c, side):
    """The list the host reads (see tpw_rat_do_animate)."""
    out = [c["a"], int(c["present"]), c["set_flags"], c["fc0"], c["fc4"], c["mesh_flags"], int(c["engine"]),
           side.absolute("header"), side.absolute("control")]
    for count, entries in c["groups"]:
        out.append(count)
        out += [side.absolute("anim%d" % a) if a is not None else 0 for a in entries]
    return out


def ppc_do_animate(e, case):
    objs = do_animate_objs(case)
    holder = {}

    def args(s):
        set_ = e.alloc(0xFC, b"\xa5" * 0xFC)
        mesh = e.alloc(0x40, b"\xa5" * 0x40)
        engine = e.alloc(0x15C)
        holder.update(set=set_, mesh=mesh)
        s.log.labels.update({set_: 10, mesh: 20, engine: 60})
        e.engine_object, e.engine_link = engine, engine + 0x154
        ws(e, engine, engine + 0x154)
        ws(e, set_ + 4, case["set_flags"], mesh)
        ws(e, set_ + 0xC0, case["fc0"], case["fc4"])
        ws(e, mesh, case["mesh_flags"], s.addr["header"])
        ws(e, mesh + 0x1C, engine if case["engine"] else 0)
        for k, (count, entries) in enumerate(case["groups"]):
            block = e.alloc(8 * ENTRIES, b"\xa5" * (8 * ENTRIES))
            ws(e, set_ + 0x18 + 12 * k, 0xA5A5A5A5, count, block)
            for j, a in enumerate(entries):
                ws(e, block + 8 * j, j, s.addr["anim%d" % a] if a is not None else 0)
        return [set_ if case["present"] else 0, s.addr["control"], case["a"] & MASK]

    def results(s, r3):
        return [e.u32(holder["set"] + 4), e.u32(holder["mesh"])] + dump(s, do_animate_dump_items(case))

    return ppc_run(e, dict(case, objs=objs), DO_ANIMATE, args, results, [function_range(e, ANIMATE_NODE)])


def host_do_animate(lib, case):
    holder = {}

    def args(s):
        words = do_animate_numbers(case, s)
        holder["words"] = (C.c_int64 * len(words))(*words)
        holder["out"] = (C.c_int64 * 8)()
        return [holder["words"], holder["out"]]

    def results(s, r):
        return [holder["out"][0] & MASK, holder["out"][1] & MASK] + dump(s, do_animate_dump_items(case))

    lib.tpw_rat_do_animate.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_rat_do_animate.restype = C.c_int32
    return host_run(lib, dict(case, objs=do_animate_objs(case)), "tpw_rat_do_animate", args, results)


def anim_spec(r, nas=None):
    """A random animation header with up to two node animations."""
    count = r.choice([0, 1, 1, 2]) if nas is None else nas
    specs = []
    for _ in range(count):
        flags = r.choice([0, 0, 0x10000, 0x10000, 0x20000, 0x1000, 8, 0x80, 0x10000 | 8 | 0x20000, r.getrandbits(32)])
        quat = [quat_key(f, r.choice([0xFFFF, 0, 1]), [r.uniform(-2, 2) for _ in range(4)])
                for f in sorted(r.randrange(0, 20) for _ in range(r.randrange(0, 4)))]
        scale = [scale_key(f, [r.uniform(-3, 3) for _ in range(3)]) for f in sorted(r.randrange(0, 20) for _ in range(r.randrange(0, 4)))]
        specs.append(na_spec(flags=flags, quat=quat, scale=scale, enables=[r.randrange(-20, 20) for _ in range(r.randrange(0, 4))],
                             node_flags=r.choice([0, 0x4000, 0x10]),
                             matrix=[f32(r.uniform(-2, 2)) for _ in range(16)]))
    textures = []
    for _ in range(r.choice([0, 0, 1, 2])):
        frames = sorted(r.randrange(0, 12) for _ in range(r.randrange(0, PAIRS + 1)))
        textures.append((r.randrange(0, TEXTURES), [(f, r.randrange(0, 4)) for f in frames]))
    return {"flags": r.choice([0, 2, 2, 3, r.getrandbits(32)]), "f08": r.getrandbits(32), "nas": specs, "textures": textures}


def do_animate_case(control, groups=None, anims=None, a=1, present=True, set_flags=0, fc0=0x00010002, fc4=0x00030004,
                    mesh_flags=1, engine=True, header_flags=0, engine_flags=0, clock=1000, fixed=2000, queues=None,
                    textures=None):
    anims = anims if anims is not None else []
    if groups is None:
        groups = [(1, [0 if anims else None, None, None]) for _ in range(GROUPS)]
    return {"a": a, "present": present, "set_flags": set_flags & MASK, "fc0": fc0, "fc4": fc4, "mesh_flags": mesh_flags,
            "engine": engine, "control": control, "groups": groups, "anims": anims, "header_flags": header_flags,
            "textures": textures or [(0x100, 5), (0x200, 6), (0x300, 7)], "globals": (engine_flags, clock, fixed),
            "queues": queues or {}}


def basic_anim(textures=True, flags_extra=0, nas=1):
    pairs = [(0, 1), (3, 2), (6, 3)]
    return {"flags": (2 if textures else 0) | flags_extra, "f08": 0x1234,
            "nas": [na_spec(flags=0x10000 | 0x20000, enables=[3, -6], node_flags=0) for _ in range(nas)],
            "textures": [(0, pairs), (1, pairs)] if textures else []}


def animate_edges():
    anim = basic_anim()
    plain = control_words(kind=1, start=0, rate=1.0, length=100.0)             # time 1000: 30 frames, not past 100
    short = control_words(kind=1, start=0, rate=1.0, length=10.0)              # past its length
    idle = control_words(flags=0, kind=0, start=0, rate=1.0, length=10.0)
    own = control_words(flags=1, kind=1, index=2, start=0, rate=1.0, length=10.0, speed=0.0)
    own_speed = control_words(flags=1, kind=1, index=2, start=0, rate=1.0, length=10.0, speed=2.5)
    queued = control_words(kind=1, start=0, rate=1.0, length=10.0, queued_kind=3, queued_index=2, queued_flags=0x30, speed=1.5)
    for present in (True, False):
        yield do_animate_case(plain, anims=[anim], present=present)
    yield do_animate_case(control_words(kind=0xC), anims=[anim])
    # a held set
    for set_flags in (0x8000, 0x8004):
        for count, entry in ((0, 0), (1, 0), (1, None), (2, 1)):
            groups = [(count, [entry, 0, None]) for _ in range(GROUPS)]
            yield do_animate_case(plain, groups=groups, anims=[anim, basic_anim(False)], set_flags=set_flags)
    # a control that is not past its length, with each way the time is counted
    for flags in (0, 2, 4, 6, 0x40, 0x42, 0x44, 0x46, 0x10, 0x14):
        yield do_animate_case(control_words(flags=flags, kind=1, start=0, rate=1.0, length=100.0), anims=[anim, anim])
        yield do_animate_case(control_words(flags=flags, kind=1, start=0, rate=1.0, length=100.0), anims=[anim], a=0)
    # past its length: each set flag, each way the control restarts
    for set_flags in (0, 0x10, 0x8, 0x18, 0x4008, 0x4018, 0x400000, 0x20, 0x40000000):
        for control in (short, idle, own, own_speed, queued):
            for a in (0, 1):
                yield do_animate_case(control, anims=[anim, basic_anim(False)], set_flags=set_flags, a=a)
    for mesh_flags in (0, 1):
        yield do_animate_case(short, anims=[anim], set_flags=0x8, mesh_flags=mesh_flags)
        yield do_animate_case(short, anims=[anim], set_flags=0x4008, mesh_flags=mesh_flags, engine=False)
    # SetNewAnim changing the control
    for queues in ({"NewKind": [0xC]}, {"NewKind": [5], "NewIndex": [1]}, {"NewLength": [fb(1000.0)]},
                   {"NewLength": [fb(0.0)], "NewStart": [500]}, {"FlagsOr": [0x2], "FlagsAnd": [0xFFFFFFFE]},
                   {"FlagsOr": [0x4]}, {"FlagsOr": [0x8]}, {"FlagsOr": [0x10]}, {"FlagsOr": [0x6]}):
        for control in (short, own, queued):
            yield do_animate_case(control, anims=[anim], queues=queues)
        yield do_animate_case(short, anims=[anim], set_flags=0x10, queues=queues)
    # the nodes are animated: each way the test goes
    for set_flags in (0, 0x400000):
        for flags in (0, 4, 0x10, 0x14, 2, 0x40):
            for a in (0, 1, -1):
                yield do_animate_case(control_words(flags=flags, kind=1, start=0, rate=1.0, length=100.0),
                                      anims=[anim], set_flags=set_flags, a=a)
    # the engine flag that stops texture animation, an animation without texture animation or nodes
    for engine_flags in (0, 8, 0x10):
        yield do_animate_case(plain, anims=[anim], engine_flags=engine_flags)
    yield do_animate_case(plain, anims=[basic_anim(False)])
    yield do_animate_case(plain, anims=[{"flags": 2, "f08": 1, "nas": [], "textures": [(0, [(0, 9)])]}])
    yield do_animate_case(control_words(kind=7, index=2), anims=[anim], groups=[(0, [0, 0, 0])] * GROUPS)


def animate_random(r):
    anims = [anim_spec(r) for _ in range(r.choice([0, 1, 2, 3]))]
    groups = []
    for _ in range(GROUPS):
        count = r.choice([0, 1, 1, 2, 3])
        groups.append((count, [(r.randrange(len(anims)) if anims and r.random() < 0.8 else None) for _ in range(ENTRIES)]))
    length = r.choice([5.0, 10.0, 30.0, 100.0, r.uniform(0, 50), math.nan])
    control = control_words(
        flags=r.choice([0, 0, 1, 2, 4, 6, 0x10, 0x14, 0x40, 0x41, 0x42, 0x44, 0x5F, r.getrandbits(32)]),
        kind=r.choice([0, 1, 2, 3, 5, 11, 0xC, 1, 1, 2]), index=r.randrange(0, ENTRIES),
        rate=r.choice([1.0, 0.5, 2.0, r.uniform(0.1, 3), 0.0, -1.0]),
        start=r.choice([0, 500, 999, 1000, 1001, 2000, r.getrandbits(32)]), current=r.getrandbits(32), held=r.getrandbits(32),
        length=length, frame=T.rfloat(r), queued_kind=r.choice([0xC, 0xC, 0xC, 0, 1, 2, 5]), queued_index=r.randrange(ENTRIES),
        queued_flags=r.choice([0, 1, 4, 0x30, r.getrandbits(32)]), speed=r.choice([0.0, 0.0, 1.0, -0.0, 2.5, T.rfloat(r)]))
    queues = {}
    if r.random() < 0.6:
        queues["NewKind"] = [r.choice([0xC, 0, 1, 2, 3, 5]) for _ in range(2)]
    if r.random() < 0.4:
        queues["NewIndex"] = [r.randrange(ENTRIES) for _ in range(2)]
    if r.random() < 0.5:
        queues["FlagsAnd"] = [r.choice([0xFFFFFFFF, 0xFFFFFFF7, 0xFFFFFFEF, r.getrandbits(32)]) for _ in range(2)]
        queues["FlagsOr"] = [r.choice([0, 2, 4, 8, 0x10, r.getrandbits(32)]) for _ in range(2)]
    if r.random() < 0.5:
        queues["NewLength"] = [fb(f32(r.choice([0.0, 1.0, 10.0, 1000.0, r.uniform(0, 50)]))) for _ in range(2)]
        queues["NewStart"] = [r.choice([0, 100, 1000, r.getrandbits(32)]) for _ in range(2)]
    queues.update(node_queues(r))
    return do_animate_case(
        control, groups=groups, anims=anims, a=r.choice([0, 1, 1, -1, 5]), present=r.random() < 0.95,
        set_flags=r.choice([0, 0, 0x8, 0x10, 0x18, 0x4000, 0x4008, 0x8000, 0x8004, 0x400000, 0x40000000, 0x20, r.getrandbits(32)]),
        fc0=r.getrandbits(32), fc4=r.getrandbits(32), mesh_flags=r.choice([0, 1, 1, r.getrandbits(32)]),
        engine=r.random() < 0.8, header_flags=r.choice([0, 4, r.getrandbits(32)]), engine_flags=r.choice([0, 0, 8, 0x10, r.getrandbits(32)]),
        clock=r.choice([0, 1000, 2000, 1500, r.getrandbits(32)]), fixed=r.choice([0, 1000, 5000, r.getrandbits(32)]),
        queues=queues, textures=[(r.choice([0, 0x400]), r.randrange(0, 4)) for _ in range(TEXTURES)])


# ------------------------------------------------------------------ the embedded data


EMBEDDED, EMBEDDED_NODES, FACES, VERTEX_WORDS, POINTS3D = 4, 4, 4, 8, 4


def embedded_objs(c):
    """The header with its nodes, embedded entries (and their morph records) and the geometry the
    nodes' morphs read: faces, vertex words, vertex sets and points shared by every node."""
    objs = {}
    header = Obj(0xB8, 5).w(0x46, "H", c["f46"]).w(0x7C, "I", Ref("entries")).w(0x78, "I", Ref("n0"))
    objs["header"] = header
    entries = Obj(0x14 * EMBEDDED, 0)
    for i, entry in enumerate(c["entries"]):
        entries.w(0x14 * i, "I", entry["flags"]).w(0x14 * i + 0xC, "I", Ref("morph%d" % i))
        morph = entry["morph"]
        objs["morph%d" % i] = Obj(0x10, -1).w(0, "HH", morph["face"], morph["back"]).w(4, "3f", *morph["weights"])
    objs["entries"] = entries
    faces = Obj(8 * FACES, -1)
    for i, (index_flag, corners) in enumerate(c["faces"]):
        faces.w(8 * i, "HHHH", index_flag, *corners)
    objs["faces"] = faces
    objs["vwords"] = Obj(2 * VERTEX_WORDS, -1).w(0, "%dH" % VERTEX_WORDS, *c["vwords"])
    objs["verts"] = Obj(0x60, -1).w(0, "24f", *c["verts"])
    objs["points"] = Obj(12 * POINTS3D, -1).w(0, "12f", *c["points"])
    for i, node in enumerate(c["nodes"]):
        n = Obj(0xA0, -1)
        n.w(0, "I", node["flags"]).w(4, "I", Ref("n%d" % node["parent"]) if node["parent"] is not None else None)
        n.w(8, "I", Ref("n%d" % node["sibling"]) if node["sibling"] is not None else None)
        n.w(0xC, "I", Ref("n%d" % node["child"]) if node["child"] is not None else None)
        n.w(0x10, "16f", *node["matrix"]).w(0x50, "HH", node["f50"], 0)
        n.w(0x60, "I", Ref("verts")).w(0x64, "I", Ref("points")).w(0x70, "I", Ref("faces")).w(0x94, "I", Ref("vwords"))
        objs["n%d" % i] = n
    return objs


def embedded_dump_items(c):
    items = []
    for i in range(len(c["nodes"])):
        items += [("n%d" % i, 0, "I", 1), ("n%d" % i, 0x10, "F", 16)]
    return items


def embedded_record_words(c):
    out = []
    for flags, f08, f0c, f10 in c["records"]:
        out += [flags, f08, f0c, f10]
    return out


def ppc_records(e, c):
    """The embedded set and its four records with their blocks, as the Mac lays them out."""
    set_ = e.alloc(0x14, b"\xa5" * 0x14)
    records = e.alloc(0x14 * EMBEDDED, b"\xa5" * (0x14 * EMBEDDED))
    blocks = []
    for i, (flags, f08, f0c, f10) in enumerate(c["records"]):
        block = e.alloc(0x40, b"\xa5" * 0x40)
        blocks.append(block)
        ws(e, records + 0x14 * i, flags, block, f08, f0c, f10)
    ws(e, set_, c["set_flags"], records)
    return set_, records, blocks


def ppc_record_dump(e, records, blocks):
    out = []
    for i, block in enumerate(blocks):
        out += [e.u32(records + 0x14 * i + off) for off in (0, 8, 0xC, 0x10)]
        out += [canon(w) for w in words_at(e, block, 16)]
    return out


def host_record_dump(raw):
    out = []
    for i in range(EMBEDDED):
        chunk = [x & MASK for x in raw[20 * i:20 * i + 20]]
        out += chunk[:4] + [canon(w) for w in chunk[4:]]
    return out


def ppc_process(e, case):
    holder = {}

    def args(s):
        set_, records, blocks = ppc_records(e, case)
        mesh = e.alloc(0x40, b"\xa5" * 0x40)
        holder.update(records=records, blocks=blocks)
        s.log.labels[mesh] = 20
        ws(e, mesh, case["mesh_flags"], s.addr["header"])
        ws(e, mesh + 0x28, set_ if case["has_set"] else 0)
        return [mesh]

    def results(s, r3):
        return ppc_record_dump(e, holder["records"], holder["blocks"]) + dump(s, embedded_dump_items(case))

    return ppc_run(e, dict(case, objs=embedded_objs(case)), PROCESS, args, results,
                   [function_range(e, DO_WORLD), function_range(e, COMBINE_MATRICES)])


def host_process(lib, case):
    holder = {}

    def args(s):
        words = [case["mesh_flags"], int(case["has_set"]), case["set_flags"], s.absolute("header"), EMBEDDED] + \
            embedded_record_words(case)
        holder["words"] = (C.c_int64 * len(words))(*words)
        holder["out"] = (C.c_int64 * 128)()
        return [holder["words"], holder["out"]]

    def results(s, r):
        return host_record_dump(list(holder["out"])) + dump(s, embedded_dump_items(case))

    lib.tpw_rat_process_embedded.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_rat_process_embedded.restype = C.c_int32
    return host_run(lib, dict(case, objs=embedded_objs(case)), "tpw_rat_process_embedded", args, results)


def ppc_do_world(e, case):
    holder = {}

    def args(s):
        set_, records, blocks = ppc_records(e, case)
        world = e.alloc(0x40, struct.pack(">16I", *case["world"]))
        holder.update(records=records, blocks=blocks)
        return [s.addr["n0"], s.addr["header"], T.SVal("matrix", "16I", case["matrix"]), world, records]

    def results(s, r3):
        return ppc_record_dump(e, holder["records"], holder["blocks"]) + dump(s, embedded_dump_items(case))

    return ppc_run(e, dict(case, objs=embedded_objs(case)), DO_WORLD, args, results,
                   [function_range(e, DO_WORLD), function_range(e, COMBINE_MATRICES)])


def host_do_world(lib, case):
    holder = {}

    def args(s):
        words = [s.absolute("n0"), s.absolute("header")] + case["matrix"] + case["world"] + embedded_record_words(case)
        holder["words"] = (C.c_int64 * len(words))(*words)
        holder["out"] = (C.c_int64 * 128)()
        return [holder["words"], holder["out"]]

    def results(s, r):
        return host_record_dump(list(holder["out"])) + dump(s, embedded_dump_items(case))

    lib.tpw_rat_do_world.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_rat_do_world.restype = C.c_int32
    return host_run(lib, dict(case, objs=embedded_objs(case)), "tpw_rat_do_world", args, results)


def matrix_words(values):
    return [fb(f32(v)) for v in values]


def embedded_case(r=None, nodes=None, entries=None, records=None, f46=10, matrix=None, world=None, mesh_flags=1,
                  has_set=True, set_flags=1, queues=None):
    """A case from parts; the parts not given are made from `r` (a random generator) or are plain."""
    r = r or random.Random(5)
    plain_matrix = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 2.0, 3.0, 1.0]
    return {"nodes": nodes, "entries": entries, "records": records, "f46": f46, "mesh_flags": mesh_flags,
            "has_set": has_set, "set_flags": set_flags, "matrix": matrix_words(matrix or plain_matrix),
            "world": matrix_words(world or [0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 2.0, 0.0, 5.0, 6.0, 7.0, 1.0]),
            "faces": [((i % POINTS3D) << 1 | (i & 1), [(3 * i) % VERTEX_WORDS, (3 * i + 1) % VERTEX_WORDS, (3 * i + 2) % VERTEX_WORDS])
                      for i in range(FACES)],
            "vwords": [(5 * i) % VERTEX_WORDS for i in range(VERTEX_WORDS)],
            "verts": [f32(0.5 * i - 3.0) for i in range(24)], "points": [f32(0.25 * i + 0.5) for i in range(12)],
            "globals": (0, 0, 0), "queues": queues or {"Orient": [fb(f32(0.5 + i)) for i in range(9)]}}


def node_spec(flags=0, f50=0, parent=None, sibling=None, child=None, matrix=None):
    return {"flags": flags, "f50": f50, "parent": parent, "sibling": sibling, "child": child,
            "matrix": matrix or [f32(0.5 * i - 2.0) for i in range(16)]}


def entry_spec(flags=0, face=0, weights=(0.25, 0.5, 0.125)):
    return {"flags": flags, "morph": {"face": face, "back": 7, "weights": [f32(w) for w in weights]}}


def chain(flags_list, f46=10):
    """A root with a child with a child...: node i flagged flags_list[i] and numbered f46 + i."""
    n = len(flags_list)
    return [node_spec(flags=flags_list[i], f50=f46 + min(i, EMBEDDED - 1), parent=i - 1 if i else None,
                      child=i + 1 if i + 1 < n else None) for i in range(n)]


def plain_entries(flags=0):
    return [entry_spec(flags=flags) for _ in range(EMBEDDED)]


def plain_records(flags=0):
    return [(flags, 0xA5A5A5A5, 0xFFFFFFFF, 0x77) for _ in range(EMBEDDED)]


def embedded_edges():
    EM, SKIP, NORM = 0x20000000, 0x40000000, 0x10000000 | 0x10000
    GEOM = 0x200000
    # TPProcessEmbeddedData: each test, and a hierarchy
    for mesh_flags in (0, 1, 0xFFFFFFFE, 0xFFFFFFFF):
        for set_flags in (0, 1, 2, 0xFFFFFFFE, 0xFFFFFFFF):
            yield ("process", embedded_case(nodes=chain([0, EM, 0]), entries=plain_entries(), records=plain_records(),
                                            mesh_flags=mesh_flags, set_flags=set_flags))
    # a plain walk, with the matrices combining down a chain
    yield ("process", embedded_case(nodes=chain([0, 0, 0, 0]), entries=plain_entries(), records=plain_records()))
    yield ("world", embedded_case(nodes=chain([0, 0, 0, 0]), entries=plain_entries(), records=plain_records()))
    # nodes with embedded records: each record flag state
    for record_flags in (0, 0x20, 0x2, 0x22, 0x4, 0x24, 0x8, 0x28, 0xE, 0x2E, 0x1, 0x21, 0x10, 0x30, 0xFFFFFFFF):
        for entry_flags in (0, 0x40, 0x40000, 0x40040, 0x8, 0x30):
            yield ("world", embedded_case(nodes=chain([GEOM, EM, EM]), entries=plain_entries(entry_flags),
                                          records=plain_records(record_flags), f46=10))
    # the morph: every node under a parent with geometry or not
    for parent_flags in (GEOM, 0, NORM | GEOM, NORM):
        for entry_flags in (0x40, 0x40000, 0x40040, 0):
            nodes = [node_spec(flags=parent_flags, f50=10, child=1), node_spec(flags=EM, f50=11, parent=0)]
            yield ("world", embedded_case(nodes=nodes, entries=plain_entries(entry_flags),
                                          records=plain_records(0)))
    # faces other than the first, weights at the ends
    for face in range(FACES):
        for weights in ((0.0, 0.0, 0.0), (1.0, 1.0, 1.0), (0.5, 0.25, -1.0), (math.nan, 0.5, 0.5), (1e30, 1.0, 1.0)):
            nodes = [node_spec(flags=GEOM, f50=0, child=1), node_spec(flags=EM, f50=11, parent=0)]
            entries = [entry_spec(0x40040, face=face, weights=weights) for _ in range(EMBEDDED)]
            yield ("world", embedded_case(nodes=nodes, entries=entries, records=plain_records(0)))
    # the normals and the skipped children
    for flags in (NORM, 0x10000000, 0x10000, SKIP, SKIP | EM, SKIP | NORM):
        yield ("world", embedded_case(nodes=chain([NORM, flags, 0]), entries=plain_entries(), records=plain_records()))


def embedded_random(r):
    n = r.randrange(1, EMBEDDED_NODES + 1)
    parents = [None] + [r.randrange(i) for i in range(1, n)]
    f46 = r.choice([0, 1, 10, 100, 60000])
    entries = [entry_spec(flags=r.choice([0, 0x40, 0x40000, 0x40040, 0x40040, 0x8, 0x30, r.getrandbits(32)]),
                          face=r.randrange(FACES), weights=[r.uniform(-1, 2) if r.random() < 0.85 else r.choice([0.0, 1.0, 0.5]) for _ in range(3)])
               for _ in range(EMBEDDED)]
    nodes = []
    for i in range(n):
        flags = 0
        for bit, p in ((0x20000000, 0.6), (0x10000000, 0.4), (0x10000, 0.5), (0x40000000, 0.15), (0x200000, 0.5)):
            if r.random() < p:
                flags |= bit
        if r.random() < 0.1:
            flags |= r.getrandbits(32) & 0x0FEFFFFF
        index = r.randrange(EMBEDDED)
        if flags & 0x20000000 and parents[i] is None and entries[index]["flags"] & 0x40040:
            flags &= ~0x20000000      # the original takes the parent without looking
        nodes.append(node_spec(flags=flags, f50=(f46 + index) & 0xFFFF if flags & 0x20000000 else r.getrandbits(16),
                               parent=parents[i], matrix=[f32(r.choice([r.uniform(-2, 2), 0.0, 1.0, T.rfloat(r)])) for _ in range(16)]))
    for i in range(n):          # the links: the first child and the next sibling
        kids = [j for j in range(n) if parents[j] == i]
        nodes[i]["child"] = kids[0] if kids else None
        for a, b in zip(kids, kids[1:]):
            nodes[a]["sibling"] = b
    records = [(r.choice([0, 0x20, 0x22, 0x2, 0xE, 0x1, 0x21, r.getrandbits(32)]), r.getrandbits(32), r.getrandbits(32), r.getrandbits(32))
               for _ in range(EMBEDDED)]
    case = embedded_case(r, nodes=nodes, entries=entries, records=records, f46=f46,
                         matrix=[r.choice([r.uniform(-2, 2), 0.0, 1.0]) for _ in range(16)],
                         world=[r.choice([r.uniform(-2, 2), 0.0, 1.0]) for _ in range(16)],
                         mesh_flags=r.choice([1, 1, 1, 0, r.getrandbits(32)]), has_set=True,
                         set_flags=r.choice([1, 1, 0, r.getrandbits(32)]),
                         queues={"Orient": [fb(f32(r.uniform(-2, 2))) for _ in range(9)]})
    case["faces"] = [(((r.randrange(POINTS3D)) << 1) | r.randrange(2), [r.randrange(VERTEX_WORDS) for _ in range(3)])
                     for _ in range(FACES)]
    case["vwords"] = [r.randrange(VERTEX_WORDS) for _ in range(VERTEX_WORDS)]
    case["verts"] = [f32(r.uniform(-5, 5)) for _ in range(24)]
    case["points"] = [f32(r.uniform(-1, 1)) for _ in range(12)]
    return ("process" if r.random() < 0.5 else "world", case)


def run_embedded(kind):
    def ppc(e, case):
        which, c = case
        return (ppc_process if which == "process" else ppc_do_world)(e, c) if which == kind else {"skipped": 1}

    def host(lib, case):
        which, c = case
        return (host_process if which == "process" else host_do_world)(lib, c) if which == kind else {"skipped": 1}

    def edges():
        return [x for x in embedded_edges() if x[0] == kind]

    def random_cases(r):
        case = embedded_random(r)
        return (kind, case[1])

    return ppc, host, edges, random_cases


# ------------------------------------------------------------------ the tests


TESTS = [
    FnTest("IsMeshDrawEnabled", ppc_is_draw, host_is_draw, edge=is_draw_edges, rnd=lambda r: r.getrandbits(32)),
    FnTest("GetAnimFrameData", ppc_frame, host_frame, edge=frame_edges, rnd=frame_random),
    FnTest("TickRideAnimSet", ppc_tick_set, host_tick_set, edge=tick_edges, rnd=tick_random),
    FnTest("RAAnimateTextureFrames", ppc_textures, host_textures, edge=texture_edges, rnd=texture_random),
    FnTest("AnimateNode", ppc_animate_node, host_animate_node, edge=node_edges, rnd=node_random),
    FnTest("DoAnimateRideSet", ppc_do_animate, host_do_animate, edge=animate_edges, rnd=animate_random),
    FnTest("TPProcessEmbeddedData", *[f for f in run_embedded("process")][:2], edge=run_embedded("process")[2],
           rnd=run_embedded("process")[3]),
    FnTest("DoWorldTransformsForEmbeddedData", *[f for f in run_embedded("world")][:2], edge=run_embedded("world")[2],
           rnd=run_embedded("world")[3]),
]
