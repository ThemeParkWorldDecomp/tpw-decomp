"""Script objects of the ride scripts (unit w150), against libtpw_unit_RideObjects.dylib (tests/unit_RideObjects.cpp), main app
("SimTheme Park"), src/game/rides/RideEffects.cpp, src/game/rides/rsse.cpp:
  ExtractData 0x100cece4, KillLinkedObject 0x100ced44, CreateObject 0x100cee2c, KillObject 0x100ceedc, SetObjParam 0x100cefb0,
  StartEvent 0x100cf064, InitObject 0x100cf6c8, ProcessObjects 0x100cf884, ReadScript 0x100cfbf8, Push 0x100cfc9c, Pop 0x100cfd44,
  GetRidePosition 0x100d2e60, GetRideCentrePosition 0x100cc778, RSSE_Setvar 0x100d6734, RSSE_SetRideSpeed 0x100d6ac0,
  RSSE_GetRideSpeed 0x100d6b48, RSSE_GetTourBaseID 0x100d6bd0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (GetNodePos, the particle and sound calls, the ride database and its info, the data node and matrix helpers, the ride lookups, the
assert and the trace) are fakes on both sides that record their arguments and answer from the case's queues; LbMem_Alloc and LbMem_Free are
the emulator's allocator on the Mac side and the arena on the host side (not logged: the object list is compared instead).

The world of a case: a script record (filled with random words, the fields the functions read set), its code, variable and stack arrays, a
list of objects (some unlinked "strays"), four ride sets (node, embedded records and their 0x40-byte blocks), six thing rows, the sound
banks, the system state words, the live object count and a 16-word matrix. What is compared: the answer, the script fields and arrays, the
object list (by label, new objects by order of appearance), the globals, a scratch block of four words (the float outputs) and the log.

Fused operations: the floats of StartEvent and ProcessObjects run under HardwareRounding over the unit's own functions (fmuls, fctiwz, fneg).
Not tested: a NULL object where the original reads it unconditionally (KillLinkedObject, SetObjParam, InitObject), a NULL ride database
answer, a thing row without a thing, ReadScript with a NULL program (reads through it), a node index past the eight records, a ride slot without a set.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_RideObjects"
MASK = 0xFFFFFFFF
KEEP = 0x5A5A5A5A

FN = dict(ExtractData=0x100CECE4, KillLinkedObject=0x100CED44, CreateObject=0x100CEE2C, KillObject=0x100CEEDC,
          SetObjParam=0x100CEFB0, StartEvent=0x100CF064, InitObject=0x100CF6C8, ProcessObjects=0x100CF884,
          ReadScript=0x100CFBF8, Push=0x100CFC9C, Pop=0x100CFD44, GetRidePosition=0x100D2E60, GetRideCentrePosition=0x100CC778,
          RSSE_Setvar=0x100D6734, RSSE_SetRideSpeed=0x100D6AC0, RSSE_GetRideSpeed=0x100D6B48, RSSE_GetTourBaseID=0x100D6BD0)
SZ = dict(ExtractData=48, KillLinkedObject=164, CreateObject=128, KillObject=152, SetObjParam=116, StartEvent=1584, InitObject=380,
          ProcessObjects=832, ReadScript=116, Push=124, Pop=136, GetRidePosition=44, GetRideCentrePosition=220, RSSE_Setvar=120,
          RSSE_SetRideSpeed=80, RSSE_GetRideSpeed=80, RSSE_GetTourBaseID=80)
SIZES = {FN[n]: SZ[n] for n in FN}
OWN = set(SIZES)
# the kind of each answer: an int (r3), a pointer (object label) or nothing (void)
KIND = dict(ExtractData="int", KillLinkedObject="void", CreateObject="ptr", KillObject="ptr", SetObjParam="void", StartEvent="int",
            InitObject="void", ProcessObjects="void", ReadScript="int", Push="void", Pop="int", GetRidePosition="void",
            GetRideCentrePosition="int", RSSE_Setvar="void", RSSE_SetRideSpeed="void", RSSE_GetRideSpeed="int",
            RSSE_GetTourBaseID="int")

# ---- the callees faked on both sides
A_ASSERT, A_CTRACE, A_GETNODEPOS = 0x1000DDE8, 0x1006FCBC, 0x100CC888
A_LIFE, A_INIT, A_LAUNCH, A_SCALE, A_POS, A_VEC = 0x100BE2E8, 0x100BEC30, 0x100BE7E4, 0x100BE36C, 0x100BE150, 0x100BE070
A_SETPARAM, A_SENDEV, A_MOVE, A_STOP, A_FADE = 0x100DD380, 0x100DD9FC, 0x100DDF60, 0x100DE058, 0x100DE0FC
A_GETINFO, A_PARTEFF, A_SNDBANK, A_FINDNODE = 0x10154868, 0x10156DB0, 0x10156DF8, 0x101FFDC8
A_NORM, A_GDNWM, A_GRFI, A_GRMBA = 0x1002AD34, 0x1002ADDC, 0x1002AE38, 0x101ED058
A_ALLOC, A_FREE = 0x10238518, 0x10238548

ADDR = dict(F7BE0=0x102F7BE0, STATE=0x102F7BEC, BANKS=0x102F7C94, RIDEDB=0x1037F6C8, RIDESETS=0x102C79A4, THINGS=0x1034CBFC)
HOST_LAYOUT = [0xF4, 0x1C, 0x18, 0xB0, 0x20, 0xC0, 0x18, 4, 0xE, 0x78]

L_SCRIPT, L_INFO, L_RIDEDB = 1, 60, 61


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def u32at(e, a):
    return struct.unpack(">I", e.read(a, 4))[0]


def stub_ctrace(e):
    L = e.cl
    L.rec("CTRACE", (s32(e.r(3)), s32(e.r(5))), (e.cstr(e.r(4))[:159],))


def stub_getnodepos(e):
    L = e.cl
    script, pos, dire, node, mask = e.r(3), e.r(4), e.r(5), s32(e.r(6)), s32(e.r(7))
    L.rec("GetNodePos", (L.lab(script), int(pos != 0), int(dire != 0), node, mask))
    ans = L.ret("GetNodePos", 1) & MASK
    if ans:
        e.write(pos, b"".join(struct.pack(">I", L.ret("gp%d" % i) & MASK) for i in range(3)))
        if dire:
            e.write(dire, b"".join(struct.pack(">I", L.ret("gd%d" % i) & MASK) for i in range(3)))
    e.set_r(3, ans)


def stub_getinfo(e):
    L = e.cl
    L.rec("GetRideInfo", (L.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, L.dyn["INFO"] if L.ret("GetRideInfo", 1) != 0 else 0)


def stub_norm(e):
    L = e.cl
    v = e.r(3)
    L.rec("NormaliseVector", tuple(u32at(e, v + 4 * i) for i in range(3)))
    for i in range(3):
        x = L.ret("nv%d" % i, KEEP)
        if x != KEEP:
            e.write(v + 4 * i, struct.pack(">I", x & MASK))


def stub_gdnwm(e):
    L = e.cl
    mesh, idx = u32at(e, e.r(3)), u32at(e, e.r(4))
    L.rec("GetDataNodeWorldMatrix", (L.lab(mesh), idx))
    flags = L.ret("gdnwm_flag", KEEP)
    if flags != KEEP:
        recs = u32at(e, u32at(e, mesh + 0x28) + 4)
        e.write(recs + idx * 0x14, struct.pack(">I", flags & MASK))
    e.set_r(3, 0 if L.ret("gdnwm", -1) < 0 else L.dyn["MATRIX"])


def stub_grfi(e):
    L = e.cl
    L.rec("GetRideFromID", (u32at(e, e.r(3)),))
    e.set_r(3, ADDR["RIDESETS"] + 4 * L.ret("grfi", 1))


def stub_grmba(e):
    L = e.cl
    L.rec("GetRideMasterBoundArea", (e.r(3),))
    ans = L.ret("grmba", 0) & MASK
    if ans:
        box = e.r(4)
        bits = [L.ret("bx%d" % i) for i in range(3)]
        for i in range(3):
            bits.append(L.ret("by%d" % i))
        # host order: min[i] from bx<i>, max[i] from by<i> (interleaved reads, same queues)
        e.write(box, b"".join(struct.pack(">I", bits[i] & MASK) for i in range(3)) +
                b"".join(struct.pack(">I", bits[3 + i] & MASK) for i in range(3)))
    e.set_r(3, ans)


def glue_toc(e):
    """The cross-TOC glue the stubs stand for saves r2 at 0x14(r1); the caller reloads it afterwards."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))


