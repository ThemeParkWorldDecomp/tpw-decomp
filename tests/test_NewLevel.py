"""Differential tests for the theme and the new level (unit w117, main app), against libtpw_unit_NewLevel.dylib (tests/unit_NewLevel.cpp):

  src/Game_level.cpp           CGame::InitTheme (0x10147ee4), InitLevel (0x10148028), InitLevel_Load (0x1014809c), InitLevel_Initialise
                               (0x101482bc), LoadTheme (0x1014960c)
  src/World_setup.cpp          CWorld::SetupNewLevel (0x1013a2ac)
  src/FS_LoadSave_load.cpp     CFSLoadSave::Load (0x101fc408), LoadInternal (0x101fc5e0)
  src/LLCellSystem_costs.cpp   LLCC_SystemSetPathCost (0x10091b24), SetQueueCost (0x10091b9c), SetKartTrackCost (0x10091c14), SetWaterTrackCost
                               (0x10091c94)
  src/SkyObjects_ctor.cpp      CSkyObjects::CSkyObjects (0x1015b5f0)

The 13 functions run as the original on the PowerPC side and as src/ on the host, and call each other for real (an InitTheme case runs the
whole chain). Every other callee is a fake on both sides that records the same event (name, integers, texts) and answers from the case's
queues: the balance reader's Load, the ride database (LoadTheme and the walk over the case's rides), the ten thing constructors and CObject's
(each writes the next id of the queue "id" into the object's first half word), GetThingId, the control manager, action recorder, map, macro
AI, research lab, bitmap system and pilots, the sky, the display's fog, the sky objects, the message event, centre and headers, the user
info, the string conversions, CSaveLoad::Load, the traces, `time`, _wcsicmp and operator new (its size). The bullfrog strings are functional
and unrecorded. The game object, the worlds and the lists the case needs are raw memory filled with a byte, so a store that does not happen
shows. What a case compares: the answer, the event log, every global the functions write (bit for bit), the pointers they set (by label),
the world's last 0x4c bytes by field and that every other byte of the world still holds the fill.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_NewLevel"

# op -> (the host's call number, the original's entry)
OPS = {
    "initTheme": (0, 0x10147EE4), "initLevel": (1, 0x10148028), "levelLoad": (2, 0x1014809C), "levelInit": (3, 0x101482BC),
    "loadTheme": (4, 0x1014960C), "setup": (5, 0x1013A2AC), "load": (6, 0x101FC408), "loadInternal": (7, 0x101FC5E0),
    "setPath": (8, 0x10091B24), "setQueue": (9, 0x10091B9C), "setKart": (10, 0x10091C14), "setWater": (11, 0x10091C94),
    "skyCtor": (12, 0x1015B5F0),
}
OWN = {entry for _, entry in OPS.values()}
WORLD_OPS = ("setup", "initTheme")
WORLD_SIZE = 0x1DA748
TAIL = 0x1DA6FC                  # the world's last 0x4c bytes are compared by field
GLOBALS = 0x103B74C8             # gTPGlobals
BALANCE = 0x102B4568             # gMainBalance (value slot i at +8 + 4 i)
HOARD_A, HOARD_B = 0x102B434C, 0x102B4130
GAME_TYPE, GAME_GUARD = 0x102B3AA0, 0x102B3AA4
GATES, LIGHTS = 0x1034CB18, 0x1034CAF8
WIDE_CTOR_TV, WIDE_DTOR_TV = 0x102657E8, 0x10267288      # __construct_array's constructor and destructor tables
RIDE_RECORD = 0x700

# Global -> (PowerPC address, count, kind). kind "w": 32-bit words; "p": a pointer (label); "b": bytes.
STATE = {
    "extraX": (0x102C6C58, 1, "w"), "extraY": (0x102C6C54, 1, "w"), "ambient": (0x102C6C98, 1, "w"), "lightColour": (0x102C6C94, 1, "w"),
    "boundX": (0x102A4D6C, 1, "w"), "boundY": (0x102A4D70, 1, "w"), "lightDir": (0x102C6C88, 3, "w"), "waterStart": (0x102F9BC8, 3, "w"),
    "advOn": (0x102B354C + 0x144, 4, "w"), "advOff": (0x102B354C + 0x154, 4, "w"),
    "cost0": (0x102E47DC, 1, "w"), "cost1": (0x102E47D8, 1, "w"), "cost2": (0x102E47D4, 1, "w"), "cost3": (0x102E47D0, 1, "w"),
    "bitmap": (0x1034CBB8, 0x20, "b"),
    "messageCentre": (0x1037F6C4, 1, "p"), "userInfo": (0x10380B34, 1, "p"), "gameWorld": (0x1037EBFC, 1, "p"),
}
OUTPUTS = [n for n, v in STATE.items() if v[2] != "p"]            # the words and bytes a case fills with random values first
NARROW_TEXTS = {"hoardings": (0x40EC, "hoardings.sam"), "endhoardings": (0x4100, "endhoardings.sam"), "easy": (0x427C, "Easy_"),
                "separator": (0x404C, ":")}
WIDE_TEXTS = {"ext": (0x4420, ".TPWS"), "wsep": (0x44C0, ":")}
MASK = 0xFFFFFFFF

# ------------------------------------------------------------------ helpers


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def wide_read(e, a, limit=1000):
    out = []
    while a and len(out) < limit:
        ch = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if ch == 0:
            break
        out.append(chr(ch))
    return "".join(out)


def s_read(e, obj, wide):
    data = e.u32(obj + 8)
    if not data:
        return ""
    return wide_read(e, data) if wide else e.cstr(data, limit=1000)


def s_write(e, obj, text, wide):
    """A TbDynamicStringTemplate in the original's layout {vtable, capacity, data, length, grow-by}."""
    if wide:
        blob = b"".join(struct.pack(">H", ord(ch) & 0xFFFF) for ch in text) + b"\0\0"
    else:
        blob = text.encode("latin-1") + b"\0"
    data = e.alloc(len(blob), blob, alignment=4)
    e.write(obj, struct.pack(">IIIII", CL.FAKE_VT, 0, data, len(text), 0))


