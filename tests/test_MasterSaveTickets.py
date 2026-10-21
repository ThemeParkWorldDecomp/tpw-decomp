"""Differential tests for the master save's ticket bookkeeping, the current theme and the park sign names (unit w89), against
libtpw_unit_MasterSaveTickets.dylib (tests/unit_MasterSaveTickets.cpp):

  src/game/save/FS_MasterSave.cpp   CFSGlobalMasterSave: ~CFSGlobalMasterSave, HowManyTickets, HowManyTicketsIncludingSecrets,
                                  SpendingPower, SpendTicketOn, HasBeenBought, HaveTicket (local, secret), EarnedTicket (local,
                                  global, secret), ChangeCurrentTheme, SetSignNameA, SetSignNameB, SetSwearFilter, IsSwearFilterOn,
                                  ClearThemeMap, CheckTicketTypeWon, HasAllResearchBeenCompleted,
                                  SetAllResearchCompletedInCurrentTheme, IsFirstTimePlayer, ClearFirstTimePlayer;
                                  CFSGlobalParkData: ~CFSGlobalParkData, HaveTicket, EarnedTicket, AwardInThisPark, ScoreForAward,
                                  ClearAward, SetAward
  src/game/save/FS_MasterSave.cpp     CFSGlobalParkData: GetSignNameA, GetSignNameB, SetSignNameA, SetSignNameB

A case is a world and a script. The world: eight themes whose balance files the world scripts (Load's answer, the key cost and the
gate object's number), a ride database with the rides the case gives (an id and two sign texts) and the ride name table's strings
(the real ones, read from the game's OBJECT_NAMES string table when it is there). A script is a list of operations that call the
unit's functions or change the world (the numbers are in OP below). After every operation the test compares the answer (a return
value or the output buffer, whole, so that a write past the size shows), the calls the fakes recorded during it, and the state: the
master save's bytes, bought set and theme map in order, the current theme's place in the map, every park record (tickets, awards
and scores, flags, names, sign texts), the sign work area, the name builder's buffers and the ride database's cache. At the end it
compares operator new's and delete's counts.

Real on both sides: the unit's functions, the master save's container code (the Metrowerks list, set and map instances run as the
original's on the PowerPC side, the standard containers on the host), the ride names, the C library's 16-bit string functions on the
host. Fakes that record the same event on both sides: the balance table (Init, UnInit, Load), the trace output and the string
table's FormatString. The PowerPC side decides which operations the state allows (a NULL or dangling pointer the original would read
is skipped on both sides).
"""
import ctypes as C
import re
import struct

import calllog as CL
import test_FSSystem as FS
import test_IslandLobby as IL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_MasterSaveTickets"

MINUS = 0xFFFFFFFF

# ---- the unit's functions (and the real helpers they call)
FN = dict(
    ms_dtor=0x1016997C, ms_tickets=0x10169B24, ms_incl=0x10169C24, ms_power=0x10169D68, ms_spend=0x10169FE8, ms_bought=0x1016A094,
    ms_have_local=0x1016A12C, ms_have_secret=0x1016A1A4, ms_earn_local=0x1016A200, ms_earn_global=0x1016A2A0,
    ms_earn_secret=0x1016A420, ms_change=0x1016B224, ms_sign_a=0x1016B31C, ms_sign_b=0x1016B388, ms_swear_set=0x1016B3F4,
    ms_swear_get=0x1016B438, ms_clear_map=0x1016B6DC, ms_check_won=0x1016B788, ms_research_all=0x1016B94C,
    ms_research_set=0x1016BA04, ms_first=0x1016BA70, ms_clear_first=0x1016BAB8,
    pd_dtor=0x1016BCD4, pd_have=0x1016C158, pd_earned=0x1016C1AC, pd_award_in=0x1016C220, pd_score=0x1016C280,
    pd_clear=0x1016C2F8, pd_set=0x1016C354, pd_signa=0x1016C7E8, pd_signb=0x1016CA34, pd_set_a=0x1016CBE8, pd_set_b=0x1016CC60)
# the unit's neighbours that run as the original (unit w32 and the ride names)
NEIGHBOURS = dict(
    ms_ctor=0x101694DC, ms_reset=0x10169A88, ms_keys=0x10169EA0, ms_getinfo=0x1016B47C, ms_create=0x1016B5A0,
    pd_ctor=0x1016BBC4, pd_tickets=0x1016C0E0, pd_cost=0x1016C3BC, pd_check=0x1016C43C, rdb_sign_a=0x10154C24,
    rdb_sign_b=0x10154D68, name_a=0x10157250, name_b=0x1015734C)
# the Metrowerks container code under them (the list clear, the tree's destroy, find, insert and rotations), real
REAL_HELPERS = [0x10152DB0, 0x10153B24, 0x10153DEC, 0x10153EFC, 0x1016B168, 0x1016CD78, 0x1016CF68, 0x1016D1F8, 0x1016D4E8,
                0x1016D63C, 0x1016D784, 0x1016DC80, 0x1016DFD0, 0x1016E0B8, 0x1016E37C, 0x1016E65C, 0x1016E8B0, 0x1016E988,
                0x1016E9C4, 0x1016EAF0, 0x1016EB98, 0x1016F080, 0x1016F164]
# the fakes (in-binary functions)
A = dict(bal_init=0x1001EA28, bal_uninit=0x1001ED40, bal_load=0x10020134, ctrace=0x1006FCBC)
# the globals
G = dict(master=0x10380A8C, ride_db=0x1037F6C8, text_table=0x1037FF18 + 0x59C, scratch_full=0x10380A4A, scratch_sign=0x10380A08)
SAVED = [(G["master"], 4), (G["ride_db"], 0x738), (G["text_table"], 4), (G["scratch_full"], 0x42), (G["scratch_sign"], 0x42)]
MISSING_STRING = 0x102A5CE2

# ---- the world (the same as the C++ file's)
THEMES = IL.THEMES
THEME_BALANCE = IL.THEME_BALANCE
SCRATCH = len(THEMES)
# the sign texts the setters take (index into this list): the two-hyphen rule needs names that end in "--", one hyphen, a lone
# hyphen, and the 33-character name (not terminated: it runs into the second line's array)
SIGNS = ["", "Spooky", "Park-", "Park--", "--", "-", "a", "a--", "Fun Land", "x" * 29 + "--", "y" * 30 + "--", "z" * 31 + "--",
         "0123456789012345678901234567890-", "ab-c--", "a-b"]
LAB_TABLE = 90
RIDES = 6
BUFFER_UNITS = 0x90
FIRST_NEW = 100
BOUGHT_SHOWN = 12
BUFFER_SIZES = [0, 1, 2, 8, 0x1F, 0x20, 0x21, 0x22, 0x40, 0x90]
# the ride database of a case: (number, sign text A, sign text B)
DEFAULT_RIDES = [(7777, "Seven", "Sevens"), (5555, "Dash--", ""), (6666, "", "Own B")]
GATES = [1150, 1190, 1180, 1100, 1605, 2601, 3601, 1601, 4601, 7777, 5555, 6666, 0x10000 + 1190, 1108]

# the operations (the C++ file's numbers)
OP = dict(ms_new=10, ms_reset=11, ms_getinfo=13, ms_create=14, pd_new=15, balance=34, globals=37, park_tickets=38, tickets=50,
          incl=51, power=52, spend=53, bought=54, have_local=55, have_secret=56, earned_local=57, earned_global=58,
          earned_secret=59, check_won=60, change=61, ms_sign_a=62, ms_sign_b=63, swear_set=64, swear_get=65, clear_map=66,
          research_all=67, research_set=68, first=69, clear_first=70, ms_delete=71, pd_have=72, pd_earned=73, pd_award_in=74,
          pd_score=75, pd_clear=76, pd_set=77, pd_signa=78, pd_signb=79, pd_set_a=80, pd_set_b=81, pd_delete=82, raw_save=83,
          raw_park=84, cur_null=85, cur_to=86, ms_delete0=87, pd_delete0=88)
# the operations that need a master save, a park of the map or the scratch park, and the current theme
MASTER_OPS = {OP[k] for k in ("ms_reset", "ms_getinfo", "ms_create", "globals", "tickets", "incl", "power", "spend", "bought",
                              "have_local", "have_secret", "earned_local", "earned_global", "earned_secret", "check_won", "change",
                              "ms_sign_a", "ms_sign_b", "swear_set", "swear_get", "clear_map", "research_all", "research_set",
                              "first", "clear_first", "ms_delete", "ms_delete0", "raw_save", "cur_null", "cur_to", "park_tickets", "pd_have",
                              "pd_earned", "pd_award_in", "pd_score", "pd_clear", "pd_set", "pd_signa", "pd_signb", "pd_set_a",
                              "pd_set_b", "raw_park")}
