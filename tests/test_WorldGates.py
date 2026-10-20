"""The park gates and the arrival vehicle (unit w193, main app), against libtpw_unit_WorldGates.dylib (tests/unit_WorldGates.cpp):

  src/World_gates.cpp  CWorld::SetParkOpenOrClosed (0x1013fc3c), ParkGates_Open (0x1014007c), ParkGates_Close (0x1014013c),
                       ArrivalVehicle_DoNextLeg (0x10140234), ArrivalVehicle_GetStatus (0x101406cc), ArrivalVehicle_CrossingIsClear
                       (0x1014080c), ArrivalVehicle_PeopleWaitingToLeave (0x10140b6c), ArrivalVehicle_PeopleLeftToDropOff (0x10140d98),
                       ArrivalVehicle_IsSeagoing (0x10140de0)

The nine run as the original on the PowerPC side and as src/ on the host, and call each other for real. Every other callee is a fake that
records the same event on both sides (name, integers, texts): GAME_ASSERTMSG, CTRACE (with the %s text), the script pointer and variables
(RSSE_Getvar answers the queue "var"), IsParkClosed ("closed"), ParkGates_GetStatus ("gstatus"), the analyser's visitors ("visitors"),
CObject's CloseObject, IsOpenable ("openable") and Reopen, CMsgEvent, CMessageCentre::PostMessage, the nothrow new of the centre ("newnull")
and its array construction, the ride database walk, _wcsicmp ("wcsicmp"), CObject's constructor (it writes the queue "id" at +0 and "handle"
at +0x20), CThing::GetThingId, CMap::GetBusStopPos ("stop"), operator new and the string class (functional, unrecorded).

A case describes the world (a size, a fill and the words the functions read, written by offset), the thing blocks of 0x240 bytes over the fill
0xa5 (kind +2, list link +0xc, script handle +0x20, flags +0x2e, state word +0x220), the thing table rows, the ride records, the three vehicle
names, the message centre and the fakes' queues. Compared: the answer, the event log, the world's start time, closed word and four vehicle
ids by value, that every other byte of the world still holds the case's value, that every block is unchanged, the object DoNextLeg made
(id and handle) and the message centre pointer's label. Not tested: ParkGates_Open and Close with no gates object (the Mac reads through
thing row 0, NULL), a vehicle whose table row holds no thing, a DoNextLeg leg outside 0 to 3 (the Mac reads address 0), a failing allocation of
the new vehicle object (the Mac tests the pointer, the translation does not), a table row beyond 0x27ff.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_WorldGates"

OPS = {
    "set": (0, 0x1013FC3C, "CWorld::SetParkOpenOrClosed"), "open": (1, 0x1014007C, "CWorld::ParkGates_Open"),
    "close": (2, 0x1014013C, "CWorld::ParkGates_Close"), "leg": (3, 0x10140234, "CWorld::ArrivalVehicle_DoNextLeg"),
    "status": (4, 0x101406CC, "CWorld::ArrivalVehicle_GetStatus"), "clear": (5, 0x1014080C, "CWorld::ArrivalVehicle_CrossingIsClear"),
    "waiting": (6, 0x10140B6C, "CWorld::ArrivalVehicle_PeopleWaitingToLeave"),
    "left": (7, 0x10140D98, "CWorld::ArrivalVehicle_PeopleLeftToDropOff"), "sea": (8, 0x10140DE0, "CWorld::ArrivalVehicle_IsSeagoing"),
}
OWN = {v[1] for v in OPS.values()}

TABLE, ROWS, ROW_BYTES = 0x1034CBFC, 0x2800, 0x14
G_CENTRE = 0x1037F6C4
NAME_ADDR = {"bus": 0x1034CB78, "seaplane": 0x1034CB58, "ferry": 0x1034CB38}
WORLD_SIZE, BIG_SIZE = 0x1DA748, 0x450000
BLOCK, RIDE_RECORD = 0x240, 0x700
MASK = 0xFFFFFFFF
L_WORLD, L_SCRIPT, L_CENTRE, L_BLOCK = 1, 3, 6, 10
MAP, CELL = 0x2D8, 0x44
ENTRY_ZERO = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"

W_FIELDS = {
    "start": (0x1DA708, 4), "closed": (0x1DA710, 4), "analyser": (0x1DA720, 2), "veh": (0x1DA72A, 2), "bus": (0x1DA72C, 2),
    "sea": (0x1DA72E, 2), "ferry": (0x1DA730, 2), "gates": (0x1DA732, 2), "list": (0x1DA746, 2), "people": (0x2D0, 4),
}
DEFAULT_W = dict(start=0x1234, closed=0, analyser=3, veh=0, bus=0, sea=0, ferry=0, gates=1, list=0, people=0)
NAMES = ("bus", "seaplane", "Ferry")
DEFAULT_BLOCK = dict(kind=1, flags=0, f0c=0, f20=None, w220=0)
BLANK = [(0x1DA708, 4), (0x1DA710, 4), (0x1DA72A, 8)]
FILL_TABLE = bytes(ROWS * ROW_BYTES)
STATE = {}


# ------------------------------------------------------------------ helpers (the string objects, as test_NewLevel.py)


def wide_read(e, a, limit=1000):
    out = []
    while a and len(out) < limit:
        ch = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if ch == 0:
            break
        out.append(chr(ch))
    return "".join(out)


def s_read(e, obj):
    data = e.u32(obj + 8)
    return wide_read(e, data) if data else ""


def s_write(e, obj, text):
    """A TbDynamicStringTemplate<wchar> in the original's layout {vtable, capacity, data, length, grow-by}."""
    blob = b"".join(struct.pack(">H", ord(ch) & 0xFFFF) for ch in text) + b"\0\0"
    data = e.alloc(len(blob), blob, alignment=4)
    e.write(obj, struct.pack(">IIIII", CL.FAKE_VT, 0, data, len(text), 0))


