"""Differential tests for the advisor's message scoring (unit w95), against libtpw_unit_AdvisorScoring.dylib (tests/unit_AdvisorScoring.cpp):

  src/game/advisor/Advisor.cpp, src/game/advisor/AdvisorScoring.cpp   CAdvisorScoring: the constructor (0x10010054), the destructor (0x1000adb0), GetScoreFor (0x1001019c),
                           GetNumResponsesFor, GetMaxDuplicatesInQueueFor, GetMinTimeFor, SayOnlyOnce, GetCyclingFor, GetMessageCategory,
                           HasThisBeenKilledTooManyTimes, IsThisValidInTheCurrentMode and IsThisATutorialMessage (0x1001055c..0x10010f38)

A case is a world, one call and its arguments. The world: the message table of the file image (0x1027efa4, 351 records) with some fields
of some records patched (a patch is [record, word, value]; word 12 is the record's scorer, 0 .. 2), the object's 393 balance slots, the game type
object (its word, its guard byte, and what its constructor would make), and the queues the fakes answer ("Scorer", "Balance.Load"). The
PowerPC side runs the original on the emulator, the host the translation; the test compares the answer, the call log (the traces, the scorer
calls, the balance table's Init, Load and UnInit, the delete), the object's first words and the heap counts.

Real on both sides: the unit's functions and the message table. Fakes that record the same event on both sides: CBalanceTable::Init, Load
and UnInit, CTRACE, the three scorers (_ADVISOR_NO_SCORE, _ADVISOR_WELCOME and _ADVISOR_OPEN_PARK, whose addresses the PowerPC side's table
points at through the runtime's pointer-to-member glue at 0x1023ba70, which runs as the original). CGameType's constructor is a fake that
records nothing and makes the type `ctor_type`; the guard byte of the game type object is the compiler's static guard on the host, which a test
cannot reset, so the host side starts the type at `ctor_type` when the guard byte is 0 (the original then runs the constructor, if its path gets
that far) and at the object's word otherwise.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_AdvisorScoring"

TABLE = 0x1027EFA4
RECORDS = 351
GAME_TYPE, GAME_GUARD = 0x102B3AA0, 0x102B3AA4
SLOTS = 393
VALUES_AT = 0x14               # the first balance slot's offset in the object
OBJECT_SIZE = VALUES_AT + 4 * SLOTS + 8
MINUS = 0xFFFFFFFF

QUERIES = dict(score=0x1001019C, num=0x1001055C, maxdup=0x10010654, mintime=0x10010758, once=0x10010864, cycling=0x10010980,
               category=0x10010A74, killed=0x10010B6C, valid=0x10010CC8, tutorial=0x10010E78)
FN = dict(score=0, num=1, maxdup=2, mintime=3, once=4, cycling=5, category=6, killed=7, valid=8, tutorial=9, ctor=10, dtor_inplace=11,
          dtor_delete=12, dtor_none=13)
CTOR, DTOR = 0x10010054, 0x1000ADB0
SCORERS = [0x10011358, 0x1001139C, 0x100113DC]       # _ADVISOR_NO_SCORE, _ADVISOR_WELCOME, _ADVISOR_OPEN_PARK
SCORER_NAMES = ["ADVISOR_NO_SCORE", "ADVISOR_WELCOME", "ADVISOR_OPEN_PARK"]
FAKES = dict(bal_init=0x1001EA28, bal_uninit=0x1001ED40, bal_load=0x10020134, ctrace=0x1006FCBC, game_type=0x1016F5D8)
LAB = dict(object=2, balance=3)

PRISTINE = b""
STUBS = None


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MINUS))


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def record(i):
    """The pristine record i as twelve words."""
    return struct.unpack_from(">12I", PRISTINE, i * 0x30)


# ======================================================================= PowerPC side


class Ppc:
    pass


def build_stubs():
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(QUERIES.values())
    S.real.update([CTOR, DTOR])
    S.internal(FAKES["bal_init"], R("Balance.Init", "N"))
    S.internal(FAKES["bal_uninit"], R("Balance.UnInit", "L"))

    def bal_load(e):
        L = e.cl
        L.rec("Balance.Load", (L.lab(e.r(3)), T.s32(e.r(5))), (e.cstr(e.r(4))[:159],))
        e.set_r(3, T.u32(L.ret("Balance.Load")))

    S.internal(FAKES["bal_load"], bal_load)
    S.internal(FAKES["ctrace"], lambda e: e.cl.rec("CTRACE", (e.r(3),), (T.c_format(e, e.cstr(e.r(4)), 5)[:159],)))
    # CGameType::CGameType: makes the type, records nothing
    S.internal(FAKES["game_type"], lambda e: w32(e, e.r(3), e.mv.ctor_type))
    for address, name in zip(SCORERS, SCORER_NAMES):
        S.internal(address, R(name, "L", ret=lambda e, L: L.ret("Scorer")))

    def op_delete(e):
        L = e.cl
        p = e.r(3)
        e.deletes.append(p)
        e.alloc_log.append(("delete", p, 0))
        if p and L.lab(p) > 0:
            L.rec("delete", (L.lab(p),))
            L.forget(p)

    S.imp("__dl__FPv", op_delete)
    return S


class World:
    """The PowerPC side of one case: builds the world, calls, dumps."""

    def __init__(self, e, case):
        self.e = e
        s = e.mv = Ppc()
        e.heap_in_scratch = True
        s.ctor_type = case.get("ctor_type", 0) & MINUS
        e.cl = CL.PpcLog({}, case.get("q", {}))
        # the table: the file's, then the patches, then a pointer to member per record
        table = bytearray(PRISTINE)
        scorer = {i: i % 3 for i in range(RECORDS)}
        for index, word, value in case.get("patches", []):
            if word == 12:
                scorer[index] = value
            else:
                struct.pack_into(">I", table, index * 0x30 + 4 * word, value & MINUS)
        glue = [e.alloc(8, struct.pack(">II", address, e.toc)) for address in SCORERS]
        for i in range(RECORDS):
            struct.pack_into(">III", table, i * 0x30 + 0x10, 0, MINUS, glue[scorer[i]])
        e.write(TABLE, bytes(table))
        # the game type object
        w32(e, GAME_TYPE, case.get("type", 0))
        e.write(GAME_GUARD, bytes([case.get("guard", 1) & 0xFF]))
        # the object
        self.this = self.block = 0
        if case["fn"] == "ctor":
            self.block = e.alloc(OBJECT_SIZE, b"\xa5" * OBJECT_SIZE)
        elif case.get("object", True):
            self.this = e.alloc(OBJECT_SIZE, b"\xa5" * OBJECT_SIZE)
            values = case["values"]
            for i in range(SLOTS):
                w32(e, self.this + VALUES_AT + 4 * i, values[i])
            w32(e, self.this, case.get("cursor", 0))
            e.cl.labels[self.this] = LAB["object"]
            e.cl.labels[self.this + 0xC] = LAB["balance"]
        e.alloc_log = []

    def call(self, case):
        e = self.e
        fn = case["fn"]
        if fn in QUERIES:
            args = [self.this, case["message"] & MINUS]
            if fn == "killed":
                args.append(case["killed"] & MINUS)
            return T.s32(T.call_ppc(e, QUERIES[fn], args)[0])
        if fn == "ctor":
            T.call_ppc(e, CTOR, [self.block])
            return 0
        T.call_ppc(e, DTOR, [self.this, case["flag"] & MINUS])
        return 0

    def dump(self, case):
        e = self.e
        base = self.block or self.this
        if not base or case["fn"] == "dtor_delete":
            return []
        v = [e.u32(base), struct.unpack(">H", e.read(base + 4, 2))[0], e.u32(base + 8)]
        v += [e.u32(base + VALUES_AT + 4 * i) for i in range(4)]
        return v

    def heap(self):
        log = self.e.alloc_log
        return [sum(1 for k in log if k[0] == "new"), sum(1 for k in log if k[0] == "delete")]


def setup(runner):
    global STUBS, PRISTINE
    e = runner.emu
    T.install_std_stubs(e)
    PRISTINE = bytes(e.read(TABLE, RECORDS * 0x30))
    STUBS = build_stubs()


def ppc_run(e, case):
    saved = STUBS.activate(e)
    try:
        w = World(e, case)
        result = w.call(case)
        out = dict(result=result, log=e.cl.log, dump=w.dump(case), heap=w.heap())
    finally:
        CL.Stubs.deactivate(e, saved)
        e.cl = None
    return out


# ======================================================================= host side


def host_run(lib, case):
    lib.tpw_as_reset.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_as_patch.argtypes = [C.c_int32, C.c_int32, C.c_int64]
    lib.tpw_as_build.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_as_call.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_as_call.restype = C.c_int64
    lib.tpw_as_dump.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_as_heap.argtypes = [C.POINTER(C.c_int64)]
    CL.host_begin(lib, {}, case.get("q", {}))
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    guard = case.get("guard", 1)
    ctor_type = case.get("ctor_type", 0)
    # a guard byte of 0 makes the original run the constructor before it reads the type
    lib.tpw_as_reset(T.s32(ctor_type if (guard & 0xFF) == 0 else case.get("type", 0)), T.s32(ctor_type))
    for index, word, value in case.get("patches", []):
        lib.tpw_as_patch(index, word, T.s32(value))
    fn = case["fn"]
    if fn == "ctor":
        lib.tpw_as_block()
    elif case.get("object", True):
        values = [T.s32(x) for x in case["values"]] + [T.s32(case.get("cursor", 0))]
        lib.tpw_as_build((C.c_int64 * len(values))(*values))
    before = (C.c_int64 * 4)()
    lib.tpw_as_heap(before)
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 1
    if fn in QUERIES:
        call = (C.c_int64 * 3)(FN[fn], T.s32(case["message"]), T.s32(case.get("killed", 0)))
    elif fn == "ctor":
        call = (C.c_int64 * 3)(FN[fn], 0, 0)
    else:
        flag = s16(case["flag"])
        op = FN["dtor_none"] if not case.get("object", True) else FN["dtor_delete"] if flag > 0 else FN["dtor_inplace"]
        call = (C.c_int64 * 3)(op, 0, 0)
    result = lib.tpw_as_call(call)
    log = CL.host_end(lib)
    after = (C.c_int64 * 4)()
    lib.tpw_as_heap(after)
    out = (C.c_int64 * 16)()
    lib.tpw_as_dump(out)
    dump = list(out[1:1 + out[0]])
    if fn == "dtor_delete":
        dump = []
    return dict(result=T.s32(result) if fn in QUERIES else 0, log=log, dump=dump, heap=[after[0] - before[0], after[2] - before[2]])


# ======================================================================= case generation

INTS = [0, 1, -1, 2, 3, 5, 10, 100, 255, 256, 0x1FF, 0x160, 0x7FFFFFFF, -0x80000000, 0x12345678]
TYPES = [0, 1, 2, 3, -1, 7]
GUARDS = [0, 1, 0x80, 0xFF, 2]


def baseline_values():
    """Slots that tell each other apart; zeros on some (the flag queries test for them)."""
    return [(i * 37 + 11) % 97 for i in range(SLOTS)]


def base_case(fn, message, **extra):
    case = dict(fn=fn, message=message, patches=[], values=baseline_values(), type=0, guard=1, ctor_type=0, killed=0, q={})
    case.update(extra)
    return case


def rnd_values(r):
    pool = [0, 1, 2, 3, 5, -1, 100, 0x7FFFFFFF, -0x80000000]
    return [r.choice(pool) if r.random() < 0.7 else r.randrange(-50, 400) for _ in range(SLOTS)]


def rnd_patch(r):
    index = r.randrange(0, RECORDS)
    word = r.choice([0, 1, 2, 3, 7, 8, 9, 10, 11, 12, 1, 2, 3])
    if word == 1:
        value = r.choice([r.randrange(0, RECORDS), index, index + 1, 0, 5, 9])
    elif word == 2:
        value = r.randrange(-1, 12)
    elif word == 3:
        value = r.randrange(-1, 5)
    elif word == 12:
        value = r.randrange(0, 3)
    else:
        value = r.choice(INTS)
    return [index, word, value]


def rnd_message(r, patches):
    k = r.random()
    if k < 0.5 or not patches:
        return r.choice([r.randrange(-5, 364), r.randrange(0, RECORDS), 0x15F, 0x160, 0x161])
    p = r.choice(patches)
    return r.choice([p[0], p[2] if p[1] == 1 else p[0], p[0] + 1])


def rnd_world(r, fn):
    patches = [rnd_patch(r) for _ in range(r.choice([0, 0, 1, 2, 3, 6]))]
    case = base_case(fn, rnd_message(r, patches), patches=patches, values=rnd_values(r) if r.random() < 0.8 else baseline_values(),
                     type=r.choice(TYPES), guard=r.choice(GUARDS), ctor_type=r.choice([0, 1, 2, 3]))
    case["q"] = {"Scorer": [r.choice([0, 1, -1, 7, 1000, 0x7FFFFFFF, -0x80000000]) for _ in range(2)]}
    if case["message"] == -1 and fn in WILD_ON_MINUS_ONE:
        case["message"] = -2
    if fn == "killed":
        case["killed"] = r.choice(INTS + [r.randrange(-3, 12)])
    return case


# Message -1 finds the data before the table as a record (AdvisorScoring.h); these four then run wild on the Mac (a call through garbage, a
# read far outside the object), so they are not asked for it.
WILD_ON_MINUS_ONE = ("score", "mintime", "once", "killed")


def id_edges(fn, extra=None):
    """Every message number around the table, with the file's table and distinct slot values."""
    ids = list(range(-3, RECORDS + 8)) + [0x7FFFFFFF, -0x80000000, 1000, 5000, -1000, -5000, 20000]
    for message in ids:
        if message == -1 and fn in WILD_ON_MINUS_ONE:
            continue
        case = base_case(fn, message)
        case["q"] = {"Scorer": [123]}
        if extra:
            case.update(extra)
        yield case


