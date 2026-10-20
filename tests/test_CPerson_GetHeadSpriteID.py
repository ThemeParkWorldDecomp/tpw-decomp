"""Unit v51034: seven functions against libtpw_unit_CPerson_GetHeadSpriteID.dylib (tests/unit_CPerson_GetHeadSpriteID.cpp), main app
("SimTheme Park"), src/CPerson_..._CPerson_GetHeadSpriteID.cpp:
  CPerson::GetHeadSpriteID 0x1011836c, SetSpotAnim 0x10117e64, ScoreOnCloserInspection 0x10119068, FinalDestroy 0x10116da4,
  LOADVAR<ESP_Sprites> 0x101161a8, Pilot::~Pilot 0x10113028, CParkTime::GetFunnyTimeSinceGameTurn 0x10112c80

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory, big-endian, for the original; the
host library's memory, little-endian, for the translation). Every callee outside the unit is a fake on both sides that records its
arguments (pointers as offsets from the person block) and answers from the case's queues: ESP_GetNumberOfSprites, ESP_GetRandomSpritesFrom,
CThing::GetThingId (a halfword through a hidden pointer), CWorld::SeedRand, SCR_GetScriptPosition, SCR_KillScript, CPeep::PlaySound, CPeep::FinalDestroy,
CDelayedScript::SetSpriteScript, CPerson::SetState, CObject::IsDirty / GetExcitementLevel, CPerson::GetPreferredExcitement, CWorldSavvy::GetWorld,
CSaveLoad::WriteSerialiseLog, LbFile_Read (the error word, the transferred count and four data bytes), GAME_ASSERTMSG and Vehicle::~Vehicle.

The world of a case: one block (person at +0, a value at +0x300, a name at +0x340, a file at +0x400, a CObject at +0x800), gWorld pointing at
a world whose turn word is the case's, the sound categories. What is compared: the answer, the whole person block, the value word and
the event log. Pilot::~Pilot: the host destructor is the empty body (the Mac's deleting form: ~Vehicle then operator delete when the
halfword flag is above 0); the host wrapper stands in for the flag test and delete, so the PowerPC delete count is compared with that flag test.
No fused operations. Not tested: nothing skipped.
"""
import random
import struct
import ctypes as C

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPerson_GetHeadSpriteID"
MASK = 0xFFFFFFFF

HEAD, SPOT, SCORE, FINAL, LOAD, PILOT, FUNNY = 0x1011836C, 0x10117E64, 0x10119068, 0x10116DA4, 0x101161A8, 0x10113028, 0x10112C80
SIZES = {HEAD: 148, SPOT: 212, SCORE: 180, FINAL: 84, LOAD: 236, PILOT: 96, FUNNY: 180}
OWN = set(SIZES)

ADDR = dict(gWorld=0x1037EC0C, gSoundBanks=0x102F7C94)
HOST_LAYOUT = [0x1C, 0x1DA70C]
PERSON_WORDS = 0x240 // 4
VAL, NAME, FILE, OBJ = 0x300, 0x340, 0x400, 0x800
CUR = {"base": 0}


def s32(v):
    return T.s32(v)


def off(v):
    return (v - CUR["base"]) & MASK


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def world(e):
    return e.u32(ADDR["gWorld"])


def st_num(e):
    e.cl.rec("ESP_GetNumberOfSprites", (s32(e.r(3)),))
    e.set_r(3, e.cl.ret("ESP_GetNumberOfSprites") & MASK)


def st_rand(e):
    e.cl.rec("ESP_GetRandomSpritesFrom", (s32(e.r(3)),))
    e.set_r(3, e.cl.ret("ESP_GetRandomSpritesFrom") & MASK)