BIG_BASE = 0x78000000


def setup(runner):
    T.install_std_stubs(runner.emu)
    # the wrapped map cells of PeopleWaitingToLeave reach 4.4 MB beyond the world's start: more than the scratch area, so a region of its own
    runner.emu.uc.mem_map(BIG_BASE, 0x470000)


def construct_array(e):
    p, ctor, dtor, size, count = (e.r(i) for i in range(3, 8))
    if ctor == 0x102657E8:
        e.cl.rec("CMessageCentre", [e.cl.newlab(p), int(ctor == 0x102657E8), int(dtor == 0x10267288), size, count])


# ------------------------------------------------------------------ the case


def mk(op, **kw):
    """A case with every field defaulted. w: the world words by name (merged over DEFAULT_W); cells: map cell id -> thing id (their mWho);
    blocks: the thing blocks (dicts over DEFAULT_BLOCK); rows: thing table row -> block index; rides: (type id, item name, low half);
    names: the three vehicle names; centre: whether gMessageCentre is set; q: the fakes' queues; a, b: the arguments."""
    c = dict(op=op, size=WORLD_SIZE, fill=0xA5, w={}, cells={}, blocks=[], rows={}, rides=[], names=NAMES, centre=0, q={}, a=0, b=0)
    c.update(kw)
    w = dict(DEFAULT_W)
    w.update(c["w"])
    c["w"] = w
    return c


def block(**kw):
    b = dict(DEFAULT_BLOCK)
    b.update(kw)
    return b


def block_bytes(k, b):
    data = bytearray(b"\xA5" * BLOCK)
    data[2] = b["kind"] & 0xFF
    struct.pack_into(">H", data, 0xC, b["f0c"] & 0xFFFF)
    struct.pack_into(">I", data, 0x20, (0x100 + k * 0x11 if b["f20"] is None else b["f20"]) & MASK)
    struct.pack_into(">H", data, 0x2E, b["flags"] & 0xFFFF)
    struct.pack_into(">I", data, 0x220, b["w220"] & MASK)
    return bytes(data)


def world_writes(c):
    """(offset, size, value) for every word of the case's world, in the Mac's byte order's value form."""
    out = [(off, size, c["w"][name]) for name, (off, size) in W_FIELDS.items()]
    for cell, ident in c["cells"].items():
        out.append((MAP + ((cell - 1) * CELL) + 0x24, 2, ident))
    return out


def world_image(c, order):
    data = bytearray(bytes([c["fill"]]) * c["size"])
    for off, size, value in world_writes(c):
        data[off:off + size] = (value & ((1 << (8 * size)) - 1)).to_bytes(size, order)
    return data


def blank(data):
    for off, n in BLANK:
        data[off:off + n] = bytes(n)
    return bytes(data)


def hostish(v):
    return v & MASK


# ------------------------------------------------------------------ the PowerPC side