def category_edges(fn):
    """A record's category 0 .. 11 (and -1), so that every group of the balance table is read."""
    for category in range(-1, 12):
        for index in (0, 3, 100):
            yield base_case(fn, index, patches=[[index, 2, category]])


def lookup_edges(fn):
    """The search path and its quirk: a record whose number is not its index."""
    for first, second in ((9, 20), (9, 5), (0, 100), (350, 0), (100, 349), (7, 7)):
        # record `first` carries the number `second`... and `second` the number `first`
        for message in (first, second):
            yield base_case(fn, message, patches=[[first, 1, second], [second, 1, first]])
        yield base_case(fn, first, patches=[[first, 1, second]])      # the number `first` is nowhere: a miss with the trace
        yield base_case(fn, second, patches=[[first, 1, second]])     # two records carry it: the fast path
        yield base_case(fn, second, patches=[[second, 1, first], [first, 1, second]])
    yield base_case(fn, 0, patches=[[0, 1, 5]])                       # the first record's number changed; 0 is nowhere
    yield base_case(fn, 5, patches=[[0, 1, 5], [5, 1, 6]])            # found at index 0, answered with record 5's fields
    yield base_case(fn, 350, patches=[[350, 1, 0], [0, 1, 350]])
    yield base_case(fn, 350, patches=[[349, 1, 350], [350, 1, 0]])   # found at index 349 (the loop's last but one)
    yield base_case(fn, 349, patches=[[350, 1, 349], [349, 1, 0]])


