"""Differential tests for TPSettings.cpp's player settings (src/game/TPResource.cpp, src/game/TPSettings.cpp; Milestone 1
unit 6, part 1), against libtpw_unit_TPSettings.dylib (tests/unit_TPSettings.cpp's fakes;
see calllog.py), with test_FSSystem.py's PowerPC strings.

A case sets a CPlayerSettings (gPlayerSettings, or another object), sound.sam's and the
options files' values, Gestalt's answers, the bytes of the one fake file, the lobby and the
user info, runs the function and compares the logs, the result and every settings field.
The serialisation instances of other files (LOADVAR/SAVEVAR of bool and 32-bit integers)
are fakes on both sides that read and write the fake file (little-endian).
"""
import ctypes as C
import random
import struct

import calllog as CL
import test_FSSystem as FS
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_TPSettings"

P = "CPlayerSettings::"
F_ = {
    P + "~CPlayerSettings": 0x10165AD0, P + "Init": 0x10165B40, P + "GetMusicVolume": 0x10165D9C,
    P + "GetSFXVolume": 0x10165DDC, P + "GetSpeechVolume": 0x10165E1C, P + "GetMovieVolume": 0x10165E5C,
    P + "UpdateSettings": 0x10165E9C, P + "ResetSettings": 0x10166158, P + "StoreSettings": 0x10166244,
    P + "SetGameContinueState": 0x10166334, P + "ResetGameContinueState": 0x101663AC,
    P + "SetGameRefreshState": 0x101663F8, P + "ResetGameRefreshState": 0x1016646C,
    P + "SetSoundQuality": 0x101664BC, P + "SetRendererType": 0x101664FC, P + "SetResolution": 0x10166558,
    P + "SetVideoCard": 0x101665C0, P + "SetGraphicsDetail": 0x10166628, P + "LoadGraphicsDetails": 0x10166690,
    P + "HasTextureSizeChanged": 0x10166918, P + "HasNumKidsChanged": 0x10166970,
    P + "HasMPEGRateChanged": 0x101669C4, P + "UpdateVolume": 0x10166A40, P + "SerialiseLocal": 0x10166B7C,
    P + "SerialiseGlobal": 0x10166F7C, P + "SaveGlobal": 0x101671EC, P + "LoadGlobal": 0x10167348,
    "RestartCallback_OK": 0x101674C0, "RestartCallback_Cancel": 0x10167574, "RestartDialog": 0x101675B8,
    "SAVEVARSTRUCT<Volume>": 0x1016765C, "LOADVARSTRUCT<Volume>": 0x101677BC,
    "LOADVAR<eRENDERER>": 0x10167900, "LOADVAR<eRESOLUTION>": 0x10167A4C, "LOADVAR<eDETAIL>": 0x10167BA0,
    "LOADVAR<eVIDEOCARD>": 0x10167CE8, "SAVEVAR<eRENDERER>": 0x10167E38, "SAVEVAR<eRESOLUTION>": 0x10167F9C,
    "SAVEVAR<eDETAIL>": 0x10168108, "SAVEVAR<eVIDEOCARD>": 0x10168268,
    "__sinit_TPSettings_cpp": 0x101683D0}
A = dict(gPlayerSettings=0x1038071C, gGame=0x1037F134, gTPGlobals=0x103B74C8, gText=0x1037FF18,
         gTPFileInfo=0x102B3458, gLoadSave=0x10396A7C, gSaveLoad=0x1037FEFC, gLobby=0x102E4888,
         gUserInfo=0x10380B34, gSoundBalance=0x102B4E2C, gOptionsBalance=0x102B4DA8,
         gTPSettingsLanguages=0x10380804, gTPSettingsTitle=0x103807E4, gTPSettingsLanguage=0x103807C4)
WIDE = dict(mWideSeparator=0x44C0, mConfigFile=0x454C, mRefreshName=0x4538, mAutosaveName=0x4510)
NARROW = dict(mLowOptionsFile=0x41A0, mMediumOptionsFile=0x41B4, mHighOptionsFile=0x41C8)
LABELS = dict(settings=1, gGame=2, gText=3, gTPFileInfo=4, gLoadSave=5, gSaveLoad=6, lobby=7, file=8, table=9,
              userinfo=10, path=11)
FILE_HANDLE = 0x6E100000           # the PowerPC side's file (never dereferenced)
CALLBACKS = {0x10267390: 1, 0x10267398: 2}   # RestartCallback_OK, _Cancel TVectors