PARK_OPS = {OP[k] for k in ("park_tickets", "pd_have", "pd_earned", "pd_award_in", "pd_score", "pd_clear", "pd_set", "pd_signa",
                            "pd_signb", "pd_set_a", "pd_set_b", "raw_park", "cur_to")}
NEEDS_CURRENT = {OP[k] for k in ("have_local", "earned_local", "earned_global")}
NEEDS_CURRENT_OR_NULL = {OP[k] for k in ("ms_sign_a", "ms_sign_b", "research_set")}
TEXT_OPS = {OP["ms_sign_a"]: 0, OP["ms_sign_b"]: 0, OP["pd_set_a"]: 1, OP["pd_set_b"]: 1}


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MINUS))


# ======================================================================= the PowerPC side's fakes


def build_stubs(emu):
    S = CL.Stubs()
    S.real.update(FN.values())
    S.real.update(NEIGHBOURS.values())
    S.real.update(REAL_HELPERS)
    FS.string_stubs(S)
    U = lambda e: e.us   # noqa: E731  (the case's world)

    def ctrace(e):
        L = e.cl
        fmt = e.cstr(e.r(4))
        if "%s" in fmt:
            L.rec("CTRACE", (e.r(3),), (fmt[:159], e.cstr(e.r(5))[:159]))
        else:
            L.rec("CTRACE", (e.r(3),), (fmt[:159],))

    def bal_load(e):
        L = e.cl
        this, path = e.r(3), e.cstr(e.r(4))
        L.rec("Balance.Load", (L.lab(this), T.s32(e.r(5))), (path[:159],))
        found = re.fullmatch(r"data:levels:(.*):global\.sam", path)
        theme = THEMES.index(found.group(1)) if found and found.group(1) in THEMES else -1
        balance = U(e).balance[theme] if theme >= 0 else (0, 0, 0)
        if balance[0] == 0:
            e.set_r(3, 0)
            return
        w32(e, this + 0x14, balance[1])
        w32(e, this + 0x18, balance[2])
        e.set_r(3, 1)

    R = CL.recorder
    S.internal(A["bal_init"], lambda e: e.cl.rec("Balance.Init", (e.cl.newlab(e.r(3)),)))
    S.internal(A["bal_uninit"], R("Balance.UnInit", "L"))
    S.internal(A["bal_load"], bal_load)
    S.internal(A["ctrace"], ctrace)

    # the C library: the 16-bit string functions and swprintf
    def wcslen(e):
        e.set_r(3, len(FS.wide_read(e, e.r(3), 4000)))

    def wcsncpy(e):
        dst, src, count = e.r(3), e.r(4), e.r(5)
        text = FS.wide_read(e, src, 4000)[:count]
        e.write(dst, IL.wide_bytes(text)[:-2].ljust(2 * count, b"\0") if count else b"")
        e.set_r(3, dst)

    def swprintf(e):
        out, size, fmt = e.r(3), e.r(4), FS.wide_read(e, e.r(5), 4000)
        text = IL.wformat(e, fmt, 6)
        if size:
            kept = text[:size - 1]
            e.write(out, IL.wide_bytes(kept))
        e.set_r(3, T.u32(len(text) if len(text) <= size - 1 and size else -1))

    def format_string(e):
        L = e.cl
        this, out, size, sid = e.r(3), e.r(4), e.r(5), e.r(6)
        L.rec("FormatString", (L.lab(this), size, sid))
        text = U(e).strings.get(sid, "") if sid < 2048 else U(e).strings[0]
        if size == 0:
            e.set_r(3, 0)
            return
        kept = text[:size - 1]
        e.write(out, IL.wide_bytes(kept))
        e.set_r(3, len(kept))

    S.imp("wcslen", wcslen)
    S.imp("wcsncpy", wcsncpy)
    S.imp("swprintf", swprintf)
    S.imp("FormatString__8TbITableCFPwUiUle", format_string)
    S.imp("__lt__15TbStringBase<c>CFRC15TbStringBase<c>",
          lambda e: e.set_r(3, int(FS.s_read(e, e.r(3), False) < FS.s_read(e, e.r(4), False))))

    # the heap: counted per case; a delete of a labelled object is an event
    def op_new(e):
        us = U(e)
        block = e.alloc(max(e.r(3), 4), b"\xa5" * max(e.r(3), 4))
        us.heap[0] += 1
        e.set_r(3, block)

    def op_delete(e):
        L, us = e.cl, U(e)
        block = e.r(3)
        if block:
            us.heap[2] += 1
            if L.lab(block) > 0:
                L.rec("delete", (L.lab(block),))
                L.forget(block)

    S.imp("__nw__FUl", op_new)
    S.imp("__dl__FPv", op_delete)
    return S


# ======================================================================= the PowerPC side's world