def stub_alloc(e):
    glue_toc(e)
    e.set_r(3, e.alloc(e.r(3)))


STUBS.internal(A_ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(A_CTRACE, stub_ctrace)
STUBS.internal(A_GETNODEPOS, stub_getnodepos)
STUBS.internal(A_LIFE, CL.recorder("PAR_SetEmitterLife", "ii"))
STUBS.internal(A_INIT, CL.recorder("PAR_InitEmitter", "iiii", "i"))
STUBS.internal(A_LAUNCH, CL.recorder("PAR_InitEmitterLaunch", "iiiiiii", "i"))
STUBS.internal(A_SCALE, CL.recorder("PAR_SetEmitterScale", "ii"))
STUBS.internal(A_POS, CL.recorder("PAR_SetEmitterPos", "iiii"))
STUBS.internal(A_VEC, CL.recorder("PAR_SetEmitterLaunchVector", "iiii"))
STUBS.internal(A_SETPARAM, CL.recorder("SOUND_SetParamValue", "iuu", "i"))
STUBS.internal(A_SENDEV, CL.recorder("SOUND_SendEvent", "iiiiii", "i"))
STUBS.internal(A_MOVE, CL.recorder("SOUND_MoveSoundEffect", "iiii", "i"))
STUBS.internal(A_STOP, CL.recorder("SOUND_StopSoundEffect", "i"))
STUBS.internal(A_FADE, CL.recorder("SOUND_FadeSoundEffect", "i"))
STUBS.internal(A_GETINFO, stub_getinfo)
STUBS.internal(A_PARTEFF, CL.recorder("RideInfo::GetCustomParticleEffect", "Li", "i"))
STUBS.internal(A_SNDBANK, CL.recorder("RideInfo::GetCustomSoundBank", "L", "i"))
STUBS.internal(A_FINDNODE, CL.recorder("FindDataNodeIndexByID", "Luu", "i"))
STUBS.internal(A_NORM, stub_norm)
STUBS.internal(A_GDNWM, stub_gdnwm)
STUBS.internal(A_GRFI, stub_grfi)
STUBS.internal(A_GRMBA, stub_grmba)
STUBS.internal(A_ALLOC, stub_alloc)
STUBS.internal(A_FREE, glue_toc)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["RIDEDB"]: L_RIDEDB}, case["queues"])
        self.dyn = self.L.dyn = {"INFO": e.alloc(0x40), "MATRIX": e.alloc(0x40)}
        self.L.labels[self.dyn["INFO"]] = L_INFO
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return self.dyn.get(name) or ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def label(self, a, n):
        self.L.labels[a] = n

    def p(self, a):
        return a

    def up(self, w):
        return w

    def set_thing(self, row, a):
        self.words(ADDR["THINGS"] + 0x14 * row, [a])

    def make_ride(self, slot, spec):
        node = self.alloc(0x50)
        self.words(node, spec["node"])
        header = self.alloc(0x80)
        self.words(header + 0x78, [node])
        recs = self.alloc(0x14 * 8)
        for i, (flags, has, blk) in enumerate(spec["recs"]):
            b = 0
            if has:
                b = self.alloc(0x40)
                self.words(b, blk)
            self.words(recs + 0x14 * i, [flags, b, 0, 0, 0])
        emb = self.alloc(0x10)
        self.words(emb, [0, recs])
        mesh = self.alloc(0x40)
        self.words(mesh + 4, [header])
        self.words(mesh + 0x28, [emb])
        st = self.alloc(0x10)
        self.words(st + 8, [mesh])
        self.words(ADDR["RIDESETS"] + 4 * slot, [st])
        return st, mesh

    def call(self, op, args):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN[op], [a & MASK for a, _p in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_ro_base()
        CL.host_begin(lib, {L_RIDEDB: self.g("RIDEDB"), L_INFO: self.g("INFO")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ro_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ro_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def label(self, a, n):
        self.lib.tpw_cl_label(n, a)

    def p(self, a):
        return a - self.base if a else 0

    def up(self, w):
        return self.base + w if w else 0

    def set_thing(self, row, a):
        self.lib.tpw_ro_thing(row, a)

    def make_ride(self, slot, spec):
        d = list(spec["node"])
        d.append(len(spec["recs"]))
        for flags, has, blk in spec["recs"]:
            d.append(flags)
            d.append(1 if has else 0)
            d.extend(blk)
        arr = (C.c_int64 * len(d))(*d)
        out = (C.c_uint64 * 3)()
        self.lib.tpw_ro_ride(slot, arr, out)
        return out[0], out[1]

    def call(self, op, args):
        f = getattr(self.lib, "tpw_ro_" + op)
        a = [(x if p else x & MASK) for x, p in args] + [0] * (5 - len(args))
        v = f(*a)
        return v if KIND[op] == "ptr" else v & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ro_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "base").restype = C.c_uint64
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ro_thing.argtypes = [C.c_uint32, C.c_uint64]
    lib.tpw_ro_ride.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_uint64)]
    lib.tpw_cl_label.argtypes = [C.c_int32, C.c_void_p]
    for n in FN:
        f = getattr(lib, p + n)
        f.restype, f.argtypes = C.c_uint64, [C.c_uint64] * 5
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

