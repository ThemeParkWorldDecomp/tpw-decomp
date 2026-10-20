"""Unit w104, the way out of the lobby, against libtpw_unit_LobbyExit.dylib (tests/unit_LobbyExit.cpp), in the main app ("SimTheme Park"):
  src/Game_theme_set.cpp     CGame::SetCurrentTheme(int) 0x10149ba8, SetCurrentTheme(name) 0x10149c7c, SetCurrentThemeName 0x1014a08c,
                             ConverttoCurrentTheme 0x1014a10c
  src/lobby_exit.cpp         KeyParticlePOUFFE 0x101e1460, UIInterface::DeInitLobby 0x101ef7a4, DeInitLobbyUI 0x10186434,
                             IsRideFirstPersonRide 0x100329cc
  src/hlsprite_remove.cpp    ESP_RemoveSprites 0x1007c790
  src/lobby_globe_close.cpp  CLBScreenComponentOnlineGlobe: destructor 0x100aca4c, Close 0x100ad50c, DeleteGlobeListEntries 0x100adf48,
                             DeleteCityListEntries 0x100ae104

A case is a world (objects at the Mac's layout in emulator memory: the game and its theme list, the master save, the globe and its lists
and entries, the sprites, the light) and an operation, built twice from one description: for the original in emulator memory and for the
translation by the unit's entry points (tpw_lx_*). Every callee outside the unit is a fake that records the same event on both sides: the
strings' copy constructor, destructor and virtual assignment (slot 0x1c of the game's fake vtable on the PowerPC side), the master save's
constructor and ChangeCurrentTheme, GetCurrentThemeName, _stricmp, UI_DisableHelpText, CMapWho::Unlink, GetWindow, GetPosition (which writes
the rectangle from the case's queue), PAR_InitEmitter, SOUND_SendEvent, CSpriteFileIO::Free, CSpriteList::Close, the screen base class's
destructor, and the globe's entries and lists (their virtual destructors, slot 8 of fake vtables). The heap's new and delete counts are
compared where the functions allocate or free.

Not tested (both sides would fault, or the original reads address 0): an index past the theme list (or any index of an empty list) in
SetCurrentThemeName, NULL sprite pointers below the count, NULL list pointers in the globe's destructor and list helpers, a list whose
count is not 0 but whose head is NULL (an endless loop).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyExit"
MASK = 0xFFFFFFFF

SET_INT, SET_NAME, SET_THEME_NAME, CONVERT = 0x10149BA8, 0x10149C7C, 0x1014A08C, 0x1014A10C
KEY_PARTICLE, DEINIT_LOBBY, DEINIT_UI, IS_RIDE, REMOVE_SPRITES = 0x101E1460, 0x101EF7A4, 0x10186434, 0x100329CC, 0x1007C790
GLOBE_DTOR, GLOBE_CLOSE, DELETE_GLOBES, DELETE_CITIES = 0x100ACA4C, 0x100AD50C, 0x100ADF48, 0x100AE104
GET_NAME, MASTER_CTOR, CHANGE_THEME = 0x1014A058, 0x101694DC, 0x1016B224
DISABLE_HELP, PAR_INIT, SOUND_SEND, GAME_TYPE_CTOR = 0x101CAA4C, 0x100BEC30, 0x100DD9FC, 0x1016F5D8
GET_WINDOW, GET_POSITION, COOL_CTOR, COOL_DTOR = 0x101DBE40, 0x101DB884, 0x101C7448, 0x101C74E0
REMOVE_HEAD_GLOBE, REMOVE_HEAD_CITY, SCREEN_DTOR = 0x100AFB98, 0x100AF7E0, 0x100B6C2C
G_GAME, G_MASTER, G_CONTROLLER, G_TYPE, G_GUARD, G_BANK = 0x1037F134, 0x10380A8C, 0x10395CBC, 0x102B3AA0, 0x102B3AA4, 0x102F7CBC
G_LIGHT, G_FLAGS, G_RIDE_SET, G_RIDE_INSTANCE = 0x1038CC78, 0x102B5A40, 0x102B5A28, 0x102B5A3C
G_SPRITE_COUNT, G_SPRITES, G_LISTS, G_GLOBAL_LIST = 0x102DB1E4, 0x102DB520, 0x102DBDFC, 0x102DBD48

# The labels the unit file gives the same objects.
K_GAME, K_MASTER, K_MAP, K_LINK, K_CONTROLLER, K_FOUND, K_GLOBE, K_GLOBE_LIST, K_CITY_LIST = 5, 6, 7, 8, 9, 10, 11, 12, 13
K_NAME, K_ARG, K_SPRITE, K_FILE, K_SPRITE_LIST, K_ENTRY = 20, 30, 50, 80, 120, 150

TABLE = ["fantasy", "hallow", "jungle", "space", "Fantasy", "HALLOW", "", "jungle ", "JUNGLE", "zzz", "start"]


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def pattern(n, fill):
    return bytes([fill & 0xFF]) * n


# ---------------------------------------------------------------------------------------------------------------- the fakes


def dtor_fn(name):
    """A virtual destructor of a fake object (slot 8): records, and with a positive flag also the delete (the host's operator
    delete records it for a labelled object)."""
    def fn(e):
        L = e.cl
        this, flag = e.r(3), T.s16(e.r(4))
        L.rec(name, (L.lab(this),))
        if flag > 0:
            if L.lab(this) > 0:
                L.rec("delete", (L.lab(this),))
                L.forget(this)
            e.lx_deletes += 1
    return fn


def game_assign(e):
    """The string's virtual operator= (slot 0x1c): records and copies the text."""
    L = e.cl
    this, other = e.r(3), e.r(4)
    text = CL.tds_text(e, other)
    L.rec("String.assign", (L.lab(this), L.lab(other)), (text[:159],))
    vptr = e.u32(this)
    CL.tds_set(e, this, text)
    w32(e, this, vptr)


