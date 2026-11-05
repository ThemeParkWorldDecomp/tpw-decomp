"""The real main balance table under the code that reads it, against libtpw_unit_BalanceRealValues.dylib (tests/unit_BalanceRealValues.cpp),
main app ("SimTheme Park"):
  CBalanceTable::Load 0x10020134 (src/game/balance/BalanceLoader.cpp), CPerson::GetHappinessAmount 0x1011b198, RaiseHappiness 0x1011b258,
  LowerHappiness 0x1011b384, ExtraExcitementEffects 0x10119c0c (src/game/people/Person.cpp), CUI2DMap::CreateMapSprites 0x1022fd78
  (src/game/ui/ui_2dmap.cpp)

The other tests of these readers write the balance words by hand, one slot at a time. Here no slot is written by the test on either side: the
game's own data/levels/Standard.sam (read from ROOT/extracted at run time, nothing is copied into the repository) goes through the original's
reader at the original's gMainBalance on the PowerPC side and through ours at ours on the host side, as CWorld::SetupNewLevel does first, and
the readers then run on what each table holds. Without ROOT/extracted the module has no tests.

The file calls, the heap and GAME_ASSERTMSG are fakes on both sides (tests/test_BalanceTable.py's); so are the readers' other callees (those of
tests/test_CPerson_GetBagged.py, tests/test_CPerson_TooPricey.py and tests/test_CUI2DMap_CreateMapSprites.py, whose map world this reuses).

Compared on both sides: the loader's answer and call log, the row information and every slot by number (strings and maps by content); each
reader's person block, answer and log. Checked against the original's run as well as against the host: a change of 5, 15 or 25 for Amount 0, 1, 2;
the excitement bands' 25, 15, 5 and the divisor 10; and, from the file's text with no reader involved, every scalar field by group and name.

Not tested: the level folder's and the easy game's Standard.sam loaded over this one (tests/test_BalanceTable.py parses them; none of them sets
any of these rows), and the online game's Online_Standard.sam, which has its own numbers. Nothing here plays a park.
"""
import ctypes as C
import re
import struct

import calllog as CL
import test_BalanceTable as BT
import test_CPerson_GetBagged as GB
import test_CPerson_TooPricey as TP
import test_CUI2DMap_CreateMapSprites as M
import tpwtest as T
from balance_support import UNITS
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BalanceRealValues"
MASK = 0xFFFFFFFF

STANDARD = BT.EXTRACTED / "disc-hfs" / "Theme Park Data" / "data" / "levels" / "Standard.sam"
GAME_PATH = "data:levels:Standard.sam"
MAIN = UNITS[1]
FN_RAISE, FN_LOWER, FN_AMOUNT, FN_EXTRA = GB.RAISE, GB.LOWER, GB.AMOUNT, TP.FN_EXTRA
BLOCK = 0x240

# What the original subtracts for each Amount, and adds for each excitement band (a difference below the first number gets the second).
CHANGES = {0: 5, 1: 15, 2: 25}
BANDS = ((5, 25), (15, 15), (40, 5))
DIVISOR = 10
HAPPINESS_SLOTS = {4: 5, 5: 15, 6: 25, 7: 25, 8: 15, 9: 5, 10: 10}
INT_KINDS, FLOAT_KINDS = (4, 5, 6), (7, 8, 9)

LOADED = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def setup(runner):
    T.install_std_stubs(runner.emu)
    BT.setup(runner)
    data = STANDARD.read_bytes()
    table = BT.REAL[1]
    LOADED["text"] = data
    LOADED["table"] = table
    LOADED["ppc"] = ppc_load(runner.emu, table, data)
    LOADED["host"] = host_load(runner.lib, table, data)


def queues(data):
    return {"LbFile_GetLength": [len(data)], "LbFile_Load": [0]}


def ppc_load(e, table, data):
    """As the game: the static initialiser's Init on the object at its own address, then the load."""
    saved = BT.STUBS.activate(e)
    try:
        e.stub("__register_global_object", lambda e: None)
        obj = MAIN["glob"]
        e.cl = CL.PpcLog({obj: BT.TABLE_LABEL}, {})
        e.file_bytes = data
        T.call_ppc(e, MAIN["sinit"], [])
        e.cl = CL.PpcLog({obj: BT.TABLE_LABEL, BT.EXIT_ERROR: BT.EXIT_LABEL}, queues(data))     # as the host's log, which starts after its Init
        st = dict(obj=obj, table=table, file=0, mark=0)
        return BT.ppc_op(e, st, ("load_path", GAME_PATH, 0))
    finally:
        CL.Stubs.deactivate(e, saved)