def settings_layout(base):
    words = [(base + o, 4) for o in (0, 4, 8, 0xC)]
    for o in (0x10, 0x18, 0x20, 0x28):
        words += [(base + o, 1), (base + o + 4, 4)]
    words.append((base + 0x30, 4))
    words += [(base + 0x34 + k, 1) for k in range(7)]
    return words


FIELDS = settings_layout(0) + settings_layout(0x3C) + [(o, 4) for o in range(0x78, 0x9C, 4)]
INT_RESULT = {P + "GetMusicVolume", P + "GetSFXVolume", P + "GetSpeechVolume", P + "GetMovieVolume",
              P + "LoadGraphicsDetails", P + "SerialiseLocal", P + "SerialiseGlobal", P + "SaveGlobal", P + "LoadGlobal",
              "RestartCallback_OK", "RestartCallback_Cancel", "SAVEVARSTRUCT<Volume>", "LOADVARSTRUCT<Volume>",
              "LOADVAR<eRENDERER>", "LOADVAR<eRESOLUTION>", "LOADVAR<eDETAIL>", "LOADVAR<eVIDEOCARD>",
              "SAVEVAR<eRENDERER>", "SAVEVAR<eRESOLUTION>", "SAVEVAR<eDETAIL>", "SAVEVAR<eVIDEOCARD>"}
BYTE_RESULT = {P + "HasTextureSizeChanged", P + "HasNumKidsChanged", P + "HasMPEGRateChanged"}

VP, I32, U32, I64 = C.c_void_p, C.c_int32, C.c_uint32, C.c_int64
F = FS.F
w32 = FS.w32


def hexs(data):
    return data.hex()[:150]

# ======================================================================= PowerPC side


class PsState:
    pass