def string_stubs(S):
    """The bullfrog string imports, narrow (c) and wide (w): functional, unrecorded."""
    for t, wide in (("c", False), ("w", True)):
        def cstr(e, a, wide=wide):
            return wide_read(e, a) if wide else e.cstr(a, limit=1000)

        def ct0(e, wide=wide):
            s_write(e, e.r(3), "", wide)

        def ct_text(e, wide=wide, cstr=cstr):
            s_write(e, e.r(3), cstr(e, e.r(4)), wide)

        def ct_copy(e, wide=wide):
            s_write(e, e.r(3), s_read(e, e.r(4), wide), wide)

        def append(e, wide=wide):
            s_write(e, e.r(3), s_read(e, e.r(3), wide) + s_read(e, e.r(4), wide), wide)

        def append_text(e, wide=wide, cstr=cstr):
            s_write(e, e.r(3), s_read(e, e.r(3), wide) + cstr(e, e.r(4)), wide)

        S.imp(f"__ct__26TbDynamicStringTemplate<{t}>Fv", ct0)
        S.imp(f"__ct__26TbDynamicStringTemplate<{t}>FPC{t}", ct_text)
        S.imp(f"__ct__26TbDynamicStringTemplate<{t}>FRC26TbDynamicStringTemplate<{t}>", ct_copy)
        S.imp(f"__ct__26TbDynamicStringTemplate<{t}>FRC15TbStringBase<{t}>", ct_copy)
        S.imp(f"__dt__26TbDynamicStringTemplate<{t}>Fv", lambda e: None)
        S.imp(f"__as__26TbDynamicStringTemplate<{t}>FRC26TbDynamicStringTemplate<{t}>", ct_copy_assign(wide))
        S.imp(f"__as__26TbDynamicStringTemplate<{t}>FRC15TbStringBase<{t}>", ct_copy_assign(wide))
        S.imp(f"__as__26TbDynamicStringTemplate<{t}>FPC{t}", assign_text(wide))
        S.imp(f"__apl__26TbDynamicStringTemplate<{t}>FRC15TbStringBase<{t}>", append)
        S.imp(f"__apl__26TbDynamicStringTemplate<{t}>FPC{t}", append_text)


def ct_copy_assign(wide):
    def assign(e):
        s_write(e, e.r(3), s_read(e, e.r(4), wide), wide)
    return assign


def assign_text(wide):
    def assign(e):
        a = e.r(4)
        s_write(e, e.r(3), wide_read(e, a) if wide else e.cstr(a, limit=1000), wide)
    return assign


# ------------------------------------------------------------------ the PowerPC side


class Objects:
    """The PowerPC memory of one case."""


def build_objects(e, c, op):
    o = Objects()
    fill = c["fill"]
    o.world = e.alloc(WORLD_SIZE, bytes([fill]) * WORLD_SIZE) if op in WORLD_OPS else e.alloc(16, bytes([fill]) * 16)
    o.other = e.alloc(64, bytes([fill]) * 64)
    o.game = e.alloc(0x40, b"\xa5" * 0x40)
    o.centre = e.alloc(0x1D0, b"\xa5" * 0x1D0)
    o.sky = e.alloc(0x10, b"\xa5" * 0x10)
    o.user = e.alloc(16, b"\xa5" * 16)
    o.research = e.alloc(16, b"\0" * 16)
    o.theme = CL.new_text(e, c["themePath"])
    o.find = CL.new_text(e, c["find"] if c["find"] is not None else "")
    o.arg = CL.new_text(e, c["arg"])
    o.rides = []
    for ride_id, name, low in c["rides"]:
        blob = bytearray(b"\xa5" * RIDE_RECORD)
        struct.pack_into(">I", blob, 0, ((ride_id & 0xFFFF) << 16) | (low & 0xFFFF))
        text = b"".join(struct.pack(">H", ord(ch) & 0xFFFF) for ch in name[:0x3F]) + b"\0\0"
        blob[0x68C:0x68C + len(text)] = text
        o.rides.append(e.alloc(RIDE_RECORD, bytes(blob)))
    o.display = e.data_import("gei_Display")
    o.sky_engine = e.data_import("gei_Sky")
    return o


