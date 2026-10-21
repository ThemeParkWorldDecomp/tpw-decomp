"""Unit v5047, TbMapStreamer::ReShuffleSoundForDroppable and SaveCatgoryFixupTable, against
libtpw_unit_TbMapStreamer_ReShuffleSoundForDroppable.dylib (tests/unit_TbMapStreamer_ReShuffleSoundForDroppable.cpp), sound library
(sound, TOC r2 0x1003abf0), src/sound/TbMapStreamer.cpp:
  TbMapStreamer::ReShuffleSoundForDroppable 0x1002309c, TbMapStreamer::SaveCatgoryFixupTable 0x100233e0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. Callees outside the unit are fakes on both sides that record
their arguments: TbSoundManager::IsBankDroppable (answers from a queue), the file's virtual Write / Seek / GetPos (reached on the Mac
through the glue 0x1002f154, told apart by the vtable slot in r12; the Write event holds the size, the first four bytes and a hash
of all bytes), the string helpers (name length, name text, the UString constructor, Replace, operator char*, destructor). CResize::Size,
CResize::operator[], USwap32 and the TbError constructor run as the original on the Mac side.

The world: mpSoundManager, a sound with its sample list (reshuffle); a streamer with its category array, containers with names, and
the file's GetPos answers (save). Compared: the sound's count word and samples, the answer, the event log.

No fused operations. Not tested: a sample count of 0x8000 or more (the list would not fit in memory).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_TbMapStreamer_ReShuffleSoundForDroppable"
PROGRAM = "sound symbols shared"
MASK = 0xFFFFFFFF

FN_A, FN_B = 0x1002309C, 0x100233E0
SIZES = {FN_A: 276, FN_B: 576}
OWN = set(SIZES)

GLUE = 0x1002F154
NAME_LENGTH, NAME_TEXT, STR_INIT, STR_REPLACE, STR_TEXT, STR_DESTROY = (0x1002EFA4, 0x10016BD8, 0x1002EE6C, 0x1002F0C4,
                                                                         0x10023674, 0x1002EE84)
IS_DROPPABLE = 0x100272B8
VT_WRITE, VT_SEEK, VT_GETPOS = 0xD0001014, 0xD0001018, 0xD0001020

ADDR = dict(mpSoundManager=0x1003F3B4)
HOST = {}


def s32(v):
    return T.s32(v)


def fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & MASK
    return h


def cbytes(e, p):
    t = e.cstr(p)
    return t.encode("latin-1") if isinstance(t, str) else bytes(t)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def keep_r2(e):
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def stub_droppable(e):
    L = e.cl
    L.rec("IsBankDroppable", (L.lab(e.r(3)), e.r(4) & MASK))
    e.set_r(3, L.ret("IsBankDroppable") & 0xFF)


def stub_glue(e):
    L = e.cl
    keep_r2(e)
    slot, f = e.r(12), e.r(3)
    if slot == VT_WRITE:
        size = e.r(5) & MASK
        data = e.read(e.r(4), size)
        L.rec("Write", (L.lab(f), size, int.from_bytes(data[:4], "little"), fnv(data)))
    elif slot == VT_SEEK:
        L.rec("Seek", (L.lab(f), e.r(4) & MASK, e.r(5) & MASK))
    elif slot == VT_GETPOS:
        L.rec("GetPos", (L.lab(f),))
        e.set_r(3, L.ret("GetPos") & MASK)
    else:
        raise AssertionError("unknown slot %x" % slot)


def stub_name_length(e):
    keep_r2(e)
    a = e.r(3)
    e.cl.rec("NameLength", (e.u32(a + 4),))
    e.set_r(3, len(cbytes(e, e.u32(a))))


def stub_name_text(e):
    a = e.r(3)
    p = e.u32(a)
    e.cl.rec("NameText", (e.u32(a + 4), fnv(cbytes(e, p))))
    e.set_r(3, p)


def stub_str_init(e):
    keep_r2(e)
    p = e.r(4)
    e.cl.rec("StringInit", (fnv(cbytes(e, p)),))
    e.write(e.r(3), struct.pack(">I", p))


def stub_str_replace(e):
    keep_r2(e)
    e.cl.rec("StringReplace", (s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))


def stub_str_text(e):
    e.cl.rec("StringText", ())
    e.set_r(3, e.u32(e.r(3)))


def stub_str_destroy(e):
    keep_r2(e)
    e.cl.rec("StringDestroy", (s32(e.r(4)),))


STUBS.internal(IS_DROPPABLE, stub_droppable)
STUBS.internal(GLUE, stub_glue)
STUBS.internal(NAME_LENGTH, stub_name_length)
STUBS.internal(NAME_TEXT, stub_name_text)
STUBS.internal(STR_INIT, stub_str_init)
STUBS.internal(STR_REPLACE, stub_str_replace)
STUBS.internal(STR_TEXT, stub_str_text)
STUBS.internal(STR_DESTROY, stub_str_destroy)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def halves(self, a, x, y):
        self.write(a, struct.pack(self.fmt[0] + "HH", x & 0xFFFF, y & 0xFFFF))

    def rd_sample(self, a):
        return self.rd_words(a, 3) + list(struct.unpack(self.fmt[0] + "HH", self.read(a + 12, 4)))


class Ppc(Side):
    fmt = ">I"
    name_off = 0x14
    tag_off = 4

    def __init__(self, e, case):
        self.e = e
        self.tbm = e.alloc(0x100)
        self.sm = e.alloc(16)
        self.file = e.alloc(0x20)
        vt = e.alloc(0x40)
        e.write(self.file, struct.pack(">I", vt))
        e.write(vt + 0x14, struct.pack(">III", VT_WRITE, VT_SEEK, 0))
        e.write(vt + 0x20, struct.pack(">I", VT_GETPOS))
        self.L = e.cl = CL.PpcLog({self.file: 1, self.sm: 2}, case["queues"])
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

    def ptr(self, a, p):
        self.write(a, struct.pack(">I", p))

    def p32(self, p):
        return p

    def log(self):
        return self.L.log

    def cat_array(self, ptrs):
        a = self.alloc(4 * len(ptrs) + 4)
        self.words(a, ptrs)
        self.ptr(self.tbm + 0x1C, a if ptrs else 0)
        self.words(self.tbm + 0x20, [len(ptrs)])

    def call_reshuffle(self, sound):
        T.call_ppc(self.e, FN_A, [self.tbm, sound])
        return 0

    def call_save(self):
        buf = self.alloc(4)
        self.words(buf, [0x77777777])
        T.call_ppc(self.e, FN_B, [buf, self.tbm, self.file])
        return self.rd_word(buf)


class Host(Side):
    fmt = "<I"
    tag_off = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.tbm = lib.tpw_msf_alloc(0x200)
        self.sm = lib.tpw_msf_alloc(16)
        self.file = lib.tpw_msf_addr(b"file")
        self.name_off = HOST["name_off"]
        CL.host_begin(lib, {1: self.file, 2: self.sm}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_msf_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_msf_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, p):
        self.write(a, struct.pack("<Q", p))

    def p32(self, p):
        return p - HOST["base"]

    def log(self):
        return CL.host_end(self.lib)

    def cat_array(self, ptrs):
        a = self.alloc(8 * len(ptrs) + 8)
        for i, p in enumerate(ptrs):
            self.ptr(a + 8 * i, p)
        off = HOST["cat_off"]
        self.ptr(self.tbm + off, a if ptrs else 0)
        self.words(self.tbm + off + 8, [len(ptrs)])

    def call_reshuffle(self, sound):
        self.lib.tpw_msf_reshuffle(self.tbm, sound)
        return 0

    def call_save(self):
        return self.lib.tpw_msf_save(self.tbm, self.file) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_msf_addr.restype, lib.tpw_msf_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_msf_alloc.restype, lib.tpw_msf_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_msf_base.restype, lib.tpw_msf_base.argtypes = C.c_uint64, []
    lib.tpw_msf_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_msf_reshuffle.restype, lib.tpw_msf_reshuffle.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    lib.tpw_msf_save.restype, lib.tpw_msf_save.argtypes = C.c_int32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_msf_layout(out)
    assert list(out)[2:n] == [42, 16], list(out)[:n]
    HOST["cat_off"], HOST["name_off"] = out[0], out[1]
    HOST["base"] = lib.tpw_msf_base()
    lib.bound = True


def build(s, case):
    s.ptr(s.g("mpSoundManager"), s.sm)
    if case["op"] == "reshuffle":
        samples = case["samples"]
        arr = s.alloc(16 * len(samples) + 32)
        for i, (w0, w1, w2, bank, h) in enumerate(samples):
            s.words(arr + 16 * i, [w0, w1, w2])
            s.halves(arr + 16 * i + 12, bank, h)
        s.sound = s.alloc(0x30)
        s.arr = arr
        s.words(s.sound, [case["count"], case["branch"], s.p32(arr)])
    else:
        ptrs = []
        for tag, text in case["cats"]:
            if text is None:
                ptrs.append(0)
                continue
            c = s.alloc(0x100)
            t = s.alloc(len(text) + 1)
            s.write(t, text + b"\0")
            name = c + s.name_off
            s.ptr(name, t)
            s.words(name + s.tag_off, [tag])
            ptrs.append(c)
        s.cat_array(ptrs)


def execute(s, case):
    return s.call_reshuffle(s.sound) if case["op"] == "reshuffle" else s.call_save()


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log()}
    if case["op"] == "reshuffle":
        out["count"] = s.rd_word(s.sound)
        out["branch"] = s.rd_word(s.sound + 4)
        out["samples"] = [s.rd_sample(s.arr + 16 * i) for i in range(len(case["samples"]))]
    return out


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


# ---------------------------------------------------------------------------------------------------------------- cases


def re_case(r, n=None, drops=None, high=None):
    if n is None:
        n = r.choice([0, 1, 2, 3, 4, 5, 8, r.randrange(0, 10)])
    if drops is None:
        drops = [r.choice([0, 1]) for _ in range(12)]
    if high is None:
        high = r.choice([0, 0xFFFF, r.randrange(0, 0x10000)])
    samples = [(r.randrange(1 << 32), r.randrange(1 << 32), r.randrange(1 << 32),
                r.choice([r.randrange(0, 8), r.randrange(0, 0x10000)]), r.randrange(0, 0x10000)) for _ in range(n)]
    return {"op": "reshuffle", "queues": {"IsBankDroppable": drops}, "count": (high << 16) | n,
            "branch": r.randrange(1 << 32), "samples": samples}


def re_edges():
    r = random.Random(5047)
    yield re_case(r, 0, [1])
    yield re_case(r, 0, [0], 0)
    yield re_case(r, 1, [0], 0)
    yield re_case(r, 1, [1], 0xFFFF)
    for n in (2, 3, 4, 7):
        yield re_case(r, n, [1], 0)
        yield re_case(r, n, [0], 0xFFFF)
        yield re_case(r, n, [1, 0, 1, 0, 1, 0, 1, 0], 7)
        yield re_case(r, n, [0, 1, 1, 0, 0, 1, 0, 1], 1)


def re_random(r):
    return re_case(r)


def name_bytes(r, n):
    return bytes(r.choice(b"abcXYZ019_:\\ ./-") if r.random() < 0.8 else r.randrange(33, 127) for _ in range(n))


def sv_case(r, cats=None, pos=None):
    if cats is None:
        cats = []
        for i in range(r.choice([0, 1, 2, 3, 4, r.randrange(0, 7)])):
            if r.random() < 0.25:
                cats.append((i + 1, None))
            else:
                cats.append((i + 1, name_bytes(r, r.choice([0, 1, 2, 3, r.randrange(0, 40)]))))
    if pos is None:
        pos = [r.choice([0, r.randrange(1 << 32), r.randrange(0, 1000)]), r.choice([0, r.randrange(1 << 32), r.randrange(0, 5000)])]
    return {"op": "save", "queues": {"GetPos": pos}, "cats": cats}


def sv_edges():
    r = random.Random(5048)
    yield sv_case(r, [], [0, 8])
    yield sv_case(r, [(1, None)], [10, 20])
    yield sv_case(r, [(1, b"")], [10, 20])
    yield sv_case(r, [(1, b"a:b\\c")], [5, 3])
    yield sv_case(r, [(1, b"x" * 254), (2, b"y" * 255), (3, b"z" * 253)], [0xFFFFFFF0, 4])
    yield sv_case(r, [(1, b"q" * 3), (2, None), (3, b"rs")], [100, 200])
    yield sv_case(r, [(i + 1, b"n%d" % i) for i in range(6)], [1, 1000])


def sv_random(r):
    return sv_case(r)


TESTS = [
    FnTest("TbMapStreamer::ReShuffleSoundForDroppable", ppc, host, edge=re_edges, rnd=re_random),
    FnTest("TbMapStreamer::SaveCatgoryFixupTable", ppc, host, edge=sv_edges, rnd=sv_random),
]
