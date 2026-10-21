"""Unit v51032: CSprocketsChannelMac::SetDescription, the UReferenceCounter<USoundDescription> copy constructor and destructor,
TbMapStreamer::CCatFixupTable::Unload / LoadCatgoryFixupTable / SetDroppable and TbStaticFileBank's destructor and Deregister,
against libtpw_unit_CSprocketsChannel_SetDescription_2.dylib (tests/unit_CSprocketsChannel_SetDescription_2.cpp), sound program
("sound symbols shared"), src/sound/TbMapStreamer.cpp, src/sound/TimerCodec.cpp:
  SetDescription 0x1002a9f8, UReferenceCounter copy 0x1002ad08, ~UReferenceCounter 0x1002ae58, CCatFixupTable::Unload 0x10023a78,
  LoadCatgoryFixupTable 0x1002370c, SetDroppable 0x100231fc, ~TbStaticFileBank 0x10013350, TbStaticFileBank::Deregister 0x10013278

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses
(big-endian) for the original, and in the host library's memory (little-endian, native pointers) for the translation. The callees outside
the unit (Cleanup, the buffer and object allocations, the USoundBuffer constructor, the counter and command constructors, the send glue,
RemoveReference, AddReference, the buffer release, the object free, UDebug, CCatFixupTable::Load, ReShuffleSoundForDroppable,
~TbFileBank) are fakes on both sides that record their arguments (pointers as labels, UDebug as a hash of its texts) and answer from the
case's queues. The Size and operator[] of the category array, the TbError constructor and, for ~TbStaticFileBank, the send glue (it
calls Deregister through the vtable the destructor stores) run as the original on the PowerPC side.

The world of a case: the channel, header, buffer and description blocks of SetDescription, the counters, the fixup table, the streamer
with its category array (containers, data, events and sounds in a pool whose sounds are labelled), the file bank. What is compared:
the answer, every field the call may write, the whole sound pool and the event log.
The Mac returns this from the two counter functions; the host functions return nothing, so the host answer is the token of the
object it was given. Not tested: LoadCatgoryFixupTable with a NULL file (the host would build a null reference), a category count
beyond the array (the original grows the array through operator new).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CSprocketsChannel_SetDescription_2"
PROGRAM = "sound symbols shared"
MASK = 0xFFFFFFFF

SETDESC, COPY, CDTOR, UNLOAD, LOADCAT, SETDROP, SFBDTOR, SFBDEREG = (
    0x1002A9F8, 0x1002AD08, 0x1002AE58, 0x10023A78, 0x1002370C, 0x100231FC, 0x10013350, 0x10013278)
SIZES = {SETDESC: 332, COPY: 68, CDTOR: 104, UNLOAD: 68, LOADCAT: 68, SETDROP: 224, SFBDTOR: 120, SFBDEREG: 96}
OWN = set(SIZES)
REAL_EXTRA = {0x100015B8, 0x1001F320, 0x10016FC8}     # TbError::TbError, CResize::Size, CResize::operator[]
GLUE = 0x1002F154

MAC = dict(buf=0x40, bufsize=0x44, fix_data=4, fix_count=0, st_fixup=4, st_cats=0x1C, cats_size=0x20, cont_data=8,
           bank_data=8, bank_cache=0x1C)
HOST_KEYS = ["buf", "bufsize", "fix_data", "fix_count", "st_fixup", "st_cats", "cont_data", "bank_data", "bank_cache", "cats_size"]
NBLK = 6
POOL_SOUNDS = 8
SOUND = 0x2A


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes


def hash_text(text, h):
    if isinstance(text, str):
        text = text.encode("latin1")
    for c in text:
        h = (h * 31 + c) & MASK
    return h


def build_stubs(real_glue):
    S = CL.Stubs()
    S.real.update(OWN)
    S.real.update(REAL_EXTRA)
    plain_internal = S.internal

    def internal(addr, fn):
        """The callees are glue calls: the glue saves the caller's TOC at 0x14(r1) and the caller reloads it afterwards."""
        def wrapped(e):
            e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
            fn(e)
        plain_internal(addr, wrapped)

    S.internal = internal

    def rec(name, fn):
        def stub(e):
            e.cl.rec(name, tuple(fn(e)))
        return stub

    L = lambda e, v: e.cl.lab(v)

    def stub_udebug(e):
        fmt = e.cstr(e.r(3))
        h = hash_text(fmt, 7)
        if "%s" in (fmt if isinstance(fmt, str) else fmt.decode("latin1")):
            h = hash_text(e.cstr(e.r(4)), h)
        e.cl.rec("UDebug", (h & 0x7FFFFFFF,))

    def stub_alloc(e):
        e.cl.rec("AllocBuffer", (e.r(3) & MASK,))
        e.set_r(3, e.world.blk[2] if e.cl.ret("AllocBuffer") else 0)

    def stub_make_obj(e):
        e.cl.rec("UAllocatorMake", (s32(e.r(3)),))
        e.set_r(3, e.world.blk[3] if e.cl.ret("UAllocatorMake") else 0)

    def stub_soundbuffer(e):
        hi, lo = struct.unpack(">II", struct.pack(">d", e.f(1)))
        e.cl.rec("USoundBuffer", (L(e, e.r(3)), L(e, e.r(4)), e.r(5) & MASK, e.r(6) & 0xFF, hi, lo, e.r(9) & MASK, s32(e.r(10))))

    def stub_make(e):
        e.cl.rec("CounterMake", (L(e, e.r(4)),))
        e.write(e.r(3), struct.pack(">I", e.r(4) & MASK))

    S.internal(0x1002A3F0, rec("Cleanup", lambda e: (L(e, e.r(3)),)))
    S.internal(0x1002EBFC, stub_alloc)
    S.internal(0x1002EAC4, stub_make_obj)
    S.internal(0x1002ECD4, stub_soundbuffer)
    S.internal(0x1002AFEC, stub_make)
    S.internal(0x1002ADFC, rec("AddReference", lambda e: (L(e, e.r(3)),)))
    S.internal(0x1002AF28, rec("RemoveReference", lambda e: (L(e, e.r(3)),)))
    S.internal(0x1002AC30, rec("CmdCtor", lambda e: (L(e, e.u32(e.r(4))),)))
    S.internal(0x1002AB8C, rec("CmdDtor", lambda e: (T.s16(e.r(4)),)))
    S.internal(0x1002EA94, rec("Free", lambda e: (L(e, e.r(3)),)))
    S.internal(0x1002EBCC, rec("Dispose", lambda e: (L(e, e.r(3)),)))
    S.internal(0x10003950, stub_udebug)
    S.internal(0x1002388C, rec("Load", lambda e: (L(e, e.r(4)), L(e, e.r(5)))))
    S.internal(0x1002309C, rec("Reshuffle", lambda e: (L(e, e.r(3)), L(e, e.r(4)))))
    S.internal(0x10009144, rec("~TbFileBank", lambda e: (L(e, e.r(3)),)))
    if real_glue:
        S.real.add(GLUE)
    else:
        S.internal(GLUE, rec("Send", lambda e: (L(e, e.r(4)),)))
    return S


