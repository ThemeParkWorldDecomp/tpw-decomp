"""Differential tests for FS_UserInfo.cpp's start-up path (src/FS_UserInfo.cpp; Milestone 1
unit 5, part 2), against libtpw_unit_FSSystem.dylib (tests/unit_FSSystem.cpp's fakes),
with test_FSSystem.py's PowerPC strings, vtables and file-system fakes.

A case gives the login slots, the folders that exist (a directory tree: the users folder
lists "<slot><name>" folders), the themes, the master save and the paths' parts; the
unit's callees in FS_System.cpp and the string helpers run as themselves on both sides.
"""
import ctypes as C
import random
import struct

import calllog as CL
import test_FSSystem as FS
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_FSSystem"

F_ = {
    "CFSUserInfo::CFSUserInfo": 0x1017DE54, "TFSUserInfoGameLoginUser::~TFSUserInfoGameLoginUser": 0x1017DEE8,
    "TFSUserInfoGameLoginUser::TFSUserInfoGameLoginUser": 0x1017DF80, "CFSUserInfo::~CFSUserInfo": 0x1017DFEC,
    "CFSUserInfo::GameLoginInit": 0x1017E0A8, "CFSUserInfo::CheckAllThemeDirectoriesExist": 0x1017E634,
    "CFSUserInfo::GetNumValidGameLoginSlots": 0x1017E788, "CFSUserInfo::ReturnPathToSaveDirectory": 0x1017E838,
    "CFSUserInfo::ReturnPathToUsersDirectory": 0x1017E8FC, "CFSUserInfo::GetGameLoginSlotUserName": 0x1017E9D8,
    "CFSUserInfo::IsGameLoginSlotUserEasyMode": 0x1017EA74,
    "CFSUserInfo::ReturnGameLoginSlotUsersDirectoryName": 0x1017EAD8,
    "CFSUserInfo::ReturnGameLoginSlotUsersPath": 0x1017EBF0, "CFSUserInfo::ReturnUsersPath": 0x1017F7F4,
    "CFSUserInfo::ReturnUsersPathWithTheme": 0x1017F858, "CFSUserInfo::ReturnOnlinePath": 0x1017F980,
    "__sinit_FS_UserInfo_cpp": 0x1017FD58}
A = dict(gMasterSave=0x10380A8C, gFSUserInfoUpload=0x10380B58, gFSUserInfoGmsFile=0x10380B38)
WIDE_GLOBALS = dict(mWideSeparator=0x44C0, mSaveFolder=0x44D4, mUsersFolder=0x44E8, mOnlineFolder=0x44FC)
LABELS = dict(object=70, outbox=71, addressbook=72, slot=73, master_save=77, path=78, theme_name=79, theme=80)
TEXT_RESULT = {"CFSUserInfo::ReturnPathToSaveDirectory", "CFSUserInfo::ReturnPathToUsersDirectory",
               "CFSUserInfo::ReturnGameLoginSlotUsersDirectoryName", "CFSUserInfo::ReturnGameLoginSlotUsersPath",
               "CFSUserInfo::ReturnUsersPath", "CFSUserInfo::ReturnUsersPathWithTheme", "CFSUserInfo::ReturnOnlinePath"}
INT_RESULT = {"CFSUserInfo::GetNumValidGameLoginSlots", "CFSUserInfo::IsGameLoginSlotUserEasyMode"}
LABEL_RESULT = {"CFSUserInfo::GetGameLoginSlotUserName"}

VP, I32, U32, I64 = C.c_void_p, C.c_int32, C.c_uint32, C.c_int64
F = FS.F
w32 = FS.w32

# ======================================================================= PowerPC side


