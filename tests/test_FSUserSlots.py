"""Differential tests for unit w48 (src/FS_UserInfo_slots.cpp, src/FS_MasterSave_easy.cpp, src/lobby_sendquit.cpp):
creating and deleting a player on disk, the user folders, the global master save's write-back, SetEasyModeUser and the
lobby's SendQuit, against libtpw_unit_FSUserSlots.dylib (tests/unit_FSUserSlots.cpp).

The harness is test_FSUserInfo's (and so test_FSSystem's): the four login slots, the paths of the players' folders, the faked
file system that FS_DelTree walks, the narrow and wide strings and the master save's recording constructor and destructor.
This module adds the recording fakes of the master save's Save, LbFile_Exists (answers from the queue "LbFile_Exists") and
LbFile_Copy, and the lobby global. Everything the unit's functions call that is not a fake runs as itself on both sides
(FS_DelTree, UniStringToString, GetDataPath, ReturnGameLoginSlotUsersPath, the slot's name virtuals).
"""
import ctypes as C
import itertools
import random
import struct

import calllog as CL
import test_FSSystem as FS
import test_FSUserInfo as FU
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_FSUserSlots"

F_ = {
    "CFSUserInfo::CreateNewGameLoginUserInSlot": 0x1017EE38, "CFSUserInfo::DeleteGameLoginUserInSlot": 0x1017ED10,
    "CFSUserInfo::CreateUsersDirectories": 0x1017F0B8, "CFSUserInfo::InternalSaveGlobalMasterSave": 0x1017FAB0,
    "CFSGlobalMasterSave::SetEasyModeUser": 0x1016A540, "CLBLobby::SendQuit": 0x100A643C}
GLOBAL_LOBBY = 0x102E4888      # gLobby
LEVELS_OFFSET, EXT_OFFSET = 0x409C, 0x4434     # gTPGlobals' "levels" folder and the ".TPWI" extension strings
GAME_RESTART = FS.A["gGame"] + 0x18
LABELS = dict(name=90, path=78, lobby=91)
VP, I32, I64 = C.c_void_p, C.c_int32, C.c_int64
F = FS.F
w32 = FS.w32

# ======================================================================= PowerPC side


def build_stubs(emu):
    S = FU.build_stubs(emu)
    R = CL.recorder
    S.real.update(F_.values())
    # the master save's Save: the receiver's label and the path
    S.internal(0x1016A584, R("CFSGlobalMasterSave::Save", "Ls", ret="i"))
    # LbFile_Exists answers 0 or 1 from the queue
    S.imp("LbFile_Exists__FPCc", R("LbFile_Exists", "s", ret="b"))

    def copy(e):
        # a TbError returned in a hidden buffer (r3): the source and the target in r4 and r5
        e.cl.rec("LbFile_Copy", (), (e.cstr(e.r(4))[:159], e.cstr(e.r(5))[:159]))
        w32(e, e.r(3), 0)

    S.imp("LbFile_Copy__FPCcPCc", copy)
    return S


def ppc_begin(e, c):
    FU.ppc_begin(e, c)
    FS.s_write(e, FS.A["gTPGlobals"] + LEVELS_OFFSET, c["levels"], False)
    FS.s_write(e, FS.A["gTPGlobals"] + EXT_OFFSET, c["ext"], False)
    w32(e, GAME_RESTART, c.get("restart", 0) & 0xFFFFFFFF)
    e.fsu.lobby = 0
    w32(e, GLOBAL_LOBBY, 0)
    if c.get("lobby") is not None:
        e.fsu.lobby = e.alloc(0x40)
        w32(e, e.fsu.lobby + 0x14, c["lobby"] & 0xFFFFFFFF)
        e.cl.labels[e.fsu.lobby] = LABELS["lobby"]
        w32(e, GLOBAL_LOBBY, e.fsu.lobby)
    if c.get("byte0") is not None and e.fsu.master:
        e.write(e.fsu.master + 0x24, bytes([c["byte0"]]))


def ppc_observe(e, c):
    out = FU.ppc_observe(e, c)
    master = e.u32(FU.A["gMasterSave"])
    if c.get("observe_byte"):
        out["easy_byte"] = e.read(master + 0x24, 1)[0] if master else -1
    out["restart"] = T.s32(e.u32(GAME_RESTART))
    lobby = e.u32(GLOBAL_LOBBY)
    out["state"] = T.s32(e.u32(lobby + 0x14)) if lobby else -1
    return out

# ======================================================================= host side