STUBS = build_stubs(False)
STUBS_GLUE = build_stubs(True)


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

    def tok(self, v):
        """A pointer as a token: 0, or the number of the block it points to."""
        if v == 0:
            return 0
        for i, b in enumerate(self.blk):
            if v == b:
                return i + 1
        return 0xDEAD

    def fixed(self):
        d = {b: i + 1 for i, b in enumerate(self.blk)}
        for i in range(POOL_SOUNDS):
            d[self.pool + SOUND * i] = 40 + i
        d[self.blk[0] + self.O["st_fixup"]] = 30
        return d

    def alloc_world(self):
        self.blk = [self.alloc(0x200) for _ in range(NBLK)]
        self.pool = self.alloc(SOUND * POOL_SOUNDS)


class Ppc(Side):
    fmt = ">I"
    psz = 4
    O = MAC

    def __init__(self, e, case):
        self.e = e
        self.alloc_world()
        e.world = self
        fixed = self.fixed()
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = (STUBS_GLUE if case["op"] == "sfb_dtor" else STUBS).activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a, v):
        self.write(a, struct.pack(">I", v & MASK))

    def rd_ptr(self, a):
        return self.rd_word(a)

    def p32(self, v):
        return v

    def log(self):
        return self.L.log

    def call(self, entry, args):
        return T.call_ppc(self.e, entry, [a & MASK for a in args])[0] & MASK