PTR_WORDS = (6, 7, 8, 44)          # code, vars, stack, objects
HALF_AT = (0xAC, 0xAE, 0xC0, 0xC2)  # halfword fields (thing id and its neighbour, speed and its neighbour)
HALF_WORDS = (0xAC // 4, 0xC0 // 4)


def build(s, c):
    env = {"sets": [], "meshes": []}
    g = c["g"]
    s.words(s.g("F7BE0"), [g["f7be0"]])
    s.words(s.g("STATE"), [g["init"], g["f04"]])
    s.words(s.g("BANKS"), g["banks"])
    for row in range(6):
        t = g["things"][row]
        if t is None:
            s.set_thing(row, 0)
        else:
            b = s.alloc(0x20)
            s.half(b + 0xE, t)
            s.set_thing(row, b)
    s.words(s.g("MATRIX"), c["matrix"])
    for slot in range(4):
        st, mesh = s.make_ride(slot, c["rides"][slot])
        env["sets"].append(st)
        env["meshes"].append(mesh)
        s.label(st, 10 + slot)
        s.label(mesh, 20 + slot)
    sc = c["sc"]
    script = s.alloc(0xF4)
    env["script"] = script
    s.label(script, L_SCRIPT)
    words = list(c["fill"])
    arrs = {}
    for name, idx in (("code", 6), ("vars", 7), ("stack", 8)):
        if sc[name]:
            a = s.alloc(32)
            s.words(a, c["arr"][name])
            arrs[name] = a
            words[idx] = s.p(a)
        else:
            words[idx] = 0
    env["arrs"] = arrs
    objs = []
    for i, o in enumerate(c["objs"]):
        a = s.alloc(0x1C)
        objs.append(a)
        s.label(a, 30 + i)
    env["objs"] = objs
    linked = [i for i, o in enumerate(c["objs"]) if o["linked"]]
    for i, o in enumerate(c["objs"]):
        nxt = prv = 0
        if o["linked"]:
            k = linked.index(i)
            nxt = objs[linked[k + 1]] if k + 1 < len(linked) else 0
            prv = objs[linked[k - 1]] if k > 0 else 0
        s.words(objs[i], [s.p(nxt), s.p(prv), o["type"], o["handle"], o["nodeid"], o["nodeindex"], o["f18"]])
    words[44] = s.p(objs[linked[0]]) if linked else 0
    for name, off in (("handle", 8), ("state", 0x3C), ("sidx", 0x40), ("pw", 0x50), ("ss", 0x54), ("vc", 0x8C), ("fly", 0x9C),
                      ("ride", 0xC8)):
        words[off // 4] = sc[name]
    s.words(script, words)
    s.half(script + 0xAC, sc["thing"])
    s.half(script + 0xAE, sc["f_ae"])
    s.half(script + 0xC0, sc["speed"])
    s.half(script + 0xC2, sc["f_c2"])
    scr = s.alloc(0x20)
    s.words(scr, c["scratch"])
    env["scr"] = scr
    return env


def resolve(s, env, x):
    """(value, is_pointer)"""
    if isinstance(x, tuple):
        k = x[0]
        if k == "null":
            return 0, True
        if k == "sc":
            return env["script"], True
        if k == "obj":
            return (0, True) if x[1] < 0 else (env["objs"][x[1]], True)
        if k == "set":
            return env["sets"][x[1]], True
        if k == "scr":
            return env["scr"] + x[1], True
    return x, False


def execute(s, c, env):
    args = [resolve(s, env, x) for x in c["args"]]
    return s.call(c["op"], args)


class Namer:
    def __init__(self, s, env):
        self.known = {a: i for i, a in enumerate(env["objs"])}
        self.names = {}

    def __call__(self, a):
        if a == 0:
            return 0
        if a in self.known:
            return 1 + self.known[a]
        return self.names.setdefault(a, 100 + len(self.names))


def observe(s, c, env, answer):
    script = env["script"]
    w = s.rd_words(script, 0x3D)
    sc = []
    for i, v in enumerate(w):
        if i in PTR_WORDS or i in HALF_WORDS:
            continue
        sc.append(v)
    sc += [s.rd_half(script + o) for o in HALF_AT]
    sc += [int(w[i] != 0) for i in (6, 7, 8)]
    nm = Namer(s, env)
    head = s.up(w[44])
    out = []
    a, seen = head, 0
    hn = nm(head)
    while a and seen < 12:
        ow = s.rd_words(a, 7)
        nx, pv = s.up(ow[0]), s.up(ow[1])
        if a in nm.known:
            out.append((nm(a), nm(pv), nm(nx)) + tuple(ow[2:]))
        else:
            out.append((nm(a), nm(pv), nm(nx)))
        a, seen = nx, seen + 1
    arrs = {k: s.rd_words(a, 8) for k, a in env["arrs"].items()}
    kind = KIND[c["op"]]
    if kind == "ptr":
        ans = nm(answer if s.__class__ is Ppc else answer)
    elif kind == "void":
        ans = None
    else:
        ans = answer
    scr = s.rd_words(env["scr"], 8)
    scr = scr[:1] + [canon_f(v) for v in scr[1:4]] + scr[4:]
    return {"answer": ans, "log": s.log(), "sc": sc, "head": hn, "list": out, "arrs": arrs,
            "g": s.rd_words(s.g("F7BE0"), 1) + s.rd_words(s.g("STATE"), 2) + s.rd_words(s.g("BANKS"), 8), "scr": scr}


def run(s, c):
    try:
        env = build(s, c)
        return observe(s, c, env, execute(s, c, env))
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
    if k < 0.06:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-3000, 3000))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def ri(r):
    return r.choice([0, 1, 2, 7, 100, r.randrange(0, 1000), r.getrandbits(32)])