def vtables(e):
    """The fake vtables, made once per emulator (persistent memory): the game's string (slot 0x1c), and slot 8 of the globe's
    entries, the city flags and the two lists."""
    vt = getattr(e, "lx_vt", None)
    if vt is not None:
        return vt

    def table(slot, fn):
        address = T.persist_alloc(e, 0x40)
        w32(e, address + slot, T.fake_function(e, fn))
        return address

    vt = dict(game=table(0x1C, game_assign), globe=table(8, dtor_fn("Globe.dtor")), city=table(8, dtor_fn("CityFlag.dtor")),
              globe_list=table(8, dtor_fn("GlobeList.dtor")), city_list=table(8, dtor_fn("CityList.dtor")))
    e.lx_vt = vt
    return vt


def build_stubs():
    S = CL.Stubs()
    R = CL.recorder
    S.real = {SET_INT, SET_NAME, SET_THEME_NAME, CONVERT, KEY_PARTICLE, DEINIT_LOBBY, DEINIT_UI, IS_RIDE, REMOVE_SPRITES, GLOBE_DTOR,
              GLOBE_CLOSE, DELETE_GLOBES, DELETE_CITIES, COOL_CTOR, COOL_DTOR, REMOVE_HEAD_GLOBE, REMOVE_HEAD_CITY}

    def change_theme(e):
        L = e.cl
        L.rec("ChangeCurrentTheme", (L.lab(e.r(3)), L.lab(e.r(4))), (CL.tds_text(e, e.r(4))[:159],))

    def copy_ctor(e):
        L = e.cl
        text = CL.tds_text(e, e.r(4))
        L.rec("String.copy", (L.lab(e.r(4)),), (text[:159],))
        CL.tds_set(e, e.r(3), text)

    def string_dtor(e):
        e.cl.rec("String.dtor", (e.cl.lab(e.r(3)),))

    def stricmp(e):
        a, b = e.cstr(e.r(3)), e.cstr(e.r(4))
        e.cl.rec("_stricmp", (), (a[:159], b[:159]))
        la, lb = a.lower(), b.lower()
        e.set_r(3, 0 if la == lb else T.u32(-1 if la < lb else 1))

    def operator_delete(e):
        L = e.cl
        p = e.r(3)
        if L.lab(p) > 0:
            L.rec("delete", (L.lab(p),))
            L.forget(p)
        e.deletes.append(p)
        e.alloc_log.append(("delete", p, 0))

    def get_window(e):
        L = e.cl
        L.rec("GetWindow", (L.lab(e.r(3)), T.s32(e.r(4))))
        e.set_r(3, e.lx.found)

    def get_position(e):
        L, rect = e.cl, e.r(4)
        L.rec("GetPosition", (L.lab(e.r(3)), T.s16(e.r(5))))
        values = [L.ret("Rect", 0) for _ in range(4)]
        e.write(rect + 4, struct.pack(">hhhh", *[T.s16(v & 0xFFFF) for v in values]))
        e.set_r(3, 1)

    S.internal(GET_NAME, R("GetCurrentThemeName", "L", ret=lambda e, L: e.r(3)))
    S.internal(MASTER_CTOR, R("CFSGlobalMasterSave.ctor", "N"))
    S.internal(CHANGE_THEME, change_theme)
    S.internal(DISABLE_HELP, R("UI_DisableHelpText"))
    S.internal(PAR_INIT, R("PAR_InitEmitter", "iiii"))
    S.internal(SOUND_SEND, R("SOUND_SendEvent", "iiiiii"))
    S.internal(GAME_TYPE_CTOR, lambda e: w32(e, e.r(3), e.lx.ctor_type))
    S.internal(GET_WINDOW, get_window)
    S.internal(GET_POSITION, get_position)
    S.internal(SCREEN_DTOR, R("Screen.dtor", "L"))
    S.imp("__ct__26TbDynamicStringTemplate<c>FRC26TbDynamicStringTemplate<c>", copy_ctor)
    S.imp("__dt__26TbDynamicStringTemplate<c>Fv", string_dtor)
    S.imp("_stricmp__10NS_MacDozeFPCcPCc", stricmp)
    S.imp("Unlink__7CMapWhoFP12CMapLinkBasei", R("CMapWho.Unlink", "LLi"))
    S.imp("Free__13CSpriteFileIOFv", R("CSpriteFileIO.Free", "L"))
    S.imp("Close__11CSpriteListFv", R("CSpriteList.Close", "L"))
    S.imp("__dl__FPv", operator_delete)
    return S


