"""Unit v5084, the pylon edit states of the track editor, against libtpw_unit_DisableCurrentPylonEditState.dylib
(tests/unit_DisableCurrentPylonEditState.cpp), main app ("SimTheme Park"), src/game/rides/CoasterEdit.cpp:
  ExchangeTrackBraces 0x10053594, ToggleSwapUpPylonState 0x10053720, ToggleSwapDownPylonState 0x10053898,
  SetTrackPreview2ndBrace 0x10053a04, DisableCurrentPylonEditState 0x10053b58

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers, the host's offsets from tpw_cb_layout) for the
translation; the same code reads both back. The callees outside the unit (FlagGenerateTrack, FlashEndPylons, LockPointedAtRide,
CActionRecorder::RecordAction, SetupBraceAnimFrameData, UpdatePylonStackStructures, UpdatePylonStackMeshes and the Toggle Loft, Rotate,
LoopTurn and BankIncline states) are fakes on both sides that record their arguments and the words they read (the numbers of the
braces, the pylon's slots, the instance's flags) at the moment of the call. The functions of the unit call each other for real.

The world of a case: the editor's instance (or none), the input flags, a pylon of 1 to 4 braces with their numbers, flags, loft and ride
set numbers, three ride sets whose first node holds the float the exchange swaps, a chain of braces linked by their previous word (the
preview range marks walk it) with strictly rising numbers, the marks, the dragged brace and the words the disable clears. What is
compared: the event log, every global, the instance flags, every brace and pylon word the functions may write, the nodes' floats.

No fused operations. Not tested: a dragged brace that is NULL while the swap bit is set (the original reads through it), a preview walk
whose end is not reachable from its start, a second mark without a first (the original reads through the first).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DisableCurrentPylonEditState"
MASK = 0xFFFFFFFF

FN_EXCH, FN_UP, FN_DOWN, FN_PREV, FN_DIS = 0x10053594, 0x10053720, 0x10053898, 0x10053A04, 0x10053B58
SIZES = {FN_EXCH: 316, FN_UP: 328, FN_DOWN: 316, FN_PREV: 276, FN_DIS: 508}
OWN = set(SIZES)

ADDR = dict(edit=0x102B68B0, extrude=0x102B74A8, w16e0=0x102C16E0, mark2=0x102C1780, mark1=0x102C1784, w1788=0x102C1788,
            drag=0x102C178C, w1790=0x102C1790, iflags=0x102C1794, w16dc=0x102C16DC, rides=0x102C79A4)

# the Mac's offsets, in the order of tpw_cb_layout
KEYS = ["ei_type", "ei_blk", "ei_f40", "br_data", "br_prev", "br_w70", "br_loft", "br_flags", "pl_ctrl", "pl_count", "pl_br",
        "pl_flags", "rs_heads", "ms_header", "hd_f78"]
MAC = dict(zip(KEYS, [4, 0x3C, 0x40, 0x100, 0x64, 0x70, 0xE8, 8, 0x18, 0x30, 0x34, 4, 8, 4, 0x78]))
GLOBAL_WORDS = ["extrude", "w16e0", "w1788", "w1790", "iflags", "w16dc"]
GLOBAL_PTRS = ["edit", "mark1", "mark2", "drag"]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def W(e, p):
    return e.u32(p) if p else MASK


def view(e, pylon):
    """The pylon's flags word, count and the numbers of its first four braces."""
    count = e.u32(pylon + 0x30)
    slots = [W(e, e.u32(pylon + 0x34 + 4 * i)) if i < count else MASK for i in range(4)]
    return (e.u32(pylon + 4), count) + tuple(slots)


def stub_flag(e):
    e.cl.rec("Flag", (W(e, e.r(3)), s32(e.r(4))))


def stub_flash(e):
    e.cl.rec("Flash", (W(e, e.r(3)), e.r(4)))


def stub_lock(e):
    e.cl.rec("Lock", (s32(e.r(3)),))


def stub_record(e):
    e.cl.rec("Record", (s32(e.r(4)), e.r(5) & 0xFFFF))


def stub_setup(e):
    brace = e.r(4)
    e.cl.rec("Setup", (W(e, e.r(3)), W(e, brace), W(e, brace + 4), W(e, brace + 0xE8)))


def stub_structs(e):
    v = view(e, e.r(4))
    e.cl.rec("Structs", (W(e, e.r(3)),) + v)


def stub_meshes(e):
    v = view(e, e.r(4))
    e.cl.rec("Meshes", (W(e, e.r(3)),) + v + (s32(e.r(5)), s32(e.r(6))))


def plain(name):
    return lambda e: e.cl.rec(name, ())