class Ppc:
    """One case on the PowerPC side: the world and the operations."""

    def __init__(self, e, case):
        self.e = e
        e.us = self
        self.case = case
        self.heap = [0, 0, 0, 0]
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in SAVED}
        self.balance = list(THEME_BALANCE)
        self.strings = dict(IL.base_strings())
        self.strings.update(case.get("strings", {}))
        self.table = e.alloc(16)
        self.rides = []
        self.ride_ids = []
        for rid, sign_a, sign_b in case.get("rides", DEFAULT_RIDES):
            block = e.alloc(0x800, b"\xa5" * 0x800)
            IL.w16(e, block, rid)
            w32(e, block + 0x7F0, 0)
            w32(e, block + 0x7F4, e.alloc(len(IL.wide_bytes(sign_a)), IL.wide_bytes(sign_a)))
            w32(e, block + 0x7F8, e.alloc(len(IL.wide_bytes(sign_b)), IL.wide_bytes(sign_b)))
            self.rides.append(block)
            self.ride_ids.append(rid & 0xFFFF)
        self.L = e.cl = CL.PpcLog({self.table: LAB_TABLE}, {}, FIRST_NEW)
        for key in ("master",):
            w32(e, G[key], 0)
        e.write(G["ride_db"], bytes(0x738))
        for i, block in enumerate(self.rides):
            w32(e, G["ride_db"] + 4 * i, block)
        w32(e, G["ride_db"] + 0x528, len(self.rides))
        w32(e, G["text_table"], self.table)
        e.write(G["scratch_full"], bytes(0x42))
        e.write(G["scratch_sign"], bytes(0x42))
        self.scratch_park = 0
        self.buffer = e.alloc(2 * BUFFER_UNITS)

    def restore(self):
        for (a, n), data in self.saved.items():
            self.e.write(a, data)

    # ---- helpers
    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def master(self):
        return self.e.u32(G["master"])

    def walk(self, root):
        """The nodes of a Metrowerks tree in order (left, node, right)."""
        e, out = self.e, []

        def go(node):
            if node:
                go(e.u32(node))
                out.append(node)
                go(e.u32(node + 4))
        go(root)
        return out

    def tree(self):
        """The master save's theme map in order: [(name, record)]."""
        e = self.e
        if not self.master():
            return []
        return [(FS.s_read(e, node + 0xC, False), e.u32(node + 0x20)) for node in self.walk(e.u32(self.master() + 0x38))]

    def bought(self):
        """The things bought, in order."""
        e = self.e
        if not self.master():
            return []
        return [struct.unpack(">H", e.read(node + 0xC, 2))[0] for node in self.walk(e.u32(self.master() + 0x2C))]

    def park(self, which):
        if which == SCRATCH:
            return self.scratch_park
        if not 0 <= which < len(THEMES) or not self.master():
            return 0
        for name, record in self.tree():
            if name == THEMES[which]:
                return record
        return 0

    def current(self):
        """null, valid (a record of the map or the scratch park) or bad (garbage or freed)."""
        cur = self.e.u32(self.master() + 0x44)
        if cur == 0:
            return "null"
        records = {record for _, record in self.tree()}
        if self.scratch_park:
            records.add(self.scratch_park)
        return "valid" if cur in records else "bad"

    def name_string(self, text):
        return FS.new_string(self.e, text, False)

    def text(self, index):
        data = IL.wide_bytes(SIGNS[index % len(SIGNS)])
        return self.e.alloc(len(data), data)

    def fill_buffer(self):
        self.e.write(self.buffer, b"\xaa\xaa" * BUFFER_UNITS)

    def read_buffer(self):
        return list(struct.unpack(">%dH" % BUFFER_UNITS, self.e.read(self.buffer, 2 * BUFFER_UNITS)))

    def new_object(self, size):
        self.heap[0] += 1
        return self.e.alloc(size, b"\xa5" * size)

    def can_name(self, park, second):
        """Whether GetSignNameA/B can run without reading what the host cannot: names that end inside their arrays, a hyphen cut that
        stays inside the work area, a gate object the ride database or the name table knows."""
        e = self.e
        if e.read(park + 0xB8, 1)[0] != 0:
            both = struct.unpack(">66H", e.read(park + 0x34, 132))
            if second:
                return 0 in both[33:]
            if 0 not in both:
                return False
            length = both.index(0)
            return not (length > 0x21 and length > 1 and both[length - 1] == 0x2D and both[length - 2] == 0x2D)
        bal = e.u32(park + 0x1C)
        if bal:
            gate = e.u32(bal + 0x18) & 0xFFFF
        else:
            name = e.cstr(e.u32(park + 0x28))
            if name not in THEMES or self.balance[THEMES.index(name)][0] == 0:
                return True
            gate = self.balance[THEMES.index(name)][2] & 0xFFFF
        known = {row[0] for row in IL.NAME_TABLE}
        return gate in known or (gate != 0 and gate in self.ride_ids)

    # ---- the dump (the C++ file's order)
    def park_fields(self, park):
        e = self.e
        if park == 0:
            return [0]
        v = [1] + list(e.read(park, 6)) + list(e.read(park + 6, 4)) + list(struct.unpack(">4i", e.read(park + 0xC, 16)))
        v += [int(e.u32(park + 0x1C) != 0), e.read(park + 0xB8, 1)[0], e.read(park + 0xB9, 1)[0]]
        v += IL.codes(e.cstr(e.u32(park + 0x28)), 12)
        # (both sign arrays whole, 33 units each: what follows a terminator and the last unit show)
        v += list(struct.unpack(">33H", e.read(park + 0x34, 66))) + list(struct.unpack(">33H", e.read(park + 0x76, 66)))
        return v

    def dump_state(self):
        e = self.e
        master = self.master()
        v = [int(master != 0)]
        if master:
            v += list(e.read(master + 0x18, 4)) + [T.s32(e.u32(master + 0x1C)), T.s32(e.u32(master + 0x20)),
                                                   e.read(master + 0x24, 1)[0], e.read(master + 0x25, 1)[0],
                                                   e.read(master + 0x26, 1)[0], e.read(master + 0x27, 1)[0],
                                                   e.read(master + 0x48, 1)[0]]
            bought = self.bought()
            v += [len(bought)] + bought[:BOUGHT_SHOWN]
            tree = self.tree()
            v += [len(tree)]
            cur = e.u32(master + 0x44)
            place = -1 if cur == 0 else -2
            for index, (_, record) in enumerate(tree):
                if record == cur:
                    place = index
            v += [place]
            for name, record in tree:
                v += IL.codes(name, 12) + self.park_fields(record)
        v += self.park_fields(self.scratch_park)
        v += list(struct.unpack(">33H", e.read(G["scratch_sign"], 66)))
        db = G["ride_db"] + 0x52C
        for offset in (0, 0x80, 0x100):
            v += IL.codes(FS.wide_read(e, db + offset), 24)
        v += [struct.unpack(">H", e.read(G["ride_db"] + 0x730, 2))[0]]
        cached = e.u32(G["ride_db"] + 0x734)
        v += [self.rides.index(cached) if cached in self.rides else -1]
        return v

    # ---- the operations: the answer, or None when the state does not allow it
    def op(self, code, a):
        e = self.e
        if code == OP["ms_new"]:
            block = self.new_object(0x4C)
            self.call(FN_CTOR, block)
            w32(e, G["master"], block)
            return []
        if code == OP["balance"]:
            self.balance[a[0]] = (a[1], a[2], a[3])
            return []
        if code == OP["pd_new"]:
            block = self.new_object(0xBC)
            self.call(NEIGHBOURS["pd_ctor"], block, self.name_string(THEMES[a[0]]))
            self.scratch_park = block
            return []
        if code == OP["pd_delete"]:
            if not self.scratch_park:
                return None
            self.call(FN["pd_dtor"], self.scratch_park, 1)
            self.scratch_park = 0
            return []
        if code == OP["pd_delete0"]:
            # the destructor's first part (the flag 0): the object is taken apart and its block is left alone
            if not self.scratch_park:
                return None
            self.call(FN["pd_dtor"], self.scratch_park, 0)
            self.scratch_park = 0
            return []
        master = self.master()
        if code in MASTER_OPS and not master:
            return None
        if code in PARK_OPS and not self.park(a[0]):
            return None
        if code in NEEDS_CURRENT and self.current() != "valid":
            return None
        if code in NEEDS_CURRENT_OR_NULL and self.current() == "bad":
            return None
        park = self.park(a[0]) if code in PARK_OPS or code in (OP["pd_signa"], OP["pd_signb"]) else 0
        c = self.call
        if code == OP["ms_reset"]:
            c(NEIGHBOURS["ms_reset"], master)
            return []
        if code in (OP["ms_getinfo"], OP["ms_create"]):
            name = self.name_string(THEMES[a[0]])
            made = c(NEIGHBOURS["ms_getinfo" if code == OP["ms_getinfo"] else "ms_create"], master, name)
            if code == OP["ms_create"]:
                return [int(made != 0 and made == self.park(a[0]))]
            return [int(made == 0), int(made != 0 and made == self.park(a[0]))]
        if code == OP["globals"]:
            e.write(master + 0x18, bytes((a[0] >> i) & 1 for i in range(4)))
            e.write(master + 0x26, bytes((a[1] >> i) & 1 for i in range(2)))
            w32(e, master + 0x20, a[2])
            w32(e, master + 0x1C, a[3])
            return []
        if code == OP["park_tickets"]:
            e.write(park, bytes((a[1] >> i) & 1 for i in range(6)))
            e.write(park + 6, bytes((a[2] >> i) & 1 for i in range(4)))
            return []
        if code == OP["tickets"]:
            return [T.s32(c(FN["ms_tickets"], master))]
        if code == OP["incl"]:
            return [T.s32(c(FN["ms_incl"], master))]
        if code == OP["power"]:
            return [T.s32(c(FN["ms_power"], master))]
        if code == OP["spend"]:
            return [int(c(FN["ms_spend"], master, a[0] & 0xFFFF, a[1]) & 0xFF != 0)]
        if code == OP["bought"]:
            return [int(c(FN["ms_bought"], master, a[0] & 0xFFFF) & 0xFF != 0)]
        if code == OP["have_local"]:
            return [int(c(FN["ms_have_local"], master, a[0]) & 0xFF != 0)]
        if code == OP["have_secret"]:
            return [int(c(FN["ms_have_secret"], master, a[0]) & 0xFF != 0)]
        if code == OP["earned_local"]:
            return [T.s32(c(FN["ms_earn_local"], master, a[0]))]
        if code == OP["earned_global"]:
            return [T.s32(c(FN["ms_earn_global"], master, a[0], a[1]))]
        if code == OP["earned_secret"]:
            return [T.s32(c(FN["ms_earn_secret"], master, a[0]))]
        if code == OP["check_won"]:
            return [T.s32(c(FN["ms_check_won"], master))]
        if code == OP["change"]:
            c(FN["ms_change"], master, self.name_string(THEMES[a[0]]))
            return []
        if code in (OP["ms_sign_a"], OP["ms_sign_b"]):
            c(FN["ms_sign_a" if code == OP["ms_sign_a"] else "ms_sign_b"], master, self.text(a[0]))
            return []
        if code == OP["swear_set"]:
            c(FN["ms_swear_set"], master, int(a[0] != 0))
            return []
        if code == OP["swear_get"]:
            return [int(c(FN["ms_swear_get"], master) & 0xFF != 0)]
        if code == OP["clear_map"]:
            c(FN["ms_clear_map"], master)
            return []
        if code == OP["research_all"]:
            return [int(c(FN["ms_research_all"], master) & 0xFF != 0)]
        if code == OP["research_set"]:
            c(FN["ms_research_set"], master)
            return []
        if code == OP["first"]:
            return [int(c(FN["ms_first"], master) & 0xFF != 0)]
        if code == OP["clear_first"]:
            c(FN["ms_clear_first"], master)
            return []
        if code == OP["ms_delete"]:
            c(FN["ms_dtor"], master, 1)
            w32(e, G["master"], 0)
            return []
        if code == OP["ms_delete0"]:
            c(FN["ms_dtor"], master, 0)
            w32(e, G["master"], 0)
            return []
        if code == OP["pd_have"]:
            return [int(c(FN["pd_have"], park, a[1]) & 0xFF != 0)]
        if code == OP["pd_earned"]:
            return [int(c(FN["pd_earned"], park, a[1]) & 0xFF != 0)]
        if code == OP["pd_award_in"]:
            return [int(c(FN["pd_award_in"], park, a[1]) & 0xFF != 0)]
        if code == OP["pd_score"]:
            return [T.s32(c(FN["pd_score"], park, a[1]))]
        if code == OP["pd_clear"]:
            c(FN["pd_clear"], park, a[1])
            return []
        if code == OP["pd_set"]:
            c(FN["pd_set"], park, a[1], a[2])
            return []
        if code in (OP["pd_signa"], OP["pd_signb"]):
            if not self.can_name(park, code == OP["pd_signb"]):
                return None
            self.fill_buffer()
            c(FN["pd_signa" if code == OP["pd_signa"] else "pd_signb"], park, self.buffer, a[1])
            return self.read_buffer()
        if code in (OP["pd_set_a"], OP["pd_set_b"]):
            c(FN["pd_set_a" if code == OP["pd_set_a"] else "pd_set_b"], park, self.text(a[1]))
            return []
        if code == OP["raw_save"]:
            idx = a[0]
            offset = master + (0x18 + idx if idx < 4 else 0x22 + idx if idx < 6 else 0x25 if idx == 6 else 0x48 if idx == 7 else 0x24)
            e.write(offset, bytes([a[1] & 0xFF]))
            return []
        if code == OP["raw_park"]:
            kind, idx, value = a[1], a[2], a[3]
            if kind == 0:
                e.write(park + idx, bytes([value & 0xFF]))
            elif kind == 1:
                e.write(park + 6 + idx, bytes([value & 0xFF]))
            elif kind == 2:
                w32(e, park + 0xC + 4 * idx, value)
            elif kind == 3:
                e.write(park + 0xB9, bytes([value & 0xFF]))
            else:
                e.write(park + 0xB8, bytes([value & 0xFF]))
            return []
        if code == OP["cur_null"]:
            w32(e, master + 0x44, 0)
            return []
        if code == OP["cur_to"]:
            w32(e, master + 0x44, park)
            return []
        raise ValueError(code)


