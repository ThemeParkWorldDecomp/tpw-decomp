"""Unit v5261, the editor coaster's layout commands, against libtpw_unit_LayoutSetMovePylon.dylib (tests/unit_LayoutSetMovePylon.cpp), main app,
src/TPConfig_LayoutSetMovePylon.cpp:
  LayoutStackPylonUp 0x10058c08, LayoutStackPylonDown 0x10058c80, LayoutDeleteMultiPylons 0x10058cf8, LayoutSetMovePylon 0x10058da4,
  LayoutRegenerateTrack 0x10058e48 and 0x10058ec0, LayoutSetCoasterEditOld 0x10058fa0, LayoutSetCoasterEdit 0x1005901c

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian for the original, host memory for
the translation). The callees outside the unit (DisableCurrentPylonEditState, GetTrackBrace, StartMovePylon, DoDeleteMultiPylons, GenerateTrack,
ExchangeTrackBraces, FindCoasterInstanceFromEntranceID, SetCoasterEdit) are fakes on both sides recording their arguments (objects as an index
of the case's registry: 0 the edit instance, 1..3 braces, 4 and 5 other instances, 6 the edit instance's +0x3c block, -2 NULL) and answering
from the case's queues.

The world: the edit instance (flags), braces with an index word at +4 and a pylon record (+0x100 on the Mac) whose pointer table starts at +0x30,
the object table, random starting values of the globals the functions write. Compared: the log, gCoasterDragBrace, gCoasterWord16E0,
gCoasterCurrent, gCoasterCurrentData and gCoasterEditInstance.

No fused operations. Not tested: nothing skipped (ids stay inside the 0x100 object table).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LayoutSetMovePylon"
MASK = 0xFFFFFFFF

F_UP, F_DOWN, F_DEL, F_MOVE = 0x10058C08, 0x10058C80, 0x10058CF8, 0x10058DA4
F_REGEN, F_REGEN_ID, F_EDIT_OLD, F_EDIT = 0x10058E48, 0x10058EC0, 0x10058FA0, 0x1005901C
SIZES = {F_UP: 76, F_DOWN: 76, F_DEL: 120, F_MOVE: 116, F_REGEN: 76, F_REGEN_ID: 92, F_EDIT_OLD: 76, F_EDIT: 88}
OWN = set(SIZES)

C_DISABLE, C_GETBRACE, C_START, C_DODEL = 0x10053B58, 0x1004DE40, 0x10057738, 0x10053F98
C_GEN, C_EXCH, C_FIND, C_SETEDIT = 0x10050FCC, 0x10053594, 0x10043380, 0x100585F4

ADDR = dict(edit=0x102B68B0, drag=0x102C178C, w16e0=0x102C16E0, current=0x102B68A8, currentdata=0x102B68A4, table=0x102B6F04)
# the host's offsets: brace +0x100 pointer, brace size, instance size, flags offset, pointer size
HOST_LAYOUT = [0x108, 0x1f0, 0x1f0, 0, 8]
PPC_LAYOUT = dict(data=0x100, brace=0x110, inst=0x200, psz=4)

CUR = None


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s_disable(e):
    e.cl.rec("DisableCurrentPylonEditState", ())


def s_getbrace(e):
    e.cl.rec("GetTrackBrace", (CUR.idx(e.r(3)), e.r(4) & MASK))
    e.set_r(3, CUR.obj(e.cl.ret("GetTrackBrace")))


def s_start(e):
    e.cl.rec("StartMovePylon", (CUR.idx(e.r(3)),))
    e.set_r(3, e.cl.ret("StartMovePylon") & MASK)


def s_dodel(e):
    e.cl.rec("DoDeleteMultiPylons", (CUR.idx(CUR.rd_ptr(ADDR["drag"])),))


def s_gen(e):
    e.cl.rec("GenerateTrack", (CUR.idx(e.r(3)),))


def s_exch(e):
    e.cl.rec("ExchangeTrackBraces", (CUR.idx(e.r(3)), CUR.idx(e.r(4))))


def s_find(e):
    e.cl.rec("FindCoasterInstanceFromEntranceID", (e.r(3) & MASK,))
    e.set_r(3, CUR.obj(e.cl.ret("FindCoasterInstanceFromEntranceID")))


def s_setedit(e):
    e.cl.rec("SetCoasterEdit", (CUR.idx(e.r(3)), T.s32(e.r(4))))


for _a, _f in ((C_DISABLE, s_disable), (C_GETBRACE, s_getbrace), (C_START, s_start), (C_DODEL, s_dodel), (C_GEN, s_gen),
               (C_EXCH, s_exch), (C_FIND, s_find), (C_SETEDIT, s_setedit)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def idx(self, p):
        if p == 0:
            return -2
        return self.objs.index(p) if p in self.objs else -1

    def obj(self, i):
        return self.objs[i] if 0 <= i < len(self.objs) else 0


class Ppc(Side):
    fmt = ">I"
    L = PPC_LAYOUT

    def __init__(self, e, case):
        global CUR
        self.e = e
        self.L_ = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        CUR = self

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
        self.write(a, struct.pack(">I", v & MASK))

    def rd_ptr(self, a):
        return struct.unpack(">I", self.read(a, 4))[0]

    def register(self):
        self.objs = self.objs0 + [self.objs0[0] + 0x3C]

    def log(self):
        return self.L_.log

    def call(self, entry, args):
        T.call_ppc(self.e, entry, [a & MASK for a in args])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        global CUR
        self.lib = lib
        bind(lib)
        self.L = dict(data=HOST_LAYOUT[0], brace=HOST_LAYOUT[1], inst=HOST_LAYOUT[2], psz=8)
        CL.host_begin(lib, {}, case["queues"])
        CUR = self

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lmp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_lmp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def register(self):
        self.objs = self.objs0 + [self.lib.tpw_lmp_block(self.objs0[0])]
        for i, o in enumerate(self.objs0):
            self.lib.tpw_lmp_reg(i, o)
        self.lib.tpw_lmp_reg(6, self.objs[6])

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        name = {F_UP: "up", F_DOWN: "down", F_DEL: "delete", F_MOVE: "move", F_REGEN: "regen", F_REGEN_ID: "regen_id",
                F_EDIT_OLD: "edit_old", F_EDIT: "edit"}[entry]
        getattr(self.lib, "tpw_lmp_" + name)(*[a & MASK for a in args])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lmp_addr.restype, lib.tpw_lmp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lmp_alloc.restype, lib.tpw_lmp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lmp_block.restype, lib.tpw_lmp_block.argtypes = C.c_uint64, [C.c_uint64]
    lib.tpw_lmp_reg.restype, lib.tpw_lmp_reg.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_lmp_layout.argtypes = [C.POINTER(C.c_int64)]
    for n, k in (("up", 1), ("down", 1), ("delete", 2), ("move", 3), ("regen", 0), ("regen_id", 1), ("edit_old", 1), ("edit", 1)):
        f = getattr(lib, "tpw_lmp_" + n)
        f.restype, f.argtypes = None, [C.c_uint32] * k
    out = (C.c_int64 * 32)()
    n = lib.tpw_lmp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, [hex(x) for x in list(out)[:n]]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    L = s.L
    edit = s.alloc(L["inst"])
    braces = [s.alloc(L["brace"]) for _ in range(3)]
    datas = [s.alloc(0x100) for _ in range(3)]
    others = [s.alloc(L["inst"]) for _ in range(2)]
    s.objs0 = [edit] + braces + others
    s.register()
    s.words(edit, [case["eflags"]])
    s.ptr(s.g("edit"), edit)
    for i, b in enumerate(braces):
        s.words(b + 4, [case["bidx"][i]])
        s.ptr(b + L["data"], datas[i])
        for k, t in enumerate(case["tab"][i]):
            s.ptr(datas[i] + 0x30 + k * L["psz"], s.obj(t) if t >= 0 else 0)
    for k, t in enumerate(case["otab"]):
        s.ptr(s.g("table") + k * L["psz"], s.obj(t) if t >= 0 else 0)
    s.ptr(s.g("drag"), s.obj(case["drag0"]) if case["drag0"] >= 0 else 0)
    s.ptr(s.g("current"), s.obj(case["cur0"]) if case["cur0"] >= 0 else 0)
    s.ptr(s.g("currentdata"), s.obj(case["cd0"]) if case["cd0"] >= 0 else 0)
    s.words(s.g("w16e0"), [case["w0"]])


def observe(s, case):
    return {"log": s.log(), "drag": s.idx(s.rd_ptr(s.g("drag"))), "w16e0": s.rd_word(s.g("w16e0")),
            "current": s.idx(s.rd_ptr(s.g("current"))), "currentdata": s.idx(s.rd_ptr(s.g("currentdata"))),
            "edit": s.idx(s.rd_ptr(s.g("edit")))}


def run(s, case):
    try:
        build(s, case)
        s.call(case["entry"], case["args"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def base(r, entry, args, **over):
    c = {"entry": entry, "args": args, "eflags": r.choice([0, 1, 2, 3, 4, 5, 0xFFFFFFFE, 0xFFFFFFFF, r.getrandbits(32)]),
         "bidx": [r.randrange(0, 9) for _ in range(3)],
         "tab": [[r.choice([-1, 0, 1, 2, 3, 4, 5]) for _ in range(16)] for _ in range(3)],
         "otab": [r.choice([-1, 0, 1, 2, 3, 4, 5]) for _ in range(16)],
         "drag0": r.choice([-1, 0, 1, 2, 3]), "cur0": r.choice([-1, 0, 1, 4]), "cd0": r.choice([-1, 0, 5, 6]),
         "w0": r.choice([0, 1, 2, 3, r.getrandbits(32)])}
    nulls = [] if entry in (F_UP, F_DOWN) else [99]      # Up and Down read through the brace
    c["queues"] = {"GetTrackBrace": [r.choice([1, 2, 3] + nulls), r.choice([1, 2, 3] + nulls)], "StartMovePylon": [r.choice([0, 1])],
                   "FindCoasterInstanceFromEntranceID": [r.choice([0, 1, 4, 5, 99])]}
    c.update(over)
    return c


def u16ish(r):
    return r.choice([0, 1, 0xFFFF, 0x10000, 0x1FFFF, 0xFFFF0000, 0x8000, r.getrandbits(32), r.getrandbits(16)])


def mk(entry):
    def rnd(r):
        if entry == F_DEL:
            args = [u16ish(r), u16ish(r)]
        elif entry == F_MOVE:
            args = [u16ish(r), u16ish(r), u16ish(r)]
        elif entry in (F_UP, F_DOWN):
            args = [u16ish(r)]
        elif entry == F_REGEN:
            args = []
        elif entry in (F_REGEN_ID, F_EDIT):
            args = [r.choice([0, 1, 2, 3, 4, 5, 15, r.randrange(0, 16)])]
        else:
            args = [r.choice([0, 1, 0xFFFFFFFF, 0x80000000, r.getrandbits(32)])]
        return base(r, entry, args)

    def edges():
        r = random.Random(entry)
        for i in range(8):
            yield rnd(r)
        if entry in (F_REGEN, F_REGEN_ID):
            for fl in (0, 1, 2, 3, 0xFFFFFFFE, 0xFFFFFFFF):
                a = [1] if entry == F_REGEN_ID else []
                yield base(r, entry, a, eflags=fl, otab=[-1, 0, 4, 5] * 4)
        if entry == F_REGEN_ID:
            yield base(r, entry, [0], otab=[-1] * 16)
            yield base(r, entry, [3], otab=[4] * 16, eflags=1)
        if entry in (F_EDIT, F_EDIT_OLD):
            yield base(r, entry, [0])
            yield base(r, entry, [1])

    return edges, rnd


def test(name, entry):
    edges, rnd = mk(entry)
    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


TESTS = [
    test("LayoutStackPylonUp", F_UP),
    test("LayoutStackPylonDown", F_DOWN),
    test("LayoutDeleteMultiPylons", F_DEL),
    test("LayoutSetMovePylon", F_MOVE),
    test("LayoutRegenerateTrack", F_REGEN),
    test("LayoutRegenerateTrack__FUl", F_REGEN_ID),
    test("LayoutSetCoasterEditOld", F_EDIT_OLD),
    test("LayoutSetCoasterEdit", F_EDIT),
]