def mk_queues(r):
    q = {"GetNodePos": [r.choice([1, 1, 1, 0]) for _ in range(r.randrange(1, 4))]}
    for k in ("gp0", "gp1", "gp2", "gd0", "gd1", "gd2"):
        q[k] = [rf(r) for _ in range(r.randrange(1, 3))]
    for k in ("bx0", "bx1", "bx2", "by0", "by1", "by2"):
        q[k] = [rf(r)]
    q["GetRideInfo"] = [1]
    q["RideInfo::GetCustomParticleEffect"] = [ri(r)]
    q["RideInfo::GetCustomSoundBank"] = [r.choice([0, r.randrange(1, 50), ri(r)])]
    q["FindDataNodeIndexByID"] = [r.randrange(0, 8) for _ in range(2)]
    for k in ("PAR_InitEmitter", "PAR_InitEmitterLaunch", "SOUND_SetParamValue", "SOUND_SendEvent", "SOUND_MoveSoundEffect"):
        q[k] = [r.choice([0, r.randrange(1, 100), ri(r)]) for _ in range(2)]
    for i in range(3):
        q["nv%d" % i] = [r.choice([KEEP, rf(r)])]
    q["gdnwm_flag"] = [r.choice([KEEP, KEEP, 0, 0x10, 0x11, r.getrandbits(32)])]
    q["gdnwm"] = [r.choice([-1, 0, 0])]
    q["grfi"] = [r.randrange(0, 4)]
    q["grmba"] = [r.choice([0, 1, 1])]
    return q