def install(e, L, c, o, op):
    S = CL.Stubs()
    S.real = set(OWN)
    R = CL.recorder
    string_stubs(S)
    cursor = [0]

    def which(a, offset):
        return 1 if a == o.world + offset else 2 if a == o.other + offset else -1

    def word(offset):
        # a word of the world under test now (the host's WorldWord)
        return e.u32(o.world + offset) if op in WORLD_OPS else 0

    def digest():
        # the sixteen thing ids at 0x1da718 (the host's IdDigest)
        if op not in WORLD_OPS:
            return 0
        h = 0
        for i in range(16):
            h = (h * 31 + struct.unpack(">H", e.read(o.world + 0x1DA718 + 2 * i, 2))[0]) & MASK
        return h

    def internal(addr, name, spec="", ret=None, then=None):
        S.internal(addr, R(name, spec, ret, then))

    # --- debug output
    S.internal(0x1000DDE8, lambda e: L.rec("GAME_ASSERTMSG", (T.s32(e.r(3)),), (e.cstr(e.r(4))[:159],)))
    S.internal(0x1006FCBC, lambda e: L.rec("CTRACE", (e.r(3),), (T.c_format(e, e.cstr(e.r(4)), 5)[:159],)))
    S.internal(0x1006FC90, lambda e: L.rec("DEBUG_MESSAGE", (e.r(4),), (e.cstr(e.r(3))[:159],)))

    # --- the balance reader and the ride database
    def balance_load(e):
        L.rec("Balance.Load", (L.lab(e.r(3)), T.s32(e.r(5))), (e.cstr(e.r(4))[:159],))
        e.set_r(3, T.u32(L.ret("Balance.Load", 1)))

    S.internal(0x10020134, balance_load)

    def ride_theme(e):
        L.rec("RideDB.LoadTheme", (L.lab(e.r(3)),), (e.cstr(e.r(4))[:159],))
        e.set_r(3, T.u32(L.ret("RideDB.LoadTheme", 1)))

    S.internal(0x10154EA8, ride_theme)

    def first_ride(e):
        L.rec("GetFirstRide", (L.lab(e.r(3)),))
        cursor[0] = 0
        e.set_r(3, o.rides[0] if o.rides else 0)

    def next_ride(e):
        L.rec("GetNextRide", (L.lab(e.r(3)),))
        cursor[0] += 1
        e.set_r(3, o.rides[cursor[0]] if cursor[0] < len(o.rides) else 0)

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

    S.imp("_wcsicmp__10NS_MacDozeFPCwPCw", wcsicmp)

    # --- the level, the theme and the engine
    S.internal(0x1015139C, lambda e: (L.rec("get_theme_path"), e.set_r(3, o.theme))[1])
    S.internal(0x100597F8, lambda e: (L.rec("FindFilePath", (e.r(4), L.lab(e.r(5))), (e.cstr(e.r(3))[:159],)),
                                      e.set_r(3, 0 if c["find"] is None else o.find))[1])
    internal(0x1007A0B8, "OBJECT_Load", "s")
    S.imp("SkyLoad__4CSkyFPc", lambda e: (L.rec("SkyLoad", (L.lab(e.r(3)),), (e.cstr(e.r(4))[:159] if e.r(4) else "(null)",)), e.set_r(3, 0))[1])
    S.imp("SetFogColor__8CDisplayFUl", lambda e: L.rec("SetFogColor", (L.lab(e.r(3)), e.r(4))))
    internal(0x1015B688, "InitSkyObjects", "N")

    def add_separator(e):
        value = s_read(e, e.r(3), False)
        if value == "" or value[-1] != ":":
            s_write(e, e.r(3), value + s_read(e, GLOBALS + 0x404C, False), False)

    S.internal(0x10004BC0, add_separator)

    # --- the things
    def write_id(e, obj):
        e.write(obj, struct.pack(">H", L.ret("id", 0x1234) & 0xFFFF))

    def thing(name):
        def stub(e):
            L.rec(name, (L.newlab(e.r(3)), word(0x1DA714), digest()))
            write_id(e, e.r(3))
        return stub

    for name, addr in (("CStaffHQ", 0x1012A7FC), ("CMechanicHQ", 0x1010521C), ("CAdvisor", 0x10009F9C), ("CTagSystem", 0x1012C57C),
                       ("CParkAnalyser", 0x100E2A4C), ("CResearchLab", 0x10121AD0), ("CWeather", 0x10137DA0),
                       ("CBankAccount", 0x100F29F4), ("CUIMsgReceiver", 0x101305B0), ("CChallengeHQ", 0x100F6640)):
        S.internal(addr, thing(name))

    def cobject(e):
        L.rec("CObject", (L.newlab(e.r(3)), (e.r(4) >> 16) & 0xFFFF, (e.r(5) >> 16) & 0xFFFF, T.s32(e.r(6)), T.s32(e.r(7)), digest()))
        write_id(e, e.r(3))

    S.internal(0x10105D74, cobject)

    def thing_id(e):
        L.rec("GetThingId", (L.lab(e.r(4)),))
        e.write(e.r(3), e.read(e.r(4), 2))

    S.internal(0x1012E1F8, thing_id)
    S.internal(0x10121BD4, lambda e: L.rec("CResearchLab::Init", (L.lab(e.r(3)),)))
    S.internal(0x1013ECE8, lambda e: (L.rec("GetResearchLab", (which(e.r(3), 0), digest())), e.set_r(3, o.research))[1])

    # --- the world's leaves
    S.internal(0x100FA2AC, lambda e: L.rec("SetupInitialControls", (which(e.r(3), 0x1D943C), 0x1D943C, word(0x1DA738))))
    S.internal(0x10143E08, lambda e: L.rec("ResetActionRecording", (L.lab(e.r(3)),)))
    S.internal(0x10100A8C, lambda e: L.rec("CMap::Init", (which(e.r(3), 0x2D8), 0x2D8, word(0x1DA70C), word(0x1DA708))))
    S.internal(0x1010096C, lambda e: L.rec("CMacroAI::Init", (which(e.r(3), 0), 0, word(0x1DA708), word(0x1DA714))))
    S.internal(0x10025A94, lambda e: L.rec("BMP_InitSystem"))
    S.internal(0x10134404, lambda e: L.rec("Pilot::init_behaviours"))
    S.internal(0x1016F5D8, lambda e: w32(e, e.r(3), c["ctorType"]))        # CGameType::CGameType: makes the type, records nothing

    # --- the first message
    S.internal(0x10151DF8, lambda e: L.rec("CMsgEvent", (L.newlab(e.r(3)), T.s32(e.r(4)))))
    S.internal(0x1015256C, lambda e: (L.rec("CMessageCentre::PostMessage", (L.lab(e.r(3)), L.lab(e.r(4)), word(0x1DA738))), e.set_r(3, 1))[1])

    def construct_array(e):
        L.rec("CMessageCentre", (L.newlab(e.r(3)), int(e.r(4) == WIDE_CTOR_TV), int(e.r(5) == WIDE_DTOR_TV), e.r(6), e.r(7)))
        e.set_r(3, e.r(3))

    S.imp("__construct_array", construct_array)

    # --- the park file
    S.internal(0x1017DE54, lambda e: L.rec("CFSUserInfo", (L.newlab(e.r(3)),)))

    def users_path(e):
        L.rec("ReturnUsersPathWithTheme", (L.lab(e.r(4)),))
        s_write(e, e.r(3), c["usersPath"], True)

    S.internal(0x1017F858, users_path)

    def uni_to_string(e):
        text = "".join(ch if ord(ch) <= 0xFF else "?" for ch in s_read(e, e.r(4), True))
        L.rec("UniStringToString", (), (text[:159],))
        s_write(e, e.r(3), text, False)
        e.set_r(3, len(text) + 1)

    S.internal(0x10005670, uni_to_string)

    def string_to_uni(e):
        text = s_read(e, e.r(4), False)
        L.rec("StringToUniString", (), (text[:159],))
        s_write(e, e.r(3), text, True)
        e.set_r(3, len(text) + 1)

    S.internal(0x100057A0, string_to_uni)

    def save_load(e):
        L.rec("CSaveLoad::Load", (L.lab(e.r(3)), L.lab(e.r(5)), T.s32(e.r(6))), (e.cstr(e.r(4))[:159],))
        e.set_r(3, T.u32(L.ret("CSaveLoad::Load", 1)))

    S.internal(0x10157FEC, save_load)

    # --- the C library and the allocator
    def op_new(e):
        size = e.r(3)
        L.rec("new", (size,))
        e.set_r(3, e.alloc(max(size, 4), b"\xa5" * max(size, 4)))

    S.imp("__nw__FUl", op_new)

    def time_(e):
        L.rec("time", (word(0x1DA70C),))
        e.set_r(3, L.ret("time", 0) & MASK)

    S.imp("time", time_)
    return S


