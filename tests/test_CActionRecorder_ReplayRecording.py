"""Unit v5224: CActionRecorder::ReplayRecording, EndReplay and three RecordAction overloads, against
libtpw_unit_CActionRecorder_ReplayRecording.dylib (tests/unit_CActionRecorder_ReplayRecording.cpp), main app ("SimTheme Park"),
src/ActionRecorder_CActionRecorder_CActionRecorder_CActionRecorder_ReplayRecording.cpp:
  ReplayRecording 0x10144e2c, EndReplay 0x10144ce4, RecordAction(float, ushort) 0x10144704,
  RecordAction(float, float, ushort) 0x10144868, RecordAction(ushort, uchar, wchar_t*) 0x10144a00

A case is a recorder object and a call. The object is built twice by ONE piece of Python code: in emulator memory at the Mac's
offsets (big-endian) for the original, and in the host library's memory for the translation. The three files of the recorder are
fakes on both sides (virtual Write and Seek record an event: the object, the sizes and an FNV hash of the bytes written); the callees
outside the unit (GetBankAccount, GetNext*FileEntry, DoTick, RSSE_Tick, LayoutStartAllCoasterSounds, CTRACE, SwearFilter::Filter,
the master save constructor) record the same events on both sides. The string constructor and destructor and _USwapBlock16 are
not logged: the PowerPC stubs do what the real ones do and the bytes written show the result.

What is compared: the event log, the recorder's state, mode, mF08, recording and replay-type words, the bank account's flag and
whether gMasterSave was made. No fused operations. Not tested: a NULL text (the original builds a string from it unchecked), the
size passed to operator new (the host uses new).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CActionRecorder_ReplayRecording"
MASK = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF

FN_REPLAY, FN_END, FN_RA_F, FN_RA_FF, FN_RA_W = 0x10144E2C, 0x10144CE4, 0x10144704, 0x10144868, 0x10144A00
OWN = {FN_REPLAY, FN_END, FN_RA_F, FN_RA_FF, FN_RA_W}

PTR_GLUE = 0x1023BAA8
GETBANK = 0x1013EDFC
NEXT_TIMING = 0x10145318
NEXT_CAMERA = 0x10145360
DOTICK = 0x10144F80
RSSE_TICK = 0x100D33DC
LAYOUT_SOUNDS = 0x10046BC0
CTRACE = 0x1006FCBC
STR_CTOR = 0x10146A14
STR_DTOR = 0x101468F8
FILTER = 0x1020D2CC
SWAP16 = 0x102398B0
MASTER_CTOR = 0x101694DC

ADDR = dict(gGameWorld=0x1037EBFC, gMasterSave=0x10380A8C)
MAC_OFF = dict(state=0, mode=4, f08=8, act=0xC, tim=0x14, cam=0x1C, rec=0x24, rtype=0x28, bank=0x114)
G = {}


def fnv(bs):
    h = 0xCBF29CE484222325
    for b in bs:
        h ^= b
        h = (h * 0x100000001B3) & M64
    return [h & MASK, h >> 32]


def units_bytes(units):
    return b"".join(struct.pack("<H", u & 0xFFFF) for u in units)


def s32(v):
    return T.s32(v)


def canon64(bits):
    if (bits >> 52) & 0x7FF == 0x7FF and bits & ((1 << 52) - 1):
        return 0x7FF8000000000000
    return bits


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def save_toc(e):
    """The glue stores the caller's TOC pointer at 0x14(r1); the code reloads r2 from there after the call."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def stub_ptr_glue(e):
    L = e.cl
    save_toc(e)
    tv = e.r(12)
    obj = e.r(3)
    if tv == G["tv_write"]:
        n = e.r(5)
        data = e.read(e.r(4), n) if n else b""
        L.rec("Write", tuple([L.lab(obj), n] + fnv(data)))
    elif tv == G["tv_seek"]:
        L.rec("Seek", (L.lab(obj), e.r(4), e.r(5)))
    else:
        raise AssertionError("unknown virtual %x" % tv)
    e.set_r(3, 0)


def stub_getbank(e):
    e.cl.rec("GetBankAccount", (e.cl.lab(e.r(3)),))
    e.set_r(3, G["bank"])


def read_units(e, p):
    out = []
    while True:
        u = struct.unpack(">H", e.read(p, 2))[0]
        if u == 0:
            return out
        out.append(u)
        p += 2