def mk_ride(r):
    node = [r.getrandbits(32) for _ in range(20)]
    for i in (16, 17, 18):
        node[i] = rf(r)
    recs = []
    for _ in range(8):
        blk = [r.getrandbits(32) for _ in range(16)]
        for i in (12, 13, 14):
            blk[i] = rf(r)
        recs.append((r.choice([0, 0x10, 0x01, 0x11, r.getrandbits(32)]), r.random() < 0.8, blk))
    return {"node": node, "recs": recs}


def mk_obj(r, linked=True, typ=None):
    return {"type": r.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 11, -1]) if typ is None else typ,
            "handle": r.choice([0, 1, 2, 3, 8, r.randrange(0, 100), r.getrandbits(32)]),
            "nodeid": r.choice([-1, 0, 1, 2, 5, r.randrange(-3, 40)]), "nodeindex": r.randrange(0, 8), "f18": ri(r), "linked": linked}


def base(r, op):
    c = {"op": op, "queues": mk_queues(r)}
    c["rides"] = [mk_ride(r) for _ in range(4)]
    c["matrix"] = [rf(r) for _ in range(16)]
    c["fill"] = [r.getrandbits(32) for _ in range(0x3D)]
    c["sc"] = {"handle": r.randrange(0, 40), "state": r.randrange(-2, 10), "sidx": r.randrange(-2, 10), "pw": r.randrange(0, 9),
               "ss": r.randrange(0, 9), "vc": r.randrange(0, 9), "fly": ri(r), "ride": r.randrange(0, 4),
               "thing": r.randrange(1, 6), "f_ae": r.getrandbits(16), "speed": r.getrandbits(16), "f_c2": r.getrandbits(16),
               "code": True, "vars": True, "stack": True}
    c["arr"] = {k: [ri(r) for _ in range(8)] for k in ("code", "vars", "stack")}
    things = [None] + [r.getrandbits(16) for _ in range(5)]
    c["g"] = {"f7be0": r.choice([0, 1, 5, 20, -1]), "init": r.choice([1, 1, 1, 0]), "f04": r.randrange(0, 40),
              "banks": [r.choice([0, 1, 2, r.randrange(1, 90), ri(r)]) for _ in range(8)], "things": things}
    c["objs"] = [mk_obj(r) for _ in range(r.randrange(0, 4))]
    c["scratch"] = [r.choice([0, 1, 2, 3]), 0x11111111, 0x22222222, 0x33333333, 0x44444444, 0, 0, 0]
    c["args"] = []
    return c