def host_load(lib, table, data):
    """The library's static initialiser has already run Init on gMainBalance."""
    h = BT.F(lib, "tpw_bt_main", C.c_void_p)()
    BT.F(lib, "tpw_bt_set_file_text", None, C.c_char_p, C.c_int32)(data, len(data))
    CL.host_begin(lib, {BT.TABLE_LABEL: h, BT.EXIT_LABEL: BT.F(lib, "tpw_bt_exit_error", C.c_void_p)()}, queues(data))
    try:
        return BT.host_op(lib, h, table, ("load_path", GAME_PATH, 0), dict(mark=0))
    finally:
        CL.host_end(lib)


# ================================================================== the file's text, read with no reader


def file_scalars(text):
    """{"Group.Field": token}: the last line that sets each plain field."""
    out = {}
    for line in re.split(rb"[\r\n]+", text):
        m = re.match(rb"\s*([A-Za-z_0-9]+\.[A-Za-z_0-9]+)\s+(\S+)", line)
        if m:
            out[m.group(1).decode("latin-1")] = m.group(2).decode("latin-1")
    return out


def text_mismatches(table, loaded, text):
    """Every plain integer or float field the file sets, against the slot the original filled."""
    slots_by_row, _ = BT.layout(table)
    words = loaded["slots"]
    named, members = {}, []
    for i in range(table.n):
        kind = table.rows[i][0]
        if kind in (0, 2):
            members = []
        elif kind == 1:
            for row in members:
                named["%s.%s" % (table.rows[i][1], table.rows[row][1])] = row
            members = []
        elif kind == 3:
            members = []
        elif 4 <= kind <= 11:
            members.append(i)
    out, checked = [], 0
    for name, token in file_scalars(text).items():
        row = named.get(name)
        if row is None or table.rows[row][0] not in INT_KINDS + FLOAT_KINDS:
            continue
        slot = slots_by_row[row][0]
        word = words[slot]
        checked += 1
        if table.rows[row][0] in INT_KINDS:
            if word != int(token) & MASK:     # the light levels are colours, written as unsigned
                out.append("%s: slot %d holds %08x, the file says %s" % (name, slot, word, token))
        elif word != fb(float(token)):
            out.append("%s: slot %d holds %08x, the file says %s" % (name, slot, word, token))
    return out, checked


# ================================================================== the loader


def load_ppc(e, case):
    return pick(LOADED["ppc"], case)


def load_host(lib, case):
    return pick(LOADED["host"], case)


def pick(loaded, case):
    if "slot" in case:
        return {"slot": loaded["slots"][case["slot"]]}
    return loaded


class LoadTest(FnTest):
    def run_case(self, runner, case):
        out = super().run_case(runner, case)
        if "slot" in case:
            return out
        loaded = LOADED["ppc"]
        if loaded["ret"] != 1 or any(n in ("GAME_ASSERTMSG", "CExitError::SetError") for n, _, _ in loaded["log"]):
            out.append("the original's load was not clean: ret %r, %r" % (loaded["ret"], loaded["log"]))
        wrong, checked = text_mismatches(LOADED["table"], loaded, LOADED["text"])
        if checked < 100:
            out.append("only %d fields read from the file's text" % checked)
        for slot, value in HAPPINESS_SLOTS.items():
            if T.s32(loaded["slots"][slot]) != value:
                wrong.append("slot %d holds %d, expected %d" % (slot, T.s32(loaded["slots"][slot]), value))
        return out + wrong


def load_edge():
    yield {"whole": True}
    for slot in range(LOADED_SLOTS):
        yield {"slot": slot}


def load_random(r):
    return {"slot": r.randrange(LOADED_SLOTS)}


# ================================================================== the happiness calls