STUBS = build_stubs()


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ctx:
    """One case on the PowerPC side: the fakes active, a fresh log, the case's state for the fakes."""

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case.get("q"))
        e.lx = self
        e.lx_deletes = 0
        self.found = 0
        self.ctor_type = 0
        self.news0 = len(e.alloc_log)
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)
        self.e.lx = None

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MASK for a in args])[0]

    def heap(self):
        log = self.e.alloc_log[self.news0:]
        news = len([x for x in log if x[0] in ("new", "new[]")])
        deletes = len([x for x in log if x[0] in ("delete", "delete[]")]) + self.e.lx_deletes
        return news, deletes


def host_run(lib, entry, description, queues=None):
    fn = getattr(lib, entry)
    fn.restype = C.c_int32
    fn.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    values = (C.c_int64 * len(description))(*description)
    out = (C.c_int64 * 2048)()
    CL.host_begin(lib, {}, queues)
    n = fn(values, out)
    log = CL.host_end(lib)
    return [int(out[i]) for i in range(n)], log


def host_only(entry):
    def run(lib, case):
        state, log = host_run(lib, entry, case["d"], case.get("q"))
        return dict(state=state, log=log)
    return run


# ---------------------------------------------------------------------------------------------------------------- the theme setters
# description: the operation (0 SetCurrentTheme(int), 1 SetCurrentTheme(name), 2 SetCurrentThemeName, 3 ConverttoCurrentTheme), the
#   argument (the index, or the theme number), the current index, whether the master save exists, the argument name's id (an index into
#   TABLE), the number of names, then their ids