def sset(c, **kw):
    """Overrides: sc_state=3 sets c["sc"]["state"]; g_init, q_name (via dict) are applied by the callers."""
    for k, v in kw.items():
        sect, key = k.split("_", 1)
        if sect == "sc":
            c["sc"][key] = v
        elif sect == "g":
            c["g"][key] = v
    return c


def need_obj(r, c, n=1, typ=None):
    while len(c["objs"]) < n:
        c["objs"].append(mk_obj(r))
    if typ is not None:
        for o in c["objs"]:
            o["type"] = typ


def want_row(c, row0):
    """The thing row the script uses is present; row 0 holds a thing when row0."""
    sc = c["sc"]
    if row0 or sc["thing"] == 0:
        c["g"]["things"][0] = 0x1234
    if c["g"]["things"][sc["thing"]] is None:
        c["g"]["things"][sc["thing"]] = 0x4321


def case(r, op, args, row0=False, **ov):
    c = base(r, op)
    sset(c, **ov)
    c["args"] = args
    want_row(c, row0)
    return c


S = ("sc",)
NULL = ("null",)
DICT_OV = lambda c, **q: c["queues"].update(q)


# ---- ExtractData
def ed_edges():
    r = random.Random(1501)
    for w in (0x40000000, 0x40000005, 0x40000007, 0x41000000 - 0x01000000 + 3, 0x0000FFFF, 0x00008000, 0xFFFF8000, 0x00007FFF,
              0x12340001, 0x40FFFFFF - 0xFFFFF8, 0x41000003, 0x3F000003, 0x80000003, 0x00000000):
        yield case(r, "ExtractData", [S, w])


def ed_random(r):
    w = r.choice([0x40000000 | r.randrange(0, 8), r.getrandbits(32), r.getrandbits(16), (r.getrandbits(8) << 24) | r.randrange(0, 8)])
    return case(r, "ExtractData", [S, w])


# ---- CreateObject
def co_edges():
    r = random.Random(1502)
    for n in (0, 1, 2, 3):
        c = case(r, "CreateObject", [S])
        c["objs"] = [mk_obj(r) for _ in range(n)]
        yield c


def co_random(r):
    return case(r, "CreateObject", [S])


# ---- KillObject
def ko_edges():
    r = random.Random(1503)
    for n in (1, 2, 3, 4):
        for target in list(range(n)) + [-1, n]:
            c = case(r, "KillObject", [S, ("obj", target)])
            c["objs"] = [mk_obj(r) for _ in range(n)] + ([mk_obj(r, linked=False)] if target == n else [])
            yield c
    c = case(r, "KillObject", [S, NULL])
    c["objs"] = []
    yield c


def ko_random(r):
    c = case(r, "KillObject", [S, ("obj", 0)])
    need_obj(r, c, 1)
    c["args"] = [S, ("obj", r.randrange(-1, len(c["objs"])))]
    return c


# ---- KillLinkedObject / SetObjParam
def kl_edges():
    r = random.Random(1504)
    for t in list(range(-1, 13)):
        for fade in (0, 1, 5, -1):
            c = case(r, "KillLinkedObject", [S, ("obj", 0), fade])
            c["objs"] = [mk_obj(r, typ=t)]
            yield c


def kl_random(r):
    c = case(r, "KillLinkedObject", [S, ("obj", 0), r.choice([0, 1, r.getrandbits(32)])])
    need_obj(r, c, 1)
    return c


def sp_edges():
    r = random.Random(1505)
    for t in list(range(-1, 13)):
        for pv in ((0, 0), (5, 7), (-1, 0x7FFFFFFF)):
            c = case(r, "SetObjParam", [S, ("obj", 0), pv[0], pv[1]])
            c["objs"] = [mk_obj(r, typ=t)]
            yield c


def sp_random(r):
    c = case(r, "SetObjParam", [S, ("obj", 0), ri(r), ri(r)])
    need_obj(r, c, 1)
    return c


# ---- StartEvent
def se_args(r, typ):
    a = r.choice([0, 1, 5, 0x7FFF, 0x8000, 0x8005, 0xFFFF, 0x12345678, -1, r.getrandbits(32)])
    b = r.choice([1000, 1000, 0, 7, -1, r.getrandbits(32)])
    return [S, typ, r.choice([0, 1, 2, -1, r.randrange(0, 50)]), a, b]


def se_edges():
    r = random.Random(1506)
    for typ in list(range(-1, 14)) + [0x80000000, 0x7FFFFFFF]:
        for gnp in (0, 1):
            for row0 in (False, True):
                for bank in (0, 3):
                    c = case(r, "StartEvent", se_args(r, typ), row0=row0)
                    c["queues"]["GetNodePos"] = [gnp]
                    c["queues"]["RideInfo::GetCustomSoundBank"] = [bank]
                    yield c
    for a in (0, 5, 0x8000, 0x8003, 0xFFFF):
        for b in (1000, 999, 1001):
            for typ in (1, 2):
                c = case(r, "StartEvent", [S, typ, 3, a, b])
                c["queues"]["GetNodePos"] = [1]
                yield c


