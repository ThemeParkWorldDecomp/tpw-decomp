"""Unit v5927, CThing::Move, FinalDestroy and Serialise, against libtpw_unit_CThing_Move_2.dylib (tests/unit_CThing_Move_2.cpp), main app,
src/Thing_receive_CThing_Move_2.cpp:
  CThing::Move 0x1012e5b4, CThing::FinalDestroy 0x1012e9f8, CThing::Serialise 0x1012e010

A case is a thing (a 0x40 byte block with the 12 byte common part and a ride word at +0x1c), the map pointer, the selection global and the
message centre global, built twice by one piece of code. The callees (GAME_ASSERTMSG, CMapCell::RemoveThing/InsertThing, DestroyRideInstance,
operator new, __construct_array, CMessageCentre::UnsubscribeAll, SAVEVAR/LOADVAR and the struct forms) are fakes on both sides that record
every argument (cell addresses and pointers as offsets from the map or the thing, names as text) and answer from the case's queues.

Compared: the answer, the thing's fields, gESPF20 (the thing itself or another value), the message centre global (0, the preset block or
a new one) and the event log. No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CThing_Move_2"
MASK = 0xFFFFFFFF

FN_MOVE, FN_FINAL, FN_SER = 0x1012E5B4, 0x1012E9F8, 0x1012E010
SIZES = {FN_MOVE: 324, FN_FINAL: 496, FN_SER: 444}
OWN = set(SIZES)

A_ASSERT, A_REMOVE, A_INSERT = 0x1000DDE8, 0x10102E54, 0x10102D34
A_DESTROY_RIDE, A_NEW, A_CONSTRUCT, A_UNSUB = 0x1006A3EC, 0x102384B8, 0x10238758, 0x10152448
A_SAVE, A_LOAD, A_SAVES, A_LOADS = 0x100F5658, 0x100F518C, 0x1000E9D4, 0x1000EB4C

ADDR = dict(gWorldMap=0x1037EC08, gESPF20=0x102DBD20, gMessageCentre=0x1037F6C4)
HOST_LAYOUT = [0x44, 4, 6, 8, 10, 12]

CTX = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mapoff(e, a):
    return s32(a - e.u32(ADDR["gWorldMap"]))


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (e.r(3) & 0xFF,), (e.cstr(e.r(4)),))


def stub_remove(e):
    e.cl.rec("RemoveThing", (mapoff(e, e.r(3)), (e.r(4) >> 16) & 0xFFFF))


def stub_insert(e):
    e.cl.rec("InsertThing", (mapoff(e, e.r(3)), (e.r(4) >> 16) & 0xFFFF))


def stub_destroy(e):
    e.cl.rec("DestroyRideInstance", (e.r(3),))


def stub_new(e):
    e.cl.rec("operator new", (e.r(3),))
    CTX["new"] = e.alloc(e.r(3))
    e.set_r(3, CTX["new"])


def stub_construct(e):
    assert e.r(4) == 0x102657E8 and e.r(5) == 0x10267288, (hex(e.r(4)), hex(e.r(5)))
    e.cl.rec("__construct_array", (1 if e.r(3) == CTX["new"] else 0, e.r(6), e.r(7)))


def stub_unsub(e):
    r3 = e.r(3)
    which = 1 if r3 == CTX["preset"] else (2 if r3 == CTX.get("new") else 0)
    e.cl.rec("UnsubscribeAll", (which, s32(e.r(4) - CTX["thing"])))


def stub_save(e):
    v = e.read(e.r(4), 2)
    e.cl.rec("SAVEVAR<short>", (s32(e.r(4) - CTX["thing"]), struct.unpack(">h", v)[0], s32(e.r(6))), (e.cstr(e.r(5)),))
    e.set_r(3, e.cl.ret("SAVEVAR") & MASK)


def stub_load(e):
    e.cl.rec("LOADVAR<short>", (s32(e.r(4) - CTX["thing"]), s32(e.r(6))), (e.cstr(e.r(5)),))
    e.write(e.r(4), struct.pack(">H", e.cl.ret("LOADVALUE") & 0xFFFF))
    e.set_r(3, e.cl.ret("LOADVAR") & MASK)


def stub_saves(e):
    v = e.read(e.r(4), 2)
    e.cl.rec("SAVEVARSTRUCT", (s32(e.r(4) - CTX["thing"]), struct.unpack(">H", v)[0], s32(e.r(6))), (e.cstr(e.r(5)),))
    e.set_r(3, e.cl.ret("SAVEVARSTRUCT") & MASK)


def stub_loads(e):
    e.cl.rec("LOADVARSTRUCT", (s32(e.r(4) - CTX["thing"]), s32(e.r(6))), (e.cstr(e.r(5)),))
    e.write(e.r(4), struct.pack(">H", e.cl.ret("LOADSTRUCTVALUE") & 0xFFFF))
    e.set_r(3, e.cl.ret("LOADVARSTRUCT") & MASK)


for a, f in ((A_ASSERT, stub_assert), (A_REMOVE, stub_remove), (A_INSERT, stub_insert), (A_DESTROY_RIDE, stub_destroy),
             (A_NEW, stub_new), (A_CONSTRUCT, stub_construct), (A_UNSUB, stub_unsub), (A_SAVE, stub_save), (A_LOAD, stub_load),
             (A_SAVES, stub_saves), (A_LOADS, stub_loads)):
    STUBS.internal(a, f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def put16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def put8(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def get16(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def get8(self, a):
        return self.read(a, 1)[0]

    def get32(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def putptr(self, a, v):
        """A pointer global: 4 bytes on the Mac, 8 on the host."""
        self.write(a, struct.pack(self.pfmt, v))

    def getptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        CTX.clear()
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK if fn == FN_SER else 0

    def new_block(self):
        return CTX.get("new")


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cm2_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cm2_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        t = self.thing
        if fn == FN_MOVE:
            self.lib.tpw_cm2_move(t, *[a & 0xFF for a in args[1:]])
            return 0
        if fn == FN_FINAL:
            self.lib.tpw_cm2_final(t)
            return 0
        return self.lib.tpw_cm2_serialise(t, 0x12340, s32(args[2]), s32(args[3]), 0) & MASK

    def new_block(self):
        return None


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cm2_addr.restype, lib.tpw_cm2_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cm2_alloc.restype, lib.tpw_cm2_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cm2_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cm2_move.restype, lib.tpw_cm2_move.argtypes = None, [C.c_uint64] + [C.c_uint32] * 4
    lib.tpw_cm2_final.restype, lib.tpw_cm2_final.argtypes = None, [C.c_uint64]
    lib.tpw_cm2_serialise.restype = C.c_uint32
    lib.tpw_cm2_serialise.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cm2_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    t = s.alloc(0x40)
    s.thing = t
    CTX["thing"] = t
    th = case["thing"]
    s.put16(t, th["id"])
    s.put8(t + 2, th["kind"])
    s.put8(t + 3, th["dead"])
    for i, v in enumerate(th["pos"]):
        s.put8(t + 4 + i, v)
    s.put16(t + 8, th["prev"])
    s.put16(t + 10, th["next"])
    s.words(t + 0x1C, [th["ride"]])
    s.putptr(s.g("gWorldMap"), s.alloc(0x100))
    s.putptr(s.g("gESPF20"), t if case["sel_is_thing"] else case["sel"])
    s.preset = s.alloc(0x1D0) if case["has_mc"] else 0
    CTX["preset"] = s.preset
    s.putptr(s.g("gMessageCentre"), s.preset)


def execute(s, case):
    if case["op"] == "move":
        return s.call(FN_MOVE, [s.thing, case["x"][0] << 24 | case["x"][1] << 16, case["y"][0] << 24 | case["y"][1] << 16]
                      if not isinstance(s, Host) else [s.thing] + [case["x"][0], case["x"][1], case["y"][0], case["y"][1]])
    if case["op"] == "final":
        return s.call(FN_FINAL, [s.thing])
    return s.call(FN_SER, [s.thing, 0x12340, case["save"], case["version"], 0])


def observe(s, case, answer):
    t = s.thing
    sel = s.get32(s.g("gESPF20"))
    mc = s.getptr(s.g("gMessageCentre"))
    t32 = t & MASK
    preset32 = s.preset & MASK
    return {"answer": answer, "log": s.log(),
            "id": s.get16(t), "kind": s.get8(t + 2), "dead": s.get8(t + 3), "pos": [s.get8(t + 4 + i) for i in range(4)],
            "prev": s.get16(t + 8), "next": s.get16(t + 10), "ride": s.get32(t + 0x1C),
            "sel": "thing" if sel == t32 else sel,
            "mc": 0 if mc == 0 else ("preset" if mc & MASK == preset32 else "new")}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    kind = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x12, 0x13, r.randrange(256)])
    return {"op": op,
            "thing": {"id": r.choice([0, 1, 0xFFFF, r.randrange(65536)]), "kind": kind, "dead": r.choice([0, 1, 5]),
                      "pos": [r.choice([0, 1, 127, 255, r.randrange(256)]) for _ in range(4)],
                      "prev": r.randrange(65536), "next": r.randrange(65536), "ride": r.randrange(1 << 32)},
            "sel_is_thing": r.choice([True, False]), "sel": r.choice([0, 0x1234, r.randrange(1 << 32)]),
            "has_mc": r.choice([True, False]),
            "queues": {"SAVEVAR": [r.choice([0, 0, 1]) for _ in range(4)], "SAVEVARSTRUCT": [r.choice([0, 0, 1]) for _ in range(4)],
                       "LOADVAR": [r.choice([0, 0, 1]) for _ in range(4)], "LOADVARSTRUCT": [r.choice([0, 0, 1]) for _ in range(4)],
                       "LOADVALUE": [r.randrange(65536) for _ in range(4)], "LOADSTRUCTVALUE": [r.randrange(65536) for _ in range(4)]},
            "save": r.choice([0, 1]), "version": r.choice([0, 1, 2, -1, 5]), "x": (0, 0), "y": (0, 0)}


def move_case(r, same=None):
    c = base(r, "move")
    px = c["thing"]["pos"]
    same = r.choice([True, False]) if same is None else same
    if same:
        c["x"] = (px[0], r.randrange(256))
        c["y"] = (px[2], r.randrange(256))
    else:
        c["x"] = (r.choice([0, 1, 127, 128, 255, r.randrange(256)]), r.randrange(256))
        c["y"] = (r.choice([0, 1, 127, 128, 255, r.randrange(256)]), r.randrange(256))
        if r.random() < 0.2:
            c["x"] = (px[0], c["x"][1])
    return c


def move_edges():
    r = random.Random(11)
    for same in (True, False, None, None):
        yield move_case(r, same)
    for cell in ((0, 0), (255, 0), (0, 128), (0, 127), (128, 128), (255, 255), (0, 129)):
        c = move_case(r, False)
        c["thing"]["pos"] = [1, 0, 1, 0]
        c["x"], c["y"] = (cell[0], 3), (cell[1], 4)
        yield c
    c = move_case(r, False)
    c["thing"]["pos"] = [0, 0, 0, 0]
    c["x"], c["y"] = (5, 1), (5, 2)
    yield c
    c = move_case(r, False)
    c["thing"]["pos"] = [255, 0, 255, 0]
    c["x"], c["y"] = (0, 1), (0, 2)
    yield c


def move_random(r):
    return move_case(r)


def final_case(r, kind=None, sel=None, mc=None):
    c = base(r, "final")
    if kind is not None:
        c["thing"]["kind"] = kind
    if sel is not None:
        c["sel_is_thing"] = sel
    if mc is not None:
        c["has_mc"] = mc
    return c


def final_edges():
    r = random.Random(12)
    for kind in range(0, 0x15):
        for sel in (True, False):
            yield final_case(r, kind, sel, r.choice([True, False]))
    for mc in (True, False):
        yield final_case(r, 3, True, mc)
    yield final_case(r, 0xFF, True, False)


def final_random(r):
    return final_case(r)


def ser_case(r, save=None, version=None):
    c = base(r, "ser")
    if save is not None:
        c["save"] = save
    if version is not None:
        c["version"] = version
    return c


def ser_edges():
    r = random.Random(13)
    for save in (0, 1):
        for version in (-1, 0, 1, 2, 3):
            yield ser_case(r, save, version)
        for fail in range(5):
            for k in ("SAVEVAR", "SAVEVARSTRUCT", "LOADVAR", "LOADVARSTRUCT"):
                c = ser_case(r, save, 1)
                for kk in c["queues"]:
                    if kk.endswith("VAR") or kk.endswith("STRUCT"):
                        c["queues"][kk] = [0, 0, 0, 0]
                c["queues"][k] = [0] * fail + [1, 0]
                yield c


def ser_random(r):
    return ser_case(r)


TESTS = [
    FnTest("CThing::Move", ppc, host, edge=move_edges, rnd=move_random),
    FnTest("CThing::FinalDestroy", ppc, host, edge=final_edges, rnd=final_random),
    FnTest("CThing::Serialise", ppc, host, edge=ser_edges, rnd=ser_random),
]
