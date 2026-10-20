"""Unit v5956: std::vector<Argument>::clear / tear_down, CToken::CToken (copy and string + member pointer), CParser::GetChar,
COnlinePerson::COnlinePerson and std::list<std::wstring>::insert, against libtpw_unit_std_vector_clear.dylib
(tests/unit_std_vector_clear.cpp), main app ("SimTheme Park"),
src/COnlineParser_ParseGoto_COnlineParser_COnlineParser_std_vector_clear.cpp:
  clear 0x102108a4, tear_down 0x10201310, CToken::CToken 0x10210d4c and 0x10211f88, CParser::GetChar 0x10211ef8,
  COnlinePerson::COnlinePerson 0x10212d98, std::list<std::wstring>::insert 0x1020deec

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers) for the translation; the same code reads both back.
The callees outside the unit (the two std::__destroy<Argument*>, operator new / new[] / delete, strlen, strcpy, __throw, the string
copy constructor and operator[], CPeep::CPeep, CThing::GetThingId, CWorld::SeedRand, ESP_Get*, SpawnSpriteScript,
SCR_SetFloatVariable, swprintf) are fakes on both sides that record their arguments. Objects are persistent blocks A..D at fixed
addresses, so pointers are logged as labels; the allocator hands out eight pool blocks in order.

The world of a case: random bytes in the blocks, the object under test with the fields the function reads, the answer queues of the
fakes. What is compared: the answer, every field the function writes (typed, so the byte order does not matter), the rest of the
object bytes, the pointer fields as block names, and the event log.

No fused operations. Not tested: nothing is skipped (the throw path of insert continues after the fake __throw returns, as the original does).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_vector_clear"
MASK = 0xFFFFFFFF

A_CLEAR, A_TEAR, A_TOKCOPY, A_GETCHAR, A_TOKCTOR, A_PERSON, A_INSERT = (
    0x102108A4, 0x10201310, 0x10210D4C, 0x10211EF8, 0x10211F88, 0x10212D98, 0x1020DEEC)
SIZES = {A_CLEAR: 76, A_TEAR: 88, A_TOKCOPY: 92, A_GETCHAR: 104, A_TOKCTOR: 96, A_PERSON: 292, A_INSERT: 500}
OWN = set(SIZES)

C_DESTROY_CLEAR, C_DESTROY_TAG = 0x10210944, 0x102013C0
C_DELETE, C_NEW, C_NEWARR, C_STRLEN, C_STRCPY, C_THROW = 0x10238218, 0x102384B8, 0x10238668, 0x10238650, 0x10238410, 0x10238680
C_WCOPY, C_INDEX, C_PEEP, C_THING, C_SEED = 0x101F7424, 0x10212298, 0x10112E18, 0x1012E1F8, 0x1013B244
C_ESPR, C_ESPN, C_SPAWN, C_SCR, C_SWPRINTF = 0x1007C900, 0x1007C8B0, 0x100FAB9C, 0x100D95A0, 0x10239160

ADDR = dict(gWorld=0x1037EC0C, gPersonTable=0x102A0D54, vtbl=0x10278B74, dtortv=0x10265680)
# block offsets in the persistent arena (the host library's gArena has the same)
BLK = {"A": 0, "B": 0x400, "C": 0x800, "D": 0xC00}
for _i in range(8):
    BLK["P%d" % _i] = 0x1000 + 0x100 * _i
BSIZE = {"A": 0x400, "B": 0x400, "C": 0x400, "D": 0x400}
HOST_LAYOUT = [4, 8, 16, 8, 0x48, 0x10, 8, 16]

STATE = {"base": 0, "pool": 0}


def th(b):
    """The text hash of the host fakes."""
    h = 0
    for c in b:
        h = (h * 31 + c) & MASK
    return (h + len(b) * 1000003) & MASK


def cstr(e, a):
    b = e.cstr(a)
    return b.encode("latin1") if isinstance(b, str) else bytes(b)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def pool_next():
    i = STATE["pool"] & 7
    STATE["pool"] += 1
    return STATE["base"] + BLK["P%d" % i]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def glue(fn):
    """The import glue saves the caller's TOC pointer at 0x14(r1) and the caller reloads it after the call."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return wrapped


def s_destroy_clear(e):
    e.cl.rec("destroyClear", (e.cl.lab(e.r(3)), (e.r(4) - e.r(3)) & MASK))