class PersonSide:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class PpcPerson(PersonSide):
    fmt = ">I"

    def __init__(self, e, case, stubs):
        self.e = e
        self.obj = e.alloc(BLOCK)
        self.L = e.cl = CL.PpcLog({self.obj: 1, TP.OBJ: 2}, case.get("queues"))
        self.saved = stubs.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, case):
        op = case["op"]
        if op == "extra":
            T.call_ppc(self.e, FN_EXTRA, [self.obj, TP.OBJ])
            return 0
        r3, _, _ = T.call_ppc(self.e, {"lower": FN_LOWER, "raise": FN_RAISE, "amount": FN_AMOUNT}[op], [self.obj, case["amount"]])
        return r3 & MASK if op == "amount" else 0


class HostPerson(PersonSide):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_rv_alloc(BLOCK)
        CL.host_begin(lib, {1: self.obj, 2: TP.OBJ}, case.get("queues"))

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, case):
        op = case["op"]
        if op == "extra":
            self.lib.tpw_rv_extra(self.obj, TP.OBJ)
            return 0
        if op == "amount":
            return self.lib.tpw_rv_amount(self.obj, case["amount"]) & MASK
        getattr(self.lib, "tpw_rv_" + op)(self.obj, case["amount"])
        return 0


def bind(lib):
    if getattr(lib, "rv_bound", False):
        return
    lib.tpw_rv_alloc.restype, lib.tpw_rv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    for name in ("lower", "raise"):
        f = getattr(lib, "tpw_rv_" + name)
        f.restype, f.argtypes = None, [C.c_uint64, C.c_int32]
    lib.tpw_rv_amount.restype, lib.tpw_rv_amount.argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    lib.tpw_rv_extra.restype, lib.tpw_rv_extra.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.rv_bound = True