STUBS.internal(0x10040F08, stub_flag)
STUBS.internal(0x10052158, stub_flash)
STUBS.internal(0x10067288, stub_lock)
STUBS.internal(0x101441FC, stub_record)
STUBS.internal(0x1003BB44, stub_setup)
STUBS.internal(0x1003D844, stub_structs)
STUBS.internal(0x1003D958, stub_meshes)
STUBS.internal(0x10052AC4, plain("Loft"))
STUBS.internal(0x100530A8, plain("Rotate"))
STUBS.internal(0x10052E5C, plain("LoopTurn"))
STUBS.internal(0x100532F4, plain("Bank"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"
    psz = 4
    o = MAC

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

    def ptr(self, a, v):
        self.words(a, [v])

    p32 = ptr

    def rd_ptr(self, a):
        return self.rd_word(a)

    def call(self, op, args):
        entry = {"exch": FN_EXCH, "up": FN_UP, "down": FN_DOWN, "prev": FN_PREV, "dis": FN_DIS}[op]
        T.call_ppc(self.e, entry, [a & MASK for a in args])


class Host(Side):
    fmt = "<I"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.o = dict(zip(KEYS, lib.layout))
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cb_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def p32(self, a, v):
        self.words(a, [self.lib.tpw_cb_off(v) if v else 0])

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def call(self, op, args):
        lib = self.lib
        if op == "exch":
            lib.tpw_cb_exchange(args[0], args[1])
        elif op == "up":
            lib.tpw_cb_up()
        elif op == "down":
            lib.tpw_cb_down()
        elif op == "prev":
            lib.tpw_cb_preview(args[0], s32(args[1]))
        else:
            lib.tpw_cb_disable()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for name, res, args in (("addr", C.c_uint64, [C.c_char_p]), ("alloc", C.c_uint64, [C.c_uint32]), ("off", C.c_uint64, [C.c_uint64]),
                            ("exchange", None, [C.c_uint64, C.c_uint64]), ("up", None, []), ("down", None, []),
                            ("preview", None, [C.c_uint64, C.c_int32]), ("disable", None, [])):
        f = getattr(lib, "tpw_cb_" + name)
        f.restype, f.argtypes = res, args
    lib.tpw_cb_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cb_layout(out)
    lib.layout = list(out)[:n]
    d = dict(zip(KEYS, lib.layout))
    # the words that sit at the same place on both machines
    assert d["br_flags"] == 8 and d["pl_flags"] == 4 and d["hd_f78"] == 0x78, lib.layout
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def mkbrace(s, name, b, pylon):
    o = s.o
    a = s.alloc(0x200)
    s.names[a] = name
    s.words(a, [b["number"], b["index"]])
    s.words(a + o["br_flags"], [b["flags"]])
    s.ptr(a + o["br_data"], pylon)
    s.words(a + o["br_w70"], [b["w70"]])
    s.words(a + o["br_loft"], [b["loft"]])
    return a


def build(s, c):
    o, P = s.o, s.psz
    s.names = {}
    s.braces = []        # (name, address)
    s.nodes = []
    for k in GLOBAL_WORDS:
        s.words(s.g(k), [c[k]])
    # the ride sets 1..3 and their nodes
    nodes = []
    for k in range(3):
        if k > 0 and c["share"][k] and nodes:
            node = nodes[0]
        else:
            node = s.alloc(0x60)
            s.words(node + 0x44, [c["nodef"][k]])
        nodes.append(node)
        header = s.alloc(0xB0)
        mesh = s.alloc(0x40)
        rset = s.alloc(0x80)
        s.p32(header + o["hd_f78"], node)
        s.ptr(mesh + o["ms_header"], header)
        s.ptr(rset + o["rs_heads"], mesh)
        s.ptr(s.g("rides") + (k + 1) * P, rset)
    s.nodes = nodes
    # the edit instance
    if c["edit"]:
        inst = s.alloc(0x200)
        s.names[inst] = "edit"
        typ = s.alloc(16)
        s.words(typ, [c["typew"]])
        s.words(inst, [c["eflags"]])
        s.ptr(inst + o["ei_type"], typ)
        s.words(inst + o["ei_blk"], [c["block3c"]])
        s.words(inst + o["ei_f40"], [c["ef40"]])
        s.ptr(s.g("edit"), inst)
        s.inst = inst
    else:
        s.inst = 0
    # the pylon and its braces
    pylon = s.alloc(0x100)
    s.names[pylon] = "pylon"
    control = s.alloc(16)
    s.words(control, [c["ctrlw"]])
    s.words(pylon + o["pl_flags"], [c["pflags"]])
    s.ptr(pylon + o["pl_ctrl"], control)
    n = len(c["braces"])
    s.words(pylon + o["pl_count"], [n])
    s.pylon = pylon
    pb = []
    for i, b in enumerate(c["braces"]):
        a = mkbrace(s, "b%d" % i, b, pylon)
        s.ptr(pylon + o["pl_br"] + i * P, a)
        pb.append(a)
        s.braces.append(("b%d" % i, a))
    s.pb = pb
    # the chain, linked by the previous word
    chain = []
    for j, b in enumerate(c["chain"]):
        chain.append(mkbrace(s, "c%d" % j, b, 0))
        s.braces.append(("c%d" % j, chain[-1]))
    for j in range(len(chain) - 1):
        s.ptr(chain[j] + o["br_prev"], chain[j + 1])
    s.chain = chain
    for key in ("mark1", "mark2"):
        s.ptr(s.g(key), chain[c[key]] if c[key] is not None else 0)
    s.ptr(s.g("drag"), pb[c["drag"]] if c["drag"] is not None else 0)
    if not c["edit"]:
        s.ptr(s.g("edit"), 0)


def args_of(s, c):
    if c["op"] == "exch":
        return [s.pb[c["a"]], s.pb[c["b"]]]
    if c["op"] == "prev":
        return [s.chain[c["pbrace"]] if c["pbrace"] is not None else 0, c["pflag"]]
    return []


def nm(s, v):
    return "null" if v == 0 else s.names.get(v, "?")


def observe(s, c):
    o, P = s.o, s.psz
    out = {"log": s.log()}
    for k in GLOBAL_WORDS:
        out[k] = s.rd_word(s.g(k))
    for k in GLOBAL_PTRS:
        out[k] = nm(s, s.rd_ptr(s.g(k)))
    if s.inst:
        out["inst"] = s.rd_word(s.inst)
    for name, a in s.braces:
        out[name] = s.rd_words(a, 2) + [s.rd_word(a + o["br_flags"]), s.rd_word(a + o["br_w70"]), s.rd_word(a + o["br_loft"]),
                                        nm(s, s.rd_ptr(a + o["br_prev"])), nm(s, s.rd_ptr(a + o["br_data"]))]
    out["slots"] = [nm(s, s.rd_ptr(s.pylon + o["pl_br"] + i * P)) for i in range(len(c["braces"]))]
    out["pylon"] = [s.rd_word(s.pylon + o["pl_flags"]), s.rd_word(s.pylon + o["pl_count"])]
    out["nodes"] = [s.rd_word(n + 0x44) for n in s.nodes]
    return out


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"], args_of(s, case))
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rw(r):
    return r.getrandbits(32)


def world(r, op):
    n = r.randint(1, 4)
    k = r.randint(1, 5)
    base = r.choice([r.randrange(0, 1000), r.randrange(0, 0xFFFFF000)])
    nums, v = [], base
    for _ in range(k):
        nums.append(v)
        v += r.randint(1, 50)
    c = {
        "op": op, "queues": {},
        "edit": r.random() < 0.9, "eflags": rw(r), "ef40": r.choice([0, 1, rw(r) | 1]), "block3c": r.choice([0, 1, 2, 3, rw(r)]),
        "typew": rw(r), "pflags": r.choice([0, 0, 1, 2, 3, rw(r)]), "ctrlw": rw(r),
        "braces": [dict(number=rw(r), index=i, flags=r.choice([0, 0x40, 0x80, 0xC0, rw(r)]), w70=r.randint(1, 3),
                        loft=fb(r.uniform(-50, 50))) for i in range(n)],
        "chain": [dict(number=nums[j], index=0, flags=r.choice([0, rw(r), 0xFEFFBFFF, 0x01004000]), w70=0, loft=0) for j in range(k)],
        "nodef": [fb(r.uniform(-9, 9)) for _ in range(3)], "share": [False] + [r.random() < 0.3 for _ in range(2)],
        "mark1": r.choice([None] + list(range(k))), "mark2": r.choice([None] + list(range(k))),
        "drag": r.choice([None] + list(range(n))),
        "iflags": rw(r), "extrude": rw(r), "w16e0": rw(r), "w1788": rw(r), "w1790": rw(r), "w16dc": rw(r),
        "a": r.randrange(n), "b": r.randrange(n), "pbrace": r.choice([None] + list(range(k))), "pflag": r.choice([0, 1, -1, 7]),
    }
    return c


def fix(c):
    """Keep the cases to what the original can take (see the docstring)."""
    k = len(c["chain"])
    if c["mark2"] is not None and c["mark1"] is None:
        c["mark1"] = c["mark2"] if c["mark2"] % 2 else 0
    if c["op"] == "prev" and c["pbrace"] is not None and c["mark1"] is None:
        c["mark1"] = k - 1
    low, fl = c["iflags"] & 0x3F, c["iflags"]
    if c["op"] == "dis" and low in (8, 0x10) and c["drag"] is None:
        c["drag"] = 0
    if c["op"] == "up" and fl & 8 and c["drag"] is None:
        c["drag"] = 0
    if c["op"] == "down" and fl & 0x10 and c["drag"] is None:
        c["drag"] = 0
    if c["op"] == "exch":
        c["edit"] = True
    return c


def case_of(r, op, **kw):
    c = world(r, op)
    c.update(kw)
    return fix(c)


def low_flags(r, low, hi=None):
    return low | ((rw(r) & ~0x3F) if hi is None else hi)


def exch_edges():
    r = random.Random(5084)
    for a in range(4):
        for b in range(4):
            c = case_of(r, "exch", a=a, b=b)
            n = len(c["braces"])
            c["a"], c["b"] = a % n, b % n
            yield c


def exch_random(r):
    return case_of(r, "exch")


def swap_edges(op, bit):
    r = random.Random(5085 + bit)
    for low in (0, bit, 1, 4, 0x3F, 0x20):
        for drag in (None, 0, 1, 2, 3):
            for pflags in (0, 2):
                for edit in (True, False):
                    for ef40 in (0, 1):
                        c = case_of(r, op, iflags=low_flags(r, low), drag=drag, pflags=pflags, edit=edit, ef40=ef40)
                        if drag is not None and drag >= len(c["braces"]):
                            c["drag"] = len(c["braces"]) - 1
                        yield fix(c)


def swap_random(op, bit):
    def gen(r):
        low = r.choice([0, 0, 0, bit, r.randrange(64)])
        c = case_of(r, op, iflags=low_flags(r, low), pflags=r.choice([0, 0, 2]), ef40=r.choice([1, 1, 0]),
                    drag=r.choice([None, 0, 1, 2, 3]))
        if c["drag"] is not None:
            c["drag"] = min(c["drag"], len(c["braces"]) - 1)
        return fix(c)
    return gen


def prev_edges():
    r = random.Random(5086)
    for flag in (0, 1, -1):
        for pbrace in (None, 0, 1, 2):
            for m1 in (None, 0, 1, 2):
                for m2 in (None, 0, 1, 2):
                    c = case_of(r, "prev", pflag=flag, pbrace=pbrace, mark1=m1, mark2=m2)
                    k = len(c["chain"])
                    for key in ("pbrace", "mark1", "mark2"):
                        if c[key] is not None:
                            c[key] = min(c[key], k - 1)
                    yield fix(c)


def prev_random(r):
    return case_of(r, "prev")


def dis_edges():
    r = random.Random(5087)
    for low in range(0x40):
        for hi in (0, 0x80, 0x40, 0xC0):
            for w16 in (0, 1):
                c = case_of(r, "dis", iflags=low_flags(r, low, hi | (rw(r) & ~0xFF)), w16e0=(rw(r) & ~1) | w16)
                if low == 4:
                    c["pflags"], c["ef40"], c["edit"] = r.choice([0, 2]), 1, True
                    c["drag"] = r.choice([None, 0, 1, 2, 3])
                    c["drag"] = None if c["drag"] is None else min(c["drag"], len(c["braces"]) - 1)
                    if c["drag"] is not None:
                        c["braces"][c["drag"]]["flags"] = r.choice([0, 0x40, 0xFFFFFFBF, 0x80])
                if low == 0x20:
                    c["mark2"] = r.choice([None, 0, 1, 2, 3])
                    c["mark2"] = None if c["mark2"] is None else min(c["mark2"], len(c["chain"]) - 1)
                yield fix(c)


def dis_random(r):
    low = r.choice([0, 1, 2, 4, 8, 0x10, 0x20, r.randrange(64)])
    c = case_of(r, "dis", iflags=low_flags(r, low, rw(r) & ~0x3F), pflags=r.choice([0, 0, 2]), ef40=r.choice([1, 1, 0]))
    if low == 8 or low == 0x10:
        c["drag"] = r.choice([None, 0, 1, 2, 3])
        c["drag"] = None if c["drag"] is None else min(c["drag"], len(c["braces"]) - 1)
    return fix(c)


TESTS = [
    FnTest("ExchangeTrackBraces", ppc, host, edge=exch_edges, rnd=exch_random),
    FnTest("ToggleSwapUpPylonState", ppc, host, edge=lambda: swap_edges("up", 8), rnd=swap_random("up", 8)),
    FnTest("ToggleSwapDownPylonState", ppc, host, edge=lambda: swap_edges("down", 0x10), rnd=swap_random("down", 0x10)),
    FnTest("SetTrackPreview2ndBrace", ppc, host, edge=prev_edges, rnd=prev_random),
    FnTest("DisableCurrentPylonEditState", ppc, host, edge=dis_edges, rnd=dis_random),
]
