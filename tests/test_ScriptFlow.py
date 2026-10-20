"""Unit w164, the script interpreter's flow and arithmetic, against libtpw_unit_ScriptFlow.dylib (tests/unit_ScriptFlow.cpp), main app
("SimTheme Park"), src/ScriptFlow.cpp:
  SCR_FindNextEndif 0x100d89f4, SCR_FindNextElseOrEndif 0x100d8bcc, PK_IF 0x100da9d8, PK_ENDIF 0x100dadc8, PK_ELSE 0x100dae38,
  PK_REPEAT 0x100daff8, PK_UNTIL 0x100db060, PK_DO 0x100db588, PK_LOOP 0x100db620, PK_ADD 0x100db864, PK_SUB 0x100db93c, PK_MUL 0x100dba14,
  PK_MULVAR 0x100dbaec, PK_DIV 0x100dbba0, PK_DIVVAR 0x100dbc80, PK_MOD 0x100dbd38, PK_AND 0x100dbdc0, PK_MODVAR 0x100dbe5c,
  PK_ADDVAR 0x100dbeec, PK_COPYVAR 0x100dbf9c, PK_RANDOM 0x100dc01c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides: SCR_Error (records the message), SCR_Push (the value), SCR_Pop (answers from the queue "pop") and
CWorld::GetRand (records the world's label, answers from the queue "GetRand").

The world of a case: a script block of 70 random words with mPos, mCodeBase, the two depths and the 19 variables set; the code array (883
words at 0x1029f20c, zero but for the command's operands and a token stream of IF, ENDIF, ELSE and filler words after them, placed in the
middle or at the very end of the array); the globals gScriptCurrent, gScriptCodeBase (usually mCodeBase) and gScriptVars. What is compared:
the whole block, gScriptCodeBase and the event log. A float result is compared with its NaN made one NaN.

No fused operations (the commands use fadds, fsubs, fmuls and fdivs only). Not tested: a negative position or one past the code array (the
Mac reads the data after the array, the host answers 0), a variable index above 0x12 (it leaves the block).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ScriptFlow"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
FN = {
    "SCR_FindNextEndif": (0x100D89F4, 432), "SCR_FindNextElseOrEndif": (0x100D8BCC, 224), "PK_IF": (0x100DA9D8, 980),
    "PK_ENDIF": (0x100DADC8, 80), "PK_ELSE": (0x100DAE38, 72), "PK_REPEAT": (0x100DAFF8, 72), "PK_UNTIL": (0x100DB060, 1036),
    "PK_DO": (0x100DB588, 124), "PK_LOOP": (0x100DB620, 176), "PK_ADD": (0x100DB864, 184), "PK_SUB": (0x100DB93C, 184),
    "PK_MUL": (0x100DBA14, 184), "PK_MULVAR": (0x100DBAEC, 148), "PK_DIV": (0x100DBBA0, 192), "PK_DIVVAR": (0x100DBC80, 152),
    "PK_MOD": (0x100DBD38, 104), "PK_AND": (0x100DBDC0, 124), "PK_MODVAR": (0x100DBE5C, 112), "PK_ADDVAR": (0x100DBEEC, 144),
    "PK_COPYVAR": (0x100DBF9C, 92), "PK_RANDOM": (0x100DC01C, 164),
}
ORDER = ["SCR_FindNextEndif", "SCR_FindNextElseOrEndif", "PK_IF", "PK_ENDIF", "PK_ELSE", "PK_REPEAT", "PK_UNTIL", "PK_DO", "PK_LOOP",
         "PK_ADD", "PK_SUB", "PK_MUL", "PK_MULVAR", "PK_DIV", "PK_DIVVAR", "PK_MOD", "PK_AND", "PK_MODVAR", "PK_ADDVAR", "PK_COPYVAR",
         "PK_RANDOM"]
OWN = {a for a, _ in FN.values()}

# ---- the callees faked on both sides
SCR_ERROR, SCR_PUSH, SCR_POP, GET_RAND = 0x100D8740, 0x100D8CDC, 0x100D8D34, 0x1013B1E4

CODE, CODE_WORDS = 0x1029F20C, 883
CODE_END = 0x1029FFD8
D_IF, D_ENDIF, D_ELSE = 0x10266960, 0x10266968, 0x10266970
# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_sk_addr(name)
ADDR = dict(gScriptCurrent=0x102F7C24, gScriptCodeBase=0x102F7C20, gScriptVars=0x102F7C1C, gGameWorld=0x1037EBFC, code=CODE)
# the host's sizes and offsets the module relies on, checked once against tpw_sk_layout (catches a header that moved)
HOST_LAYOUT = [0x118, 8, 0x14, 0x70, 0x78, 0x84, CODE_END]
BLOCK_WORDS, VAR_OFFSET, WORLD_LABEL = 70, 0x84, 7


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    """A float's bits with every NaN made one (the processors make different NaNs)."""
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_error(e):
    e.cl.rec("SCR_Error", (), (e.cstr(e.r(3))[:159],))