def field_edges(fn, words):
    """Each field the function reads, at the values around its width: the first record after the sentinel, a middle one."""
    for index in (1, 2, 200, 350):
        for word in words:
            for value in INTS:
                yield base_case(fn, index, patches=[[index, word, value]])


def edges_for(fn, words=(), category=False):
    def gen():
        yield from id_edges(fn)
        if words:
            yield from field_edges(fn, words)
        if category:
            yield from category_edges(fn)
        yield from lookup_edges(fn)
    return lambda: list(gen())


def rnd_for(fn):
    return lambda r: rnd_world(r, fn)


# ---- the mode checks: modes x game type word x guard byte x constructor type

def mode_edges(fn):
    for mode in (-1, 0, 1, 2, 3, 4):
        for gtype in TYPES:
            for guard in GUARDS:
                for ctor_type in (0, 1, 2):
                    case = base_case(fn, 5, patches=[[5, 3, mode]], type=gtype, guard=guard, ctor_type=ctor_type)
                    case["q"] = {"Scorer": [31]}
                    yield case


# ---- GetScoreFor: scorers and what they answer

def score_edges():
    yield from id_edges("score")
    yield from lookup_edges("score")
    for scorer in range(3):
        for mode in (0, 1, 2):
            for gtype in (0, 1, 2):
                for index in (0, 1, 2, 3, 100):
                    case = base_case("score", index, patches=[[index, 12, scorer], [index, 3, mode]], type=gtype)
                    case["q"] = {"Scorer": [index + 500]}
                    yield case
    for answer in (0, -1, 1, 0x7FFFFFFF, -0x80000000):
        case = base_case("score", 1)
        case["q"] = {"Scorer": [answer]}
        yield case
    # the sentinels and misses never reach the scorer or the mode check
    for message in (0x15F, 0x160, 0x161, -2, 351):
        yield base_case("score", message)