def run_person(s, case):
    try:
        s.words(s.obj + 0x19C, [case["happy"]])
        s.words(s.obj + 0x1A8, [case.get("f1a8", 0)])
        s.words(s.obj + 0x1B0, [case.get("f1b0", 0)])
        answer = s.call(case)
        return {"answer": answer, "log": s.log(), "person": s.rd_words(s.obj, BLOCK // 4)}
    finally:
        s.done()


def person_stubs(case):
    return TP.STUBS if case["op"] == "extra" else GB.STUBS


def person_ppc(e, case):
    return run_person(PpcPerson(e, case, person_stubs(case)), case)


def person_host(lib, case):
    return run_person(HostPerson(lib, case), case)


class PersonTest(FnTest):
    """Both sides alike, and the original's own run against the numbers the fix claims."""

    def run_case(self, runner, case):
        emu = runner.emu
        emu.reset_scratch()
        try:
            p = self.ppc(emu, case)
        except T.ppcemu.ImportCalled as ex:
            return ["PPC side called unstubbed import %s" % ex]
        return T.same_values(p, self.host(runner.lib, case), "", self.exact_nan) + expected(p, case)


def bits_to_f(bits):
    return struct.unpack(">f", struct.pack(">I", bits & MASK))[0]


def clamp(value):
    return fb(min(max(value, 0.0), 100.0))


def expected(result, case):
    op = case["op"]
    happy = bits_to_f(case["happy"])
    got = result["person"][0x19C // 4]
    if op == "amount":
        return [] if result["answer"] == CHANGES[case["amount"]] else ["original: amount %d answers %d" % (case["amount"], result["answer"])]
    if op == "lower":
        want = clamp(happy - CHANGES[case["amount"]])
    elif op == "raise":
        want = clamp(happy + CHANGES[case["amount"]])
    else:
        added = 0
        difference = abs(case["preferred"] - case["level"])
        for limit, value in BANDS:
            if difference < limit:
                added = value
                break
        if case["level"] == 0:
            added = 0
        want = clamp(happy + added)
        if case["level"] != 0:
            f1b0 = clamp(bits_to_f(case["f1b0"]) + (case["level"] // DIVISOR) * ((100 - int(bits_to_f(case["f1a8"]))) // 20))
            if result["person"][0x1B0 // 4] != f1b0:
                return ["original: +0x1b0 %08x, expected %08x" % (result["person"][0x1B0 // 4], f1b0)]
    return [] if got == want else ["original: happiness %08x, expected %08x" % (got, want)]


STARTS = (100.0, 99.5, 75.0, 60.0, 50.0, 30.5, 25.0, 24.5, 15.0, 14.5, 5.0, 4.5, 1.0, 0.0)


def amount_edge(op):
    def edge():
        for amount in CHANGES:
            for start in STARTS:
                yield {"op": op, "amount": amount, "happy": fb(start)}
    return edge


def amount_random(op):
    def rnd(r):
        return {"op": op, "amount": r.randrange(3), "happy": fb(r.uniform(0.0, 100.0))}
    return rnd


def extra_case(level, preferred, happy=50.0, f1a8=0.0, f1b0=0.0):
    # The fakes answer each excitement call with the level, as a ride does: three calls, or one when it is 0.
    return {"op": "extra", "level": level, "preferred": preferred, "happy": fb(happy), "f1a8": fb(f1a8), "f1b0": fb(f1b0),
            "queues": {"GetExcitementLevel": [level] * 3, "GetPreferredExcitement": [preferred]}}


def extra_edge():
    for difference in (0, 1, 4, 5, 6, 14, 15, 16, 39, 40, 41, 80):
        yield extra_case(100, 100 + difference)
        yield extra_case(100, 100 - difference)
    for level in (0, 1, 9, 10, 11, 99, 100, 255):
        yield extra_case(level, level)
        for f1a8 in (0.0, 19.0, 20.0, 50.0, 100.0):
            yield extra_case(level, level, f1a8=f1a8, f1b0=7.5)
    for happy in (0.0, 80.0, 90.0, 95.0, 100.0):
        for difference in (0, 5, 15, 40):
            yield extra_case(100, 100 + difference, happy=happy)


def extra_random(r):
    level = r.randrange(0, 256)
    return extra_case(level, r.randrange(0, 256), happy=r.uniform(0.0, 100.0), f1a8=float(r.randrange(0, 101)), f1b0=r.uniform(0.0, 100.0))


# ================================================================== the map


MAP_SCALES = {3: (0x600, 0x648), 4: (0x5D0,), 5: (0x618, 0x630)}


def real_table(case):
    case["bal"] = []        # the map test's world, with its balance words left to the loaded table
    return case


def map_edge():
    for case in M.edges():
        yield real_table(case)


def map_random(r):
    return real_table(M.make(r))


def map_host(lib, case):
    flag = C.c_int32.in_dll(lib, "tpw_rv_map_asserts")
    flag.value = 1
    try:
        return M.host(lib, case)
    finally:
        flag.value = 0


def map_scales(sprite_id):
    """0.5 times the slots this kind of sprite is scaled by, from the original's table (the others have a scale of their own)."""
    return {fb(0.5 * T.s32(LOADED["ppc"]["slots"][(off - 8) // 4])) for off in MAP_SCALES.get(sprite_id, ())}


class MapTest(FnTest):
    def run_case(self, runner, case):
        emu = runner.emu
        emu.reset_scratch()
        try:
            p = self.ppc(emu, case)
        except T.ppcemu.ImportCalled as ex:
            return ["PPC side called unstubbed import %s" % ex]
        out = T.same_values(p, self.host(runner.lib, case), "", self.exact_nan)
        for name, ints, _ in p["log"]:
            if name == "Ctor" and ints[2] in MAP_SCALES and ints[5] not in map_scales(ints[2]):
                out.append("original: sprite %d's scale %08x is not one of %r" % (ints[2], ints[5], sorted(map_scales(ints[2]))))
        return out


LOADED_SLOTS = MAIN["count"]
TESTS = [] if not STANDARD.exists() else [
    LoadTest("Standard.sam: original's load and ours, every slot", load_ppc, load_host, edge=load_edge, rnd=load_random),
    PersonTest("real table: CPerson::LowerHappiness", person_ppc, person_host, edge=amount_edge("lower"), rnd=amount_random("lower")),
    PersonTest("real table: CPerson::RaiseHappiness", person_ppc, person_host, edge=amount_edge("raise"), rnd=amount_random("raise")),
    PersonTest("real table: CPerson::GetHappinessAmount", person_ppc, person_host, edge=amount_edge("amount"), rnd=amount_random("amount")),
    PersonTest("real table: CPerson::ExtraExcitementEffects", person_ppc, person_host, edge=extra_edge, rnd=extra_random),
    MapTest("real table: CUI2DMap::CreateMapSprites", M.ppc, map_host, edge=map_edge, rnd=map_random),
]