FN_CTOR = NEIGHBOURS["ms_ctor"]


# ======================================================================= the host side


def open_host(lib):
    if getattr(lib, "_ms_open", False):
        return
    lib.tpw_ms_op.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_ms_dump.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ms_ride.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_uint16), C.POINTER(C.c_uint16)]
    lib.tpw_ms_string.argtypes = [C.c_int32, C.POINTER(C.c_uint16)]
    lib.tpw_ms_text.argtypes = [C.POINTER(C.c_uint16)]
    lib.tpw_ms_heap.argtypes = [C.POINTER(C.c_int64)]
    lib._ms_open = True


def utf16(text):
    return (C.c_uint16 * (len(text) + 1))(*[ord(c) for c in text], 0)


class Host:
    def __init__(self, lib, case, base):
        self.lib = lib
        open_host(lib)
        CL.host_begin(lib, {}, {}, FIRST_NEW)
        lib.tpw_ms_reset()
        self.overrides = case.get("strings", {})
        for sid, text in self.overrides.items():
            lib.tpw_ms_string(sid, utf16(text))
        for i, (rid, sign_a, sign_b) in enumerate(case.get("rides", DEFAULT_RIDES)):
            lib.tpw_ms_ride(i, rid, utf16(sign_a), utf16(sign_b))
        self.base = base
        buf = (C.c_int64 * 4)()
        lib.tpw_ms_heap(buf)
        self.heap0 = list(buf)

    def op(self, code, a):
        if code in TEXT_OPS:
            self.lib.tpw_ms_text(utf16(SIGNS[a[TEXT_OPS[code]] % len(SIGNS)]))
        args = (C.c_int64 * 8)(*a, *([0] * (8 - len(a))))
        out = (C.c_int64 * 4096)()
        self.lib.tpw_ms_op(code, args, out)
        return list(out[1:1 + out[0]])

    def dump_state(self):
        out = (C.c_int64 * 4096)()
        self.lib.tpw_ms_dump(out)
        return list(out[1:1 + out[0]])

    def take_log(self):
        """The events recorded since the last call (the log restarts)."""
        n = C.c_int32.in_dll(self.lib, "tpw_cl_n").value
        log = (CL.TpwCallEvent * 4096).in_dll(self.lib, "tpw_cl_log")
        events = []
        for i in range(min(n, 4096)):
            ev = log[i]
            events.append((ev.name.decode("latin-1"), tuple(ev.ints[k] for k in range(ev.nints)),
                           tuple(ev.texts[k].value.decode("latin-1") for k in range(ev.ntexts))))
        if n > 4096:
            events.append(("OVERFLOW", (n,), ()))
        C.c_int32.in_dll(self.lib, "tpw_cl_n").value = 0
        return events

    def heap(self):
        buf = (C.c_int64 * 4)()
        self.lib.tpw_ms_heap(buf)
        return [buf[i] - self.heap0[i] for i in range(4)]

    def finish(self):
        C.c_int32.in_dll(self.lib, "tpw_cl_on").value = 0
        for sid in self.overrides:
            self.lib.tpw_ms_string(sid, utf16(self.base.get(sid, "")))


# ======================================================================= running a script

RAN = {}
STUBS = None
BASE = {}


def with_stubs(fn):
    def run(e, case):
        saved = STUBS.activate(e)
        try:
            return fn(e, case)
        finally:
            CL.Stubs.deactivate(e, saved)
    return run


@with_stubs
def ms_ppc(e, case):
    world = Ppc(e, case)
    steps, ran = [], []
    try:
        for code, a in case["ops"]:
            answer = world.op(code, a)
            if answer is None:
                continue
            ran.append((code, a))
            log, world.L.log = list(world.L.log), []
            steps.append(dict(op=code, answer=answer, log=log, state=world.dump_state()))
        RAN[id(case)] = ran
        return dict(steps=steps, heap=[world.heap[0], world.heap[2]])
    finally:
        world.restore()


def ms_host(lib, case):
    host = Host(lib, case, BASE)
    steps = []
    try:
        for code, a in RAN.pop(id(case)):
            answer = host.op(code, a)
            log = host.take_log()
            steps.append(dict(op=code, answer=answer, log=log, state=host.dump_state()))
        heap = host.heap()
    finally:
        host.finish()
    return dict(steps=steps, heap=[heap[0], heap[2]])


def setup(runner):
    global STUBS, BASE
    e = runner.emu
    T.install_std_stubs(e)
    if not FS.VT:
        FS.make_vtables(e)
    IL.load_name_table(e)
    STUBS = build_stubs(e)
    BASE = IL.base_strings()
    lib = runner.lib
    open_host(lib)
    for sid, text in BASE.items():
        lib.tpw_ms_string(sid, utf16(text))


# ======================================================================= scripts

TESTS = []


def o(name, *args):
    return (OP[name], list(args))


def case_of(ops, rides=None, strings=None):
    case = dict(ops=ops)
    if rides is not None:
        case["rides"] = rides
    if strings:
        case["strings"] = strings
    return case


def themes(n, first=0):
    """A master save with the first n themes read."""
    return [o("ms_new")] + [o("ms_getinfo", t % len(THEMES)) for t in range(first, first + n)]


def tickets_setup(globals_mask, secrets_mask, extra, spent, park_masks, awards=0):
    ops = [o("globals", globals_mask, secrets_mask, extra, spent)]
    for t, mask in enumerate(park_masks):
        ops.append(o("park_tickets", t, mask, awards))
    return ops


# ---- HowManyTickets, HowManyTicketsIncludingSecrets, SpendingPower

def count_edges():
    yield case_of([o("ms_new"), o("tickets"), o("incl"), o("power")])
    for g in range(16):
        for s in range(4):
            ops = themes(3) + tickets_setup(g, s, 0, g, [(1 << (g % 7)) - 1, 0x2A, (s * 21) % 64]) + [o("tickets"), o("incl"), o("power")]
            yield case_of(ops)
    # parks whose balance file cannot be read do not count (and are asked again every time)
    ops = themes(4) + tickets_setup(5, 2, 0, 3, [0x3F, 0x3F, 0x3F, 0x3F]) + [o("tickets"), o("incl"), o("power")]
    ops += [o("balance", 1, 0, 0, 0), o("balance", 2, 0, 0, 0), o("clear_map"), o("ms_getinfo", 3)]
    yield case_of(ops)
    ops = [o("balance", 1, 0, 0, 0), o("balance", 3, 0, 0, 0)] + themes(4) + tickets_setup(15, 3, 0, 0, [0x3F, 0x3F, 0x3F, 0x3F])
    ops += [o("tickets"), o("incl"), o("power"), o("balance", 1, 1, 2, 1190), o("tickets"), o("incl"), o("power")]
    yield case_of(ops)
    # the parks are only created, not read: ms_create makes a record without a balance table, which the counts then read
    ops = [o("ms_new")] + [o("ms_create", t) for t in range(5)] + tickets_setup(3, 1, 0, 1, [0x3F, 1, 2, 4, 8])
    ops += [o("tickets"), o("incl"), o("power")]
    yield case_of(ops)
    # what was spent and the counts: negative, large, wrapping
    for spent in (0, 1, 5, -1, -7, 0x7FFFFFFF, -0x80000000, 100):
        yield case_of(themes(2) + tickets_setup(15, 3, 0, spent, [0x3F, 0x3F]) + [o("power"), o("incl")])
    # tickets that are not 0 or 1 count as one
    ops = themes(2) + [o("raw_save", 0, 2), o("raw_save", 1, 255), o("raw_save", 4, 7), o("raw_save", 5, 128),
                       o("raw_park", 0, 0, 0, 2), o("raw_park", 0, 1, 3, 255), o("raw_park", 1, 0, 0, 2), o("tickets"), o("incl"), o("power")]
    yield case_of(ops)