# ---- HasThisBeenKilledTooManyTimes: the kill count around the category's limit

def killed_edges():
    yield from id_edges("killed", dict(killed=3))
    yield from lookup_edges("killed")
    for index in (0, 1, 2, 60, 200, 350):
        category = record(index)[2]
        slot = 4 + 3 * category + 2
        limit = baseline_values()[slot]
        for killed in (limit - 1, limit, limit + 1, 0, -1, 1, 0x7FFFFFFF, -0x80000000):
            yield base_case("killed", index, killed=killed)
        for limit in (0, 1, -1, 2, 0x7FFFFFFF, -0x80000000):
            values = baseline_values()
            values[slot] = limit
            for killed in (limit - 1, limit, limit + 1, 0, 1, -1):
                yield base_case("killed", index, killed=killed, values=values)
    for category in range(-1, 12):
        values = baseline_values()
        slot = 4 + 3 * category + 2
        for limit in (0, 1, 5):
            values[slot] = limit
            for killed in (limit - 1, limit, limit + 1):
                yield base_case("killed", 3, patches=[[3, 2, category]], values=list(values), killed=killed)


def once_edges():
    yield from id_edges("once")
    yield from lookup_edges("once")
    yield from category_edges("once")
    for index in (0, 1, 2, 60, 200, 350):
        slot = 4 + 3 * record(index)[2] + 1
        for flag in (0, 1, -1, 2, 0x100, 0x7FFFFFFF, -0x80000000):
            values = baseline_values()
            values[slot] = flag
            yield base_case("once", index, values=values)