def stub_str_ctor(e):
    obj, text = e.r(3), e.r(4)
    units = read_units(e, text)
    data = e.alloc(2 * len(units) + 2)
    e.write(data, b"".join(struct.pack(">H", u) for u in units) + b"\0\0")
    rep = e.alloc(0x20)                      # the string object is one pointer to a rep: length at +0, data at +0xc
    e.write(rep, struct.pack(">I", len(units)))
    e.write(rep + 0xC, struct.pack(">I", data))
    e.write(obj, struct.pack(">I", rep))
    e.set_r(3, obj)


def stub_str_dtor(e):
    pass


def stub_filter(e):
    L = e.cl
    this, sobj, flag = e.r(3), struct.unpack(">I", e.read(e.r(4), 4))[0], e.r(5)
    n = struct.unpack(">I", e.read(sobj, 4))[0]
    data = struct.unpack(">I", e.read(sobj + 0xC, 4))[0]
    cut = s32(L.ret("FilterCut"))
    if 0 <= cut < n:
        e.write(sobj, struct.pack(">I", cut))
        e.write(data + 2 * cut, b"\0\0")
        n = cut
    units = [struct.unpack(">H", e.read(data + 2 * i, 2))[0] for i in range(n)]
    L.rec("Filter", tuple([L.lab(this), n] + fnv(units_bytes(units)) + [flag & 0xFF]))
    e.set_r(3, 1)


def stub_swap16(e):
    save_toc(e)
    p, n = e.r(3), s32(e.r(4))
    if n > 0:
        raw = e.read(p, 2 * n)
        e.write(p, b"".join(raw[i + 1:i + 2] + raw[i:i + 1] for i in range(0, 2 * n, 2)))


def stub_ctrace(e):
    L = e.cl
    fmt = e.cstr(e.r(4))
    fb = fmt.encode("latin-1") if isinstance(fmt, str) else bytes(fmt)
    vals = [e.r(3) & MASK] + fnv(fb)
    gi = 5
    i = 0
    while i < len(fb):
        if fb[i] == ord("%"):
            i += 1
            c = chr(fb[i])
            if c == "d":
                vals.append(e.r(gi) & MASK)
                gi += 1
            elif c == "g":
                bits = canon64(((e.r(gi) & MASK) << 32) | (e.r(gi + 1) & MASK))
                vals += [bits & MASK, bits >> 32]
                gi += 2
            elif c == "l":
                i += 1
                units = read_units(e, e.r(gi))
                gi += 1
                vals += [len(units)] + fnv(units_bytes(units))
        i += 1
    L.rec("CTRACE", tuple(vals))


STUBS.internal(PTR_GLUE, stub_ptr_glue)
STUBS.internal(GETBANK, stub_getbank)
STUBS.internal(NEXT_TIMING, CL.recorder("GetNextTimingFileEntry", "L"))
STUBS.internal(NEXT_CAMERA, CL.recorder("GetNextCameraFileEntry", "L"))
STUBS.internal(DOTICK, CL.recorder("DoTick", "L"))
STUBS.internal(RSSE_TICK, CL.recorder("RSSE_Tick", ""))
STUBS.internal(LAYOUT_SOUNDS, CL.recorder("LayoutStartAllCoasterSounds", ""))
STUBS.internal(MASTER_CTOR, CL.recorder("MasterSaveCtor", "N"))
STUBS.internal(STR_CTOR, stub_str_ctor)
STUBS.internal(STR_DTOR, stub_str_dtor)
STUBS.internal(FILTER, stub_filter)
STUBS.internal(SWAP16, stub_swap16)
STUBS.internal(CTRACE, stub_ctrace)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    vt = T.persist_alloc(e, 0x40)
    G["tv_write"] = T.persist_alloc(e, 8)
    G["tv_seek"] = T.persist_alloc(e, 8)
    e.write(vt + 0x14, struct.pack(">I", G["tv_write"]))
    e.write(vt + 0x18, struct.pack(">I", G["tv_seek"]))
    G["vt"] = vt


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def wstr(self, units):
        a = self.alloc(2 * len(units) + 2)
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", u) for u in units))
        return a

    def mlabel(self):
        v = self.getptr(self.g("gMasterSave"))
        if v == 0:
            return 0
        return 7 if v == self.master else 100