def st_thing(e):
    e.cl.rec("GetThingId", (off(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", e.cl.ret("GetThingId") & 0xFFFF))


def st_seed(e):
    e.cl.rec("SeedRand", (1 if e.r(3) == world(e) else 0, e.r(4) & MASK))


def st_pos(e):
    e.cl.rec("SCR_GetScriptPosition", (s32(e.r(3)),))


def st_play(e):
    e.cl.rec("PlaySound", (off(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def st_sprite(e):
    e.cl.rec("SetSpriteScript", (off(e.r(3)), s32(e.r(4))))


def st_state(e):
    e.cl.rec("SetState", (off(e.r(3)), s32(e.r(4))))


def st_dirty(e):
    e.cl.rec("IsDirty", (off(e.r(3)),))
    e.set_r(3, e.cl.ret("IsDirty") & MASK)


def st_exc(e):
    e.cl.rec("GetExcitementLevel", (off(e.r(3)), e.r(4) & 0xFF))
    e.set_r(3, e.cl.ret("GetExcitementLevel") & MASK)


def st_pref(e):
    e.cl.rec("GetPreferredExcitement", (off(e.r(3)),))
    e.set_r(3, e.cl.ret("GetPreferredExcitement") & MASK)


def st_kill(e):
    e.cl.rec("SCR_KillScript", (s32(e.r(3)),))


def st_peepfinal(e):
    e.cl.rec("CPeep_FinalDestroy", (off(e.r(3)),))


def st_getworld(e):
    e.cl.rec("GetWorld", (off(e.r(3)),))
    e.set_r(3, world(e))


def st_vdtor(e):
    e.cl.rec("Vehicle_dtor", (off(e.r(3)), s32(e.r(4))))


def st_read(e):
    L = e.cl
    L.rec("LbFile_Read", (off(e.r(4)), off(e.r(5)), e.r(6) & MASK))
    e.write(e.r(5), struct.pack("<I", L.ret("LbFile_Read.data") & MASK))
    n = L.ret("LbFile_Read.n") & MASK
    if n != MASK:
        e.write(e.r(7), struct.pack(">I", n))
    e.write(e.r(3), struct.pack(">I", L.ret("LbFile_Read.err") & MASK))


STUBS.internal(0x1007C8B0, st_num)
STUBS.internal(0x1007C900, st_rand)
STUBS.internal(0x1012E1F8, st_thing)
STUBS.internal(0x1013B244, st_seed)
STUBS.internal(0x100D93B8, st_pos)
STUBS.internal(0x10115B80, st_play)
STUBS.internal(0x100FAA74, st_sprite)
STUBS.internal(0x10120354, st_state)
STUBS.internal(0x1010C900, st_dirty)
STUBS.internal(0x1010CFB0, st_exc)
STUBS.internal(0x1011B4B4, st_pref)
STUBS.internal(0x100D9478, st_kill)
STUBS.internal(0x101130AC, st_peepfinal)
STUBS.internal(0x1014205C, st_getworld)
STUBS.internal(0x101332CC, st_vdtor)
STUBS.internal(0x10238848, st_read)
STUBS.internal(0x1015B3DC, CL.recorder("WriteSerialiseLog", "xisi"))
STUBS.internal(0x1000DDE8, CL.recorder("GAME_ASSERTMSG", "is"))


def setup(runner):
    T.install_std_stubs(runner.emu)


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
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        self.deletes0 = len(e.deletes)

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

    def ptr(self, a, v):
        self.words(a, [v])

    def log(self):
        return self.L.log

    def deleted(self):
        return len(self.e.deletes) - self.deletes0

    def call(self, addr, args):
        return T.call_ppc(self.e, addr, [a & MASK for a in args])[0] & MASK

    def head(self, p):
        return self.call(HEAD, [p])

    def spot(self, p, script):
        self.call(SPOT, [p, script])
        return 0

    def score(self, p, o):
        return self.call(SCORE, [p, o])

    def final(self, p):
        self.call(FINAL, [p])
        return 0

    def load(self, base, file, value, name, nolog):
        return self.call(LOAD, [file, value, name, nolog])

    def pilot(self, p, flag):
        self.call(PILOT, [p, flag])
        return 1 if self.deleted() else 0

    def funny(self, p, turn):
        out = self.e.alloc(16)
        self.call(FUNNY, [out, p, turn])
        return struct.unpack(">Q", self.e.read(out, 8))[0]


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gh_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_gh_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def log(self):
        return CL.host_end(self.lib)

    def head(self, p):
        return self.lib.tpw_gh_head(p) & MASK

    def spot(self, p, script):
        self.lib.tpw_gh_spot(p, s32(script))
        return 0

    def score(self, p, o):
        return self.lib.tpw_gh_score(p, o) & MASK

    def final(self, p):
        self.lib.tpw_gh_final(p)
        return 0

    def load(self, base, file, value, name, nolog):
        return self.lib.tpw_gh_loadvar(base, file, value, name, s32(nolog)) & MASK

    def pilot(self, p, flag):
        return self.lib.tpw_gh_pilot(p, s32(flag))

    def funny(self, p, turn):
        return self.lib.tpw_gh_funny(p, turn & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gh_addr.restype, lib.tpw_gh_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gh_alloc.restype, lib.tpw_gh_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gh_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_gh_head.restype, lib.tpw_gh_head.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_gh_spot.restype, lib.tpw_gh_spot.argtypes = None, [C.c_uint64, C.c_int32]
    lib.tpw_gh_score.restype, lib.tpw_gh_score.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    lib.tpw_gh_final.restype, lib.tpw_gh_final.argtypes = None, [C.c_uint64]
    lib.tpw_gh_loadvar.restype = C.c_uint32
    lib.tpw_gh_loadvar.argtypes = [C.c_uint64] * 4 + [C.c_int32]
    lib.tpw_gh_pilot.restype, lib.tpw_gh_pilot.argtypes = C.c_int32, [C.c_uint64, C.c_int32]
    lib.tpw_gh_funny.restype, lib.tpw_gh_funny.argtypes = C.c_uint64, [C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gh_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    a = s.alloc(0x1000)
    CUR["base"] = a
    s.words(a, case["pw"])
    for o, v in case.get("set", {}).items():
        s.words(a + o, [v])
    s.words(a + VAL, [case.get("val", 0)])
    s.write(a + NAME, case.get("name", b"mSprites") + b"\0")
    wb = s.alloc(16)
    s.words(wb, [case["turn"]])
    s.ptr(s.g("gWorld"), wb - 0x1DA70C)
    s.words(s.g("gSoundBanks"), case["banks"])
    return a


def execute(s, case, a):
    op = case["op"]
    if op == "head":
        return s.head(a)
    if op == "spot":
        return s.spot(a, case["script"])
    if op == "score":
        return s.score(a, a + OBJ)
    if op == "final":
        return s.final(a)
    if op == "load":
        return s.load(a, a + FILE, a + VAL, a + NAME, case["nolog"])
    if op == "pilot":
        return s.pilot(0 if case["null"] else a, case["flag"])
    return s.funny(a, case["arg"])


def observe(s, case, a, answer):
    # LOADVAR: the file's bytes are little-endian. A failed read leaves them as read (same bytes on both sides, so read both little-endian);
    # a good one makes the value (the Mac reverses the bytes in its big-endian memory), so read each side in its own order.
    raw = s.read(a + VAL, 4)
    val = struct.unpack("<I" if (case["op"] == "load" and answer) else s.fmt, raw)[0]
    return {"answer": answer, "log": s.log(), "person": s.rd_words(a, PERSON_WORDS), "val": val,
            "obj": s.rd_words(a + OBJ, 8)}


def run(s, case):
    try:
        a = build(s, case)
        return observe(s, case, a, execute(s, case, a))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def u32(r):
    k = r.random()
    if k < 0.2:
        return r.choice([0, 1, 2, 3, 4, 0xFF, 0x100, 0x7FFFFFFF, 0x80000000, MASK, 0xFFFF, 0x10000])
    return r.getrandbits(32)


def base(r, op, **kw):
    c = {"op": op, "pw": [r.getrandbits(32) for _ in range(PERSON_WORDS)], "turn": u32(r), "banks": [u32(r) for _ in range(8)],
         "val": u32(r), "set": {}, "nolog": r.choice([0, 1, 2, -1]), "flag": r.choice([0, 1, -1, 0x8000, 0x10000, 0x7FFF, 0xFFFF0001]),
         "null": r.random() < 0.1, "script": r.choice([4, 4, 4, 0, 3, 5, 8, -1, r.randrange(0, 14)]), "arg": u32(r)}
    q = {"ESP_GetNumberOfSprites": [r.choice([0, 1, 5, -1])], "ESP_GetRandomSpritesFrom": [u32(r)],
         "GetThingId": [r.choice([0, 0x10, 0xFFF0, 0x1F, r.getrandbits(16)])], "IsDirty": [r.choice([0, 0, 1, -1, 7])],
         "GetExcitementLevel": [r.choice([r.randrange(256), u32(r)])], "GetPreferredExcitement": [r.choice([r.randrange(256), u32(r)])],
         "LbFile_Read.data": [u32(r)], "LbFile_Read.n": [r.choice([4, 4, 4, 0, 3, 5, MASK])],
         "LbFile_Read.err": [r.choice([0, 0, 1, -1, 0x80000000, 5])]}
    c["queues"] = q
    c.update(kw)
    return c


def mk(op, queues=None, seed=0, **kw):
    r = random.Random(seed)
    c = base(r, op, **kw)
    if queues:
        for k, v in queues.items():
            c["queues"][k] = [v]
    return c


def make_cases(op):
    cs = []
    n = 0
    if op == "head":
        for ns in (0, 1, 5):
            for t24 in (2, 0, 3, 0xFFFFFFFF):
                for tid in (0, 0xFFFF, 0x10):
                    n += 1
                    c = mk("head", {"ESP_GetNumberOfSprites": ns, "GetThingId": tid}, n)
                    c["pw"][8], c["pw"][9] = n * 0x01010101 & MASK, t24
                    c["pw"][8] = 0x8000FFFF if n % 3 == 0 else c["pw"][8]
                    cs.append(c)
    elif op == "spot":
        for script in (4, 0, 3, 5, -1, 8):
            for tid in (0, 0x10, 0xFFF0, 1, 0x1F, 0xF):
                n += 1
                cs.append(mk("spot", {"GetThingId": tid}, n, script=script))
    elif op == "score":
        for dirty in (0, 1, -1):
            for pref, exc in ((0, 0), (255, 0), (0, 255), (100, 50), (50, 100), (10, 60), (60, 10), (0x1FF, 0x2FF), (7, 7), (80, 30), (30, 80), (81, 30), (30, 81)):
                n += 1
                cs.append(mk("score", {"IsDirty": dirty, "GetPreferredExcitement": pref, "GetExcitementLevel": exc}, n))
    elif op == "final":
        for h in (0, 1, MASK, 0x1234, 0x80000000):
            n += 1
            c = mk("final", None, n)
            c["pw"][0x210 // 4] = h
            cs.append(c)
    elif op == "load":
        for err in (0, -1, 5, 0x80000000, 1):
            for cnt in (4, 0, 3, 5, MASK):
                for nolog in (0, 1, -1):
                    n += 1
                    cs.append(mk("load", {"LbFile_Read.err": err, "LbFile_Read.n": cnt}, n, nolog=nolog))
    elif op == "pilot":
        for null in (False, True):
            for flag in (0, 1, -1, 0x8000, 0x10000, 0x7FFF, 0xFFFF0001, 0x18000, 2):
                n += 1
                cs.append(mk("pilot", None, n, null=null, flag=flag))
    else:
        for turn, arg, secs in ((100, 40, 15000), (5, 10, 15000), (10, 10, 0), (0, 1, 1), (MASK, 0, MASK), (0x80000000, 0, 3), (MASK, 1, 2),
                                (4, 0, 1), (3, 0, 1), (0x7FFFFFFF, 0, 4), (0, MASK, MASK), (0xFFFFFFFE, 0, MASK), (0x10000, 0, 0x10000)):
            n += 1
            c = mk("funny", None, n, turn=turn, arg=arg)
            c["pw"][0x1C // 4] = secs
            cs.append(c)
    return cs


def edges_for(op):
    def f():
        yield from make_cases(op)
    return f


def rnd_for(op):
    def f(r):
        c = base(r, op)
        c["pw"][0x210 // 4] = r.choice([0, u32(r)])
        c["pw"][0x1C // 4] = u32(r)
        c["pw"][9] = r.choice([2, 2, u32(r)])
        return c
    return f


TESTS = [FnTest(name, ppc, host, edge=edges_for(op), rnd=rnd_for(op)) for name, op in (
    ("CPerson::GetHeadSpriteID", "head"), ("CPerson::SetSpotAnim", "spot"), ("CPerson::ScoreOnCloserInspection", "score"),
    ("CPerson::FinalDestroy", "final"), ("LOADVAR<ESP_Sprites>", "load"), ("Pilot::~Pilot", "pilot"),
    ("CParkTime::GetFunnyTimeSinceGameTurn", "funny"))]
