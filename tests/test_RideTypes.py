"""Differential test for src/game/rides/TPRide.cpp (LoadRideType, InstanceRide, DestroyRideInstance,
DestroyAllRideTypes) and src/game/rides/RideAnim.cpp (AnimateRideSet, TickRideAnims), against
libtpw_unit_RideTypes.dylib.

Every function the six call is a fake on both sides that records its name, its arguments and
the contents of the records it is handed (tests/unit_RideTypes.cpp is the host's half, the
Stubs below the PowerPC's); the calls are compared in order, then what the function left in
the world. A case builds a world of rides from a list of numbers, the same list on both sides:
the PowerPC side lays the records out as the Mac does, the host side as the translation does.

A ride is 31 numbers (ride_words): its set (flags, map words, width, depth, the word at
+0xcc, animations, time controls), its mesh (words at 0 and 0x10, whether it has embedded
data, a root node, an engine mesh), its header (flags, node and mini node counts and flags).
Everything the functions are not meant to read is 0xa5 on both sides.

What the original reads before it writes it, and the translation reads as 0: InstanceRide's
cell (when no height field corrects it: the test clears the stack below the call) and the
height from f31 (flag 8 without a placement; the test sets f31 to 0). A failed
LoadRideAnimSet leaves no set in the original's stack slot, which it then writes through; the
fake writes the set anyway so that both sides do the same.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_RideTypes"

MASK = 0xFFFFFFFF
TICK, ANIMATE = 0x100C6964, 0x100C74D4
LOAD, INSTANCE, DESTROY_INSTANCE, DESTROY_TYPES = 0x10068FD0, 0x10069B24, 0x1006A3EC, 0x1006A9F0

TPGLOBALS = 0x103B74C8       # +0x4010, +0x4014, +0x4018: the animation clock
CLOCK = 0x1037F048
GAME = 0x1037F134
DATABASE = 0x1037F6C8
FIELD_HEADER = 0x102C6C7C
SETS = 0x102C79A4            # instance n at +4n
FIRST_FREE, COUNT_A, COUNT_B = 0x102D19A4, 0x102D19A8, 0x102D19AC
TYPES = 0x102D19B0           # type n at +4n
TYPE_COUNT = 0x102D1ED8
HIGHLIGHTED, POTENTIAL = 0x102C6CE8, 0x102D1F08
GAME_OPTIONS = 0x102B2B48
CALLBACK = 0x102660F0        # the TVector of AnimateRideCallback

RIDES, NODES, MINIS, POINTS, WINDOW, CONTROLS, GROUPS, ENTRIES = 6, 3, 2, 8, 40, 3, 12, 3
SET_LABEL, MESH_LABEL, HEADER_LABEL, ROOT_LABEL, SPECIAL_LABEL, ENGINE_LABEL = 10, 20, 30, 40, 50, 60
TYPE_LABEL, FIELD_LABEL, CLOCK_LABEL, GAME_LABEL, SIZE_LABEL, OBJECT_LABEL, DATABASE_LABEL = 80, 90, 4, 6, 7, 8, 9
NODE_LABEL, ANIM_LABEL = 100, 200

RIDE_NAMES = ("set_flags", "fc0", "fc4", "ff0", "ff4", "fcc", "mesh_f00", "mesh_f10", "embedded", "root", "vertices",
              "engine", "header_flags", "nodes", "node0", "node1", "node2", "all_nodes", "mini0", "mini1",
              "animations", "time_count")


def ws(e, address, *words):
    """Writes 32-bit words, big-endian, to the emulator's memory."""
    e.write(address, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def fb(x):
    """The bit pattern of a single-precision value."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bf(bits):
    """The single-precision value of a bit pattern."""
    return struct.unpack(">f", struct.pack(">I", bits & MASK))[0]


def ride_words(controls=((0, 0, 0), (0, 0, 0), (0, 0, 0)), **fields):
    """The 31 numbers of a ride; fields not given are 0."""
    unknown = set(fields) - set(RIDE_NAMES)
    assert not unknown, unknown
    words = [fields.get(name, 0) for name in RIDE_NAMES]
    for control in controls:
        words += list(control)
    return words


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


class PpcRide:
    """Ride i in the emulator's memory, laid out as the Mac does (offsets as in the original)."""

    def __init__(self, e, i, words, fixed):
        f = dict(zip(RIDE_NAMES, words))
        controls = words[len(RIDE_NAMES):]

        def poison(size):
            return e.alloc(size, b"\xa5" * size)

        self.e = e
        self.set = poison(0xFC)
        self.mesh = poison(0x40)
        self.header = poison(0xB8)
        self.nodes = poison(NODES * 0xA0)
        self.minis = poison(MINIS * 0x58)
        self.root = poison(0xA0)
        self.special = poison(0xA0)
        self.points = e.alloc(POINTS * 12)
        self.engine = poison(0x15C)
        self.controls = poison(CONTROLS * 0x38)
        self.entries = e.alloc(GROUPS * ENTRIES * 8)
        self.anims = [[e.alloc(0x48) for _ in range(ENTRIES)] for _ in range(GROUPS)]
        self.embedded = e.alloc(16)
        fixed[self.set] = SET_LABEL + i
        fixed[self.mesh] = MESH_LABEL + i
        fixed[self.header] = HEADER_LABEL + i
        fixed[self.root] = ROOT_LABEL + i
        fixed[self.special] = SPECIAL_LABEL + i
        fixed[self.engine] = ENGINE_LABEL + i
        for k in range(NODES):
            fixed[self.nodes + 0xA0 * k] = NODE_LABEL + 8 * i + k
        for k in range(GROUPS):
            for j in range(ENTRIES):
                fixed[self.anims[k][j]] = ANIM_LABEL + (i * GROUPS + k) * ENTRIES + j
        ws(e, self.set, self.set, f["set_flags"], self.mesh)
        e.write(self.set + 0xE, struct.pack(">H", f["time_count"]))
        ws(e, self.set + 0x10, self.controls)
        ws(e, self.set + 0xA8, f["animations"])
        ws(e, self.set + 0xC0, f["fc0"], f["fc4"])
        ws(e, self.set + 0xCC, f["fcc"], 0)
        ws(e, self.set + 0xF0, f["ff0"], f["ff4"])
        for k in range(GROUPS):
            ws(e, self.set + 0x18 + 12 * k, 0xA5A5A5A5, ENTRIES, self.entries + k * ENTRIES * 8)
            for j in range(ENTRIES):
                ws(e, self.entries + (k * ENTRIES + j) * 8, j, self.anims[k][j])
        ws(e, self.mesh, f["mesh_f00"], self.header)
        ws(e, self.mesh + 0x10, f["mesh_f10"])
        ws(e, self.mesh + 0x1C, self.engine if f["engine"] else 0)
        ws(e, self.mesh + 0x28, self.embedded if f["embedded"] else 0, self.root if f["root"] else 0, 0)
        ws(e, self.header + 0x30, f["header_flags"])
        e.write(self.header + 0x42, struct.pack(">HH", f["all_nodes"], f["nodes"]))
        ws(e, self.header + 0x70, self.nodes, self.minis, self.special)
        ws(e, self.header + 0xB4, 0)
        for k in range(NODES):
            ws(e, self.nodes + 0xA0 * k, f["node%d" % k])
        for k in range(MINIS):
            ws(e, self.minis + 0x58 * k, f["mini%d" % k])
        e.write(self.root + 0x58, struct.pack(">H", f["vertices"]))
        ws(e, self.root + 0x64, self.points)
        ws(e, self.engine + 0x140, 0)
        for c in range(CONTROLS):
            ws(e, self.controls + 0x38 * c, *controls[3 * c:3 * c + 3])

    def dump(self, lab):
        """The 16 numbers the host's DumpRide gives."""
        e = self.e
        f30 = e.u32(self.mesh + 0x30)
        out = [e.u32(self.set + 4), e.u32(self.set + 0xE4), e.u32(self.set + 0xD4), e.u32(self.set + 0xE8),
               e.u32(self.set + 0xEC), lab(e.u32(self.set + 0xD0)), e.u32(self.set + 0xCC),
               -1 if f30 == 0 else (f30 - self.points) // 12, e.u32(self.header + 0x30)]
        out += [e.u32(self.nodes + 0xA0 * k) for k in range(NODES)]
        out += [e.u32(self.minis + 0x58 * k) for k in range(MINIS)]
        out.append(lab(e.u32(self.header + 0xB4)))
        if e.u32(self.mesh + 0x1C) == 0:
            out.append(-1)
        else:
            callback = e.u32(self.engine + 0x140)
            out.append(1 if callback == CALLBACK else (0 if callback == 0 else 2))
        return out


# ------------------------------------------------------------------ the PowerPC's fakes


def stack_word(e, n):
    """The n-th argument word after the eight in registers (a callee's view at its entry)."""
    return e.u32(e.r(1) + 0x38 + 4 * n)


def make_stubs():
    """The fakes of everything the six functions call, on the PowerPC side. Each records the
    event the host's fake of the same name records (R: arguments by the codes of calllog.py)."""
    S = CL.Stubs()
    R = CL.recorder
    S.real.update([TICK, ANIMATE, LOAD, INSTANCE, DESTROY_INSTANCE, DESTROY_TYPES])

    def queue(key, default=0):
        """The answer: the next value of the case's queue `key`."""
        return lambda e, L: L.ret(key, default) & MASK

    def strcat(e):
        """The library's strcat, unrecorded."""
        destination = e.r(3)
        e.write(destination, (e.cstr(destination) + e.cstr(e.r(4))).encode("latin-1") + b"\0")

    S.imp("strcat", strcat)
    # the clock
    S.internal(0x101472F4, R("Read", "L", queue("Read")))
    S.internal(0x1014733C, R("ReadFixed", "L", queue("ReadFixed")))
    # ride types and the loader
    S.internal(0x1006A768, R("DestroyRideType", "u"))

    def trace(e):
        """CTRACE(level, format, one int)."""
        e.cl.rec("CTRACE", [e.r(3), T.s32(e.r(5))], [e.cstr(e.r(4))])

    S.internal(0x1006FCBC, trace)

    def error_box(e):
        """ERRORBox(format)."""
        e.cl.rec("ERRORBox", [], [e.cstr(e.r(3))])

    S.internal(0x1006FD3C, error_box)

    def shared_path(e):
        """BuildSharedTexturePath(out) writes a fixed path."""
        e.cl.rec("BuildSharedTexturePath")
        e.write(e.r(3), b"Data:Fantasy:sharetex\0")

    S.internal(0x100B8960, shared_path)

    def setup_directory(e):
        """SetupDirectoryForMeshLoad(file, folder, shared) answers the queue."""
        e.cl.rec("SetupDirectory", [], [e.cstr(e.r(3)), e.cstr(e.r(4)), e.cstr(e.r(5))])
        e.set_r(3, T.u32(e.cl.ret("Setup")))

    S.internal(0x100680FC, setup_directory)

    def load_ride_anim_set(e):
        """LoadRideAnimSet(name, flags, &output, &loaded name, count): the first call makes
        ride 0's set, the second (into the set's +0xd0) ride 1's; answers the queue."""
        world = e.cw
        call = world["load_calls"]
        world["load_calls"] += 1
        loaded = e.r(6)
        into_variant = int(e.r(5) == world["rides"][0].set + 0xD0)
        e.cl.rec("LoadRideAnimSet", [call, e.r(4), e.r(7), int(loaded != 0), into_variant], [e.cstr(e.r(3))])
        if loaded:
            e.write(loaded, world["loaded"].encode("latin-1") + b"\0")
        ws(e, e.r(5), world["rides"][1 if call else 0].set)
        e.set_r(3, T.u32(e.cl.ret("LoadSet")))

    S.internal(0x1006840C, load_ride_anim_set)
    S.internal(0x1006F748, R("CalculateSkirtVertices", "LLLii"))
    S.internal(0x1006EBEC, R("SetBaseBlendFlags", "LLi"))
    S.internal(0x10067FDC, R("CorrectFaceFlags1", "Li"))

    def theme_name(e):
        """CGame::GetCurrentThemeName() answers the string object."""
        e.cl.rec("GetCurrentThemeName", [e.cl.lab(e.r(3))])
        e.set_r(3, e.cw["theme"])

    S.internal(0x1014A058, theme_name)
    S.internal(0x1005FA70, R("GenerateHoardingMeshes", "sLL"))

    def ride_heads(e):
        """SetupRideHeads(&set->mHeads): names the set and the mesh in it."""
        e.cl.rec("SetupRideHeads", [e.cl.lab(e.r(3) - 8), e.cl.lab(e.u32(e.r(3)))])

    S.internal(0x10200B28, ride_heads)
    S.internal(0x1005E8E8, R("GetRideHeightMap", "LuusLu"))

    def font_path(e):
        """GetFontPath(out) writes a fixed path and answers the queue."""
        e.cl.rec("GetFontPath")
        e.write(e.r(3), b"fonts:Ride\0")
        e.set_r(3, T.u32(e.cl.ret("FontPath")))

    S.internal(0x101EEAC8, font_path)

    def ride_sign(e):
        """GenerateRideSign(set, folder, name, font path, sign 1, sign 2): two events, since
        an event holds three texts."""
        e.cl.rec("GenerateRideSign", [e.cl.lab(e.r(3))], [e.cstr(e.r(4)), e.cstr(e.r(5)), e.cstr(e.r(6))])
        signs = [CL.wide_text(e, e.r(r)) if e.r(r) else "(null)" for r in (7, 8)]
        e.cl.rec("GenerateRideSignText", [], signs)

    S.internal(0x101EE290, ride_sign)

    def type_name(e):
        """CRideDatabase::GetTypeName(id in the high half): answers the case's name."""
        e.cl.rec("GetTypeName", [e.cl.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF])
        e.set_r(3, e.cw["type_name"])

    S.internal(0x10154A10, type_name)
    S.internal(0x10067E1C, R("CorrectNodeFlags2", "L"))

    # instances
    def instance_ride_anim_set(e):
        """InstanceRideAnimSet(set, heap, flags, &output, count): hands out ride 0's set when the
        queue says 1."""
        e.cl.rec("InstanceRideAnimSet", [e.cl.lab(e.r(3)), e.r(4), e.r(5), e.r(7)])
        result = e.cl.ret("InstanceSet", 1)
        if result == 1:
            ws(e, e.r(6), e.cw["rides"][0].set)
        e.set_r(3, T.u32(result))

    S.internal(0x10062E1C, instance_ride_anim_set)

    def init_ride_anim_set(e):
        """InitRideAnimSet(set, a, b, c) answers 1."""
        e.cl.rec("InitRideAnimSet", [e.cl.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))])
        e.set_r(3, 1)

    S.internal(0x100C7768, init_ride_anim_set)

    def is_valid_animation(e):
        """IsValidAnimation(&set, a, b): the set is passed by reference."""
        e.cl.rec("IsValidAnimation", [e.cl.lab(e.u32(e.r(3))), e.r(4), e.r(5)])
        e.set_r(3, T.u32(e.cl.ret("Valid")))

    S.internal(0x100C5D3C, is_valid_animation)
    S.internal(0x100C6670, R("SetCurRideAnim", "Luuufu"))

    def correct_position(e):
        """CorrectRidePosnForRotation(&x, &z, rotation, &out x, &out z, w, h): moves the cell by
        the queue's offsets and writes its two outputs."""
        x = e.u32(e.r(3))
        z = e.u32(e.r(4))
        e.cl.rec("CorrectRidePosnForRotation", [T.s32(x), T.s32(z), T.s32(e.r(5)), T.s32(e.r(8)), T.s32(e.r(9))])
        ws(e, e.r(3), x + e.cl.ret("PosDx"))
        ws(e, e.r(4), z + e.cl.ret("PosDz"))
        ws(e, e.r(6), e.cl.ret("OutA"))
        ws(e, e.r(7), e.cl.ret("OutB"))

    S.internal(0x101ED5BC, correct_position)

    def required_y_offset(e):
        """GetRideRequiredYOffset(...) answers a float from the queue (as bits)."""
        ints = [T.s32(e.r(3)), T.s32(e.r(4)), e.cl.lab(e.r(5)), T.s32(e.r(6)), T.s32(e.r(7)), e.r(8), T.s32(e.r(9))]
        e.cl.rec("GetRideRequiredYOffset", ints)
        e.set_f1(bf(e.cl.ret("YOffset")))

    S.internal(0x101ED29C, required_y_offset)

    def set_position(e):
        """SetRidePositionAndRotation(set, 4 ints, rotation, y, flags, then two on the stack)."""
        ints = [e.cl.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), T.s32(e.r(7)), T.s32(e.r(8)),
                CL.fbits(e.f(1)), e.r(10), T.s32(stack_word(e, 0)), T.s32(stack_word(e, 1))]
        e.cl.rec("SetRidePositionAndRotation", ints)

    S.internal(0x101ED6AC, set_position)
    S.internal(0x1006E930, R("DoMeshSkirtDeform", "LLfu"))
    S.internal(0x10069770, R("GetBlendedBaseTextures", "L"))
    S.internal(0x100B9180, R("SetWorldBoundingArea", "LL"))
    S.internal(0x10060C80, R("SetHoardingYOffsets", "L"))

    def face_normals(e):
        """CalculateRootNodeFaceNormals(node, a, &result) writes the queue's answer."""
        e.cl.rec("CalculateRootNodeFaceNormals", [e.cl.lab(e.r(3)), T.s32(e.r(4))])
        ws(e, e.r(5), e.cl.ret("Normals"))

    S.internal(0x100694C8, face_normals)
    S.internal(0x10065178, R("EnableMeshDraw", "L"))
    S.internal(0x100650C4, R("EnableMeshDrawRange", "Luu"))
    S.internal(0x101FFB0C, R("TPProcessEmbeddedData", "L"))
    S.internal(0x100432DC, R("IsCoasterType", "u", queue("Coaster")))
    S.internal(0x100434EC, R("InstanceCoaster", "uuuuiL"))
    S.internal(0x100C609C, R("OverrideActiveChannels", "L"))
    S.internal(0x1006D2F8, R("StartFlashingObject", "L"))
    S.internal(0x100C7418, R("RecalculateInvalidFaceNormals", "L"))
    # taking an instance away
    S.internal(0x1006D0F8, R("UnhighlightRide"))
    S.internal(0x100329CC, R("IsRideFirstPersonRide", "u", queue("FirstPerson")))
    S.internal(0x10032584, R("StopRidingRideIn1stPerson"))
    S.internal(0x1006D430, R("StopFlashingObject", "L"))
    S.internal(0x100438FC, R("DestroyCoasterInstanceFromEntranceID", "u"))
    S.internal(0x10063338, R("DestroyRideAnimSetInstance", "Luu"))
    # animating a set
    S.internal(0x100C6A20, R("TickRideAnimSet", "L", queue("Tick")))
    S.internal(0x100C5054, R("ResetRideAnimationSingleChannel", "LLL"))

    def animate_control(e):
        """DoAnimateRideSet(set, control, a): a control is named by its place in the set's controls."""
        e.cl.rec("DoAnimateRideSet", [e.cl.lab(e.r(3)), (e.r(4) - e.u32(e.r(3) + 0x10)) // 0x38, T.s32(e.r(5))])

    S.internal(0x100C6C2C, animate_control)
    S.internal(0x100C7224, R("RecalculateInvalidNodeFaceNormals", "L"))
    return S


STUBS = None


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    STUBS = make_stubs()


def start(e, keep=()):
    """Activates the fakes. keep: (address, size) pairs of globals to put back afterwards."""
    g = Globals(e)
    for address, size in keep:
        g.keep(address, size)
    e.cw = {"load_calls": 0}
    return g, STUBS.activate(e)


def begin_log(e, case, fixed):
    """Starts the call log, once the world is built and its objects have their labels."""
    e.cl = CL.PpcLog(fixed, case.get("queues"))


def finish(e, g, saved):
    CL.Stubs.deactivate(e, saved)
    g.close()


def host_call(lib, name, words, texts, case):
    """Runs a host entry point on a list of numbers (and texts); returns its numbers and the log."""
    CL.host_begin(lib, {}, case.get("queues"))
    fn = getattr(lib, name)
    fn.argtypes = [C.POINTER(C.c_int64)] + [C.c_char_p] * len(texts) + [C.POINTER(C.c_int64)]
    fn.restype = C.c_int32
    out = (C.c_int64 * 256)()
    n = fn((C.c_int64 * len(words))(*words), *[t.encode("latin-1") for t in texts], out)
    return list(out[:n]), CL.host_end(lib)


# ------------------------------------------------------------------ TickRideAnims


def ppc_tick(e, case):
    fixed = {CLOCK: CLOCK_LABEL}
    g, saved = start(e, [(TPGLOBALS + 0x4010, 12)])
    try:
        ws(e, TPGLOBALS + 0x4010, case["previous"], 0, 0)
        begin_log(e, case, fixed)
        T.call_ppc(e, TICK, [])
        out = [e.u32(TPGLOBALS + 0x4010), e.u32(TPGLOBALS + 0x4014), e.u32(TPGLOBALS + 0x4018)]
        return out, e.cl.log
    finally:
        finish(e, g, saved)


def host_tick(lib, case):
    return host_call(lib, "tpw_rt_tick", [case["previous"]], [], case)


def tick_case(previous, now, fixed):
    return {"previous": previous & MASK, "queues": {"Read": [T.s32(now)], "ReadFixed": [T.s32(fixed)]}}


def tick_edges():
    # (the word before, the clock now, the fixed clock): zero, a second, wrapping, going back, 2^24 + 1
    # and 2^25 + 1 (float rounding), the biggest differences
    cases = ((0, 0, 0), (0, 1000, 5), (1000, 2000, -1), (0xFFFFFFFF, 0, 7), (5, 3, 9), (0, 0x7FFFFFFF, 1),
             (0, 0x80000000, 2), (0, 0xFFFFFFFF, 3), (0x12345678, 0x12345679, 0x7FFFFFFF), (0, 16777217, 4),
             (0, 16777219, 4), (0, 33554433, 4), (100, 100, 0x80000000), (0xFFFFFFF0, 0x10, -2), (0, 1, 0),
             (0, 999, 0), (0, 1001, 0), (0, 4294967295, 0))
    for previous, now, fixed in cases:
        yield tick_case(previous, now, fixed)


def tick_random(r):
    previous = r.getrandbits(32)
    kind = r.randrange(4)
    if kind == 0:
        now = r.getrandbits(32)
    elif kind == 1:
        now = previous + r.randrange(3000)
    elif kind == 2:
        now = previous - r.randrange(3000)
    else:
        now = previous + r.getrandbits(r.randrange(1, 33))
    return tick_case(previous, now & MASK, r.getrandbits(32))


# ------------------------------------------------------------------ DestroyAllRideTypes


def ppc_destroy_types(e, case):
    g, saved = start(e, [(TYPE_COUNT, 4)])
    try:
        ws(e, TYPE_COUNT, case["count"])
        begin_log(e, case, {})
        T.call_ppc(e, DESTROY_TYPES, [])
        return [e.u32(TYPE_COUNT)], e.cl.log
    finally:
        finish(e, g, saved)


def host_destroy_types(lib, case):
    return host_call(lib, "tpw_rt_destroy_types", [case["count"]], [], case)


def destroy_types_edges():
    for count in (0, 1, 2, 3, 5, 10, 40, 100, 329):
        yield {"count": count}


def destroy_types_random(r):
    return {"count": r.randrange(0, 30)}


# ------------------------------------------------------------------ DestroyRideInstance


def destroy_instance_words(c):
    return [c["instance"], c["first_free"], c["count_a"], c["count_b"], c["highlighted"], c["potential"]] + \
        c["window"] + c["flags"]


def ppc_destroy_instance(e, case):
    fixed = {}
    g, saved = start(e, [(SETS, 4 * WINDOW), (FIRST_FREE, 12), (HIGHLIGHTED, 4), (POTENTIAL, 4)])
    try:
        rides = []
        for i in range(RIDES):
            rides.append(PpcRide(e, i, ride_words(set_flags=case["flags"][i]), fixed))
        sets = [ride.set for ride in rides]
        ws(e, FIRST_FREE, case["first_free"], case["count_a"], case["count_b"])
        ws(e, HIGHLIGHTED, sets[case["highlighted"] - 1] if case["highlighted"] else 0)
        ws(e, POTENTIAL, sets[case["potential"] - 1] if case["potential"] else 0)
        ws(e, SETS, *[sets[v - 1] if v else 0 for v in case["window"]])
        begin_log(e, case, fixed)
        T.call_ppc(e, DESTROY_INSTANCE, [case["instance"]])
        lab = e.cl.lab
        out = [e.u32(FIRST_FREE), e.u32(COUNT_A), e.u32(COUNT_B), lab(e.u32(HIGHLIGHTED)), lab(e.u32(POTENTIAL))]
        out += [lab(e.u32(SETS + 4 * s)) for s in range(WINDOW)]
        return out, e.cl.log
    finally:
        finish(e, g, saved)


def host_destroy_instance(lib, case):
    return host_call(lib, "tpw_rt_destroy_instance", destroy_instance_words(case), [], case)


def destroy_instance_case(instance, window, first_free=0, count_a=0, count_b=0, highlighted=0, potential=0, flags=None,
                          first_person=0):
    return {"instance": instance, "window": window, "first_free": first_free, "count_a": count_a, "count_b": count_b,
            "highlighted": highlighted, "potential": potential, "flags": flags or [0] * RIDES,
            "queues": {"FirstPerson": [first_person]}}


def slots(*pairs):
    """A window with ride numbers (1 to 6) at the given slots: slots((3, 1), (4, 2))."""
    window = [0] * WINDOW
    for slot, ride in pairs:
        window[slot] = ride
    return window


def destroy_instance_edges():
    yield destroy_instance_case(0, slots((1, 1)), 1, 0, 1)
    yield destroy_instance_case(5, slots((1, 1)), 1, 0, 1)
    yield destroy_instance_case(1, slots((1, 1)), 1, 0, 1)
    yield destroy_instance_case(1, slots((1, 1)), 1, 0, 1, highlighted=1)
    yield destroy_instance_case(1, slots((1, 1)), 1, 0, 1, potential=1)
    yield destroy_instance_case(1, slots((1, 1)), 1, 0, 1, highlighted=1, potential=1, first_person=1, flags=[0x4000000] * 6)
    yield destroy_instance_case(1, slots((1, 1)), 1, 0, 1, highlighted=2, potential=3)
    # a chain of three: the last, then the middle, then the first
    window = slots((1, 1), (2, 2), (3, 3))
    yield destroy_instance_case(3, window, 3, 0, 3)
    yield destroy_instance_case(2, window, 3, 0, 3)
    yield destroy_instance_case(1, window, 3, 0, 3)
    # a hole at 2: destroying 3 (the top) drops the hole as well, destroying 1 leaves two holes
    window = slots((1, 1), (3, 2), (4, 3))
    yield destroy_instance_case(4, window, 1, 1, 3)
    yield destroy_instance_case(3, window, 1, 1, 3)
    yield destroy_instance_case(1, window, 1, 1, 3)
    yield destroy_instance_case(1, window, 3, 1, 3)
    # holes at 1 and 2 under the top
    window = slots((3, 1), (4, 2))
    yield destroy_instance_case(4, window, 0, 2, 2)
    yield destroy_instance_case(3, window, 0, 2, 2)
    yield destroy_instance_case(3, window, 5, 2, 2)


def destroy_instance_random(r):
    count_b = r.randrange(0, 12)
    count_a = r.choice([0, 0, 1, 2, 3, r.randrange(0, 8)])
    top = count_a + count_b
    window = [0] * WINDOW
    for s in range(1, top + 2):
        if r.random() < 0.75:
            window[s] = r.randrange(1, RIDES + 1)
    kind = r.randrange(6)
    if kind == 0:
        instance = r.randrange(0, 20)
    elif kind == 1:
        instance = top
    elif kind == 2:
        instance = count_b
    else:
        instance = r.randrange(1, max(2, top + 2))
    instance = max(0, min(instance, WINDOW - 1))
    highlighted = r.choice([0, 0, window[instance], window[instance], r.randrange(1, RIDES + 1)])
    potential = r.choice([0, 0, window[instance], window[instance], r.randrange(1, RIDES + 1)])
    return destroy_instance_case(instance, window, r.randrange(0, 14), count_a, count_b, highlighted, potential,
                                 [r.choice([0, 0x4000000, r.getrandbits(32)]) for _ in range(RIDES)], r.randrange(2))


# ------------------------------------------------------------------ AnimateRideSet


def animate_words(c):
    return [c["a"], c["flags"], int(c["present"]), c["system"]] + c["ride"]


def ppc_animate(e, case):
    fixed = {}
    system = e.data_import("gei_System")
    g, saved = start(e, [(system, 4)])
    try:
        ride = PpcRide(e, 0, case["ride"], fixed)
        ws(e, system, case["system"])
        begin_log(e, case, fixed)
        result = T.call_ppc(e, ANIMATE, [ride.set if case["present"] else 0, case["a"], case["flags"]])[0]
        out = [e.u32(ride.set + 4), e.u32(ride.header + 0x30)]
        out += [e.u32(ride.nodes + 0xA0 * k) for k in range(NODES)]
        out += [e.u32(ride.minis + 0x58 * k) for k in range(MINIS)]
        out.append(e.u32(system))
        out.append(int(result == (1 if case["present"] else 0)))
        return out, e.cl.log
    finally:
        finish(e, g, saved)


def host_animate(lib, case):
    return host_call(lib, "tpw_rt_animate", animate_words(case), [], case)


def animate_case(ride=None, a=1, flags=0, present=True, system=0, ticks=(0,)):
    return {"ride": ride or ride_words(), "a": a, "flags": flags, "present": present, "system": system,
            "queues": {"Tick": list(ticks)}}


def animate_edges():
    controls = ((0, 3, 1), (0x40, 0xC, 2), (5, 0, 0))
    yield animate_case(present=False)
    yield animate_case(ride_words(animations=0), ticks=(0,))
    yield animate_case(ride_words(animations=1, time_count=3, controls=controls), ticks=(1, 1, 1, 1))
    yield animate_case(ride_words(animations=1, time_count=3, controls=controls, header_flags=4, nodes=2, node0=0x10000,
                                  node1=0x10001, all_nodes=2), ticks=(1, 0))
    yield animate_case(ride_words(set_flags=0x4, animations=1, time_count=2, controls=controls), ticks=(1,))
    yield animate_case(ride_words(set_flags=0x8000, animations=1, time_count=2, controls=controls), ticks=(1,))
    yield animate_case(ride_words(set_flags=0x8004, animations=1), ticks=(1,))
    yield animate_case(ride_words(set_flags=0x40000000 | 0x400000, animations=2, nodes=2, all_nodes=4, node0=0x100,
                                  node1=0x900, mini0=0x100, mini1=0x800), system=1)
    yield animate_case(ride_words(set_flags=0x40000000, animations=2, nodes=2, all_nodes=4, node0=0x800, node1=0x1,
                                  mini0=0x80, mini1=0x800), system=1)
    yield animate_case(ride_words(embedded=1, set_flags=0, animations=0), flags=0)
    yield animate_case(ride_words(embedded=1, set_flags=0x20000000, animations=0), flags=0)
    yield animate_case(ride_words(embedded=1, header_flags=4, animations=0), flags=8)
    yield animate_case(ride_words(embedded=1, header_flags=0, animations=0), flags=8)
    yield animate_case(ride_words(embedded=1, header_flags=0, animations=1, time_count=1), flags=0xC, a=0)


def animate_random(r):
    nodes = r.randrange(0, NODES + 1)
    all_nodes = r.randrange(0, nodes + MINIS + 1)
    ride = ride_words(
        set_flags=r.choice([0, 0x4, 0x8000, 0x8004, 0x40000000, 0x400000, 0x20000000, r.getrandbits(32), r.getrandbits(32)]),
        animations=r.choice([0, 1, 1, 2]),
        embedded=r.randrange(2),
        header_flags=r.choice([0, 4, r.getrandbits(32)]),
        nodes=nodes,
        node0=r.choice([0, 0x100, 0x800, 0x10000, r.getrandbits(32)]),
        node1=r.choice([0, 0x100, 0x800, 0x10000, r.getrandbits(32)]),
        node2=r.choice([0, 0x100, 0x800, 0x10000, r.getrandbits(32)]),
        all_nodes=all_nodes,
        mini0=r.choice([0, 0x100, 0x800, r.getrandbits(32)]),
        mini1=r.choice([0, 0x100, 0x800, r.getrandbits(32)]),
        time_count=r.randrange(0, CONTROLS + 1),
        controls=[(r.getrandbits(32), r.choice([0, 1, 5, 11, 12, 12]), r.randrange(0, ENTRIES)) for _ in range(CONTROLS)])
    return animate_case(ride, r.choice([0, 1, -1, 5]), r.choice([0, 4, 8, 12, 1, r.getrandbits(32)]), r.random() < 0.95,
                        r.choice([0, 0x80000000, 0x40000000, r.getrandbits(32)]),
                        [r.choice([0, 1, 1, 2]) for _ in range(r.randrange(1, 6))])


# ------------------------------------------------------------------ InstanceRide


def instance_words(c):
    return ([c["type"], c["flags"], int(c["params"] is not None)] + list(c["params"] or (0, 0, 0, 0)) +
            [c["count"], c["user"], c["coaster_data"], c["d"], c["e"], c["slot"], c["g"], c["first_free"], c["count_a"],
             c["count_b"], c["options"], c["field_mode"], c["width"]] + c["heights"] + list(c["exists"]) +
            list(c["variants"]) + c["window"] + c["ride"])


def ppc_instance(e, case):
    fixed = {CLOCK: CLOCK_LABEL}
    keep = [(SETS, 4 * WINDOW), (FIRST_FREE, 12), (TYPES, 24), (FIELD_HEADER, 4), (GAME_OPTIONS, 4)]
    g, saved = start(e, keep)
    try:
        ride = PpcRide(e, 0, case["ride"], fixed)
        e.cw["rides"] = [ride]
        others = [ride] + [PpcRide(e, i, ride_words(), fixed) for i in range(1, RIDES)]
        size_block = e.alloc(16)
        fixed[size_block] = SIZE_LABEL
        object_block = e.alloc(16)
        fixed[object_block] = OBJECT_LABEL
        # the type sets (types 1 to 3 and the variant): only +0xd0 is read
        variant = e.alloc(0xFC, b"\xa5" * 0xFC)
        fixed[variant] = TYPE_LABEL + 3
        types = []
        for k in range(3):
            block = e.alloc(0xFC, b"\xa5" * 0xFC)
            fixed[block] = TYPE_LABEL + k
            ws(e, block + 0xD0, variant if case["variants"][k] else 0)
            types.append(block if case["exists"][k] else 0)
        ws(e, TYPES, 0, *types, 0, 0)
        # the height field
        header = e.alloc(0xB8, b"\xa5" * 0xB8)
        field = e.alloc(0x30, b"\xa5" * 0x30)
        heights = e.alloc(80, struct.pack(">20I", *[h & MASK for h in case["heights"]]))
        fixed[header] = FIELD_LABEL
        ws(e, header + 0x6C, field if case["field_mode"] == 2 else 0)
        ws(e, field + 0x18, case["width"], 3)
        ws(e, field + 0x28, heights)
        ws(e, FIELD_HEADER, header if case["field_mode"] else 0)
        ws(e, FIRST_FREE, case["first_free"], case["count_a"], case["count_b"])
        ws(e, GAME_OPTIONS, case["options"])
        ws(e, SETS, *[others[v - 1].set if v else 0 for v in case["window"]])
        params = 0
        if case["params"] is not None:
            x, z, rotation, extra = case["params"]
            params = e.alloc(16)
            ws(e, params, x, z, rotation, object_block if extra else 0)
        e.set_f(31, 0.0)
        # the locals the original reads before it writes them (the cell, when there is no
        # height field to correct it) are 0 here, as they are on the host
        top = ppcemu.STACK_BASE + ppcemu.STACK_SIZE - 0x1000 - 0x400
        e.write(top - 0x400, b"\0" * 0x400)
        begin_log(e, case, fixed)
        answer = T.call_ppc(e, INSTANCE, [case["type"], case["flags"], params, case["count"], case["user"],
                                          size_block if case["coaster_data"] else 0, case["d"], case["e"], case["slot"],
                                          case["g"]])[0]
        lab = e.cl.lab
        out = [answer, e.u32(FIRST_FREE), e.u32(COUNT_A), e.u32(COUNT_B)]
        out += [lab(e.u32(SETS + 4 * s)) for s in range(WINDOW)]
        out += ride.dump(lab)
        return out, e.cl.log
    finally:
        finish(e, g, saved)


def host_instance(lib, case):
    return host_call(lib, "tpw_rt_instance", instance_words(case), [], case)


def place_queues(r, **given):
    """The answers of the fakes InstanceRide calls; each given as a list."""
    queues = {"InstanceSet": [1], "Valid": [1], "Coaster": [0], "YOffset": [fb(1.5)], "Normals": [1], "PosDx": [0],
              "PosDz": [0], "OutA": [3], "OutB": [4]}
    queues.update(given)
    return queues


HEIGHTS = [fb(0.25 * k - 1.0) for k in range(20)]


def instance_case(type=1, flags=0x101, params=None, count=0, user=0, coaster_data=0, d=0, e=0, slot=0xFFFFFFFF, g=0,
                  first_free=0, count_a=0, count_b=0, options=0, field_mode=2, width=4, heights=None, exists=(1, 1, 1),
                  variants=(0, 0, 0), window=None, ride=None, **queues):
    return {"type": type, "flags": flags, "params": params, "count": count, "user": user, "coaster_data": coaster_data,
            "d": d, "e": e, "slot": slot, "g": g, "first_free": first_free, "count_a": count_a, "count_b": count_b,
            "options": options, "field_mode": field_mode, "width": width, "heights": heights or HEIGHTS, "exists": exists,
            "variants": variants, "window": window or [0] * WINDOW, "ride": ride or ride_words(nodes=2, node0=1, node1=2),
            "queues": place_queues(None, **queues)}


def instance_edges():
    node_ride = ride_words(set_flags=0x1, nodes=3, node0=5, node1=6, node2=7, header_flags=0x10, mesh_f00=0x40, root=1,
                           vertices=3, engine=1, fc0=0x00010002, fc4=0x00030004, ff0=2, ff4=3)
    # the lobby's call: flags 0x101, no placement
    yield instance_case()
    yield instance_case(ride=node_ride)
    yield instance_case(type=0)
    yield instance_case(type=4)
    yield instance_case(type=3, exists=(1, 1, 0))
    yield instance_case(InstanceSet=[0])
    yield instance_case(InstanceSet=[2])
    # the variant: used with flag 0x400 when the type has one
    yield instance_case(type=2, flags=0x401, variants=(0, 1, 0))
    yield instance_case(type=2, flags=0x401, variants=(0, 0, 0))
    yield instance_case(type=2, flags=0x1, variants=(0, 1, 0))
    yield instance_case(type=2, flags=0x801, variants=(0, 1, 0))
    yield instance_case(flags=0xC01, variants=(1, 0, 0))
    # numbering: the first free, filling a hole, an explicit slot
    yield instance_case(first_free=0, count_b=0, count_a=0)
    yield instance_case(first_free=2, count_b=2, count_a=0, window=[0, 1, 2] + [0] * 37)
    yield instance_case(first_free=1, count_b=2, count_a=1, window=[0, 1, 0, 3] + [0] * 36)
    yield instance_case(first_free=1, count_b=1, count_a=2, window=[0, 1, 0, 0, 4] + [0] * 35)
    yield instance_case(first_free=1, count_b=3, count_a=2, window=[0, 1, 0, 2, 0, 5] + [0] * 34)
    yield instance_case(first_free=0, count_b=2, count_a=1, window=[0, 0, 2, 3] + [0] * 36)
    yield instance_case(first_free=5, count_b=3, count_a=2, window=[0, 1, 0, 3, 0, 5] + [0] * 34)
    yield instance_case(slot=7)
    yield instance_case(slot=1, first_free=3, count_b=3, window=[0, 1, 2, 3] + [0] * 36)
    yield instance_case(slot=0)
    # flags that set the set's flags, enable the drawing, flash, override channels
    for flags in (0x1, 0x2, 0x80, 0x83, 0x100, 0x102, 0x181, 0x400, 0x800, 0x1000, 0xFFFFFFFF & ~0x26 & ~0x8 & ~0x200):
        for set_flags in (0, 0x4000, 0x1000000, 0x2000000, 0x20000000, 0x3000001, 0xFFFFFFFF):
            yield instance_case(flags=flags, params=(1, 1, 0, 0) if flags & 0x26 else None,
                                ride=ride_words(set_flags=set_flags, nodes=1, node0=0, embedded=1, root=1, vertices=2,
                                                engine=1, fc0=7, fc4=9))
    # placement: from the height field, from an object, with the corrections
    placed = ride_words(nodes=1, node0=0, ff0=2, ff4=3, engine=1, mesh_f00=0x40, root=1, vertices=4, mesh_f10=9, embedded=1)
    for flags in (0x2, 0x4, 0x20, 0x6, 0x26, 0x22, 0x28 | 0x2, 0x2A, 0x2E, 0x102, 0x10E, 0x20A, 0x20E, 0x30E, 0xFFFFFFFF):
        for extra in (0, 1):
            for dx, dz in ((0, 0), (1, 1), (-1, -1)):
                yield instance_case(flags=flags, params=(2, 1, 3, extra), ride=placed, PosDx=[dx], PosDz=[dz],
                                    options=0x4000 if flags & 0x20 else 0, g=5, coaster_data=extra, Coaster=[1])
    # no height field: a placement from an object reads no height, the cell is 0
    for field_mode in (0, 1):
        yield instance_case(flags=0x2A, params=(2, 1, 3, 1), field_mode=field_mode, ride=placed, options=0x4000)
        yield instance_case(flags=0x22A, params=(2, 1, 3, 1), field_mode=field_mode, ride=placed, Coaster=[1], g=3)
    yield instance_case(flags=0x200, ride=placed, Coaster=[1], g=3, coaster_data=1)
    yield instance_case(flags=0x200, ride=placed, Coaster=[0])
    # flag 8 without placement, with and without the height field
    yield instance_case(flags=0x8, field_mode=2, ride=placed, options=0x4000)
    yield instance_case(flags=0x8, field_mode=0, ride=placed, options=0x4000)
    yield instance_case(flags=0x8, field_mode=1, ride=placed)
    yield instance_case(flags=0x1, field_mode=0)
    yield instance_case(flags=0x1, field_mode=1)
    # the root node's face normals
    yield instance_case(ride=ride_words(root=1, vertices=0, nodes=0), Normals=[1])
    yield instance_case(ride=ride_words(root=1, vertices=5, nodes=0), Normals=[0])
    yield instance_case(ride=ride_words(root=1, vertices=7, mesh_f10=-3, nodes=0), Normals=[-1])
    yield instance_case(ride=ride_words(root=0, vertices=7, nodes=0), Normals=[1])
    # a coaster
    yield instance_case(flags=0x226, params=(1, 1, 0, 0), g=9, coaster_data=1, Coaster=[1], ride=placed)
    yield instance_case(flags=0x226, params=(1, 1, 0, 0), g=9, coaster_data=0, Coaster=[0], ride=placed)
    yield instance_case(flags=0x226, params=(1, 1, 0, 0), g=9, coaster_data=1, Coaster=[1], ride=placed, slot=6)
    # IsValidAnimation says no
    yield instance_case(Valid=[0])
    # the placement's numbers: negative, large
    yield instance_case(flags=0x2, params=(3, 2, -7, 0), ride=placed, OutA=[-5], OutB=[0x7FFFFFFF], user=-1, count=3)
    yield instance_case(flags=0x2A, params=(0, 0, 0x7FFFFFFF, 1), ride=placed, YOffset=[fb(-3.25)], user=0x7FFFFFFF)
    yield instance_case(flags=0x2A, params=(0, 0, 0, 1), ride=placed, YOffset=[0x7FC00000])
    yield instance_case(flags=0x2A, params=(0, 0, 0, 1), ride=placed, YOffset=[fb(-0.0)])


def instance_random(r):
    kind = r.randrange(8)
    place = kind < 5
    flags = 0
    for bit in (1, 2, 4, 8, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x10000):
        if r.random() < 0.4:
            flags |= bit
    if place:
        flags |= r.choice([0x2, 0x4, 0x20, 0x6, 0x22, 0x26])
    else:
        flags &= ~0x26
    params = None
    if flags & 0x26 or r.random() < 0.2:
        params = (r.randrange(1, 4), r.randrange(1, 3), r.choice([0, 1, 2, 3, -1, r.getrandbits(31)]), r.randrange(2))
    # without a height field a placement must take its height from an object
    from_object = params is not None and params[3] != 0 and flags & 8 != 0
    if flags & 0x26 == 0 or from_object:
        field_mode = r.randrange(3)
    else:
        field_mode = 2
    count_b = r.randrange(0, 12)
    count_a = r.choice([0, 0, 1, 2, r.randrange(0, 6)])
    window = [0] * WINDOW
    for s in range(1, count_a + count_b + 3):
        if r.random() < 0.7:
            window[s] = 1
    slot = 0xFFFFFFFF if r.random() < 0.8 else r.randrange(0, 30)
    first_free = r.choice([count_b, count_b, r.randrange(0, 14)])
    ride = ride_words(
        set_flags=r.choice([0, 1, 0x4000, 0x1000000, 0x2000000, 0x20000000, r.getrandbits(32)]),
        mesh_f00=r.choice([0, 0x40, r.getrandbits(32)]), mesh_f10=r.randrange(-5, 5), embedded=r.randrange(2),
        root=r.randrange(2), vertices=r.randrange(0, POINTS), engine=r.randrange(2), header_flags=r.getrandbits(32),
        nodes=r.randrange(0, NODES + 1), node0=r.getrandbits(32), node1=r.getrandbits(32), node2=r.getrandbits(32),
        fc0=r.getrandbits(32), fc4=r.getrandbits(32), ff0=r.randrange(0, 5), ff4=r.randrange(0, 5))
    return instance_case(
        type=r.choice([0, 1, 1, 2, 3, 4]), flags=flags, params=params, count=r.randrange(0, 4),
        user=r.getrandbits(32) - (1 << 31),
        coaster_data=r.randrange(2), d=r.choice([0, 1]), e=r.choice([0, 1]), slot=slot, g=r.randrange(0, 20),
        first_free=first_free, count_a=count_a, count_b=count_b, options=r.choice([0, 0x4000, r.getrandbits(32)]),
        field_mode=field_mode, width=4, heights=[fb(r.uniform(-20, 20)) for _ in range(20)],
        exists=tuple(int(r.random() < 0.85) for _ in range(3)), variants=tuple(r.randrange(2) for _ in range(3)),
        window=window, ride=ride, InstanceSet=[r.choice([1, 1, 1, 1, 0, 2])], Valid=[r.randrange(2)],
        Coaster=[r.randrange(2)], YOffset=[fb(r.uniform(-50, 50))], Normals=[r.choice([0, 1, 1, -1])],
        PosDx=[r.randrange(-1, 2)], PosDz=[r.randrange(-1, 2)], OutA=[r.randrange(-3, 40)], OutB=[r.randrange(-3, 40)])


# ------------------------------------------------------------------ LoadRideType


def load_words(c):
    return ([c["flags"], c["id"], c["count"], c["d"], c["e"], int(c["has_size"]), c["size"][0], c["size"][1],
             int(c["has_object"]), int(c["signs"] is not None), c["type_count"], c["field"]] + c["set"] + c["variant"])


def ppc_load(e, case):
    fixed = {GAME: GAME_LABEL, DATABASE: DATABASE_LABEL}
    keep = [(TYPES, 64), (TYPE_COUNT, 4), (FIELD_HEADER, 4)]
    g, saved = start(e, keep)
    try:
        rides = [PpcRide(e, 0, case["set"], fixed), PpcRide(e, 1, case["variant"], fixed)]
        e.cw["rides"] = rides
        e.cw["loaded"] = case["loaded"]
        theme = e.alloc(0x14)
        CL.tds_set(e, theme, case["theme"])
        e.cw["theme"] = theme
        e.cw["type_name"] = CL.new_wide(e, case["type_name"])
        size_block = e.alloc(8)
        ws(e, size_block, *[v & MASK for v in case["size"]])
        fixed[size_block] = SIZE_LABEL
        object_block = e.alloc(8)
        fixed[object_block] = OBJECT_LABEL
        header = e.alloc(0xB8, b"\xa5" * 0xB8)
        fixed[header] = FIELD_LABEL
        ws(e, FIELD_HEADER, header if case["field"] else 0)
        ws(e, TYPES, *([0] * 16))
        ws(e, TYPE_COUNT, case["type_count"])
        signs = case["signs"]
        folder = CL.new_text(e, case["folder"])
        name = CL.new_text(e, case["name"])
        begin_log(e, case, fixed)
        answer = T.call_ppc(e, LOAD, [folder, name, size_block if case["has_size"] else 0,
                                      object_block if case["has_object"] else 0, case["flags"], case["id"], case["count"],
                                      CL.new_wide(e, signs[0]) if signs else 0, CL.new_wide(e, signs[1]) if signs else 0,
                                      case["d"], case["e"]])[0]
        count = e.u32(TYPE_COUNT)
        a = rides[0].set
        out = [answer, count, e.cl.lab(e.u32(TYPES + 4 * count)) if count < 16 else -2, e.u32(a + 4), e.u32(a + 0xF8),
               e.u32(a + 0xF0), e.u32(a + 0xF4), e.cl.lab(e.u32(a + 0xD0)), e.u32(a + 0xCC), e.u32(rides[1].set + 0xCC)]
        return out, e.cl.log
    finally:
        finish(e, g, saved)


def host_load(lib, case):
    signs = case["signs"] or ("", "")
    texts = [case["folder"], case["name"], case["loaded"], signs[0], signs[1], case["theme"], case["type_name"]]
    return host_call(lib, "tpw_rt_load", load_words(case), texts, case)


FOLDERS = ["data:lobby:globe", "Data:Fantasy:rides", "disc:models"]
NAMES = ["bullfrog", "Ride.md2", "a.b.c", "disc:.:ride", "tree", ".hidden", "x.", "dir:file.md2"]
LOADED = ["disc:models:bullfrog.md2", "bullfrog", "disc:.:bullfrog", "a.b.hmp", "disc:models:x.", "dir.d:file", ".", ""]


def load_case(flags=0xC0, id=18000, count=1, d=0, e=0, has_size=False, size=(7, 9), has_object=False, signs=None,
              type_count=0, field=1, folder=FOLDERS[0], name=NAMES[0], loaded=LOADED[0], theme="Fantasy",
              type_name="Test ride", set=None, variant=None, setup=(1,), load_set=(1, 1), font=(1,)):
    return {"flags": flags, "id": id, "count": count, "d": d, "e": e, "has_size": has_size, "size": size,
            "has_object": has_object, "signs": signs, "type_count": type_count, "field": field, "folder": folder,
            "name": name, "loaded": loaded, "theme": theme, "type_name": type_name,
            "set": set or ride_words(root=1, ff0=0), "variant": variant or ride_words(root=0),
            "queues": {"Setup": list(setup), "LoadSet": list(load_set), "FontPath": list(font)}}


LOAD_FLAGS = (0x10, 0x20, 0x40, 0x2, 0x100, 0x200, 0x4, 0x4000, 0x10000, 0x40000, 0x100000, 0x200000, 0x400000, 0x1000000)
CONTROL_FLAGS = (0x1, 0x8, 0x80, 0x400, 0x800, 0x2000, 0x20000)


def load_edges():
    # the lobby's call
    yield load_case()
    yield load_case(flags=0x40C0)
    yield load_case(setup=(0,))
    yield load_case(setup=(2,))
    yield load_case(load_set=(0, 1))
    yield load_case(load_set=(2, 1))
    yield load_case(font=(0,))
    yield load_case(font=(2,), signs=("One", "Two"))
    yield load_case(signs=("One", "Two"))
    yield load_case(signs=("", ""))
    yield load_case(id=0x12345678, type_name="Name of type")
    yield load_case(type_count=5)
    yield load_case(type_count=14, load_set=(0, 0))
    # every flag alone, with and without the variant
    for flag in LOAD_FLAGS + CONTROL_FLAGS:
        yield load_case(flags=flag)
        yield load_case(flags=flag | 0x20000, load_set=(1, 0))
    yield load_case(flags=0xFFFFFFFF)
    yield load_case(flags=0xFFFFFFFF, load_set=(0, 1))
    # the file names
    for name in NAMES:
        for folder in FOLDERS[:2]:
            yield load_case(name=name, folder=folder)
    # the loaded name when the height map is made
    for loaded in LOADED:
        for flags in (0x400, 0x400 | 0x800, 0x400 | 0x20000):
            yield load_case(flags=flags, loaded=loaded, set=ride_words(fcc=0x1234), variant=ride_words(fcc=7))
    # sizes
    for d, e in ((0, 0), (3, 0), (0, 4), (3, 4), (-1, -2)):
        for has_size in (False, True):
            yield load_case(flags=0x401, d=d, e=e, has_size=has_size, size=(11, 12))
    yield load_case(flags=0x8, has_object=True)
    yield load_case(flags=0x8, has_object=False)
    yield load_case(flags=0x80)
    yield load_case(flags=0x2000, set=ride_words(root=1), variant=ride_words())
    yield load_case(flags=0x2000 | 0x20000, set=ride_words(root=0), variant=ride_words())
    yield load_case(flags=0x2000 | 0x20000, set=ride_words(root=1), variant=ride_words())
    yield load_case(flags=0x1, field=0)
    yield load_case(flags=0x400000)
    yield load_case(flags=0x401, count=0)
    yield load_case(flags=0x401, count=5)
    yield load_case(id=0xFFFFFFFF, flags=0x400, type_count=2)


def load_random(r):
    flags = 0
    for bit in LOAD_FLAGS + CONTROL_FLAGS:
        if r.random() < 0.3:
            flags |= bit
    flags |= r.choice([0, 0, 0x8000, 0x80000, r.getrandbits(32) & 0xFE000000])
    signs = None
    if r.random() < 0.4:
        signs = (r.choice(["One", "", "Sign text"]), r.choice(["Two", "", "x"]))
    return load_case(
        flags=flags, id=r.getrandbits(32), count=r.choice([0, 1, 1, 3, 12]), d=r.choice([0, 0, 5, -1, r.randrange(1 << 20)]),
        e=r.choice([0, 0, 6, -1, r.randrange(1 << 20)]), has_size=r.random() < 0.6,
        size=(r.randrange(-3, 50), r.randrange(-3, 50)), has_object=r.random() < 0.6, signs=signs,
        type_count=r.randrange(0, 12),
        field=r.randrange(2), folder=r.choice(FOLDERS), name=r.choice(NAMES), loaded=r.choice(LOADED),
        theme=r.choice(["Fantasy", "Halloween", ""]), type_name=r.choice(["Test ride", "", "Another name"]),
        set=ride_words(set_flags=r.getrandbits(32), root=r.randrange(2), fcc=r.getrandbits(32), ff0=r.randrange(5)),
        variant=ride_words(set_flags=r.getrandbits(32), root=r.randrange(2), fcc=r.getrandbits(32)),
        setup=(r.choice([1, 1, 1, 1, 0, 2]),), load_set=(r.choice([1, 1, 1, 0, 2]), r.choice([1, 1, 0, 2])),
        font=(r.choice([1, 1, 1, 0, 2]),))


# ------------------------------------------------------------------ the tests


def with_random(edge, rnd, extra=0, seed=7):
    """The edge cases, then `extra` more random ones drawn from a fixed seed."""
    def edges():
        yield from edge()
        generator = random.Random(seed)
        for _ in range(extra):
            yield rnd(generator)

    return edges


TESTS = [
    FnTest("TickRideAnims", ppc_tick, host_tick, edge=tick_edges, rnd=tick_random),
    FnTest("DestroyAllRideTypes", ppc_destroy_types, host_destroy_types, edge=destroy_types_edges, rnd=destroy_types_random),
    FnTest("DestroyRideInstance", ppc_destroy_instance, host_destroy_instance, edge=with_random(
        destroy_instance_edges, destroy_instance_random, 300), rnd=destroy_instance_random),
    FnTest("AnimateRideSet", ppc_animate, host_animate, edge=with_random(animate_edges, animate_random, 300),
           rnd=animate_random),
    FnTest("InstanceRide", ppc_instance, host_instance, edge=with_random(instance_edges, instance_random, 800),
           rnd=instance_random),
    FnTest("LoadRideType", ppc_load, host_load, edge=with_random(load_edges, load_random, 500), rnd=load_random),
]