def build_stubs(emu):
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(F_.values())
    FS.string_stubs(S)

    def i(addr, name, spec="", ret=None, then=None):
        S.internal(addr, R(name, spec, ret, then))

    st = lambda e: e.ps   # noqa: E731

    def gestalt(e):
        L, sel = e.cl, e.r(3)
        L.rec("Gestalt", (sel,))
        w32(e, e.r(4), L.ret(f"GestaltValue:{sel}", 0))
        e.set_r(3, L.ret(f"Gestalt:{sel}", 0) & 0xFFFF)

    S.imp("Gestalt", gestalt)
    for addr, name, spec in ((0x100DD2BC, "SOUND_SetMusicStatus", "u"), (0x100DD2FC, "SOUND_SetSfxStatus", "u"),
                             (0x100DD33C, "SOUND_SetSpeechStatus", "u"), (0x100DD1F8, "SOUND_SetMusicVolume", "u"),
                             (0x100DD238, "SOUND_SetSfxVolume", "u"), (0x100DD278, "SOUND_SetSpeechVolume", "u"),
                             (0x100DD41C, "SOUND_StopAllSfx", ""), (0x100DCF0C, "SOUND_SetSoundQuality", "i"),
                             (0x100DD668, "SOUND_Tick", ""), (0x100B9CBC, "MOVIE_SetVolume", "u"),
                             (0x100346A0, "RoundRotationToNearest90Degrees", ""), (0x101AB904, "RemoveOptionsPanel", ""),
                             (0x10236E6C, "GS_SetCurrentGameStatus", "i"), (0x1001EA28, "CBalanceTable::Init", ""),
                             (0x1001ED40, "CBalanceTable::UnInit", "")):
        i(addr, name, spec)

    def wide_event(name):
        return lambda e: e.cl.rec(name, (e.cl.lab(e.r(3)),), (FS.narrow(FS.s_read(e, e.r(4), True)),))

    S.internal(0x101FC568, wide_event("CFSLoadSave::SaveHidden"))
    S.internal(0x101FC47C, wide_event("CFSLoadSave::LoadHidden"))
    S.internal(0x101FC4F4, wide_event("CFSLoadSave::Save"))
    i(0x1017DE54, "CFSUserInfo", "N")

    def save_directory(e):
        e.cl.rec("CFSUserInfo::ReturnPathToSaveDirectory", (e.cl.lab(e.r(4)),))
        FS.s_write(e, e.r(3), "HD:TPW:save", True)

    S.internal(0x1017E838, save_directory)

    def uni_to_string(e):
        text = FS.narrow(FS.s_read(e, e.r(4), True))
        e.cl.rec("UniStringToString", (e.cl.lab(e.r(3)),), (text,))
        FS.s_write(e, e.r(3), text, False)
        e.set_r(3, len(text) + 1)

    S.internal(0x10005670, uni_to_string)

    def data_path(e):
        e.cl.rec("CSYS_TPFileInfo::GetDataPath", (e.cl.lab(e.r(3)),))
        obj = e.alloc(0x14)
        FS.s_write(e, obj, st(e).data_path, False)
        e.set_r(3, obj)

    S.internal(0x10004EC4, data_path)

    def add_separator(e):
        text = FS.s_read(e, e.r(3), False)
        e.cl.rec("FS_AddDirectorySeparator", (), (text[:159],))
        if not text.endswith(":"):
            FS.s_write(e, e.r(3), text + ":", False)

    S.internal(0x10004BC0, add_separator)

    def balance_load(e):
        L = e.cl
        L.rec("CBalanceTable::Load", (T.s32(e.r(5)),), (e.cstr(e.r(4))[:159],))
        result = L.ret("CBalanceTable::Load", 1)
        if result:
            for k, v in enumerate(st(e).options_file):
                w32(e, e.r(3) + 8 + 4 * k, v)
        e.set_r(3, T.u32(result))

    S.internal(0x10020134, balance_load)
    S.internal(0x1000DDE8, lambda e: e.cl.rec("GAME_ASSERTMSG", (T.s32(e.r(3)),), (e.cstr(e.r(4))[:159],)))
    S.internal(0x1006FCBC, lambda e: e.cl.rec("CTRACE", (e.r(3),), (T.c_format(e, e.cstr(e.r(4)), 5)[:159],)))
    i(0x1015B3DC, "CSaveLoad::WriteSerialiseLog", "Lisi")

    # ---- the fake file
    def read_file(e, size):
        s = st(e)
        data = s.file[s.pos:s.pos + size]
        s.pos += len(data)
        return data

    def lb_open(e):
        L = e.cl
        L.rec("LbFile_Open", (e.r(6),), (e.cstr(e.r(5))[:159],))
        result = L.ret("LbFile_Open")
        w32(e, e.r(4), FILE_HANDLE if result >= 0 else 0)
        w32(e, e.r(3), result)

    def lb_close(e):
        e.cl.rec("LbFile_Close", (e.cl.lab(e.r(4)),))
        w32(e, e.r(3), 0)

    def lb_read(e):
        # a read that fails or comes up short leaves the buffer alone (as the host's fake)
        L, buf, length = e.cl, e.r(5), e.r(6)
        L.rec("LbFile_Read", (L.lab(e.r(4)), length))
        data = read_file(e, length)
        n = len(data)
        if L.ret("LbFile_Read:short") and n:
            n -= 1
        w32(e, e.r(7), n)
        result = L.ret("LbFile_Read")
        if n == length and result >= 0 and length:
            e.write(buf, data)
        w32(e, e.r(3), result)

    def lb_write(e):
        L, length = e.cl, e.r(6)
        L.rec("LbFile_Write", (L.lab(e.r(4)), length), (hexs(e.read(e.r(5), length)) if length else "",))
        w32(e, e.r(7), length - 1 if L.ret("LbFile_Write:short") else length)
        w32(e, e.r(3), L.ret("LbFile_Write"))

    S.imp("LbFile_Open__FRPvPCcUl", lb_open)
    S.imp("LbFile_Close__FPv", lb_close)
    S.imp("LbFile_Read__FPvPvUlPUl", lb_read)
    S.imp("LbFile_Write__FPvPCvUlPUl", lb_write)

    def fake_load(kind, size):
        def stub(e):
            L = e.cl
            L.rec(kind, (T.s32(e.r(6)),), (e.cstr(e.r(5))[:159],))
            data = read_file(e, size)
            if len(data) != size:
                e.set_r(3, 1)
                return
            if size == 1:
                e.write(e.r(4), data)
            else:
                w32(e, e.r(4), struct.unpack("<I", data)[0])
            e.set_r(3, T.u32(L.ret(kind)))
        return stub

    def fake_save(kind, size):
        def stub(e):
            L = e.cl
            data = e.read(e.r(4), 1) if size == 1 else struct.pack("<I", e.u32(e.r(4)))
            L.rec(kind, (T.s32(e.r(6)),), (e.cstr(e.r(5))[:159], hexs(data)))
            e.set_r(3, T.u32(L.ret(kind)))
        return stub

    S.internal(0x1000F144, fake_load("LOADVAR<bool>", 1))
    S.internal(0x1000EF18, fake_load("LOADVAR<int32>", 4))     # LOADVAR<int>
    S.internal(0x1000EE04, fake_load("LOADVAR<int32>", 4))     # LOADVAR<long>
    S.internal(0x1000F70C, fake_save("SAVEVAR<bool>", 1))
    S.internal(0x1000F4B0, fake_save("SAVEVAR<int32>", 4))     # SAVEVAR<int>
    S.internal(0x1000F384, fake_save("SAVEVAR<int32>", 4))     # SAVEVAR<long>

    def format_string(e):
        e.cl.rec("TbITable::FormatString", (e.cl.lab(e.r(3)), e.r(5), e.r(6)))
        text = "Restart the game?"
        e.write(e.r(4), b"".join(struct.pack(">H", ord(ch)) for ch in text) + b"\0\0")

    S.imp("FormatString__8TbITableCFPwUiUle", format_string)

    def dialog(e):
        p = e.r(3)
        red, green, blue = e.read(p + 4, 3)
        cb = lambda a: CALLBACKS.get(a, -1 if a else 0)   # noqa: E731
        e.cl.rec("UI_DialogBox", (red, green, blue, T.s32(e.u32(p + 8)), T.s32(e.r(4)), cb(e.r(5)), T.s32(e.r(6)),
                                  cb(e.r(7)), T.s32(e.r(8)), cb(e.r(9)), 1 if e.r(10) & 0xFF else 0),
                 (FS.narrow(FS.wide_read(e, e.u32(p))),))

    S.internal(0x101F1800, dialog)

    def op_delete(e):
        L, obj = e.cl, e.r(3)
        if obj and L.lab(obj) > 0:
            L.rec("delete", (L.lab(obj),))
            L.forget(obj)

    S.imp("__dl__FPv", op_delete)
    return S