def stub_push(e):
    e.cl.rec("SCR_Push", (s32(e.r(3)),))


def stub_pop(e):
    e.cl.rec("SCR_Pop")
    e.set_r(3, e.cl.ret("pop", 0) & MASK)


def stub_rand(e):
    e.cl.rec("GetRand", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("GetRand", 0) & MASK)


STUBS.internal(SCR_ERROR, stub_error)
STUBS.internal(SCR_PUSH, stub_push)
STUBS.internal(SCR_POP, stub_pop)
STUBS.internal(GET_RAND, stub_rand)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
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

    def label(self, l, a):
        self.L.labels[a] = l

    def setptr(self, name, a):
        self.write(self.g(name), struct.pack(">I", a))

    def log(self):
        return self.L.log

    def call(self, name):
        T.call_ppc(self.e, FN[name][0], [])


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sk_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sk_alloc(max(n, 4))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def label(self, l, a):
        self.lib.tpw_cl_label(l, a)

    def setptr(self, name, a):
        self.write(self.g(name), struct.pack("<Q", a))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, name):
        self.lib.tpw_sk_call(ORDER.index(name))


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    p = "tpw_sk_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "call").argtypes = [C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's code array, script block and globals through Side.words: the same code for both memories."""
    code = [0] * CODE_WORDS
    for index, ws in case["code"]:
        code[index:index + len(ws)] = ws
    s.words(s.g("code"), code)
    block = s.alloc(0x118)
    s.words(block, case["block"])
    s.words(block + 8, [case["mpos"]])
    s.words(block + 0x14, [CODE + 4 * case["mcb"]])
    s.words(block + 0x70, [case["ifd"]])
    s.words(block + 0x78, [case["loopd"]])
    s.words(block + VAR_OFFSET, case["vars"])
    world = s.alloc(16)
    s.label(WORLD_LABEL, world)
    s.setptr("gScriptCurrent", block)
    s.words(s.g("gScriptCodeBase"), [CODE + 4 * case["shift"]])
    s.setptr("gScriptVars", block + VAR_OFFSET)
    s.setptr("gGameWorld", world)
    return block


def execute(s, case):
    s.call(case["fn"])


def observe(s, case, block):
    """Everything the call may change; the float destinations' NaNs made one."""
    words = s.rd_words(block, BLOCK_WORDS)
    for i in case["canon"]:
        words[VAR_OFFSET // 4 + i] = canon_f(words[VAR_OFFSET // 4 + i])
    return {"block": words, "base": s.rd_word(s.g("gScriptCodeBase")), "log": s.log()}


def run(s, case):
    try:
        block = build(s, case)
        execute(s, case)
        return observe(s, case, block)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

IV = [0, 1, -1, 5, 6, -5, 0x7FFFFFFF, -0x80000000]
CONSTS = [0, 1, -1, 2, 5, 6, 7, -5, 0x1000001, -0x1000001, 0x7FFFFFFF, -0x80000000, 0x40000000, 100, -100, 0x7FFFFF01]
FV = [0, 0x80000000, fb(1), fb(-1), fb(2.5), fb(5), fb(6), 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, fb(1e30), fb(-1e-30)]
SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]
TESTS_CODES = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xFFFFFFFF]
FILLERS = [0x10266958, 0x10266978, 0x10266980, 0, 1, 0x12345678, 0x10266988]


def rf(r):
    """A float's bits: mostly ordinary numbers of every scale, now and then a special value."""
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def ri(r):
    """An integer's bits: edges and small and full-range values."""
    k = r.random()
    if k < 0.45:
        return r.choice(CONSTS) & MASK
    if k < 0.8:
        return r.randrange(-20, 20) & MASK
    return r.getrandbits(32)


def rvar(r):
    return r.choice([1, 2, 3, 4, 5, 6, 0, 7, 8, 12, 18])


def rvars(r):
    return [rf(r) if r.random() < 0.5 else ri(r) for _ in range(19)]


def token_stream(r, n):
    out = []
    for _ in range(n):
        k = r.random()
        out.append(D_IF if k < 0.25 else D_ENDIF if k < 0.5 else D_ELSE if k < 0.6 else r.choice(FILLERS))
    return out