def theme_ppc(e, case):
    p = Ctx(e, case)
    try:
        L = p.L
        op, arg, current, master, argname, count = case["d"][:6]
        ids = case["d"][6:6 + count]
        vt = vtables(e)
        e.write(G_GAME, pattern(0x40, 0x5A))
        CL.tds_set(e, G_GAME, "start")
        w32(e, G_GAME, vt["game"])
        w32(e, G_GAME + 0x14, current)
        nodes = []
        for i, name_id in enumerate(ids):
            name = e.alloc(0x14)
            CL.tds_set(e, name, TABLE[name_id])
            L.labels[name] = K_NAME + i
            nodes.append(name)
        cells = [e.alloc(12) for _ in nodes]
        for i, cell in enumerate(cells):
            ws(e, cell, [nodes[i], cells[i + 1] if i + 1 < len(cells) else 0, cells[i - 1] if i else 0])
        ws(e, G_GAME + 0x24, [0, cells[0] if cells else 0, cells[-1] if cells else 0, count])
        arg_name = e.alloc(0x14)
        CL.tds_set(e, arg_name, TABLE[argname])
        L.labels[arg_name] = K_ARG
        L.labels[G_GAME] = K_GAME
        if master:
            save = e.alloc(0x4C, pattern(0x4C, 0x33))
            L.labels[save] = K_MASTER
            w32(e, G_MASTER, save)
        else:
            w32(e, G_MASTER, 0)
        p.news0 = len(e.alloc_log)
        if op == 0:
            p.call(SET_INT, G_GAME, arg)
        elif op == 1:
            p.call(SET_NAME, G_GAME, arg_name)
        elif op == 2:
            p.call(SET_THEME_NAME, G_GAME)
        else:
            p.call(CONVERT, arg)
        text = CL.tds_text(e, G_GAME)
        news, deletes = p.heap()
        state = [T.s32(e.u32(G_GAME + 0x14)), TABLE.index(text) if text in TABLE else -1, L.lab(e.u32(G_MASTER)), news, deletes]
        return dict(state=state, log=L.log)
    finally:
        p.done()


theme_host = host_only("tpw_lx_theme")


def walk_ok(index, count):
    """SetCurrentThemeName's walk stays on the list: a negative index takes the first name, one past the end reads NULL."""
    return count >= 1 and index < count


def theme_valid(op, arg, current, count, ids, argname):
    if op == 0:
        return arg == current or walk_ok(arg, count)
    if op == 2:
        return walk_ok(current, count)
    if op == 3:
        target = {3: 0, 1: 2, 2: 1, 4: 3}.get(arg, 2)
        return target == current or walk_ok(target, count)
    if count == 0:
        return True
    if TABLE[argname].lower() in [TABLE[i].lower() for i in ids]:
        return True
    return walk_ok(current, count)


def theme_case(op, arg, current, master, argname, ids):
    return dict(d=[op, arg, current, master, argname, len(ids)] + list(ids), q={})


def random_theme_case(r, op=None):
    while True:
        op = r.randint(0, 3) if op is None else op
        count = r.choice([0, 1, 2, 3, 4, 4, 5])
        ids = [r.randint(0, 9) for _ in range(count)]
        if r.random() < 0.4:
            ids = r.sample(range(10), count)
        argname = r.randint(0, 9)
        if r.random() < 0.5 and ids:
            argname = r.choice(ids)
        if r.random() < 0.3 and ids:
            argname = {0: 4, 1: 5, 2: 8, 4: 0, 5: 1, 8: 2}.get(r.choice(ids), argname)   # the same text in another case
        current = r.choice([-1, -1, 0, 1, 2, 3, count - 1, count, count + 1, -2, r.randint(-3, 6)])
        arg = r.choice([-1, 0, 1, 2, 3, 4, 5, current, current, count - 1, count, r.randint(-3, 7)])
        if op == 3:
            arg = r.choice([-2, -1, 0, 1, 2, 3, 4, 5, 6, 0x7FFFFFFF, -0x80000000, r.randint(-10, 10)])
        master = r.randint(0, 1)
        if theme_valid(op, arg, current, count, ids, argname):
            return theme_case(op, arg, current, master, argname, ids)


def random_theme_op(op):
    return lambda r: random_theme_case(r, op)