def host_begin(lib, c):
    h = FU.host_begin(lib, c)
    addr = lambda n: F(lib, "tpw_slots_addr", VP, C.c_char_p)(n.encode())   # noqa: E731
    sset = F(lib, "tpw_fs_set_string", None, VP, I32, C.c_char_p)
    sset(addr("mLevelsFolder"), 0, c["levels"].encode("latin-1"))
    sset(addr("mExtension"), 0, c["ext"].encode("latin-1"))
    F(lib, "tpw_slots_set_restart", None, I32)(c.get("restart", 0))
    F(lib, "tpw_slots_clear_lobby", None)()
    h.lobby = None
    if c.get("lobby") is not None:
        h.lobby = F(lib, "tpw_slots_new_lobby", VP, I32)(c["lobby"])
        h.fixed[LABELS["lobby"]] = h.lobby
    master = F(lib, "tpw_fsu_master_save", VP)()
    if c.get("byte0") is not None and master:
        F(lib, "tpw_slots_set_easy_byte", None, VP, I32)(master, c["byte0"])
    return h


def host_observe(lib, h, c):
    out = FU.host_observe(lib, h, c)
    master = F(lib, "tpw_fsu_master_save", VP)()
    if c.get("observe_byte"):
        out["easy_byte"] = F(lib, "tpw_slots_easy_byte", I32, VP)(master)
    out["restart"] = F(lib, "tpw_slots_restart", I32)()
    out["state"] = F(lib, "tpw_slots_lobby_state", I32)()
    return out

# ======================================================================= running a case


STUBS = None


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    runner.emu.write(runner.emu.data_import("__ctype_map"), FU.ctype_map())
    if "c" not in FS.VT:
        FS.make_vtables(runner.emu)
    STUBS = build_stubs(runner.emu)


def fn_test(name, first, args=None, edge=None, rnd=None, strings=False):
    """first(c, side) is the receiver (or the first value); args(c, side) the others; strings: compare the text of the string
    arguments after the call (they are const)."""
    t = FS.FsTest(name, None, None, edge=edge, rnd=rnd)

    def ppc(e, c):
        saved = STUBS.activate(e)
        try:
            ppc_begin(e, c)
            obj = first(c, ("ppc", e))
            a = args(c, ("ppc", e)) if args else []
            T.call_ppc(e, F_[name], [T.u32(obj)] + [T.u32(v) for v in a])
            out = ppc_observe(e, c)
            out["log"] = e.cl.log
            out["texts"] = FS.arg_texts(c, ("ppc", e)) if strings else None
        finally:
            CL.Stubs.deactivate(e, saved)
            e.cl = None
        t._p = out
        return out

    def host(lib, c):
        h = host_begin(lib, c)
        obj = first(c, ("host", lib, h))
        a = args(c, ("host", lib, h)) if args else []
        CL.host_begin(lib, h.fixed, c.get("q", {}))
        if h.obj and not c.get("blank"):
            FU.set_slot_labels(lib, h)
        vals = (I64 * 8)(*([int(obj)] + [int(v) for v in a] + [0] * (7 - len(a))))
        F(lib, "tpw_slots_call", I32, C.c_char_p, VP)(name.encode(), vals)
        log = CL.host_end(lib)
        out = host_observe(lib, h, c)
        out["log"] = log
        out["texts"] = FS.arg_texts(c, ("host", lib, h)) if strings else None
        t._last = (t._p, out)
        return out

    t.ppc, t.host = ppc, host
    return t

# ======================================================================= case generators


NEW_NAMES = ["A", "Bob", "alice", "Sam Smith", "B" * 16, "C" * 17, "€€", "Zo\xeb", "x y", "z", "Āx", "Al" * 20]
LEVELS = ["levels", "Levels", "", "lv"]
EXTS = [".TPWI", ".tpwi", "", ".x"]
DATA_PATHS = ["", "HD:Data", "HD:Games:TPW:data:", "x" * 30, "d"]
EASY = [0, 1, -1, 2, 7, 256, 0x7FFFFFFF, -0x80000000]
PATH_ARGS = ["", "x", "HD:TPW:save:users:1Bob", "€:p", "a" * 20]
THEMES = ["Lost", "Halloween", "Wonderland", "Space"]


def slot_path(c, slot, name):
    """The narrow text of the slot's folder for a user `name` (ReturnGameLoginSlotUsersPath, narrowed)."""
    wide = c["wide"]
    users = FU.save_paths(c)[1]
    return FS.latin(users + wide["mWideSeparator"] + str(slot + 1) + name)