def s_destroy_tag(e):
    e.cl.rec("destroyTagged", (e.cl.lab(e.r(3)), (e.r(4) - e.r(3)) & MASK))


def s_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def s_new(e):
    e.cl.rec("new", (e.r(3),))
    e.set_r(3, pool_next())


def s_newarr(e):
    e.cl.rec("newarr", (e.r(3),))
    e.set_r(3, pool_next())


def s_strlen(e):
    b = cstr(e, e.r(3))
    e.cl.rec("strlen", (th(b),))
    e.set_r(3, len(b))


def s_strcpy(e):
    dst, b = e.r(3), cstr(e, e.r(4))
    e.cl.rec("strcpy", (e.cl.lab(dst), th(b)))
    e.write(dst, b + b"\0")
    e.set_r(3, dst)


def s_throw(e):
    L = e.cl
    t, obj = cstr(e, e.r(3)), e.r(4)
    vt, p, c = e.u32(obj), e.u32(obj + 4), e.u32(obj + 8)
    txt = cstr(e, p) if p else b""
    cnt = e.u32(c) if c else 0
    L.rec("throw", (th(t), L.lab(vt), L.lab(p), L.lab(c), th(txt), cnt, L.lab(e.r(5))))


def s_wcopy(e):
    e.cl.rec("WStringCopy", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), e.r(5), e.r(6)))


def s_index(e):
    e.cl.rec("index", (e.cl.lab(e.r(3)), e.r(4)))
    e.set_r(3, (STATE["base"] + BLK["D"] + 2 * e.r(4)) & MASK)


def s_peep(e):
    e.cl.rec("CPeep", (e.cl.lab(e.r(3)), e.r(4) & 0xFFFF, e.r(5)))