def edge_theme(op):
    def gen():
        names = [0, 1, 2, 3]
        # SetCurrentTheme(int) and the name walk over lists of 0, 1 and 4 names
        for ids in ([], [2], names, [4, 5, 8, 0], [7, 6, 9, 2]):
            n = len(ids)
            for master in (0, 1):
                for current in (-1, 0, 1, 2, 3, n - 1, n, n + 1):
                    for arg in (-1, 0, 1, 2, 3, 4, current, n - 1):
                        for argname in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9):
                            if op == 3 and arg not in (-1, 0, 1, 2, 3, 4):
                                continue
                            if op in (0, 3) and argname != 0:
                                continue
                            if op == 2 and (arg != 0 or argname != 0):
                                continue
                            if theme_valid(op, arg, current, n, ids, argname):
                                yield theme_case(op, arg, current, master, argname, ids)
        if op == 3:
            for arg in range(-2, 7):
                for current in (-1, 0, 1, 2, 3, 4):
                    for master in (0, 1):
                        yield theme_case(3, arg, current, master, 0, names + [9])
            for arg in (0x7FFFFFFF, -0x80000000, 0x80000000 - 1, 100, -100, 0x10003):
                yield theme_case(3, arg, 0, 1, 0, names + [9])
        if op == 1:
            # names that differ only in case, a name found first, middle and last, one not found, an empty one
            lists = [[0, 1, 2, 3], [4, 5, 8, 6], [6, 7, 2], [2, 2, 2], [3, 1, 0]]
            for ids in lists:
                for argname in range(10):
                    for current in (-1, 0, len(ids) - 1, 1):
                        if theme_valid(1, 0, current, len(ids), ids, argname):
                            yield theme_case(1, 0, current, 0, argname, ids)
    return gen


# ---------------------------------------------------------------------------------------------------------------- IsRideFirstPersonRide
# description: the first-person flags, whether a ride set is current, the remembered instance, the argument


def ride_ppc(e, case):
    p = Ctx(e, case)
    try:
        flags, ride_set, instance, arg = case["d"]
        w32(e, G_FLAGS, flags)
        w32(e, G_RIDE_SET, 0x1000 if ride_set else 0)
        w32(e, G_RIDE_INSTANCE, instance)
        return dict(state=[T.s32(p.call(IS_RIDE, arg))], log=p.L.log)
    finally:
        p.done()


ride_host = host_only("tpw_lx_ride")


def ride_case(flags, ride_set, instance, arg):
    return dict(d=[flags, ride_set, instance, arg & MASK], q={})


def edge_ride():
    for flags in (0, 4, 0xB, 0xFFFFFFFB, 0xFFFFFFFF, 0x16, 0x14, 0x2, 0x8, 0x80000004, 0xFFFFFFFD):
        for ride_set in (0, 1):
            for instance in (0, 1, 5, 0xFFFFFFFF, 0x80000000):
                for arg in (instance, 0, instance ^ 1, 5, 0xFFFFFFFF, 0x80000000):
                    yield ride_case(flags, ride_set, instance, arg)


def random_ride(r):
    instance = r.choice([0, 1, 2, r.getrandbits(32), r.getrandbits(16)])
    arg = r.choice([instance, instance, 0, instance + 1, r.getrandbits(32)])
    return ride_case(r.choice([0, 4, 4, r.getrandbits(32), r.getrandbits(32) | 4, r.getrandbits(32) & ~4]), r.randint(0, 1), instance, arg)


# ---------------------------------------------------------------------------------------------------------------- DeInitLobby, DeInitLobbyUI
# description: the operation (0 UIInterface::DeInitLobby, 1 DeInitLobbyUI), whether the map exists


def deinit_ppc(e, case):
    p = Ctx(e, case)
    try:
        op, map_exists = case["d"]
        light = e.alloc(0x68, pattern(0x68, 0x44))
        link = e.alloc(0x20, pattern(0x20, 0x66))
        w32(e, light, link)
        w32(e, G_LIGHT, light)
        map_object = e.alloc(0x100, pattern(0x100, 0x77))
        w32(e, e.data_import("gei_MapWho"), map_object if map_exists else 0)
        p.L.labels[map_object] = K_MAP
        p.L.labels[link] = K_LINK
        p.call(DEINIT_LOBBY if op == 0 else DEINIT_UI)
        return dict(state=[], log=p.L.log)
    finally:
        p.done()


deinit_host = host_only("tpw_lx_deinit")


def edge_deinit(op):
    return lambda: [dict(d=[op, map_exists], q={}) for map_exists in (0, 1)]


def random_deinit(op):
    return lambda r: dict(d=[op, r.randint(0, 1)], q={})