def ppc_get(e, names):
    out = {}
    for name in names:
        addr, n, kind = STATE[name]
        out[name] = list(struct.unpack(">%dI" % n, e.read(addr, 4 * n))) if kind == "w" else \
            list(e.read(addr, n)) if kind == "b" else [e.u32(addr)]
    return out


def ppc_set(e, values):
    for name, words in values.items():
        addr, n, kind = STATE[name]
        if kind == "w":
            e.write(addr, struct.pack(">%dI" % n, *[w & MASK for w in words]))
        elif kind == "b":
            e.write(addr, bytes(words))


def world_fields(data, endian):
    """The last 0x4c bytes of a world: 12 bytes, four words, sixteen half words, a word, six half words."""
    head = list(data[:12])
    return head + list(struct.unpack(endian + "4I16HI6H", data[12:]))


def ppc(e, c):
    op = c["op"]
    code, entry = OPS[op]
    o = build_objects(e, c, op)
    fixed = {o.world: 1, o.other: 2, BALANCE: 3, HOARD_A: 4, HOARD_B: 5, o.centre: 6, o.sky: 7, o.user: 8, o.game: 9, o.research: 10,
             0x1037EC44: 11, 0x1037F6C8: 12, 0x10396A7C: 13, o.display: 14, o.sky_engine: 15, 0x1037FEFC: 16}
    L = CL.PpcLog(fixed, c["q"])
    e.cl = L
    S = install(e, L, c, o, op)
    saved = S.activate(e)
    out = {}
    try:
        # the world the case starts from
        for name, (addr, text) in NARROW_TEXTS.items():
            CL.tds_set(e, GLOBALS + addr, c["texts"].get(name, text))
        for name, (addr, text) in WIDE_TEXTS.items():
            CL.wds_set(e, GLOBALS + addr, c["wtexts"].get(name, text))
        CL.wds_set(e, GATES, c["wtexts"].get("gates", "gates"))
        CL.wds_set(e, LIGHTS, c["wtexts"].get("lights", "lights"))
        w32(e, o.display, c["display"])
        for slot, word in c["balance"].items():
            w32(e, BALANCE + 8 + 4 * slot, word)
        w32(e, GAME_TYPE, c["type"])
        e.write(GAME_GUARD, bytes([c["guard"] & 0xFF]))
        w32(e, o.game + 0x38, o.sky if c["existing"][1] else 0)
        w32(e, 0x1037F6C4, o.centre if c["existing"][0] else 0)
        w32(e, 0x10380B34, o.user if c["existing"][2] else 0)
        w32(e, 0x1037EBFC, {0: 0, 1: o.world, 2: o.other}[c["which"]])
        w32(e, 0x1037F2B4, c["loadFile"])
        e.write(0x1037F2B8, c["file"].encode("latin-1") + b"\0")
        ppc_set(e, c["init"])
        # the call
        args = {
            "initTheme": [o.game, o.arg], "initLevel": [o.game], "levelLoad": [o.game], "levelInit": [o.game], "loadTheme": [o.game, o.arg],
            "setup": [o.world, o.arg], "setPath": [c["value"]], "setQueue": [c["value"]], "setKart": [c["value"]], "setWater": [c["value"]],
        }
        if op == "skyCtor":
            block = e.alloc(0x20, b"\xa5" * 0x20)
            r3 = T.call_ppc(e, entry, [block])[0]
        elif op in ("load", "loadInternal"):
            name = e.alloc(20)
            CL.wds_set(e, name, c["name"])
            call_args = [0x10396A7C, name]
            if op == "loadInternal":
                extension = e.alloc(20)
                CL.wds_set(e, extension, c["ext"])
                call_args.append(extension)
            r3 = T.call_ppc(e, entry, call_args)[0]
        else:
            r3 = T.call_ppc(e, entry, args[op])[0]
        out["result"] = 0 if op in ("setPath", "setQueue", "setKart", "setWater", "skyCtor") else T.s32(r3)
        if op == "skyCtor":
            out["block"] = e.read(block, 0x20) == b"\xa5" * 0x20
        state = ppc_get(e, list(STATE))
        for name in ("messageCentre", "userInfo", "gameWorld"):
            state[name] = [L.lab(state[name][0])]
        state["skyObjects"] = [L.lab(e.u32(o.game + 0x38))]
        state["displayFlags"] = [e.u32(o.display)]
        out["state"] = state
        if op in WORLD_OPS:
            data = e.read(o.world, WORLD_SIZE)
            out["worldChanged"] = TAIL - data[:TAIL].count(bytes([c["fill"]]))
            out["world"] = world_fields(data[TAIL:], ">")
    finally:
        S.deactivate(e, saved)
        e.cl = None
    out["log"] = L.log
    return out


# ------------------------------------------------------------------ the host side


def host_address(lib, name):
    lib.tpw_nl_addr.restype, lib.tpw_nl_addr.argtypes = C.c_void_p, [C.c_char_p]
    return lib.tpw_nl_addr(name.encode())


