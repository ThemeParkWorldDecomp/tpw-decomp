"""Unit v5337, the coaster editor's brace deletion, against libtpw_unit_DeleteLastTrackBrace.dylib (tests/unit_DeleteLastTrackBrace.cpp), main app
("SimTheme Park"), src/game/rides/CoasterEdit.cpp:
  DeleteTrackBrace 0x10056660, DeleteLastTrackBrace 0x10056a80

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's offsets (big-endian,
4-byte pointers) for the original, and in the host library's memory (host offsets from tpw_dltb_layout, 8-byte pointers) for the
translation. The callees outside the unit (RemoveTrackBraceFromPylon, StopFlashingObject, RemoveBraceFromCollisionSystem, DestroyTrackBrace,
DeletePylon, Assign3DObjectToAIWorld, UpdatePylonStack*, SetPotentialPylonArea, FlagGenerateTrack, RecordAction, GetCoasterEditAvailableButtons,
SetCoasterButtonState, ChangeCoasterButtons*) are fakes on both sides recording every argument (objects as labels).

The world of a case: an instance with a type block, a list of 1 to 6 braces each with its pylon record, control, ride animation set (mesh
chain of three records) and owner block; the globals (edit instance, drag brace, input flags, button masks, the recorder's flag). What is
compared: the instance's and the braces' fields (pointers as labels), the globals and the event log.

With the port fixes on (unit coaster64), the port fix in DeleteTrackBrace reads the type's word 0xa4 as tag_sCoasterType's mFA4 and the
pylon owner's word 0x70 as sPylonOwner's mF70, and the one in sTrackBraceData puts the pylon record's words where sPylonRecord keeps them:
the host world writes them at those offsets (tpw_dltb_layout) and allocates the type and the owner at their host sizes.

No fused operations. Not tested: a list whose first remaining brace chain ends flagged 0x40 (the original reads a NULL brace there), a NULL
first brace read by the button tests.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DeleteLastTrackBrace"
MASK = 0xFFFFFFFF

FN_DEL, FN_LAST = 0x10056660, 0x10056A80
SIZES = {FN_DEL: 976, FN_LAST: 720}
OWN = set(SIZES)

ADDR = dict(edit=0x102B68B0, enabled=0x102C16D4, down=0x102C16D8, drag=0x102C178C, w1790=0x102C1790, input=0x102C1794,
            sets=0x102C79A4, recorder=0x1037EC44, ui=0x10396440)

# the layout lists, in the order of tpw_dltb_layout
NAMES = ["i_flags", "i_type", "i_blk", "i_count", "i_last", "i_braces", "i_b5c", "i_prev", "i_b64", "i_f118", "i_flash", "i_size",
         "b_num", "b_flags", "b_prev", "b_next", "b_f6c", "b_w70", "b_data", "b_size",
         "p_flags", "p_cx", "p_cz", "p_ctrl", "p_f30", "p_owner", "p_size",
         "c_f2c", "c_size", "s_flags", "s_heads", "s_size", "m_f28", "m_size", "r_rec", "t_fa4", "t_size", "o_f70", "o_size"]
MAC = dict(zip(NAMES, [0, 4, 0x3C, 0x44, 0x54, 0x58, 0x5C, 0x60, 0x64, 0x118, 0x144, 0x150,
                       0, 8, 0x64, 0x68, 0x6C, 0x70, 0x100, 0x120,
                       4, 0xC, 0x10, 0x18, 0x30, 0x34, 0x40,
                       0x2C, 0x100, 4, 8, 0x20, 0x28, 0x40, 0x24, 0xA4, 0xC0, 0x70, 0x90]))


def s32(v):
    return T.s32(v)


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def mk(name, spec):
    def f(e):
        L = e.cl
        args = []
        for i, ch in enumerate(spec):
            v = e.r(3 + i)
            args.append(L.lab(v) if ch == "p" else (v & MASK if ch == "u" else s32(v)))
        L.rec(name, tuple(args))
    return f


STUBS = CL.Stubs()
STUBS.real.update(OWN)
for addr, name, spec in [(0x10054640, "RemoveTrackBraceFromPylon", "pp"), (0x1006D430, "StopFlashingObject", "p"),
                         (0x10048FC4, "RemoveBraceFromCollisionSystem", "pp"), (0x1004094C, "DestroyTrackBrace", "pp"),
                         (0x10054814, "DeletePylon", "ppi"), (0x1005F53C, "Assign3DObjectToAIWorld", "puuui"),
                         (0x1003D844, "UpdatePylonStackStructures", "pp"), (0x1003D958, "UpdatePylonStackMeshes", "ppii"),
                         (0x10041A58, "SetPotentialPylonArea", ""), (0x10040F08, "FlagGenerateTrack", "pi"),
                         (0x10143F48, "RecordAction", "pi"), (0x10057278, "GetCoasterEditAvailableButtons", "u"),
                         (0x10057064, "SetCoasterButtonState", "uu"), (0x101F0F94, "ChangeCoasterButtonsEnabled", "pu"),
                         (0x101F0F2C, "ChangeCoasterButtonsDown", "pu")]:
    STUBS.internal(addr, mk(name, spec))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.pw))[0]

    def lab(self, p):
        return 0 if p == 0 else self.rev.get(p, -1)


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    pw = 4
    off = MAC
    psz = 4

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.saved = None

    def start(self, labels):
        self.rev = {a: l for l, a in labels.items()}
        self.L = self.e.cl = CL.PpcLog(dict(self.rev), self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def sets_at(self, k):
        return ADDR["sets"] + 4 * k

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, case, inst, brace):
        if case["op"] == "del":
            T.call_ppc(self.e, FN_DEL, [inst, brace])
        else:
            T.call_ppc(self.e, FN_LAST, [])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    pw = 8

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)
        self.off = dict(zip(NAMES, lib.lay))

    def start(self, labels):
        self.rev = {a: l for l, a in labels.items()}
        CL.host_begin(self.lib, dict(labels), self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_dltb_addr(name.encode())

    def sets_at(self, k):
        return self.g("sets") + 8 * k

    def alloc(self, n):
        return self.lib.tpw_dltb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, case, inst, brace):
        if case["op"] == "del":
            self.lib.tpw_dltb_delete(inst, brace)
        else:
            self.lib.tpw_dltb_deletelast()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_dltb_addr.restype, lib.tpw_dltb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_dltb_alloc.restype, lib.tpw_dltb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_dltb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_dltb_delete.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_dltb_deletelast.argtypes = []
    out = (C.c_int64 * 48)()
    n = lib.tpw_dltb_layout(out)
    assert n == len(NAMES), n
    lib.lay = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    O = s.off
    labels = {100: s.g("ui"), 101: s.g("recorder")}

    def mkinst(label, tflags, tf118):
        inst = s.alloc(max(O["i_size"], 0x200) + 16)
        typ = s.alloc(max(O["t_size"], 0xC0))
        s.words(typ, [tflags])
        s.words(typ + O["t_fa4"], [tf118])
        s.ptr(inst + O["i_type"], typ)
        labels[label] = inst
        return inst

    inst = mkinst(1, case["tflags"], case["tf118"])
    inst2 = mkinst(2, case["tflags2"], 0)
    s.words(inst + O["i_flags"], [case["iflags"]])
    s.words(inst + O["i_blk"], [case["blk"]])
    s.words(inst + O["i_count"], [case["count"]])
    s.words(inst + O["i_f118"], [case["f118"]])
    n = case["n"]
    braces = []
    for i in range(n):
        b = s.alloc(max(O["b_size"], 0x120) + 16)
        py = s.alloc(max(O["p_size"], 0x40) + 16)
        ctrl = s.alloc(max(O["c_size"], 0x100) + 16)
        owner = s.alloc(max(O["o_size"], 0x90))
        labels[10 + i], labels[30 + i], labels[50 + i], labels[70 + i] = b, py, ctrl, owner
        braces.append(b)
        bc = case["braces"][i]
        s.words(b + O["b_num"], [bc["num"]])
        s.words(b + O["b_flags"], [bc["flags"]])
        s.words(b + O["b_w70"], [bc["w70"]])
        s.words(b + O["b_f6c"], [0x3F800000])
        s.ptr(b + O["b_data"], py)
        s.words(py + O["p_flags"], [bc["pflags"]])
        s.words(py + O["p_cx"], [bc["cx"]])
        s.words(py + O["p_cz"], [bc["cz"]])
        s.words(py + O["p_f30"], [bc["f30"]])
        s.ptr(py + O["p_ctrl"], ctrl)
        s.ptr(py + O["p_owner"], owner)
        s.words(owner + O["o_f70"], [bc["owner"]])
        s.words(ctrl + O["c_f2c"], [bc["f2c"]])
    for i in range(n):
        if i > 0:
            s.ptr(braces[i] + O["b_prev"], braces[i - 1])
        if i < n - 1:
            s.ptr(braces[i] + O["b_next"], braces[i + 1])

    def at(idx):
        return braces[idx] if 0 <= idx < n else 0
    s.ptr(inst + O["i_braces"], at(case["head"]))
    s.ptr(inst + O["i_last"], at(case["last"]))
    s.ptr(inst + O["i_b5c"], at(case["b5c"]))
    s.ptr(inst + O["i_prev"], at(case["preview"]))
    s.ptr(inst + O["i_b64"], at(case["b64"]))
    s.ptr(inst + O["i_flash"], at(case["flash"]))
    # ride animation sets 1..3
    for k in range(1, 4):
        sc = case["sets"][k - 1]
        st = s.alloc(max(O["s_size"], 0x40) + 16)
        mesh = s.alloc(max(O["m_size"], 0x40) + 16)
        a = s.alloc(0x20)
        bb = s.alloc(0x40)
        labels[80 + k] = st
        s.words(st + O["s_flags"], [sc["flags"]])
        s.ptr(st + O["s_heads"], mesh)
        s.ptr(mesh + O["m_f28"], a)
        s.ptr(a + 4, bb)
        for j in range(3):
            c = sc["chain"][j]
            if c is not None:
                cb = s.alloc(0x50)
                s.words(cb + 0x34, [c])
                s.ptr(bb + j * 0x14 + 4, cb)
        s.ptr(s.sets_at(k), st)
    # globals
    edit = [inst, inst2, 0][case["edit"]]
    s.ptr(s.g("edit"), edit if case["op"] == "del" else inst)
    s.ptr(s.g("drag"), at(case["drag"]))
    s.words(s.g("enabled"), [case["enabled"]])
    s.words(s.g("down"), [case["down"]])
    s.words(s.g("w1790"), [case["w1790"]])
    s.words(s.g("input"), [case["input"]])
    s.words(s.g("recorder") + O["r_rec"], [case["recording"]])
    s.inst, s.inst2, s.braces = inst, inst2, braces
    s.start(labels)


def execute(s, case):
    brace = s.braces[case["k"]] if case["op"] == "del" else 0
    s.call(case, s.inst, brace)


def observe(s, case):
    O = s.off
    inst = s.inst
    out = {"log": s.log()}
    out["i_words"] = [s.rd_word(inst + O[k]) for k in ("i_flags", "i_blk", "i_count", "i_f118")]
    for k in ("i_last", "i_braces", "i_b5c", "i_prev", "i_b64", "i_flash"):
        out[k] = s.lab(s.rd_ptr(inst + O[k]))
    out["i2_flags"] = s.rd_word(s.inst2 + O["i_flags"])
    for i, b in enumerate(s.braces):
        out["b%d" % i] = [s.rd_word(b + O["b_num"]), canon_f(s.rd_word(b + O["b_f6c"])), s.rd_word(b + O["b_flags"]),
                          s.lab(s.rd_ptr(b + O["b_prev"])), s.lab(s.rd_ptr(b + O["b_next"]))]
    out["drag"] = s.lab(s.rd_ptr(s.g("drag")))
    out["glob"] = [s.rd_word(s.g(k)) for k in ("enabled", "down", "w1790", "input")]
    return out


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases


def rw(r):
    return r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32), r.getrandbits(32)])


def gen(r, op, n=None, blk=None, tf=None, df=None, k=None):
    n = r.randint(1, 6) if n is None else n
    c = {"op": op, "n": n, "queues": {}}
    c["blk"] = r.randrange(8) if blk is None else blk
    c["tflags"] = r.randrange(8) if tf is None else tf
    c["tflags2"] = r.randrange(8)
    c["tf118"] = r.choice([0, 0x3F800000, 0xC0490FDB, r.getrandbits(32) & 0x7F7FFFFF])
    c["iflags"] = r.choice([0, 0x2000, 0xFFFFFFFF, r.getrandbits(32)])
    c["count"] = r.choice([0, 1, n, r.randrange(10), 0xFFFFFFFF])
    c["f118"] = r.getrandbits(32) & 0x7F7FFFFF
    bs = []
    for i in range(n):
        f = 0
        for bit in (0x40, 0x80, 0x20, 0x10000, 0x200000):
            if r.random() < 0.4:
                f |= bit
        if i >= n - 2:
            f &= ~0x40
        bs.append({"num": r.randrange(20), "flags": f, "w70": r.randint(1, 3), "pflags": r.randrange(4), "cx": rw(r), "cz": rw(r),
                   "f30": r.choice([0, 1, 2]), "owner": rw(r), "f2c": r.randrange(3)})
    c["braces"] = bs
    c["k"] = r.randrange(n) if k is None else k
    if df is not None and n >= 3:
        bs[c["k"]]["flags"] = (bs[c["k"]]["flags"] & ~0xC0) | df
    c["head"] = 0
    c["last"] = r.choice([n - 1, n - 1, r.randrange(n), -1])
    c["b5c"] = r.choice([-1, c["k"], r.randrange(n), r.randrange(n)])
    c["preview"] = r.choice([-1, c["k"], r.randrange(n), r.randrange(n)])
    c["b64"] = r.choice([-1, r.randrange(n)])
    c["flash"] = r.choice([-1, -1, c["k"], r.randrange(n)])
    c["drag"] = r.choice([-1, c["k"], r.randrange(n)])
    c["edit"] = r.randrange(3)
    c["sets"] = [{"flags": r.choice([0, 0x4000000, 0xFFFFFFFF, r.getrandbits(32)]),
                  "chain": [r.choice([None, 0, 0x3F800000, 0x41200000, r.getrandbits(32) & 0x7F7FFFFF]) for _ in range(3)]}
                 for _ in range(3)]
    c["enabled"], c["down"] = rw(r), rw(r)
    c["w1790"], c["input"] = rw(r), rw(r)
    c["recording"] = r.choice([0, 0, 1, 5])
    if op == "last":
        # the original reads the first brace and the preview brace unchecked
        if c["preview"] < 0 and (c["tflags"] & 2) and (c["blk"] & 1) and (c["blk"] & 2):
            c["preview"] = c["k"]
        if df is not None and n >= 3:
            c["preview"] = c["k"]
        c["k"] = 0 if (c["blk"] & 2) == 0 else c["preview"] if c["preview"] >= 0 else 0
    return c


def edges(op):
    def g():
        r = random.Random(5337 if op == "del" else 7335)
        for blk in range(8):
            for tf in (0, 2):
                for n in (1, 3, 5):
                    for df in (None, 0x00, 0x40, 0xC0, 0x80):
                        c = gen(r, op, n=n, blk=blk, tf=tf, df=df, k=0)
                        if op == "last":
                            c["preview"] = 0
                            c["k"] = 0
                        yield c
    return g


def rnd(op):
    return lambda r: gen(r, op)


TESTS = [
    FnTest("DeleteTrackBrace", ppc, host, edge=edges("del"), rnd=rnd("del")),
    FnTest("DeleteLastTrackBrace", ppc, host, edge=edges("last"), rnd=rnd("last")),
]