# ---------------------------------------------------------------------------------------------------------------- KeyParticlePOUFFE
# description: whether the island controller exists, the game type, the type object's guard byte (PowerPC only: the host's exists
#   already), whether GetWindow finds the window, the thunder bank
# queues: Rect (left, top, right, bottom of GetPosition)


def key_ppc(e, case):
    p = Ctx(e, case)
    try:
        controller, game_type, guard, found, bank = case["d"]
        window = e.alloc(0x140, pattern(0x140, 0x31))
        other = e.alloc(0x140, pattern(0x140, 0x32))
        p.L.labels[window] = K_CONTROLLER
        p.L.labels[other] = K_FOUND
        w32(e, G_CONTROLLER, window if controller else 0)
        p.found = other if found else 0
        p.ctor_type = game_type
        w32(e, G_TYPE, game_type)
        e.write(G_GUARD, bytes([guard & 0xFF]))
        w32(e, G_BANK, bank)
        p.call(KEY_PARTICLE)
        return dict(state=[], log=p.L.log)
    finally:
        p.done()


key_host = host_only("tpw_lx_key")


def key_case(controller, game_type, found, bank, top, bottom, guard=1, left=0, right=0):
    return dict(d=[controller, game_type, guard, found, bank], q={"Rect": [left, top, right, bottom]})


def edge_key():
    tops = [(0, 0), (0, 1), (0, 2), (0, 3), (1, 2), (1, 1), (-1, 0), (-1, -1), (-2, -1), (-3, 0), (-3, -1), (-7, 2), (5, -4), (100, 3000),
            (0, 1535), (0, 2047), (1024, 1025), (0x7FFF, 0x7FFF), (-0x8000, 0x7FFF), (-0x8000, -0x8000), (0x7FFF, -0x8000), (28633, 28633),
            (28634, 28634), (-28633, -28633), (-28634, -28634), (-1, 1), (-1, 2), (-2, 1), (-1535, -1), (1535, 1536), (20480, 20481),
            (0x4000, 0x4001), (-0x4000, -0x3fff), (255, 256), (-256, -255), (767, 770), (768, 769), (-768, -767), (-769, -766)]
    for controller in (0, 1):
        for game_type in (0, 1, 2, 3, -1, 0x10000, -0x80000000, 0x7FFFFFFF):
            for found in (0, 1):
                for guard in (0, 1, 0x80):
                    yield key_case(controller, game_type, found, 0x1234, 10, 20, guard)
    for top, bottom in tops:
        for found in (0, 1):
            yield key_case(1, 0, found, -5, top, bottom, left=3, right=9)
    for bank in (0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x102F7CBC):
        yield key_case(1, 1, 1, bank, 7, 8)
    # the left and right edges must not matter (the listing reads +6 and +0xa of the rectangle)
    for left, right in ((0, 0), (-0x8000, 0x7FFF), (0x7FFF, -0x8000), (1000, 1)):
        yield key_case(1, 0, 1, 3, 100, 200, left=left, right=right)


def random_key(r):
    def edge():
        return r.choice([0, 1, -1, 2047, -2047, 1536, 0x7FFF, -0x8000, 28633, 28634, -28634, r.randint(-0x8000, 0x7FFF), r.randint(-3000, 3000)])
    return key_case(r.choice([1, 1, 1, 1, 0]), r.choice([0, 1, 3, 2, 0, -1, r.randint(-5, 5)]), r.randint(0, 1), r.choice([0, 0x55, r.getrandbits(31)]),
                    edge(), edge(), r.choice([0, 1, 1, 0x80]), edge(), edge())


# ---------------------------------------------------------------------------------------------------------------- ESP_RemoveSprites
# description: gESPSpriteCount, the sets made (at least the count and one more), then per set: the loaded byte, whether it has a file