HOST_NAMES = {"extraX": "extraX", "extraY": "extraY", "ambient": "ambient", "lightColour": "lightColour", "boundX": "boundX",
              "boundY": "boundY", "lightDir": "lightDir", "waterStart": "waterStart", "advOn": "advOn", "advOff": "advOff",
              "cost0": "cost0", "cost1": "cost1", "cost2": "cost2", "cost3": "cost3", "bitmap": "bitmap",
              "messageCentre": "messageCentre", "userInfo": "userInfo", "gameWorld": "gameWorld"}


def host_get(lib, names):
    out = {}
    for name in names:
        _, n, kind = STATE[name]
        addr = host_address(lib, HOST_NAMES[name])
        if kind == "w":
            out[name] = list((C.c_uint32 * n).from_address(addr))
        elif kind == "b":
            out[name] = list(C.string_at(addr, n))
        else:
            out[name] = [C.c_uint64.from_address(addr).value]
    return out


def host_set(lib, values):
    for name, words in values.items():
        _, n, kind = STATE[name]
        addr = host_address(lib, HOST_NAMES[name])
        if kind == "w":
            array = (C.c_uint32 * n).from_address(addr)
            for i, w in enumerate(words):
                array[i] = w & MASK
        elif kind == "b":
            C.memmove(addr, bytes(words), n)


def wide_array(text):
    codes = [ord(ch) & 0xFFFF for ch in text]
    return (C.c_uint16 * (len(codes) + 1))(*codes, 0), len(codes)


def host(lib, c):
    op = c["op"]
    code, _ = OPS[op]
    lib.tpw_nl_reset.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_nl_text.argtypes = [C.c_char_p, C.c_char_p]
    lib.tpw_nl_wtext.argtypes = [C.c_char_p, C.c_void_p, C.c_int32]
    lib.tpw_nl_balance.argtypes = [C.c_int32, C.c_uint32]
    lib.tpw_nl_ride.argtypes = [C.c_int32, C.c_uint16, C.c_void_p, C.c_int32]
    lib.tpw_nl_ride_count.argtypes = [C.c_int32]
    lib.tpw_nl_existing.argtypes = [C.c_int32] * 3
    lib.tpw_nl_find_null.argtypes = [C.c_int32]
    lib.tpw_nl_gametype.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_nl_world_changed.restype, lib.tpw_nl_world_changed.argtypes = C.c_int64, [C.c_int32, C.c_int64]
    lib.tpw_nl_call.restype = C.c_int64
    lib.tpw_nl_call.argtypes = [C.c_int32, C.c_int64, C.c_void_p, C.c_int32, C.c_void_p, C.c_int32]
    CL.host_begin(lib, {}, c["q"])
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    lib.tpw_nl_reset(c["fill"], c["which"])
    lib.tpw_nl_text(b"arg", c["arg"].encode("latin-1"))
    lib.tpw_nl_text(b"themePath", c["themePath"].encode("latin-1"))
    lib.tpw_nl_text(b"find", (c["find"] or "").encode("latin-1"))
    lib.tpw_nl_find_null(1 if c["find"] is None else 0)
    for name, text in c["texts"].items():
        lib.tpw_nl_text(name.encode(), text.encode("latin-1"))
    for name, text in c["wtexts"].items():
        array, n = wide_array(text)
        lib.tpw_nl_wtext(name.encode(), array, n)
    array, n = wide_array(c["usersPath"])
    lib.tpw_nl_wtext(b"users", array, n)
    lib.tpw_nl_text(b"file", c["file"].encode("latin-1"))
    C.c_uint32.from_address(host_address(lib, "displayFlags")).value = c["display"] & MASK
    C.c_int32.from_address(host_address(lib, "loadFile")).value = C.c_int32(c["loadFile"]).value
    for slot, word in c["balance"].items():
        lib.tpw_nl_balance(slot, word & MASK)
    for i, (ride_id, name, _) in enumerate(c["rides"]):
        array, n = wide_array(name)
        lib.tpw_nl_ride(i, ride_id & 0xFFFF, array, n)
    lib.tpw_nl_ride_count(len(c["rides"]))
    # the host's game type object is made once: the module sets its word to what the original would see
    lib.tpw_nl_gametype(T.s32(c["ctorType"] if (c["guard"] & 0xFF) == 0 else c["type"]), T.s32(c["ctorType"]))
    lib.tpw_nl_existing(*[int(x) for x in c["existing"]])
    host_set(lib, c["init"])
    first, nfirst = wide_array(c.get("name", ""))
    second, nsecond = wide_array(c.get("ext", ""))
    out = {}
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 1
    try:
        result = lib.tpw_nl_call(code, c["value"], first, nfirst, second, nsecond)
    finally:
        out["log"] = CL.host_end(lib)
    out["result"] = 0 if op in ("setPath", "setQueue", "setKart", "setWater", "skyCtor") else T.s32(result)
    if op == "skyCtor":
        out["block"] = C.string_at(host_address(lib, "skyBlock"), 0x20) == b"\xa5" * 0x20
    state = host_get(lib, list(STATE))
    for name in ("messageCentre", "userInfo", "gameWorld"):
        state[name] = [CL_lab(lib, state[name][0])]
    state["skyObjects"] = [CL_lab(lib, C.c_uint64.from_address(host_address(lib, "skyObjects")).value)]
    state["displayFlags"] = [C.c_uint32.from_address(host_address(lib, "displayFlags")).value]
    out["state"] = state
    if op in WORLD_OPS:
        world = host_address(lib, "worldA")
        out["worldChanged"] = lib.tpw_nl_world_changed(c["fill"], TAIL)
        out["world"] = world_fields(C.string_at(world + TAIL, WORLD_SIZE - TAIL), "<")
    return out


def CL_lab(lib, value):
    lib.tpw_cl_lab.restype, lib.tpw_cl_lab.argtypes = C.c_int32, [C.c_void_p]
    return lib.tpw_cl_lab(value) if value else 0


# ------------------------------------------------------------------ cases