def se_random(r):
    return case(r, "StartEvent", se_args(r, r.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 0, 11, -1, r.getrandbits(32)])),
                row0=r.random() < 0.3)


# ---- InitObject
def io_args(r):
    return [S, ("obj", 0), r.choice([0, 5, 0x8000, 0x8004, r.getrandbits(32)]), r.choice([1000, 3, 0]), ri(r)]


def io_edges():
    r = random.Random(1507)
    for typ in list(range(-1, 13)):
        for n, target in ((1, 0), (2, 0), (2, 1), (3, 1), (3, 2)):
            for gnp in (0, 1):
                for h in (0, 5):
                    c = case(r, "InitObject", io_args(r), row0=(typ == 10 and h == 5))
                    c["objs"] = [mk_obj(r) for _ in range(n)]
                    c["objs"][target]["type"] = typ
                    c["args"][1] = ("obj", target)
                    c["queues"]["GetNodePos"] = [gnp]
                    for k in ("PAR_InitEmitter", "PAR_InitEmitterLaunch", "SOUND_SendEvent"):
                        c["queues"][k] = [h]
                    c["queues"]["RideInfo::GetCustomSoundBank"] = [r.choice([0, 3])]
                    yield c


def io_random(r):
    c = case(r, "InitObject", io_args(r), row0=r.random() < 0.3)
    need_obj(r, c, 1)
    t = r.randrange(0, len(c["objs"]))
    c["args"][1] = ("obj", t)
    return c


# ---- ProcessObjects
def po_edges():
    r = random.Random(1508)
    c = case(r, "ProcessObjects", [S])
    c["objs"] = []
    yield c
    for typ in list(range(-1, 13)):
        for nodeid in (-1, 0, 3):
            for h, f04 in ((0, 0), (8, 0), (3, 11), (3, 4), (1, 7)):
                c = case(r, "ProcessObjects", [S], sc_handle=h, g_f04=f04)
                c["objs"] = [mk_obj(r, typ=typ), mk_obj(r)]
                c["objs"][0]["nodeid"] = nodeid
                yield c
    for flags in (KEEP, 0, 0x10, 0x11):
        for gd in (-1, 0):
            for recf in (0, 0x10):
                c = case(r, "ProcessObjects", [S])
                c["objs"] = [mk_obj(r, typ=2)]
                c["objs"][0]["nodeid"] = 1
                c["queues"]["gdnwm_flag"] = [flags]
                c["queues"]["gdnwm"] = [gd]
                for ride in c["rides"]:
                    for i, rec in enumerate(ride["recs"]):
                        ride["recs"][i] = (recf, rec[1], rec[2])
                yield c
    for blk in (False, True):
        for typ in (1, 2, 3):
            c = case(r, "ProcessObjects", [S], sc_handle=2, g_f04=2)
            c["objs"] = [mk_obj(r, typ=typ)]
            c["objs"][0]["nodeid"] = 0
            for ride in c["rides"]:
                ride["recs"] = [(f, blk, b) for f, h, b in ride["recs"]]
            yield c


def po_random(r):
    c = case(r, "ProcessObjects", [S])
    c["sc"]["handle"] = r.choice([0, 1, 2, 3, 8, 9, c["g"]["f04"], c["g"]["f04"] + 8])
    for _ in range(r.randrange(0, 3)):
        c["objs"].append(mk_obj(r))
    return c


# ---- ReadScript / Push / Pop
def rs_edges():
    r = random.Random(1509)
    for pw in (0, 1, 5, 8):
        for st in (-2, -1, 0, 1, pw - 2, pw - 1, pw, pw + 1):
            yield case(r, "ReadScript", [S], sc_pw=pw, sc_state=st)


def rs_random(r):
    pw = r.randrange(0, 9)
    return case(r, "ReadScript", [S], sc_pw=pw, sc_state=r.randrange(-3, pw + 3))


def stack_edges(op, args):
    r = random.Random(1510 + (op == "Pop"))
    for ss in (0, 1, 2, 5, 8):
        for ix in (-3, -2, -1, 0, 1, ss - 2, ss - 1, ss, ss + 1):
            for has in (True, False):
                yield case(r, op, args, sc_ss=ss, sc_sidx=ix, sc_stack=has)


def pu_edges():
    return stack_edges("Push", [S, -5])


def pu_random(r):
    ss = r.randrange(0, 9)
    return case(r, "Push", [S, ri(r)], sc_ss=ss, sc_sidx=r.randrange(-3, ss + 3), sc_stack=r.random() < 0.9)