class Ppc(Side):
    fmt = ">I"
    off = MAC_OFF

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(0x400)
        self.world = e.alloc(0x40)
        self.bank = e.alloc(0x200)
        self.master = e.alloc(0x80)
        G["bank"] = self.bank
        o = self.obj
        self.L = e.cl = CL.PpcLog({o: 1, o + 0xC: 21, o + 0x14: 22, o + 0x1C: 23, self.bank: 30, self.world: 40, self.master: 7},
                                  case["queues"])
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

    def setptr(self, a, v):
        self.words(a, [v])

    def getptr(self, a):
        return self.rd_word(a)

    def log(self):
        return self.L.log

    def init_files(self):
        for k in ("act", "tim", "cam"):
            self.words(self.obj + self.off[k], [G["vt"], 0])

    def call(self, fn, args):
        T.call_ppc(self.e, fn, args)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = lib.off
        self.obj = lib.tpw_rec_alloc(lib.size_rec + 64)
        self.world = lib.tpw_rec_alloc(0x40)
        self.bank = lib.tpw_rec_alloc(lib.size_bank + 64)
        self.master = lib.tpw_rec_alloc(0x80)
        lib.tpw_rec_setbank(self.bank)
        o = self.obj
        CL.host_begin(lib, {1: o, 21: o + self.off["act"], 22: o + self.off["tim"], 23: o + self.off["cam"], 30: self.bank,
                            40: self.world, 7: self.master}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rec_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_rec_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def getptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def init_files(self):
        pass

    def call(self, fn, args):
        getattr(self.lib, "tpw_rec_" + fn)(*args)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_rec_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "setbank").argtypes = [C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rec_replay.argtypes = [C.c_uint64, C.c_int32]
    lib.tpw_rec_end.argtypes = [C.c_uint64]
    lib.tpw_rec_ra_f.argtypes = [C.c_uint64, C.c_int32, C.c_float, C.c_uint32]
    lib.tpw_rec_ra_ff.argtypes = [C.c_uint64, C.c_int32, C.c_float, C.c_float, C.c_uint32]
    lib.tpw_rec_ra_w.argtypes = [C.c_uint64, C.c_int32, C.c_uint32, C.c_uint32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    v = list(out)[:n]
    lib.off = dict(state=v[0], mode=v[1], f08=v[2], act=v[3], tim=v[4], cam=v[5], rec=v[6], rtype=v[7], bank=v[8])
    lib.size_rec, lib.size_bank = v[9], v[10]
    assert v[0:3] == [0, 4, 8], v
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o, off = s.obj, s.off
    s.words(o + off["state"], [case["state"]])
    s.words(o + off["mode"], [case["mode"]])
    s.words(o + off["f08"], [case["f08"]])
    s.words(o + off["rec"], [case["rec"]])
    s.words(o + off["rtype"], [case["rtype"]])
    s.words(s.bank + off["bank"], [case["bankflag"]])
    s.setptr(s.g("gGameWorld"), s.world)
    s.setptr(s.g("gMasterSave"), s.master if case["preset"] else 0)
    s.init_files()


def execute(s, case):
    op = case["op"]
    if op == "replay":
        s.call(FN_REPLAY if isinstance(s, Ppc) else "replay", [s.obj, case["arg"]] if isinstance(s, Ppc) else [s.obj, s32(case["arg"])])
    elif op == "end":
        s.call(FN_END if isinstance(s, Ppc) else "end", [s.obj])
    elif op == "ra_f":
        a, v, i = case["args"]
        if isinstance(s, Ppc):
            s.call(FN_RA_F, [s.obj, a & MASK, T.F(v), i])
        else:
            s.call("ra_f", [s.obj, s32(a), v, i])
    elif op == "ra_ff":
        a, v, w, i = case["args"]
        if isinstance(s, Ppc):
            s.call(FN_RA_FF, [s.obj, a & MASK, T.F(v), T.F(w), i])
        else:
            s.call("ra_ff", [s.obj, s32(a), v, w, i])
    else:
        a, x, b, units = case["args"]
        t = s.wstr(units)
        if isinstance(s, Ppc):
            s.call(FN_RA_W, [s.obj, a & MASK, x, b, t])
        else:
            s.call("ra_w", [s.obj, s32(a), x, b, t])


def observe(s, case):
    o, off = s.obj, s.off
    return {"log": s.log(), "state": s.rd_word(o + off["state"]), "mode": s.rd_word(o + off["mode"]),
            "f08": s.rd_word(o + off["f08"]), "rec": s.rd_word(o + off["rec"]), "rtype": s.rd_word(o + off["rtype"]),
            "bank": s.rd_word(s.bank + off["bank"]), "master": s.mlabel()}


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


# ---------------------------------------------------------------------------------------------------------------- case generators

FLOATS = [0.0, -0.0, 1.0, -1.0, 0.5, 123.456, -7.25, 1e-40, 3.0e38, float("inf"), float("-inf"), float("nan")]


def rf(r):
    k = r.random()
    if k < 0.15:
        return r.choice(FLOATS)
    return struct.unpack(">f", struct.pack(">f", r.uniform(-300, 300)))[0]


def base(r, op):
    return {"op": op, "state": r.choice([0, 1, 1, 5, -1]), "mode": r.choice([0, 0, 1, 7]), "f08": r.randrange(1 << 32),
            "rec": r.choice([0, 0, 0, 1, 2]), "rtype": r.randrange(1 << 32), "bankflag": r.randrange(1 << 32),
            "preset": r.random() < 0.5, "queues": {"FilterCut": [r.choice([100, 100, 100, 0, 1, 3, MASK])]}}


def pick_action(r):
    return r.choice([0, 1, 0x43, 0xE7, 255, 256, 0x1234, -1, r.randrange(256)]) & MASK


def pick_id(r):
    return r.choice([0, 1, 0x80, 0x1234, 0xFFFF, r.randrange(0x10000)])


def text(r):
    n = r.choice([0, 1, 2, 3, 5, 12, r.randrange(0, 30)])
    return [r.choice([0x41, 0x7A, 0x20, 0x100, 0xFF00, 0xABCD, r.randrange(1, 0x10000)]) for _ in range(n)]


def replay_case(r, rec=None, mode=None, rtype=None):
    c = base(r, "replay")
    c["arg"] = r.choice([0, 1, 2, 3, -1]) & MASK if rtype is None else rtype
    if rec is not None:
        c["rec"] = rec
    if mode is not None:
        c["mode"] = mode
    return c


def replay_edges():
    r = random.Random(11)
    for rec in (0, 1, 5):
        for mode in (0, 1):
            for t in (0, 1, 2, 3, MASK):
                yield replay_case(r, rec, mode, t)


def end_case(r, rec=None, mode=None):
    c = base(r, "end")
    if rec is not None:
        c["rec"] = rec
    if mode is not None:
        c["mode"] = mode
    return c


def end_edges():
    r = random.Random(12)
    for rec in (0, 1):
        for mode in (0, 1, 7):
            yield end_case(r, rec, mode)


def f_case(r, state=None, rec=None):
    c = base(r, "ra_f")
    c["args"] = [pick_action(r), rf(r), pick_id(r)]
    if state is not None:
        c["state"] = state
    if rec is not None:
        c["rec"] = rec
    return c


def f_edges():
    r = random.Random(13)
    for rec in (0, 1):
        for state in (0, 1, 5, -1):
            yield f_case(r, state, rec)
    for v in FLOATS:
        c = f_case(r, 1, 0)
        c["args"][1] = v
        yield c


def ff_case(r, state=None, rec=None):
    c = base(r, "ra_ff")
    c["args"] = [pick_action(r), rf(r), rf(r), pick_id(r)]
    if state is not None:
        c["state"] = state
    if rec is not None:
        c["rec"] = rec
    return c


def ff_edges():
    r = random.Random(14)
    for rec in (0, 1):
        for state in (0, 1, 5, -1):
            yield ff_case(r, state, rec)
    for v in FLOATS:
        c = ff_case(r, 1, 0)
        c["args"][1] = v
        c["args"][2] = r.choice(FLOATS)
        yield c


def w_case(r, state=None, rec=None):
    c = base(r, "ra_w")
    c["args"] = [pick_action(r), pick_id(r), r.choice([0, 1, 0x7F, 0x80, 255, 256, r.randrange(256)]), text(r)]
    if state is not None:
        c["state"] = state
    if rec is not None:
        c["rec"] = rec
    return c


def w_edges():
    r = random.Random(15)
    for rec in (0, 1):
        for state in (0, 1, 5, -1):
            yield w_case(r, state, rec)
    for preset in (False, True):
        for cut in (100, 0, 1, 3):
            for n in (0, 1, 2, 5):
                c = w_case(r, 1, 0)
                c["preset"] = preset
                c["queues"]["FilterCut"] = [cut]
                c["args"][3] = [0x41 + i for i in range(n)]
                yield c


TESTS = [
    FnTest("CActionRecorder::ReplayRecording", ppc, host, edge=replay_edges, rnd=replay_case),
    FnTest("CActionRecorder::EndReplay", ppc, host, edge=end_edges, rnd=end_case),
    FnTest("CActionRecorder::RecordAction", ppc, host, edge=f_edges, rnd=f_case),
    FnTest("CActionRecorder::RecordAction", ppc, host, edge=ff_edges, rnd=ff_case),
    FnTest("CActionRecorder::RecordAction", ppc, host, edge=w_edges, rnd=w_case),
]