def mintime_edges():
    yield from id_edges("mintime")
    yield from lookup_edges("mintime")
    yield from category_edges("mintime")
    for index in (0, 1, 60, 350):
        slot = 4 + 3 * record(index)[2]
        for value in INTS:
            values = baseline_values()
            values[slot] = value
            yield base_case("mintime", index, values=values)


# ---- the constructor and destructor

def ctor_cases(r=None):
    for answer in (0, 1, -1, 2, 0x100, 0x7FFFFFFF):
        yield dict(fn="ctor", q={"Balance.Load": [answer]}, patches=[])


def rnd_ctor(r):
    return dict(fn="ctor", q={"Balance.Load": [r.choice([0, 1, -1, 2, 0x100, 0x7FFFFFFF, r.randrange(-5, 5)])]}, patches=[])


FLAGS = [1, 0, 2, 0x7FFF, 0x8000, 0x10000, 0x10001, -1, -0x8000, 0xFFFF, 0x7FFFFFFF, -0x80000000]


def dtor_edges():
    for flag in FLAGS:
        yield base_case("dtor", 0, flag=flag, fn_name="dtor")
    for flag in (1, 0, -1):
        yield base_case("dtor", 0, flag=flag, object=False)


def rnd_dtor(r):
    return base_case("dtor", 0, flag=r.choice(FLAGS + [r.randrange(-0x10000, 0x10000)]), values=rnd_values(r),
                     cursor=r.choice([0, 5, -1, 0x15E]), object=r.random() < 0.9)