def build_stubs(emu):
    S = FS.build_stubs(emu)
    R = CL.recorder
    S.real.update(F_.values())

    def i(addr, name, spec="", ret=None, then=None):
        S.internal(addr, R(name, spec, ret, then))

    i(0x101694DC, "CFSGlobalMasterSave", "N")

    def master_dtor(e):
        L, obj, flag = e.cl, e.r(3), T.s16(e.r(4))
        L.rec("~CFSGlobalMasterSave", (L.lab(obj),))
        if flag > 0 and L.lab(obj) > 0:
            L.rec("delete", (L.lab(obj),))
            L.forget(obj)

    S.internal(0x1016997C, master_dtor)
    # a queue value 1 for "Load clears" empties gMasterSave (the caller then makes a new one)
    i(0x1016A6C8, "CFSGlobalMasterSave::Load", "Ls", ret=lambda e, L: 1,
      then=lambda e, L: w32(e, A["gMasterSave"], 0) if L.ret("Load clears") else None)
    i(0x1016A4FC, "CFSGlobalMasterSave::IsEasyModeUser", "L", "b")
    i(0x10203940, "Outbox", "N")
    i(0x10203BA8, "~Outbox", "L")
    i(0x102064CC, "AddressBook", "N")
    i(0x1017FC04, "~AddressBook", "L")          # the address book's inline destructor: the list's clear
    i(0x1014A058, "CGame::GetCurrentThemeName", "L", ret=lambda e, L: e.fsu.theme_name)

    def itow(e):
        value, text, radix = T.s32(e.r(3)), e.r(4), T.s32(e.r(5))
        e.cl.rec("_itow", (value, radix))
        e.write(text, b"".join(struct.pack(">H", ord(ch)) for ch in str(value)) + b"\0\0")
        e.set_r(3, text)

    S.imp("_itow__10NS_MacDozeFiPwi", itow)

    def atoi(e):
        text = e.cstr(e.r(3), limit=64).lstrip(" \t\n\v\f\r")
        sign, k = 1, 0
        if text[:1] in "+-" and text:
            sign, k = (-1 if text[0] == "-" else 1), 1
        digits = ""
        while k < len(text) and text[k].isdigit():
            digits += text[k]
            k += 1
        e.set_r(3, T.u32(sign * int(digits) if digits else 0))

    S.imp("atoi", atoi)
    return S


class UserState:
    pass


def ppc_slots(e, obj):
    names = [FS.latin(FS.s_read(e, obj + 0x18 * k, True)) for k in range(4)]
    easy = [T.s32(e.u32(obj + 0x18 * k + 0x14)) for k in range(4)]
    return names, easy, T.s32(e.u32(obj + 0x60))


def ppc_begin(e, c):
    FS.ppc_begin(e, c)
    u = e.fsu = UserState()
    L = e.cl
    for name, off in WIDE_GLOBALS.items():
        FS.s_write(e, FS.A["gTPGlobals"] + off, c["wide"][name], True)
    FS.s_write(e, A["gFSUserInfoGmsFile"], "gms.dat", True)
    u.theme_name = FS.new_string(e, c.get("theme_name", "Lost"), False, LABELS["theme_name"])
    # themes: nodes {name string, next}
    nxt = 0
    for k in range(len(c.get("themes", [])) - 1, -1, -1):
        node = e.alloc(8)
        s = FS.new_string(e, c["themes"][k], False, LABELS["theme"] + k)
        e.write(node, struct.pack(">II", s, nxt))
        nxt = node
    w32(e, FS.A["gGame"] + 0x28, nxt)
    # the master save
    u.master = 0
    if c.get("master_save"):
        u.master = e.alloc(0x4C)
        L.labels[u.master] = LABELS["master_save"]
    w32(e, A["gMasterSave"], u.master)
    # the object
    u.obj = 0
    if not c.get("null"):
        u.obj = e.alloc(0xD4)
        L.labels[u.obj] = LABELS["object"]
        L.labels[u.obj + 0x64] = LABELS["outbox"]
        L.labels[u.obj + 0xC4] = LABELS["addressbook"]
        if not c.get("blank"):
            for k, (name, easy) in enumerate(c["slots"]):
                FS.s_write(e, u.obj + 0x18 * k, name, True)
                if k:      # slot 0's name is at the object's own address (label object)
                    L.labels[u.obj + 0x18 * k] = LABELS["slot"] + k
                w32(e, u.obj + 0x18 * k + 0x14, easy)
            w32(e, u.obj + 0x60, c.get("user", -1))


def ppc_observe(e, c):
    out = FS.ppc_observe(e, c)
    u = e.fsu
    out["slots"] = ppc_slots(e, u.obj) if u.obj and not c.get("destroyed") else None
    out["master_save"] = e.cl.lab(e.u32(A["gMasterSave"]))
    return out

# ======================================================================= host side


def host_wide(lib, obj, text):
    units = (C.c_uint16 * (len(text) + 1))(*([ord(ch) for ch in text] + [0]))
    F(lib, "tpw_fs_set_wide", None, VP, VP)(obj, units)


