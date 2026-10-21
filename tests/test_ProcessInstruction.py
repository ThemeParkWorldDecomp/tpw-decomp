"""Unit v5882, the ride script interpreter (one instruction of a script's program per call), against
libtpw_unit_ProcessInstruction.dylib (tests/unit_ProcessInstruction.cpp), main app ("SimTheme Park"),
src/game/rides/rsse.cpp:
  ProcessInstruction 0x100cfdf4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee is a
fake on both sides that records its arguments (pointers as labels, floats as bits) and answers from the case's queues: ReadScript (the
instruction word, then the operand words), ExtractData (the operands' values), CClock::Read, CWorld::GetRand, SetCurRideAnim, GetCurrentAnim
(it also writes the kind), the object, limbo, bounce, walk-node, light, scream and sound calls, RSSE_InitScript and RSSE_GetScriptPointer
(1 to 3: three other scripts, 0: NULL), GetRideFromID (a slot holding the ride set, whose mesh is the head mesh), the C imports labs, strcpy,
strcat, strcmp (C semantics), time and localtime (a struct tm block).

The world of a case: the script (every field of its 0xf4 bytes, by Mac field size), its variables, stack, head words, save block (texts)
and name, up to three objects in its list, three other scripts (fields, variables, texts) and the live script list (gRSSEState.mScripts,
the script itself in it now and then), gRSSEF7BE4, gSoundBanks[2], gGameWorld. What is compared: the script, the other scripts, the
variables, the stack, the head words, the objects and the object CreateObject answers (fields by size, pointers as block names),
gRSSEF7BE4 and the event log.

No fused operations; fctiwz (op 16 to 26, 44, 86, 88, 93) only sees values below 2^31 (the emulator saturates differently above it), and
the speed is never -50 (a zero divisor). Not tested: an index operand outside the variables (ops 3, 5 and every destination word: the
original writes anywhere), a negative remote index in ops 66 to 69 (written before the table), RSSE_GetScriptPointer answering NULL in
ops 63 and 66 to 69 (no NULL check in the original), a NULL save block in ops 63 and 64 (read without a check), division by zero and INT_MIN / -1 in op 21 and ops 49 and 50 (the emulator and the
hardware differ), a GetRand queue that never lands on a free head in op 56 (the original loops forever).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ProcessInstruction"
MASK = 0xFFFFFFFF

# ---- the unit's own function
FN = 0x100cfdf4
OWN = {FN}

# ---- the Mac's addresses of the globals (the asm's TOC comments)
ADDR = dict(gRSSEF7BE4=0x102f7be4, gRSSEState=0x102f7bec, gGameWorld=0x1037ebfc, gClock=0x1037f048, gSoundBanks=0x102f7c94)
# ---- the host's offsets and sizes the module relies on (sizeof RSSE_SCRIPT, RSSE_OBJECT, mAnimFactor, RSSEState::mScripts, mHeads)
HOST_LAYOUT = [0xf4, 0x1c, 0xe4, 0x10, 0x10]
HOST_SCRIPTS = 0x10

# The labels of the log
LAB = dict(S=1, set=2, mesh=3, light=4, gClock=5, world=6, o0=10, o1=11, o2=12, newobj=13, A=20, B=21, C=22)
OTHERS = ["A", "B", "C"]


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def R(e, i):
    return s32(e.r(i))


def U(e, i):
    return e.r(i) & MASK


def fake(name, args, answer=None):
    """A recorder: args(e, L) -> the ints; answer: a queue name for r3 (or None)."""
    def fn(e):
        L = e.cl
        L.rec(name, tuple(args(e, L)))
        if answer is not None:
            e.set_r(3, L.ret(answer) & MASK)
    return fn


def lab3(e, L):
    return (L.lab(e.r(3)),)


def internal(addr, name, args, answer=None):
    STUBS.internal(addr, fake(name, args, answer))


internal(0x100cfbf8, "ReadScript", lab3, "ReadScript")
internal(0x100cece4, "ExtractData", lambda e, L: (L.lab(e.r(3)), R(e, 4)), "ExtractData")
internal(0x101472f4, "Read", lab3, "Read")
internal(0x1013b1e4, "GetRand", lab3, "GetRand")
internal(0x100cf6c8, "InitObject", lambda e, L: (L.lab(e.r(3)), L.lab(e.r(4)), R(e, 5), R(e, 6), R(e, 7)))
internal(0x100ced44, "KillLinkedObject", lambda e, L: (L.lab(e.r(3)), L.lab(e.r(4)), R(e, 5)))
internal(0x100cefb0, "SetObjParam", lambda e, L: (L.lab(e.r(3)), L.lab(e.r(4)), R(e, 5), R(e, 6)))
internal(0x100cf064, "StartEvent", lambda e, L: (L.lab(e.r(3)), R(e, 4), R(e, 5), R(e, 6), R(e, 7)), "StartEvent")
internal(0x100c6604, "FlushDeferredAnim", lambda e, L: (L.lab(e.r(3)), U(e, 4)))
internal(0x100c6670, "SetCurRideAnim", lambda e, L: (L.lab(e.r(3)), U(e, 4), U(e, 5), U(e, 6), fb(e.f(1)), U(e, 8)), "SetCurRideAnim")
internal(0x100cfc9c, "Push", lambda e, L: (L.lab(e.r(3)), R(e, 4)))
internal(0x100cfd44, "Pop", lab3, "Pop")
internal(0x100d6fec, "ExecuteTourSpecificCommand", lab3)
internal(0x100d78ec, "ExecuteBumperSpecificCommand", lab3)
internal(0x100d8104, "ExecuteCoasterSpecificCommand", lab3)
internal(0x101ffdc8, "FindDataNodeIndexByID", lambda e, L: (L.lab(e.r(3)), U(e, 4), U(e, 5)), "FindDataNodeIndexByID")
internal(0x10200230, "EnableRideHead", lambda e, L: (L.lab(e.r(3)), U(e, 4), U(e, 5)))
internal(0x1020039c, "DisableRideHead", lambda e, L: (L.lab(e.r(3)), U(e, 4)))
internal(0x100ceab0, "LIMBO_Put", lambda e, L: (L.lab(e.r(3)), U(e, 4), R(e, 5)), "LIMBO_Put")
internal(0x100ceb90, "LIMBO_Get", lab3, "LIMBO_Get")
internal(0x100cec58, "LIMBO_ForceGet", lab3, "LIMBO_ForceGet")
internal(0x100d42d0, "RSSE_KillScript", lambda e, L: (R(e, 3), R(e, 4)))
internal(0x100ce3d4, "BOUNCE_Put", lambda e, L: (L.lab(e.r(3)), U(e, 4), R(e, 5)), "BOUNCE_Put")
internal(0x100ce4ec, "BOUNCE_Get", lab3, "BOUNCE_Get")
internal(0x100ce610, "BOUNCE_ForceGet", lab3, "BOUNCE_ForceGet")
internal(0x100cdd4c, "WALKNODE_Trigger", lambda e, L: (L.lab(e.r(3)), U(e, 4)) + tuple(R(e, i) for i in range(5, 11)))
internal(0x100ce0f4, "WALKNODE_Release", lambda e, L: (L.lab(e.r(3)), U(e, 4)))
internal(0x100cdfc8, "WALKNODE_Get", lab3, "WALKNODE_Get")
internal(0x100ce054, "WALKNODE_Float", lambda e, L: (L.lab(e.r(3)), R(e, 4), R(e, 5), R(e, 6)))
internal(0x10064ab8, "EnableLight", lab3)
internal(0x10064b30, "DisableLight", lab3)
internal(0x10064854, "SetLightColourRelative", lambda e, L: (L.lab(e.r(3)), fb(e.f(1))))
internal(0x1006498c, "SetLightColourExplicit", lambda e, L: (L.lab(e.r(3)), fb(e.f(1)), fb(e.f(2)), fb(e.f(3))))
internal(0x100ccb30, "SCREAM_Start", lambda e, L: tuple(R(e, i) for i in range(3, 10)), "SCREAM_Start")
internal(0x100de0fc, "SOUND_FadeSoundEffect", lambda e, L: (R(e, 3),))
internal(0x100cd098, "SCREAM_SingleSad", lambda e, L: tuple(R(e, i) for i in range(3, 7)), "SCREAM_SingleSad")
internal(0x100ccde8, "SCREAM_Single", lambda e, L: tuple(R(e, i) for i in range(3, 9)), "SCREAM_Single")
internal(0x100cccc0, "SCREAM_SetLevel", lambda e, L: (R(e, 3), R(e, 4), R(e, 5)), "SCREAM_SetLevel")
internal(0x100cd190, "TriggerRepairParticleEffect", lambda e, L: (L.lab(e.r(3)), R(e, 4)))
internal(0x100dd9fc, "SOUND_SendEvent", lambda e, L: tuple(R(e, i) for i in range(3, 9)), "SOUND_SendEvent")
internal(0x100deb04, "SND_DipMusic", lambda e, L: (R(e, 3),))
internal(0x100cc888, "GetNodePos", lambda e, L: (L.lab(e.r(3)), L.lab(e.r(5)), R(e, 6), R(e, 7)), "GetNodePos")


def stub_create_object(e):
    e.cl.rec("CreateObject", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.pi["newobj"])


def stub_kill_object(e):
    e.cl.rec("KillObject", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))
    e.set_r(3, 0)


def stub_ride_from_id(e):
    e.cl.rec("GetRideFromID", (e.u32(e.r(3)),))
    e.set_r(3, e.pi["slot"])


def stub_current_anim(e):
    L = e.cl
    L.rec("GetCurrentAnim", (L.lab(e.r(3)), L.lab(e.r(5)), U(e, 6)))
    e.write(e.r(4), struct.pack(">I", L.ret("GetCurrentAnim.kind") & MASK))
    e.set_r(3, L.ret("GetCurrentAnim") & MASK)


def stub_assert(e):
    fmt = e.cstr(e.r(4))
    texts = (fmt, e.cstr(e.r(5))) if "%s" in fmt else (fmt,)
    e.cl.rec("GAME_ASSERTMSG", (R(e, 3),), texts)


def stub_init_script(e):
    L = e.cl
    L.rec("RSSE_InitScript", (U(e, 4) >> 16, R(e, 5)), (e.cstr(e.r(3)),))
    e.set_r(3, L.ret("RSSE_InitScript") & MASK)


def stub_script_pointer(e):
    L = e.cl
    L.rec("RSSE_GetScriptPointer", (R(e, 3),))
    which = L.ret("RSSE_GetScriptPointer")
    e.set_r(3, e.pi["others"][which - 1] if 1 <= which <= 3 else 0)


def stub_light_handle(e):
    L = e.cl
    L.rec("GetLightsourceHandle", (U(e, 3), U(e, 4)))
    e.set_r(3, e.pi["light"] if L.ret("GetLightsourceHandle") else 0)


def position(name, key, first):
    def fn(e):
        L = e.cl
        L.rec(name, first(e, L))
        for reg, axis in ((4, "x"), (5, "y"), (6, "z")):
            e.write(e.r(reg), struct.pack(">I", L.ret(key + "." + axis) & MASK))
        if name == "GetRideCentrePosition":
            e.set_r(3, L.ret(name) & MASK)
    return fn


STUBS.internal(0x100cee2c, stub_create_object)
STUBS.internal(0x100ceedc, stub_kill_object)
STUBS.internal(0x1002ae38, stub_ride_from_id)
STUBS.internal(0x100c7a00, stub_current_anim)
STUBS.internal(0x1000dde8, stub_assert)
STUBS.internal(0x100d37b4, stub_init_script)
STUBS.internal(0x100d668c, stub_script_pointer)
STUBS.internal(0x100647ec, stub_light_handle)
STUBS.internal(0x100cc778, position("GetRideCentrePosition", "GRCP", lambda e, L: (e.u32(e.r(3)),)))
STUBS.internal(0x100d2e60, position("GetRidePosition", "GRP", lambda e, L: (L.lab(e.r(3)),)))


def imp_labs(e):
    v = R(e, 3)
    e.cl.rec("labs", (v,))
    e.set_r(3, (-v if v < 0 else v) & MASK)


def raw(e, a):
    return e.cstr(a).encode("latin-1")


def imp_strcpy(e):
    src = raw(e, e.r(4))
    e.cl.rec("strcpy", (), (src.decode("latin-1"),))
    e.write(e.r(3), src + b"\0")


def imp_strcat(e):
    dst, src = raw(e, e.r(3)), raw(e, e.r(4))
    e.cl.rec("strcat", (), (dst.decode("latin-1"), src.decode("latin-1")))
    e.write(e.r(3) + len(dst), src + b"\0")


def imp_strcmp(e):
    a, b = raw(e, e.r(3)), raw(e, e.r(4))
    e.cl.rec("strcmp", (), (a.decode("latin-1"), b.decode("latin-1")))
    e.set_r(3, (-1 if a < b else 1 if a > b else 0) & MASK)


def imp_time(e):
    L = e.cl
    L.rec("time", ())
    now = L.ret("time") & MASK
    if e.r(3):
        e.write(e.r(3), struct.pack(">I", now))
    e.set_r(3, now)


def imp_localtime(e):
    e.cl.rec("localtime", (s32(e.u32(e.r(3))),))
    e.set_r(3, e.pi["tm"])


STUBS.imp("labs", imp_labs)
STUBS.imp("strcpy", imp_strcpy)
STUBS.imp("strcat", imp_strcat)
STUBS.imp("strcmp", imp_strcmp)
STUBS.imp("time", imp_time)
STUBS.imp("localtime", imp_localtime)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides

# The script's fields by Mac offset and size: w word, p a Ptr32 (compared as a block name), h s16, H u16, b byte.
PTRS = {0x00, 0x04, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38, 0xb0}
SPLIT = {0x6c: "hh", 0x88: "hh", 0xac: "Hbb", 0xb8: "bbbb", 0xc0: "hbb", 0xdc: "hh", 0xe4: "hbb"}
SIZE = {"w": 4, "p": 4, "h": 2, "H": 2, "b": 1}
FMT = {"w": "I", "p": "I", "h": "h", "H": "H", "b": "B"}


def script_fields():
    out = []
    for off in range(0, 0xf4, 4):
        if off in SPLIT:
            o = off
            for k in SPLIT[off]:
                out.append((o, k))
                o += SIZE[k]
        else:
            out.append((off, "p" if off in PTRS else "w"))
    return out


FIELDS = script_fields()
OBJ_FIELDS = [(0, "p"), (4, "p")] + [(o, "w") for o in range(8, 0x1c, 4)]

# The blocks of a case (name, bytes); the host's ride set, mesh, light and struct tm are the library's own.
BLOCKS = [("S", 0xf4), ("vars", 64), ("stack", 64), ("saves", 32), ("sblock", 64), ("name", 32), ("newobj", 0x1c),
          ("o0", 0x1c), ("o1", 0x1c), ("o2", 0x1c), ("A", 0xf4), ("B", 0xf4), ("C", 0xf4), ("Avars", 32), ("Bvars", 32),
          ("Cvars", 32), ("Ablock", 64), ("Bblock", 64), ("Cblock", 64), ("world", 16)]


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def put(self, a, kind, v):
        if kind == "h":
            v = T.s16(v)
        elif kind == "H":
            v &= 0xFFFF
        elif kind == "b":
            v &= 0xFF
        else:
            v &= MASK
        self.write(a, struct.pack(self.fmt[0] + FMT[kind], v))

    def get(self, a, kind):
        return struct.unpack(self.fmt[0] + FMT[kind], self.read(a, SIZE[kind]))[0]

    def make_blocks(self):
        self.b = {name: self.alloc(size) for name, size in BLOCKS}
        self.names = {addr: name for name, addr in self.b.items()}
        self.names[0] = None

    def fixed(self):
        """{label: address} of the log's objects."""
        out = {LAB[n]: self.b[n] for n in ("S", "o0", "o1", "o2", "newobj", "A", "B", "C", "world")}
        out[LAB["set"]], out[LAB["mesh"]], out[LAB["light"]] = self.set, self.mesh, self.light
        out[LAB["gClock"]] = self.g("gClock")
        return out

    def ptr_name(self, word):
        return self.names.get(self.unp32(word), "?%x" % word)


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make_blocks()
        self.set, self.mesh, self.light, self.tm, self.slot = (e.alloc(n) for n in (0x100, 0x40, 8, 36, 4))
        self.words(self.set + 8, [self.mesh])
        self.words(self.slot, [self.set])
        e.pi = dict(newobj=self.b["newobj"], slot=self.slot, light=self.light, tm=self.tm,
                    others=[self.b[n] for n in OTHERS])
        self.L = e.cl = CL.PpcLog({a: l for l, a in self.fixed().items()}, case["queues"])
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

    def native(self, a, addr):
        self.words(a, [addr])

    def p32(self, addr):
        return addr

    def unp32(self, word):
        return word

    def log(self):
        return self.L.log

    def call(self):
        T.call_ppc(self.e, FN, [self.b["S"]])


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_blocks()
        self.base = lib.tpw_pi_base()   # after the first allocation, which maps the arena
        self.set, self.mesh, self.light, self.tm = (self.g(n) for n in ("set", "mesh", "light", "tm"))
        lib.tpw_pi_blocks(self.b["A"], self.b["B"], self.b["C"], self.b["newobj"])
        CL.host_begin(lib, self.fixed(), case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_pi_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_pi_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def native(self, a, addr):
        self.write(a, struct.pack("<Q", addr))

    def p32(self, addr):
        return addr - self.base if addr else 0

    def unp32(self, word):
        return word + self.base if word else 0

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_pi_run(self.b["S"])


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_pi_addr.restype, lib.tpw_pi_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_pi_alloc.restype, lib.tpw_pi_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pi_base.restype, lib.tpw_pi_base.argtypes = C.c_uint64, []
    lib.tpw_pi_blocks.restype, lib.tpw_pi_blocks.argtypes = None, [C.c_uint64] * 4
    lib.tpw_pi_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pi_run.restype, lib.tpw_pi_run.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_pi_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def put_fields(s, base, spec, values):
    for off, kind in spec:
        v = values[off]
        if kind == "p":
            s.words(base + off, [s.p32(s.b[v]) if v else 0])
        else:
            s.put(base + off, kind, v)


def get_fields(s, base, spec):
    out = []
    for off, kind in spec:
        v = s.get(base + off, kind)
        out.append((off, s.ptr_name(v) if kind == "p" else v))
    return out


def build(s, case):
    """Writes the case's globals and blocks through Side.words: the same code for both memories (offsets are the Mac's)."""
    b = s.b
    order = case["list"]
    nexts = {name: (order[i + 1] if i + 1 < len(order) else None) for i, name in enumerate(order)}
    sf = dict(case["S"])
    sf[0x00] = nexts.get("S")
    put_fields(s, b["S"], FIELDS, sf)
    for name in OTHERS:
        f = dict(case[name])
        f[0x00] = nexts.get(name)
        put_fields(s, b[name], FIELDS, f)
        s.words(b[name + "vars"], case[name + "vars"])
        s.write(b[name + "block"], case["text"])
    s.words(b["vars"], case["vars"])
    s.words(b["stack"], case["stack"])
    s.words(b["saves"], case["saves"])
    s.write(b["sblock"], case["text"])
    s.write(b["name"], case["name"])
    objs = case["objs"]
    for i, words in enumerate(objs):
        f = {0: "o%d" % (i + 1) if i + 1 < len(objs) else None, 4: None}
        f.update({8 + 4 * k: w for k, w in enumerate(words)})
        put_fields(s, b["o%d" % i], OBJ_FIELDS, f)
    nf = {0: None, 4: None}
    nf.update({8 + 4 * k: w for k, w in enumerate(case["newobj"])})
    put_fields(s, b["newobj"], OBJ_FIELDS, nf)
    s.native(s.g("gRSSEState") + HOST_SCRIPTS, b[order[0]] if order else 0)
    s.native(s.g("gGameWorld"), b["world"])
    s.words(s.g("gRSSEF7BE4"), [case["f7be4"]])
    s.words(s.g("gSoundBanks") + 8, [case["bank"]])
    s.words(s.tm, case["tm"])


def observe(s, case):
    """Everything the call may change, read back the same way on both sides; pointers as block names."""
    b = s.b
    out = {"log": s.log(), "S": get_fields(s, b["S"], FIELDS), "vars": s.rd_words(b["vars"], 16),
           "stack": s.rd_words(b["stack"], 16), "saves": s.rd_words(b["saves"], 8),
           "f7be4": s.rd_words(s.g("gRSSEF7BE4"), 1)}
    for name in OTHERS:
        out[name] = get_fields(s, b[name], FIELDS)
        out[name + "vars"] = s.rd_words(b[name + "vars"], 8)
    for name in ("o0", "o1", "o2", "newobj"):
        out[name] = get_fields(s, b[name], OBJ_FIELDS)
    return out


def run(s, case):
    try:
        build(s, case)
        s.call()
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

TEXTS = {0: b"a", 8: b"bc", 16: b"coaster", 24: b"", 32: b"bc", 40: b"xyz"}
TEXT_OFFS = sorted(TEXTS)


def text_block():
    out = bytearray(64)
    for off, t in TEXTS.items():
        out[off:off + len(t)] = t
    return bytes(out)


QUEUE_KEYS = ["ReadScript", "ExtractData", "Read", "GetRand", "Pop", "SetCurRideAnim", "GetCurrentAnim", "GetCurrentAnim.kind",
              "FindDataNodeIndexByID", "StartEvent", "LIMBO_Put", "LIMBO_Get", "LIMBO_ForceGet", "RSSE_InitScript",
              "RSSE_GetScriptPointer", "BOUNCE_Put", "BOUNCE_Get", "BOUNCE_ForceGet", "WALKNODE_Get", "GetLightsourceHandle",
              "GRCP.x", "GRCP.y", "GRCP.z", "GetRideCentrePosition", "GRP.x", "GRP.y", "GRP.z", "SCREAM_Start", "SCREAM_SingleSad",
              "SCREAM_Single", "SCREAM_SetLevel", "SOUND_SendEvent", "GetNodePos", "time"]


def rint(r):
    return r.choice([0, 1, 2, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, r.randrange(0, 1000), r.getrandbits(32)])


def val(r):
    """An operand's value."""
    return r.choice([0, 1, -1, 2, 100, r.randrange(-100, 101), r.randrange(-2 ** 20, 2 ** 20), s32(r.getrandbits(32))])


def small(r):
    return r.choice([0, 1, -1, r.randrange(-100, 101), r.randrange(-2 ** 20, 2 ** 20)])


def var_word(r):
    return 0x40000000 | r.randrange(16)


def raw_word(r):
    """A word that names no variable (bit 30 clear: op 90 tests that bit alone)."""
    return r.choice([0, 1, r.randrange(0, 0x10000), r.randrange(0, 0x40000000), 0x80000000 | r.randrange(0, 0x40000000)])


def word(r):
    k = r.random()
    if k < 0.4:
        return var_word(r)
    if k < 0.6:
        return 0x20000000 | r.randrange(0, 0x1000)
    if k < 0.75:
        return 0x10000000 | r.choice(TEXT_OFFS)
    return raw_word(r)


def fbits(r):
    return fb(r.choice([0.0, -0.0, 0.5, -0.5, 1.5, -2.5, 1e6, -1e6, float(r.randrange(-1000, 1000)), r.uniform(-1e6, 1e6),
                        r.uniform(-2, 2)]))


def speed(r):
    while True:
        v = r.choice([0, 100, -49, -51, 50, r.randrange(-1000, 1001), r.randrange(-100, 300)])
        if v != -50:
            return v


def f31(sp):
    return T.f32(0.5 + T.f32(float(sp) / 100.0))


def fields_random(r):
    f = {}
    for off, kind in FIELDS:
        if kind == "p":
            f[off] = None
        elif kind == "w":
            f[off] = rint(r)
        elif kind == "h":
            f[off] = T.s16(rint(r))
        elif kind == "H":
            f[off] = rint(r) & 0xFFFF
        else:
            f[off] = rint(r) & 0xFF
    return f


def other_script(r, name, handle):
    f = fields_random(r)
    f[0x08] = handle
    f[0x1c] = name + "vars"
    f[0x34] = r.choice([name + "block"] * 4 + [None])
    f[0x74] = r.choice(TEXT_OFFS * 2 + [0xFFFFFFFF, 0x80000000])
    f[0x8c] = r.randrange(0, 9)
    return f


def base(r, op):
    """The world every case starts from: a random script, its tables, three other scripts and the fakes' queues."""
    S = fields_random(r)
    S[0x08] = r.randrange(1, 100)
    S[0x0c] = r.choice([0, r.randrange(1, 100)])
    S[0x10] = r.choice([0, r.randrange(1, 100)])
    S[0x1c] = "vars"
    S[0x20] = r.choice(["stack"] * 4 + [None])
    S[0x30] = r.choice(["saves"] * 4 + [None])
    S[0x34] = r.choice(["sblock"] * 4 + [None])
    S[0x38] = "name"
    S[0x3c] = r.choice([r.randrange(0, 2000)] * 12 + [-1, -10000])
    S[0x40] = r.randrange(-2, 12) & MASK
    S[0x44] = r.randrange(-2, 13) & MASK
    S[0x48] = val(r) & MASK
    S[0x4c] = r.randrange(0, 9)
    S[0x54] = r.randrange(-1, 11) & MASK
    S[0x74] = r.choice(TEXT_OFFS * 2 + [0xFFFFFFFF, 0x80000000, rint(r)])
    S[0x84] = r.choice([0, rint(r)])
    S[0x8c] = r.randrange(0, 17)
    S[0xa0] = r.choice([0, 0, rint(r)])
    S[0xa4] = r.choice([0, 0, rint(r)])
    S[0xa8] = r.choice([0xFFFF, rint(r)])
    S[0xbc] = r.choice([0, 0, r.randrange(0, 100), rint(r)])
    S[0xc0] = speed(r)
    S[0xc8] = r.choice([0, 0, r.randrange(1, 100), rint(r)])
    nobj = r.randrange(0, 4)
    S[0xb0] = "o0" if nobj else None
    objs = [[r.randrange(0, 11), rint(r), small(r) & MASK, rint(r), r.choice([1, 2, 3])] for _ in range(nobj)]
    order = [n for n in OTHERS + ["S"] if r.random() < (0.7 if n != "S" else 0.25)]
    r.shuffle(order)
    saves = [r.choice([0, 0, r.randrange(1, 50), rint(r)]) for _ in range(8)]
    q = {k: [rint(r)] for k in QUEUE_KEYS}
    q["ReadScript"] = [0x80000000 | op] + [word(r) for _ in range(6)]
    q["ExtractData"] = [val(r) for _ in range(8)]
    q["Read"] = [rint(r) for _ in range(3)]
    q["GetRand"] = [r.getrandbits(32) for _ in range(2)]
    q["SetCurRideAnim"] = [r.choice([0, 300, 599, 600, 601, r.randrange(0, 2 ** 20), (-r.randrange(1, 2 ** 20)) & MASK])]
    q["GetCurrentAnim"] = [r.choice([0, 4, 5, 3, rint(r)])]
    q["GetCurrentAnim.kind"] = [rint(r)]
    q["FindDataNodeIndexByID"] = [r.choice([0, 3, 0xFFFFFFFF, rint(r)]) for _ in range(3)]
    q["RSSE_InitScript"] = [r.choice([0, r.randrange(1, 100)])]
    q["RSSE_GetScriptPointer"] = [r.choice([0, 1, 2, 3])]
    q["GetLightsourceHandle"] = [r.choice([0, 1, 1])]
    for k in ("GRCP.x", "GRCP.y", "GRCP.z", "GRP.x", "GRP.y", "GRP.z"):
        q[k] = [fbits(r)]
    q["GetNodePos"] = [r.choice([0, 1]) for _ in range(2)]
    c = {"op": op, "queues": q, "S": S, "objs": objs, "list": order, "saves": saves,
         "vars": [rint(r) for _ in range(16)], "stack": [rint(r) for _ in range(16)], "text": text_block(),
         "name": b"ride/".ljust(32, b"\0"), "newobj": [rint(r) for _ in range(5)],
         "f7be4": rint(r), "bank": rint(r), "tm": [r.randrange(0, 100) for _ in range(9)]}
    for k, name in enumerate(OTHERS):
        c[name] = other_script(r, name, 200 + k)
        c[name + "vars"] = [rint(r) for _ in range(8)]
    return c


def shape(r, c):
    """The constraints of the case's instruction (see the docstring's "Not tested")."""
    op, S, q = c["op"], c["S"], c["queues"]
    ed = q["ExtractData"]
    rs = q["ReadScript"]
    sp = S[0xc0]
    if op in (16, 19, 21, 23, 26):
        # the answer stays below 2^20: its quotient by the speed stays below 2^31
        a = q["SetCurRideAnim"][0]
        if 2 ** 20 <= a < 2 ** 32 - 2 ** 20:
            q["SetCurRideAnim"] = [a & 0xFFFFF]
    if op == 17:
        a = 0 if S[0xc8] == 0 else q["SetCurRideAnim"][0]
        t = (a - 300) & MASK
        if f31(sp) > 0 and t / f31(sp) >= 2 ** 31 - 2 ** 10:
            S[0xc0] = r.choice([r.randrange(300, 1001), r.randrange(-1000, -50)])
    if op == 21 and ed[2] == 0:
        ed[2] = r.choice([1, -1, 1000, 7])
    if op == 44:
        c["queues"]["ExtractData"] = [small(r) for _ in ed]
    if op in (49, 50) and ed[0] == -2 ** 31 and ed[1] == -1:
        ed[1] = 3
    if op in (10, 11, 12):
        ed[0] = r.choice([0, 1, 2, 3])
    if op in (66, 67, 68, 69):
        ed[0] = r.randrange(0, 12)
        q["RSSE_GetScriptPointer"] = [r.choice([1, 2, 3])]
    if op == 63:
        q["RSSE_GetScriptPointer"] = [r.choice([1, 2, 3])]
    if op in (63, 64):
        S[0x34] = "sblock"
    if op in (91, 92):
        ed[1] = r.randrange(-3, 12)
    if op == 93:
        ed[0] = r.choice([0, 0, 1, -1, 5])
    if op == 28 and (rs[2] & 0xFFFF) == 0xFFFF:
        rs[2] ^= 1
    if op == 18 and r.random() < 0.4:
        S[0xa8] = (ed[0] + (ed[1] << 16)) & MASK
    if op in (19, 26) and S[0xbc] and r.random() < 0.5:
        if S[0xc8] == 0:
            S[0xbc] = (rs[3] + 1) & MASK
        else:
            q["GetCurrentAnim.kind"] = [(S[0xbc] - 1) & MASK]
    if op in (17, 24, 44, 45) and S[0xa0] and r.random() < 0.5:
        q["Read"] = [r.choice([S[0xa0], S[0xa0] - 1, S[0xa0] + 1]) & MASK]
    if op == 46 and S[0xa4] and r.random() < 0.5:
        q["Read"] = [r.choice([S[0xa4], S[0xa4] - 1, S[0xa4] + 1]) & MASK]
    if op == 56:
        cnt = S[0x4c]
        free = [i for i in range(cnt) if c["saves"][i] == 0]
        if free:
            z = r.choice(free)
            q["GetRand"] = [r.getrandbits(32) for _ in range(r.randrange(0, 3))] + [2 * z]
    if op == 57 and r.random() < 0.6:
        ed[0] = s32(r.choice(c["saves"]))
    if op == 90:
        if S[0x74] not in TEXT_OFFS and S[0x74] < 0x80000000:
            S[0x74] = r.choice(TEXT_OFFS)
        rs[1] =0x10000000 | r.choice(TEXT_OFFS) if r.random() < 0.8 else rs[1]
        if rs[2] & 0x40000000:
            rs[2] = var_word(r)
    return c


def gen(r, op=None):
    if op is None:
        op = r.randrange(0, 0x6a)
    c = shape(r, base(r, op))
    k = r.random()
    if k < 0.03:
        c["queues"]["ReadScript"][0] = r.choice([0, 0x40000003, 0x7F000000 | op, 0x81000000]) & MASK
    elif k < 0.06:
        c["queues"]["ReadScript"][0] = 0x80000000 | r.choice([0x6a, 0x6b, 0xFF, 0xFFFFFF])
    return c


def edges():
    """Every instruction many times with its branches (ride or none, waits running or not, NULL tables), the bad words and the dead
    script; then the boundaries: the clock exactly at a wait's end, the animation lengths 599, 600 and 601, the remote index at the count."""
    r = random.Random(5882)
    for word0 in (0, 0x40000000, 0x7FFFFFFF, 0x80000069, 0x8000006a, 0x80FFFFFF, 0x81000000, 0xFFFFFFFF):
        c = gen(r, 0)
        c["queues"]["ReadScript"][0] = word0
        yield c
    c = gen(r, 1)
    c["S"][0x3c] = -1
    yield c
    for op in range(0x6a):
        for _ in range(14):
            yield gen(r, op)
    for op in (16, 19, 21, 23, 24, 26):
        for a in (599, 600, 601, 0):
            c = gen(r, op)
            c["S"][0xc8] = 5
            c["S"][0xbc] = 0
            c["S"][0xa0] = 0
            c["queues"]["SetCurRideAnim"] = [a]
            yield c
    for op in (17, 24, 44, 45, 46):
        for d in (0, -1, 1):
            c = gen(r, op)
            c["S"][0xa0] = c["S"][0xa4] = 1000
            c["queues"]["Read"] = [1000 + d]
            yield c
    for op in (66, 67, 68, 69, 91, 92):
        for i in (0, 1, 7, 8):
            c = gen(r, op)
            c["S"][0x0c] = c["S"][0x10] = 9
            c["queues"]["ExtractData"][0 if op < 91 else 1] = i
            for name in OTHERS:
                c[name][0x8c] = 8
            yield c
    for vc in (5, 6):
        c = gen(r, 93)
        c["S"][0x8c] = vc
        c["queues"]["ExtractData"][0] = 0
        yield c


TESTS = [FnTest("ProcessInstruction", ppc, host, edge=edges, rnd=gen)]