def ppc_settings_write(e, obj, values):
    e.write(obj, b"\0" * 0x9C)
    for (off, size), v in zip(FIELDS, values):
        if size == 1:
            e.write(obj + off, bytes([v & 0xFF]))
        else:
            w32(e, obj + off, v)


def ppc_settings_read(e, obj):
    return [e.read(obj + off, 1)[0] if size == 1 else T.s32(e.u32(obj + off)) for off, size in FIELDS]


def ppc_begin(e, c):
    s = e.ps = PsState()
    e.heap_in_scratch = True
    fixed = {A["gGame"]: LABELS["gGame"], A["gText"]: LABELS["gText"], A["gTPFileInfo"]: LABELS["gTPFileInfo"],
             A["gLoadSave"]: LABELS["gLoadSave"], A["gSaveLoad"]: LABELS["gSaveLoad"], FILE_HANDLE: LABELS["file"]}
    s.obj = A["gPlayerSettings"]
    if c.get("other"):
        s.obj = e.alloc(0x9C)
    fixed[s.obj] = LABELS["settings"]
    e.cl = CL.PpcLog(fixed, c.get("q", {}))
    if c.get("global_settings") is not None:
        ppc_settings_write(e, A["gPlayerSettings"], c["global_settings"])
    ppc_settings_write(e, s.obj, c["settings"])
    for k, v in enumerate(c["sound"]):
        w32(e, A["gSoundBalance"] + 8 + 4 * k, v)
    for k, v in enumerate(c["options"]):
        w32(e, A["gOptionsBalance"] + 8 + 4 * k, v)
    s.options_file = c["options_file"]
    s.file = bytes(c.get("file", b""))
    s.pos = 0
    s.data_path = c.get("data_path", "HD:TPW:data")
    for name, off in WIDE.items():
        FS.s_write(e, A["gTPGlobals"] + off, c["wide"][name], True)
    for name, off in NARROW.items():
        FS.s_write(e, A["gTPGlobals"] + off, c["narrow"][name], False)
    w32(e, A["gGame"] + 0x18, c.get("restart", 0))
    s.lobby = 0
    if c.get("lobby"):
        s.lobby = e.alloc(0x38)
        w32(e, s.lobby + 0x14, c.get("lobby_state", 1))
        e.cl.labels[s.lobby] = LABELS["lobby"]
    w32(e, A["gLobby"], s.lobby)
    s.userinfo = 0
    if c.get("userinfo"):
        s.userinfo = e.alloc(0xD4)
        e.cl.labels[s.userinfo] = LABELS["userinfo"]
    w32(e, A["gUserInfo"], s.userinfo)
    table = e.alloc(16)
    e.cl.labels[table] = LABELS["table"]
    w32(e, A["gText"] + 0x564, table)