def count_random(r):
    ops = themes(r.randrange(0, 8), r.randrange(0, 3))
    for _ in range(r.randrange(2, 10)):
        k = r.random()
        if k < 0.2:
            ops.append(o("globals", r.randrange(16), r.randrange(4), r.choice([0, 1, 2, -3]), r.choice([0, 1, 4, -2, 0x7FFFFFFF])))
        elif k < 0.4:
            ops.append(o("park_tickets", r.randrange(8), r.randrange(64), r.randrange(16)))
        elif k < 0.5:
            ops.append(o("balance", r.randrange(8), r.randrange(2), r.choice([0, 1, 2, 3]), r.choice(GATES)))
        elif k < 0.55:
            ops.append(o("ms_getinfo", r.randrange(8)))
        elif k < 0.6:
            ops.append(o("raw_save", r.randrange(6), r.choice([0, 1, 2, 255])))
        else:
            ops.append(o(r.choice(["tickets", "incl", "power"])))
    return case_of(ops)


# ---- SpendTicketOn, HasBeenBought

THINGS = [0, 1, 2, 3, 0x7FFF, 0x8000, 0xFFFF, 100, 101, 1150, 1190]
COSTS = [0, 1, 2, 5, -1, -9, 100, 0x7FFFFFFF, -0x80000000, 0x40000000]


def spend_edges():
    yield case_of([o("ms_new"), o("bought", 5), o("spend", 5, 3), o("bought", 5), o("spend", 5, 3), o("spend", 5, 10), o("bought", 6)])
    for thing in THINGS:
        yield case_of([o("ms_new"), o("bought", thing), o("spend", thing, 4), o("bought", thing), o("spend", thing, 4), o("power")])
    for cost in COSTS:
        yield case_of([o("ms_new"), o("spend", 1, cost), o("spend", 2, cost), o("spend", 2, cost), o("spend", 3, 0x7FFFFFFF)])
    # many things (the set is a balanced tree: the order of insertion decides its shape)
    yield case_of([o("ms_new")] + [o("spend", t, 1) for t in (5, 3, 8, 1, 4, 7, 9, 2, 6, 10, 0, 11, 15, 13, 12, 14)] + [o("bought", 14), o("bought", 99)])
    yield case_of([o("ms_new")] + [o("spend", t, 1) for t in range(20)] + [o("bought", 0), o("bought", 19), o("bought", 20)])
    yield case_of([o("ms_new")] + [o("spend", 20 - t, 2) for t in range(20)] + [o("bought", 1), o("ms_reset"), o("bought", 1), o("spend", 1, 1)])
    # spending with tickets to spend
    yield case_of(themes(2) + tickets_setup(15, 3, 0, 0, [0x3F, 0x3F]) + [o("power"), o("spend", 9, 4), o("power"), o("spend", 9, 4), o("power")])


def spend_random(r):
    ops = [o("ms_new")]
    for _ in range(r.randrange(3, 30)):
        k = r.random()
        if k < 0.5:
            ops.append(o("spend", r.choice(THINGS + [r.randrange(0, 40)]), r.choice(COSTS)))
        elif k < 0.8:
            ops.append(o("bought", r.choice(THINGS + [r.randrange(0, 40)])))
        elif k < 0.9:
            ops.append(o("power"))
        else:
            ops.append(o("ms_reset"))
    return case_of(ops)


# ---- HaveTicket and EarnedTicket (local, secret), CheckTicketTypeWon

def local_edges():
    for current in range(3):
        for t in range(6):
            ops = themes(3) + [o("change", current), o("have_local", t), o("earned_local", t), o("have_local", t), o("earned_local", t),
                               o("tickets"), o("have_local", (t + 1) % 6)]
            yield case_of(ops)
    # a byte that is neither 0 nor 1: won again and set to 1; the park's own answer
    ops = themes(2) + [o("change", 1), o("raw_park", 1, 0, 2, 2), o("have_local", 2), o("earned_local", 2), o("have_local", 2),
                       o("raw_park", 1, 0, 3, 255), o("earned_local", 3), o("earned_local", 3)]
    yield case_of(ops)
    # the answers of CheckTicketTypeWon through the local tickets: three tickets make a key, a park costing that many keys opens
    for cost in (0, 1, 2, 3, 4):
        ops = themes(3) + [o("balance", 1, 1, cost, 1190), o("change", 1)]
        ops += [o("earned_local", t) for t in range(6)] + [o("tickets"), o("check_won")]
        yield case_of(ops)
    ops = themes(3) + [o("change", 0), o("raw_save", 0, 1), o("earned_local", 0), o("earned_local", 1), o("earned_local", 2)]
    yield case_of(ops)
    yield case_of([o("ms_new"), o("have_local", 0), o("change", 2), o("earned_local", 4)])


def secret_edges():
    for t in range(2):
        yield case_of([o("ms_new"), o("have_secret", t), o("earned_secret", t), o("have_secret", t), o("earned_secret", t), o("tickets"), o("incl")])
    # secret tickets that are 2: won again
    yield case_of([o("ms_new"), o("raw_save", 4, 2), o("have_secret", 0), o("earned_secret", 0), o("raw_save", 5, 1), o("earned_secret", 1)])
    # the keys: global, local and secret tickets together
    for g in range(16):
        ops = themes(3) + [o("globals", g, 0, 0, 0), o("park_tickets", 0, g % 64, 0), o("earned_secret", 0), o("earned_secret", 1), o("check_won")]
        yield case_of(ops)
    for extra in (0, 1, 2, -1, -3):
        ops = themes(2) + [o("globals", 7, 0, extra, 0), o("earned_secret", 0), o("earned_secret", 1), o("check_won")]
        yield case_of(ops)


def local_random(r):
    ops = themes(r.randrange(1, 6)) + [o("change", r.randrange(4))]
    for _ in range(r.randrange(3, 25)):
        k = r.random()
        if k < 0.3:
            ops.append(o("earned_local", r.randrange(6)))
        elif k < 0.4:
            ops.append(o("earned_secret", r.randrange(2)))
        elif k < 0.5:
            ops.append(o("have_local", r.randrange(6)))
        elif k < 0.55:
            ops.append(o("have_secret", r.randrange(2)))
        elif k < 0.65:
            ops.append(o("change", r.randrange(8)))
        elif k < 0.75:
            ops.append(o("balance", r.randrange(8), r.randrange(2), r.choice([0, 1, 2, 3, 4, 5]), r.choice(GATES)))
        elif k < 0.85:
            ops.append(o("globals", r.randrange(16), r.randrange(4), r.choice([0, 0, 1, -1]), r.randrange(3)))
        elif k < 0.9:
            ops.append(o("raw_park", r.randrange(8), 0, r.randrange(6), r.choice([0, 1, 2])))
        elif k < 0.95:
            ops.append(o("check_won"))
        else:
            ops.append(o("ms_getinfo", r.randrange(8)))
    return case_of(ops)


# ---- CheckTicketTypeWon

def check_edges():
    yield case_of([o("ms_new"), o("check_won")])
    # every total of tickets from 0 to 12 across the global, local and secret ones, with 0..2 extra keys
    for extra in (0, 1, 2, -1, -2, -4):
        for total in range(0, 13):
            g = min(total, 4)
            s = min(max(total - g, 0), 2)
            local = max(total - g - s, 0)
            ops = themes(3) + [o("globals", (1 << g) - 1, (1 << s) - 1, extra, 0)]
            ops += [o("park_tickets", t, (1 << min(6, max(0, local - 6 * t))) - 1, 0) for t in range(2)]
            ops += [o("check_won")]
            yield case_of(ops)
    # a park costs exactly the keys: the first park in the map's order that does answers 3; none answers 2
    for cost in range(0, 6):
        for which in range(4):
            ops = themes(4) + [o("globals", 15, 3, 0, 0)] + [o("balance", which, 1, cost, 1150), o("clear_map")] + themes(4)
            ops += [o("globals", 15, 3, 0, 0), o("check_won")]
            yield case_of(ops)
    # the key cost of a park with no balance file is -1; keys are -1 with enough negative extra
    ops = themes(2) + [o("globals", 15, 3, -6, 0), o("check_won"), o("globals", 15, 3, -7, 0), o("check_won"), o("balance", 0, 0, 0, 0),
                       o("balance", 1, 0, 0, 0), o("clear_map"), o("ms_getinfo", 0), o("ms_getinfo", 1), o("check_won")]
    yield case_of(ops)
    # parks that are only records (no balance table yet): the file is read by the check
    ops = [o("ms_new")] + [o("ms_create", t) for t in range(4)] + [o("globals", 15, 3, 0, 0), o("check_won"), o("tickets")]
    yield case_of(ops)