def mk(r, fn, words=(), tail=(), vars=None, ifd=None, loopd=None, queues=None, canon=(), place=None):
    """A case: the command's operand words and a token tail in the code array, the block, the globals and the fakes' queues."""
    stream = list(words) + list(tail)
    n = len(stream)
    if place == "tail" or (place is None and r.random() < 0.3):
        index = CODE_WORDS - n
    else:
        index = r.randrange(0, min(CODE_WORDS - n, 700) + 1)
    shift = 0 if r.random() < 0.7 else r.randrange(0, index + 1)
    mcb = shift if r.random() < 0.85 else r.randrange(0, index + 1)
    return {
        "fn": fn, "code": [(index, stream)], "block": [r.getrandbits(32) for _ in range(BLOCK_WORDS)], "mpos": index - shift,
        "shift": shift, "mcb": mcb, "ifd": r.choice([0, 1, 2, 5, 9, 10, 11, -1, 12]) if ifd is None else ifd,
        "loopd": r.choice([0, 1, 2, 3, -1]) if loopd is None else loopd, "vars": rvars(r) if vars is None else vars,
        "queues": queues if queues is not None else {}, "canon": list(canon),
    }


def setvars(r, assign):
    v = rvars(r)
    for i, x in assign.items():
        v[i] = x & MASK
    return v


def cmp_cases(r, fn, queues_for=None, ifd=None, loopd=None):
    """The IF / UNTIL grid: float and integer destinations, every test code, variable and constant right sides, NaNs."""
    k = 0
    for a in (1, 6, 3, 0, 7, 18, 2):
        floaty = 1 <= a <= 6
        for test in TESTS_CODES:
            if floaty:
                pairs = [(x, y) for x in FV for y in FV] if test in (1, 2, 3, 4) else [(x, c) for x in FV for c in CONSTS]
            else:
                pairs = [(x, y) for x in IV for y in IV] if test in (1, 2, 3, 4) else [(x, c) for x in IV for c in CONSTS]
            if test in (0, 9, 0xFFFFFFFF):
                pairs = pairs[:6]
            if a in (3, 2, 18, 7):
                pairs = pairs[::5]
            for x, y in pairs:
                b = r.choice([5, 8, 10, 4, a]) if test in (1, 2, 3, 4, 0, 9, 0xFFFFFFFF) else y
                assign = {a: x}
                if test in (1, 2, 3, 4):
                    assign = {b: y}
                    assign[a] = x
                tails = [[FILLERS[0], D_ELSE, FILLERS[1], D_ENDIF], [FILLERS[2], FILLERS[0]], [D_IF, FILLERS[1], D_ENDIF, D_ELSE, D_ENDIF],
                         [D_ENDIF], [D_ELSE]]
                tail = tails[k % len(tails)]
                k += 1
                yield mk(r, fn, [a, test, b & MASK], tail, vars=setvars(r, assign), ifd=ifd, loopd=loopd,
                         queues=queues_for(r) if queues_for else None)