def ppc_observe(e, c):
    s = e.ps
    out = {"settings": ppc_settings_read(e, s.obj) if not c.get("destroyed") else None,
           "restart": T.s32(e.u32(A["gGame"] + 0x18)),
           "lobby_state": T.s32(e.u32(s.lobby + 0x14)) if s.lobby else None,
           "userinfo": e.cl.lab(e.u32(A["gUserInfo"])), "file_position": s.pos}
    if s.obj != A["gPlayerSettings"]:
        out["global"] = ppc_settings_read(e, A["gPlayerSettings"])
    return out

# ======================================================================= host side


class HostState:
    pass


def host_wide(lib, obj, text):
    units = (C.c_uint16 * (len(text) + 1))(*([ord(ch) for ch in text] + [0]))
    F(lib, "tpw_ps_set_wide", None, VP, VP)(obj, units)


def host_begin(lib, c):
    h = HostState()
    h.keep = []
    addr = lambda n: F(lib, "tpw_ps_addr", VP, C.c_char_p)(n.encode())   # noqa: E731
    h.fixed = {LABELS["gGame"]: addr("gGame"), LABELS["gText"]: addr("gText"), LABELS["gTPFileInfo"]: addr("gTPFileInfo"),
               LABELS["gLoadSave"]: addr("gLoadSave"), LABELS["gSaveLoad"]: addr("gSaveLoad"),
               LABELS["file"]: addr("file")}
    sset = F(lib, "tpw_ps_set", None, VP, VP)
    h.obj = addr("gPlayerSettings")
    if c.get("other"):
        h.obj = F(lib, "tpw_ps_new_object", VP)() if c.get("deletable") else F(lib, "tpw_ps_new_settings", VP)()
    h.fixed[LABELS["settings"]] = h.obj
    if c.get("global_settings") is not None:
        sset(addr("gPlayerSettings"), (I32 * len(FIELDS))(*[T.s32(v) for v in c["global_settings"]]))
    sset(h.obj, (I32 * len(FIELDS))(*[T.s32(v) for v in c["settings"]]))
    bal = F(lib, "tpw_ps_set_balance", None, VP, VP, I32)
    bal(addr("gSoundBalance"), (I32 * len(c["sound"]))(*c["sound"]), len(c["sound"]))
    bal(addr("gOptionsBalance"), (I32 * len(c["options"]))(*c["options"]), len(c["options"]))
    of = c["options_file"]
    F(lib, "tpw_ps_set_options_file", None, VP, I32)((I32 * max(len(of), 1))(*of), len(of))
    data = bytes(c.get("file", b""))
    F(lib, "tpw_ps_set_file", None, C.c_char_p, I32)(data, len(data))
    F(lib, "tpw_ps_set_data_path", None, C.c_char_p)(c.get("data_path", "HD:TPW:data").encode())
    gstr = F(lib, "tpw_ps_tpglobals_string", VP, C.c_char_p)
    for name in WIDE:
        host_wide(lib, gstr(name.encode()), c["wide"][name])
    for name in NARROW:
        F(lib, "tpw_ps_set_string", None, VP, C.c_char_p)(gstr(name.encode()), c["narrow"][name].encode())
    lobby = F(lib, "tpw_ps_lobby", VP)()
    C.c_void_p.from_address(addr("gLobby")).value = lobby if c.get("lobby") else None
    h.lobby = lobby if c.get("lobby") else None
    if h.lobby:
        F(lib, "tpw_ps_set_lobby_state", None, VP, I32)(lobby, c.get("lobby_state", 1))
        h.fixed[LABELS["lobby"]] = lobby
    h.userinfo = None
    if c.get("userinfo"):
        h.userinfo = F(lib, "tpw_ps_new_userinfo", VP)()
        h.fixed[LABELS["userinfo"]] = h.userinfo
    C.c_void_p.from_address(addr("gUserInfo")).value = h.userinfo
    table = F(lib, "tpw_ps_text_table", VP)()
    F(lib, "tpw_ps_set_text_table", None, VP)(table)
    h.fixed[LABELS["table"]] = table
    F(lib, "tpw_ps_set_restart", None, I32)(c.get("restart", 0))
    return h


def host_settings_read(lib, obj):
    out = (I32 * len(FIELDS))()
    F(lib, "tpw_ps_get", None, VP, VP)(obj, out)
    return [v & 0xFF if size == 1 else v for v, (_, size) in zip(out, FIELDS)]