WORDS = [0, 1, 2, 3, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x100, 0xFF, 0x12345678, 0xFF000000, 0x000000FF, 0xA5A5A5A5]
FLOAT_BITS = [0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x7F800000, 0xFF800000, 0x7FC00000, 0x7F800001, 0x7FA00000, 0xFFC00001,
              0x00000001, 0x007FFFFF, 0x00800000, 0x7F7FFFFF, 0x42DC0000, 0xC2C80000, 0x1E3CE508]
QUESTION = "abcXYZ019 _:.-"


def rw(r):
    return r.choice(WORDS) if r.random() < 0.5 else r.getrandbits(32)


def rf(r):
    return r.choice(FLOAT_BITS) if r.random() < 0.6 else r.getrandbits(32)


def rtext(r, limit=24, chars=QUESTION):
    return "".join(r.choice(chars) for _ in range(r.randrange(0, limit)))


def rwide(r, limit=20):
    chars = QUESTION + "éĀ€￿ÿ"
    return "".join(r.choice(chars) for _ in range(r.randrange(0, limit)))


def default_init():
    """Every output word 0xa5a5a5a5 (the bitmap's bytes 0xa5): the same on both sides, so a store shows."""
    return {name: ([0xA5] * n if kind == "b" else [0xA5A5A5A5] * n) for name, (_, n, kind) in STATE.items() if kind != "p"}


def base_case(op, **kw):
    c = dict(op=op, arg="data:levels:fantasy", themePath="data:levels:fantasy", find="data:levels:fantasy:Scape.omp", q={}, balance={},
             display=0, existing=(False, False, False), file="", loadFile=0, texts={}, wtexts={}, rides=[], fill=0xA5, which=1,
             type=0, guard=1, ctorType=0, usersPath="", name="", ext="", value=0, init=default_init())
    c.update(kw)
    return c


def random_init(r, names=None):
    out = {}
    for name in (names or OUTPUTS):
        _, n, kind = STATE[name]
        out[name] = [r.getrandbits(8) for _ in range(n)] if kind == "b" else [rw(r) for _ in range(n)]
    return out


# --- the setters and the CSkyObjects constructor
SETTERS = {"setPath": "cost0", "setQueue": "cost1", "setKart": "cost2", "setWater": "cost3"}


def setter_cases(op):
    def edge():
        r = random.Random(11)
        for v in (0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x100, 5, -5):
            yield base_case(op, value=v, init=random_init(r))

    def rnd(r):
        return base_case(op, value=T.s32(rw(r)), init=random_init(r))
    return edge, rnd


def sky_edges():
    yield base_case("skyCtor")


def sky_random(r):
    return base_case("skyCtor", init=random_init(r))


# --- LoadTheme
def theme_case(r=None, answer=1, path="data:levels:fantasy"):
    return base_case("loadTheme", arg=path, q={"RideDB.LoadTheme": [answer]}, init=random_init(r) if r else {})


def theme_edges():
    r = random.Random(12)
    for answer in (0, 1, -1, 0x10000, 2, 0x7FFFFFFF, -0x80000000):
        yield theme_case(r, answer)
    yield theme_case(r, 1, "")
    yield theme_case(r, 1, "x" * 900)


def theme_random(r):
    return theme_case(r, T.s32(rw(r)), r.choice(["", "a", "data:levels:space", rtext(r, 60)]))


# --- InitLevel_Initialise
SLOTS_READ = list(range(365, 369)) + list(range(419, 439))
FLOAT_SLOTS = list(range(424, 427)) + list(range(436, 439))


def init_balance(r, special=None):
    balance = {}
    for slot in SLOTS_READ:
        balance[slot] = rf(r) if slot in FLOAT_SLOTS else rw(r)
    if special:
        balance.update(special)
    return balance


def level_init_case(r, special=None, bounds=None):
    init = random_init(r)
    if bounds is not None:
        init["boundX"], init["boundY"] = [bounds[0]], [bounds[1]]
    return base_case("levelInit", balance=init_balance(r, special), init=init)


def level_init_edges():
    r = random.Random(13)
    for fog in (0, 0xFF000000, 0x000000FF, 0x12345678, 0x80000001):
        yield level_init_case(r, {419: fog, 420: fog ^ 0x01020304, 421: fog ^ 0x0F0F0F0F})
    offsets = (0, 1, -1, 5, -5, 0x7FFFFFFF, -0x80000000, 100, -100)
    for bx in offsets:
        for ox in (0, 3, -3, bx, bx - 1, bx + 1):
            yield level_init_case(r, {422: T.u32(ox), 423: T.u32(ox)}, (T.u32(bx), T.u32(bx)))
    for bits in FLOAT_BITS:
        yield level_init_case(r, {s: bits for s in FLOAT_SLOTS})
    yield level_init_case(r, {s: 0 for s in SLOTS_READ})
    yield level_init_case(r, {s: 0xFFFFFFFF for s in SLOTS_READ})
    yield level_init_case(r, {365: 11, 366: 22, 367: 33, 368: 44})


def level_init_random(r):
    bounds = (rw(r), rw(r)) if r.random() < 0.7 else None
    c = level_init_case(r, bounds=bounds)
    if r.random() < 0.3:
        c["balance"][422] = c["init"]["boundX"][0] if bounds else c["balance"][422]
        c["balance"][423] = c["init"]["boundY"][0] if bounds else c["balance"][423]
    return c


# --- InitLevel_Load / InitLevel
PATHS = ["data:levels:fantasy", "data:levels:fantasy:", "", ":", "a", "data:levels:space::", "x" * 300, "ab:cd"]
FINDS = ["data:levels:fantasy:Scape.omp", "", "a", "x" * 200]
SEPARATORS = [":", "::", "", "/", "\\"]


