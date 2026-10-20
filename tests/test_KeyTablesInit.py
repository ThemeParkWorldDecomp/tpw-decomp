"""Unit w53: static entries, InitialiseKeytables, DeInitialiseKeyTables and Search.

Compare all seven globals, table bytes, entry fields, resolved action labels, help
identifiers and call order. Nullable allocation faults are observed on both sides.
The recording strncpy fake can change counts; formatter fakes write two arbitrary
UTF-16 characters and can change the current count. No action is called here.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from unicorn import UcError

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_KeyTablesInit"
INIT, DEINIT, SEARCH = 0x1015014C, 0x1015120C, 0x1014FD68
GLOBALS = [0x1037F234, 0x1037F230, 0x1037F22C, 0x1037F228,
           0x1037F224, 0x1037F220, 0x1037F21C]
ARRAYS = [0x102A4E58, 0x102A4F48, 0x102A5074, 0x102A5998,
          0x102A5268, 0x102A52B8, 0x102A540C]
COUNTS = [12, 15, 25, 42, 4, 17, 71]
DESCRIPTORS = {
    0x10266020: 1,  # AllowFullCameraZoomout
    0x10266028: 2,  # ResetCameraPosition
    0x10266048: 3,  # ToggleDrawUpSpline
    0x10266050: 4,  # OutputCoasterValues
    0x10266058: 5,  # IncCoasterConst2
    0x10266060: 6,  # DecCoasterConst2
    0x10266068: 7,  # IncCoasterConst1
    0x10266070: 8,  # DecCoasterConst1
    0x102660d0: 9,  # ToggleProceduralTexturing
    0x10266e98: 10,  # RealTimeLightingMasterToggle
    0x10266ea0: 11,  # FPSDisplayToggle
    0x10266ea8: 12,  # place_litter
    0x10266eb0: 13,  # place_vomit
    0x10266eb8: 14,  # cheat_cleanup_park
    0x10266ec0: 15,  # complete_all_research
    0x10266ec8: 16,  # uncomplete_all_research
    0x10266ed0: 17,  # spawn_person
    0x10266ed8: 18,  # spawn_five_persons
    0x10266ee0: 19,  # populate_park
    0x10266ee8: 20,  # start_winning_sequence
    0x10266ef0: 21,  # start_bankruptcy_sequence
    0x10266ef8: 22,  # spawn_guard
    0x10266f00: 23,  # spawn_researcher
    0x10266f08: 24,  # spawn_mechanic
    0x10266f10: 25,  # spawn_handyman
    0x10266f18: 26,  # spawn_entertainer
    0x10266f20: 27,  # spawn_bag_o_staff
    0x10266f28: 28,  # apply_for_loan_1
    0x10266f30: 29,  # apply_for_loan_2
    0x10266f38: 30,  # apply_for_loan_3
    0x10266f40: 31,  # apply_for_loan_4
    0x10266f48: 32,  # get_doshed_up
    0x10266f50: 33,  # force_ride_breakdown
    0x10266f58: 34,  # force_ride_worn
    0x10266f60: 35,  # start_strike
    0x10266f68: 36,  # show_script
    0x10266f70: 37,  # LoadPtclLib
    0x10266f78: 38,  # force_ride_condemned
    0x10266f80: 39,  # force_ride_fix
    0x10266f88: 40,  # place_bomb
    0x10266f90: 41,  # park_attraction_value
    0x10266f98: 42,  # debug_cell_dirs
    0x10266fa0: 43,  # debug_track_cell_dirs
    0x10266fa8: 44,  # toggle_cell_flash
    0x10266fb0: 45,  # debug_cell_neighbs
    0x10266fb8: 46,  # debug_track_cell_neighbs
    0x10266fc0: 47,  # debug_hoarding_cell_neighbs
    0x10266fc8: 48,  # select_debug_person
    0x10266fd0: 49,  # set_allow_loops_no_research
    0x10266fd8: 50,  # save_hoardings
    0x10266fe0: 51,  # save_complete_hoardings
    0x10266fe8: 52,  # enter_hoarding_mode
    0x10266ff0: 53,  # show_person_history_info
    0x10266ff8: 54,  # wheres_my_script
    0x10267000: 55,  # dbgchan0
    0x10267008: 56,  # dbgchan1
    0x10267010: 57,  # dbgchan2
    0x10267018: 58,  # dbgchan3
    0x10267020: 59,  # dbgchan4
    0x10267028: 60,  # dbgchan5
    0x10267030: 61,  # dbgchan6
    0x10267038: 62,  # dbgchan7
    0x10267040: 63,  # dbgchan8
    0x10267048: 64,  # dbgchan9
    0x10267050: 65,  # ChatterToggle
    0x10267058: 66,  # delete_ride
    0x10267060: 67,  # togglehud
    0x10267068: 68,  # TestAdvisor
    0x10267070: 69,  # TestAdvisor2
    0x10267078: 70,  # TestAdvisor3
    0x10267080: 71,  # TestAdvisor4
    0x10267088: 72,  # start_cheating
    0x10267090: 73,  # recreate_recording
    0x10267098: 74,  # recreate_recording_real_time
    0x102670a0: 75,  # recreate_recording_medium_speed
    0x102670a8: 76,  # save_recording
    0x102670b0: 77,  # create_arrival_vehicle
    0x102670b8: 78,  # clear_patrol_area
    0x102670c0: 79,  # next_debug_text_mode
    0x102670c8: 80,  # win_ticket_0
    0x102670d0: 81,  # win_ticket_1
    0x102670d8: 82,  # win_ticket_2
    0x102670e0: 83,  # win_ticket_3
    0x102670e8: 84,  # win_ticket_4
    0x102670f0: 85,  # win_ticket_5
    0x102670f8: 86,  # win_ticket_6
    0x10267100: 87,  # win_ticket_7
    0x10267108: 88,  # win_ticket_8
    0x10267110: 89,  # win_ticket_9
    0x10267118: 90,  # quickload_layout
    0x10267120: 91,  # quicksave_layout
    0x10267128: 92,  # mainmenu
    0x10267130: 93,  # Screen_BuyObject
    0x10267138: 94,  # Screen_BuyStaff
    0x10267140: 95,  # Screen_ParkStatus
    0x10267148: 96,  # Screen_AllItems
    0x10267150: 97,  # Screen_AllStaff
    0x10267158: 98,  # Screen_AllPeeps
    0x10267160: 99,  # Screen_Finance
    0x10267168: 100,  # Screen_Loans
    0x10267170: 101,  # Screen_StaffCosts
    0x10267178: 102,  # Screen_EntryPrice
    0x10267180: 103,  # Screen_Research
    0x10267188: 104,  # Screen_Map
    0x10267190: 105,  # Screen_StaffLoc
    0x10267198: 106,  # Screen_PeepLoc
    0x102671a0: 107,  # TakePostCard
    0x102671a8: 108,  # CamcorderMode
    0x102671b0: 109,  # abort_coaster_edit
    0x102671b8: 110,  # AnaglyphToggle
    0x102671c0: 111,  # abort_all_requesters
    0x102671c8: 112,  # turn_object_clockwise
    0x102671d0: 113,  # turn_object_counterclockwise
    0x102671d8: 114,  # enter_area_delete_mode
    0x102671e0: 115,  # togglegamewindow
    0x102671e8: 116,  # backtrack
    0x102671f0: 117,  # slow_down_time
    0x102671f8: 118,  # speed_up_time
    0x10267200: 119,  # normal_speed_time
    0x10267208: 120,  # close_park
    0x10267210: 121,  # open_park
    0x10267218: 122,  # quickload
    0x10267220: 123,  # quicksave
    0x10267228: 124,  # context_help
    0x10267230: 125,  # show_version
    0x10267238: 126,  # pause
    0x10267240: 127,  # ToggleHelpText
    0x10267248: 128,  # snapshot
    0x10267250: 129,  # continuous_snapshot
    0x10267258: 130,  # DeactivateWeather
    0x10267260: 131,  # WeatherReset
    0x10267268: 132,  # WeatherDownLots
    0x10267270: 133,  # WeatherUpLots
    0x10267278: 134,  # WeatherDown
    0x10267280: 135,  # WeatherUp
    0x103bc8dc: 136,  # ToggleFullScreen__Fv
}


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & 0xFFFFFFFF))


def w16(e, address, value):
    e.write(address, struct.pack(">H", value & 0xFFFF))


def pack(words):
    return struct.pack("<%di" % len(words), *words)


def queues(case):
    return {**{"chars:%d" % i: [v] for i, v in enumerate(case["chars"])},
            **{"tails:%d" % i: [v] for i, v in enumerate(case["tails"])}}


def ppc_init(e, case):
    saved_memory = [(a, bytes(e.read(a, n * 20))) for a, n in zip(ARRAYS, COUNTS)]
    saved_memory += [(a, bytes(e.read(a, 4))) for a in GLOBALS + [0x1037FF18 + 0x554]]
    e.cl = CL.PpcLog({}, queues(case))
    S = CL.Stubs()
    S.real.add(INIT)
    blocks = [0] * 7
    slot = [0]
    text_table = e.alloc(4)
    e.cl.labels[text_table] = 10
    w32(e, 0x1037FF18 + 0x554, text_table)
    for a in GLOBALS:
        w32(e, a, 0)
    for table, index, ident in case["edits"]:
        w16(e, ARRAYS[table] + 20 * index, ident)

    # Nullable allocator records every request, including the debug allocation last.
    def allocate(emu):
        index = slot[0]
        slot[0] += 1
        emu.cl.rec("new", (index, emu.r(3)))
        if case["mask"] & (1 << index):
            address = 0
        else:
            address = emu.alloc(0x18, bytes([case["fill"]]) * 0x18)
            blocks[index] = address
            emu.cl.labels[address] = 100 + index
        emu.set_r(3, address)

    # Faithful n-byte copy with controllable count edits after construction.
    def copy(emu):
        dest, source, n = [emu.r(i) for i in (3, 4, 5)]
        index = slot[0] - 1
        text = emu.cstr(source)
        emu.cl.rec("strncpy", (emu.cl.lab(dest - 8), 8, n), (text,))
        emu.write(dest, text.encode("latin-1")[:n].ljust(n, b"\0"))
        if case["counts"][index] != 0x10000:   # 0x10000: leave the count the constructor stored
            w16(emu, blocks[index] + 4, case["counts"][index])
        emu.set_r(3, dest)

    # The formatter's object, size and id are recorded; both characters are written.
    def format_string(emu):
        table, out, size, ident = [emu.r(i) for i in (3, 4, 5, 6)]
        emu.cl.rec("FormatString", (emu.cl.lab(table), emu.cl.lab(out), size, ident))
        emu.write(out, struct.pack(">2H", case["chars"][ident], case["tails"][ident]))
        if case["after"] != -999:
            target = 5 if ident <= 14 else 0 if ident in (15, 20) else 1
            w16(emu, emu.u32(GLOBALS[target]) + 4, case["after"])
        emu.set_r(3, T.u32(-7))

    S.imp("__nw__FUl", allocate)
    S.imp("strncpy", copy)
    S.imp("FormatString__8TbITableCFPwUiUle", format_string)
    previous = S.activate(e)
    try:
        fault = 0
        try:
            T.call_ppc(e, INIT, [])
        except (UcError, RuntimeError):
            # Only null +4 accesses are expected from the three mandatory tables.
            if not case["mask"] & 0x23 or e.last_fault[1] != 4:
                raise
            fault = 1
        out = [fault]
        for t, (base, count) in enumerate(zip(ARRAYS, COUNTS)):
            pointer = e.u32(GLOBALS[t])
            out.append(0 if pointer == 0 else 100 + blocks.index(pointer))
            table = blocks[t]
            out.append(int(table != 0))
            if table:
                entries = e.u32(table)
                out += [ARRAYS.index(entries), *struct.unpack(">2h", e.read(table + 4, 4)),
                        *bytes(e.read(table + 8, 16))]
            for i in range(count):
                at = base + i * 20
                ident, key, mods, state, text, up, down = struct.unpack(">4h3I", e.read(at, 20))
                help_text = e.cstr(text).encode("latin-1") if text else b""
                out += [ident, key, mods, state, int(text != 0), len(help_text),
                        *help_text, DESCRIPTORS.get(up, -1) if up else 0,
                        DESCRIPTORS.get(down, -1) if down else 0]
        return pack(out), e.cl.log
    finally:
        CL.Stubs.deactivate(e, previous)
        for address, data in saved_memory:
            e.write(address, data)


def host_init(lib, case):
    CL.host_begin(lib, {}, queues(case))
    edits = [v for entry in case["edits"] for v in entry]
    lib.tpw_ki_prepare.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int32),
                                  C.POINTER(C.c_int32), C.c_int32, C.c_int32]
    lib.tpw_ki_prepare(case["mask"], case["fill"], (C.c_int32 * 7)(*case["counts"]),
                       (C.c_int32 * (len(edits) + 1))(*edits), len(case["edits"]), case["after"])
    fault = lib.tpw_ki_init()
    out = (C.c_int32 * 5000)()
    lib.tpw_ki_state.argtypes = [C.POINTER(C.c_int32)]
    n = lib.tpw_ki_state(out)
    log = CL.host_end(lib)
    lib.tpw_ki_cleanup()
    return pack([fault, *list(out)[:n]]), log


def init_case(mask=0, fill=0xA5, counts=None, edits=(), chars=None, after=-999):
    return dict(mask=mask, fill=fill, counts=list(COUNTS if counts is None else counts),
                edits=list(edits), chars=list(range(65, 86) if chars is None else chars),
                tails=list(range(0x8100, 0x8115)), after=after)


def init_edges():
    yield init_case()
    # Every allocation can return null, including the three original fault paths.
    for mask in range(1, 128):
        yield init_case(mask=mask)
    for ch in (0, 1, 0x7F, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF):
        for fill in (0, 0x19, 0xFF):
            yield init_case(fill=fill, chars=[ch] * 21)
    # Counts on either side of every patched id exercise strict signed guards.
    for table in (0, 1, 5):
        for count in [-32768, -1, *range(COUNTS[table] + 1)]:
            counts = COUNTS[:]
            counts[table] = count
            yield init_case(counts=counts)
    # Rotate ids to make searches succeed away from the preferred index.
    for table in (0, 1, 5):
        for shift in range(1, COUNTS[table]):
            yield init_case(edits=[(table, i, (i + shift) % COUNTS[table]) for i in range(COUNTS[table])])
        yield init_case(edits=[(table, i, -1) for i in range(COUNTS[table])])
        for ident in range(COUNTS[table]):
            # A preferred hit wins over an earlier duplicate; otherwise first wins.
            yield init_case(edits=[(table, 0, ident)])
            yield init_case(edits=[(table, i, ident) for i in range(COUNTS[table])])
    for count in (-32768, -1, 0, 1, 2, 11, 15, 16, 17):
        yield init_case(after=count)
    # triage 2026-10-13a: the counts MakeTable stores are kept (the fakes overwrite them in every other case): all seven together,
    # then each table's count alone with the others overwritten
    yield init_case(counts=[0x10000] * 7)
    yield init_case(counts=[0x10000] * 7, fill=0)
    yield init_case(counts=[0x10000] * 7, mask=0x40)
    for table in range(7):
        counts = COUNTS[:]
        counts[table] = 0x10000
        yield init_case(counts=counts)


def init_random(r):
    counts = COUNTS[:]
    edits = []
    for t in (0, 1, 5):
        counts[t] = r.choice([COUNTS[t], r.randrange(-2, COUNTS[t] + 1)])
        edits += [(t, i, r.randrange(-2, COUNTS[t] + 2)) for i in range(COUNTS[t]) if r.randrange(3) == 0]
    case = init_case(mask=r.choice([0, 0, 0, r.randrange(128)]), fill=r.randrange(256),
                     counts=counts, edits=edits, chars=[r.getrandbits(16) for _ in range(21)],
                     after=r.choice([-999, -999, -1, 0, 1, 7, 16]))
    case["tails"] = [r.getrandbits(16) for _ in range(21)]
    return case


def ppc_deinit(e, mask):
    saved_memory = [(a, bytes(e.read(a, 4))) for a in GLOBALS]
    blocks = [e.alloc(0x18) if mask & (1 << i) else 0 for i in range(7)]
    e.cl = CL.PpcLog({address: 100 + i for i, address in enumerate(blocks) if address})
    for address, block in zip(GLOBALS, blocks):
        w32(e, address, block)
    S = CL.Stubs()
    S.real.add(DEINIT)
    S.imp("__dl__FPv", lambda emu: emu.cl.rec("delete", (emu.cl.lab(emu.r(3)),)))
    previous = S.activate(e)
    try:
        T.call_ppc(e, DEINIT, [])
        return [e.cl.lab(e.u32(a)) for a in GLOBALS], e.cl.log
    finally:
        CL.Stubs.deactivate(e, previous)
        for address, data in saved_memory:
            e.write(address, data)


def host_deinit(lib, mask):
    CL.host_begin(lib, {})
    out = (C.c_int32 * 7)()
    lib.tpw_ki_deinit.argtypes = [C.c_int32, C.POINTER(C.c_int32)]
    lib.tpw_ki_deinit(mask, out)
    return list(out), CL.host_end(lib)


def ppc_search(e, case):
    count, enabled, key, mods, entries, junk = case
    raw = b"".join(struct.pack(">4h3I", *row, 0, 0, 0) for row in entries)
    base = e.alloc(max(20, len(raw)), raw)
    table = e.alloc(0x18, struct.pack(">I2h", base, count, enabled).ljust(0x18, b"\xa5"))
    S = CL.Stubs()
    S.real.add(SEARCH)
    previous = S.activate(e)
    try:
        result = T.call_ppc(e, SEARCH, [table, (key & 0xFFFF) | junk << 16,
                                       (mods & 0xFFFF) | junk << 16])[0]
        state = [*struct.unpack(">2h", e.read(table + 4, 4))]
        for i in range(len(entries)):
            state += list(struct.unpack(">4h", e.read(base + 20 * i, 8)))
        assert bytes(e.read(base, len(raw))) == raw
        return T.s32(result), state
    finally:
        CL.Stubs.deactivate(e, previous)


def host_search(lib, case):
    count, enabled, key, mods, entries, _ = case
    flat = [v for row in entries for v in row]
    out = (C.c_int32 * (2 + len(flat)))()
    lib.tpw_ki_search.argtypes = [C.c_int32] * 5 + [C.POINTER(C.c_int32)] * 2
    result = lib.tpw_ki_search(count, enabled, key, mods, len(entries),
                              (C.c_int32 * (len(flat) + 1))(*flat), out)
    return result, list(out)


def search_edges():
    for n in (0, 1, 7):
        entries = [(i, i * 10, i % 3, -1) for i in range(n)]
        for count in [-32768, -1, *range(n + 1)]:
            for key, mods in [(0, 0), (30, 0), (60, 0), (99, 0), (0, 1), (30, 1)]:
                yield count, 0, key, mods, entries, 0xA5A5
    for key in (-32768, -1, 0, 1, 0x7FFF):
        for mods in (-32768, -1, 0, 0x7FFF):
            for junk in (0, 0x8000, 0xFFFF):
                yield 7, -1, key, mods, [(i, key, mods, i) for i in range(7)], junk


def search_random(r):
    n = r.choice([0, 1, 7])
    entries = [tuple(T.s16(r.getrandbits(16)) for _ in range(4)) for _ in range(n)]
    key, mods = (entries[r.randrange(n)][1:3] if n and r.randrange(2) else
                 (T.s16(r.getrandbits(16)), T.s16(r.getrandbits(16))))
    return r.randrange(-2, n + 1), T.s16(r.getrandbits(16)), key, mods, entries, r.getrandbits(16)


TESTS = [FnTest("InitialiseKeytables", ppc_init, host_init, edge=init_edges, rnd=init_random),
         FnTest("DeInitialiseKeyTables", ppc_deinit, host_deinit,
                edge=lambda: iter(range(128)), rnd=lambda r: r.randrange(128)),
         FnTest("CKeyTable::Search", ppc_search, host_search, edge=search_edges, rnd=search_random)]