def host_observe(lib, h, c):
    addr = lambda n: F(lib, "tpw_ps_addr", VP, C.c_char_p)(n.encode())   # noqa: E731
    out = {"settings": host_settings_read(lib, h.obj) if not c.get("destroyed") else None,
           "restart": F(lib, "tpw_ps_restart", I32)(),
           "lobby_state": F(lib, "tpw_ps_lobby_state", I32, VP)(h.lobby) if h.lobby else None,
           "userinfo": F(lib, "tpw_cl_lab", I32, VP)(C.c_void_p.from_address(addr("gUserInfo")).value),
           "file_position": F(lib, "tpw_ps_file_position", I32)()}
    if h.obj != addr("gPlayerSettings"):
        out["global"] = host_settings_read(lib, addr("gPlayerSettings"))
    return out

# ======================================================================= running a case


STUBS = None


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    if "c" not in FS.VT:
        FS.make_vtables(runner.emu)
    STUBS = build_stubs(runner.emu)


def fn_test(name, args=None, edge=None, rnd=None):
    """args(c, side) -> the arguments after `this` (none for free functions: see FREE)."""
    t = FS.FsTest(name, None, None, edge=edge, rnd=rnd)
    free = name in ("RestartCallback_OK", "RestartCallback_Cancel", "RestartDialog") or "VAR" in name

    def ppc(e, c):
        saved = STUBS.activate(e)
        try:
            ppc_begin(e, c)
            a = args(c, ("ppc", e)) if args else []
            this = [] if free else [0 if c.get("null") else e.ps.obj]
            r3 = T.call_ppc(e, F_[name], this + [T.u32(v) for v in a])[0]
            res = T.s32(r3) if name in INT_RESULT else r3 & 0xFF if name in BYTE_RESULT else None
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
        this = 0 if c.get("null") else h.obj
        # the instances' value is a field of the object: `this` goes first for them too
        vals = (I64 * 8)(this, *[int(v) for v in a], *([0] * (7 - len(a))))
        res = F(lib, "tpw_ps_call", I32, C.c_char_p, VP)(name.encode(), vals)
        log = CL.host_end(lib)
        out = host_observe(lib, h, c)
        out["log"] = log
        out["result"] = res if name in INT_RESULT | BYTE_RESULT else None
        t._last = (t._p, out)
        return out

    t.ppc, t.host = ppc, host
    return t

# ======================================================================= case generators


def rnd_settings_block(r):
    block = [r.choice([0, 1, 1, 2]), r.choice([0, 1, 2, 3]), r.choice([0, 1, 2, 2, 3, -1]), r.choice([0, 1])]
    for _ in range(4):
        block += [r.choice([0, 1]), r.choice([0, 50, 100, 37, 99, 1, r.randint(-5, 400)])]
    block.append(r.choice([0, 10, 50, 64, 65, 100, r.randint(0, 200)]))
    block += [r.choice([0, 1]) for _ in range(7)]
    return block


def rnd_settings(r):
    current = rnd_settings_block(r)
    stored = list(current) if r.random() < 0.4 else rnd_settings_block(r)
    if r.random() < 0.3:          # change one field only
        stored = list(current)
        k = r.randrange(len(current))
        stored[k] = stored[k] ^ 1
    extra = [r.getrandbits(32), r.getrandbits(32), r.getrandbits(32), r.randint(-1, 9), r.getrandbits(32),
             r.choice([0, 8, 16]), r.choice([0, 1, 2]), r.choice([0, 1, 2, 3, -1]), r.getrandbits(8)]
    return current + stored + extra


def base(r, **extra):
    c = dict(settings=rnd_settings(r),
             sound=[r.randint(0, 3) for _ in range(6)] + [r.choice([64, 50, 0, 100])] + [0, 0] +
                   [r.choice([60, 61, 0, 100, 7])] + [0] + [r.choice([0, 1, 1])] +
                   [r.randint(0, 100) for _ in range(4)] + [0] * 14,
             options=[r.randint(0, 3) for _ in range(28)],
             wide=dict(mWideSeparator=":", mConfigFile="Config.tcf", mRefreshName="Refresh", mAutosaveName="autosave"),
             narrow=dict(mLowOptionsFile="low.sam", mMediumOptionsFile="med.sam", mHighOptionsFile="high.sam"),
             lobby=r.random() < 0.5, lobby_state=r.choice([1, 2]), userinfo=r.random() < 0.7,
             restart=r.choice([0, 7]), data_path=r.choice(["HD:TPW:data", "HD:TPW:data:", ""]), q={})
    c["options_file"] = [v if r.random() < 0.7 else v + r.choice([1, -1]) for v in c["options"]]
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