def pp_edges():
    return stack_edges("Pop", [S])


def pp_random(r):
    ss = r.randrange(0, 9)
    return case(r, "Pop", [S], sc_ss=ss, sc_sidx=r.randrange(-3, ss + 3), sc_stack=r.random() < 0.9)


# ---- the small getters and setters
def sv_edges():
    r = random.Random(1512)
    for init in (0, 1):
        for sc_arg in (S, NULL):
            for vc in (0, 1, 4, 8):
                for ix in (-1, 0, vc - 1, vc, vc + 1, 0x7FFFFFFF, -0x80000000):
                    yield case(r, "RSSE_Setvar", [sc_arg, ix, r.choice([5, -7, 0x12345678])], g_init=init, sc_vc=vc)


def sv_random(r):
    vc = r.randrange(0, 9)
    return case(r, "RSSE_Setvar", [r.choice([S, S, S, NULL]), r.randrange(-2, vc + 2), ri(r)], g_init=r.choice([0, 1, 1]), sc_vc=vc)


def getset_edges(op, args):
    r = random.Random(1513 + len(op))
    for init in (0, 1):
        for sc_arg in (S, NULL):
            for v in (0, 1, -1, 0x7FFF, 0x8000, 0x12345):
                yield case(r, op, [sc_arg] + [v] * len(args), g_init=init, sc_speed=v & 0xFFFF, sc_fly=v)


def srs_edges():
    return getset_edges("RSSE_SetRideSpeed", [0])


def srs_random(r):
    return case(r, "RSSE_SetRideSpeed", [r.choice([S, S, S, NULL]), ri(r)], g_init=r.choice([0, 1, 1]))


def grs_edges():
    return getset_edges("RSSE_GetRideSpeed", [])


def grs_random(r):
    return case(r, "RSSE_GetRideSpeed", [r.choice([S, S, S, NULL])], g_init=r.choice([0, 1, 1]))


def gtb_edges():
    return getset_edges("RSSE_GetTourBaseID", [])


def gtb_random(r):
    return case(r, "RSSE_GetTourBaseID", [r.choice([S, S, S, NULL])], g_init=r.choice([0, 1, 1]))


# ---- the position getters
def grp_edges():
    r = random.Random(1514)
    for slot in range(4):
        yield case(r, "GetRidePosition", [("set", slot), ("scr", 4), ("scr", 8), ("scr", 12)])


def grp_random(r):
    return case(r, "GetRidePosition", [("set", r.randrange(0, 4)), ("scr", 4), ("scr", 8), ("scr", 12)])


def grc_edges():
    r = random.Random(1515)
    for ride in (0, 1, 2, 3):
        for found in (0, 1):
            c = case(r, "GetRideCentrePosition", [("scr", 0), ("scr", 4), ("scr", 8), ("scr", 12)])
            c["scratch"][0] = ride
            c["queues"]["grmba"] = [found]
            yield c


def grc_random(r):
    c = case(r, "GetRideCentrePosition", [("scr", 0), ("scr", 4), ("scr", 8), ("scr", 12)])
    c["scratch"][0] = r.randrange(0, 4)
    return c


TESTS = [
    FnTest("ExtractData", ppc, host, edge=ed_edges, rnd=ed_random),
    FnTest("CreateObject", ppc, host, edge=co_edges, rnd=co_random),
    FnTest("KillObject", ppc, host, edge=ko_edges, rnd=ko_random),
    FnTest("KillLinkedObject", ppc, host, edge=kl_edges, rnd=kl_random),
    FnTest("SetObjParam", ppc, host, edge=sp_edges, rnd=sp_random),
    FnTest("StartEvent", ppc, host, edge=se_edges, rnd=se_random),
    FnTest("InitObject", ppc, host, edge=io_edges, rnd=io_random),
    FnTest("ProcessObjects", ppc, host, edge=po_edges, rnd=po_random),
    FnTest("ReadScript", ppc, host, edge=rs_edges, rnd=rs_random),
    FnTest("Push", ppc, host, edge=pu_edges, rnd=pu_random),
    FnTest("Pop", ppc, host, edge=pp_edges, rnd=pp_random),
    FnTest("RSSE_Setvar", ppc, host, edge=sv_edges, rnd=sv_random),
    FnTest("RSSE_SetRideSpeed", ppc, host, edge=srs_edges, rnd=srs_random),
    FnTest("RSSE_GetRideSpeed", ppc, host, edge=grs_edges, rnd=grs_random),
    FnTest("RSSE_GetTourBaseID", ppc, host, edge=gtb_edges, rnd=gtb_random),
    FnTest("GetRidePosition", ppc, host, edge=grp_edges, rnd=grp_random),
    FnTest("GetRideCentrePosition", ppc, host, edge=grc_edges, rnd=grc_random),
]