def host_begin(lib, c):
    h = FS.host_begin(lib, c)
    for name in WIDE_GLOBALS:
        host_wide(lib, F(lib, "tpw_fsu_addr", VP, C.c_char_p)(name.encode()), c["wide"][name])
    host_wide(lib, F(lib, "tpw_fsu_addr", VP, C.c_char_p)(b"gFSUserInfoGmsFile"), "gms.dat")
    F(lib, "tpw_fsu_set_theme_name", None, C.c_char_p)(c.get("theme_name", "Lost").encode("latin-1"))
    h.fixed[LABELS["theme_name"]] = F(lib, "tpw_fsu_theme_name", VP)()
    themes = c.get("themes", [])
    names = (C.c_char_p * max(len(themes), 1))(*[t.encode("latin-1") for t in themes])
    strings = (C.c_void_p * max(len(themes), 1))()
    F(lib, "tpw_fsu_set_themes", None, VP, I32, VP)(names, len(themes), strings)
    for k in range(len(themes)):
        h.fixed[LABELS["theme"] + k] = strings[k]
    master = F(lib, "tpw_fsu_new_master_save", VP)() if c.get("master_save") else None
    F(lib, "tpw_fsu_set_master_save", None, VP)(master)
    if master:
        h.fixed[LABELS["master_save"]] = master
    h.obj = None
    if not c.get("null"):
        h.obj = F(lib, "tpw_fsu_buffer", VP)() if c.get("blank") else F(lib, "tpw_fsu_new", VP)()
        h.fixed[LABELS["object"]] = h.obj
        members = (C.c_void_p * 2)()
        F(lib, "tpw_fsu_members", None, VP, VP)(h.obj, members)
        h.fixed[LABELS["outbox"]], h.fixed[LABELS["addressbook"]] = members[0], members[1]
        if not c.get("blank"):
            for k, (name, easy) in enumerate(c["slots"]):
                units = (C.c_uint16 * (len(name) + 1))(*([ord(ch) for ch in name] + [0]))
                F(lib, "tpw_fsu_set_slot", None, VP, I32, VP, I32)(h.obj, k, units, easy)
            F(lib, "tpw_fsu_set_user", None, VP, I32)(h.obj, c.get("user", -1))
    return h


def host_observe(lib, h, c):
    out = FS.host_observe(lib, c)
    if h.obj and not c.get("destroyed"):
        names = C.create_string_buffer(4096)
        easy = (I32 * 4)()
        user = F(lib, "tpw_fsu_get", I32, VP, VP, VP)(h.obj, names, easy)
        out["slots"] = ([names.raw[1024 * k:1024 * (k + 1)].split(b"\0")[0].decode("latin-1") for k in range(4)],
                        list(easy), user)
    else:
        out["slots"] = None
    out["master_save"] = F(lib, "tpw_cl_lab", I32, VP)(F(lib, "tpw_fsu_master_save", VP)())
    return out

# ======================================================================= running a case


STUBS = None


def ctype_map():
    """MSL's C-locale character classes (__ctype_map): 0x01 control, 0x04 space, 0x08
    punctuation, 0x10 digit, 0x20 hex digit, 0x40 lower case, 0x80 upper case."""
    table = bytearray(256)
    for c in range(128):
        ch = chr(c)
        if c < 0x20 or c == 0x7F:
            table[c] |= 0x01
        if ch in " \t\n\v\f\r":
            table[c] |= 0x04
        if 0x21 <= c <= 0x7E and not ch.isalnum():
            table[c] |= 0x08
        if ch.isdigit():
            table[c] |= 0x30
        if ch in "abcdefABCDEF":
            table[c] |= 0x20
        if "a" <= ch <= "z":
            table[c] |= 0x40
        if "A" <= ch <= "Z":
            table[c] |= 0x80
    return bytes(table)


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    runner.emu.write(runner.emu.data_import("__ctype_map"), ctype_map())
    if "c" not in FS.VT:
        FS.make_vtables(runner.emu)
    STUBS = build_stubs(runner.emu)
    FS.STUBS = STUBS