def stubs(e, L, st):
    S = CL.Stubs()
    S.real.update(OWN)
    s = lambda v: T.s32(v)

    S.internal(0x1000DDE8, lambda e: L.rec("GAME_ASSERTMSG", (s(e.r(3)),), (e.cstr(e.r(4))[:159],)))

    def ctrace(e):
        fmt = e.cstr(e.r(4))[:159]
        L.rec("CTRACE", (e.r(3),), (fmt, e.cstr(e.r(5))[:159]) if "%s" in fmt else (fmt,))

    S.internal(0x1006FCBC, ctrace)

    def script_pointer(e):
        L.rec("RSSE_GetScriptPointer", (e.r(3),))
        e.set_r(3, st["script"])

    S.internal(0x100D668C, script_pointer)

    def getvar(e):
        L.rec("RSSE_Getvar", (L.lab(e.r(3)), s(e.r(4))))
        e.set_r(3, T.u32(L.ret("var", 1)))

    S.internal(0x100D6C58, getvar)
    S.internal(0x100D6734, lambda e: L.rec("RSSE_Setvar", (L.lab(e.r(3)), s(e.r(4)), s(e.r(5)))))

    def simple(name, key, dflt):
        def stub(e):
            L.rec(name, (L.lab(e.r(3)),))
            e.set_r(3, T.u32(L.ret(key, dflt)))
        return stub

    S.internal(0x1013FF7C, simple("IsParkClosed", "closed", 0))
    S.internal(0x1013FFB4, simple("ParkGates_GetStatus", "gstatus", 1))
    S.internal(0x100E7278, simple("NumberOfVisitorsInPark", "visitors", 0))
    S.internal(0x1010AAAC, simple("IsOpenable", "openable", 1))
    S.internal(0x1010ABCC, lambda e: L.rec("CloseObject", (L.lab(e.r(3)),)))
    S.internal(0x1010ACDC, lambda e: L.rec("Reopen", (L.lab(e.r(3)),)))

    def bus_stop(e):
        L.rec("GetBusStopPos", (e.r(4) - st["world"], s(e.r(5))))
        e.write(e.r(3), struct.pack(">H", L.ret("stop", 0) & 0xFFFF))

    S.internal(0x10101A50, bus_stop)

    def msg_ctor(e):
        L.rec("CMsgEvent", [L.newlab(e.r(3)), s(e.r(4))])
        e.write(e.r(3) + 8, (e.r(4) & MASK).to_bytes(4, "big"))

    S.internal(0x10151DF8, msg_ctor)

    def post(e):
        L.rec("PostMessage", [L.lab(e.r(3)), L.lab(e.r(4)), s(e.u32(e.r(4) + 8))])
        e.set_r(3, 1)

    S.internal(0x1015256C, post)

    def op_new(e):
        size = e.r(3)
        if size == 0x1D0:
            fail = bool(L.ret("newnull", 0))
            L.rec("new", [size, int(fail)])
            e.set_r(3, 0 if fail else e.alloc(size, b"\xA5" * size))
        else:
            L.rec("new", [size])
            addr = e.alloc(size, b"\xA5" * size)
            if size == 0x448:
                st["newobj"] = addr
            e.set_r(3, addr)

    S.internal(0x102384B8, op_new)

    def first_ride(e):
        L.rec("GetFirstRide", (L.lab(e.r(3)),))
        st["cursor"] = 0
        e.set_r(3, st["rides"][0] if st["rides"] else 0)

    def next_ride(e):
        L.rec("GetNextRide", (L.lab(e.r(3)),))
        st["cursor"] += 1
        e.set_r(3, st["rides"][st["cursor"]] if st["cursor"] < len(st["rides"]) else 0)

    S.internal(0x101547B4, first_ride)
    S.internal(0x10154800, next_ride)

    def wcsicmp(e):
        a, b = e.r(3), e.r(4)
        L.rec("wcsicmp", (), (CL.wide_text(e, a), CL.wide_text(e, b)))
        queued = L.ret("wcsicmp", None)
        if queued is None:
            def fold(text):
                return "".join(chr(ord(ch) + 32) if "A" <= ch <= "Z" else ch for ch in text)
            queued = 0 if fold(wide_read(e, a, 5000)) == fold(wide_read(e, b, 5000)) else 1
        e.set_r(3, T.u32(queued))

    S.internal(0x102399B8, wcsicmp)

    def cobject(e):
        L.rec("CObject", (L.newlab(e.r(3)), (e.r(4) >> 16) & 0xFFFF, (e.r(5) >> 16) & 0xFFFF, s(e.r(6)), s(e.r(7))))
        e.write(e.r(3), struct.pack(">H", L.ret("id", 0x1234) & 0xFFFF))
        e.write(e.r(3) + 0x20, struct.pack(">I", L.ret("handle", 0x777) & MASK))

    S.internal(0x10105D74, cobject)

    def thing_id(e):
        L.rec("GetThingId", (L.lab(e.r(4)),))
        e.write(e.r(3), e.read(e.r(4), 2))

    S.internal(0x1012E1F8, thing_id)
    S.internal(0x10238758, construct_array)
    # the wide string class: default constructor, copy constructor, assignment, destructor (functional, unrecorded)
    S.internal(0x10238740, lambda e: s_write(e, e.r(3), ""))
    S.internal(0x102387A0, lambda e: s_write(e, e.r(3), s_read(e, e.r(4))))
    S.internal(0x10239850, lambda e: s_write(e, e.r(3), s_read(e, e.r(4))))
    S.internal(0x10238710, lambda e: None)
    return S


