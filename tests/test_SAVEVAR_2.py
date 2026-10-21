"""Unit SAVEVAR_2: SAVEVAR<ESP_Sprites> and the static initialiser of Peep.cpp, against libtpw_unit_SAVEVAR_2.dylib (tests/unit_SAVEVAR_2.cpp), main app
("SimTheme Park"), src/game/people/Peep.cpp:
  SAVEVAR<ESP_Sprites> 0x101162d4, __sinit_Peep_cpp 0x10116418

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian, host memory little-endian).
Callees outside the unit are fakes on both sides: CSaveLoad::WriteSerialiseLog, GAME_ASSERTMSG (condition and text), LbFile_Write (file, the
bytes written as a big-endian word, the file's bytes on both sides, length; it answers a result and a count from the case),
__register_global_object (object offset, destructor identity, node offset).

The world of a case: the sprite word, the zero word at 0x102a0f04, noLog, the write's result and count, the name; for the initialiser junk in
the nine steps and the two words. Compared: answer, the log, the nine steps and the two words. No fused operations. Not tested: nothing.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SAVEVAR_2"
MASK = 0xFFFFFFFF

FN_SAVE, FN_SINIT = 0x101162D4, 0x10116418
SIZES = {FN_SAVE: 260, FN_SINIT: 504}
OWN = set(SIZES)

ASSERT = 0x1000DDE8
LOGFN = 0x1015B3DC

ADDR = dict(zero=0x102A0F04, saveload=0x1037FEFC, steps=0x1034C3AC, nodes=0x1034C3F4, w498=0x1034C498, w49c=0x1034C49C)
HOST_LAYOUT = [8, 108]
DTOR = 0x102663D0


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(ASSERT, CL.recorder("Assert", "is"))
STUBS.internal(LOGFN, CL.recorder("WriteSerialiseLog", "Lisi"))


def stub_write(e):
    L = e.cl
    word = e.u32(e.r(5))
    L.rec("LbFile_Write", (L.lab(e.r(4)), word, e.r(6) & MASK))
    e.write(e.r(3), struct.pack(">I", L.ret("wr") & MASK))
    e.write(e.r(7), struct.pack(">I", L.ret("wn") & MASK))


def stub_register(e):
    L = e.cl
    L.rec("Register", (s32(e.r(3) - ADDR["steps"]), 1 if e.r(4) == DTOR else 0, s32(e.r(5) - ADDR["nodes"])))


STUBS.imp("LbFile_Write__FPvPCvUlPUl", stub_write)
STUBS.imp("__register_global_object", stub_register)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["saveload"]: 1, ADDR["nodes"]: 2}, case["queues"])
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

    def save(self, file, value, name, nolog):
        r3, _, _ = T.call_ppc(self.e, FN_SAVE, [file, value, name, nolog & MASK])
        return r3 & MASK

    def sinit(self):
        T.call_ppc(self.e, FN_SINIT, [])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("saveload"), 2: self.g("nodes")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sv_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sv_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def save(self, file, value, name, nolog):
        return self.lib.tpw_sv_save(file, value, name, s32(nolog)) & MASK

    def sinit(self):
        self.lib.tpw_sv_sinit()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sv_addr.restype, lib.tpw_sv_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sv_alloc.restype, lib.tpw_sv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sv_save.restype, lib.tpw_sv_save.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_sv_sinit.restype, lib.tpw_sv_sinit.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_sv_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("zero"), [case["zero"]])
    s.words(s.g("steps"), case.get("junk", [0] * 18))
    s.words(s.g("w498"), [case.get("j498", 0)])
    s.words(s.g("w49c"), [case.get("j49c", 0)])
    s.words(s.g("nodes"), [0] * 27)
    s.val = s.alloc(8)
    s.words(s.val, [case["value"]])
    s.name = s.alloc(32)
    s.write(s.name, case["name"].encode() + b"\0")


def execute(s, case):
    if case["op"] == "save":
        return s.save(s.g("nodes"), s.val, s.name, case["nolog"])
    return s.sinit()


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "steps": s.rd_words(s.g("steps"), 18), "w498": s.rd_words(s.g("w498"), 1),
            "w49c": s.rd_words(s.g("w49c"), 1), "val": s.rd_words(s.val, 1), "nodes": s.rd_words(s.g("nodes"), 27)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


DBG = {}


def ppc(e, case):
    r = run(Ppc(e, case), case)
    DBG["p"] = r
    return r


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    c = base2(r, op)
    if op == "save":
        for k in ("j498", "j49c", "junk"):
            del c[k]
    return c


def base2(r, op):
    return {"op": op, "zero": r.choice([0, 0, 1, 0x80000000, r.getrandbits(32)]), "value": r.getrandbits(32), "nolog": r.choice([0, 1, -1, 7]),
            "name": r.choice(["a", "Sprite", "x y"]),
            "queues": {"wr": [r.choice([0, 1, -1, 5, -0x80000000, 0x7FFFFFFF])], "wn": [r.choice([4, 4, 0, 3, 5, 8])]},
            "j498": r.getrandbits(32), "j49c": r.getrandbits(32), "junk": [r.getrandbits(32) for _ in range(18)]}


def save_case(r, **kw):
    c = base(r, "save")
    c.update(kw)
    for k in ("wr", "wn"):
        if k in kw:
            c["queues"][k] = [kw[k]]
    return c


def save_edges():
    r = random.Random(5237)
    for zero in (0, 1):
        for wr in (0, 1, -1, 0x7FFFFFFF, -0x80000000):
            for wn in (4, 3, 5, 0):
                for nolog in (0, 1):
                    yield save_case(r, zero=zero, wr=wr, wn=wn, nolog=nolog)
    for v in (0, 1, 0x80000000, 0xFFFFFFFF, 0x12345678, 0x000000FF, 0xFF00FF00):
        yield save_case(r, value=v, zero=0, wr=0, wn=4, nolog=0)


def save_random(r):
    return base(r, "save")


def sinit_edges():
    r = random.Random(77)
    yield base(r, "sinit")


def sinit_random(r):
    return base(r, "sinit")


TESTS = [
    FnTest("SAVEVAR<ESP_Sprites>", ppc, host, edge=save_edges, rnd=save_random),
    FnTest("__sinit_Peep_cpp", ppc, host, edge=sinit_edges, rnd=sinit_random),
]