def fn_test(name, args=None, edge=None, rnd=None):
    t = FS.FsTest(name, None, None, edge=edge, rnd=rnd)

    def ppc(e, c):
        saved = STUBS.activate(e)
        try:
            ppc_begin(e, c)
            a = args(c, ("ppc", e)) if args else []
            if name in TEXT_RESULT:
                buf = e.alloc(0x14)
                T.call_ppc(e, F_[name], [buf, e.fsu.obj] + [T.u32(v) for v in a])
                res = FS.latin(FS.s_read(e, buf, True))
            else:
                r3 = T.call_ppc(e, F_[name], [e.fsu.obj] + [T.u32(v) for v in a])[0]
                res = (T.s32(r3) if name in INT_RESULT else e.cl.lab(r3) if name in LABEL_RESULT else None)
            out = ppc_observe(e, c)
            out["log"] = e.cl.log
            out["result"] = res
        finally:
            CL.Stubs.deactivate(e, saved)
            e.cl = None
        t._p = out
        return out

    def host(lib, c):
        h = host_begin(lib, c)
        a = args(c, ("host", lib, h)) if args else []
        CL.host_begin(lib, h.fixed, c.get("q", {}))
        if h.obj and not c.get("blank"):
            set_slot_labels(lib, h)
        vals = (I64 * 8)(*([h.obj or 0] + [int(v) for v in a] + [0] * (7 - len(a))))
        res = F(lib, "tpw_fsu_call", I32, C.c_char_p, VP)(name.encode(), vals)
        log = CL.host_end(lib)
        out = host_observe(lib, h, c)
        out["log"] = log
        out["result"] = (C.string_at(C.addressof(C.c_char.in_dll(lib, "tpw_fs_result_text"))).decode("latin-1")
                         if name in TEXT_RESULT else res if name in INT_RESULT | LABEL_RESULT else None)
        t._last = (t._p, out)
        return out

    t.ppc, t.host = ppc, host
    return t


def set_slot_labels(lib, h):
    """Labels the four slot name strings (their addresses from the object's layout)."""
    for k in range(1, 4):
        addr = F(lib, "tpw_fsu_slot_name", VP, VP, I32)(h.obj, k)
        lib.tpw_cl_label(LABELS["slot"] + k, addr)

# ======================================================================= case generators


NAMES = ["", "", "Bob", "alice", "Sam Smith", "x", "€uro", "Al" * 20]


def base(r, **extra):
    c = FS.base(r)
    c["wide"] = dict(mWideSeparator=r.choice([":", ":", "/"]), mSaveFolder=r.choice(["save", "Save"]),
                     mUsersFolder=r.choice(["users", "u"]), mOnlineFolder=r.choice(["online", "net"]))
    c["paths"]["mLocalPath"] = r.choice(["HD:Games:TPW:", "HD:TPW:", "", "x:"])
    c["slots"] = [(r.choice(NAMES), r.choice([0, 1, 0, 7])) for _ in range(4)]
    c["user"] = r.choice([-1, 0, 1, 2, 3])
    c["themes"] = r.sample(["Lost", "Halloween", "Wonderland", "Space"], r.randint(0, 4))
    c["theme_name"] = r.choice(["Lost", "Space", "", "Haunted"])
    c["master_save"] = r.random() < 0.3
    c.update(extra)
    return c


def gen(make, n=40, seed=5):
    def edge():
        r = random.Random(seed)
        return [make(r) for _ in range(n)]
    return edge, make


TESTS = []


def add(name, make, args=None):
    edge, rnd = gen(make)
    TESTS.append(fn_test(name, args, edge, rnd))


def named_slot(r, c):
    """A slot with a user (the folder name functions need one)."""
    slots = [k for k, (name, _) in enumerate(c["slots"]) if name]
    if not slots:
        c["slots"][0] = ("Bob", 0)
        slots = [0]
    return r.choice(slots)


add("CFSUserInfo::CFSUserInfo", lambda r: base(r, blank=True))
add("CFSUserInfo::~CFSUserInfo", lambda r: base(r, flag=r.choice([0, 1, -1, 3]), null=r.random() < 0.1, destroyed=True),
    lambda c, s: [c["flag"]])
add("CFSUserInfo::GetNumValidGameLoginSlots", base)
add("CFSUserInfo::GetGameLoginSlotUserName", lambda r: base(r, slot=r.randint(0, 3)), lambda c, s: [c["slot"]])
add("CFSUserInfo::IsGameLoginSlotUserEasyMode", lambda r: base(r, slot=r.randint(0, 3)), lambda c, s: [c["slot"]])
add("CFSUserInfo::ReturnPathToSaveDirectory", base)
add("CFSUserInfo::ReturnPathToUsersDirectory", base)
add("CFSUserInfo::ReturnOnlinePath", base)