def ppc(e, c):
    code, entry, _ = OPS[c["op"]]
    if c["size"] > 0x300000:
        world = BIG_BASE
        e.write(world, bytes(world_image(c, "big")))
    else:
        world = e.alloc(c["size"], bytes(world_image(c, "big")))
    initial = blank(world_image(c, "big"))
    script = e.alloc(0x40, b"\xA5" * 0x40)
    centre = e.alloc(0x1D0, b"\xA5" * 0x1D0)
    addrs = []
    init_blocks = []
    for k, b in enumerate(c["blocks"]):
        data = block_bytes(k, b)
        init_blocks.append(data)
        addrs.append(e.alloc(BLOCK, data))
    table = bytearray(FILL_TABLE)
    for row, k in c["rows"].items():
        struct.pack_into(">I", table, row * ROW_BYTES, addrs[k])
    e.write(TABLE, bytes(table))
    e.write(G_CENTRE, struct.pack(">I", centre if c["centre"] else 0))
    for name, text in zip(("bus", "seaplane", "ferry"), c["names"]):
        s_write(e, NAME_ADDR[name], text)
    rides = []
    for ride_id, name, low in c["rides"]:
        blob = bytearray(b"\xA5" * RIDE_RECORD)
        struct.pack_into(">I", blob, 0, ((ride_id & 0xFFFF) << 16) | (low & 0xFFFF))
        text = b"".join(struct.pack(">H", ord(ch) & 0xFFFF) for ch in name[:0x3F]) + b"\0\0"
        blob[0x68C:0x68C + len(text)] = text
        rides.append(e.alloc(RIDE_RECORD, bytes(blob)))
    out = e.alloc(16, b"\xEE" * 16)
    fixed = {world: L_WORLD, script: L_SCRIPT}
    if c["centre"]:
        fixed[centre] = L_CENTRE
    for k, a in enumerate(addrs):
        fixed[a] = L_BLOCK + k
    L = CL.PpcLog(fixed, c["q"])
    e.cl = L
    st = {"script": script, "world": world, "rides": rides, "cursor": 0, "newobj": 0}
    S = stubs(e, L, st)
    saved = S.activate(e)
    result = {}
    try:
        args = {
            "set": [world, c["a"] & MASK, c["b"] & MASK], "open": [world], "close": [world, c["a"] & MASK],
            "leg": [out + 4, world, c["a"] & MASK], "status": [world], "clear": [world], "waiting": [world], "left": [world],
            "sea": [world],
        }[c["op"]]
        r3 = T.call_ppc(e, entry, args)[0]
        if c["op"] == "leg":
            result["ret"] = struct.unpack(">H", e.read(out + 4, 2))[0]
            result["outguard"] = int(e.read(out, 4) == b"\xEE" * 4 and e.read(out + 6, 10) == b"\xEE" * 10)
        elif c["op"] in ("status", "clear", "waiting", "left", "sea"):
            result["ret"] = T.s32(r3)
        final = bytearray(e.read(world, c["size"]))
        fields = [struct.unpack(">I", final[0x1DA708:0x1DA70C])[0], struct.unpack(">I", final[0x1DA710:0x1DA714])[0]]
        fields += [struct.unpack(">H", final[0x1DA72A + 2 * i:0x1DA72C + 2 * i])[0] for i in range(4)]
        result["fields"] = fields
        result["worldfill"] = int(blank(final) == initial)
        result["blocks"] = [int(e.read(a, BLOCK) == init_blocks[k]) for k, a in enumerate(addrs)]
        if st["newobj"]:
            raw = e.read(st["newobj"], 0x448)
            result["new"] = (1, struct.unpack(">H", raw[:2])[0], struct.unpack(">I", raw[0x20:0x24])[0])
        else:
            result["new"] = (0, 0, 0)
        result["centre"] = L.lab(e.u32(G_CENTRE))
        result["log"] = L.log
    finally:
        CL.Stubs.deactivate(e, saved)
    return result


# ------------------------------------------------------------------ the host side