def remove_ppc(e, case):
    p = Ctx(e, case)
    try:
        L = p.L
        count, sets = case["d"][:2]
        w32(e, G_SPRITE_COUNT, count)
        e.write(G_SPRITES, bytes(4 * 0x200))
        sprites = []
        for i in range(sets):
            loaded, has_file = case["d"][2 + 2 * i:4 + 2 * i]
            sprite = e.alloc(0x260, pattern(0x260, 0xA5))
            file_object = e.alloc(0x10, pattern(0x10, 0x5B)) if has_file else 0
            w32(e, sprite, file_object)
            e.write(sprite + 0x204, bytes([loaded & 0xFF]))
            w32(e, G_SPRITES + 4 * i, sprite)
            L.labels[sprite] = K_SPRITE + i
            if file_object:
                L.labels[file_object] = K_FILE + i
            sprites.append(sprite)
        for k in range(16):
            L.labels[G_LISTS + 0x28 * k] = K_SPRITE_LIST + k
        L.labels[G_GLOBAL_LIST] = K_SPRITE_LIST + 16
        p.news0 = len(e.alloc_log)
        p.call(REMOVE_SPRITES)
        state = [T.s32(e.u32(G_SPRITE_COUNT))]
        for i in range(sets):
            state += [int(e.u32(G_SPRITES + 4 * i) != 0), int(e.u32(sprites[i]) != 0)]
        state += list(p.heap())
        return dict(state=state, log=L.log)
    finally:
        p.done()


remove_host = host_only("tpw_lx_remove")


def remove_case(count, cells):
    """cells: (loaded, has_file) per set; one more set than the count is always made (it must not be touched)."""
    d = [count, len(cells)]
    for loaded, has_file in cells:
        d += [loaded, has_file]
    return dict(d=d, q={})


def edge_remove():
    for count in (0, 1, 2, 5, 9):
        for loaded in (0, 1, 2, 0x80, 0xFF):
            for has_file in (0, 1):
                yield remove_case(count, [(loaded, has_file)] * (count + 1))
    for count in (-1, -5, -0x80000000):
        yield remove_case(count, [(1, 1)])
    # a mixed row: every combination in turn
    cells = [(0, 0), (0, 1), (1, 0), (1, 1), (0xFF, 1), (2, 0)]
    for count in range(1, 6):
        yield remove_case(count, cells[:count] + [(1, 1)])
        yield remove_case(count, cells[::-1][:count] + [(1, 1)])
    yield remove_case(20, [(1, 1)] * 21)


def random_remove(r):
    count = r.choice([0, 1, 2, 3, 5, 8, -1, 12])
    cells = [(r.choice([0, 1, 1, 0x80, r.randint(0, 255)]), r.randint(0, 1)) for _ in range(max(count, 0) + 1)]
    return remove_case(count, cells)


# ---------------------------------------------------------------------------------------------------------------- the online globe
# description: the operation (0 destructor, 1 DeleteGlobeListEntries, 2 DeleteCityListEntries, 3 Close, 4 destructor of NULL), the
#   destructor's flag, the globes, the city flags, then one kind per entry (0 a NULL entry, 1 an object): the globes', then the flags'


def globe_ppc(e, case):
    p = Ctx(e, case)
    try:
        L = p.L
        op, flag, globes, flags = case["d"][:4]
        kinds = case["d"][4:4 + globes + flags]
        vt = vtables(e)
        obj = e.alloc(0x7C, pattern(0x7C, 0x2B))

        def make_list(list_vt, label, entry_vt, entry_base, entry_kinds):
            entries = []
            for i, kind in enumerate(entry_kinds):
                if kind:
                    entry = e.alloc(0x10, pattern(0x10, 0x6D))
                    w32(e, entry, entry_vt)
                    L.labels[entry] = entry_base + i
                    entries.append(entry)
                else:
                    entries.append(0)
            cells = [e.alloc(12) for _ in entries]
            for i, cell in enumerate(cells):
                ws(e, cell, [entries[i], cells[i + 1] if i + 1 < len(cells) else 0, cells[i - 1] if i else 0])
            lst = e.alloc(16)
            ws(e, lst, [list_vt, cells[0] if cells else 0, cells[-1] if cells else 0, len(cells)])
            L.labels[lst] = label
            return lst

        globe_list = make_list(vt["globe_list"], K_GLOBE_LIST, vt["globe"], K_ENTRY, kinds[:globes])
        city_list = make_list(vt["city_list"], K_CITY_LIST, vt["city"], K_ENTRY + 40, kinds[globes:])
        w32(e, obj + 8, globe_list)
        w32(e, obj + 0x6C, city_list)
        L.labels[obj] = K_GLOBE
        p.news0 = len(e.alloc_log)
        if op == 0:
            p.call(GLOBE_DTOR, obj, flag)
        elif op == 1:
            p.call(DELETE_GLOBES, obj)
        elif op == 2:
            p.call(DELETE_CITIES, obj)
        elif op == 3:
            p.call(GLOBE_CLOSE, obj)
        else:
            p.call(GLOBE_DTOR, 0, flag)
        state = list(p.heap())
        if op != 0:
            state += [T.s32(e.u32(globe_list + 0xC)), T.s32(e.u32(city_list + 0xC)), int(e.u32(obj + 8) == globe_list),
                      int(e.u32(obj + 0x6C) == city_list)]
        return dict(state=state, log=L.log)
    finally:
        p.done()


