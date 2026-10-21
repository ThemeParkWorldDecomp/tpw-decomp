"""Unit w167, the ride scripts' load, against libtpw_unit_RideLoad.dylib (tests/unit_RideLoad.cpp), main app ("SimTheme Park"), src/game/rides/rsse.cpp:
  RSSE_Load 0x100d56bc and RSSE_WALK::Byteswap 0x100d65a8.

A case is a file (a byte stream in the save's format, little-endian), the system's state before the call (initialised or not, a few live scripts,
the malloc count), the ride sets of the sound branch and the answers of the fakes. The world is built twice by ONE piece of Python code: in
emulator memory at the Mac's addresses (big-endian) for the original, and in the host library's memory (little-endian, the arena) for the
translation. The callees outside the unit are fakes on both sides that log their arguments: LbFile_Read (the file, the length, whether a
transferred pointer was given; the bytes come from the stream, zeros past its end), LbMem_Alloc (the Mac's; the host takes the arena directly,
so the sizes are read back from the blocks), CTRACE and GAME_ASSERTMSG (the texts), KillScript, RSSE_Init (the state to its start values),
_USwapBlock32 (the count and whether a block was given; the Mac's fake swaps the words, the host's does not: the file is little-endian),
GetRideMasterBoundArea and SOUND_AddObject (answers from the queues; the floats as bits). After the call the state, the globals, the log and the
list (each record's fields in native terms, its tables through their pointers with the swapped part as words and the rest raw, its objects) are
compared. The pointer words compare as zero or not zero (an address on the Mac, an arena offset on the host). RSSE_WALK::Byteswap is run on 32
bytes in each memory; the Mac swaps every field, the host swaps the same fields (it is unconditional there too).

Not tested: a state size above 0x14 (the Mac overflows the state), a record or object size above the struct's (the Mac overflows the block),
a state size below 0x10 (the Mac swaps stale words), a negative table size or count, a big-endian host's swaps (the unit is built without
TPW_SWAP_FILE_WORDS, so the host's load never calls RSSE_WALK::Byteswap or the thing id's swap; the Mac's run as the original), the size-0
branch of the allocator (dead in the original: its callers test the size first).
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_RideLoad"
MASK = 0xFFFFFFFF

FN = {"RSSE_Load": 0x100D56BC, "RSSE_WALK::Byteswap": 0x100D65A8}
OWN = set(FN.values()) | {0x100D5324, 0x100D5368, 0x100D53B0, 0x100D5424, 0x100D546C, 0x100D54B4, 0x100D54F8, 0x100D5554, 0x100D55C0,
                          0x100D561C, 0x100D5660, 0x10079A0C}
A_ASSERT, A_CTRACE, A_KILL, A_INIT, A_GRMBA, A_SOUND = 0x1000DDE8, 0x1006FCBC, 0x100D2ECC, 0x100D32E8, 0x101ED058, 0x100DE8E4
STATE, MALLOCS, F7BE0, RIDESETS = 0x102F7BEC, 0x102F7BDC, 0x102F7BE0, 0x102C79A4
FILE = 0x1234
HOST_LAYOUT = [0xF4, 0x1C, 0x20, 0x08, 0xCC, 0x78]
RSSE_TAG, OBJ_TAG, PAD = 0x45535352, 0x204A424F, 0x5F444150

# The record's fields in native terms: w a word, h a half word, p a pointer word (zero or not), rN N raw bytes.
RECORD = ([(0x00, "p"), (0x04, "p")] + [(o, "w") for o in range(0x08, 0x18, 4)] + [(o, "p") for o in range(0x18, 0x3C, 4)]
          + [(o, "w") for o in range(0x3C, 0x6C, 4)] + [(0x6C, "h"), (0x6E, "h")] + [(o, "w") for o in range(0x70, 0x80, 4)]
          + [(0x80, "w"), (0x84, "w"), (0x88, "h"), (0x8A, "h")] + [(o, "w") for o in range(0x8C, 0xA0, 4)]
          + [(0xA0, "w"), (0xA4, "w"), (0xA8, "w"), (0xAC, "h"), (0xAE, "r2"), (0xB0, "p"), (0xB4, "w"), (0xB8, "r4"), (0xBC, "w"),
             (0xC0, "h"), (0xC2, "h")] + [(o, "w") for o in range(0xC4, 0xDC, 4)]
          + [(0xDC, "h"), (0xDE, "h"), (0xE0, "w"), (0xE4, "h"), (0xE6, "h"), (0xE8, "r12")])
WALK = [(0, "h"), (2, "h"), (4, "h"), (6, "h"), (8, "w"), (0xC, "w"), (0x10, "w"), (0x14, "h"), (0x16, "h"), (0x18, "h"), (0x1A, "h"), (0x1C, "r4")]
OBJECT = [(0, "p"), (4, "p")] + [(o, "w") for o in range(8, 0x1C, 4)]
# The tables: name -> (pointer offset, count offset, words per count unit); the count fields hold the counts after the load.
TABLES = {"prog": (0x18, 0x50, 1), "vars": (0x1C, 0x8C, 1), "stack": (0x20, 0x54, 1), "limbo": (0x24, 0x58, 2), "bounce": (0x28, 0x64, 4),
          "words": (0x30, 0x4C, 1), "block": (0x34, 0x90, 0)}


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def u32at(e, a):
    return struct.unpack(">I", e.read(a, 4))[0]


def stub_read(e):
    # r3: the TbError's place, r4: the file, r5: the buffer, r6: the length, r7: bytes done.
    L, buffer, length, done = e.cl, e.r(5), e.r(6), e.r(7)
    L.rec("LbFile_Read", (e.r(4), length, 1 if done else 0))
    data = L.stream[L.fpos:L.fpos + length]
    data += bytes(length - len(data))
    L.fpos += length
    if length:
        e.write(buffer, data)
    if done:
        e.write(done, struct.pack(">I", length))
    e.write(e.r(3), bytes(4))


def stub_alloc(e):
    size = e.r(3)
    a = e.alloc(max(size, 4))
    e.cl.sizes[a] = size
    e.set_r(3, a)


def stub_swap_block(e):
    words, count = e.r(3), T.s32(e.r(4))
    e.cl.rec("USwapBlock32", (count, 1 if words else 0))
    if words and count > 0:
        data = e.read(words, 4 * count)
        e.write(words, b"".join(data[i:i + 4][::-1] for i in range(0, len(data), 4)))


def stub_ctrace(e):
    e.cl.rec("CTRACE", (T.s32(e.r(3)), T.s32(e.r(5))), (e.cstr(e.r(4))[:159],))


def stub_kill(e):
    script = e.r(3)
    e.cl.rec("KillScript", (T.s32(u32at(e, script + 8)), T.s32(e.r(4))))
    e.set_r(3, u32at(e, script))


def stub_init(e):
    e.cl.rec("RSSE_Init", ())
    e.write(STATE, struct.pack(">5I", 1, 0, 1, 0, 0))


def stub_grmba(e):
    L = e.cl
    L.rec("GetRideMasterBoundArea", (e.r(3),))
    ans = L.ret("grmba", 0) & MASK
    if ans:
        box = e.r(4)
        lo = [L.ret("bx%d" % i) for i in range(3)]
        hi = [L.ret("by%d" % i) for i in range(3)]
        e.write(box, b"".join(struct.pack(">I", v & MASK) for v in lo + hi))
    e.set_r(3, ans)


STUBS.imp("LbFile_Read__FPvPvUlPUl", stub_read)
STUBS.imp("LbMem_Alloc__FUi", stub_alloc)
STUBS.imp("_USwapBlock32__13SamsUtilitiesFPUll", stub_swap_block)
STUBS.internal(A_ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(A_CTRACE, stub_ctrace)
STUBS.internal(A_KILL, stub_kill)
STUBS.internal(A_INIT, stub_init)
STUBS.internal(A_GRMBA, stub_grmba)
STUBS.internal(A_SOUND, CL.recorder("SOUND_AddObject", "fffff", "i"))


def setup(runner):
    T.install_std_stubs(runner.emu)       # memset


# ---------------------------------------------------------------------------------------------------------------- the two sides

class Side:
    """What the shared code needs from either memory."""

    def get(self, a, kind):
        if kind in ("w", "p"):
            return struct.unpack(self.fmt, self.read(a, 4))[0]
        if kind == "h":
            return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]
        return self.read(a, int(kind[1:]))

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def field(self, a, kind):
        """A field as the comparison sees it: pointers as zero or one."""
        if kind == "p":
            return 1 if self.get(a, "w") else 0
        return self.get(a, kind)


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.stream, self.L.fpos, self.L.sizes = case["stream"], 0, {}
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(max(n, 4))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def up(self, w):
        return w

    def size(self, a):
        return self.L.sizes.get(a, -1)

    def state(self, init, f04, f08, count, head):
        self.words(STATE, [init, f04, f08, count, head])

    def globals(self, mallocs, f7):
        self.words(MALLOCS, [mallocs])
        self.words(F7BE0, [f7])

    def ride(self, slot, ybits):
        node = self.alloc(0x50)
        self.words(node + 0x44, [ybits])
        header = self.alloc(0x80)
        self.words(header + 0x78, [node])
        mesh = self.alloc(0x40)
        self.words(mesh + 4, [header])
        st = self.alloc(0x10)
        self.words(st + 8, [mesh])
        self.words(RIDESETS + 4 * slot, [st])

    def leak(self, flag):
        pass

    def call(self):
        r3, _, _ = T.call_ppc(self.e, FN["RSSE_Load"], [FILE])
        return r3 & MASK

    def after_state(self):
        return [u32at(self.e, STATE + 4 * i) for i in range(4)], u32at(self.e, STATE + 16)

    def after_globals(self):
        return [u32at(self.e, MALLOCS), u32at(self.e, F7BE0)]

    def log(self):
        return self.L.log


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_rw_reset()
        self.base = lib.tpw_rw_base()
        CL.host_begin(lib, {}, case["queues"])
        data = case["stream"]
        self.keep = C.create_string_buffer(data, max(len(data), 1))
        lib.tpw_rw_stream(self.keep, len(data))

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_rw_alloc(max(n, 4))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def up(self, w):
        return self.base + w if w else 0

    def size(self, a):
        return self.lib.tpw_rw_blocksize(a)

    def state(self, init, f04, f08, count, head):
        self.lib.tpw_rw_state(T.s32(init), T.s32(f04), T.s32(f08), T.s32(count), head)

    def globals(self, mallocs, f7):
        self.lib.tpw_rw_globals(T.s32(mallocs), T.s32(f7))

    def ride(self, slot, ybits):
        self.lib.tpw_rw_ride(slot, ybits)

    def leak(self, flag):
        self.lib.tpw_rw_leaknames(1 if flag else 0)

    def call(self):
        return self.lib.tpw_rw_RSSE_Load(FILE) & MASK

    def after_state(self):
        out = (C.c_int32 * 4)()
        head = self.lib.tpw_rw_state_out(out)
        return [v & MASK for v in out], head

    def after_globals(self):
        out = (C.c_int32 * 2)()
        self.lib.tpw_rw_globals_out(out)
        return [v & MASK for v in out]

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_rw_alloc.restype, lib.tpw_rw_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rw_base.restype = C.c_uint64
    lib.tpw_rw_blocksize.restype, lib.tpw_rw_blocksize.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_rw_stream.argtypes = [C.c_void_p, C.c_uint32]
    lib.tpw_rw_state.argtypes = [C.c_int32] * 4 + [C.c_uint64]
    lib.tpw_rw_state_out.restype, lib.tpw_rw_state_out.argtypes = C.c_uint64, [C.POINTER(C.c_int32)]
    lib.tpw_rw_globals.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_rw_globals_out.argtypes = [C.POINTER(C.c_int32)]
    lib.tpw_rw_ride.argtypes = [C.c_int32, C.c_uint32]
    lib.tpw_rw_leaknames.argtypes = [C.c_int32]
    lib.tpw_rw_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rw_RSSE_Load.restype, lib.tpw_rw_RSSE_Load.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_rw_Byteswap.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rw_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the file


def w(*values):
    return b"".join(struct.pack("<I", v & MASK) for v in values)


def script_bytes(sc):
    """One script's part of the file: the record, the tables behind their counts, the name, the objects."""
    out = bytearray(sc["rec"])
    for name in ("prog", "stack", "vars", "block", "limbo", "bounce"):
        out += w(len(sc[name])) + sc[name]
    out += w(len(sc["walk"]) // 32) + sc["walk"]
    out += w(len(sc["words"])) + sc["words"]
    name = sc["name"] + b"\0" if sc["name"] is not None else b""
    out += w(len(name)) + name
    out += w(sc.get("otag", OBJ_TAG), sc.get("ocount", len(sc["objects"])), sc.get("osize", 0x1C))
    for o in sc["objects"]:
        out += o
    return bytes(out)


def stream_bytes(f):
    out = bytearray(w(f.get("tag", RSSE_TAG), f.get("ssize", 0x14)))
    out += b"".join(w(v) for v in f["state"][:f.get("ssize", 0x14) // 4])
    if f.get("short"):
        return bytes(out)
    out += w(*[PAD] * 5) + w(f.get("count", len(f["scripts"])), f.get("rsize", 0xF4))
    for sc in f["scripts"]:
        out += script_bytes(sc)
    return bytes(out)


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """The state before the call (live scripts), the globals and the ride sets; the same code for both memories."""
    live = [s.alloc(0xF4) for _ in case["live"]]
    for i, handle in enumerate(case["live"]):
        s.words(live[i] + 0, [s.ptr(live[i + 1]) if i + 1 < len(live) else 0, s.ptr(live[i - 1]) if i else 0, handle])
    f = case["pre"]
    s.state(f[0], f[1], f[2], f[3], live[0] if live else 0)
    s.globals(case["mallocs"], case["f7"])
    for slot, ybits in case["rides"].items():
        s.ride(slot, ybits)
    s.leak(case.get("stale"))


Side.ptr = lambda self, a: a if self.fmt == ">I" else ((a - self.base) & MASK if a else 0)


def table(s, rec, name, ptrword):
    """A table through its pointer: its size, the swapped part as words, the rest raw."""
    a = s.up(ptrword)
    size = s.size(a)
    words = struct.unpack(s.fmt[0] + "%dI" % (size // 4), s.read(a, 4 * (size // 4))) if size >= 4 else ()
    return size, words


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log(), "globals": s.after_globals()}
    words, head = s.after_state()
    out["state"] = words
    out["head"] = 1 if head else 0
    scripts = []
    a = s.up(head) if s.fmt == ">I" else head
    guard = 0
    while a and guard < 16:
        guard += 1
        rec = {"size": s.size(a), "rec": [s.field(a + off, kind) for off, kind in RECORD]}
        field = {off: s.get(a + off, "w") for off in (0x4C, 0x50, 0x54, 0x58, 0x64, 0x7C, 0x8C, 0x90)}
        for name, (poff, coff, per) in TABLES.items():
            pw = s.get(a + poff, "w")
            if not pw:
                rec[name] = None
                continue
            p = s.up(pw)
            size = s.size(p)
            count = T.s32(field[coff])
            if name == "block":
                rec[name] = (size, s.read(p, size))
                continue
            nwords = min(per * count, size // 4)
            rec[name] = (size, struct.unpack(s.fmt[0] + "%dI" % nwords, s.read(p, 4 * nwords)) if nwords > 0 else (), s.read(p + 4 * nwords, size - 4 * nwords))
        pw = s.get(a + 0x2C, "w")
        if pw:
            p = s.up(pw)
            size = s.size(p)
            rec["walk"] = (size, [[s.get(p + 0x20 * k + off, kind) for off, kind in WALK] for k in range(size // 32)])
        else:
            rec["walk"] = None
        pw = s.get(a + 0x38, "w")
        rec["name"] = None if not pw else (1 if case.get("stale") else (s.size(s.up(pw)), s.read(s.up(pw), s.size(s.up(pw)))))
        objs = []
        o = s.up(s.get(a + 0xB0, "w"))
        while o and len(objs) < 16:
            objs.append((s.size(o), [s.field(o + off, kind) for off, kind in OBJECT]))
            o = s.up(s.get(o, "w"))
        rec["objects"] = objs
        scripts.append(rec)
        a = s.up(s.get(a, "w"))
    out["scripts"] = scripts
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call())
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the cases


def bits(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def rbytes(r, n):
    return bytes(r.getrandbits(8) for _ in range(n))


def rec_bytes(r, over=None):
    """A record of random bytes; the ride number (+0xc8) small, the sound handle (+0xd4) as asked."""
    b = bytearray(rbytes(r, 0xF4))
    b[0xC8:0xCC] = w(r.choice([0, 1, 2, 3]))
    b[0xD4:0xD8] = w(r.choice([0, 0, 5, 0x12345678]))
    for off, v in (over or {}).items():
        b[off:off + 4] = w(v)
    return bytes(b)


def rand_script(r, over=None, objects=None):
    def table_bytes(unit):
        return rbytes(r, r.choice([0, 0, unit, unit * 2, unit * 3 + r.randrange(unit)]))
    return {"rec": rec_bytes(r, over), "prog": table_bytes(4), "stack": table_bytes(4), "vars": table_bytes(4), "block": table_bytes(4),
            "limbo": table_bytes(8), "bounce": table_bytes(16), "walk": rbytes(r, 32 * r.choice([0, 0, 1, 2, 3])),
            "words": table_bytes(4), "name": bytes(r.choice(b"abcdefghijklmnopqrstuvwxyz_0123456789") for _ in range(r.randrange(1, 21))),
            "objects": [rbytes(r, 0x1C) for _ in range(r.randrange(0, 5) if objects is None else objects)]}


def rfloat32(r):
    return bits(float(T.f32(r.choice([0.0, -0.0, 1.0, -1.0, 0.5, 100.0, -250.5, 1e-20, 12345.678, r.uniform(-1e4, 1e4), r.uniform(-1, 1)]))))


def mk_case(r, scripts, f=None, live=(), init=0, mallocs=0, stale=False, answers=None):
    f = dict(f or {})
    f.setdefault("state", [r.choice([0, 1, 2]), r.randrange(100), r.randrange(100), r.randrange(100), r.getrandbits(32)])
    f["scripts"] = scripts
    queues = {"grmba": answers if answers is not None else [r.choice([0, 1, 1])] * 2, "SOUND_AddObject": [r.choice([0, 7, 1000])]}
    for i in range(3):
        queues["bx%d" % i] = [rfloat32(r), rfloat32(r)]
        queues["by%d" % i] = [rfloat32(r), rfloat32(r)]
    if answers is not None and len(answers) > 1:
        queues["grmba"] = list(answers)
    return {"stream": stream_bytes(f), "live": list(live), "pre": [init, r.randrange(50), r.randrange(50), len(live)], "mallocs": mallocs,
            "f7": r.randrange(100), "rides": {1: rfloat32(r), 2: rfloat32(r), 3: rfloat32(r)}, "queues": queues, "stale": stale, "desc": ""}


def random_load(r):
    n = r.choice([0, 1, 1, 2, 3, 4])
    live = [r.randrange(1, 100) for _ in range(r.choice([0, 0, 1, 2, 3]))]
    f = {}
    case = mk_case(r, [rand_script(r) for _ in range(n)], f, live, init=r.choice([0, 1]) if live or r.random() < 0.5 else 0,
                   mallocs=r.choice([0, 3]))
    case["pre"][0] = 1 if live else r.choice([0, 1])
    return case


def load_edges():
    from random import Random
    r = Random(167)
    cases = []

    def add(**kw):
        scripts = kw.pop("scripts", [rand_script(r)])
        cases.append(mk_case(r, scripts, **kw))
        return cases[-1]

    add(scripts=[])                                                     # no scripts
    add(f={"tag": 0x12345678})                                          # a wrong tag
    add(f={"tag": 0x12345678}, live=[4, 9], init=1, mallocs=2)          # a wrong tag, live scripts killed first
    add(f={"short": True}, scripts=[])                                  # the file ends after the state
    add(f={"ssize": 0x10}, scripts=[rand_script(r)])                    # a state of four words
    for k in range(4):                                                  # live scripts before the load
        for mallocs in (0, 5):
            c = add(live=[10 + i for i in range(k)], mallocs=mallocs)
            c["pre"][0] = 1
    c = add(live=[3, 4])
    c["pre"][0] = 0                                                     # not initialised: the live list is left alone
    add(scripts=[dict(rand_script(r), otag=0x11111111)])                # a wrong object tag
    add(scripts=[dict(rand_script(r), otag=0), rand_script(r)])         # ... in the first of two
    add(scripts=[rand_script(r), dict(rand_script(r), otag=0)])         # ... in the second
    add(scripts=[dict(rand_script(r), name=None, objects=[])], stale=True)   # a name of length 0 (the record's word stays)
    add(scripts=[dict(rand_script(r, {0x38: 0}), name=None)], stale=False)
    add(scripts=[dict(rand_script(r, {0x38: 0x00ABCDE8}), name=None)], stale=True)
    add(scripts=[dict(rand_script(r), prog=b"", stack=b"", vars=b"", block=b"", limbo=b"", bounce=b"", walk=b"", words=b"")])   # empty tables
    for sizes in (1, 3, 5, 7, 9, 15, 17):                                # byte counts that are not multiples
        add(scripts=[dict(rand_script(r), prog=rbytes(r, sizes), stack=rbytes(r, sizes), vars=rbytes(r, sizes), block=rbytes(r, sizes),
                          limbo=rbytes(r, sizes), bounce=rbytes(r, sizes), words=rbytes(r, sizes))])
    add(scripts=[dict(rand_script(r), objects=[rbytes(r, 0x1C) for _ in range(5)])])
    add(scripts=[dict(rand_script(r), objects=[])])
    # the sound: no handle, a handle without a ride, then each answer of the bound area
    for over, answers in (({0xD4: 0}, [1, 1]), ({0xD4: 9, 0xC8: 0}, [1, 1]), ({0xD4: 9, 0xC8: 1}, [0, 1]), ({0xD4: 9, 0xC8: 1}, [1, 0]),
                          ({0xD4: 9, 0xC8: 2}, [1, 1]), ({0xD4: 9, 0xC8: 3}, [5, -1])):
        add(scripts=[rand_script(r, over)], answers=answers)
    add(scripts=[rand_script(r, {0xD4: 9, 0xC8: 1}), rand_script(r, {0xD4: 9, 0xC8: 2})], answers=[1, 1])
    add(f={"count": 2}, scripts=[rand_script(r)])                       # the count says more than the file has: zeros
    add(f={"rsize": 0xF0}, scripts=[dict(rand_script(r), rec=rec_bytes(r)[:0xF0])])   # a record size smaller than the struct
    return cases


# ---------------------------------------------------------------------------------------------------------------- RSSE_WALK::Byteswap


def swap_ppc(e, case):
    s = Ppc(e, {"queues": {}, "stream": b""})
    try:
        a = e.alloc(32, case["rec"])
        T.call_ppc(e, FN["RSSE_WALK::Byteswap"], [a])
        return {"image": e.read(a, 32)}
    finally:
        s.done()


def swap_host(lib, case):
    bind(lib)
    buf = C.create_string_buffer(case["rec"], 32)
    lib.tpw_rw_Byteswap(C.addressof(buf))
    return {"image": buf.raw}


def swap_edges():
    return [{"rec": bytes(32)}, {"rec": b"\xff" * 32}, {"rec": bytes(range(32))}, {"rec": bytes(range(32, 0, -1))}]


def swap_rnd(r):
    return {"rec": rbytes(r, 32)}


TESTS = [FnTest("RSSE_Load", ppc, host, edge=load_edges, rnd=random_load),
         FnTest("RSSE_WALK::Byteswap", swap_ppc, swap_host, edge=swap_edges, rnd=swap_rnd)]