def tree_of(r, c, slot, name):
    """A folder tree under the slot's folder (the entries FS_DelTree lists, with sub folders and hidden files)."""
    if r.random() < 0.25:
        return {}
    path = slot_path(c, slot, name)
    sep = c["globals"]["mSeparator"]
    tree = {}
    entries = []
    for _ in range(r.randint(0, 4)):
        kind = r.choice(["file", "file", "dir", "hidden", "hiddendir"])
        entries.append({"file": (r.choice(["gms.dat", "a.txt"]), 0), "dir": (r.choice(["Lost", "Space"]), 0x10),
                        "hidden": (".h", 0), "hiddendir": (".d", 0x10)}[kind])
    tree[path] = entries
    for entry, attributes in entries:
        if attributes & 0x10:
            tree[path + sep + entry] = [(r.choice(["x.sam", "y"]), 0)] if r.random() < 0.5 else []
    return tree


def base(r, **extra):
    c = FU.base(r)
    c["paths"]["mDataPath"] = r.choice(DATA_PATHS)
    c["levels"], c["ext"] = r.choice(LEVELS), r.choice(EXTS)
    c["restart"] = r.choice([0, 1, 2, -1])
    c["q"] = {"LbFile_Exists": [r.choice([0, 1]) for _ in range(4)], "CFSGlobalMasterSave::Save": [r.choice([0, 1, -1])],
              "LbFile_CreateDirectory": [r.choice([0, -1])]}
    c["tree"] = {}
    c.update(extra)
    return c


def named(c, slot):
    """Makes sure the slot holds a user (the folder name functions need one)."""
    if not c["slots"][slot][0]:
        c["slots"][slot] = ("Bob", c["slots"][slot][1])


def slot_arg(c, side):
    return c["slot"]


def info_first(c, side):
    if side[0] == "ppc":
        return side[1].fsu.obj
    return side[2].obj


def with_themes(c, count, r):
    c["themes"] = r.sample(THEMES, count)
    c["q"]["LbFile_Exists"] = [r.choice([0, 1]) for _ in range(max(count, 1))]


def name_arg(key):
    def args(c, side):
        return [c["slot"]] + [FS.arg_string(c, side, key, LABELS["name"], wide=True)] + [c["easy"]]
    return args


# ---- CreateNewGameLoginUserInSlot
def g_create(r, **fixed):
    c = base(r)
    c["slot"] = r.randint(0, 3)
    c["name"] = r.choice(NEW_NAMES)
    c["easy"] = r.choice(EASY)
    c["tree"] = tree_of(r, c, c["slot"], c["slots"][c["slot"]][0])
    c.update(fixed)
    return c


def create_edge():
    r = random.Random(11)
    cases = []
    for slot, old, easy, master, themes in itertools.product(range(4), ["", "Bob", "€uro"], [0, 1, -1, 2, 256], [False, True],
                                                             [0, 1, 3]):
        c = g_create(r, slot=slot, easy=easy, master_save=master)
        c["slots"][slot] = (old, c["slots"][slot][1])
        c["tree"] = tree_of(r, c, slot, old)
        with_themes(c, themes, r)
        c["observe_byte"] = True
        cases.append(c)
    for name, level, ext in itertools.product(NEW_NAMES, LEVELS, EXTS):
        c = g_create(r, name=name, easy=1, levels=level, ext=ext)
        c["observe_byte"] = True
        cases.append(c)
    return cases


def create_rnd(r):
    c = g_create(r)
    c["observe_byte"] = True
    return c


def create_first(c, side):
    return info_first(c, side)


TESTS = [fn_test("CFSUserInfo::CreateNewGameLoginUserInSlot", create_first, name_arg("name"), create_edge, create_rnd,
                 strings=True)]


# ---- DeleteGameLoginUserInSlot
def g_delete(r, **fixed):
    c = base(r)
    c["slot"] = r.randint(0, 3)
    named(c, c["slot"])
    c["tree"] = tree_of(r, c, c["slot"], c["slots"][c["slot"]][0])
    c.update(fixed)
    return c


def delete_edge():
    r = random.Random(12)
    cases = []
    for slot, easy, master in itertools.product(range(4), [0, 1, 7], [False, True]):
        c = g_delete(r, slot=slot, master_save=master)
        c["slots"][slot] = (r.choice(NEW_NAMES), easy)
        c["tree"] = tree_of(r, c, slot, c["slots"][slot][0])
        cases.append(c)
    return cases