def g_init(r):
    c = base(r)
    ram, pclk = 0x72616D20, 0x70636C6B
    c["q"] = {f"Gestalt:{ram}": [r.choice([0, 0, 0, -1])], f"Gestalt:{pclk}": [r.choice([0, 0, 0, -5])],
              f"GestaltValue:{ram}": [r.choice([32, 64, 65, 128, 191, 192, 256, 1000]) * 0x100000 + r.randint(0, 0xFFFFF)],
              f"GestaltValue:{pclk}": [r.choice([200, 300, 301, 449, 450, 500]) * 1000000 + r.randint(0, 999999)]}
    c["other"] = r.random() < 0.2
    return c


add(P + "Init", g_init)
add(P + "~CPlayerSettings", lambda r: base(r, other=True, deletable=True, destroyed=True, flag=r.choice([0, 1, -1, 3]),
                                           null=r.random() < 0.1), lambda c, s: [c["flag"]])
for getter in ("GetMusicVolume", "GetSFXVolume", "GetSpeechVolume", "GetMovieVolume", "ResetSettings", "StoreSettings",
               "ResetGameContinueState", "ResetGameRefreshState", "UpdateVolume"):
    add(P + getter, base)
for setter, values in (("SetGameContinueState", [0, 1, 2, 3, -1]), ("SetGameRefreshState", [0, 1, 2, 7, 8, 0xF0]),
                       ("SetSoundQuality", [0, 50, -1]), ("SetRendererType", [0, 1, 2]),
                       ("SetResolution", [0, 1, 2, 3]), ("SetVideoCard", [0, 1]), ("SetGraphicsDetail", [0, 1, 2])):
    add(P + setter, (lambda vs: lambda r: base(r, value=r.choice(vs)))(values), lambda c, s: [c["value"]])
for check in ("HasTextureSizeChanged", "HasNumKidsChanged", "HasMPEGRateChanged"):
    add(P + check, lambda r: base(r, a=r.choice([0, 63, 64, 65, 100, 61, 60]), b=r.choice([0, 63, 64, 65, 100, 61, 60])),
        lambda c, s: [c["a"], c["b"]])


def g_graphics(r):
    c = base(r, detail=r.choice([0, 1, 2, 3, -1]))
    c["q"] = {"CBalanceTable::Load": [r.choice([1, 1, 1, 0])]}
    return c


add(P + "LoadGraphicsDetails", g_graphics, lambda c, s: [c["detail"]])


def g_update(r):
    c = g_graphics(r)
    c["q"]["LbFile_Open"] = [r.choice([0, 0, -1])]
    return c


add(P + "UpdateSettings", g_update)

# ---- serialisation: files built for the reads the function makes


def le32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def volume_bytes(r):
    return bytes([r.choice([0, 1])]) + bytes(r.getrandbits(8) for _ in range(3)) + le32(r.choice([0, 50, 100, 0x12345678]))


def local_file(r):
    return b"".join(volume_bytes(r) for _ in range(4)) + bytes(r.choice([0, 1]) for _ in range(7))


def global_file(r):
    return b"".join(le32(r.choice([0, 1, 2, 3, -1])) for _ in range(4)) + volume_bytes(r) + le32(r.randint(-5, 120))


def truncate(r, data):
    return data if r.random() < 0.7 else data[:r.randint(0, len(data))]


def file_queues(r):
    """Failures at a random call: the queues give 0 (success) until the failing one."""
    q = {}
    if r.random() < 0.15:
        q["LbFile_Read"] = [0] * r.randint(0, 6) + [-1]
    if r.random() < 0.1:
        q["LbFile_Read:short"] = [0] * r.randint(0, 6) + [1, 0]
    if r.random() < 0.15:
        q["LbFile_Write"] = [0] * r.randint(0, 6) + [-1]
    if r.random() < 0.1:
        q["LbFile_Write:short"] = [0] * r.randint(0, 6) + [1, 0]
    for kind in ("LOADVAR<bool>", "SAVEVAR<bool>", "LOADVAR<int32>", "SAVEVAR<int32>"):
        if r.random() < 0.2:
            q[kind] = [0] * r.randint(0, 7) + [1]
    return q


def g_serialise(make_file):
    def make(r):
        c = base(r, save=r.choice([0, 1, 2]), version=r.getrandbits(32))
        c["file"] = truncate(r, make_file(r))
        c["q"] = file_queues(r)
        return c
    return make


def file_arg(c, side):
    return FILE_HANDLE if side[0] == "ppc" else F(side[1], "tpw_ps_addr", VP, C.c_char_p)(b"file")