def check_random(r):
    ops = [o("ms_new")] + [o("ms_create", t) if r.random() < 0.3 else o("ms_getinfo", t) for t in r.sample(range(8), r.randrange(0, 7))]
    for _ in range(r.randrange(2, 10)):
        k = r.random()
        if k < 0.3:
            ops.append(o("globals", r.randrange(16), r.randrange(4), r.choice([0, 0, 1, 2, -1, -2]), 0))
        elif k < 0.5:
            ops.append(o("park_tickets", r.randrange(8), r.randrange(64), 0))
        elif k < 0.65:
            ops.append(o("balance", r.randrange(8), r.randrange(2), r.choice([0, 1, 2, 3, 4, 5, 6]), r.choice(GATES)))
        else:
            ops.append(o("check_won"))
    return case_of(ops)


# ---- EarnedTicket (global, score)

def global_edges():
    # first time: sets the byte and the award in the current park, answers CheckTicketTypeWon
    for t in range(4):
        for score in (0, 1, 100, -1, -100, 0x7FFFFFFF, -0x80000000):
            yield case_of(themes(3) + [o("change", 1), o("earned_global", t, score), o("tickets"), o("earned_global", t, score + 1)])
    # already won: the current park holds the award (answer 0); another park does with a lower score (4: the award moves); with a
    # higher or equal score (0)
    for first, second in ((5, 9), (9, 5), (7, 7), (-3, 4), (4, -3), (0, 1), (1, 0), (-0x80000000, 0x7FFFFFFF), (0x7FFFFFFF, -0x80000000)):
        for t in (0, 3):
            ops = themes(3) + [o("change", 0), o("earned_global", t, first), o("change", 2), o("earned_global", t, second),
                               o("change", 2), o("earned_global", t, second + 1), o("change", 0), o("earned_global", t, first - 1)]
            yield case_of(ops)
    # the byte is set but no park holds the award (nothing moves, 0)
    ops = themes(3) + [o("change", 1), o("raw_save", 2, 1), o("earned_global", 2, 50), o("earned_global", 2, 60)]
    yield case_of(ops)
    # the first park in the map's order with a lower score wins
    ops = themes(5) + [o("raw_save", 1, 1)] + [o("raw_park", t, 1, 1, 1) for t in range(5)] + [o("raw_park", t, 2, 1, sc) for t, sc in enumerate((9, 3, 5, 4, 1))]
    ops += [o("change", 4), o("raw_park", 4, 1, 1, 0), o("earned_global", 1, 6), o("earned_global", 1, 6), o("change", 1), o("earned_global", 1, 10)]
    yield case_of(ops)
    # a byte that is 2 is not "won": it is set to 1 and the award goes to the current park
    ops = themes(2) + [o("change", 0), o("raw_save", 0, 2), o("earned_global", 0, 8), o("raw_save", 1, 255), o("earned_global", 1, 8)]
    yield case_of(ops)
    # the whole park record: awards of all four kinds, scores for each
    ops = themes(3) + [o("change", 0)] + [o("earned_global", t, 10 * t) for t in range(4)] + [o("change", 2)] + [o("earned_global", t, 10 * t + 5) for t in range(4)]
    ops += [o("tickets"), o("incl"), o("check_won")]
    yield case_of(ops)


def global_random(r):
    ops = themes(r.randrange(1, 6)) + [o("change", r.randrange(4))]
    for _ in range(r.randrange(4, 25)):
        k = r.random()
        if k < 0.5:
            ops.append(o("earned_global", r.randrange(4), r.choice([0, 1, 2, 5, 9, -1, -5, r.randrange(-20, 20)])))
        elif k < 0.7:
            ops.append(o("change", r.randrange(8)))
        elif k < 0.8:
            ops.append(o("raw_park", r.randrange(8), r.randrange(1, 3), r.randrange(4), r.choice([0, 1, r.randrange(-9, 9)])))
        elif k < 0.85:
            ops.append(o("raw_save", r.randrange(4), r.choice([0, 1, 2])))
        elif k < 0.9:
            ops.append(o("ms_getinfo", r.randrange(8)))
        elif k < 0.95:
            ops.append(o("globals", r.randrange(16), r.randrange(4), r.choice([0, 1, -1]), 0))
        else:
            ops.append(o("balance", r.randrange(8), r.randrange(2), r.choice([0, 1, 2, 3]), r.choice(GATES)))
    return case_of(ops)


# ---- ChangeCurrentTheme

def change_edges():
    # a theme that is in the map, one that is not (made, with a trace line), repeated, and a theme whose balance file is missing
    yield case_of([o("ms_new"), o("change", 0), o("change", 0), o("change", 3), o("change", 0), o("ms_getinfo", 1), o("change", 1)])
    for t in range(8):
        yield case_of(themes(3) + [o("change", t), o("change", (t + 3) % 8), o("change", t)])
    for t in range(8):
        yield case_of([o("ms_new"), o("balance", t, 0, 0, 0), o("change", t), o("change", t), o("balance", t, 1, 2, 1150), o("change", t), o("tickets")])
    # the balance file is read at once, once (a table kept; the trace line when it fails)
    yield case_of(themes(2) + [o("balance", 1, 0, 0, 0), o("change", 1), o("tickets"), o("change", 0), o("tickets")])
    # a record that was only created (no balance table) becomes current: its file is read
    yield case_of([o("ms_new"), o("ms_create", 2), o("change", 2), o("tickets"), o("ms_create", 2), o("change", 2)])
    # after the map is cleared
    yield case_of(themes(2) + [o("change", 1), o("clear_map"), o("change", 1), o("change", 0), o("tickets")])


def change_random(r):
    ops = themes(r.randrange(0, 5), r.randrange(0, 3))
    for _ in range(r.randrange(3, 14)):
        k = r.random()
        if k < 0.5:
            ops.append(o("change", r.randrange(8)))
        elif k < 0.65:
            ops.append(o("balance", r.randrange(8), r.randrange(2), r.choice([0, 1, 2, 3]), r.choice(GATES)))
        elif k < 0.75:
            ops.append(o("ms_getinfo", r.randrange(8)))
        elif k < 0.8:
            ops.append(o("ms_create", r.randrange(8)))
        elif k < 0.85:
            ops.append(o("clear_map"))
        else:
            ops.append(o("tickets"))
    return case_of(ops)


# ---- the master save's sign names, the swear filter, the first-time player, the research flags

def flag_edges():
    for first in range(len(SIGNS)):
        for second in (0, 1, 3, 11):
            ops = themes(2) + [o("ms_sign_a", first), o("change", 1), o("ms_sign_a", first), o("ms_sign_b", second),
                               o("pd_signa", 1, 0x40), o("pd_signb", 1, 0x40), o("cur_null"), o("ms_sign_a", 2), o("ms_sign_b", 2)]
            yield case_of(ops)
    # no current theme (the constructor does not set it): the setters would read garbage, so first a null current
    yield case_of([o("ms_new"), o("cur_null"), o("ms_sign_a", 1), o("ms_sign_b", 1), o("research_set"), o("research_all")])
    yield case_of([o("ms_new"), o("swear_get"), o("swear_set", 0), o("swear_get"), o("swear_set", 1), o("swear_get"), o("ms_reset"), o("swear_get")])
    for raw in (0, 1, 2, 255):
        yield case_of([o("ms_new"), o("raw_save", 6, raw), o("swear_get"), o("raw_save", 7, raw), o("first"), o("clear_first"), o("first"), o("ms_reset"), o("first")])
    yield case_of([o("ms_new"), o("first"), o("first"), o("clear_first"), o("clear_first"), o("first"), o("ms_reset"), o("first")])
    # the research flags: none, all, one missing; the current theme's flag set
    yield case_of([o("ms_new"), o("research_all")])
    yield case_of(themes(3) + [o("research_all"), o("change", 1), o("research_set"), o("research_all"), o("change", 0), o("research_set"),
                               o("research_all"), o("change", 2), o("research_set"), o("research_all")])
    for flag in (0, 1, 2, 255):
        yield case_of(themes(3) + [o("raw_park", t, 3, 0, flag) for t in range(3)] + [o("research_all"), o("raw_park", 1, 3, 0, 0), o("research_all")])
    yield case_of(themes(2) + [o("cur_null"), o("research_set"), o("research_all"), o("cur_to", 1), o("research_set"), o("research_all")])