def level_load_case(op, r, **kw):
    existing = (r.random() < 0.3, kw.pop("sky", r.random() < 0.3), r.random() < 0.3)
    c = base_case(op, themePath=kw.pop("themePath", r.choice(PATHS) if r.random() < 0.6 else rtext(r, 40)),
                  find=kw.pop("find", None if r.random() < 0.25 else r.choice(FINDS + [rtext(r, 30)])),
                  display=kw.pop("display", r.choice([0, 0x400000, 0xFFFFFFFF, 0xFFBFFFFF, 0x410100, 0x3FFFFF, 0x800000, r.getrandbits(32)])),
                  existing=existing, init=random_init(r))
    c["balance"] = init_balance(r)
    c["balance"][439] = kw.pop("numSky", r.choice([0, 1, 2, 4, 5, T.u32(-1), 0x80000000, 0x7FFFFFFF, r.getrandbits(32)]))
    c["q"] = {"Balance.Load": kw.pop("loads", [r.choice([0, 1]), r.choice([0, 1, 1])])}
    c["texts"] = {"hoardings": r.choice(["hoardings.sam", "", "h", "hoardings.sam:"]),
                  "endhoardings": r.choice(["endhoardings.sam", "", "e"]), "separator": r.choice(SEPARATORS)}
    c.update(kw)
    return c


def level_load_edges(op):
    def edge():
        r = random.Random(14 if op == "levelLoad" else 15)
        for display in (0, 0x400000, 0xFFFFFFFF, 0xFFBFFFFF):
            for numSky in (0, 1, T.u32(-1), 0x80000000, 5):
                for sky in (False, True):
                    yield level_load_case(op, r, display=display, numSky=numSky, sky=sky, loads=[1, 1])
        for loads in ([1, 1], [0, 1], [1, 0], [0, 0]):
            yield level_load_case(op, r, loads=loads)
        for path in PATHS:
            yield level_load_case(op, r, themePath=path, loads=[1, 1])
        yield level_load_case(op, r, find=None, loads=[1, 1])
        yield level_load_case(op, r, find="", loads=[1, 1])
    return edge


def level_load_random(op):
    def rnd(r):
        return level_load_case(op, r)
    return rnd


# --- Load / LoadInternal
def load_case(op, r=None, **kw):
    r = r or random.Random(16)
    c = base_case(op, usersPath=kw.pop("usersPath", r.choice(["", "save:users:1Bob:fantasy", "a", rwide(r, 40)])),
                  name=kw.pop("name", r.choice(["", "park", "Park Name", rwide(r)])),
                  ext=kw.pop("ext", r.choice(["", ".TPWS", ".INTS", ".x", rwide(r, 6)])),
                  existing=(False, False, kw.pop("user", r.random() < 0.4)),
                  q={"CSaveLoad::Load": kw.pop("answers", [r.choice([0, 1, 2, -1, 0x10000])])})
    c["wtexts"] = {"ext": kw.pop("tpws", r.choice([".TPWS", "", ".x", rwide(r, 6)])), "wsep": kw.pop("wsep", r.choice([":", "", "::", "/"]))}
    c.update(kw)
    return c


def load_edges(op):
    def edge():
        r = random.Random(17)
        for answer in (0, 1, 2, -1, 0x10000):
            yield load_case(op, r, answers=[answer], user=False)
            yield load_case(op, r, answers=[answer], user=True)
        for name in ("", "a", "x" * 300, "éĀz", "￿"):
            yield load_case(op, r, name=name)
        for users in ("", "a:b", "z" * 300, "Āÿ"):
            yield load_case(op, r, usersPath=users)
        yield load_case(op, r, tpws="")
        yield load_case(op, r, tpws=".LONGEXTENSION")
        yield load_case(op, r, wsep="")
    return edge


def load_random(op):
    def rnd(r):
        return load_case(op, r)
    return rnd


# --- SetupNewLevel
THING_NAMES = ["StaffHQ", "MechanicHQ", "Advisor", "TagSystem", "ParkAnalyser", "ResearchLab", "Weather", "BankAccount", "UIMsgReceiver",
               "ChallengeHQ", "gates", "lights"]
FOLDERS = ["data:levels:fantasy", "", "a", "x" * 900, "data:levels:space:", "ab:cd:ef"]


def setup_case(r, **kw):
    n = r.choice([0, 1, 2, 3, 5])
    rides = []
    for i in range(n):
        rides.append((r.getrandbits(16), r.choice(["gates", "lights", "GATES", "Lights", "other", rtext(r, 12), ""]), r.getrandbits(16)))
    c = base_case("setup", arg=kw.pop("arg", r.choice(FOLDERS)), fill=r.choice([0xA5, 0x00, 0xFF]),
                  which=kw.pop("which", r.choice([1, 1, 2])), type=kw.pop("type", r.choice([0, 1, 2, 3, T.u32(-1), 0x80000000, 7])),
                  guard=kw.pop("guard", r.choice([0, 1, 0x80, 0xFF, 2])), ctorType=kw.pop("ctorType", r.choice([0, 1, 2, 3])),
                  rides=kw.pop("rides", rides), init=random_init(r))
    c["texts"] = {"easy": r.choice(["Easy_", "", "E"])}
    c["wtexts"] = {"gates": r.choice(["gates", "gates", "Gates", "", rwide(r, 8)]), "lights": r.choice(["lights", "lights", "lights ", ""])}
    c["q"] = {"Balance.Load": kw.pop("loads", [r.choice([0, 1, 1, 1]) for _ in range(3)]),
              "id": kw.pop("ids", [r.choice([0, 1, 0xFFFF, r.getrandbits(16)]) for _ in range(12)]),
              "time": [kw.pop("time", r.choice([0, 1, 0xFFFFFFFF, 0x100000000, 0x123456789ABC, r.getrandbits(40)]))]}
    if "wcsicmp" in kw:
        c["q"]["wcsicmp"] = kw.pop("wcsicmp")
    c["existing"] = (kw.pop("centre", r.random() < 0.4), False, False)
    c.update(kw)
    return c