def s_thing(e):
    e.cl.rec("GetThingId", (e.cl.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", e.cl.ret("thing") & 0xFFFF))


def s_seed(e):
    e.cl.rec("SeedRand", (e.cl.lab(e.r(3)), e.r(4)))


def s_espr(e):
    e.cl.rec("ESPRand", (e.r(3),))
    e.set_r(3, e.cl.ret("rand") & MASK)


def s_espn(e):
    e.cl.rec("ESPNum", (e.r(3),))
    e.set_r(3, e.cl.ret("num") & MASK)


def s_spawn(e):
    e.cl.rec("Spawn", (e.cl.lab(e.r(3)), e.r(4), e.r(5), e.r(6)))
    e.write(e.r(3), struct.pack(">I", e.cl.ret("spawn") & MASK))


def s_scr(e):
    e.cl.rec("SCRFloat", (e.r(3), e.r(4), fb(e.f(1))))


def s_swprintf(e):
    fmt = e.read(e.r(5), 24)
    chars = []
    for i in range(12):
        ch = struct.unpack(">H", fmt[2 * i:2 * i + 2])[0]
        if ch == 0:
            break
        chars.append(ch)
    chars += [0] * (12 - len(chars))
    e.cl.rec("swprintf", (e.cl.lab(e.r(3)), e.r(4)) + tuple(chars[:9]) + (e.cl.lab(e.r(6)),))


for _a, _n, _f in ((C_DESTROY_CLEAR, "dc", s_destroy_clear), (C_DESTROY_TAG, "dt", s_destroy_tag), (C_DELETE, "del", s_delete),
                   (C_NEW, "new", s_new), (C_NEWARR, "newarr", s_newarr), (C_STRLEN, "strlen", s_strlen),
                   (C_STRCPY, "strcpy", s_strcpy), (C_THROW, "throw", s_throw), (C_WCOPY, "wc", s_wcopy),
                   (C_INDEX, "idx", s_index), (C_PEEP, "peep", s_peep), (C_THING, "thing", s_thing), (C_SEED, "seed", s_seed),
                   (C_ESPR, "espr", s_espr), (C_ESPN, "espn", s_espn), (C_SPAWN, "spawn", s_spawn), (C_SCR, "scr", s_scr),
                   (C_SWPRINTF, "swp", s_swprintf)):
    STUBS.internal(_a, glue(_f) if _a >= 0x10238000 else _f)


def setup(runner):
    STATE["base"] = T.persist_alloc(runner.emu, 0x1800 + 0x100)
    STATE["base"] = (STATE["base"] + 15) & ~15


# ---------------------------------------------------------------------------------------------------------------- the two sides


def label_table(side):
    """(label, address) of every named place: the blocks, the pool nodes' string slots and the object's inner addresses."""
    P = side.psize
    t = [(1, side.g("A")), (2, side.g("B")), (3, side.g("C")), (4, side.g("D")),
         (31, side.g("A") + 0xC), (32, side.g("A") + 0x19C), (33, side.g("A") + P),
         (50, side.g("vtbl")), (51, side.g("dtortv"))]
    for i in range(8):
        t.append((20 + i, side.g("P%d" % i)))
        t.append((40 + i, side.g("P%d" % i) + 2 * P))
    return t


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def halfs(self, a, hs):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(hs), *[h & 0xFFFF for h in hs]))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def put_ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psize))[0]

    def name(self, v):
        """A pointer as a block name (and offset), so both memories compare."""
        if v == 0:
            return "null"
        for n, off in BLK.items():
            b = self.g(n)
            if b <= v < b + (BSIZE.get(n) or 0x100):
                return "%s+%d" % (n, v - b)
        return "other"

    def ref(self, n, off=0):
        return 0 if n is None else self.g(n) + off


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psize = 4
    S_STR, S_POS, S_DATA = 4, 0x24, 0xC

    def __init__(self, e, case):
        self.e = e
        STATE["pool"] = 0
        self.L = e.cl = CL.PpcLog({a: l for l, a in label_table(self)}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name in BLK:
            return STATE["base"] + BLK[name]
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psize = 8
    S_STR, S_POS, S_DATA = 8, 0x48, 0x10

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.arena = lib.tpw_pv_addr(b"arena")
        lib.tpw_pv_reset()
        CL.host_begin(lib, {l: a for l, a in label_table(self)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        if name in BLK:
            return self.arena + BLK[name]
        return self.lib.tpw_pv_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        f = {A_CLEAR: self.lib.tpw_pv_clear, A_TEAR: self.lib.tpw_pv_teardown, A_TOKCOPY: self.lib.tpw_pv_token_copy,
             A_GETCHAR: self.lib.tpw_pv_getchar, A_TOKCTOR: self.lib.tpw_pv_token_ctor, A_PERSON: self.lib.tpw_pv_person,
             A_INSERT: self.lib.tpw_pv_insert}[entry]
        r = f(*[a & 0xFFFFFFFFFFFFFFFF for a in args])
        return (r or 0) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_pv_addr.restype, lib.tpw_pv_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_pv_layout.argtypes = [C.POINTER(C.c_int64)]
    u64, u32 = C.c_uint64, C.c_uint32
    for n, ar, rt in (("clear", [u64], None), ("teardown", [u64], None), ("token_copy", [u64, u64], None),
                      ("token_ctor", [u64, u64, u32, u32, u32], None), ("getchar", [u64, u64], u32),
                      ("person", [u64, u32, u32, u64, u32], None), ("insert", [u64, u64, u64, u64], None)):
        f = getattr(lib, "tpw_pv_" + n)
        f.argtypes, f.restype = ar, rt
    out = (C.c_int64 * 32)()
    n = lib.tpw_pv_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.write(s.g("A"), b"\0" * 0x1800)
    for i, n in enumerate("ABCD"):
        s.write(s.g(n), case["fill"][0x400 * i:0x400 * (i + 1)])
    op = case["op"]
    A, B, C_, D = s.g("A"), s.g("B"), s.g("C"), s.g("D")
    if op in ("clear", "tear"):
        s.words(A, [case["cap"], case["size"]])
        s.put_ptr(A + 8, s.ref(case["data"][0], case["data"][1]))
    elif op in ("tokcopy", "tokctor"):
        s.words(A, case["mine"])
        s.words(B, case["other"])
    elif op == "getchar":
        s.halfs(D, case["chars"])
        s.put_ptr(A + s.S_STR, B)
        s.words(B, [case["len"]])
        s.words(A + s.S_POS, [case["pos"]])
        s.halfs(C_, [0xBEEF])
    elif op == "person":
        s.put_ptr(B, C_)
        s.put_ptr(C_ + s.S_DATA, D)
        s.put_ptr(s.g("gWorld"), s.g("P7"))
        s.write(s.g("gPersonTable"), struct.pack(s.fmt[0] + "2H", *case["table"]))
    elif op == "insert":
        P = s.psize
        s.words(A, [case["size"]])
        if case["mode"] == "empty":
            s.put_ptr(B, B)
            s.put_ptr(B + P, B)
        else:
            s.put_ptr(B, C_)
            s.put_ptr(B + P, C_)
            s.put_ptr(C_, B)
            s.put_ptr(C_ + P, B)


def execute(s, case):
    op = case["op"]
    A, B, C_, D = s.g("A"), s.g("B"), s.g("C"), s.g("D")
    if op == "clear":
        return s.call(A_CLEAR, [A])
    if op == "tear":
        return s.call(A_TEAR, [A])
    if op == "tokcopy":
        return s.call(A_TOKCOPY, [A, B])
    if op == "tokctor":
        return s.call(A_TOKCTOR, [A, B] + case["w"])
    if op == "getchar":
        return s.call(A_GETCHAR, [A, C_])
    if op == "person":
        return s.call(A_PERSON, [A, case["cell"], case["avatar"], B, case["flag"]])
    if op == "insert":
        return s.call(A_INSERT, [A + 0x20, A, B, D])


PERSON_TYPED = [(0xC, "w"), (0x20, "w"), (0x24, "w"), (0x188, "w"), (0x3A4, "w"), (0x3A0, "b"), (0xC2, "h")]


def observe(s, case, answer):
    op = case["op"]
    A, B, C_ = s.g("A"), s.g("B"), s.g("C")
    o = {"log": s.log()}
    if op in ("clear", "tear"):
        o["vec"] = s.rd_words(A, 2) + [s.name(s.rd_ptr(A + 8))]
    elif op in ("tokcopy", "tokctor"):
        o["a"] = s.rd_words(A, 4)
        o["b"] = s.rd_words(B, 4)
    elif op == "getchar":
        o["answer"] = answer & 0xFF
        o["pos"] = s.rd_word(A + s.S_POS)
        o["ch"] = s.rd_half(C_)
        o["str"] = s.name(s.rd_ptr(A + s.S_STR))
        o["len"] = s.rd_word(B)
    elif op == "person":
        typed = {}
        for off, k in PERSON_TYPED:
            if k == "w":
                typed[off] = s.rd_word(A + off)
                s.words(A + off, [0])
            elif k == "h":
                typed[off] = s.rd_half(A + off)
                s.halfs(A + off, [0])
            else:
                typed[off] = s.read(A + off, 1)[0]
                s.write(A + off, b"\0")
        o["typed"] = typed
        o["bytes"] = s.read(A, 0x3A8)
        o["bname"] = s.rd_word(B) if False else s.name(s.rd_ptr(B))
    elif op == "insert":
        P = s.psize
        o["size"] = s.rd_word(A)
        o["res"] = s.name(s.rd_ptr(A + 0x20))
        o["b"] = [s.name(s.rd_ptr(B)), s.name(s.rd_ptr(B + P))]
        o["c"] = [s.name(s.rd_ptr(C_)), s.name(s.rd_ptr(C_ + P))]
        o["pool"] = [[s.name(s.rd_ptr(s.g("P%d" % i))), s.name(s.rd_ptr(s.g("P%d" % i) + P))] for i in range(4)]
    return o


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


def base(r, op, **kw):
    c = {"op": op, "fill": bytes(r.getrandbits(8) for _ in range(0x1000)),
         "queues": {"thing": [r.randrange(0x10000)], "rand": [r.randrange(1 << 32)], "num": [r.choice([0, 1, 2, 5, 40, 1000])],
                    "spawn": [r.randrange(1 << 32)]}}
    c.update(kw)
    return c


def datas(r):
    return r.choice([("B", 0), ("B", 0), ("null", 0) if False else (None, 0), ("B", 0x10)])


def vec_case(op, r, size=None, data=-1):
    if size is None:
        size = r.choice([0, 1, 2, 3, 7, 20, r.randrange(0, 30)])
    d = datas(r) if data == -1 else data
    return base(r, op, cap=r.choice([0, size, size + 4, r.randrange(1 << 32)]), size=size, data=d)


def vec_edges(op):
    r = random.Random(11 if op == "clear" else 12)
    for size, data in ((0, ("B", 0)), (0, (None, 0)), (1, ("B", 0)), (3, (None, 0)), (5, ("B", 0x10)), (20, ("B", 0)),
                       (0x40000001, ("B", 0)), (0xFFFFFFFF, ("B", 0)), (0x80000000, (None, 0)), (0x15555556, ("B", 0))):
        yield vec_case(op, r, size, data)


def vec_random(op):
    return lambda r: vec_case(op, r)


def tok_case(op, r):
    c = base(r, op, mine=[r.getrandbits(32) for _ in range(4)], other=[r.getrandbits(32) for _ in range(4)], w=[r.getrandbits(32) for _ in range(3)])
    return c


def tok_edges(op):
    r = random.Random(21)
    for w in ([0, 0, 0], [MASK, MASK, MASK], [1, 2, 3], [0x80000000, 0, 0x7FFFFFFF]):
        c = tok_case(op, r)
        c["w"] = w
        c["other"] = [r.getrandbits(32), w[0], w[1], w[2]]
        yield c


def getchar_case(r, length=None, pos=None):
    if length is None:
        length = r.choice([0, 1, 2, 5, 8, 0x80000000, MASK])
    if pos is None:
        pos = r.choice([0, 1, 3, 7, length, length - 1, length + 1, MASK, 0x80000000, r.randrange(0, 10)]) & MASK
    if length > 8 and pos < length and pos > 8:
        pos = r.randrange(0, 8)
    return base(r, "getchar", len=length, pos=pos, chars=[r.randrange(0x10000) for _ in range(10)])


def getchar_edges():
    r = random.Random(31)
    for length, pos in ((0, 0), (1, 0), (1, 1), (5, 4), (5, 5), (5, 6), (8, 7), (0x80000000, 5), (0x80000000, 0x90000000),
                        (5, MASK), (MASK, 3), (MASK, MASK), (3, 0x80000000), (0, MASK)):
        yield getchar_case(r, length, pos)


def person_case(r, num=None, avatar=None):
    c = base(r, "person", cell=r.randrange(0x10000), flag=r.choice([0, 1]), table=[r.randrange(0x10000), r.randrange(0x10000)])
    if num is not None:
        c["queues"]["num"] = [num]
    n = c["queues"]["num"][0]
    c["avatar"] = avatar if avatar is not None else r.choice([0, 1, n - 2, n - 1, n, n + 1, MASK, 0x80000000, r.randrange(0, 50)]) & MASK
    return c


def person_edges():
    r = random.Random(41)
    for num, avatar in ((0, 0), (0, 5), (0, MASK), (1, 0), (1, 1), (5, 3), (5, 4), (5, 5), (5, 6), (5, MASK), (40, 39), (40, 40),
                        (40, 41), (0x80000000, 5), (0x80000000, 0x7FFFFFFF), (0x80000001, 0x80000000), (MASK, MASK), (MASK, 0)):
        c = person_case(r, num, avatar)
        yield c
    for flag in (0, 1):
        c = person_case(r, 5, 2)
        c["flag"] = flag
        yield c


def insert_case(r, size=None, mode=None):
    if size is None:
        size = r.choice([0, 1, 5, 0x7FFFFFFD, 0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, MASK, r.randrange(0, 100)])
    return base(r, "insert", size=size, mode=mode or r.choice(["empty", "one"]))


def insert_edges():
    r = random.Random(51)
    for size in (0, 1, 0x7FFFFFFD, 0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, MASK):
        for mode in ("empty", "one"):
            yield insert_case(r, size, mode)


TESTS = [
    FnTest("std::vector<Argument,std::allocator<Argument>>::clear", ppc, host, edge=lambda: vec_edges("clear"), rnd=vec_random("clear")),
    FnTest("std::vector<Argument,std::allocator<Argument>>::tear_down", ppc, host, edge=lambda: vec_edges("tear"), rnd=vec_random("tear")),
    FnTest("CToken::CToken@0x10210d4c", ppc, host, edge=lambda: tok_edges("tokcopy"), rnd=lambda r: tok_case("tokcopy", r)),
    FnTest("CParser::GetChar", ppc, host, edge=getchar_edges, rnd=getchar_case),
    FnTest("CToken::CToken@0x10211f88", ppc, host, edge=lambda: tok_edges("tokctor"), rnd=lambda r: tok_case("tokctor", r)),
    FnTest("COnlinePerson::COnlinePerson", ppc, host, edge=person_edges, rnd=person_case),
    FnTest("std::list<std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>,std::allocator<std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>>>::insert",
           ppc, host, edge=insert_edges, rnd=insert_case),
]
