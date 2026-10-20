"""Unit v51126, std tree constructors and insert_one, destroyers, deque tear_down and four UI forwarders, against
libtpw_unit_std___tree_std_pair_std_map_nocase_string_BuddyI.dylib (tests/unit_std___tree_std_pair_std_map_nocase_string_BuddyI.cpp), main app
("SimTheme Park"), src/std___tree_std_pair_std_map_nocase_string_BuddyI.cpp:
  the three tree constructors 0x10222fe0 0x10223c74 0x10224540, the two insert_one 0x10223a24 0x102243c0, the node deallocates 0x102229f8
  0x10223670, std::__destroy<Argument> 0x102224e8, <Argument*> 0x10222458, <vector> 0x102254e4, <vector*> 0x102253cc, the nocase wide string
  destructor 0x10222020, the deque tear_down 0x1022462c, the cdeque destructor 0x10224ea4, getloc 0x10220ab4, make_iterator 0x101f56f8,
  UIInterface::InitOnline 0x101ef7f8, FeatureStatusPanel::Remove 0x1019ee48, DeInitWindow 0x1019e91c, DeInitStatusWindow 0x1019e6e4.

A case is a world and a call. The world is one block of 0x800 bytes allocated on each side (objects at fixed offsets, pointers written as
block offsets); every aligned address of the block is a label of the call log, so a pointer a fake sees is its offset. The callees outside
the unit (operator delete, the string destructors, the string compares, insert_node_at, the vector tear_down, the deque make_iterator and
destroy, the cdeque iterator operations, InitOnlineUI, CloseWindow) are fakes on both sides that record their arguments and answer from the
case's queues. What is compared: the answer (pointers as block offsets), the whole block (words numeric, pointers as offsets, byte fields as
bytes), the seven globals the constructors and insert_one read, and the event log.

No fused operations. Not tested: a deque make_iterator with a block size of 0 (the original divides by 0: the emulator answers 0, the host
cannot), a NULL representation in the nocase string destructor (the original reads through it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___tree_std_pair_std_map_nocase_string_BuddyI"
MASK = 0xFFFFFFFF
SIZE = 0x800
PFX = "tpw_v51126_"

SIZES = {0x10222fe0: 44, 0x102229f8: 40, 0x10223670: 40, 0x10223a24: 236, 0x102224e8: 68, 0x10222458: 76, 0x10223c74: 44,
         0x10222020: 188, 0x102243c0: 236, 0x10224540: 44, 0x1022462c: 432, 0x10224ea4: 104, 0x102253cc: 76, 0x102254e4: 40,
         0x10220ab4: 44, 0x101f56f8: 240, 0x101ef7f8: 36, 0x1019ee48: 36, 0x1019e91c: 4, 0x1019e6e4: 4}
OWN = set(SIZES)

# the Mac's addresses of the globals; (name, size)
ADDR = dict(NocaseCount=0x102b1f9c, NocaseIns=0x102b1fa0, NocaseNot=0x102b1fa1, WCount=0x102b1fa4, WIns=0x102b1fa8, WNot=0x102b1fa9,
            BuddyCount=0x102b1fac)
GLOBS = [("NocaseCount", 4), ("NocaseIns", 1), ("NocaseNot", 1), ("WCount", 4), ("WIns", 1), ("WNot", 1), ("BuddyCount", 4)]
HOST_LAYOUT = [12, 16, 0xc, 8, 16, 12]


def s32(v):
    return T.s32(v)


def P(off):
    return ("P", off)


def pack_words(ws):
    return b"".join(struct.pack(">I", w & MASK) for w in ws)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(0x10238218, CL.recorder("OperatorDelete", "L"))
STUBS.internal(0x101468f8, CL.recorder("WStringDtor", "Li"))
STUBS.internal(0x1000221c, CL.recorder("StringDtor", "Li"))
STUBS.internal(0x10201310, CL.recorder("ArgumentVectorTearDown", "L"))
STUBS.internal(0x10225d2c, CL.recorder("NocaseLess", "LL", "b"))
STUBS.internal(0x10225e8c, CL.recorder("WStringLess", "LL", "b"))
STUBS.internal(0x10228458, CL.recorder("NocaseInsertNodeAt", "LLiiL", "i"))
STUBS.internal(0x10227c78, CL.recorder("WStringInsertNodeAt", "LLiiL", "i"))
STUBS.internal(0x101864b0, CL.recorder("InitOnlineUI", ""))
STUBS.internal(0x10193774, CL.recorder("CloseWindow", "L"))


def iter_words(idx):
    idx &= MASK
    return [(idx * 3 + 1) & MASK, idx ^ 0x55, (idx + 7) & MASK, (0x1000 + idx) & MASK, (0x2000 + idx) & MASK, idx]


def stub_make_iterator(e):
    L = e.cl
    out, this, idx = e.r(3), e.r(4), e.r(5) & MASK
    L.rec("MakeIterator", (L.lab(out), L.lab(this), idx))
    e.write(out, pack_words(iter_words(idx)))


def stub_destroy(e):
    L = e.cl
    sp = e.r(1)
    first = [e.r(i) & MASK for i in range(4, 10)]
    last = [e.r(10) & MASK] + [e.u32(sp + 0x38 + 4 * k) for k in range(5)]
    L.rec("DestroyFirst", (L.lab(e.r(3)),) + tuple(first))
    L.rec("DestroyLast", tuple(last))


def stub_decrement(e):
    L = e.cl
    c = e.r(3)
    w0, w1, w2 = e.u32(c), e.u32(c + 4), e.u32(c + 8)
    L.rec("CdequeDecrement", (L.lab(w0), w1, L.lab(w2)))
    e.write(c + 4, pack_words([w1 - 1]))


def stub_cursor_add(e):
    L = e.cl
    c, n = e.r(3), s32(e.r(4))
    w0, w1, w2 = e.u32(c), e.u32(c + 4), e.u32(c + 8)
    L.rec("CursorAdd", (L.lab(w0), w1, L.lab(w2), n))
    e.write(c + 4, pack_words([w1 + n, w2 + 4 * n]))


STUBS.internal(0x10228e2c, stub_make_iterator)
STUBS.internal(0x10228fd4, stub_destroy)
STUBS.internal(0x101f4c84, stub_decrement)
STUBS.internal(0x101f2bec, stub_cursor_add)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def resolve(self, v):
        """A word stored in the world (a Mac word: an arena offset on the host)."""
        return (self.wbase + v[1]) & MASK if isinstance(v, tuple) else v & MASK

    def resolve_arg(self, v):
        """An argument of the call (a real pointer)."""
        return self.base + v[1] if isinstance(v, tuple) else v & MASK

    def norm(self, w):
        w &= MASK
        if self.wbase <= w <= self.wbase + SIZE:
            return "P%x" % (w - self.wbase)
        return w


# op: (ppc address, host function, leading host args, returns a pointer)
OPS = {
    "ctor_nocase": (0x10224540, "ctor", (0,), False),
    "ctor_wstring": (0x10223c74, "ctor", (1,), False),
    "ctor_buddy": (0x10222fe0, "ctor", (2,), False),
    "insert_nocase": (0x102243c0, "insert_one", (0,), False),
    "insert_wstring": (0x10223a24, "insert_one", (1,), False),
    "dealloc_buddy": (0x102229f8, "dealloc", (0,), False),
    "dealloc_wstring": (0x10223670, "dealloc", (1,), False),
    "nocase_dtor": (0x10222020, "nocase_dtor", (), True),
    "arg_destroy": (0x102224e8, "arg_destroy", (), False),
    "arg_range": (0x10222458, "arg_range", (), False),
    "vec_destroy": (0x102254e4, "vec_destroy", (), False),
    "vec_range": (0x102253cc, "vec_range", (), False),
    "cdeque_dtor": (0x10224ea4, "cdeque_dtor", (), True),
    "tear_down": (0x1022462c, "tear_down", (), False),
    "getloc": (0x10220ab4, "getloc", (), False),
    "make_iterator": (0x101f56f8, "make_iterator", (), False),
    "init_online": (0x101ef7f8, "init_online", (), False),
    "remove": (0x1019ee48, "remove", (), False),
    "deinit_window": (0x1019e91c, "deinit_window", (), False),
    "deinit_status": (0x1019e6e4, "deinit_status", (), False),
}


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.base = self.wbase = e.alloc(SIZE)
        fixed = {self.base + 4 * k: k + 1 for k in range(SIZE // 4)}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, args):
        r3, _, _ = T.call_ppc(self.e, OPS[op][0], list(args))
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_v51126_alloc(SIZE)
        self.wbase = self.base - lib.tpw_v51126_arena()
        assert 0 < self.wbase and self.wbase + SIZE < (1 << 32), hex(self.wbase)
        CL.host_begin(lib, {k + 1: self.base + 4 * k for k in range(SIZE // 4)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v51126_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        _, name, lead, ret = OPS[op]
        r = getattr(self.lib, PFX + name)(*lead, *args)
        return (r or 0) & MASK if ret else 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, u32, i32 = C.c_uint64, C.c_uint32, C.c_int32
    f = lambda n: getattr(lib, PFX + n)
    f("addr").restype, f("addr").argtypes = u64, [C.c_char_p]
    f("arena").restype, f("arena").argtypes = u64, []
    f("alloc").restype, f("alloc").argtypes = u64, [u32]
    f("layout").argtypes = [C.POINTER(C.c_int64)]
    sig = {"ctor": [i32, u64, u64], "insert_one": [i32, u64, u64, u64], "dealloc": [i32, u64, u64, u32],
           "nocase_dtor": [u64, i32], "arg_destroy": [u64], "arg_range": [u64, u64], "vec_destroy": [u64], "vec_range": [u64, u64],
           "cdeque_dtor": [u64, i32], "tear_down": [u64], "getloc": [u64, u64], "make_iterator": [u64, u64, u32],
           "init_online": [u64], "remove": [u64], "deinit_window": [u64], "deinit_status": [u64]}
    for n, a in sig.items():
        f(n).argtypes = a
        f(n).restype = u64
    out = (C.c_int64 * 32)()
    n = f("layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.words(s.base + 0x700, [0xDEAD0000 + i for i in range(64)])
    for off, v in sorted(case["words"].items()):
        s.words(s.base + off, [s.resolve(v)])
    for off, data in sorted(case["bytes"].items()):
        s.write(s.base + off, data)
    for name, size in GLOBS:
        v = case["glob"][name]
        s.write(s.g(name), struct.pack(">I" if s.fmt == ">I" else "<I", v)[:4] if size == 4 else bytes([v & 0xFF]))


def execute(s, case):
    return s.call(case["op"], [s.resolve_arg(a) for a in case["args"]])


def observe(s, case, answer):
    raw = s.read(s.base, SIZE)
    bz = set(case["bz"])
    mem = []
    for off in range(0, SIZE, 4):
        chunk = raw[off:off + 4]
        mem.append(list(chunk) if off in bz else s.norm(struct.unpack(s.fmt, chunk)[0]))
    glob = []
    for name, size in GLOBS:
        data = s.read(s.g(name), size)
        glob.append(data[0] if size == 1 else struct.unpack(s.fmt, data)[0])
    return {"answer": s.norm(answer) if OPS[case["op"]][3] else 0, "log": s.log(), "mem": mem, "glob": glob}


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


def rglob(r, edge=False):
    if edge:
        return {"NocaseCount": 0, "NocaseIns": 1, "NocaseNot": 0, "WCount": 0, "WIns": 1, "WNot": 0, "BuddyCount": 0}
    return {"NocaseCount": r.choice([0, 0x1234, 0xFFFFFFFF, r.randrange(1, 0x1000)]), "NocaseIns": r.choice([0x11, 0x31, 1]),
            "NocaseNot": r.choice([0x22, 0x23, 0]), "WCount": r.choice([0, 0x4321, 0xFFFFFFFF, r.randrange(1, 0x1000)]),
            "WIns": r.choice([0x51, 0x71, 1]), "WNot": r.choice([0x44, 0x45, 0]),
            "BuddyCount": r.choice([0, 0x5678, 0xFFFFFFFF, r.randrange(1, 0x1000)])}


def mk(op, r, args, words=None, bytes_=None, queues=None, bz=(), edge=False):
    return {"op": op, "args": args, "words": words or {}, "bytes": bytes_ or {}, "queues": queues or {}, "bz": list(bz),
            "glob": rglob(r, edge)}


# ---- constructors
def ctor_case(op, r, count_edge=None, byte=None):
    c = mk(op, r, [P(0), P(0x40)], words={0: 0x1111, 4: 0x2222, 8: 0x3333, 0xC: 0x4444},
           bytes_={0x40: bytes([r.randrange(256 if byte is None else 1) if byte is None else byte, 1, 2, 3])}, bz=[8, 0x40] if op == "ctor_buddy" else [0x40])
    if op == "ctor_buddy":
        c["bytes"][8] = bytes([0xAA, 0xBB, 0xCC, 0xDD])
        c["words"].pop(8)
    return c


def ctor_edges(op):
    def gen():
        r = random.Random(11)
        for b in (0, 1, 0x7F, 0x80, 0xFF):
            yield ctor_case(op, r, byte=b)
        c = ctor_case(op, r, byte=5)
        c["glob"] = rglob(r, True)
        yield c
    return gen


# ---- deallocate
def dealloc_case(op, r, node=None, count=None):
    node = r.choice([P(0x40), P(0x100), 0, P(0x7f0)]) if node is None else node
    return mk(op, r, [P(0), node, r.choice([0, 1, 5, 0xFFFFFFFF]) if count is None else count])


def dealloc_edges(op):
    def gen():
        r = random.Random(12)
        for node in (P(0x40), 0, P(0x100)):
            for count in (0, 1, 7):
                yield dealloc_case(op, r, node, count)
    return gen


# ---- insert_one
def tree_world(r, n):
    words = {}
    free = []
    for i in range(n):
        off = 0x100 + 0x10 * i
        words[off + 0xC] = P(0x300 + 0x10 * i)
        if i == 0:
            words[4] = P(off)
        else:
            pi, side = free.pop(r.randrange(len(free)))
            words[0x100 + 0x10 * pi + 4 * side] = P(off)
            words[off + 8] = P(0x100 + 0x10 * pi)
        free += [(i, 0), (i, 1)]
    words[0] = n
    words[8] = P(0x100) if n else P(4)
    return words


def insert_case(op, r, n=None, lt=None):
    n = r.randrange(0, 8) if n is None else n
    lt = [r.choice([0, 1]) for _ in range(r.randrange(1, 9))] if lt is None else lt
    pre = "Nocase" if op == "insert_nocase" else "WString"
    queues = {pre + "Less": lt, pre + "InsertNodeAt": [r.choice([0x123, 0x2468, 0x7fff])]}
    return mk(op, r, [P(0x20), P(0), P(0x200)], words=tree_world(r, n), bytes_={0x24: bytes([0xAB, 0xCD, 0xEF, 0x01])}, queues=queues,
              bz=[0x24])


def insert_edges(op):
    def gen():
        r = random.Random(13)
        for n in (0, 1, 2, 3, 5):
            for lt in ([0], [1], [1, 1, 1, 1, 1], [0, 0, 0, 0, 0], [1, 0, 1, 0], [0, 1, 0, 1], [1, 0, 0, 0], [0, 1, 1, 1], [1, 1, 0, 0]):
                yield insert_case(op, r, n, lt)
    return gen


# ---- the Argument destroyers
def arg_case(op, r, n=None, odd=None):
    if op in ("arg_destroy", "vec_destroy"):
        return mk(op, r, [r.choice([P(0x40), P(0x44), 0])])
    n = r.randrange(0, 6) if n is None else n
    last = 0x40 + 12 * n + (r.choice([0, 0, 1, 5, 11, -1, -13]) if odd is None else odd)
    return mk(op, r, [P(0x40), P(max(last, 0))])


def arg_edges(op):
    def gen():
        r = random.Random(14)
        if op in ("arg_destroy", "vec_destroy"):
            for a in (P(0x40), 0, P(0x44)):
                yield mk(op, r, [a])
        else:
            for n in (0, 1, 2, 3):
                for odd in (0, 1, 11, -1, -12, -13):
                    yield arg_case(op, r, n, odd)
    return gen


# ---- the nocase string destructor
def dtor_case(op, r, refs=None, flag=None, null=None):
    refs = r.choice([0, 1, 1, 2, 3, 0xFFFFFFFF, 9]) if refs is None else refs
    flag = r.choice([0xFFFFFFFF, 0, 1, 0x7FFF, 0x8000, 0x10001, 0xFFFF, 0x20000, 5]) if flag is None else flag
    null = (r.random() < 0.1) if null is None else null
    return mk(op, r, [0 if null else P(0), flag], words={0: P(0x40), 0x48: refs, 0x44: 7, 0x40: 3, 0x4C: r.choice([P(0x80), P(0x84), 0])})


def dtor_edges(op):
    def gen():
        r = random.Random(15)
        for refs in (0, 1, 2, 0xFFFFFFFF):
            for flag in (0xFFFFFFFF, 0, 1, 0x8000, 0x10001, 0x7FFF):
                yield dtor_case(op, r, refs, flag, False)
        yield dtor_case(op, r, 1, 1, True)
    return gen


def cdeque_case(op, r, table=None, flag=None, null=None):
    table = r.choice([P(0x80), 0, P(0x100)]) if table is None else table
    flag = r.choice([0xFFFFFFFF, 0, 1, 0x7FFF, 0x8000, 0x10001, 0xFFFF, 5]) if flag is None else flag
    null = (r.random() < 0.1) if null is None else null
    return mk(op, r, [0 if null else P(0), flag], words={0: 4, 4: 3, 8: 1, 0xC: table})


def cdeque_edges(op):
    def gen():
        r = random.Random(16)
        for table in (P(0x80), 0):
            for flag in (0xFFFFFFFF, 0, 1, 0x8000, 0x10001):
                yield cdeque_case(op, r, table, flag, False)
        yield cdeque_case(op, r, P(0x80), 1, True)
    return gen


# ---- tear_down
def td_case(op, r, cap=None, count=None, start=None, first=None, size=None):
    cap = r.randrange(1, 9) if cap is None else cap
    count = r.randrange(0, cap + 2) if count is None else count
    start = r.randrange(0, cap) if start is None else start
    first = r.choice([0, 3, 0x2a, r.randrange(0, 0x100), 0xFFFFFFFF]) if first is None else first
    size = r.choice([0, 1, 0x2a, r.randrange(0, 0x100)]) if size is None else size
    words = {0: 0x2a, 4: cap, 8: count, 0xC: start, 0x10: P(0x40), 0x14: first, 0x18: size}
    for i in range(cap):
        words[0x40 + 4 * i] = P(0x100 + 0x10 * i)
    return mk(op, r, [P(0)], words=words)


def td_edges(op):
    def gen():
        r = random.Random(17)
        for cap, count, start in ((4, 0, 0), (4, 1, 0), (4, 2, 3), (4, 4, 0), (4, 4, 3), (4, 3, 1), (4, 3, 2), (1, 1, 0), (1, 2, 0),
                                  (3, 5, 2), (8, 8, 7), (5, 2, 4)):
            yield td_case(op, r, cap, count, start)
        yield td_case(op, r, 4, 3, 1, 0xFFFFFFFF, 5)
        yield td_case(op, r, 4, 3, 1, 0, 0)
    return gen


# ---- getloc
def getloc_case(op, r, counter=None, ref=None):
    counter = r.choice([0, 1, 5, 0xFFFFFFFF, 0x7FFFFFFF]) if counter is None else counter
    ref = r.choice([P(0x80), 0]) if ref is None else ref
    return mk(op, r, [P(0), P(0x40)], words={0x40 + 0x1C: r.choice([0x1234, 0, P(0x90)]), 0x40 + 0x20: ref, 0x80: counter, 0: 0x7777, 4: 0x8888})


def getloc_edges(op):
    def gen():
        r = random.Random(18)
        for ref in (P(0x80), 0, P(0x84)):
            for counter in (0, 1, 0xFFFFFFFF):
                yield getloc_case(op, r, counter, ref)
    return gen


# ---- make_iterator
def mi_case(op, r, bs=None, index=None, count=None, start=None):
    bs = r.randrange(1, 17) if bs is None else bs
    index = r.randrange(0, bs * 5 + 3) if index is None else index
    count = r.choice([0, 1, 3, 6]) if count is None else count
    start = r.randrange(0, 5) if start is None else start
    cap = 8
    words = {0: bs, 4: cap, 8: count, 0xC: start, 0x10: P(0x40), 0x14: r.randrange(0, 50), 0x18: r.randrange(0, 50)}
    for i in range(16):
        words[0x40 + 4 * i] = P(0x200 + 0x40 * i)
    return mk(op, r, [P(0x90), P(0), index], words=words)


def mi_edges(op):
    def gen():
        r = random.Random(19)
        for bs in (1, 4, 16):
            for index in (0, bs - 1, bs, bs + 1, 2 * bs, 3 * bs + 1):
                for count in (0, 2):
                    yield mi_case(op, r, bs, index, count, 1)
        yield mi_case(op, r, 4, 5, 2, 4)
    return gen


# ---- the UI forwarders
def ui_case(op, r):
    words = {0x40 + 4 * i: r.randrange(0, 0x100) for i in range(4)}
    return mk(op, r, [r.choice([P(0x40), P(0x44), 0])], words=words)


def ui_edges(op):
    def gen():
        r = random.Random(20)
        for a in (P(0x40), P(0x50), 0):
            c = ui_case(op, r)
            c["args"] = [a]
            yield c
    return gen


FAMILIES = [
    ("ctor_buddy", ctor_case, ctor_edges), ("ctor_nocase", ctor_case, ctor_edges), ("ctor_wstring", ctor_case, ctor_edges),
    ("dealloc_buddy", dealloc_case, dealloc_edges), ("dealloc_wstring", dealloc_case, dealloc_edges),
    ("insert_nocase", insert_case, insert_edges), ("insert_wstring", insert_case, insert_edges),
    ("arg_destroy", arg_case, arg_edges), ("arg_range", arg_case, arg_edges), ("vec_destroy", arg_case, arg_edges),
    ("vec_range", arg_case, arg_edges), ("nocase_dtor", dtor_case, dtor_edges), ("cdeque_dtor", cdeque_case, cdeque_edges),
    ("tear_down", td_case, td_edges), ("getloc", getloc_case, getloc_edges), ("make_iterator", mi_case, mi_edges),
    ("init_online", ui_case, ui_edges), ("remove", ui_case, ui_edges), ("deinit_window", ui_case, ui_edges),
    ("deinit_status", ui_case, ui_edges),
]

W = "std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>"
WT = "std::__tree<%s,std::less<%s>,std::allocator<%s>>" % (W, W, W)
NT = "std::__tree<nocase_string,std::less<nocase_string>,std::allocator<nocase_string>>"
BT = ("std::__tree<std::pair<const_nocase_string,BuddyInfo>,std::map<nocase_string,BuddyInfo,std::less<nocase_string>,"
      "std::allocator<std::pair<const_nocase_string,BuddyInfo>>>::value_compare,std::allocator<std::pair<const_nocase_string,BuddyInfo>>>")
V = "std::vector<Argument,std::allocator<Argument>>"
NAMES = {
    "ctor_buddy": BT + "::__tree",
    "dealloc_buddy": "std::allocator<%s::node>::deallocate" % BT,
    "dealloc_wstring": "std::allocator<%s::node>::deallocate" % WT,
    "insert_wstring": WT + "::insert_one",
    "arg_destroy": "std::__destroy<Argument>",
    "arg_range": "std::__destroy<Argument*>",
    "ctor_wstring": WT + "::__tree",
    "nocase_dtor": "std::basic_string<wchar_t,nocase_wchar_traits,std::allocator<wchar_t>>::~basic_string",
    "insert_nocase": NT + "::insert_one",
    "ctor_nocase": NT + "::__tree",
    "tear_down": "std::deque<%s,std::allocator<%s>>::tear_down" % (V, V),
    "cdeque_dtor": "std::__cdeque<%s*,std::allocator<%s*>>::~__cdeque" % (V, V),
    "vec_range": "std::__destroy<%s*>" % V,
    "vec_destroy": "std::__destroy<%s>" % V,
    "getloc": "std::basic_streambuf<wchar_t,std::char_traits<wchar_t>>::getloc",
    "make_iterator": "std::deque<errorthingy,std::allocator<errorthingy>>::make_iterator",
    "init_online": "UIInterface::InitOnline",
    "remove": "FeatureStatusPanel::Remove",
    "deinit_window": "FeatureStatusPanel::DeInitWindow",
    "deinit_status": "FeatureStatusPanel::DeInitStatusWindow",
}


def make_test(op, case_fn, edge_fn):
    return FnTest(NAMES[op], ppc, host, edge=edge_fn(op), rnd=lambda r: case_fn(op, r))


TESTS = [make_test(op, cf, ef) for op, cf, ef in FAMILIES]
