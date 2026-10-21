"""Unit v5133, COnlinePerson's UpdateTurn, ModelState, IsThisMyName, SetDest (cell), SetDest (x, y), Teleport and ProcessWalkingToDest, against
libtpw_unit_COnlinePerson_SetDest.dylib (tests/unit_COnlinePerson_SetDest.cpp), main app, src/game/online/OnlinePerson.cpp:
  UpdateTurn 0x10212ff0, ModelState 0x10213080, IsThisMyName 0x10213124, SetDest 0x102131e0, SetDest 0x102132a0, Teleport 0x1021334c,
  ProcessWalkingToDest 0x1021341c

A case is a person block (0x3a8 bytes: MapPos pair at +4/+6, name at +0x19c, state word +0x39c, own-avatar byte +0x3a0) and a call, built twice by
one piece of Python code (big-endian in the emulator, little-endian on the host). The callees outside the unit are fakes on both sides that record
their arguments (the person as an offset into its block): CPeep::SetDest x2, Teleport, UpdateTurn, SpawnParticleEffect, PeepWalks,
CDelayedScript::SetSpriteScript, ProcessIdle, the COnlineControl constructor and UpdateAvatarPos, operator new, CTRACE and wcscmp (strings as
length and hash).

What is compared: the answer, the event log, the person's first words, state word and flag byte, and the gOnlineControl global (none, kept, new).
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlinePerson_SetDest"
MASK = 0xFFFFFFFF

FN = dict(update_turn=0x10212FF0, model_state=0x10213080, is_my_name=0x10213124, setdest_cell=0x102131E0, setdest_pos=0x102132A0,
          teleport=0x1021334C, walk=0x1021341C)
SIZES = {0x10212FF0: 96, 0x10213080: 116, 0x10213124: 68, 0x102131E0: 104, 0x102132A0: 112, 0x1021334C: 92, 0x1021341C: 240}
OWN = set(SIZES)

ADDR = dict(gOnlineControl=0x10396D28)
HOST_LAYOUT = [2]

CUR = {"person": 0}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def off(e):
    return (e.r(3) - CUR["person"]) & MASK


def hashed(chars):
    h = 0
    for c in chars:
        h = (h * 31 + c) & MASK
    return h


def rd_u16z(e, a):
    out = []
    while True:
        c = struct.unpack(">H", e.read(a, 2))[0]
        if c == 0:
            return out
        out.append(c)
        a += 2


def pos(w):
    return (w >> 16) & 0xFFFF


def mk_rec(name, fields, retkey=None):
    def fn(e):
        e.cl.rec(name, tuple(fields(e)))
        if retkey:
            e.set_r(3, e.cl.ret(retkey) & MASK)
    return fn


STUBS.internal(0x10115400, mk_rec("SetDestCell", lambda e: (off(e), pos(e.r(4))), "SetDestCell"))
STUBS.internal(0x101154F0, mk_rec("SetDestPos", lambda e: (off(e), pos(e.r(4)), pos(e.r(5))), "SetDestPos"))
STUBS.internal(0x101159D0, mk_rec("Teleport", lambda e: (off(e), pos(e.r(4)), pos(e.r(5)))))
STUBS.internal(0x101158C8, mk_rec("UpdateTurn", lambda e: (off(e),)))
STUBS.internal(0x101152A4, mk_rec("Particle", lambda e: (off(e), T.s32(e.r(4)))))
STUBS.internal(0x10114FA8, mk_rec("PeepWalks", lambda e: (off(e),), "PeepWalks"))
STUBS.internal(0x100FAA74, mk_rec("Script", lambda e: (off(e), T.s32(e.r(4)))))
STUBS.internal(0x102133E4, mk_rec("Idle", lambda e: (off(e),)))
STUBS.internal(0x10213870, CL.recorder("ctor", "N"))
STUBS.internal(0x1021823C, mk_rec("UpdateAvatarPos", lambda e: (e.cl.lab(e.r(3)), pos(e.r(4)), pos(e.r(5)))))


def stub_new(e):
    size = e.r(3)
    e.cl.rec("new", (size,))
    e.set_r(3, e.alloc(size))


def stub_ctrace(e):
    s = e.cstr(e.r(4))
    data = s if isinstance(s, bytes) else s.encode("latin-1")
    e.cl.rec("CTRACE", (e.r(3), len(data), hashed(data)))


def stub_wcscmp(e):
    a, b = rd_u16z(e, e.r(3)), rd_u16z(e, e.r(4))
    e.cl.rec("wcscmp", (len(a), hashed(a), len(b), hashed(b)))
    i = 0
    while i < len(a) and i < len(b) and a[i] == b[i]:
        i += 1
    ca = a[i] if i < len(a) else 0
    cb = b[i] if i < len(b) else 0
    e.set_r(3, (ca - cb) & MASK)


STUBS.internal(0x102384B8, stub_new)
STUBS.internal(0x1006FCBC, stub_ctrace)
STUBS.imp("wcscmp", stub_wcscmp)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def u16z(self, a, text):
        chars = [ord(c) for c in text] + [0]
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(chars), *chars))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
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

    def put_ptr(self, a, v):
        self.words(a, [v])

    def get_ptr(self, a):
        return self.rd_words(a, 1)[0]

    def name_arg(self, data):
        obj, rep = self.alloc(4), self.alloc(0x10)
        self.words(obj, [rep])
        self.words(rep + 0xC, [data])
        return obj

    def call(self, op, p, args, name):
        a = [p]
        if op == "setdest_cell":
            a.append(args[0] << 16)
        elif op in ("setdest_pos", "teleport"):
            a += [(args[0] << 24) | (args[1] << 16), (args[2] << 24) | (args[3] << 16)]
        elif op == "is_my_name":
            a.append(name)
        r3, _, _ = T.call_ppc(self.e, FN[op], a)
        return r3 & 0xFF if op in ("is_my_name", "setdest_cell", "setdest_pos") else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_op_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_op_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def put_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def get_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def name_arg(self, data):
        return data

    def call(self, op, p, args, name):
        f = getattr(self.lib, "tpw_op_" + op)
        if op == "is_my_name":
            return f(p, name) & 0xFF
        if op == "setdest_cell":
            return f(p, args[0]) & 0xFF
        if op in ("setdest_pos", "teleport"):
            r = f(p, *args)
            return r & 0xFF if op == "setdest_pos" else 0
        f(p)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_op_addr.restype, lib.tpw_op_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_op_alloc.restype, lib.tpw_op_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_op_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("update_turn", "model_state", "walk"):
        f = getattr(lib, "tpw_op_" + n)
        f.restype, f.argtypes = None, [C.c_uint64]
    lib.tpw_op_is_my_name.restype, lib.tpw_op_is_my_name.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    lib.tpw_op_setdest_cell.restype, lib.tpw_op_setdest_cell.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32]
    lib.tpw_op_setdest_pos.restype, lib.tpw_op_setdest_pos.argtypes = C.c_uint32, [C.c_uint64] + [C.c_uint32] * 4
    lib.tpw_op_teleport.restype, lib.tpw_op_teleport.argtypes = None, [C.c_uint64] + [C.c_uint32] * 4
    out = (C.c_int64 * 32)()
    n = lib.tpw_op_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        p = s.alloc(0x3A8)
        CUR["person"] = p
        s.write(p + 4, bytes(case["pos"]))
        s.words(p + 0x39C, [case["state"]])
        s.write(p + 0x3A0, bytes([case["flag"]]))
        s.u16z(p + 0x19C, case["myname"])
        g = s.g("gOnlineControl")
        existing = s.alloc(0x10) if case["oc"] else 0
        s.put_ptr(g, existing)
        data = s.alloc(2 * (len(case["other"]) + 1))
        s.u16z(data, case["other"])
        answer = s.call(case["op"], p, case["args"], s.name_arg(data))
        now = s.get_ptr(g)
        oc = 0 if now == 0 else (1 if now == existing else 2)
        return {"answer": answer, "log": s.log(), "head": s.read(p, 32), "state": s.rd_words(p + 0x39C, 1),
                "flag": s.read(p + 0x3A0, 1), "oc": oc}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


NAMES = ["", "Bob", "bob", "Alice", "Bobby", "Bo"]


def mk(r, op, **kw):
    c = {"op": op, "state": r.choice([0, 1, 2, 3, 4, -1, 5, r.randrange(-3, 8)]), "flag": r.choice([0, 1, 0x80, 2]),
         "pos": [r.randrange(256) for _ in range(4)], "oc": r.choice([0, 1]),
         "myname": r.choice(NAMES), "other": r.choice(NAMES), "args": [],
         "queues": {"SetDestCell": [r.choice([0, 1, 5])], "SetDestPos": [r.choice([0, 1, 3])], "PeepWalks": [r.choice([0, 1, 2, 3, -1])]}}
    if op == "setdest_cell":
        c["args"] = [r.randrange(0x10000)]
    elif op in ("setdest_pos", "teleport"):
        c["args"] = [r.randrange(256) for _ in range(4)]
    c.update(kw)
    return c


def edges(op):
    def gen():
        r = random.Random(sum(map(ord, op)))
        if op in ("model_state", "walk"):
            for st in (-1, 0, 1, 2, 3, 4, 5):
                for w in (0, 1, 2, 3, -1):
                    for flag in (0, 1):
                        for oc in (0, 1):
                            yield mk(r, op, state=st, flag=flag, oc=oc, queues={"PeepWalks": [w]})
        elif op == "update_turn":
            for flag in (0, 1, 0x80):
                yield mk(r, op, flag=flag)
        elif op == "is_my_name":
            for a in NAMES:
                for b in NAMES:
                    yield mk(r, op, myname=a, other=b)
        elif op == "setdest_cell":
            for v in (0, 1, 5):
                yield mk(r, op, queues={"SetDestCell": [v]})
        elif op == "setdest_pos":
            for v in (0, 1, 3):
                yield mk(r, op, queues={"SetDestPos": [v]})
        else:
            for st in (0, 1, 2):
                yield mk(r, op, state=st)
    return gen


def rnd(op):
    return lambda r: mk(r, op)


TESTS = [
    FnTest("COnlinePerson::UpdateTurn", ppc, host, edge=edges("update_turn"), rnd=rnd("update_turn")),
    FnTest("COnlinePerson::ModelState", ppc, host, edge=edges("model_state"), rnd=rnd("model_state")),
    FnTest("COnlinePerson::IsThisMyName", ppc, host, edge=edges("is_my_name"), rnd=rnd("is_my_name")),
    FnTest("COnlinePerson::SetDest", ppc, host, edge=edges("setdest_cell"), rnd=rnd("setdest_cell")),
    FnTest("COnlinePerson::SetDest", ppc, host, edge=edges("setdest_pos"), rnd=rnd("setdest_pos")),
    FnTest("COnlinePerson::Teleport", ppc, host, edge=edges("teleport"), rnd=rnd("teleport")),
    FnTest("COnlinePerson::ProcessWalkingToDest", ppc, host, edge=edges("walk"), rnd=rnd("walk")),
]