TESTS.append(fn_test("CFSUserInfo::DeleteGameLoginUserInSlot", info_first, lambda c, s: [c["slot"]], delete_edge, g_delete))


# ---- CreateUsersDirectories
def g_dirs(r, **fixed):
    c = base(r)
    c["path_arg"] = r.choice(PATH_ARGS)
    c["easy"] = r.choice(EASY)
    with_themes(c, r.randint(0, 4), r)
    c.update(fixed)
    return c


def dirs_edge():
    r = random.Random(13)
    cases = []
    for path, easy, themes in itertools.product(PATH_ARGS, [0, 1, -1, 2, 256, -0x80000000], [0, 1, 3, 4]):
        c = g_dirs(r, path_arg=path, easy=easy)
        with_themes(c, themes, r)
        cases.append(c)
    for data, level, ext, exists in itertools.product(DATA_PATHS, LEVELS, EXTS, [[0], [1]]):
        c = g_dirs(r, easy=1)
        c["paths"]["mDataPath"], c["levels"], c["ext"] = data, level, ext
        with_themes(c, 2, r)
        c["q"]["LbFile_Exists"] = exists
        cases.append(c)
    # Exists answering per theme: every pattern of three themes
    for pattern in itertools.product([0, 1], repeat=3):
        c = g_dirs(r, easy=1)
        with_themes(c, 3, r)
        c["q"]["LbFile_Exists"] = list(pattern)
        cases.append(c)
    return cases


def dirs_args(c, side):
    return [FS.arg_string(c, side, "path_arg", LABELS["path"], wide=True), c["easy"]]


TESTS.append(fn_test("CFSUserInfo::CreateUsersDirectories", info_first, dirs_args, dirs_edge, g_dirs, strings=True))


# ---- InternalSaveGlobalMasterSave
def g_save(r, **fixed):
    c = base(r)
    c["slot"] = r.randint(0, 3)
    named(c, c["slot"])
    c["slots"][c["slot"]] = (r.choice(NEW_NAMES), c["slots"][c["slot"]][1])
    c.update(fixed)
    return c


def save_edge():
    r = random.Random(14)
    cases = []
    for slot, master, wide in itertools.product(range(4), [False, True], [":", "/"]):
        c = g_save(r, slot=slot, master_save=master)
        c["slots"][slot] = (r.choice(NEW_NAMES), 0)
        c["wide"]["mWideSeparator"] = wide
        cases.append(c)
    return cases


TESTS.append(fn_test("CFSUserInfo::InternalSaveGlobalMasterSave", info_first, lambda c, s: [c["slot"]], save_edge, g_save))


# ---- SetEasyModeUser
def master_first(c, side):
    if side[0] == "ppc":
        return side[1].fsu.master
    return F(side[1], "tpw_fsu_master_save", VP)()


def g_easy(r, **fixed):
    c = base(r, master_save=True)
    c["byte0"] = r.choice([0, 1, 0xA5, 0xFF, r.randint(0, 255)])
    c["value"] = r.choice([0, 1])
    c["observe_byte"] = True
    c.update(fixed)
    return c


def easy_edge():
    r = random.Random(15)
    cases = [g_easy(r, byte0=b, value=v) for b, v in itertools.product([0, 1, 0xA5, 0xFF, 0x80], [0, 1])]
    # triage: a value that is neither 0 nor 1 (the Mac stores the register's low byte; every caller passes a comparison's 0 or 1)
    cases += [g_easy(r, byte0=b, value=v) for b, v in itertools.product([0, 0xA5], [2, 255])]
    return cases


TESTS.append(fn_test("CFSGlobalMasterSave::SetEasyModeUser", master_first, lambda c, s: [c["value"]], easy_edge, g_easy))


# ---- SendQuit
def lobby_first(c, side):
    if side[0] == "ppc":
        return side[1].fsu.lobby
    return side[2].lobby


def g_quit(r, **fixed):
    c = base(r)
    c["lobby"] = r.choice([0, 1, 2, 3, -1, 5, 0x7FFFFFFF])
    c["restart"] = r.choice([0, 1, 2, -1, 0x7FFFFFFF])
    c.update(fixed)
    return c


def quit_edge():
    r = random.Random(16)
    return [g_quit(r, lobby=s, restart=x) for s, x in itertools.product([0, 1, 2, 3, -1, 5], [0, 1, 2, -1])]


TESTS.append(fn_test("CLBLobby::SendQuit", lobby_first, None, quit_edge, g_quit))