def fix_dtor(case):
    case["fn"] = "dtor_delete" if s16(case["flag"]) > 0 else "dtor_inplace"
    if not case.get("object", True):
        case["fn"] = "dtor_none"
    return case


def dtor_edge_cases():
    return [fix_dtor(c) for c in dtor_edges()]


def dtor_rnd(r):
    return fix_dtor(rnd_dtor(r))


# ---- CAdvisorBalance: the two virtuals and the table behind them

BALANCE = dict(desc=0x1000E93C, values=0x1000E98C)
DESC_ROWS = 775
TABLE_DESC = 0x1028318C


def balance_ppc(e, case):
    this = e.alloc(0x20, b"\0" * 0x20)
    if case["fn"] == "values":
        return dict(result=int(T.call_ppc(e, BALANCE["values"], [this])[0] == this + 8))
    address = T.call_ppc(e, BALANCE["desc"], [this, case["row"] & MINUS])[0]
    raw = e.read(address, 0x3C)
    t, = struct.unpack(">i", raw[:4])
    mi, ma, fi, fa, ac, fl = struct.unpack(">iiIIiI", raw[36:60])
    return dict(result=int(address == TABLE_DESC + 0x3C * case["row"]), row=[t] + list(raw[4:36]) + [mi, ma, fi, fa, ac, fl])


def balance_host(lib, case):
    lib.tpw_as_values_start.restype = C.c_int64
    if case["fn"] == "values":
        return dict(result=int(lib.tpw_as_values_start()))
    out = (C.c_int64 * 40)()
    lib.tpw_as_desc.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_as_desc(case["row"], out)
    return dict(result=1, row=list(out)[:39])


def balance_desc_cases():
    return [dict(fn="desc", row=row) for row in range(DESC_ROWS)]


# ---- the tests

def make_test(name, fn, edge, rnd=None):
    return FnTest(name, ppc_run, host_run, edge=edge, rnd=rnd if rnd is not None else rnd_for(fn))


TESTS = [
    make_test("CAdvisorScoring::GetScoreFor", "score", lambda: list(score_edges())),
    make_test("CAdvisorScoring::GetNumResponsesFor", "num", edges_for("num", words=(9,), category=False)),
    make_test("CAdvisorScoring::GetMaxDuplicatesInQueueFor", "maxdup", edges_for("maxdup", words=(7,))),
    make_test("CAdvisorScoring::GetMinTimeFor", "mintime", lambda: list(mintime_edges())),
    make_test("CAdvisorScoring::SayOnlyOnce", "once", lambda: list(once_edges())),
    make_test("CAdvisorScoring::GetCyclingFor", "cycling", edges_for("cycling", words=(10,))),
    make_test("CAdvisorScoring::GetMessageCategory", "category", edges_for("category", words=(2,))),
    make_test("CAdvisorScoring::HasThisBeenKilledTooManyTimes", "killed", lambda: list(killed_edges())),
    make_test("CAdvisorScoring::IsThisValidInTheCurrentMode", "valid",
              lambda: list(id_edges("valid")) + list(lookup_edges("valid")) + list(mode_edges("valid"))),
    make_test("CAdvisorScoring::IsThisATutorialMessage", "tutorial", edges_for("tutorial", words=(2,))),
    FnTest("CAdvisorScoring::CAdvisorScoring", ppc_run, host_run, edge=lambda: list(ctor_cases()), rnd=rnd_ctor),
    FnTest("CAdvisorScoring::~CAdvisorScoring", ppc_run, host_run, edge=dtor_edge_cases, rnd=dtor_rnd),
    FnTest("CAdvisorBalance::GetBalanceDescription", balance_ppc, balance_host, edge=balance_desc_cases,
           rnd=lambda r: dict(fn="desc", row=r.randrange(DESC_ROWS))),
    FnTest("CAdvisorBalance::GetBalanceValuesStart", balance_ppc, balance_host, edge=lambda: [dict(fn="values")],
           rnd=lambda r: dict(fn="values")),
]