def setup_edges():
    r = random.Random(18)
    for gtype in (0, 1, 2, 3, T.u32(-1), 0x80000000):
        for guard in (0, 1, 0x80):
            for loads in ([1, 1, 1], [0, 1, 1], [1, 0, 1], [0, 0, 1], [1, 1, 0], [0, 0, 0]):
                yield setup_case(r, type=gtype, guard=guard, ctorType=r.choice([0, 1, 2, 3]), loads=loads)
    # the ride walk
    gates_rides = [(7, "gates", 0), (8, "lights", 0)]
    yield setup_case(r, rides=[])
    yield setup_case(r, rides=gates_rides)
    yield setup_case(r, rides=list(reversed(gates_rides)))
    yield setup_case(r, rides=[(1, "x", 0), (2, "y", 0)] + gates_rides)
    yield setup_case(r, rides=[(1, "x", 0), (2, "y", 0), (3, "z", 0)] + gates_rides)
    yield setup_case(r, rides=[(5, "gates", 0)])
    yield setup_case(r, rides=[(5, "lights", 0)])
    yield setup_case(r, rides=[(0xFFFF, "GATES", 0xFFFF), (0, "LIGHTS", 0)])
    for answers in ([1], [0], [-1], [2], [0, 1], [1, 0], [3, 1], [1, 1, 0]):
        yield setup_case(r, rides=[(1, "a", 0), (2, "b", 0), (3, "c", 0)], wcsicmp=answers)
    for centre in (False, True):
        yield setup_case(r, centre=centre)
    for t in (0, 1, 0xFFFFFFFF, 0x100000000, 0x123456789ABC, 0x7FFFFFFF, 0x80000000):
        yield setup_case(r, time=t)
    for ids in ([0] * 12, [0xFFFF] * 12, list(range(1, 13)), list(range(0x1000, 0x100C))):
        yield setup_case(r, ids=ids)
    for folder in FOLDERS:
        yield setup_case(r, arg=folder, type=2, guard=1, loads=[1, 1, 0])
    yield setup_case(r, which=2)
    yield setup_case(r, which=1)


def setup_random(r):
    return setup_case(r)


# --- InitTheme
def theme_level_case(r, **kw):
    c = setup_case(r, **kw)
    c["op"] = "initTheme"
    c["which"] = 1
    c["themePath"] = r.choice(PATHS)
    c["find"] = None if r.random() < 0.2 else r.choice(FINDS)
    c["display"] = r.choice([0, 0x400000, 0xFFFFFFFF, 0xFFBFFFFF])
    c["balance"] = init_balance(r)
    c["balance"][439] = r.choice([0, 1, 5, T.u32(-1), 0x80000000])
    c["existing"] = (c["existing"][0], r.random() < 0.3, r.random() < 0.4)
    c["q"]["Balance.Load"] = kw.get("loads") or [r.choice([0, 1, 1, 1]) for _ in range(6)]
    c["q"]["RideDB.LoadTheme"] = [kw.get("themeAnswer", r.choice([0, 1, 1, 1, -1, 0x10000, 2]))]
    c["q"]["CSaveLoad::Load"] = [r.choice([0, 1, 2, -1])]
    c["loadFile"] = kw.get("loadFile", r.choice([0, 1, 0xFFFFFFFF, 2]))
    c["file"] = kw.get("file", r.choice(["", "park", "a" * 300, "x" * 1023]))
    c["usersPath"] = r.choice(["", "save:users:1Bob:fantasy", rwide(r, 20)])
    c["wtexts"]["ext"] = r.choice([".TPWS", "", ".x"])
    c["init"] = random_init(r)
    return c


def theme_level_edges():
    r = random.Random(19)
    for answer in (0, 1, -1, 0x10000):
        yield theme_level_case(r, themeAnswer=answer)
    for loads in ([1, 1, 1, 1, 1, 1], [0, 1, 1, 1, 1, 1], [1, 0, 1, 1, 1, 1], [1, 1, 1, 1, 1, 1], [1, 1, 0, 1, 1, 1], [1, 1, 1, 0, 1, 1],
                  [1, 1, 1, 1, 0, 1], [1, 1, 1, 1, 1, 0], [0, 0, 0, 0, 0, 0], [1, 1, 1, 0, 0, 0]):
        for gtype in (0, 1, 2):
            yield theme_level_case(r, themeAnswer=1, loads=loads, type=gtype, guard=1)
    for load_file in (0, 1, 0xFFFFFFFF, 2):
        for file in ("", "park", "a" * 300, "x" * 1023):
            for answer in (0, 1, 2, -1):
                c = theme_level_case(r, themeAnswer=1, loadFile=load_file, file=file, loads=[1] * 6)
                c["q"]["CSaveLoad::Load"] = [answer]
                yield c


def theme_level_random(r):
    return theme_level_case(r)


# the function tests
TESTS = []
for op, name in (("setPath", "LLCC_SystemSetPathCost"), ("setQueue", "LLCC_SystemSetQueueCost"), ("setKart", "LLCC_SystemSetKartTrackCost"),
                 ("setWater", "LLCC_SystemSetWaterTrackCost")):
    edge, rnd = setter_cases(op)
    TESTS.append(FnTest(name, ppc, host, edge=edge, rnd=rnd))
TESTS += [
    FnTest("CSkyObjects::CSkyObjects", ppc, host, edge=sky_edges, rnd=sky_random),
    FnTest("CGame::LoadTheme", ppc, host, edge=theme_edges, rnd=theme_random),
    FnTest("CGame::InitLevel_Initialise", ppc, host, edge=level_init_edges, rnd=level_init_random),
    FnTest("CGame::InitLevel_Load", ppc, host, edge=level_load_edges("levelLoad"), rnd=level_load_random("levelLoad")),
    FnTest("CGame::InitLevel", ppc, host, edge=level_load_edges("initLevel"), rnd=level_load_random("initLevel")),
    FnTest("CFSLoadSave::Load", ppc, host, edge=load_edges("load"), rnd=load_random("load")),
    FnTest("CFSLoadSave::LoadInternal", ppc, host, edge=load_edges("loadInternal"), rnd=load_random("loadInternal")),
    FnTest("CWorld::SetupNewLevel", ppc, host, edge=setup_edges, rnd=setup_random),
    FnTest("CGame::InitTheme", ppc, host, edge=theme_level_edges, rnd=theme_level_random),
]


def setup(runner):
    T.install_std_stubs(runner.emu)