def bind(lib):
    if getattr(lib, "_wg_bound", False):
        return
    lib.tpw_wg_begin.argtypes = []
    lib.tpw_wg_ptr.argtypes = [C.c_int32]
    lib.tpw_wg_ptr.restype = C.c_void_p
    lib.tpw_wg_world.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_wg_w16.argtypes = [C.c_int32, C.c_uint32]
    lib.tpw_wg_w32.argtypes = [C.c_int32, C.c_uint32]
    lib.tpw_wg_block.argtypes = [C.c_int32]
    for n in ("b8", "b16", "b32"):
        getattr(lib, f"tpw_wg_{n}").argtypes = [C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_wg_row.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_wg_ride.argtypes = [C.c_int32, C.c_uint16, C.POINTER(C.c_uint16), C.c_int32]
    lib.tpw_wg_ride_count.argtypes = [C.c_int32]
    lib.tpw_wg_centre.argtypes = [C.c_int32]
    lib.tpw_wg_names.argtypes = [C.POINTER(C.c_uint16), C.c_int32] * 3
    lib.tpw_wg_get_world.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_wg_get_blocks.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_wg_get_new.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_wg_centre_label.restype = C.c_int64
    lib.tpw_wg_call.argtypes = [C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_wg_call.restype = C.c_int64
    lib._wg_bound = True


def wide(text):
    arr = (C.c_uint16 * max(len(text), 1))(*[ord(ch) & 0xFFFF for ch in text])
    return arr, len(text)


def host(lib, c):
    bind(lib)
    code, entry, _ = OPS[c["op"]]
    lib.tpw_wg_begin()
    lib.tpw_wg_world(c["size"], c["fill"])
    for off, size, value in world_writes(c):
        (lib.tpw_wg_w16 if size == 2 else lib.tpw_wg_w32)(off, value & ((1 << (8 * size)) - 1))
    for k, b in enumerate(c["blocks"]):
        lib.tpw_wg_block(k)
        lib.tpw_wg_b8(k, 2, b["kind"] & 0xFF)
        lib.tpw_wg_b16(k, 0xC, b["f0c"] & 0xFFFF)
        lib.tpw_wg_b32(k, 0x20, (0x100 + k * 0x11 if b["f20"] is None else b["f20"]) & MASK)
        lib.tpw_wg_b16(k, 0x2E, b["flags"] & 0xFFFF)
        lib.tpw_wg_b32(k, 0x220, b["w220"] & MASK)
    for row, k in c["rows"].items():
        lib.tpw_wg_row(row, k + 1)
    keep = []
    for k, (ride_id, name, low) in enumerate(c["rides"]):
        arr, n = wide(name)
        keep.append(arr)
        lib.tpw_wg_ride(k, ride_id & 0xFFFF, arr, n)
    lib.tpw_wg_ride_count(len(c["rides"]))
    lib.tpw_wg_centre(c["centre"])
    args = []
    for text in c["names"]:
        arr, n = wide(text)
        keep.append(arr)
        args += [arr, n]
    lib.tpw_wg_names(*args)
    lib.tpw_wg_snapshot()
    fixed = {L_WORLD: lib.tpw_wg_ptr(L_WORLD), L_SCRIPT: lib.tpw_wg_ptr(L_SCRIPT)}
    if c["centre"]:
        fixed[L_CENTRE] = lib.tpw_wg_ptr(L_CENTRE)
    for k in range(len(c["blocks"])):
        fixed[L_BLOCK + k] = lib.tpw_wg_ptr(L_BLOCK + k)
    CL.host_begin(lib, fixed, c["q"])
    try:
        r = lib.tpw_wg_call(code, T.s32(c["a"]), T.s32(c["b"]))
    finally:
        log = CL.host_end(lib)
    result = {}
    if c["op"] == "leg":
        result["ret"] = r & 0xFFFF
        result["outguard"] = 1
    elif c["op"] in ("status", "clear", "waiting", "left", "sea"):
        result["ret"] = T.s32(r)
    w = (C.c_uint32 * 8)()
    lib.tpw_wg_get_world(w)
    result["fields"] = [w[i] for i in range(6)]
    result["worldfill"] = w[6]
    blocks = (C.c_uint32 * 16)()
    lib.tpw_wg_get_blocks(blocks)
    result["blocks"] = [blocks[k] for k in range(len(c["blocks"]))]
    n = (C.c_uint32 * 4)()
    lib.tpw_wg_get_new(n)
    result["new"] = (n[0], n[1], n[2])
    result["centre"] = lib.tpw_wg_centre_label()
    result["log"] = log
    return result


# ------------------------------------------------------------------ the cases

STATUSES = [0, 1, 2, 3, 4, 5, 6, 7, -1, T.s32(0x80000000), 0x7FFFFFFF]
FLAGS = [0, 1, 2, -1, T.s32(0x80000000), 0x7FFFFFFF]


def objects(flag_list, base=5):
    """A chain of objects (list head = the first), one per flag word; returns (blocks, rows, head)."""
    blocks, rows = [], {}
    for i, fl in enumerate(flag_list):
        nxt = base + i + 1 if i + 1 < len(flag_list) else 0
        blocks.append(block(flags=fl, f0c=nxt))
        rows[base + i] = i
    return blocks, rows, (base if flag_list else 0)


def park(open_, gate=0, closed=0, lst=(), cq=0, visitors=0, gstatus=1, openable=(1,), centre=0, analyser=True, row0=False, newnull=0):
    blocks, rows, head = objects(list(lst), base=5)
    shift = len(blocks)
    blocks.append(block())
    rows[1] = len(blocks) - 1
    if analyser:
        blocks.append(block())
        rows[3] = shift + 1
    if row0:
        blocks.append(block())
        rows[0] = len(blocks) - 1
    return mk("set", a=open_, b=gate, w=dict(closed=closed, list=head, analyser=3), blocks=blocks, rows=rows, centre=centre,
              q=dict(closed=[cq], visitors=[visitors], gstatus=[gstatus], openable=list(openable), newnull=[newnull]))


def edges_set():
    for gate in FLAGS:
        for open_ in (0, 1):
            yield park(open_, gate=gate, closed=open_)
    for open_ in (0, 1, 7, -1):
        for closed in (0, 1, 2, 0x80000000):
            for cq in (0, 1):
                yield park(open_, closed=closed, cq=cq, lst=[4, 0, 4])
    for visitors in (0, 3, -1):
        for gstatus in (1, 0, -1, 2):
            yield park(0, visitors=visitors, gstatus=gstatus, cq=1)
    for lst in ([], [4], [0], [0xFFFB], [4, 4, 0, 4], [0x7FFF, 0xFFFF, 5, 0x104]):
        for open_ in (0, 1):
            for closed in (0, 1):
                for oq in ([0], [1], [2], [-1], [1, 0, 1, 0]):
                    yield park(open_, closed=closed, lst=lst, cq=1, openable=oq)
    for centre in (0, 1):
        for open_ in (0, 1):
            yield park(open_, centre=centre, closed=1 - open_)
    yield park(0, centre=0, newnull=1)
    yield park(1, centre=0, newnull=1, closed=1)
    yield park(0, cq=1, analyser=False)
    yield park(0, row0=True, lst=[4], cq=1)
    yield park(1, row0=True, lst=[4], closed=1)


def rand_set(r):
    return park(r.choice([0, 1, r.randrange(-5, 9)]), gate=r.choice([0, 0, 0, r.choice(FLAGS)]), closed=r.choice([0, 1, r.getrandbits(32)]),
                lst=[r.choice([0, 4, 0xFFFB, r.getrandbits(16)]) for _ in range(r.randrange(0, 5))], cq=r.randrange(2),
                visitors=r.choice([0, 0, r.randrange(-3, 9)]), gstatus=r.choice([1, 1, 0, -1, r.randrange(-3, 4)]),
                openable=[r.choice([0, 1, 2, -1]) for _ in range(r.randrange(1, 4))], centre=r.randrange(2),
                analyser=r.random() < 0.9, row0=r.random() < 0.1, newnull=int(r.random() < 0.05))


def gates(op, flag, handle=None, row0=False, var=1, gates_id=1):
    blocks = [block(f20=handle)]
    rows = {gates_id: 0}
    if row0:
        blocks.append(block())
        rows[0] = 1
    return mk(op, a=flag, w=dict(gates=gates_id), blocks=blocks, rows=rows, q=dict(var=[var]))


def edges_gates(op):
    def gen():
        for flag in FLAGS:
            for var in (1, 0, -1, 7):
                yield gates(op, flag, var=var)
        for handle in (0, 1, 0x1000, T.s32(0x80000000), -1, 0x7FFFFFFF):
            yield gates(op, 0, handle=handle)
            yield gates(op, 5, handle=handle)
        for gid in (1, 2, 0x27FF, 0x100):
            yield gates(op, 1, gates_id=gid)
        yield gates(op, 0, row0=True)
        yield gates(op, 1, row0=True)
    return gen


def rand_gates(op, r):
    return gates(op, r.choice(FLAGS + [r.getrandbits(32)]), handle=r.choice([None, r.getrandbits(32)]), row0=r.random() < 0.1,
                 var=r.choice([1, 0, -1, r.randrange(-9, 9)]), gates_id=r.choice([1, 2, 0x27FF, r.randrange(1, ROWS)]))


def vehicle(op, status=(1,), veh=5, bus=0, sea=0, ferry=0, people=0, leg=0, row0=False, other_id=6, **kw):
    """A case with the vehicle in row `veh` (block 0, script handle f20), status answers `status`."""
    blocks = [block()]
    rows = {veh: 0} if veh else {}
    for ident in (bus, sea, ferry):
        if ident and ident not in rows:
            blocks.append(block())
            rows[ident] = len(blocks) - 1
    if row0:
        blocks.append(block())
        rows[0] = len(blocks) - 1
    q = dict(var=list(status))
    q.update(kw.pop("q", {}))
    return mk(op, a=leg, w=dict(veh=veh, bus=bus, sea=sea, ferry=ferry, people=people), blocks=blocks, rows=rows, q=q, **kw)


def edges_status():
    yield vehicle("status", veh=0)
    yield vehicle("status", veh=0, row0=True)
    for s in STATUSES:
        yield vehicle("status", status=[s])
        yield vehicle("status", status=[s], veh=0x27FF)
    yield vehicle("status", status=[6], row0=True)
    yield vehicle("status", status=[6], veh=1)


def rand_status(r):
    return vehicle("status", status=[r.choice(STATUSES + [r.randrange(-9, 9)])], veh=r.choice([0, 5, r.randrange(1, ROWS)]),
                   row0=r.random() < 0.1)


PEOPLE = [0, 1, 9, 10, 11, -1, 0x7FFFFFFF, T.s32(0x80000000), 3]


def edges_clear():
    yield vehicle("clear", veh=0)
    yield vehicle("clear", veh=0, bus=5)
    for bus in (5, 0, 7):                                   # bus == vehicle: not seagoing
        for people in PEOPLE:
            for st in itertools_product3():
                yield vehicle("clear", status=list(st), bus=bus, people=people)
    for people in PEOPLE:
        yield vehicle("clear", status=[6], bus=5, people=people)
        yield vehicle("clear", status=[1, 6, 6], bus=5, people=people)
        yield vehicle("clear", status=[0, 0, 6], bus=5, people=people)
    yield vehicle("clear", bus=5, row0=True)


def itertools_product3():
    values = (0, 1, 2, 3, 6, 5, -1)
    for a in values:
        for b in (0, 1, 2, 6, 4):
            for c in (0, 2, 1, 6, 7):
                yield (a, b, c)


def rand_clear(r):
    return vehicle("clear", status=[r.choice(STATUSES) for _ in range(r.randrange(1, 4))], veh=r.choice([0, 5, 5, 5]),
                   bus=r.choice([0, 5, 5, 9]), people=r.choice(PEOPLE + [r.randrange(-20, 30)]), row0=r.random() < 0.05)


def edges_left():
    for p in PEOPLE + [100, 0x12345678]:
        yield mk("left", w=dict(people=p))


def edges_sea():
    for veh, bus in ((0, 0), (0, 5), (5, 5), (5, 6), (6, 5), (5, 0), (0xFFFF, 0xFFFF), (0xFFFF, 0), (1, 0x100), (0x100, 1), (0x8000, 0x8000)):
        yield mk("sea", w=dict(veh=veh, bus=bus))


def rand_sea(r):
    veh = r.choice([0, 5, r.getrandbits(16)])
    return mk("sea", w=dict(veh=veh, bus=r.choice([0, veh, veh, r.getrandbits(16)])))


def rand_left(r):
    return mk("left", w=dict(people=r.getrandbits(32)))


# the cells (as the Mac computes them): stop, stop+1, stop-0x100, stop-0xff, each cut to 16 bits
def cells_of(stops):
    stop = lambda i: stops[min(i, len(stops) - 1)]
    return [stop(0) & 0xFFFF, (stop(1) + 1) & 0xFFFF, (stop(2) - 0x100) & 0xFFFF, (stop(3) + 1 - 0x100) & 0xFFFF]


def waiting(stops, placed, kinds=(1,), states=(0x15,), blocks_none=False):
    """placed: which of the four cells hold a person (their rows are 1 to 4); kinds and states: per placed person."""
    cells = cells_of(stops)
    blocks, rows, mwho = [], {}, {}
    for n, i in enumerate(placed):
        row = n + 1
        blocks.append(block(kind=kinds[min(n, len(kinds) - 1)], w220=states[min(n, len(states) - 1)]))
        rows[row] = n
        mwho[cells[i]] = row
    return mk("waiting", size=BIG_SIZE, fill=0, cells=mwho, blocks=blocks, rows=rows, q=dict(stop=list(stops)))


STOPS = [1, 0x80, 0x100, 0xFFFF, 0, 0x101, 0x4000, 0x7FFF, 0xFF, 0x8000, 0x2000]


def edges_waiting():
    for stop in STOPS:
        yield waiting([stop], [])
        for i in range(4):
            yield waiting([stop], [i])
            yield waiting([stop], [i], kinds=[2])
            yield waiting([stop], [i], states=[0x14])
            yield waiting([stop], [i], kinds=[0, 1], states=[0x15, 0x16])
        yield waiting([stop], [0, 1, 2, 3], kinds=[1, 1, 1, 1], states=[0, 0, 0, 0x15])
        yield waiting([stop], [3, 2, 1, 0], kinds=[3, 1, 1, 1], states=[0x15, 0x15, 0x15, 0x15])
        yield waiting([stop], [1, 2], kinds=[1, 3], states=[0x15, 0x15])
        yield waiting([stop], [0, 2], kinds=[1, 1], states=[0x1, T.s32(0x80000000)])
    yield waiting([5, 0x1000, 7, 0xFFFF], [0, 3])
    yield waiting([5, 0x1000, 7, 0xFFFF], [1, 2, 3])
    yield waiting([0x80, 0x90, 0x100, 0xFFFF], [2])


def rand_waiting(r):
    stops = [r.choice(STOPS + [r.getrandbits(16)]) for _ in range(r.choice([1, 4]))]
    placed = r.sample(range(4), r.randrange(0, 5))
    return waiting(stops, placed, kinds=[r.choice([1, 1, 2, 0xFF]) for _ in placed] or (1,),
                   states=[r.choice([0x15, 0x15, 0, 0x14, r.getrandbits(32)]) for _ in placed] or (0x15,))


def leg_names():
    return [("bus", NAMES), ("seaplane", NAMES), ("Ferry", NAMES)]


def leg_case(leg, start=0x1234, veh=0, slots=(0, 0, 0), rides=(), q=None, names=NAMES):
    blocks, rows = [], {}
    wv = dict(start=start, veh=veh, bus=slots[0], sea=slots[1], ferry=slots[2])
    ids = [x for x in (veh,) + tuple(slots) if x]
    for ident in ids:
        if ident not in rows:
            blocks.append(block())
            rows[ident] = len(blocks) - 1
    return mk("leg", a=leg, w=wv, blocks=blocks, rows=rows, rides=list(rides), q=q or {}, names=names)


def choice_of(start):
    s = (start * 0x19660D + 0x3C6EF35F) & MASK
    v = s - (1 << 32) if s >> 31 else s
    if v < 0:
        v = -v
    return (v & MASK) % 3


def start_for(choice, r=None):
    for s in range(1, 100):
        if choice_of(s) == choice:
            return s
    return 0


RIDES_ALL = [(0x11, "bus", 1), (0x22, "seaplane", 2), (0x33, "Ferry", 3)]
RIDE_SETS = [
    [], RIDES_ALL, RIDES_ALL[::-1], [(7, "other", 0)] + RIDES_ALL, [(0x44, "BUS", 5)], [(0x44, "SEAPLANE", 5)], [(0x44, "ferry", 5)],
    [(0x33, "Ferry", 3)], [(0x22, "seaplane", 2)], [(0x11, "bus", 1)], [(0x7FFF, "Ferry", 0x1234)], [(0xFFFF, "bus", 0xFFFF)],
    [(1, "ferries", 0), (2, "bu", 0)], [(0x10, "", 0)],
]


def edges_leg():
    for veh in (5, 0x27FF):
        for leg in (0, 1, 2, 3):
            yield leg_case(leg, veh=veh)
    for leg in (1, 2, 3):
        for rides in RIDE_SETS:
            yield leg_case(leg, rides=rides)
            yield leg_case(leg, rides=rides, slots=(0, 0, 0), q=dict(wcsicmp=[0]))
    for start in (0, 1, 2, 3, 0x80000000, 0xFFFFFFFF, 0x7FFFFFFF, 0x12345678, start_for(0), start_for(1), start_for(2), 0x2F4A2B1E):
        for rides in RIDE_SETS[:4] + RIDE_SETS[7:10]:
            yield leg_case(0, start=start, rides=rides)
    for slots in [(7, 0, 0), (0, 8, 0), (0, 0, 9), (7, 8, 9), (7, 8, 0), (0, 8, 9), (7, 0, 9)]:
        for leg in (0, 1, 2, 3):
            yield leg_case(leg, start=start_for(leg % 3), slots=slots, rides=RIDES_ALL)
    for ident_pair in ((0x11, 0x2222), (0xFFFF, 0), (0, 0x7FFF)):
        yield leg_case(1, rides=[(ident_pair[0], "bus", ident_pair[1])], q=dict(id=[0x4321], handle=[0xDEAD]))
    yield leg_case(2, rides=RIDES_ALL, q=dict(id=[0], handle=[0]))
    yield leg_case(3, rides=RIDES_ALL, q=dict(id=[0xFFFF], handle=[T.s32(0x80000000)]))
    yield leg_case(1, rides=RIDES_ALL, names=("buzz", "seaplane", "Ferry"))
    yield leg_case(2, rides=RIDES_ALL, names=("bus", "Seaplane", "FERRY"))
    yield leg_case(3, rides=[(5, "FERRY", 0)], names=("bus", "seaplane", "ferry"))
    yield leg_case(1, rides=RIDES_ALL, q=dict(wcsicmp=[1, 1, 1, 1, 1, 1, 1, 0]))
    yield leg_case(2, rides=RIDES_ALL, q=dict(wcsicmp=[1, -1, 7, 0]))


def rand_leg(r):
    slots = tuple(r.choice([0, 0, 7, 8, 9]) for _ in range(3))
    slots = tuple(s + i * 0 for i, s in enumerate(slots))
    veh = r.choice([0, 0, 0, 0, 5])
    rides = [(r.getrandbits(16), r.choice(["bus", "seaplane", "Ferry", "BUS", "Seaplane", "x", "ferry"]), r.getrandbits(16))
             for _ in range(r.randrange(0, 4))]
    return leg_case(r.choice([0, 1, 2, 3]), start=r.getrandbits(32), veh=veh, slots=slots, rides=rides,
                    q=dict(id=[r.getrandbits(16)], handle=[r.getrandbits(32)]))


def tests():
    edge = {
        "set": edges_set, "open": edges_gates("open"), "close": edges_gates("close"), "leg": edges_leg, "status": edges_status,
        "clear": edges_clear, "waiting": edges_waiting, "left": edges_left, "sea": edges_sea,
    }
    rnd = {
        "set": rand_set, "open": lambda r: rand_gates("open", r), "close": lambda r: rand_gates("close", r), "leg": rand_leg,
        "status": rand_status, "clear": rand_clear, "waiting": rand_waiting, "left": rand_left, "sea": rand_sea,
    }
    return [FnTest(OPS[op][2], ppc, host, edge=edge[op], rnd=rnd[op]) for op in OPS]


TESTS = tests()