def flag_random(r):
    ops = themes(r.randrange(0, 5))
    if r.random() < 0.5:
        ops.append(o("change", r.randrange(8)))
    else:
        ops.append(o("cur_null"))
    for _ in range(r.randrange(3, 20)):
        k = r.random()
        if k < 0.2:
            ops.append(o("ms_sign_a", r.randrange(len(SIGNS))))
        elif k < 0.3:
            ops.append(o("ms_sign_b", r.randrange(len(SIGNS))))
        elif k < 0.4:
            ops.append(o("research_set"))
        elif k < 0.5:
            ops.append(o("research_all"))
        elif k < 0.55:
            ops.append(o("raw_park", r.randrange(8), 3, 0, r.choice([0, 1, 2])))
        elif k < 0.65:
            ops.append(o("swear_set", r.randrange(2)))
        elif k < 0.7:
            ops.append(o("swear_get"))
        elif k < 0.75:
            ops.append(o("first"))
        elif k < 0.8:
            ops.append(o("clear_first"))
        elif k < 0.85:
            ops.append(o("change", r.randrange(8)))
        elif k < 0.9:
            ops.append(o("cur_null"))
        elif k < 0.95:
            ops.append(o("ms_getinfo", r.randrange(8)))
        else:
            ops.append(o("ms_reset"))
    return case_of(ops)


# ---- ClearThemeMap and the destructors

def clear_edges():
    yield case_of([o("ms_new"), o("clear_map"), o("clear_map"), o("ms_delete")])
    for n in range(0, 8):
        yield case_of(themes(n) + [o("clear_map"), o("tickets"), o("ms_getinfo", 0)])
        yield case_of(themes(n) + [o("ms_delete")])
    # records with and without a balance table (the table is taken down: UnInit and a delete), some only created
    ops = [o("ms_new"), o("ms_getinfo", 0), o("ms_create", 1), o("ms_create", 2), o("ms_getinfo", 3), o("balance", 4, 0, 0, 0), o("ms_getinfo", 4)]
    yield case_of(ops + [o("clear_map")])
    yield case_of(ops + [o("ms_delete")])
    # the current theme is left dangling: a new current theme makes the map work again
    yield case_of(themes(3) + [o("change", 1), o("clear_map"), o("cur_null"), o("ms_sign_a", 1), o("change", 1), o("change", 2), o("tickets"), o("clear_map")])
    # a bought set and the lists go with the object
    yield case_of(themes(2) + [o("spend", 4, 2), o("spend", 5, 2), o("spend", 1, 2), o("change", 0), o("earned_local", 0), o("ms_delete")])
    yield case_of([o("ms_new")] + [o("spend", t, 1) for t in range(30)] + [o("ms_delete")])
    # a delete and a new master save
    yield case_of(themes(2) + [o("ms_delete"), o("ms_new"), o("ms_getinfo", 4), o("tickets"), o("ms_delete"), o("ms_delete")])
    # the destructor's first part (the flag 0, as a member's destruction calls it): the same take-down, the block is left alone
    yield case_of([o("ms_new"), o("ms_delete0")])
    yield case_of(themes(3) + [o("change", 1), o("ms_delete0"), o("ms_new"), o("ms_getinfo", 2), o("tickets"), o("ms_delete")])
    yield case_of(themes(2) + [o("spend", 4, 2), o("spend", 5, 2), o("ms_delete0")])


def clear_random(r):
    ops = themes(r.randrange(0, 7), r.randrange(0, 3))
    for _ in range(r.randrange(3, 14)):
        k = r.random()
        if k < 0.25:
            ops.append(o("clear_map"))
        elif k < 0.4:
            ops.append(o("ms_getinfo", r.randrange(8)))
        elif k < 0.5:
            ops.append(o("ms_create", r.randrange(8)))
        elif k < 0.6:
            ops.append(o("spend", r.randrange(30), 1))
        elif k < 0.7:
            ops.append(o("balance", r.randrange(8), r.randrange(2), 1, r.choice(GATES)))
        elif k < 0.8:
            ops.append(o("change", r.randrange(8)))
        elif k < 0.9:
            ops.append(o("ms_delete"))
        else:
            ops.append(o("ms_new"))
    return case_of(ops)


# ---- the park record's ticket members

def park_edges():
    for t in range(6):
        for before in (0, 1, 2, 255):
            ops = themes(1) + [o("raw_park", 0, 0, t, before), o("pd_have", 0, t), o("pd_earned", 0, t), o("pd_have", 0, t), o("pd_earned", 0, t)]
            yield case_of(ops)
    for t in range(4):
        for flag in (0, 1, 2, 255):
            for score in (0, 7, -7, 0x7FFFFFFF, -0x80000000):
                ops = themes(1) + [o("raw_park", 0, 1, t, flag), o("raw_park", 0, 2, t, score), o("pd_award_in", 0, t), o("pd_score", 0, t)]
                ops += [o("pd_set", 0, t, 33), o("pd_award_in", 0, t), o("pd_score", 0, t), o("pd_clear", 0, t), o("pd_award_in", 0, t), o("pd_score", 0, t)]
                yield case_of(ops)
    # the scratch park (not in the map): its destructor takes the balance table down when there is one
    yield case_of([o("pd_new", 1), o("pd_have", SCRATCH, 0), o("pd_earned", SCRATCH, 0), o("pd_set", SCRATCH, 2, 5), o("pd_delete")])
    yield case_of([o("pd_new", 1), o("pd_signa", SCRATCH, 0x40), o("pd_delete")])
    yield case_of([o("pd_new", 7), o("pd_signa", SCRATCH, 0x40), o("pd_signb", SCRATCH, 0x40), o("pd_delete")])
    yield case_of([o("pd_new", 2), o("pd_set_a", SCRATCH, 1), o("pd_delete")])
    # the destructor's first part (the flag 0): the balance table is taken down, the block is left alone
    yield case_of([o("pd_new", 1), o("pd_have", SCRATCH, 0), o("pd_delete0")])
    yield case_of([o("pd_new", 7), o("pd_signa", SCRATCH, 0x40), o("pd_delete0"), o("pd_new", 7), o("pd_delete")])


def park_random(r):
    ops = themes(r.randrange(1, 4))
    for _ in range(r.randrange(3, 20)):
        which = r.randrange(3)
        k = r.random()
        if k < 0.15:
            ops.append(o("pd_have", which, r.randrange(6)))
        elif k < 0.3:
            ops.append(o("pd_earned", which, r.randrange(6)))
        elif k < 0.4:
            ops.append(o("pd_award_in", which, r.randrange(4)))
        elif k < 0.5:
            ops.append(o("pd_score", which, r.randrange(4)))
        elif k < 0.6:
            ops.append(o("pd_clear", which, r.randrange(4)))
        elif k < 0.7:
            ops.append(o("pd_set", which, r.randrange(4), r.choice([0, 1, -1, 99, r.randrange(-50, 50)])))
        elif k < 0.8:
            ops.append(o("raw_park", which, r.randrange(3), r.randrange(4), r.choice([0, 1, 2, r.randrange(-9, 9)])))
        elif k < 0.9:
            ops.append(o("pd_new", r.randrange(8)))
        else:
            ops.append(o("pd_delete"))
    return case_of(ops)


# ---- GetSignNameA, GetSignNameB, SetSignNameA, SetSignNameB (the park's)

def sign_edges():
    # the player's names: every pair of the sign texts, a few buffer sizes
    for size in (0x90, 0x21, 8, 1, 0, 2):
        for a in range(len(SIGNS)):
            for b in (0, 1, 3, 4, 11, 13):
                ops = [o("pd_new", 1), o("pd_set_a", SCRATCH, a), o("pd_set_b", SCRATCH, b), o("pd_signa", SCRATCH, size),
                       o("pd_signb", SCRATCH, size)]
                yield case_of(ops)
    # the sizes
    for size in BUFFER_SIZES:
        for a in (0, 2, 3, 9, 10, 12, 13):
            yield case_of([o("pd_new", 0), o("pd_set_a", SCRATCH, a), o("pd_set_b", SCRATCH, 8), o("pd_signa", SCRATCH, size), o("pd_signb", SCRATCH, size)])
    # no player's names: the ride database's text for the gate object (in the name table or in the ride database), or an empty text
    for gate in GATES + [0x10000 + 1190]:
        for size in (0x90, 0x20, 6, 1, 0):
            ops = [o("pd_new", 1), o("balance", 1, 1, 2, gate), o("pd_signa", SCRATCH, size), o("pd_signb", SCRATCH, size)]
            yield case_of(ops)
    for size in (0x90, 6, 1, 0):
        ops = [o("pd_new", 1), o("balance", 1, 0, 2, 1150), o("pd_signa", SCRATCH, size), o("pd_signb", SCRATCH, size), o("balance", 1, 1, 2, 1150),
               o("pd_signa", SCRATCH, size), o("pd_signb", SCRATCH, size)]
        yield case_of(ops)
    # the sign flag: 0 uses the ride names even with texts set; 2 and 255 count as set
    for flag in (0, 1, 2, 255):
        ops = [o("pd_new", 1), o("pd_set_a", SCRATCH, 2), o("pd_set_b", SCRATCH, 8), o("raw_park", SCRATCH, 4, 0, flag), o("pd_signa", SCRATCH, 0x40), o("pd_signb", SCRATCH, 0x40)]
        yield case_of(ops)
    # a park in the map (GetInfoOn made it, its table is read): the names, then the player's
    yield case_of(themes(2) + [o("pd_signa", 1, 0x40), o("pd_signb", 1, 0x40), o("pd_set_a", 1, 3), o("pd_set_b", 1, 5), o("pd_signa", 1, 0x40), o("pd_signb", 1, 0x40)])
    # the sign work area keeps the last name that was cut
    yield case_of([o("pd_new", 1), o("pd_set_a", SCRATCH, 9), o("pd_signa", SCRATCH, 0x40), o("pd_set_a", SCRATCH, 6), o("pd_signa", SCRATCH, 0x40)])
    # the name table's strings with a hyphen at the end, and rides whose sign texts are empty or long
    long_rides = [(7777, "q" * 70, "r" * 70), (5555, "", ""), (6666, "-", "--")]
    for size in (0x90, 0x20, 1):
        yield case_of([o("pd_new", 1), o("balance", 1, 1, 2, 7777), o("pd_signa", SCRATCH, size), o("pd_signb", SCRATCH, size)], rides=long_rides)
        yield case_of([o("pd_new", 1), o("balance", 1, 1, 2, 6666), o("pd_signa", SCRATCH, size), o("pd_signb", SCRATCH, size)], rides=long_rides)