def finder_cases(r, fn):
    """Every token string up to length 5 at the end of the array, longer ones in the middle, and runs that never reach an ENDIF."""
    alphabet = [D_IF, D_ENDIF, D_ELSE, 0x10266958]
    streams = [[]]
    for n in range(1, 6):
        streams += [[alphabet[(i // 4 ** j) % 4] for j in range(n)] for i in range(4 ** n)]
    for st in streams:
        yield mk(r, fn, [], st, place="tail")
    for st in streams[::7]:
        yield mk(r, fn, [], st + [D_ENDIF, D_ENDIF], place="mid")
    for depth in range(0, 7):
        for closers in range(0, depth + 3):
            yield mk(r, fn, [], [D_IF, 0x10266958] * depth + [D_ELSE, D_ENDIF] * 0 + [D_ENDIF] * closers + [0x10266958], place="tail")
            yield mk(r, fn, [], [D_IF] * depth + [D_ELSE] + [D_ENDIF] * closers, place="mid")


def edge_finder(fn):
    def gen():
        r = random.Random(0xF1 + len(fn))
        yield from finder_cases(r, fn)
    return gen


def rnd_finder(fn):
    def gen(r):
        return mk(r, fn, [], token_stream(r, r.randrange(0, 40)))
    return gen


def if_edges():
    r = random.Random(0x1F)
    yield from cmp_cases(r, "PK_IF")
    for d in (-1, 0, 8, 9, 10, 11, 12):
        for test in (1, 5):
            yield mk(r, "PK_IF", [1 if test == 5 else 0, test, 5], [D_ELSE, D_ENDIF], ifd=d, vars=setvars(r, {0: 3, 1: fb(5), 5: 3}))


def test_operand(r, test):
    """The third operand: a variable index for tests 1 to 4 (and the invalid ones, which never read it), a constant for 5 to 8."""
    return ri(r) if 5 <= test <= 8 else rvar(r)


def if_random(r):
    a, test = rvar(r), r.choice(TESTS_CODES[:10])
    return mk(r, "PK_IF", [a, test, test_operand(r, test)], token_stream(r, r.randrange(0, 12)))


def until_edges():
    r = random.Random(0x2F)

    def queue(rr):
        return {"pop": [rr.choice([0, 3, 100, -1]) & MASK]}
    yield from cmp_cases(r, "PK_UNTIL", queue, loopd=1)
    for loopd in (0, -1, 2):
        for test in (1, 5, 0, 9):
            yield mk(r, "PK_UNTIL", [1, test, 5], [], loopd=loopd, queues={"pop": [7]})


def until_random(r):
    a, test = rvar(r), r.choice(TESTS_CODES[:10])
    return mk(r, "PK_UNTIL", [a, test, test_operand(r, test)], [], loopd=r.choice([0, 1, 2, -1]),
              queues={"pop": [r.choice([0, 5, 100, 400, -1]) & MASK]})


def endif_edges():
    r = random.Random(0x3F)
    for d in (-3, -2, -1, 0, 1, 2, 10, 11, 0x7FFFFFFF, -0x80000000):
        yield mk(r, "PK_ENDIF", [], [], ifd=d)


def else_edges():
    r = random.Random(0x4F)
    for d in (-1, 0, 1, 2, 5, 0x7FFFFFFF, -0x80000000):
        for tail in ([], [D_ENDIF], [D_IF, D_ENDIF, D_ENDIF], [D_IF, D_IF], [D_ELSE, D_ENDIF], [D_IF, D_IF, D_IF, D_ENDIF]):
            yield mk(r, "PK_ELSE", [], tail, ifd=d)
            yield mk(r, "PK_ELSE", [], tail, ifd=d, place="tail")


def repeat_edges():
    r = random.Random(0x5F)
    for d in (-1, 0, 1, 0x7FFFFFFF):
        yield mk(r, "PK_REPEAT", [], [], loopd=d)
        yield mk(r, "PK_REPEAT", [], [], loopd=d, place="tail")


def do_edges():
    r = random.Random(0x6F)
    for c in (0, 1, 2, -1, 5, -0x80000000, 0x7FFFFFFF):
        for d in (-1, 0, 3):
            yield mk(r, "PK_DO", [c & MASK], [], loopd=d)


def loop_edges():
    r = random.Random(0x7F)
    for d in (-1, 0, 1, 2):
        for c in (0, 1, 2, 3, -1, -0x80000000, 0x7FFFFFFF, -0x7FFFFFFF):
            for p in (0, 12, 700, -1):
                yield mk(r, "PK_LOOP", [], [], loopd=d, queues={"pop": [c & MASK, p & MASK]})


def one_arg_random(fn, kind):
    def gen(r):
        if fn == "PK_ENDIF":
            return mk(r, fn, [], [], ifd=r.choice([-1, 0, 1, 2, 11, r.randrange(-5, 15)]))
        if fn == "PK_ELSE":
            return mk(r, fn, [], token_stream(r, r.randrange(0, 20)), ifd=r.choice([-1, 0, 1, 2, 11, r.randrange(-5, 15)]))
        if fn == "PK_REPEAT":
            return mk(r, fn, [], [], loopd=r.choice([-1, 0, 1, r.randrange(-5, 15)]))
        if fn == "PK_DO":
            return mk(r, fn, [ri(r)], [], loopd=r.choice([-1, 0, 1, r.randrange(-5, 15)]))
        return mk(r, fn, [], [], loopd=r.choice([-1, 0, 1, 2, r.randrange(-5, 15)]), queues={"pop": [ri(r), r.randrange(0, 800)]})
    return gen


def arith_dests():
    return [1, 2, 3, 6, 0, 7, 8, 18]


def arith_cases(r, fn, kind):
    """kind: 'const' (dest, constant), 'var' (dest, source var), 'mulvar'; dest floats and integers, constant edges, float edges."""
    for d in arith_dests():
        floaty = 1 <= d <= 6
        values = FV if floaty else IV + [0x80000000, 0x7FFFFFFE]
        for x in values:
            second = CONSTS if kind in ("const", "mod") else [0, 1, 2, 5, 12, 18]
            for c in second:
                if kind in ("var", "modvar", "mulvar") and c in (0, 5, 12, 18) and r.random() < 0.3:
                    continue
                if kind in ("const", "mod"):
                    words = [d, c & MASK]
                    v = setvars(r, {d: x})
                else:
                    y = r.choice(FV if floaty else IV + [0])
                    words = [d, c]
                    v = setvars(r, {d: x, c: y} if c != d else {d: x})
                canon = [1, 2] if kind == "mulvar" else [d] if floaty else []
                yield mk(r, fn, words, [], vars=v, canon=canon)


def arith_edges(fn, kind):
    def gen():
        r = random.Random(0x80 + len(fn) * 7 + ord(fn[-1]))
        yield from arith_cases(r, fn, kind)
    return gen


def arith_random(fn, kind):
    def gen(r):
        d = rvar(r) if r.random() < 0.9 else r.choice([0, 7, 18])
        floaty = 1 <= d <= 6
        if kind in ("const", "mod"):
            words = [d, ri(r)]
        else:
            words = [d, r.choice([rvar(r), 0, 18, d])]
        v = rvars(r)
        canon = [1, 2] if kind == "mulvar" else [d] if floaty else []
        return mk(r, fn, words, [], vars=v, canon=canon)
    return gen


def and_edges():
    r = random.Random(0x9A)
    for d in (0, 7, 18, 2):
        for s in (0, 1, 7, d):
            for c in CONSTS:
                yield mk(r, "PK_AND", [d, s, c & MASK], [], vars=setvars(r, {s: r.choice(IV + [0x80000000]) & MASK}))


def and_random(r):
    return mk(r, "PK_AND", [r.choice([0, 1, 7, 18, 3]), r.choice([0, 1, 7, 18, 3]), ri(r)], [])


def copyvar_edges():
    r = random.Random(0x9B)
    for d in (0, 1, 6, 18):
        for s in (0, 1, 5, 18, d):
            for x in SPECIAL + [0xFFFFFFFF]:
                yield mk(r, "PK_COPYVAR", [d, s], [], vars=setvars(r, {s: x}))


def copyvar_random(r):
    return mk(r, "PK_COPYVAR", [rvar(r), rvar(r)], [])


def random_edges():
    r = random.Random(0x9C)
    for d in (0, 1, 18):
        for rnd in (0, 1, 5, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 100):
            for n in (0, 1, -1, -2, 4, 99, 0x7FFFFFFF, -0x80000000, 0x7FFFFFFE):
                yield mk(r, "PK_RANDOM", [d, n & MASK], [], queues={"GetRand": [rnd]})


def random_random(r):
    return mk(r, "PK_RANDOM", [rvar(r), ri(r)], [], queues={"GetRand": [ri(r)]})


KINDS = {"PK_ADD": "const", "PK_SUB": "const", "PK_MUL": "const", "PK_DIV": "const", "PK_MOD": "mod", "PK_MULVAR": "mulvar",
         "PK_DIVVAR": "var", "PK_ADDVAR": "var", "PK_MODVAR": "modvar"}

EDGES = {
    "SCR_FindNextEndif": edge_finder("SCR_FindNextEndif"), "SCR_FindNextElseOrEndif": edge_finder("SCR_FindNextElseOrEndif"),
    "PK_IF": if_edges, "PK_ENDIF": endif_edges, "PK_ELSE": else_edges, "PK_REPEAT": repeat_edges, "PK_UNTIL": until_edges,
    "PK_DO": do_edges, "PK_LOOP": loop_edges, "PK_AND": and_edges, "PK_COPYVAR": copyvar_edges, "PK_RANDOM": random_edges,
}
RANDOMS = {
    "SCR_FindNextEndif": rnd_finder("SCR_FindNextEndif"), "SCR_FindNextElseOrEndif": rnd_finder("SCR_FindNextElseOrEndif"),
    "PK_IF": if_random, "PK_UNTIL": until_random, "PK_AND": and_random, "PK_COPYVAR": copyvar_random, "PK_RANDOM": random_random,
}
for _fn in ("PK_ENDIF", "PK_ELSE", "PK_REPEAT", "PK_DO", "PK_LOOP"):
    RANDOMS[_fn] = one_arg_random(_fn, None)
for _fn, _kind in KINDS.items():
    EDGES[_fn] = arith_edges(_fn, _kind)
    RANDOMS[_fn] = arith_random(_fn, _kind)

TESTS = [FnTest(name, ppc, host, edge=EDGES[name], rnd=RANDOMS[name]) for name in ORDER]