globe_host = host_only("tpw_lx_globe")


def globe_case(op, flag, kinds_globes, kinds_flags):
    return dict(d=[op, flag, len(kinds_globes), len(kinds_flags)] + list(kinds_globes) + list(kinds_flags), q={})


KIND_ROWS = [[], [1], [0], [1, 1, 1], [1, 0, 1], [0, 0], [1, 1, 1, 1, 1], [0, 1, 1]]


def edge_globe(ops):
    def gen():
        for op in ops:
            for flag in ((1, 0, -1, 2, -0x8000, 0x7FFF) if op in (0, 4) else (0,)):
                if op == 4 and flag <= 0:
                    continue
                for globes in KIND_ROWS:
                    for flags in KIND_ROWS:
                        yield globe_case(op, flag, globes, flags)
    return gen


def random_globe(ops):
    def make(r):
        op = r.choice(ops)
        flag = r.choice([1, 1, 0, -1, 2, r.randint(-0x8000, 0x7FFF)]) if op in (0, 4) else 0
        if op == 4 and flag <= 0:
            flag = 1
        return globe_case(op, flag, [r.randint(0, 1) for _ in range(r.randint(0, 6))], [r.randint(0, 1) for _ in range(r.randint(0, 6))])
    return make


TESTS = [
    FnTest("CGame::SetCurrentTheme(int)", theme_ppc, theme_host, edge=edge_theme(0), rnd=random_theme_op(0)),
    FnTest("CGame::SetCurrentTheme(name)", theme_ppc, theme_host, edge=edge_theme(1), rnd=random_theme_op(1)),
    FnTest("CGame::SetCurrentThemeName", theme_ppc, theme_host, edge=edge_theme(2), rnd=random_theme_op(2)),
    FnTest("ConverttoCurrentTheme", theme_ppc, theme_host, edge=edge_theme(3), rnd=random_theme_op(3)),
    FnTest("IsRideFirstPersonRide", ride_ppc, ride_host, edge=edge_ride, rnd=random_ride),
    FnTest("UIInterface::DeInitLobby", deinit_ppc, deinit_host, edge=edge_deinit(0), rnd=random_deinit(0)),
    FnTest("DeInitLobbyUI", deinit_ppc, deinit_host, edge=edge_deinit(1), rnd=random_deinit(1)),
    FnTest("KeyParticlePOUFFE", key_ppc, key_host, edge=edge_key, rnd=random_key),
    FnTest("ESP_RemoveSprites", remove_ppc, remove_host, edge=edge_remove, rnd=random_remove),
    FnTest("CLBScreenComponentOnlineGlobe::~CLBScreenComponentOnlineGlobe", globe_ppc, globe_host, edge=edge_globe((0, 4)), rnd=random_globe((0, 4))),
    FnTest("CLBScreenComponentOnlineGlobe::DeleteGlobeListEntries", globe_ppc, globe_host, edge=edge_globe((1,)), rnd=random_globe((1,))),
    FnTest("CLBScreenComponentOnlineGlobe::DeleteCityListEntries", globe_ppc, globe_host, edge=edge_globe((2,)), rnd=random_globe((2,))),
    FnTest("CLBScreenComponentOnlineGlobe::Close", globe_ppc, globe_host, edge=edge_globe((3,)), rnd=random_globe((3,))),
]