def sign_random(r):
    ops = [o("pd_new", r.randrange(8))] if r.random() < 0.7 else themes(2)
    which = SCRATCH if ops[0][0] == OP["pd_new"] else r.randrange(2)
    for _ in range(r.randrange(3, 14)):
        k = r.random()
        if k < 0.2:
            ops.append(o("pd_set_a", which, r.randrange(len(SIGNS))))
        elif k < 0.35:
            ops.append(o("pd_set_b", which, r.randrange(len(SIGNS))))
        elif k < 0.55:
            ops.append(o("pd_signa", which, r.choice(BUFFER_SIZES + [r.randrange(0, 0x91)])))
        elif k < 0.75:
            ops.append(o("pd_signb", which, r.choice(BUFFER_SIZES + [r.randrange(0, 0x91)])))
        elif k < 0.85:
            ops.append(o("balance", r.randrange(8), r.randrange(2), 1, r.choice(GATES)))
        elif k < 0.95:
            ops.append(o("raw_park", which, 4, 0, r.choice([0, 1, 2])))
        else:
            ops.append(o("ms_getinfo", r.randrange(8)))
    strings = {}
    if r.random() < 0.3:
        for sid in r.sample(range(2, 60), 4):
            strings[sid] = r.choice(["", "x", "Name-", "-", "ab-", "a" * 70, "Two words", "q" * 63, "q" * 64])
    return case_of(ops, strings=strings)


# ---- scripts of many operations of every kind

def random_op(r):
    k = r.random()
    which = r.randrange(8)
    if k < 0.04:
        return o("ms_new")
    if k < 0.08:
        return o("ms_reset")
    if k < 0.14:
        return o("ms_getinfo", which)
    if k < 0.17:
        return o("ms_create", which)
    if k < 0.2:
        return o("change", which)
    if k < 0.24:
        return o("balance", which, r.randrange(2), r.choice([0, 1, 2, 3, 4, 5]), r.choice(GATES))
    if k < 0.28:
        return o("globals", r.randrange(16), r.randrange(4), r.choice([0, 0, 1, 2, -1, -2]), r.choice([0, 1, 3, -1]))
    if k < 0.32:
        return o("park_tickets", which, r.randrange(64), r.randrange(16))
    if k < 0.37:
        return o(r.choice(["tickets", "incl", "power", "check_won"]))
    if k < 0.41:
        return o("spend", r.randrange(40), r.choice(COSTS))
    if k < 0.44:
        return o("bought", r.randrange(40))
    if k < 0.48:
        return o("earned_local", r.randrange(6))
    if k < 0.51:
        return o("earned_secret", r.randrange(2))
    if k < 0.56:
        return o("earned_global", r.randrange(4), r.choice([0, 1, 5, 9, -3, r.randrange(-20, 20)]))
    if k < 0.58:
        return o("have_local", r.randrange(6))
    if k < 0.6:
        return o("have_secret", r.randrange(2))
    if k < 0.63:
        return o(r.choice(["ms_sign_a", "ms_sign_b"]), r.randrange(len(SIGNS)))
    if k < 0.65:
        return o("swear_set", r.randrange(2))
    if k < 0.67:
        return o(r.choice(["swear_get", "first", "clear_first", "research_all", "research_set"]))
    if k < 0.7:
        return o("clear_map")
    if k < 0.72:
        return o("ms_delete")
    if k < 0.74:
        return o("cur_null") if r.random() < 0.5 else o("cur_to", which)
    if k < 0.78:
        return o(r.choice(["pd_have", "pd_earned"]), which, r.randrange(6))
    if k < 0.82:
        return o(r.choice(["pd_award_in", "pd_score", "pd_clear"]), which, r.randrange(4))
    if k < 0.84:
        return o("pd_set", which, r.randrange(4), r.choice([0, 1, -1, 99]))
    if k < 0.86:
        return o("pd_new", which)
    if k < 0.87:
        return o("pd_delete")
    if k < 0.9:
        return o(r.choice(["pd_set_a", "pd_set_b"]), r.choice([which, SCRATCH]), r.randrange(len(SIGNS)))
    if k < 0.96:
        return o(r.choice(["pd_signa", "pd_signb"]), r.choice([which, SCRATCH]), r.choice(BUFFER_SIZES))
    if k < 0.98:
        return o("raw_park", r.choice([which, SCRATCH]), r.randrange(5), r.randrange(4), r.choice([0, 1, 2, r.randrange(-9, 9)]))
    return o("raw_save", r.randrange(9), r.choice([0, 1, 2, 255]))


def script_random(r):
    ops = [o("ms_new")] + [random_op(r) for _ in range(r.randrange(20, 60))]
    return case_of(ops)


def long_script(r):
    """Two hundred operations."""
    ops = themes(r.randrange(2, 8)) + [o("change", r.randrange(8))] + [random_op(r) for _ in range(196)]
    return case_of(ops)


# ---- the tests: each module-level test runs scripts through the same runner

TESTS.append(FnTest("CFSGlobalMasterSave::HowManyTickets, HowManyTicketsIncludingSecrets, SpendingPower", ms_ppc, ms_host,
                    edge=count_edges, rnd=count_random))
TESTS.append(FnTest("CFSGlobalMasterSave::SpendTicketOn, HasBeenBought", ms_ppc, ms_host, edge=spend_edges, rnd=spend_random))
TESTS.append(FnTest("CFSGlobalMasterSave::HaveTicket and EarnedTicket (local ticket), CFSGlobalParkData::HaveTicket, EarnedTicket",
                    ms_ppc, ms_host, edge=local_edges, rnd=local_random))
TESTS.append(FnTest("CFSGlobalMasterSave::HaveTicket and EarnedTicket (secret ticket)", ms_ppc, ms_host, edge=secret_edges,
                    rnd=local_random))
TESTS.append(FnTest("CFSGlobalMasterSave::EarnedTicket (global ticket, score), CFSGlobalParkData::AwardInThisPark, ScoreForAward, "
                    "ClearAward, SetAward", ms_ppc, ms_host, edge=global_edges, rnd=global_random))
TESTS.append(FnTest("CFSGlobalMasterSave::CheckTicketTypeWon", ms_ppc, ms_host, edge=check_edges, rnd=check_random))
TESTS.append(FnTest("CFSGlobalMasterSave::ChangeCurrentTheme", ms_ppc, ms_host, edge=change_edges, rnd=change_random))
TESTS.append(FnTest("CFSGlobalMasterSave::SetSignNameA, SetSignNameB, SetSwearFilter, IsSwearFilterOn, IsFirstTimePlayer, "
                    "ClearFirstTimePlayer, HasAllResearchBeenCompleted, SetAllResearchCompletedInCurrentTheme", ms_ppc, ms_host,
                    edge=flag_edges, rnd=flag_random))
TESTS.append(FnTest("CFSGlobalMasterSave::ClearThemeMap, ~CFSGlobalMasterSave, CFSGlobalParkData::~CFSGlobalParkData", ms_ppc, ms_host,
                    edge=clear_edges, rnd=clear_random))
TESTS.append(FnTest("CFSGlobalParkData ticket and award members on parks of the map and a scratch park", ms_ppc, ms_host, edge=park_edges,
                    rnd=park_random))
TESTS.append(FnTest("CFSGlobalParkData::GetSignNameA, GetSignNameB, SetSignNameA, SetSignNameB", ms_ppc, ms_host, edge=sign_edges,
                    rnd=sign_random))
TESTS.append(FnTest("Scripts of 60 operations of every kind", ms_ppc, ms_host, edge=lambda: [], rnd=script_random))
TESTS.append(FnTest("Scripts of 200 operations", ms_ppc, ms_host, edge=lambda: [], rnd=long_script))