add(P + "SerialiseLocal", g_serialise(local_file), lambda c, s: [file_arg(c, s), c["save"], c["version"]])
add(P + "SerialiseGlobal", g_serialise(global_file), lambda c, s: [file_arg(c, s), c["save"], c["version"]])


def path_arg(c, side):
    if side[0] == "ppc":
        return FS.new_string(side[1], c["path"], False, LABELS["path"])
    lib, h = side[1], side[2]
    obj = F(lib, "tpw_ps_new_string", VP, C.c_char_p)(c["path"].encode())
    h.fixed[LABELS["path"]] = obj
    return obj


def g_global_file(r):
    c = g_serialise(lambda r_: le32(r_.choice([1, 1, 2, 0, -1])) + global_file(r_))(r)
    c["path"] = r.choice(["HD:TPW:save:Config.tcf", ""])
    c["q"]["LbFile_Open"] = [r.choice([0, 0, 0, -1])]
    return c


add(P + "SaveGlobal", g_global_file, lambda c, s: [path_arg(c, s)])
add(P + "LoadGlobal", lambda r: (lambda c: (c["q"].update({k: v for k, v in g_init(r)["q"].items()}), c)[1])(g_global_file(r)),
    lambda c, s: [path_arg(c, s)])
add("RestartCallback_OK", base, lambda c, s: [0x1234])
add("RestartCallback_Cancel", base, lambda c, s: [0x1234])
add("RestartDialog", base)


def g_instance(load, size):
    def make(r):
        c = base(r, no_log=r.choice([0, 1, 1]), label=r.choice(["Renderer", "x"]))
        data = volume_bytes(r) if size == 8 else le32(r.choice([0, 1, 2, -1, 0x01020304]))
        c["file"] = truncate(r, data) if load else b""
        c["q"] = file_queues(r)
        return c
    return make


def instance_args(c, side):
    if side[0] == "ppc":
        e = side[1]
        return [FILE_HANDLE, e.ps.obj + c["field"], CL.new_text(e, c["label"]), c["no_log"]]
    lib, h = side[1], side[2]
    keep = C.create_string_buffer(c["label"].encode())
    h.keep.append(keep)
    return [F(lib, "tpw_ps_addr", VP, C.c_char_p)(b"file"), 0, C.addressof(keep), c["no_log"]]


for kind, field in (("RENDERER", 0), ("RESOLUTION", 4), ("DETAIL", 8), ("VIDEOCARD", 0xC)):
    for load in (True, False):
        nm = ("LOADVAR<e" if load else "SAVEVAR<e") + kind + ">"
        mk = g_instance(load, 4)
        add(nm, (lambda mk_, f_: lambda r: (lambda c: (c.update(field=f_), c)[1])(mk_(r)))(mk, field), instance_args)
for load in (True, False):
    nm = "LOADVARSTRUCT<Volume>" if load else "SAVEVARSTRUCT<Volume>"
    mk = g_instance(load, 8)
    add(nm, (lambda mk_: lambda r: (lambda c: (c.update(field=0x28), c)[1])(mk_(r)))(mk), instance_args)


# ---- the static initialiser
def sinit_ppc(e, c):
    saved = STUBS.activate(e)
    try:
        ppc_begin(e, base(random.Random(1)))
        regs = []
        e.stubs["__register_global_object"] = lambda em: regs.append((em.r(3), em.r(4)))
        texts = [A["gTPSettingsLanguages"] + 0x14 * k for k in range(16)] + [A["gTPSettingsTitle"],
                                                                             A["gTPSettingsLanguage"]]
        for a in texts:
            e.write(a, b"\0" * 0x14)
        T.call_ppc(e, F_["__sinit_TPSettings_cpp"], [])
        out = [FS.s_read(e, a, False) for a in texts]
        tds_dtor = e.u32(e.toc - 0x7C1C)
    finally:
        CL.Stubs.deactivate(e, saved)
        e.cl = None
    ok = regs == [(a, tds_dtor) for a in texts] + [(A["gPlayerSettings"], 0x102673A0)]
    return dict(texts=out, registrations_ok=ok)


def sinit_host(lib, c):
    out = (C.c_char * (96 * 18))()
    F(lib, "tpw_ps_sinit_texts", None, VP)(out)
    return dict(texts=[out.raw[96 * i:96 * (i + 1)].split(b"\0")[0].decode("latin-1") for i in range(18)],
                registrations_ok=True)


TESTS.append(FnTest("__sinit_TPSettings_cpp", sinit_ppc, sinit_host, edge=lambda: [{}], rnd=lambda r: {}))
