"""Unit v5937, the advisor's save struct, last minute check and request serialiser, against libtpw_unit_SAVEVARSTRUCT_2.dylib
(tests/unit_SAVEVARSTRUCT_2.cpp), main app ("SimTheme Park"), src/game/advisor/Advisor.cpp, src/game/advisor/AdvisorScoring.cpp:
  SAVEVARSTRUCT<AdvisorHistory> 0x1000f96c, CAdvisorScoring::LastMinuteCheck 0x100102d8, CAdvisorRequest::Serialise 0x1000e568

A case is a world and a call, built twice by one piece of Python code (emulator memory big-endian, host memory little-endian). The callees
outside the unit are fakes on both sides that record every argument (names and format strings as FNV-1a hashes): WriteSerialiseLog,
GAME_ASSERTMSG, CTRACE, LbFile_Write (the 16 bytes it is given, padding bytes 9 to 11 masked: the original writes stack garbage there),
IsThisValidInTheCurrentMode, the scorer glue (the event says whether the record it was called through is the message's own record, and
the scoring object's two words as they were) and the ten SAVEVAR / LOADVAR instances.

What is compared: the answer, the event log, the struct / request / scoring object bytes, the two out references.
No fused operations. Not tested: messages outside -1 .. 0x15e other than 0x15f and 0x160 (the original reads past the table, whose
layout differs on the host); the zero word at 0x1027dec8 (it never changes).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SAVEVARSTRUCT_2"
MASK = 0xFFFFFFFF

FN_SVS, FN_LMC, FN_SER = 0x1000F96C, 0x100102D8, 0x1000E568
SIZES = {FN_SVS: 296, FN_LMC: 276, FN_SER: 756}
OWN = set(SIZES)

TABLE = 0x1027EFA4
FILE = 0x10300000
SAVELOAD = 0x1037FEFC
HOST_LAYOUT = [4, 8, 4, 16, 0x18, 0xC, 0x16]

CUR = {"msg": 0}


def fnv(text):
    if isinstance(text, str):
        text = text.encode("latin1")
    h = 2166136261
    for b in text:
        h = ((h ^ b) * 16777619) & MASK
    return h


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_wsl(e):
    e.cl.rec("WriteSerialiseLog", (e.cl.lab(e.r(3)), s32(e.r(4)), fnv(e.cstr(e.r(5))), s32(e.r(6))))


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)), fnv(e.cstr(e.r(4)))))


def stub_ctrace(e):
    e.cl.rec("CTRACE", (e.r(3) & MASK, s32(e.r(5)), fnv(e.cstr(e.r(4)))))


def stub_write(e):
    L = e.cl
    d = e.read(e.r(5), 16)
    w0, w1, w2, w3 = struct.unpack(">IIII", d)
    L.rec("LbFile_Write", (L.lab(e.r(4)), e.r(6) & MASK, w0, w1, w2 & 0xFF000000, w3))
    code = L.ret("wr")
    n = L.ret("wn")
    e.write(e.r(3), struct.pack(">I", code & MASK))
    e.write(e.r(7), struct.pack(">I", n & MASK))


def stub_valid(e):
    L = e.cl
    o = e.r(3)
    L.rec("IsValid", (L.lab(o), s32(e.r(4)), struct.unpack(">H", e.read(o + 4, 2))[0], struct.unpack(">I", e.read(o + 8, 4))[0]))
    e.set_r(3, 1 if L.ret("valid") else 0)


def stub_scorer(e):
    L = e.cl
    o = e.r(3)
    target = 1 if e.r(12) == TABLE + 0x30 * CUR["msg"] + 0x10 else 0
    L.rec("Scorer", (L.lab(o), target, struct.unpack(">H", e.read(o + 4, 2))[0], struct.unpack(">I", e.read(o + 8, 4))[0]))
    e.write(o + 4, struct.pack(">H", L.ret("w4") & 0xFFFF))
    e.write(o + 8, struct.pack(">I", L.ret("w8") & MASK))
    e.set_r(3, L.ret("score") & MASK)


def make_var(name):
    def fn(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), s32(e.r(4) - CUR["obj"]), s32(e.r(6)), fnv(e.cstr(e.r(5)))))
        e.set_r(3, L.ret("ret") & MASK)
    return fn


CALLEES = [(0x1000E9D4, "SAVEVARSTRUCT_thing"), (0x1000EB4C, "LOADVARSTRUCT_thing"), (0x1000F5DC, "SAVEVAR_ulong"),
           (0x1000F02C, "LOADVAR_ulong"), (0x1000F820, "SAVEVAR_msg"), (0x1000F250, "LOADVAR_msg"), (0x1000F4B0, "SAVEVAR_int"),
           (0x1000EF18, "LOADVAR_int"), (0x1000F70C, "SAVEVAR_bool"), (0x1000F144, "LOADVAR_bool")]

STUBS.internal(0x1015B3DC, stub_wsl)
STUBS.internal(0x1000DDE8, stub_assert)
STUBS.internal(0x1006FCBC, stub_ctrace)
STUBS.imp("LbFile_Write__FPvPCvUlPUl", stub_write)
STUBS.internal(0x10010CC8, stub_valid)
STUBS.internal(0x1023BA70, stub_scorer)
for _a, _n in CALLEES:
    STUBS.internal(_a, make_var(_n))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


def labels(obj):
    d = {FILE: 2, SAVELOAD: 1, obj: 5}
    for off in (4, 8, 0xC, 0x10, 0x14, 0x15, 0x16):
        d[obj + off] = 0x200 + off
    return d


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(0x100)
        self.L = e.cl = CL.PpcLog(labels(self.obj), case["queues"])
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

    def setmsg(self, i, v):
        self.e.write(TABLE + 0x30 * i + 4, struct.pack(">I", v & MASK))

    def call(self, entry, args):
        return T.call_ppc(self.e, entry, [a & MASK for a in args])[0] & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_adv_alloc(0x4000)
        fixed = {v: k for k, v in labels(self.obj).items()}
        fixed[1] = lib.tpw_adv_saveload()
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_adv_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setmsg(self, i, v):
        self.lib.tpw_adv_setmsg(i, s32(v))

    def call(self, entry, args):
        L = self.lib
        if entry == FN_SVS:
            return L.tpw_adv_serstruct(args[0], args[1], args[2], s32(args[3])) & MASK
        if entry == FN_LMC:
            return L.tpw_adv_lmc(args[0], s32(args[1]), args[2], args[3]) & MASK
        return L.tpw_adv_ser(args[0], args[1], s32(args[2]), s32(args[3]), args[4] & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_adv_alloc.restype, lib.tpw_adv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_adv_saveload.restype, lib.tpw_adv_saveload.argtypes = C.c_uint64, []
    lib.tpw_adv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_adv_setmsg.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_adv_serstruct.restype, lib.tpw_adv_serstruct.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_adv_lmc.restype, lib.tpw_adv_lmc.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_uint64, C.c_uint64]
    lib.tpw_adv_ser.restype = C.c_uint32
    lib.tpw_adv_ser.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_adv_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def struct_bytes(s, raw):
    """The record's three words (+0, +4, +0xc) as the same values on both sides, in the side's order; the byte at +8 and the padding raw
    (v5L15: the host writes the values in file order without reversing them, so both sides must hold the same values)."""
    w0, w1, w3 = struct.unpack(">I", raw[0:4])[0], struct.unpack(">I", raw[4:8])[0], struct.unpack(">I", raw[12:16])[0]
    f = s.fmt
    return struct.pack(f, w0) + struct.pack(f, w1) + raw[8:12] + struct.pack(f, w3)


def execute(s, case):
    op = case["op"]
    if op == "svs":
        state = s.alloc(16)
        name = s.alloc(32)
        s.write(state, struct_bytes(s, case["bytes"]))
        s.write(name, case["name"] + b"\0")
        s.state = state
        return s.call(FN_SVS, [FILE, state, name, case["nolog"]])
    if op == "lmc":
        msg = case["msg"]
        CUR["msg"] = msg
        for i in range(-1, 0x15F):
            s.setmsg(i, case["over"].get(i, i))
        f = s.fmt[0]
        s.write(s.obj + 4, struct.pack(f + "HHI", case["pre"][0], 0, case["pre"][1]))
        out = s.alloc(16)
        s.write(out, struct.pack(f + "H", case["outs"][0]))
        s.write(out + 8, struct.pack(f + "I", case["outs"][1]))
        s.idx = out
        s.ul = out + 8
        return s.call(FN_LMC, [s.obj, msg, s.idx, s.ul])
    CUR["obj"] = s.obj
    s.write(s.obj, case["bytes"])
    return s.call(FN_SER, [s.obj, FILE, case["save"], case["version"], case["unused"]])


def unstruct(data):
    """The host's record back in the PowerPC side's byte order (its three words reversed)."""
    def rev(b):
        return b[::-1]
    return rev(data[0:4]) + rev(data[4:8]) + data[8:12] + rev(data[12:16])


def observe(s, case, answer):
    d = {"answer": answer, "log": s.log()}
    if case["op"] == "svs":
        d["state"] = struct_bytes(s, s.read(s.state, 16)) if s.fmt[0] == ">" else unstruct(s.read(s.state, 16))
    elif case["op"] == "lmc":
        f = s.fmt[0]
        d["obj"] = struct.unpack(f + "HHI", s.read(s.obj + 4, 8))
        d["idx"] = struct.unpack(f + "H", s.read(s.idx, 2))[0]
        d["ul"] = struct.unpack(f + "I", s.read(s.ul, 4))[0]
    else:
        d["req"] = s.read(s.obj, 0x18)
    return d


def run(s, case):
    try:
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- generators

def rb(r, n):
    return bytes(r.randrange(256) for _ in range(n))


def svs_case(r, nolog=None, wr=None, wn=None):
    wr = r.choice([0, 0, 1, 5, -1, -7, 0x7FFFFFFF]) if wr is None else wr
    wn = r.choice([16, 16, 0, 15, 17, 4, r.randrange(1 << 32)]) if wn is None else wn
    return {"op": "svs", "bytes": rb(r, 16), "name": rb(r, 6).replace(b"\0", b"a"), "nolog": r.choice([0, 1, -1, 5]) if nolog is None else nolog,
            "queues": {"wr": [wr], "wn": [wn]}}


def svs_edges():
    r = random.Random(937)
    for nolog in (0, 1, 3):
        for wr in (0, -1, 1):
            for wn in (16, 0, 15, 17):
                yield svs_case(r, nolog, wr, wn)


def svs_random(r):
    return svs_case(r)


def lmc_case(r, msg=None, mode=None, valid=None):
    msg = r.choice([-1, 0, 1, 0x15E, 0x15F, 0x160, r.randrange(0, 0x15F)]) if msg is None else msg
    mode = r.choice(["plain", "loop", "none"]) if mode is None else mode
    over = {}
    if 0 <= msg - 0 and -1 <= msg <= 0x15E and msg != 0x15F:
        if mode == "loop":
            over[msg] = -5
            j = (msg + 7) % 0x15F if msg >= 0 else 3
            over[j] = msg
        elif mode == "none":
            over[msg] = -5
    elif msg == -1:
        pass
    if msg == -1 and mode != "plain":
        over = {-1: -5} if mode == "none" else {-1: -5, 3: -1}
    valid = r.choice([0, 1]) if valid is None else valid
    return {"op": "lmc", "msg": msg, "over": over, "pre": (r.randrange(1 << 16), r.randrange(1 << 32)), "outs": (r.randrange(1 << 16), r.randrange(1 << 32)),
            "queues": {"valid": [valid], "w4": [r.randrange(1 << 16)], "w8": [r.randrange(1 << 32)],
                       "score": [r.choice([0, 1, -1, 77, r.randrange(1 << 32)])]}}


def lmc_edges():
    r = random.Random(938)
    for msg in (-1, 0, 1, 0x15E, 0x15F, 0x160, 200):
        for mode in ("plain", "loop", "none"):
            for valid in (0, 1):
                yield lmc_case(r, msg, mode, valid)


def lmc_random(r):
    return lmc_case(r)


def ser_case(r, save=None, version=None, fail=None):
    save = r.choice([0, 1, 2, -1]) if save is None else save
    version = r.choice([0, 1, 1, 0, 2, -1, 5, -9]) if version is None else version
    fail = r.choice([-1, -1, 0, 1, 2, 3, 4, 5, 6, 7]) if fail is None else fail
    q = [0] if fail < 0 else [0] * fail + [r.choice([1, 2, -1])]
    return {"op": "ser", "save": save, "version": version, "unused": r.randrange(1 << 32), "bytes": rb(r, 0x18), "queues": {"ret": q}}


def ser_edges():
    r = random.Random(939)
    for save in (0, 1):
        for version in (-1, 0, 1, 2):
            for fail in range(-1, 8):
                yield ser_case(r, save, version, fail)


def ser_random(r):
    return ser_case(r)


TESTS = [
    FnTest("SAVEVARSTRUCT<AdvisorHistory>", ppc, host, edge=svs_edges, rnd=svs_random),
    FnTest("CAdvisorScoring::LastMinuteCheck", ppc, host, edge=lmc_edges, rnd=lmc_random),
    FnTest("CAdvisorRequest::Serialise", ppc, host, edge=ser_edges, rnd=ser_random),
]
