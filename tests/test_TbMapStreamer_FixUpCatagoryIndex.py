"""Unit v51113, the small functions at the edges of the sound library's CSprocketsModule.cpp and the codec files, against
libtpw_unit_TbMapStreamer_FixUpCatagoryIndex.dylib (tests/unit_TbMapStreamer_FixUpCatagoryIndex.cpp), sound library
("sound symbols shared"), src/TbMapStreamer_TbMapStreamer_FixUpCatagoryIndex.cpp:
  TbMapStreamer::FixUpCatagoryIndex 0x10023378, UnloadCatgoryFixupTable 0x10023310, UString operator char* 0x10023674,
  CDiscFileStorage ctor 0x10020cf8, CAudioFileSystem ctor 0x10027884, CSprocketsModule::RestartChannel 0x1002c51c and RestartAll
  0x1002c748, UCommandPlay ctor 0x1002c5fc, TpBody::Init 0x1002e3bc, Tick 0x1002e404, Close 0x1002e44c, SetFades 0x1002e494,
  DeregisterCatagoryBanks 0x1002e4e0, SetCatagoryReverb 0x1002e8a4, ~TpBody 0x1002e8fc, the cleanup destructors 0x1000f9dc and 0x1000b590,
  TbError() 0x1000eb1c, std::sqrtf 0x1000c70c and std::sqrt 0x1000c6cc.

A case is a world and a call. Each side builds its world in its own memory (the Mac's big-endian words in the emulator; the host's objects
with native pointers: the same case drives both builders). The callees outside the unit are fakes on both sides that record the same
events: UCommand's constructor, the channel send glue 0x1002f154 (channel label and the command's vtable word), UStatus::IfOk, the
fixup table's Unload, CSprocketsModule::Tick, the streamer's Instance and RemoveCatagory, TbAudioBody's destructor, the two ClearCache,
the free 0x1002ea94, the file storage's base constructor and the runtime sqrt.

What is compared: the answers (TbError words through the hidden buffer), the event logs, the block contents the functions touch.
No fused operations. Not tested: the vtable word of TpBody's destructor and of the two constructors cannot be stored by the host (the classes
have no vtable of that kind), so the emulator side asserts the stored constant and the host side compares "a vtable word was stored";
Channels() NULL (the Mac reads a NULL pointer), indexes past the channel array (the original reads past it).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_TbMapStreamer_FixUpCatagoryIndex"
MASK = 0xFFFFFFFF

FIX, UNLOAD, STRTEXT, DISC, AUDFS = 0x10023378, 0x10023310, 0x10023674, 0x10020cf8, 0x10027884
RCHAN, UPLAY, RALL = 0x1002c51c, 0x1002c5fc, 0x1002c748
TP_INIT, TP_TICK, TP_CLOSE, TP_FADES, TP_DEREG, TP_REVERB, TP_DTOR = 0x1002e3bc, 0x1002e404, 0x1002e44c, 0x1002e494, 0x1002e4e0, 0x1002e8a4, 0x1002e8fc
CS_PIPE, CS_LOOP, TBERR, SQRTF, SQRT = 0x1000f9dc, 0x1000b590, 0x1000eb1c, 0x1000c70c, 0x1000c6cc
SIZES = {FIX: 44, UNLOAD: 40, STRTEXT: 8, DISC: 60, AUDFS: 12, RCHAN: 168, UPLAY: 56, RALL: 164, TP_INIT: 36, TP_TICK: 36, TP_CLOSE: 36,
         TP_FADES: 36, TP_DEREG: 100, TP_REVERB: 36, TP_DTOR: 104, CS_PIPE: 84, CS_LOOP: 84, TBERR: 4, SQRTF: 40, SQRT: 32}
OWN = set(SIZES)

# callees
G_UNLOAD, G_UCMD, G_SEND, G_IFOK, G_TICK, G_INSTANCE, G_REMOVE = 0x10023a78, 0x1002c468, 0x1002f154, 0x1002c144, 0x1002b8c4, 0x1001f6ec, 0x10022e78
G_ABODY, G_FREE, G_CLEAR1, G_CLEAR2, G_FSTORE, G_SQRT = 0x100163bc, 0x1002ea94, 0x1000edb8, 0x1000abc0, 0x1002eecc, 0x1002ecec
TOC = 0x1003abf0
VT_DISC, VT_AUDFS, VT_TPBODY, VT_PLAY = TOC + 0x22D8, TOC + 0x29F0, TOC + 0x36D8, TOC + 0x3420

INST = [0]


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes
STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_unload(e):
    e.cl.rec("Unload", (e.cl.lab(e.r(3)),))


def st_ucmd(e):
    e.cl.rec("UCommand", (e.r(4) & MASK,))


def st_send(e):
    L = e.cl
    L.rec("Send", (L.lab(e.r(4)), e.u32(e.r(5) + 4)))
    e.write(e.r(3), struct.pack(">I", L.ret("send") & MASK))


def st_ifok(e):
    L = e.cl
    L.rec("IfOk", (s32(e.u32(e.r(3))), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, L.ret("IfOk") & MASK)


def st_tick(e):
    e.cl.rec("Tick", (e.cl.lab(e.r(3)),))


def st_instance(e):
    e.cl.rec("Instance", (0,))
    e.set_r(3, INST[0])


def st_remove(e):
    L = e.cl
    L.rec("Remove", (L.lab(e.r(4)), e.r(5) & MASK))
    e.write(e.r(3), struct.pack(">I", L.ret("Remove") & MASK))


def st_abody(e):
    e.cl.rec("AudioBodyDtor", (e.cl.lab(e.r(3)), s32(e.r(4))))


def st_free(e):
    e.cl.rec("Free", (e.cl.lab(e.r(3)),))


def st_clear1(e):
    e.cl.rec("ClearCache", (0,))


def st_clear2(e):
    e.cl.rec("ClearCache2", (0,))


def st_fstore(e):
    # the import glue saves the caller's TOC in the stack slot the caller restores r2 from (lwz r2, 0x14(r1))
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
    e.cl.rec("CFileStorage", (e.cl.lab(e.r(3)),))


def st_sqrt(e):
    x = e.f(1)
    if x != x:
        y = x
    elif x < 0:
        y = float("nan")
    else:
        y = math.sqrt(x)
    e.set_f1(y)


for _a, _f in ((G_UNLOAD, st_unload), (G_UCMD, st_ucmd), (G_SEND, st_send), (G_IFOK, st_ifok), (G_TICK, st_tick), (G_INSTANCE, st_instance),
               (G_REMOVE, st_remove), (G_ABODY, st_abody), (G_FREE, st_free), (G_CLEAR1, st_clear1), (G_CLEAR2, st_clear2),
               (G_FSTORE, st_fstore), (G_SQRT, st_sqrt)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)
    INST[0] = T.persist_alloc(runner.emu, 0x40)


# ---------------------------------------------------------------------------------------------------------------- the two sides
class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e):
        self.e = e

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, d):
        self.e.write(a, d)

    def call(self, case, fixed, entry, args):
        """Runs the original with the fakes active and a fresh log; answers (r3, f1, log)."""
        L = self.e.cl = CL.PpcLog(fixed, case.get("queues", {}))
        saved = STUBS.activate(self.e)
        try:
            r3, _, f1 = T.call_ppc(self.e, entry, args)
        finally:
            CL.Stubs.deactivate(self.e, saved)
        return r3 & MASK, f1, L.log


class Host(Side):
    fmt = "<I"

    def __init__(self, lib):
        self.lib = lib
        bind(lib)

    def alloc(self, n):
        return self.lib.tpw_ms_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def begin(self, case, fixed):
        CL.host_begin(self.lib, fixed, case.get("queues", {}))

    def end(self):
        return CL.host_end(self.lib)

    def words64(self, a, ws):
        self.write(a, b"".join(struct.pack("<Q", w) for w in ws))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u32, u64, i32 = C.c_uint32, C.c_uint64, C.c_int32

    def b(name, res, *args):
        f = getattr(lib, name)
        f.restype, f.argtypes = res, list(args)

    b("tpw_ms_addr", u64, C.c_char_p)
    b("tpw_ms_alloc", u64, u32)
    b("tpw_ms_make", u64)
    b("tpw_ms_fixup_addr", u64, u64)
    b("tpw_ms_fixup", u32, u32, u64, u32)
    b("tpw_ms_unload", None, u64)
    b("tpw_ms_stringtext", u64, u32, u64)
    b("tpw_ms_disc_ctor", u32, u64)
    b("tpw_ms_audiofs_ctor", u32, u64)
    b("tpw_ms_error_ctor", None, u64)
    b("tpw_sp_setup", u64, u32, u64)
    b("tpw_sp_restart_channel", i32, u64, u32)
    b("tpw_sp_restart_all", i32, u64)
    b("tpw_sp_command_play", None, u64, u32)
    b("tpw_tp_simple", i32, i32, u64, u64)
    b("tpw_tp_tick", None, u64)
    b("tpw_tp_destruct", u64, u64, i32)
    b("tpw_cs_pipe", u64, u64, i32)
    b("tpw_cs_loop", u64, u64, i32)
    b("tpw_sq_sqrtf", u32, u32)
    b("tpw_sq_sqrt", u32, u32)
    lib.bound = True


def s16(v):
    return T.s16(v)


# ---------------------------------------------------------------------------------------------------------------- FixUpCatagoryIndex
def fix_case(r, count=None, index=None, has=None, huge=False):
    c = {"queues": {}}
    if huge:
        c["count"] = r.choice([0x80000000, 0x80000001, 0xFFFFFFFF])
        c["index"] = r.choice([0, 1, 2, 3, c["count"], c["count"] - 1 if c["count"] > 4 else 0]) if index is None else index
        c["words"] = [r.getrandbits(32) for _ in range(4)]
        c["has"] = True
        if c["index"] > 3 and c["index"] < c["count"]:
            c["index"] = 3
        return c
    n = r.choice([0, 1, 2, 3, 5, 8]) if count is None else count
    c["count"] = n
    c["words"] = [r.getrandbits(32) for _ in range(max(n, 1) + 2)]
    c["has"] = r.random() < 0.8 if has is None else has
    c["index"] = r.choice([0, n - 1 if n else 0, n, n + 1, 0xFFFFFFFF, r.randrange(0, n + 2)]) & MASK if index is None else index
    return c


def fix_edges():
    r = random.Random(11)
    yield fix_case(r, 0, 0)
    yield fix_case(r, 0, 0, has=False)
    yield fix_case(r, 3, 0)
    yield fix_case(r, 3, 2)
    yield fix_case(r, 3, 3)
    yield fix_case(r, 3, 0xFFFFFFFF)
    yield fix_case(r, 3, 1, has=False)
    for _ in range(6):
        yield fix_case(r, huge=True)
    yield fix_case(r, huge=True, index=0xFFFFFFFF)


def fix_ppc(e, c):
    s = Ppc(e)
    obj = s.alloc(0x40)
    tbl = s.alloc(4 * len(c["words"]))
    s.words(tbl, c["words"])
    s.words(obj + 4, [c["count"], tbl if c["has"] else 0])
    r3, _, _ = T.call_ppc(e, FIX, [obj, c["index"]]) if False else (T.call_ppc(e, FIX, [obj, c["index"]])[0] & MASK, None, None)
    return {"answer": r3}


def fix_host(lib, c):
    s = Host(lib)
    tbl = s.alloc(4 * len(c["words"]))
    s.words(tbl, c["words"])
    return {"answer": lib.tpw_ms_fixup(c["count"], tbl if c["has"] else 0, c["index"]) & MASK}


# ---------------------------------------------------------------------------------------------------------------- UnloadCatgoryFixupTable
def unload_case(r):
    return {"queues": {}, "words": [r.getrandbits(32) for _ in range(3)]}


def unload_ppc(e, c):
    s = Ppc(e)
    obj = s.alloc(0x40)
    s.words(obj + 4, c["words"])
    _, _, log = s.call(c, {obj + 4: 1}, UNLOAD, [obj])
    return {"log": log}


def unload_host(lib, c):
    s = Host(lib)
    obj = lib.tpw_ms_make()
    s.begin(c, {1: lib.tpw_ms_fixup_addr(obj)})
    lib.tpw_ms_unload(obj)
    return {"log": s.end()}


# ---------------------------------------------------------------------------------------------------------------- UString operator char*
def str_case(r):
    return {"queues": {}, "length": r.getrandbits(32), "text": r.choice([0, 1, 0x7FFFFFFF, r.getrandbits(32)])}


def str_ppc(e, c):
    s = Ppc(e)
    blk = s.alloc(0x20)
    s.words(blk + 4, [c["length"], c["text"]])
    return {"answer": T.call_ppc(e, STRTEXT, [blk])[0] & MASK}


def str_host(lib, c):
    return {"answer": lib.tpw_ms_stringtext(c["length"], c["text"]) & MASK}


# ---------------------------------------------------------------------------------------------------------------- constructors
def ctor_case(r):
    return {"queues": {}}


def disc_ppc(e, c):
    s = Ppc(e)
    obj = s.alloc(0x40)
    s.write(obj, b"\xAA" * 0x40)
    r3, _, log = s.call(c, {obj: 1}, DISC, [obj])
    return {"log": log, "answer": 1 if r3 == obj else 0, "vtable": 1 if s.rd_words(obj, 1)[0] == VT_DISC else 0,
            "tail": s.read(obj + 0x10, 0x30)}


def disc_host(lib, c):
    s = Host(lib)
    obj = s.alloc(0x40)
    s.write(obj, b"\xAA" * 0x40)
    s.begin(c, {1: obj})
    flag = lib.tpw_ms_disc_ctor(obj)
    return {"log": s.end(), "answer": 1, "vtable": flag, "tail": s.read(obj + 0x10, 0x30)}


def audfs_ppc(e, c):
    s = Ppc(e)
    obj = s.alloc(0x40)
    s.write(obj, b"\xAA" * 0x40)
    r3, _, log = s.call(c, {obj: 1}, AUDFS, [obj])
    return {"log": log, "answer": 1 if r3 == obj else 0, "vtable": 1 if s.rd_words(obj, 1)[0] == VT_AUDFS else 0,
            "tail": s.read(obj + 0x10, 0x30)}


def audfs_host(lib, c):
    s = Host(lib)
    obj = s.alloc(0x40)
    s.write(obj, b"\xAA" * 0x40)
    s.begin(c, {1: obj})
    flag = lib.tpw_ms_audiofs_ctor(obj)
    return {"log": s.end(), "answer": 1, "vtable": flag, "tail": s.read(obj + 0x10, 0x30)}


def err_case(r):
    return {"queues": {}, "fill": r.randrange(256)}


def err_ppc(e, c):
    s = Ppc(e)
    buf = s.alloc(8)
    s.write(buf, bytes([c["fill"]]) * 8)
    r3, _, log = s.call(c, {buf: 1}, TBERR, [buf])
    return {"log": log, "answer": 1 if r3 == buf else 0, "block": s.read(buf, 8)}


def err_host(lib, c):
    s = Host(lib)
    buf = s.alloc(8)
    s.write(buf, bytes([c["fill"]]) * 8)
    s.begin(c, {1: buf})
    lib.tpw_ms_error_ctor(buf)
    return {"log": s.end(), "answer": 1, "block": s.read(buf, 8)}


def play_case(r):
    return {"queues": {}, "on": r.choice([0, 1, 2, 0xFFFFFFFF, r.getrandbits(32)]), "pat": [r.getrandbits(32) for _ in range(32)]}


def play_ppc(e, c):
    s = Ppc(e)
    blk = s.alloc(0x80)
    s.words(blk, c["pat"])
    r3, _, log = s.call(c, {blk: 1}, UPLAY, [blk, c["on"]])
    return {"log": log, "answer": 1 if r3 == blk else 0, "block": s.rd_words(blk, 32)}


def play_host(lib, c):
    s = Host(lib)
    blk = s.alloc(0x80)
    s.words(blk, c["pat"])
    s.begin(c, {1: blk})
    lib.tpw_sp_command_play(blk, c["on"])
    return {"log": s.end(), "answer": 1, "block": s.rd_words(blk, 32)}


# ---------------------------------------------------------------------------------------------------------------- RestartChannel / RestartAll
def chan_case(r, one):
    n = r.choice([1, 2, 3, 6]) if one else r.choice([0, 1, 2, 3, 6])
    live = [r.random() < 0.7 for _ in range(n)]
    c = {"n": n, "live": live, "index": r.randrange(n) if n else 0,
         "queues": {"send": [r.choice([0, 1, -1, 7])], "IfOk": [r.choice([0, -1, 5, -9])]}}
    return c


def chan_edges(one):
    def gen():
        r = random.Random(21 if one else 22)
        for n, live in ((1, [True]), (1, [False]), (2, [True, False]), (2, [False, True]), (3, [True, True, True]), (3, [False] * 3)):
            c = chan_case(r, one)
            c["n"], c["live"] = n, live
            c["index"] = r.randrange(n)
            yield c
        if not one:
            c = chan_case(r, one)
            c["n"], c["live"], c["index"] = 0, [], 0
            yield c
    return gen


def chan_ppc(one):
    def run(e, c):
        s = Ppc(e)
        vt = s.alloc(0x40)
        chans = [0] * c["n"]
        fixed = {}
        for i in range(c["n"]):
            if c["live"][i]:
                chans[i] = s.alloc(0x20)
                s.words(chans[i] + 0x10, [vt])
                fixed[chans[i]] = 10 + i
        arr = s.alloc(4 * c["n"] + 4)
        s.words(arr, chans)
        m = s.alloc(0x20)
        s.words(m + 4, [c["n"]])
        s.words(m + 0x14, [arr])
        buf = s.alloc(8)
        s.write(buf, b"\xAA" * 8)
        args = [buf, m, c["index"]] if one else [buf, m]
        s.call(c, fixed, RCHAN if one else RALL, args) if False else None
        _, _, log = s.call(c, fixed, RCHAN if one else RALL, args)
        return {"log": log, "answer": s32(s.rd_words(buf, 1)[0])}
    return run


def chan_host(one):
    def run(lib, c):
        s = Host(lib)
        chans = [s.alloc(0x20) if c["live"][i] else 0 for i in range(c["n"])]
        arr = s.alloc(8 * c["n"] + 8)
        s.words64(arr, chans)
        m = lib.tpw_sp_setup(c["n"], arr)
        s.begin(c, {10 + i: chans[i] for i in range(c["n"]) if chans[i]})
        ans = lib.tpw_sp_restart_channel(m, c["index"]) if one else lib.tpw_sp_restart_all(m)
        return {"log": s.end(), "answer": ans}
    return run


# ---------------------------------------------------------------------------------------------------------------- TpBody
def tp_case(r, kind=None):
    return {"queues": {"Remove": [r.choice([0, -1, 3, -5])]}, "arg": r.choice([0, 1, 2, 0xFFFFFFFF, r.getrandbits(32)]), "kind": kind}


def tp_edges():
    r = random.Random(31)
    for a in (0, 1, 0xFFFFFFFF):
        yield tp_case(r)
        c = tp_case(r)
        c["arg"] = a
        yield c


def tp_simple_ppc(entry, which):
    def run(e, c):
        s = Ppc(e)
        body = s.alloc(0x40)
        buf = s.alloc(8)
        s.write(buf, b"\xAA" * 8)
        args = [buf, body] if which == 1 else [buf, body, c["arg"]]
        _, _, log = s.call(c, {body: 1, INST[0]: 7}, entry, args)
        return {"log": log, "answer": s32(s.rd_words(buf, 1)[0])}
    return run


def tp_simple_host(which):
    def run(lib, c):
        s = Host(lib)
        body = s.alloc(0x40)
        s.begin(c, {1: body, 7: lib.tpw_ms_addr(b"inst")})
        ans = lib.tpw_tp_simple(which, body, c["arg"])
        return {"log": s.end(), "answer": ans}
    return run


def tick_ppc(e, c):
    s = Ppc(e)
    body = s.alloc(0x40)
    _, _, log = s.call(c, {body: 1}, TP_TICK, [body])
    return {"log": log}


def tick_host(lib, c):
    s = Host(lib)
    body = s.alloc(0x40)
    s.begin(c, {1: body})
    lib.tpw_tp_tick(body)
    return {"log": s.end()}


# ---------------------------------------------------------------------------------------------------------------- destructors
def dtor_case(r):
    return {"queues": {}, "null": r.random() < 0.2,
            "flag": r.choice([-1, 0, 1, 2, 0x7FFF, -0x8000, r.randrange(-0x8000, 0x8000)]), "hi": r.choice([0, 0xFFFF, r.getrandbits(16)])}


def dtor_edges():
    r = random.Random(41)
    for null in (True, False):
        for flag in (-0x8000, -1, 0, 1, 2, 0x7FFF):
            c = dtor_case(r)
            c["null"], c["flag"] = null, flag
            yield c
    c = dtor_case(r)
    c["null"], c["flag"], c["hi"] = False, 0, 0xFFFF   # the high half of the register is ignored (extsh)
    yield c
    c = dtor_case(r)
    c["null"], c["flag"], c["hi"] = False, 1, 0xFFFF
    yield c


def dtor_ppc(entry, vt):
    def run(e, c):
        s = Ppc(e)
        obj = 0 if c["null"] else s.alloc(0x40)
        word = ((c["hi"] << 16) | (c["flag"] & 0xFFFF)) & MASK
        r3, _, log = s.call(c, {obj: 1} if obj else {}, entry, [obj, word])
        if vt and obj:
            assert s.rd_words(obj, 1)[0] == vt, "the destructor must store the vtable word"
        return {"log": log, "answer": 0 if r3 == 0 else (1 if r3 == obj else 2)}
    return run


def dtor_host(fn):
    def run(lib, c):
        s = Host(lib)
        obj = 0 if c["null"] else s.alloc(0x40)
        s.begin(c, {1: obj} if obj else {})
        r = getattr(lib, fn)(obj, c["flag"])
        return {"log": s.end(), "answer": 0 if r == 0 else (1 if r == obj else 2)}
    return run


# ---------------------------------------------------------------------------------------------------------------- square roots
SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000, 0x40800000,
           0x80000001, 0xFFC00000, 0x7F800001, 0x007FFFFF]


def sq_case(r, bits=None):
    if bits is None:
        k = r.random()
        if k < 0.15:
            bits = r.choice(SPECIAL)
        elif k < 0.6:
            bits = fb(r.uniform(-8, 1000))
        else:
            bits = fb(r.uniform(0, 1) * 10 ** r.uniform(-30, 30))
    return {"queues": {}, "bits": bits}


def sq_edges():
    r = random.Random(51)
    for b in SPECIAL:
        yield sq_case(r, b)
    for v in (2.0, 3.0, 0.5, 144.0, 1e-10, 1e10, 16777216.0, 16777217.0):
        yield sq_case(r, fb(v))


def sq_ppc(entry):
    def run(e, c):
        s = Ppc(e)
        _, f1, log = s.call(c, {}, entry, [T.F(bitsf(c["bits"]))])
        return {"log": log, "answer": canon_f(fb(f1))}
    return run


def sq_host(fn):
    def run(lib, c):
        s = Host(lib)
        s.begin(c, {})
        ans = getattr(lib, fn)(c["bits"])
        return {"log": s.end(), "answer": canon_f(ans)}
    return run


def edges_of(case_fn, extra=()):
    def gen():
        r = random.Random(61)
        for _ in range(4):
            yield case_fn(r)
    return gen


TESTS = [
    FnTest("TbMapStreamer::FixUpCatagoryIndex", fix_ppc, fix_host, edge=fix_edges, rnd=lambda r: fix_case(r)),
    FnTest("TbMapStreamer::UnloadCatgoryFixupTable", unload_ppc, unload_host, edge=edges_of(unload_case), rnd=unload_case),
    FnTest("SamsUtilities::UString::operator_char*", str_ppc, str_host, edge=edges_of(str_case), rnd=str_case),
    FnTest("bfl::FileSystem::CDiscFileStorage::CDiscFileStorage", disc_ppc, disc_host, edge=edges_of(ctor_case), rnd=ctor_case),
    FnTest("CAudioFileSystem::CAudioFileSystem", audfs_ppc, audfs_host, edge=edges_of(ctor_case), rnd=ctor_case),
    FnTest("CSprocketsModule::RestartChannel", chan_ppc(True), chan_host(True), edge=chan_edges(True), rnd=lambda r: chan_case(r, True)),
    FnTest("SamsUtilities::UCommandPlay::UCommandPlay", play_ppc, play_host, edge=edges_of(play_case), rnd=play_case),
    FnTest("CSprocketsModule::RestartAll", chan_ppc(False), chan_host(False), edge=chan_edges(False), rnd=lambda r: chan_case(r, False)),
    FnTest("TpBody::Init", tp_simple_ppc(TP_INIT, 0), tp_simple_host(0), edge=tp_edges, rnd=tp_case),
    FnTest("TpBody::Tick", tick_ppc, tick_host, edge=edges_of(tp_case), rnd=tp_case),
    FnTest("TpBody::Close", tp_simple_ppc(TP_CLOSE, 1), tp_simple_host(1), edge=tp_edges, rnd=tp_case),
    FnTest("TpBody::SetFades", tp_simple_ppc(TP_FADES, 2), tp_simple_host(2), edge=tp_edges, rnd=tp_case),
    FnTest("TpBody::DeregisterCatagoryBanks", tp_simple_ppc(TP_DEREG, 4), tp_simple_host(4), edge=tp_edges, rnd=tp_case),
    FnTest("TpBody::SetCatagoryReverb", tp_simple_ppc(TP_REVERB, 3), tp_simple_host(3), edge=tp_edges, rnd=tp_case),
    FnTest("TpBody::~TpBody", dtor_ppc(TP_DTOR, VT_TPBODY), dtor_host("tpw_tp_destruct"), edge=dtor_edges, rnd=dtor_case),
    FnTest("_CS_TbSoundSamplePipe::~_CS_TbSoundSamplePipe", dtor_ppc(CS_PIPE, 0), dtor_host("tpw_cs_pipe"), edge=dtor_edges, rnd=dtor_case),
    FnTest("TbError::TbError", err_ppc, err_host, edge=edges_of(err_case), rnd=err_case),
    FnTest("std::sqrtf", sq_ppc(SQRTF), sq_host("tpw_sq_sqrtf"), edge=sq_edges, rnd=sq_case),
    FnTest("std::sqrt", sq_ppc(SQRT), sq_host("tpw_sq_sqrt"), edge=sq_edges, rnd=sq_case),
    FnTest("_CS__TbSoundCodecLoop::~_CS__TbSoundCodecLoop", dtor_ppc(CS_LOOP, 0), dtor_host("tpw_cs_loop"), edge=dtor_edges, rnd=dtor_case),
]