def g_named(r):
    c = base(r)
    c["slot"] = named_slot(r, c)
    return c


add("CFSUserInfo::ReturnGameLoginSlotUsersDirectoryName", g_named, lambda c, s: [c["slot"]])
add("CFSUserInfo::ReturnGameLoginSlotUsersPath", g_named, lambda c, s: [c["slot"]])


def g_user(r):
    c = g_named(r)
    c["user"] = c["slot"]
    return c


add("CFSUserInfo::ReturnUsersPath", g_user)
add("CFSUserInfo::ReturnUsersPathWithTheme", g_user)


def save_paths(c):
    local, wide = c["paths"]["mLocalPath"], c["wide"]
    save = local + wide["mSaveFolder"]
    return save, save + wide["mWideSeparator"] + wide["mUsersFolder"], save + wide["mWideSeparator"] + wide["mOnlineFolder"]


def g_check(r):
    c = base(r)
    c["path_arg"] = r.choice(["HD:TPW:save:users:1Bob", "u", ""])
    sep = c["globals"]["mSeparator"]
    c["tree"] = {c["path_arg"] + sep + t: [] for t in c["themes"] if r.random() < 0.5}
    return c


def check_args(c, side):
    if side[0] == "ppc":
        return [FS.new_string(side[1], c["path_arg"], True, LABELS["path"])]
    lib, h = side[1], side[2]
    return [FS.host_string(lib, h, c["path_arg"], True, LABELS["path"])]


add("CFSUserInfo::CheckAllThemeDirectoriesExist", g_check, check_args)


def g_login(r):
    c = base(r)
    c["globals"]["mSeparator"] = c["wide"]["mWideSeparator"]
    save, users, online = save_paths(c)
    sep = c["globals"]["mSeparator"]
    tree = {}
    if r.random() < 0.85:
        tree[save] = []
    if r.random() < 0.85:
        folders = []
        for _ in range(r.randint(0, 6)):
            slot = r.choice(["1", "2", "3", "4", "0", "5", "9", "x", ""])
            name = r.choice(["Bob", "alice", "", "Z" * 30, ".x"])
            folders.append((slot + name, r.choice([0x10, 0x10, 0x10, 0])))
        if r.random() < 0.2:
            folders.append((".hidden", 0x10))
        tree[users] = folders
        for folder, attributes in folders:
            for t in c["themes"]:
                if r.random() < 0.5:
                    tree[users + sep + folder + sep + t] = []
    if r.random() < 0.7:
        tree[online] = []
    c["tree"] = tree
    c["q"] = {"CFSGlobalMasterSave::IsEasyModeUser": [r.choice([0, 1]) for _ in range(4)],
              "Load clears": [r.choice([0, 0, 0, 1]) for _ in range(4)]}
    return c


add("CFSUserInfo::GameLoginInit", g_login)


# ---- the static initialiser
def sinit_ppc(e, c):
    saved = STUBS.activate(e)
    try:
        e.cl = CL.PpcLog({})
        regs = []
        e.stubs["__register_global_object"] = lambda em: regs.append((em.r(3), em.r(4)))
        for a in (A["gFSUserInfoUpload"], A["gFSUserInfoGmsFile"]):
            e.write(a, b"\0" * 0x14)
        T.call_ppc(e, F_["__sinit_FS_UserInfo_cpp"], [])
        texts = [FS.s_read(e, a, True) for a in (A["gFSUserInfoUpload"], A["gFSUserInfoGmsFile"])]
        wds_dtor = e.u32(e.toc - 0x7D70)     # the TVector of TbDynamicStringTemplate<wchar_t>'s destructor
    finally:
        CL.Stubs.deactivate(e, saved)
        e.cl = None
    return dict(texts=texts, registrations_ok=regs == [(A["gFSUserInfoUpload"], wds_dtor),
                                                        (A["gFSUserInfoGmsFile"], wds_dtor)])


def sinit_host(lib, c):
    out = (C.c_char * 192)()
    F(lib, "tpw_fsu_sinit_texts", None, VP)(out)
    return dict(texts=[out.raw[96 * i:96 * (i + 1)].split(b"\0")[0].decode("latin-1") for i in range(2)],
                registrations_ok=True)


TESTS.append(FnTest("__sinit_FS_UserInfo_cpp", sinit_ppc, sinit_host, edge=lambda: [{}], rnd=lambda r: {}))