class Host(Side):
    fmt = "<I"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.O = lib.offsets
        self.alloc_world()
        self.base = lib.tpw_sd_arenabase()      # read after the first allocation: the arena may be set up lazily
        CL.host_begin(lib, {label: addr for addr, label in self.fixed().items()}, case["queues"])
        lib.tpw_sd_setblock(0, self.blk[2])
        lib.tpw_sd_setblock(1, self.blk[3])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_sd_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def p32(self, v):
        return (v - self.base) if v else 0

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    P, U32, I32 = C.c_uint64, C.c_uint32, C.c_int32
    lib.tpw_sd_alloc.restype, lib.tpw_sd_alloc.argtypes = P, [U32]
    lib.tpw_sd_arenabase.restype, lib.tpw_sd_arenabase.argtypes = P, []
    lib.tpw_sd_setblock.argtypes = [U32, P]
    lib.tpw_sd_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sd_setdesc.argtypes = [P, P]
    lib.tpw_sd_copy.argtypes = [P, P]
    lib.tpw_sd_counter_dtor.argtypes = [P, I32]
    lib.tpw_sd_unload.argtypes = [P]
    lib.tpw_sd_loadcat.restype, lib.tpw_sd_loadcat.argtypes = U32, [P, P]
    lib.tpw_sd_setdroppable.argtypes = [P, U32]
    lib.tpw_sd_sfb_dereg.argtypes = [P]
    lib.tpw_sd_sfb_dtor.restype, lib.tpw_sd_sfb_dtor.argtypes = P, [P, I32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sd_layout(out)
    assert n == len(HOST_KEYS), n
    lib.offsets = dict(zip(HOST_KEYS, list(out)[:n]))
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    op = s.op = case["op"]
    A, B, C_, D, E, F = s.blk
    O = s.O
    if op == "setdesc":
        s.ptr(A + O["buf"], 0)
        s.words(A + O["bufsize"], [case["oldsize"]])
        s.words(A + 0x10, [F & MASK])      # mVtable10: the original reads slot +0x2c of it for the send glue (a stub here)
        s.write(B + 4, struct.pack(s.fmt[0] + "H", case["rate"]) + bytes([case["bits"], case["flags"]]))
    elif op == "copy":
        s.ptr(A, E)
        s.ptr(B, C_ if case["src"] else 0)
    elif op == "cdtor":
        s.ptr(A, C_ if case["obj"] else 0)
    elif op == "unload":
        s.ptr(A + O["fix_data"], C_ if case["data"] else 0)
        s.words(A + O["fix_count"], [case["count"]])
    elif op == "loadcat":
        s.words(C_, [0xDEADBEEF])
        s.words(A + O["st_fixup"] + O["fix_count"], [case["count"]])
    elif op == "setdrop":
        for i in range(POOL_SOUNDS):       # the first word of a sound is a logical value, the other 38 bytes are opaque
            s.words(s.pool + SOUND * i, [case["pool"][i]])
            s.write(s.pool + SOUND * i + 4, case["rest"][i])
        cats = case["cats"]
        n = len(cats)
        arr = s.alloc(max(n, 1) * s.psz + 8) if n else 0
        s.ptr(A + O["st_cats"], arr)
        s.words(A + O["cats_size"], [n])
        idx = 0
        for i, cat in enumerate(cats):
            if cat is None:
                s.ptr(arr + i * s.psz, 0)
                continue
            cont = s.alloc(0x40)
            s.ptr(arr + i * s.psz, cont)
            if cat[0] == "nodata":
                s.ptr(cont + O["cont_data"], 0)
                continue
            counts = cat[1]
            events = s.alloc(max(len(counts), 1) * 0x14) if counts else 0
            data = s.alloc(24)
            s.words(data, [len(counts), s.p32(events)])
            s.ptr(cont + O["cont_data"], data)
            for j, cnt in enumerate(counts):
                s.words(events + 0x14 * j + 4, [cnt, s.p32(s.pool + SOUND * idx) if cnt else 0])
                idx += cnt
    elif op in ("sfb_dereg", "sfb_dtor"):
        s.ptr(A + O["bank_data"], C_ if case["data"] else 0)
        s.ptr(A + O["bank_cache"], D if case["cache"] else 0)


def execute(s, case):
    op = case["op"]
    A, B, C_, D, E, F = s.blk
    h = isinstance(s, Host)
    if op == "setdesc":
        if h:
            s.lib.tpw_sd_setdesc(A, B)
        else:
            s.call(SETDESC, [A, B])
        return 0
    if op == "copy":
        if h:
            s.lib.tpw_sd_copy(A, B)
            return s.tok(A)
        return s.tok(s.call(COPY, [A, B]))
    if op == "cdtor":
        this = A if case["self"] else 0
        if h:
            s.lib.tpw_sd_counter_dtor(this, s32(case["flag"]))
            return s.tok(this)
        return s.tok(s.call(CDTOR, [this, case["flag"]]))
    if op == "unload":
        if h:
            s.lib.tpw_sd_unload(A)
        else:
            s.call(UNLOAD, [A])
        return 0
    if op == "loadcat":
        if h:
            return s.lib.tpw_sd_loadcat(A, B)
        s.call(LOADCAT, [C_, A, B])
        return s.rd_word(C_)
    if op == "setdrop":
        if h:
            s.lib.tpw_sd_setdroppable(A, case["value"])
        else:
            s.call(SETDROP, [A, case["value"]])
        return 0
    if op == "sfb_dereg":
        if h:
            s.lib.tpw_sd_sfb_dereg(A)
        else:
            s.call(SFBDEREG, [A])
        return 0
    if op == "sfb_dtor":
        this = A if case["self"] else 0
        if h:
            return s.tok(s.lib.tpw_sd_sfb_dtor(this, s32(case["flag"])))
        return s.tok(s.call(SFBDTOR, [this, case["flag"]]))


def observe(s, case, answer):
    op = case["op"]
    A, B, C_, D, E, F = s.blk
    O = s.O
    r = {"answer": answer, "log": s.log()}
    if op == "setdesc":
        r["buffer"] = s.tok(s.rd_ptr(A + O["buf"]))
        r["size"] = s.rd_word(A + O["bufsize"])
    elif op == "copy":
        r["self"] = s.tok(s.rd_ptr(A))
        r["src"] = s.tok(s.rd_ptr(B))
    elif op == "cdtor":
        r["obj"] = s.tok(s.rd_ptr(A))
    elif op == "unload":
        r["data"] = s.tok(s.rd_ptr(A + O["fix_data"]))
        r["count"] = s.rd_word(A + O["fix_count"])
    elif op == "loadcat":
        r["count"] = s.rd_word(A + O["st_fixup"] + O["fix_count"])
        r["data"] = s.tok(s.rd_ptr(A + O["st_fixup"] + O["fix_data"]))
    elif op == "setdrop":
        r["pool"] = [s.rd_word(s.pool + SOUND * i) for i in range(POOL_SOUNDS)]
        r["rest"] = [s.read(s.pool + SOUND * i + 4, SOUND - 4) for i in range(POOL_SOUNDS)]
    else:
        r["data"] = s.tok(s.rd_ptr(A + O["bank_data"]))
        r["cache"] = s.tok(s.rd_ptr(A + O["bank_cache"]))
    return r


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


# ---------------------------------------------------------------------------------------------------------------- case generators


def r32(r):
    return r.getrandbits(32)


def base(r, op):
    return {"op": op, "queues": {"AllocBuffer": [r.choice([0, 1])], "UAllocatorMake": [r.choice([0, 1])]}}


def setdesc_case(r, flags=None, rate=None, bits=None, alloc=None, make=None):
    c = base(r, "setdesc")
    c["flags"] = r.choice([0, 1, 2, 0xFF, 0xFE, r.randrange(256)]) if flags is None else flags
    c["rate"] = r.choice([0, 22050, 44100, 0xFFFF, 0x8000, r.randrange(65536)]) if rate is None else rate
    c["bits"] = r.choice([0, 8, 16, 0xFF, r.randrange(256)]) if bits is None else bits
    c["oldsize"] = r32(r)
    if alloc is not None:
        c["queues"]["AllocBuffer"] = [alloc]
    if make is not None:
        c["queues"]["UAllocatorMake"] = [make]
    return c


def setdesc_edges():
    r = random.Random(11)
    for alloc in (0, 1):
        for make in (0, 1):
            for flags in (0, 1, 2, 0xFE, 0xFF):
                yield setdesc_case(r, flags=flags, alloc=alloc, make=make)
    yield setdesc_case(r, rate=0xFFFF, bits=0xFF, alloc=1, make=1)
    yield setdesc_case(r, rate=0, bits=0, alloc=1, make=1)


def copy_case(r, src=None):
    c = base(r, "copy")
    c["src"] = r.choice([0, 1]) if src is None else src
    return c


def copy_edges():
    r = random.Random(12)
    yield copy_case(r, 0)
    yield copy_case(r, 1)


FLAGS = [-1, 0, 1, 2, 0x7FFF, 0x8000, 0x10000, 0x10001, 0xFFFF8000, 0xFFFFFFFF, 5]


def cdtor_case(r, this=None, obj=None, flag=None):
    c = base(r, "cdtor")
    c["self"] = r.choice([0, 1, 1]) if this is None else this
    c["obj"] = r.choice([0, 1]) if obj is None else obj
    c["flag"] = (r.choice(FLAGS) if flag is None else flag) & MASK
    return c


def cdtor_edges():
    r = random.Random(13)
    for this in (0, 1):
        for obj in (0, 1):
            for flag in FLAGS:
                yield cdtor_case(r, this, obj, flag)


def unload_case(r, data=None):
    c = base(r, "unload")
    c["data"] = r.choice([0, 1]) if data is None else data
    c["count"] = r32(r)
    return c


def unload_edges():
    r = random.Random(14)
    yield unload_case(r, 0)
    yield unload_case(r, 1)


def loadcat_case(r):
    c = base(r, "loadcat")
    c["count"] = r32(r)
    return c


def loadcat_edges():
    r = random.Random(15)
    for _ in range(3):
        yield loadcat_case(r)


def setdrop_case(r, ncat=None, value=None):
    c = base(r, "setdrop")
    c["pool"] = [r32(r) for _ in range(POOL_SOUNDS)]
    c["rest"] = [bytes(r.getrandbits(8) for _ in range(SOUND - 4)) for _ in range(POOL_SOUNDS)]
    c["value"] = r.choice([0, 1, 2, 0x100, 0xFF, 0x101, r.randrange(256)]) if value is None else value
    left = POOL_SOUNDS
    cats = []
    for _ in range(r.randrange(0, 5) if ncat is None else ncat):
        k = r.random()
        if k < 0.2:
            cats.append(None)
        elif k < 0.35:
            cats.append(("nodata",))
        else:
            counts = []
            for _ in range(r.randrange(0, 4)):
                n = min(r.randrange(0, 3), left)
                left -= n
                counts.append(n)
            cats.append(("data", counts))
    c["cats"] = cats
    return c


def setdrop_edges():
    r = random.Random(16)
    for value in (0, 1, 0x100, 0xFF):
        for ncat in (0, 1, 3):
            yield setdrop_case(r, ncat, value)
    for value in (0, 1):
        c = setdrop_case(r, 0, value)
        c["cats"] = [None, ("nodata",), ("data", []), ("data", [0, 1, 2]), ("data", [3, 0, 2])]
        yield c


def sfb_case(r, op, data=None, cache=None, this=None, flag=None):
    c = base(r, op)
    c["data"] = r.choice([0, 1]) if data is None else data
    c["cache"] = r.choice([0, 1]) if cache is None else cache
    c["self"] = r.choice([0, 1, 1, 1]) if this is None else this
    c["flag"] = (r.choice([-1, 0, 1, 2, 0x10000, 0x10001, 0xFFFF8000]) if flag is None else flag) & MASK
    return c


def dereg_edges():
    r = random.Random(17)
    for data in (0, 1):
        for cache in (0, 1):
            yield sfb_case(r, "sfb_dereg", data, cache)


def sfbdtor_edges():
    r = random.Random(18)
    for this in (0, 1):
        for data in (0, 1):
            for cache in (0, 1):
                for flag in (-1, 0, 1, 0x10000, 0x10001):
                    yield sfb_case(r, "sfb_dtor", data, cache, this, flag)


TESTS = [
    FnTest("CSprocketsChannel::SetDescription", ppc, host, edge=setdesc_edges, rnd=setdesc_case),
    FnTest("SamsUtilities::UReferenceCounter<SamsUtilities::USoundDescription>::UReferenceCounter", ppc, host, edge=copy_edges,
           rnd=copy_case),
    FnTest("SamsUtilities::UReferenceCounter<SamsUtilities::USoundDescription>::~UReferenceCounter", ppc, host, edge=cdtor_edges,
           rnd=cdtor_case),
    FnTest("TbMapStreamer::CCatFixupTable::Unload", ppc, host, edge=unload_edges, rnd=unload_case),
    FnTest("TbMapStreamer::LoadCatgoryFixupTable", ppc, host, edge=loadcat_edges, rnd=loadcat_case),
    FnTest("TbMapStreamer::SetDroppable", ppc, host, edge=setdrop_edges, rnd=setdrop_case),
    FnTest("TbStaticFileBank::~TbStaticFileBank", ppc, host, edge=sfbdtor_edges, rnd=lambda r: sfb_case(r, "sfb_dtor")),
    FnTest("TbStaticFileBank::Deregister", ppc, host, edge=dereg_edges, rnd=lambda r: sfb_case(r, "sfb_dereg")),
]
