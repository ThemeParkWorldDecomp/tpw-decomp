"""Differential tests for main TPMain.cpp (ThemeParkWorld and main). See difftest.py,
tpwtest.py and tests/shim_TPMain.cpp (the host side).

Every callee that is not translated is a recording fake on both sides: on the PowerPC side
in-binary callees are replaced with stub_internal and imports with emu.stub; on the host
they are the weak placeholders of shim_TPMain.cpp. A case compares the two call logs
(callee, arguments, object labels and strings, in order), the return value and the
globals and fields the function writes. Objects compare by label (see LABELS). The
translated functions of the unit that others call (GetUIMousePos, LookForCd, IsCdPresent,
LoadSystemBalanceFiles, SetInitialOnlineValues) run for real on both sides, and so do
the MSL std::string copies the PowerPC side uses for OnlineConfig.

The fakes are functional where values flow on: the bullfrog strings and UStrings keep
real text (the host never frees, the PowerPC side allocates in the case's scratch),
GetPath writes a path, FS_AddDirectorySeparator appends ':', ToLocal/ToGlobal subtract or
add an offset, UEvent::GetPosition reads the EventRecord.
"""
SHIM_LIBRARY = "legacy_testshim"
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"

EVENTS = [
    "FS_ReadRegistry", "FS_ReadLanguageRegistry", "GetLocalPath", "GetDataPath", "SetRemotePath",
    "FS_AddDirectorySeparator", "FS_ScanThemes", "InitialiseGameStage1", "InitialiseGameStage2",
    "InitialiseGameStage3", "TPSaveRegistry", "ClosedownGame", "InitialiseMemorySystem",
    "InitialiseInstancing", "EngineStartup", "EngineCloseDown", "LoadGlobal", "PlayerSettingsInit",
    "OnlineCtor", "OnlineDtor", "OnlineTick", "UserInfoCtor", "UserInfoDtor", "GameLoginInit",
    "ReportAnyError", "TextUnInit", "TextInit", "TextSetRez", "GAMESYSTEM_Close", "GAMESYSTEM_Init",
    "GAMESYSTEM_Tick", "InitResources", "GamePause", "UI_SetRealScreenRez", "KeyDown", "KeyUp",
    "UI_Keydown", "UI_Keyup", "UI_KeyChar", "UI_DevicePointerMove", "UI_DeviceButtonDown",
    "UI_DeviceButtonUp", "UI_DeviceMouseWheel", "UDebug", "MOVIE_Stop", "SOUND_Pause", "SOUND_UnPause",
    "UI_Activate", "UI_DeActivate", "UIMain_GetString", "UI_DialogBox", "BalanceLoad",
    "UniStringToString", "UIMain_ConvertMulti", "GameworkCtor", "GameworkDtor", "Run",
    "BaseHandleCommand", "GetFolder", "GetItem", "GetPath", "ItemDelete", "SetRate",
    "DialogCtor", "DialogDtor", "DialogFilter", "DialogShow", "SetParamText", "SetItemTitle",
    "ToGlobal", "SetPosition", "ToLocal", "SetProjectDir", "LbFileExists", "MacToWindowsVirtualKey",
    "TCacheFlush", "GetConstString", "c2pstr", "FSMakeFSSpec", "PBHGetVInfoSync", "OnlineConfigCtor",
]
ID = {n: i for i, n in enumerate(EVENTS)}
# Recorded fields per event: aN = int slot N, tN = text slot N (shim_TPMain.cpp TpmEvent).
SPEC = {n: [] for n in EVENTS}
SPEC.update({
    "GetLocalPath": ["a0"], "GetDataPath": ["a0"], "SetRemotePath": ["a0", "t0"],
    "FS_AddDirectorySeparator": ["t0"], "FS_ScanThemes": ["t0"], "InitialiseInstancing": ["a0"],
    "LoadGlobal": ["a0", "t0"], "PlayerSettingsInit": ["a0"], "OnlineCtor": ["a0"], "OnlineDtor": ["a0"],
    "OnlineTick": ["a0"], "UserInfoCtor": ["a0"], "UserInfoDtor": ["a0"], "GameLoginInit": ["a0"],
    "ReportAnyError": ["a0"], "TextUnInit": ["a0"], "TextInit": ["a0"], "TextSetRez": ["a0", "a1"],
    "InitResources": ["a0"], "GamePause": ["a0", "a1", "a2"], "UI_SetRealScreenRez": ["a0", "a1"],
    "KeyDown": ["a0", "a1", "a2"], "KeyUp": ["a0", "a1", "a2"], "UI_Keydown": ["a0", "a1"],
    "UI_Keyup": ["a0", "a1"], "UI_KeyChar": ["a0", "a1"], "UI_DevicePointerMove": ["a0", "a1"],
    "UI_DeviceButtonDown": ["a0", "a1", "a2"], "UI_DeviceButtonUp": ["a0", "a1", "a2"],
    "UI_DeviceMouseWheel": ["a0", "a1", "a2", "a3"], "UDebug": ["t0"], "UIMain_GetString": ["a0"],
    "UI_DialogBox": ["a0", "a1", "a2", "a3", "a4", "a5", "a6"], "BalanceLoad": ["a0", "a1", "t0"],
    "UniStringToString": ["t0"], "UIMain_ConvertMulti": ["t0"], "GameworkCtor": ["a0", "a1"],
    "GameworkDtor": ["a0"], "Run": ["a0", "a1"], "BaseHandleCommand": ["a0", "a1", "a2"],
    "GetFolder": ["a0"], "GetItem": ["a0", "a1", "a2", "t0"], "GetPath": ["a0"], "ItemDelete": ["a0"],
    "SetRate": ["a0", "a1", "a2", "a3"], "DialogCtor": ["a0", "a1", "a2", "t0"], "DialogDtor": ["a0"],
    "DialogFilter": ["a0", "a1", "a2"], "DialogShow": ["a0"], "SetParamText": ["a0", "t0", "t1", "t2", "t3"],
    "SetItemTitle": ["a0", "a1", "t0"], "ToGlobal": ["a0", "a1", "a2", "a3", "a4"],
    "SetPosition": ["a0", "a1"], "ToLocal": ["a0", "a1", "a2"], "SetProjectDir": ["t0"],
    "LbFileExists": ["t0"], "MacToWindowsVirtualKey": ["a0"], "TCacheFlush": ["a0", "a1"],
    "GetConstString": ["a0", "a1"], "c2pstr": ["t0"], "FSMakeFSSpec": ["a0", "a1", "t0"],
    "PBHGetVInfoSync": ["a0", "a1", "a2", "a3"],
})

# Fixed labels (shim_TPMain.cpp's tpw_tpm_objs): created per case or in setup.
LABELS = dict(world=1, gText=2, gGame=3, gTPFileInfo=4, gPlayerSettings=5, gExitError=6, gSoundBalance=7,
              gOnlineBalance=8, gResolutionBalance=9, keytable=10, tcache=11, window=12, app_folder=13,
              gThisProcess=14, timer=15, uistrings=16, event=17, old_userinfo=20, old_online=21, dialog=30)

A = dict(gThemeParkWorld=0x102b2bb0, gTPMain_102b2bac=0x102b2bac, gAppName=0x102b2ba8, gTPMain_102b2ba4=0x102b2ba4,
         gTPWFlags=0x102b2b4c, gNoSavedSettings=0x102b2b44, gMainWindow=0x102b2bb4, gScreenWidth=0x102b2b54,
         gScreenHeight=0x102b2b50, gGameSystemState=0x103bc190, gUserInfo=0x10380b34, gOnlineSystem=0x10380ab0,
         gKeyTable=0x1037f234, gUIRealScreenWidth=0x10395bbc, gUIRealScreenHeight=0x10395bba, gLobby=0x102e4888,
         gText=0x1037ff18, gGame=0x1037f134, gTPFileInfo=0x102b3458, gPlayerSettings=0x1038071c,
         gExitError=0x1037f1c0, gSoundBalance=0x102b4e2c, gOnlineBalance=0x102b4d5c, gResolutionBalance=0x103bc640,
         gTPGlobals=0x103b74c8, gOnlineConfig=0x102b2b70)
WORLD_VT, CDDIALOG_VT = 0x10278ac4, 0x10278a4c
CB_LOBBY_QUIT, CB_GAME_QUIT = 0x10268620, 0x10268648    # TVectors of the two quit callbacks
FAKE_VT = 0x7C0FE000                                    # vptr of fake strings (never called)
LANGUAGES = ["English", "French", "German", "Spanish", "Swedish", "Italian", "Dutch", "Polish", "Danish",
             "Japanese", "TraditionalChinese", "SimplifiedChinese", "Thai", "Korean", "Hebrew", "American"]

VP, I32, U32, I16, U8, I8 = C.c_void_p, C.c_int32, C.c_uint32, C.c_int16, C.c_uint8, C.c_int8


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f


def s16(v):
    return T.s16(v)


def s32(v):
    return T.s32(v)

# ======================================================================= PowerPC side


class PState:
    """Per-case state of the PowerPC fakes."""

    def __init__(self):
        self.log, self.labels, self.next = [], {}, 40
        self.ret = {}
        self.has_dev, self.dev_path, self.app_path = 0, "", ""
        self.dx = self.dy = 0
        self.vk_flags = 0
        self.dialog_item, self.show_finds = 0, 0
        self.vref = self.vatrb = 0
        self.scan = 0
        self.local_path = self.data_path = ""


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def w16(e, a, v):
    e.write(a, struct.pack(">H", v & 0xFFFF))


def r16(e, a):
    return struct.unpack(">h", e.read(a, 2))[0]


def new_text(e, text):
    b = text.encode("latin-1") if isinstance(text, str) else bytes(text)
    return e.alloc(len(b) + 1, b + b"\0", alignment=4)


def new_wide(e, text):
    b = b"".join(struct.pack(">H", ord(ch)) for ch in text) + b"\0\0"
    return e.alloc(len(b), b, alignment=4)


def wide_text(e, a):
    out = []
    while a:
        ch = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if ch == 0 or len(out) > 200:
            break
        out.append(chr(ch & 0x7F))
    return "".join(out)


def tds_text(e, obj):      # TbDynamicStringTemplate<char>: data at +8
    d = e.u32(obj + 8)
    return e.cstr(d) if d else ""


def tds_set(e, obj, text):
    w32(e, obj, FAKE_VT)
    w32(e, obj + 4, 0)
    w32(e, obj + 8, new_text(e, text))
    w32(e, obj + 0xC, len(text))
    w32(e, obj + 0x10, 0)


def ustr_text(e, obj):     # UString: length +4, text +8
    d = e.u32(obj + 8)
    return e.cstr(d) if d else ""


def ustr_set(e, obj, text, length=None):
    raw = text.encode("latin-1")[: length if length is not None else None]
    w32(e, obj, FAKE_VT)
    w32(e, obj + 4, len(raw))
    w32(e, obj + 8, new_text(e, raw))


def msl_string(e, obj):    # MSL std::string: rep {size, capacity, refs, data}
    rep = e.u32(obj)
    if rep == 0:
        return None
    size, data = e.u32(rep), e.u32(rep + 0xC)
    return e.read(data, size).decode("latin-1")


def install(emu):
    """Installs every PowerPC fake once; they read emu.tpm (a PState) per case."""
    T.install_std_stubs(emu)
    emu.tpm = PState()
    S = lambda: emu.tpm      # noqa: E731

    def rec(name, *vals):
        S().log.append((name,) + tuple(vals))

    def lab(a):
        if a == 0:
            return 0
        return S().labels.get(a, -1)

    def newlab(a):
        s = S()
        if s.next < 64:
            s.labels[a] = s.next
            s.next += 1
            return s.next - 1
        return -1

    def ret(name):
        return S().ret.get(name, 0)

    def internal(addr, fn):
        T.stub_internal(emu, addr, fn)

    # ---- bullfrog strings and UStrings (functional, unrecorded)
    def tds_ctor0(e):
        w32(e, e.r(3), FAKE_VT); w32(e, e.r(3) + 4, 0); w32(e, e.r(3) + 8, new_text(e, ""))
        w32(e, e.r(3) + 0xC, 0); w32(e, e.r(3) + 0x10, 0)

    def tds_ctor(e):
        tds_set(e, e.r(3), e.cstr(e.r(4)))

    def tds_assign(e):
        tds_set(e, e.r(3), tds_text(e, e.r(4)))

    def tds_append(e):
        tds_set(e, e.r(3), tds_text(e, e.r(3)) + tds_text(e, e.r(4)))

    def wds_ctor0(e):
        w32(e, e.r(3), FAKE_VT); w32(e, e.r(3) + 8, new_wide(e, ""))

    def wds_ctor(e):
        w32(e, e.r(3), FAKE_VT); w32(e, e.r(3) + 8, new_wide(e, wide_text(e, e.r(4))))

    def wds_assign(e):
        w32(e, e.r(3) + 8, new_wide(e, wide_text(e, e.r(4))))

    emu.stub("__ct__26TbDynamicStringTemplate<c>Fv", tds_ctor0)
    emu.stub("__ct__26TbDynamicStringTemplate<c>FPCc", tds_ctor)
    emu.stub("__dt__26TbDynamicStringTemplate<c>Fv", lambda e: None)
    emu.stub("__as__26TbDynamicStringTemplate<c>FRC26TbDynamicStringTemplate<c>", tds_assign)
    emu.stub("__apl__26TbDynamicStringTemplate<c>FRC15TbStringBase<c>", tds_append)
    emu.stub("__ct__26TbDynamicStringTemplate<w>Fv", wds_ctor0)
    emu.stub("__ct__26TbDynamicStringTemplate<w>FPCw", wds_ctor)
    emu.stub("__dt__26TbDynamicStringTemplate<w>Fv", lambda e: None)
    emu.stub("__as__26TbDynamicStringTemplate<w>FPCw", wds_assign)
    emu.stub("__ct__Q213SamsUtilities7UStringFv", lambda e: ustr_set(e, e.r(3), ""))
    emu.stub("__ct__Q213SamsUtilities7UStringFPCc", lambda e: ustr_set(e, e.r(3), e.cstr(e.r(4))))
    emu.stub("__ct__Q213SamsUtilities7UStringFPCcUl",
             lambda e: ustr_set(e, e.r(3), e.read(e.r(4), e.r(5)).decode("latin-1"), e.r(5)))
    emu.stub("__dt__Q213SamsUtilities7UStringFv", lambda e: None)

    def resource_id(e):
        ustr_set(e, e.r(3), ustr_text(e, e.r(4)))
        w32(e, e.r(3) + 0xC, e.r(5))

    emu.stub("__ct__Q213SamsUtilities11UResourceIdFRCQ213SamsUtilities7UStringl", resource_id)

    # ---- Sam's Utilities
    def gamework_ctor(e):
        rec("GameworkCtor", newlab(e.r(3)), s32(e.r(4)))

    emu.stub("__ct__Q213SamsUtilities9UGameworkFl", gamework_ctor)
    emu.stub("__dt__Q213SamsUtilities9UGameworkFv", lambda e: rec("GameworkDtor", lab(e.r(3))))
    emu.stub("Run__Q213SamsUtilities9UGameworkFv",
             lambda e: rec("Run", lab(e.r(3)), int(e.u32(e.r(3)) == WORLD_VT)))
    emu.stub("HandleCommand__Q213SamsUtilities9UGameworkFlUl",
             lambda e: rec("BaseHandleCommand", lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))

    def set_rate(e):
        p = e.r(4)
        rec("SetRate", lab(e.r(3)), s32(e.u32(p)), s32(e.u32(p + 4)), e.r(5) & 0xFF)

    emu.stub("SetRate__Q213SamsUtilities6UTimerFRCQ213SamsUtilities13UMicrosecondsUc", set_rate)

    # fake folder (label app_folder) and development-data item, with their vtables
    def fake_vtable(slots):
        vt = T.persist_alloc(emu, 0x60)
        for off, fn in slots.items():
            w32(emu, vt + off, T.fake_function(emu, fn))
        return vt

    def item_delete(e):
        rec("ItemDelete", lab(e.r(3)))
        if T.s16(e.r(4)) <= 0:
            rec("BADFLAG", T.s16(e.r(4)))

    def item_path(e):
        rec("GetPath", lab(e.r(3)))
        ustr_set(e, e.r(4), S().dev_path)

    def folder_path(e):
        rec("GetPath", lab(e.r(3)))
        ustr_set(e, e.r(4), S().app_path)

    item_vt = fake_vtable({0x08: item_delete, 0x28: item_path})

    def folder_item(e):
        s = S()
        name, typ = ustr_text(e, e.r(4)), s32(e.r(5))
        if not s.has_dev:
            rec("GetItem", lab(e.r(3)), typ, 0, name)
            e.set_r(3, 0)
            return
        item = e.alloc(16)
        w32(e, item, item_vt)
        rec("GetItem", lab(e.r(3)), typ, newlab(item), name)
        e.set_r(3, item)

    folder_vt = fake_vtable({0x08: lambda e: rec("BADFOLDERDELETE"), 0x28: folder_path, 0x48: folder_item})
    emu.app_folder = T.persist_alloc(emu, 16)
    w32(emu, emu.app_folder, folder_vt)

    def get_folder(e):
        rec("GetFolder", lab(e.r(3)))
        e.set_r(3, emu.app_folder)

    emu.stub("GetFolder__Q213SamsUtilities8UProcessFv", get_folder)

    def rect_ctor(e):
        r = e.r(4)
        top, left, bottom, right = (r16(e, r + i) for i in (0, 2, 4, 6))
        for off, v in ((4, left), (8, top), (0xC, right), (0x10, bottom)):
            w32(e, e.r(3) + off, v)

    def to_global(e):
        o = e.r(3)
        left, top, right, bottom = (s32(e.u32(o + off)) for off in (4, 8, 0xC, 0x10))
        rec("ToGlobal", lab(e.r(4)), left, top, right, bottom)
        for off, d in ((4, S().dx), (8, S().dy), (0xC, S().dx), (0x10, S().dy)):
            w32(e, o + off, s32(e.u32(o + off)) + d)

    def to_local(e):
        o = e.r(3)
        x, y = s32(e.u32(o)), s32(e.u32(o + 4))
        rec("ToLocal", lab(e.r(4)), x, y)
        w32(e, o, x - S().dx)
        w32(e, o + 4, y - S().dy)

    def get_position(e):      # hidden result buffer in r3, the event in r4
        ev = e.r(4)
        w32(e, e.r(3), r16(e, ev + 0xC))
        w32(e, e.r(3) + 4, r16(e, ev + 0xA))

    emu.stub("__ct__Q213SamsUtilities13URectangleMacFRC4Rect", rect_ctor)
    emu.stub("ToGlobal__Q213SamsUtilities13URectangleMacFP8GrafPort", to_global)
    emu.stub("ToLocal__Q213SamsUtilities9UPointMacFP8GrafPort", to_local)
    emu.stub("SetPosition__Q213SamsUtilities7UCursorFRCQ213SamsUtilities9UPointMac",
             lambda e: rec("SetPosition", s32(e.u32(e.r(3))), s32(e.u32(e.r(3) + 4))))
    emu.stub("GetPosition__Q213SamsUtilities6UEventCFv", get_position)
    emu.stub("GetKeyModifiers__Q213SamsUtilities6UEventCFv", lambda e: e.set_r(3, e.u32(e.r(3) + 0xC) & 0xFFFF))

    def dialog_ctor(e):
        o = e.r(3)
        e.write(o, b"\0" * 0x2C)
        rec("DialogCtor", newlab(o), s32(e.u32(e.r(4) + 0xC)), e.r(5) & 0xFF, ustr_text(e, e.r(4)))
        w32(e, o + 0x18, S().dialog_item)

    def dialog_filter(e):
        rec("DialogFilter", lab(e.r(3)), lab(e.r(4)), r16(e, e.r(5)))
        v = S().ret.get("DialogFilter", 0)
        w16(e, e.r(5), v >> 8)
        e.set_r(3, int((v & 0xFF) != 0))

    def dialog_show(e):
        rec("DialogShow", lab(e.r(3)))
        if S().show_finds:
            e.write(e.r(3) + 0x28, b"\x01")

    emu.stub("__ct__Q213SamsUtilities7UDialogFRCQ213SamsUtilities11UResourceIdUc", dialog_ctor)
    emu.stub("__dt__Q213SamsUtilities7UDialogFv", lambda e: rec("DialogDtor", lab(e.r(3))))
    emu.stub("Filter__Q213SamsUtilities7UDialogFRCQ213SamsUtilities6UEventRs", dialog_filter)
    emu.stub("Show__Q213SamsUtilities7UDialogFv", dialog_show)
    emu.stub("SetParamText__Q213SamsUtilities7UDialogFRCQ213SamsUtilities7UStringRCQ213SamsUtilities7UString"
             "RCQ213SamsUtilities7UStringRCQ213SamsUtilities7UString",
             lambda e: rec("SetParamText", lab(e.r(3)), *(ustr_text(e, e.r(i)) for i in (4, 5, 6, 7))))
    emu.stub("SetItemTitle__Q213SamsUtilities7UDialogFlRCQ213SamsUtilities7UString",
             lambda e: rec("SetItemTitle", lab(e.r(3)), s32(e.r(4)), ustr_text(e, e.r(5))))

    # data imports
    emu.empty_ustring = emu.data_import("kEmptyString__Q213SamsUtilities7UString")
    empty = T.persist_alloc(emu, 4)
    w32(emu, emu.empty_ustring, FAKE_VT)
    w32(emu, emu.empty_ustring + 4, 0)
    w32(emu, emu.empty_ustring + 8, empty)
    upper = bytes((c - 32) if 0x61 <= c <= 0x7A else c for c in range(256))
    emu.write(emu.data_import("__upper_map"), upper)

    # ---- toolbox, macdoze, bullfrog files, engine, ltms
    def c2pstr(e):
        text = e.cstr(e.r(3)).encode("latin-1")
        rec("c2pstr", text.decode("latin-1"))
        e.write(e.r(3), bytes([len(text) & 0xFF]) + text)

    def make_spec(e):
        p = e.r(5)
        name = e.read(p + 1, e.read(p, 1)[0]).decode("latin-1")
        rec("FSMakeFSSpec", s16(e.r(3)), s32(e.r(4)), name[:95])
        w16(e, e.r(6), S().vref)
        e.set_r(3, T.u32(T.s16(ret("FSMakeFSSpec"))))

    def get_vinfo(e):
        pb = e.r(3)
        rec("PBHGetVInfoSync", lab(e.u32(pb + 0xC)), lab(e.u32(pb + 0x12)), r16(e, pb + 0x16), r16(e, pb + 0x1C))
        w16(e, pb + 0x26, S().vatrb)
        e.set_r(3, T.u32(T.s16(ret("PBHGetVInfoSync"))))

    emu.stub("c2pstr", c2pstr)
    emu.stub("FSMakeFSSpec", make_spec)
    emu.stub("PBHGetVInfoSync", get_vinfo)

    def vk(e):
        rec("MacToWindowsVirtualKey", e.r(3) & 0xFF)
        w16(e, e.r(4), S().vk_flags)
        e.set_r(3, ret("MacToWindowsVirtualKey") & 0xFFFF)

    emu.stub("MacToWindowsVirtualKey__10NS_MacDozeFUcRUs", vk)
    emu.stub("LbFile_SetProjectDir__FPCc", lambda e: rec("SetProjectDir", e.cstr(e.r(3))[:95]))

    def exists(e):
        rec("LbFileExists", e.cstr(e.r(3))[:95])
        e.set_r(3, ret("LbFileExists") & 0xFF)

    emu.stub("LbFile_Exists__FPCc", exists)
    emu.stub("Flush__13CTextureCacheFi", lambda e: rec("TCacheFlush", lab(e.r(3)), s32(e.r(4))))

    def const_string(e):
        rec("GetConstString", lab(e.r(3)), s32(e.r(4)))
        e.set_r(3, emu.ui_string9)

    emu.stub("GetConstString__8TbITableCFUl", const_string)

    # ---- main app callees
    def tberror(name):
        def f(e):
            rec(name)
            w32(e, e.r(3), ret(name))
        return f

    internal(0x10003A2C, tberror("FS_ReadRegistry"))
    internal(0x10003B64, tberror("FS_ReadLanguageRegistry"))

    def path_getter(name, attr):
        def f(e):
            rec(name, lab(e.r(3)))
            obj = e.alloc(0x14)
            tds_set(e, obj, getattr(S(), attr))
            e.set_r(3, obj)
        return f

    internal(0x10004D4C, path_getter("GetLocalPath", "local_path"))
    internal(0x10004EC4, path_getter("GetDataPath", "data_path"))
    internal(0x10004D88, lambda e: rec("SetRemotePath", lab(e.r(3)), tds_text(e, e.r(4))[:95]))

    def add_sep(e):
        rec("FS_AddDirectorySeparator", tds_text(e, e.r(3))[:95])
        tds_set(e, e.r(3), tds_text(e, e.r(3)) + ":")

    internal(0x10004BC0, add_sep)

    def scan(e):
        rec("FS_ScanThemes", tds_text(e, e.r(4))[:95])
        w32(e, e.r(3), S().scan)

    internal(0x10169310, scan)
    for addr, name in ((0x100591A4, "InitialiseGameStage1"), (0x10059254, "InitialiseGameStage2"),
                       (0x100592A8, "InitialiseGameStage3"), (0x1005999C, "TPSaveRegistry"),
                       (0x10059670, "ClosedownGame"), (0x10064CB0, "InitialiseMemorySystem"),
                       (0x1005B5D8, "EngineCloseDown"), (0x10236548, "GAMESYSTEM_Close"),
                       (0x10233E78, "GAMESYSTEM_Init"), (0x100B9C48, "MOVIE_Stop"), (0x100DD548, "SOUND_Pause"),
                       (0x100DD5D8, "SOUND_UnPause"), (0x101CA5E0, "UI_Activate"), (0x101CA658, "UI_DeActivate")):
        internal(addr, (lambda n: lambda e: rec(n))(name))

    def returning(name):
        def f(e):
            rec(name)
            e.set_r(3, T.u32(ret(name)))
        return f

    internal(0x1005ADBC, returning("EngineStartup"))
    internal(0x102346D0, returning("GAMESYSTEM_Tick"))
    internal(0x10061AEC, lambda e: rec("InitialiseInstancing", s32(e.r(3))))

    def load_global(e):
        rec("LoadGlobal", lab(e.r(3)), tds_text(e, e.r(4))[:95])
        e.set_r(3, T.u32(ret("LoadGlobal")))

    internal(0x10167348, load_global)
    internal(0x10165B40, lambda e: rec("PlayerSettingsInit", lab(e.r(3))))

    def dtor(name):
        def f(e):
            rec(name, lab(e.r(3)))
            if T.s16(e.r(4)) != 1:
                rec("BADFLAG", T.s16(e.r(4)))
        return f

    internal(0x1016F7D0, lambda e: rec("OnlineCtor", newlab(e.r(3))))
    internal(0x1017055C, dtor("OnlineDtor"))
    internal(0x10170F80, lambda e: rec("OnlineTick", lab(e.r(3))))
    internal(0x1017DE54, lambda e: rec("UserInfoCtor", newlab(e.r(3))))
    internal(0x1017DFEC, dtor("UserInfoDtor"))
    internal(0x1017E0A8, lambda e: rec("GameLoginInit", lab(e.r(3))))
    internal(0x1014B6C8, lambda e: rec("ReportAnyError", lab(e.r(3))))
    internal(0x1015E524, lambda e: rec("TextUnInit", lab(e.r(3))))

    def text_init(e):
        rec("TextInit", lab(e.r(3)))
        e.set_r(3, T.u32(ret("TextInit")))

    internal(0x1015BE6C, text_init)
    internal(0x1015E4B8, lambda e: rec("TextSetRez", lab(e.r(3)), s32(e.r(4))))
    internal(0x10148730, lambda e: rec("InitResources", lab(e.r(3))))
    internal(0x10149904, lambda e: rec("GamePause", lab(e.r(3)), s32(e.r(4)), e.r(5) & 0xFF))
    internal(0x101C9ED8, lambda e: rec("UI_SetRealScreenRez", s16(e.r(3)), s16(e.r(4))))
    internal(0x1014FDEC, lambda e: rec("KeyDown", lab(e.r(3)), s16(e.r(4)), s16(e.r(5))))
    internal(0x1014FEF0, lambda e: rec("KeyUp", lab(e.r(3)), s16(e.r(4)), s16(e.r(5))))
    internal(0x101C929C, lambda e: rec("UI_Keydown", s16(e.r(3)), s32(e.r(4))))
    internal(0x101C9314, lambda e: rec("UI_Keyup", s16(e.r(3)), s32(e.r(4))))
    internal(0x101C9388, lambda e: rec("UI_KeyChar", s16(e.r(3)), s32(e.r(4))))
    internal(0x101C8D94, lambda e: rec("UI_DevicePointerMove", s16(e.r(3)), s16(e.r(4))))
    internal(0x101C9034, lambda e: rec("UI_DeviceButtonDown", s16(e.r(3)), s16(e.r(4)), s16(e.r(5))))
    internal(0x101C9100, lambda e: rec("UI_DeviceButtonUp", s16(e.r(3)), s16(e.r(4)), s16(e.r(5))))
    internal(0x101C91CC, lambda e: rec("UI_DeviceMouseWheel", s32(e.r(3)), s16(e.r(4)), s16(e.r(5)), s16(e.r(6))))
    internal(0x10000E40, lambda e: rec("UDebug", e.cstr(e.r(3))[:95]))

    def get_string(e):
        rec("UIMain_GetString", s32(e.r(3)))
        e.set_r(3, emu.quit_text)

    internal(0x10180468, get_string)

    def dialog_box(e):
        p = e.r(3)
        cb = lambda a: 0 if a == 0 else 1 if a == CB_LOBBY_QUIT else 2 if a == CB_GAME_QUIT else -1  # noqa: E731
        rgb = e.read(p + 4, 3)
        rec("UI_DialogBox", 1 if e.u32(p) == emu.quit_text else -1, rgb[0] | (rgb[1] << 8) | (rgb[2] << 16),
            s32(e.u32(p + 8)), s32(e.u32(p + 0xC)),
            (e.r(4) & 0xFF) | ((e.r(6) & 0xFF) << 8) | ((e.r(8) & 0xFF) << 16),
            cb(e.r(5)) | (cb(e.r(7)) << 8) | (cb(e.r(9)) << 16), e.r(10) & 0xFF)

    internal(0x101F1800, dialog_box)

    def balance_load(e):
        rec("BalanceLoad", lab(e.r(3)), s32(e.r(5)), e.cstr(e.r(4))[:95])
        which = lab(e.r(3)) - 7
        e.set_r(3, (ret("BalanceLoad") >> (8 * which)) & 0xFF if 0 <= which < 3 else T.u32(-1))

    internal(0x10020134, balance_load)

    def uni_to_string(e):
        text = wide_text(e, e.u32(e.r(4) + 8))
        rec("UniStringToString", text[:95])
        tds_set(e, e.r(3), "".join(chr(ord(c) & 0xFF) for c in text))

    internal(0x10005670, uni_to_string)

    def convert_multi(e):
        text = e.cstr(e.r(3))
        rec("UIMain_ConvertMulti", text[:95])
        e.set_r(3, new_wide(e, text[:95]))

    internal(0x10180334, convert_multi)


def ppc_begin(e, c):
    """Fresh fakes, labels and memory for a case; returns the world object."""
    s = e.tpm = PState()
    for k in ("has_dev", "dev_path", "app_path", "dx", "dy", "vk_flags", "dialog_item", "show_finds",
              "vref", "vatrb", "scan", "local_path", "data_path"):
        if k in c:
            setattr(s, k, c[k])
    s.ret = dict(c.get("ret", {}))
    e.news, e.deletes, e.alloc_log = [], [], []
    e.heap_in_scratch = True
    e.quit_text = new_wide(e, "Quit?")
    e.ui_string9 = new_text(e, c.get("ui9", "Please insert the CD"))
    world = e.alloc(0x80)
    w32(e, world, WORLD_VT)
    e.write(world + 4, bytes([c.get("running", 1) & 0xFF]))
    ustr_set(e, world + 0x20, c.get("name", "SimTheme Park"))
    w32(e, world + 0x70, c.get("value70", 0))
    fakes = {name: e.alloc(0x20) for name in ("keytable", "tcache", "uistrings", "old_userinfo", "old_online")}
    window = e.alloc(0x20)
    top, left, bottom, right = c.get("port", (0, 0, 480, 640))
    for off, v in ((0x10, top), (0x12, left), (0x14, bottom), (0x16, right)):
        w16(e, window + off, v)
    event = e.alloc(16)
    e.write(event + 0xA, struct.pack(">hhH", c.get("v", 0), c.get("h", 0), c.get("mods", 0)))
    s.labels = {world: 1, A["gText"]: 2, A["gGame"]: 3, A["gTPFileInfo"]: 4, A["gPlayerSettings"]: 5,
                A["gExitError"]: 6, A["gSoundBalance"]: 7, A["gOnlineBalance"]: 8, A["gResolutionBalance"]: 9,
                fakes["keytable"]: 10, fakes["tcache"]: 11, window: 12, e.app_folder: 13,
                e.data_import("gThisProcess__13SamsUtilities"): 14, world + 0x5C: 15, fakes["uistrings"]: 16,
                event: 17, fakes["old_userinfo"]: 20, fakes["old_online"]: 21}
    # globals
    g = c.get("g", {})
    w32(e, A["gTPWFlags"], g.get("flags", 0))
    e.write(A["gNoSavedSettings"], bytes([g.get("nosaved", 0)]))
    w32(e, A["gMainWindow"], window if g.get("window", 1) == 1 else (fakes["tcache"] if g.get("window") == 2 else 0))
    w32(e, A["gScreenWidth"], g.get("width", 640))
    w32(e, A["gScreenHeight"], g.get("height", 480))
    w32(e, A["gGameSystemState"], g.get("state", 0))
    w32(e, A["gUserInfo"], fakes["old_userinfo"] if g.get("old_userinfo") else 0)
    w32(e, A["gOnlineSystem"], fakes["old_online"] if g.get("old_online") else 0)
    w32(e, A["gKeyTable"], fakes["keytable"])
    w16(e, A["gUIRealScreenWidth"], g.get("ui_w", 2048))
    w16(e, A["gUIRealScreenHeight"], g.get("ui_h", 1536))
    w32(e, A["gLobby"], fakes["keytable"] if g.get("lobby") else 0)
    for name in ("gThemeParkWorld", "gTPMain_102b2bac", "gAppName", "gTPMain_102b2ba4"):
        w32(e, A[name], 0xDEADBEEF)
    w32(e, e.data_import("gei_TCache"), fakes["tcache"] if g.get("tcache") else 0)
    disp = e.data_import("gei_Display")
    w32(e, disp + 8, g.get("disp_w", 640))
    w32(e, disp + 0xC, g.get("disp_h", 480))
    w32(e, A["gText"] + 0x558, fakes["uistrings"])
    files = c.get("files", ("sound.sam", "online.sam", "Resolution.sam"))
    for off, text in zip((0x4114, 0x4128, 0x41F0), files):
        tds_set(e, A["gTPGlobals"] + off, text)
    ports, servers = c.get("online", ([7525, 7593, 7598, 7591, 7597, 7599], ["mail", "chat", "login", "city",
                                                                             "news", "vote", "complaints"]))
    for i, p in enumerate(ports):
        w32(e, A["gOnlineBalance"] + 0xC + 4 * i, p)
    for i, text in enumerate(servers):
        w32(e, A["gOnlineBalance"] + 0x24 + 4 * i, new_text(e, text))
    return world, window, event


def ppc_globals(e, world, window):
    s = e.tpm
    lab = lambda a: 0 if a == 0 else s.labels.get(a, -1)   # noqa: E731
    flags = e.u32(A["gTPWFlags"])
    app = e.u32(A["gAppName"])
    return dict(flags=flags, nosaved=e.read(A["gNoSavedSettings"], 1)[0],
                world_ptr=lab(e.u32(A["gThemeParkWorld"])), bac=e.u32(A["gTPMain_102b2bac"]),
                app=(e.cstr(app) if app not in (0, 0xDEADBEEF) else app), ba4=e.u32(A["gTPMain_102b2ba4"]),
                userinfo=lab(e.u32(A["gUserInfo"])), online=lab(e.u32(A["gOnlineSystem"])),
                running=e.read(world + 4, 1)[0], value70=s32(e.u32(world + 0x70)))


def ppc_online_config(e):
    base = A["gOnlineConfig"]
    strings = [msl_string(e, base + off) for off in (0, 4, 8, 0xC, 0x10, 0x14, 0x24)]
    ports = [r16(e, base + 0x18 + 2 * i) for i in range(6)]
    return dict(strings=strings, ports=ports)


def ppc_construct_online_config(e):
    """Runs the original OnlineConfig constructor on a zeroed object (MSL strings, real
    code); the host's reset leaves the same state (empty strings, zero ports)."""
    e.write(A["gOnlineConfig"], b"\0" * 0x28)
    T.call_ppc(e, 0x1016F4C0, [A["gOnlineConfig"]])

# ======================================================================= host side


class TpmEvent(C.Structure):
    _fields_ = [("id", C.c_int32), ("a", C.c_int32 * 8), ("text", (C.c_char * 96) * 4)]


def host_log(lib):
    n = C.c_int32.in_dll(lib, "tpw_tpm_n").value
    log = (TpmEvent * 256).in_dll(lib, "tpw_tpm_log")
    out = []
    for i in range(n):
        ev = log[i]
        name = EVENTS[ev.id]
        vals = []
        for f in SPEC[name]:
            k = int(f[1])
            vals.append(ev.a[k] if f[0] == "a" else ev.text[k].value.decode("latin-1"))
        out.append((name,) + tuple(vals))
    return out


def set_cstr(lib, sym, text):
    buf = (C.c_char * 96).in_dll(lib, sym)
    b = text.encode("latin-1")[:95]
    C.memmove(buf, b + b"\0", len(b) + 1)


def host_begin(lib, c):
    H = lib
    F(H, "tpw_tpm_reset", None)()
    F(H, "tpw_alloc_reset", None)()      # the allocation log holds 1024 entries
    C.c_int32.in_dll(H, "tpw_tpm_on").value = 0
    rets = (C.c_int32 * 128).in_dll(H, "tpw_tpm_ret")
    for i in range(128):
        rets[i] = 0
    for name, v in c.get("ret", {}).items():
        rets[ID[name]] = s32(v)
    for sym, key, default in (("tpw_tpm_has_dev", "has_dev", 0), ("tpw_tpm_dx", "dx", 0), ("tpw_tpm_dy", "dy", 0),
                              ("tpw_tpm_vk_flags", "vk_flags", 0), ("tpw_tpm_dialog_item", "dialog_item", 0),
                              ("tpw_tpm_show_finds", "show_finds", 0), ("tpw_tpm_scan_result", "scan", 0)):
        C.c_int32.in_dll(H, sym).value = s32(c.get(key, default))
    C.c_int16.in_dll(H, "tpw_tpm_vref").value = s16(c.get("vref", 0))
    C.c_int16.in_dll(H, "tpw_tpm_vatrb").value = s16(c.get("vatrb", 0))
    for sym, key in (("tpw_tpm_dev_path", "dev_path"), ("tpw_tpm_app_path", "app_path"),
                     ("tpw_tpm_local_path", "local_path"), ("tpw_tpm_data_path", "data_path")):
        set_cstr(H, sym, c.get(key, ""))
    ui9 = (C.c_ubyte * 96).in_dll(H, "tpw_tpm_ui_string9")
    b = c.get("ui9", "Please insert the CD").encode("latin-1")[:95] + b"\0"
    C.memmove(ui9, b, len(b))
    world = F(H, "tpw_tpm_world_new", VP)()
    F(H, "tpw_tpm_world_set", None, VP, I32, I32, C.c_char_p)(world, c.get("running", 1), c.get("value70", 0),
                                                               c.get("name", "SimTheme Park").encode("latin-1"))
    keep = [C.create_string_buffer(0x20) for _ in range(3)]
    keytable, tcache, uistrings = (C.addressof(k) for k in keep)
    window = C.create_string_buffer(24)
    top, left, bottom, right = c.get("port", (0, 0, 480, 640))
    struct.pack_into("<hhhh", window, 0x10, top, left, bottom, right)
    event = C.create_string_buffer(16)
    struct.pack_into("<hhH", event, 0xA, c.get("v", 0), c.get("h", 0), c.get("mods", 0))
    addr = F(H, "tpw_tpm_addr", VP, C.c_char_p)
    a = lambda n: addr(n.encode())    # noqa: E731
    g = c.get("g", {})
    old_userinfo = F(H, "tpw_tpm_new_userinfo", VP)()
    old_online = F(H, "tpw_tpm_new_online", VP)()
    objs = (VP * 64).in_dll(H, "tpw_tpm_objs")
    for i in range(64):
        objs[i] = None
    table = {1: world, 2: a("gText"), 3: a("gGame"), 4: a("gTPFileInfo"), 5: a("gPlayerSettings"),
             6: a("gExitError"), 7: a("gSoundBalance"), 8: a("gOnlineBalance"), 9: a("gResolutionBalance"),
             10: keytable, 11: tcache, 12: C.addressof(window), 13: a("app_folder"), 14: a("gThisProcess"),
             15: F(H, "tpw_tpm_world_timer", VP, VP)(world), 16: uistrings, 17: C.addressof(event),
             20: old_userinfo, 21: old_online}
    for k, v in table.items():
        objs[k] = v
    C.c_uint32.from_address(a("gTPWFlags")).value = g.get("flags", 0)
    C.c_uint8.from_address(a("gNoSavedSettings")).value = g.get("nosaved", 0)
    C.c_void_p.from_address(a("gMainWindow")).value = (C.addressof(window) if g.get("window", 1) == 1 else
                                                       (tcache if g.get("window") == 2 else None))
    C.c_int32.from_address(a("gScreenWidth")).value = s32(g.get("width", 640))
    C.c_int32.from_address(a("gScreenHeight")).value = s32(g.get("height", 480))
    C.c_int32.from_address(a("gGameSystemState")).value = s32(g.get("state", 0))
    C.c_void_p.from_address(a("gUserInfo")).value = old_userinfo if g.get("old_userinfo") else None
    C.c_void_p.from_address(a("gOnlineSystem")).value = old_online if g.get("old_online") else None
    C.c_void_p.from_address(a("gKeyTable")).value = keytable
    C.c_int16.from_address(a("gUIRealScreenWidth")).value = s16(g.get("ui_w", 2048))
    C.c_int16.from_address(a("gUIRealScreenHeight")).value = s16(g.get("ui_h", 1536))
    C.c_void_p.from_address(a("gLobby")).value = keytable if g.get("lobby") else None
    C.c_void_p.from_address(a("gThemeParkWorld")).value = 0xDEADBEEF
    for n in ("gTPMain_102b2bac", "gTPMain_102b2ba4"):
        C.c_int32.from_address(a(n)).value = s32(0xDEADBEEF)
    C.c_void_p.from_address(a("gAppName")).value = 0xDEADBEEF
    C.c_void_p.from_address(a("gei_TCache")).value = tcache if g.get("tcache") else None
    F(H, "tpw_tpm_set_display", None, I32, I32)(g.get("disp_w", 640), g.get("disp_h", 480))
    F(H, "tpw_tpm_set_uistrings", None, VP)(uistrings)
    files = c.get("files", ("sound.sam", "online.sam", "Resolution.sam"))
    F(H, "tpw_tpm_set_globals_files", None, C.c_char_p, C.c_char_p, C.c_char_p)(*(f.encode() for f in files))
    ports, servers = c.get("online", ([7525, 7593, 7598, 7591, 7597, 7599], ["mail", "chat", "login", "city",
                                                                             "news", "vote", "complaints"]))
    keep_servers = [C.create_string_buffer(s.encode("latin-1")) for s in servers]
    arr = (C.c_char_p * 7)(*(C.cast(k, C.c_char_p) for k in keep_servers))
    F(H, "tpw_tpm_set_online_balance", None, VP, VP)((C.c_int32 * 6)(*[s32(p) for p in ports]), arr)
    F(H, "tpw_tpm_reset_online_config", None)()
    C.c_int32.in_dll(H, "tpw_tpm_on").value = 1
    return dict(world=world, window=window, event=event, keep=keep + keep_servers + [arr], objs=table)


def host_end(lib):
    C.c_int32.in_dll(lib, "tpw_tpm_on").value = 0


def host_globals(lib, st):
    addr = F(lib, "tpw_tpm_addr", VP, C.c_char_p)
    a = lambda n: addr(n.encode())    # noqa: E731
    objs = (VP * 64).in_dll(lib, "tpw_tpm_objs")

    def lab(p):
        if not p:
            return 0
        for i in range(1, 64):
            if objs[i] == p:
                return i
        return -1

    app = C.c_void_p.from_address(a("gAppName")).value
    out = (C.c_int32 * 2)()
    F(lib, "tpw_tpm_world_get", None, VP, VP)(st["world"], out)
    return dict(flags=C.c_uint32.from_address(a("gTPWFlags")).value,
                nosaved=C.c_uint8.from_address(a("gNoSavedSettings")).value,
                world_ptr=lab(C.c_void_p.from_address(a("gThemeParkWorld")).value),
                bac=C.c_uint32.from_address(a("gTPMain_102b2bac")).value,
                app=(C.string_at(app).decode("latin-1") if app not in (None, 0xDEADBEEF) else (app or 0)),
                ba4=C.c_uint32.from_address(a("gTPMain_102b2ba4")).value,
                userinfo=lab(C.c_void_p.from_address(a("gUserInfo")).value),
                online=lab(C.c_void_p.from_address(a("gOnlineSystem")).value),
                running=out[0], value70=out[1])


def host_online_config(lib):
    strings = (C.c_char * (96 * 7))()
    ports = (C.c_int32 * 6)()
    F(lib, "tpw_tpm_get_online_config", None, VP, VP)(strings, ports)
    return dict(strings=[strings.raw[96 * i:96 * (i + 1)].split(b"\0")[0].decode("latin-1") for i in range(7)],
                ports=list(ports))

# ======================================================================= tests


def setup(runner):
    install(runner.emu)
    # the host's pre-existing "old" objects need real allocations (Initialise deletes them)
    lib = runner.lib


def call(e, addr, args):
    return T.call_ppc(e, addr, args)


def run_ppc(fn):
    def run(e, c):
        world, window, event = ppc_begin(e, c)
        result = fn(e, c, world, window, event)
        out = dict(log=e.tpm.log, result=result)
        out.update(ppc_globals(e, world, window))
        return out
    return run


def run_host(fn):
    def run(lib, c):
        st = host_begin(lib, c)
        result = fn(lib, c, st)
        host_end(lib)
        out = dict(log=host_log(lib), result=result)
        out.update(host_globals(lib, st))
        F(lib, "tpw_tpm_world_delete", None, VP)(st["world"])
        return out
    return run


def rnd_path(r):
    return r.choice(["", "Macintosh HD:Games:STP:", "HD:TPW:", "x" * r.randint(1, 60), "A:" * r.randint(1, 10)])


# ---- main and the destructors
def main_ppc(e, c, world, window, event):
    return T.u32(call(e, 0x10000208, [])[0])


def main_host(lib, c, st):
    return F(lib, "tpw_tpm_main", I32)()


def world_dtor_ppc(e, c, world, window, event):
    r3 = call(e, 0x1000028C, [world, T.S16(c["flag"])])[0]
    return dict(this=r3 == world, freed=world in e.deletes)


def world_dtor_host(lib, c, st):
    n0 = C.c_int32.in_dll(lib, "tpw_alloc_n").value
    C.c_int32.in_dll(lib, "tpw_alloc_log_on").value = 1
    C.c_int32.in_dll(lib, "tpw_tpm_on").value = 1
    F(lib, "tpw_tpm_World_destroy", None, I32)(int(T.s16(c["flag"]) > 0))
    C.c_int32.in_dll(lib, "tpw_alloc_log_on").value = 0
    kinds = (C.c_int32 * 1024).in_dll(lib, "tpw_alloc_kind")
    freed = any(kinds[i] == 3 for i in range(n0, C.c_int32.in_dll(lib, "tpw_alloc_n").value))
    return dict(this=True, freed=freed)


def world_dtor_globals_strip(d):
    return d


# ---- Initialise, Shutdown, Process
def init_case(r):
    return dict(has_dev=r.random() < 0.5, dev_path=rnd_path(r), app_path=rnd_path(r), local_path=rnd_path(r),
                data_path=rnd_path(r), name=r.choice(["SimTheme Park", "", "x" * 40]),
                value70=T.rint(r), scan=T.rint(r),
                ret=dict(FS_ReadRegistry=T.rint(r), FS_ReadLanguageRegistry=T.rint(r),
                         LoadGlobal=r.choice([0, 1, -1, T.rint(r)]),
                         BalanceLoad=r.choice([0, 1, 0x0101, 0x010101, 0x0001, r.getrandbits(24)]),
                         LbFileExists=r.choice([0, 1, 2, 0xFF])),
                g=dict(flags=r.getrandbits(32), nosaved=r.choice([0, 1, 7]), old_userinfo=r.random() < 0.5,
                       old_online=r.random() < 0.5),
                files=(r.choice(["sound.sam", "S"]), r.choice(["online.sam", ""]), r.choice(["Resolution.sam", "R"])),
                online=([T.rint(r) for _ in range(6)], [rnd_path(r) for _ in range(7)]))


def init_edges():
    out, r = [], random.Random(5)
    for has_dev in (0, 1):
        for load_global in (0, 1, -1):
            for balance in (0, 1, 0x0101, 0x010101):
                for exists in (0, 1):
                    for old in (0, 1):
                        c = init_case(r)
                        c.update(has_dev=has_dev)
                        c["ret"].update(LoadGlobal=load_global, BalanceLoad=balance, LbFileExists=exists)
                        c["g"].update(old_userinfo=old, old_online=old, nosaved=0)
                        out.append(c)
    return out


def initialise_ppc(e, c, world, window, event):
    ppc_construct_online_config(e)
    e.news = []
    r = T.u32(call(e, 0x10000320, [world])[0])
    return dict(r=r & 0xFF, online=ppc_online_config(e), news=[n for n, _ in e.news if n in (0xD4, 0x404)])


def initialise_host(lib, c, st):
    C.c_int32.in_dll(lib, "tpw_alloc_log_on").value = 1
    n0 = C.c_int32.in_dll(lib, "tpw_alloc_n").value
    r = F(lib, "tpw_tpm_Initialise", I32, VP)(st["world"])
    C.c_int32.in_dll(lib, "tpw_alloc_log_on").value = 0
    kinds = (C.c_int32 * 1024).in_dll(lib, "tpw_alloc_kind")
    sizes = (C.c_uint64 * 1024).in_dll(lib, "tpw_alloc_size")
    # the objects by their host sizes, reported as the Mac's
    mac_size = {F(lib, "tpw_tpm_sizeof_userinfo", C.c_uint64)(): 0xD4, F(lib, "tpw_tpm_sizeof_online", C.c_uint64)(): 0x404}
    news = [mac_size[sizes[i]] for i in range(n0, C.c_int32.in_dll(lib, "tpw_alloc_n").value)
            if kinds[i] == 1 and sizes[i] in mac_size]
    return dict(r=r & 0xFF, online=host_online_config(lib), news=news)


def shutdown_ppc(e, c, world, window, event):
    call(e, 0x10000604, [world])
    return None


def shutdown_host(lib, c, st):
    F(lib, "tpw_tpm_Shutdown", None, VP)(st["world"])
    return None


def process_case(r):
    return dict(has_dev=0, vref=r.choice([0, -1, 3, T.rint(r)]), vatrb=r.choice([0, 0x80, 0x8000, 0x8080, 1, 0x7F7F, T.rint(r)]),
                show_finds=r.random() < 0.5, dialog_item=T.rint(r), dx=r.randint(-100, 100), dy=r.randint(-100, 100),
                port=tuple(r.randint(-500, 1500) for _ in range(4)), ui9=r.choice(["CD!", "", "Insert the disc"]),
                ret=dict(TextInit=r.choice([0, 1, -1]), FSMakeFSSpec=r.choice([0, 0, -43, 1]),
                         PBHGetVInfoSync=r.choice([0, 0, -35]), EngineStartup=r.choice([0, 1, 1, -1]),
                         GAMESYSTEM_Tick=r.choice([0, 2, 1, 4, 8, r.getrandbits(32)])),
                g=dict(flags=r.choice([0, 1, 3, 5, 7, 0xF, 0x11, r.getrandbits(32)]), ui_w=r.choice([0, 1, 2048, -5]),
                       window=r.choice([0, 1, 1]), old_online=r.random() < 0.5,
                       width=r.choice([640, 800, 1024, T.rint(r)]), height=r.choice([480, 600, 768, T.rint(r)])))


def process_edges():
    out, r = [], random.Random(6)
    for flags in (0, 1, 2, 3, 5, 7):
        for text in (0, 1):
            for spec in (0, -43):
                for vatrb in (0, 0x80, 0x8000):
                    for engine in (0, 1):
                        c = process_case(r)
                        c["g"]["flags"] = flags
                        c["ret"].update(TextInit=text, FSMakeFSSpec=spec, PBHGetVInfoSync=0, EngineStartup=engine)
                        c["vatrb"] = vatrb
                        out.append(c)
    return out


def process_ppc(e, c, world, window, event):
    return T.u32(call(e, 0x10000680, [world])[0]) & 0xFF


def process_host(lib, c, st):
    return F(lib, "tpw_tpm_Process", I32, VP)(st["world"]) & 0xFF


# ---- input
def typing_case(r):
    vk = r.choice([0x20, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x1F, 0x21, 0x69, 0x80, 0xFF, 0, r.getrandbits(16)])
    return dict(ch=r.choice([-1, 0, 0x61, 0x7A, 0x41, -128, 127, r.randint(-128, 127)]), key=r.randint(-128, 127),
                kmods=r.choice([0, 0x200, 0x100, 0x1000, 0x800, 0x1100, 0x1B00, r.getrandbits(32)]),
                down=r.choice([0, 1, 2, 0x80]), vk_flags=r.choice([0, 0x100, 0xFFFF, r.getrandbits(16)]),
                ret=dict(MacToWindowsVirtualKey=vk))


def typing_edges():
    out, r = [], random.Random(7)
    for vk in (0, 0x1F, 0x20, 0x21, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6F, 0x70, 0x80, 0xFF, 0x120):
        for down in (0, 1):
            for vk_flags in (0, 0x100):
                for ch in (-1, 0x61, -128):
                    c = typing_case(r)
                    c.update(down=down, vk_flags=vk_flags, ch=ch, ret=dict(MacToWindowsVirtualKey=vk))
                    out.append(c)
    return out


def typing_ppc(e, c, world, window, event):
    call(e, 0x10000940, [world, T.S8(c["ch"]), T.S8(c["key"]), T.I(c["kmods"]), T.U8(c["down"])])
    return None


def typing_host(lib, c, st):
    F(lib, "tpw_tpm_HandleTyping", None, VP, I8, I8, U32, U8)(st["world"], c["ch"], c["key"], T.u32(c["kmods"]), c["down"] & 0xFF)
    return None


def mouse_case(r):
    return dict(h=r.randint(-3000, 3000), v=r.randint(-3000, 3000), mods=r.getrandbits(16), dx=r.randint(-200, 200),
                dy=r.randint(-200, 200), button=r.choice([0, 1, 2, 0, 1, 2, 3, -1]), delta=T.rint(r),
                g=dict(disp_w=r.choice([640, 800, 1024, 2, 3, 4, r.randint(-100, 5000)]),
                       disp_h=r.choice([480, 600, 768, 2, 4, r.randint(-100, 5000)]),
                       ui_w=r.choice([2048, 640, 1, -1, r.randint(1, 30000)]),
                       ui_h=r.choice([1536, 480, 1, -7, r.randint(1, 30000)])))


def mouse_edges():
    out, r = [], random.Random(8)
    for h in (-1, 0, 1, 635, 636, 637, 5000):
        for v in (-1, 0, 475, 476, 477):
            c = mouse_case(r)
            c.update(h=h, v=v, dx=0, dy=0, button=0)
            c["g"].update(disp_w=640, disp_h=480, ui_w=640, ui_h=480)
            out.append(c)
    return out


def pos_ppc(e, c, world, window, event):
    xy = e.alloc(4)
    call(e, 0x10000B54, [world, event, xy, xy + 2])
    return (r16(e, xy), r16(e, xy + 2))


def pos_host(lib, c, st):
    x, y = C.c_int16(), C.c_int16()
    F(lib, "tpw_tpm_GetUIMousePos", None, VP, VP, VP, VP)(st["world"], C.addressof(st["event"]), C.byref(x), C.byref(y))
    return (x.value, y.value)


def mouse_fn(addr, host, extra):
    def ppc(e, c, world, window, event):
        args = [world, event] + ([T.I(c[extra])] if extra else [])
        call(e, addr, args)
        return None

    def hst(lib, c, st):
        f = F(lib, host, None, VP, VP, *([I32] if extra else []))
        f(st["world"], C.addressof(st["event"]), *([s32(c[extra])] if extra else []))
        return None
    return ppc, hst


def strip_button(d, c):
    # A button other than 0..2: the original passes its caller's r31 (unreproducible).
    if c.get("button") not in (0, 1, 2):
        d["log"] = [ev[:1] + ev[2:] if ev[0] in ("UI_DeviceButtonDown", "UI_DeviceButtonUp") else ev for ev in d["log"]]
    return d


def activate_case(r):
    return dict(which=r.choice([0, 1, 2]), active=r.choice([0, 1, 2, 0x100]),
                g=dict(flags=r.getrandbits(32), tcache=r.random() < 0.5, lobby=r.random() < 0.5, window=1))


def activate_ppc(e, c, world, window, event):
    target = window if c["which"] == 1 else (0 if c["which"] == 0 else event)
    call(e, 0x1000104C, [world, target, T.U8(c["active"])])
    return None


def activate_host(lib, c, st):
    target = C.addressof(st["window"]) if c["which"] == 1 else (None if c["which"] == 0 else C.addressof(st["event"]))
    F(lib, "tpw_tpm_HandleActivateWindow", None, VP, VP, U8)(st["world"], target, c["active"] & 0xFF)
    return None


def command_case(r):
    return dict(cmd=r.choice([3, 3, 3, 0, 1, 2, 4, T.rint(r)]), cmods=r.getrandbits(32), running=r.choice([0, 1]),
                g=dict(state=r.choice([0, 1, 2, 9, 10, 11, 12, -1, T.rint(r)])))


def command_ppc(e, c, world, window, event):
    call(e, 0x10001170, [world, T.I(c["cmd"]), T.I(c["cmods"])])
    return None


def command_host(lib, c, st):
    F(lib, "tpw_tpm_HandleCommand", None, VP, I32, U32)(st["world"], s32(c["cmd"]), T.u32(c["cmods"]))
    return None


# ---- balance files, online values
def balance_ppc(e, c, world, window, event):
    ppc_construct_online_config(e)
    call(e, 0x1000129C, [world])
    return ppc_online_config(e)


def balance_host(lib, c, st):
    F(lib, "tpw_tpm_LoadSystemBalanceFiles", None, VP)(st["world"])
    return host_online_config(lib)


def online_ppc(e, c, world, window, event):
    ppc_construct_online_config(e)
    call(e, 0x10001494, [])
    return ppc_online_config(e)


def online_host(lib, c, st):
    F(lib, "tpw_tpm_SetInitialOnlineValues", None)()
    return host_online_config(lib)


# ---- CD check
def cd_case(r):
    return dict(vref=r.choice([0, -1, 1, T.rint(r)]), vatrb=r.choice([0, 0x80, 0x8000, 0x8080, 0x7F7F, 1, T.rint(r)]),
                show_finds=r.random() < 0.5, dialog_item=T.rint(r), ui9=r.choice(["CD!", "", "Insert the disc"]),
                item=r.choice([0, 1, -1, T.rint(r)]),
                ret=dict(FSMakeFSSpec=r.choice([0, 0, -43, 1, -1]), PBHGetVInfoSync=r.choice([0, 0, -35, 1]),
                         DialogFilter=r.choice([0, 1, 0x7F01, 0x8000, r.getrandbits(24)])))


def cd_edges():
    out, r = [], random.Random(9)
    for spec in (0, -43):
        for vinfo in (0, -35):
            for vatrb in (0, 0x80, 0x8000, 0x8080, 0x7F7F):
                for finds in (0, 1):
                    c = cd_case(r)
                    c.update(vatrb=vatrb, show_finds=finds)
                    c["ret"].update(FSMakeFSSpec=spec, PBHGetVInfoSync=vinfo)
                    out.append(c)
    return out


def lookforcd_ppc(e, c, world, window, event):
    return T.u32(call(e, 0x10001B6C, [])[0]) & 0xFF


def lookforcd_host(lib, c, st):
    return F(lib, "tpw_tpm_LookForCd", I32)() & 0xFF


def iscd_ppc(e, c, world, window, event):
    return T.u32(call(e, 0x100017B8, [world])[0]) & 0xFF


def iscd_host(lib, c, st):
    return F(lib, "tpw_tpm_IsCdPresent", I32, VP)(st["world"]) & 0xFF


def filter_ppc(e, c, world, window, event):
    dialog = e.alloc(0x2C)
    w32(e, dialog, CDDIALOG_VT)
    w32(e, dialog + 0x18, c["dialog_item"])
    e.tpm.labels[dialog] = 30
    item = e.alloc(4)
    w16(e, item, c["item"])
    r = T.u32(call(e, 0x100016E4, [dialog, event, item])[0]) & 0xFF
    return dict(r=r, item=r16(e, item), found=e.read(dialog + 0x28, 1)[0])


def filter_host(lib, c, st):
    item = C.c_int16(s16(c["item"]))
    found = C.c_int32()
    r = F(lib, "tpw_tpm_Filter", I32, VP, VP, I32, VP)(C.addressof(st["event"]), C.byref(item), s32(c["dialog_item"]),
                                                      C.byref(found))
    return dict(r=r & 0xFF, item=item.value, found=found.value)


def cddtor_ppc(e, c, world, window, event):
    dialog = e.alloc(0x2C)
    w32(e, dialog, CDDIALOG_VT)
    e.tpm.labels[dialog] = 30
    r3 = call(e, 0x10001A30, [dialog, T.S16(c["flag"])])[0]
    return dict(this=r3 == dialog, freed=dialog in e.deletes)


def cddtor_host(lib, c, st):
    n0 = C.c_int32.in_dll(lib, "tpw_alloc_n").value
    C.c_int32.in_dll(lib, "tpw_alloc_log_on").value = 1
    F(lib, "tpw_tpm_CdDialog_destroy", None, I32)(int(T.s16(c["flag"]) > 0))
    C.c_int32.in_dll(lib, "tpw_alloc_log_on").value = 0
    kinds = (C.c_int32 * 1024).in_dll(lib, "tpw_alloc_kind")
    freed = any(kinds[i] == 3 for i in range(n0, C.c_int32.in_dll(lib, "tpw_alloc_n").value))
    return dict(this=True, freed=freed)


# ---- the static initialiser
def sinit_ppc(e, c, world, window, event):
    e.registered = []
    call(e, 0x10003220, [])
    texts = [tds_text(e, 0x102B2C18 + 0x14 * i) for i in range(16)]
    texts += [tds_text(e, 0x102B2BF8), tds_text(e, 0x102B2BD8), wide_text(e, e.u32(0x102B2BB8 + 8))]
    regs = [(obj, dt) for obj, dt, _ in e.registered]
    tds_dtor = e.u32(e.toc - 0x7C1C)      # the TVector of TbDynamicStringTemplate<char>'s destructor
    expect = [(0x102B2C18 + 0x14 * i, tds_dtor) for i in range(16)] + [(0x102B2BF8, tds_dtor), (0x102B2BD8, tds_dtor)]
    ok = regs[:18] == expect and regs[18][0] == 0x102B2BB8 and regs[19] == (A["gOnlineConfig"], 0x10265668)
    return dict(texts=texts, registrations_ok=ok and len(regs) == 20)


def sinit_host(lib, c, st):
    out = (C.c_char * (96 * 19))()
    F(lib, "tpw_tpm_sinit_texts", None, VP)(out)
    return dict(texts=[out.raw[96 * i:96 * (i + 1)].split(b"\0")[0].decode("latin-1") for i in range(19)],
                registrations_ok=True)


def simple_test(name, ppc, host, edge=None, rnd=None, post=None):
    p, h = run_ppc(ppc), run_host(host)
    if post:
        p0, h0 = p, h
        p = lambda e, c: post(p0(e, c), c)        # noqa: E731
        h = lambda lib, c: post(h0(lib, c), c)    # noqa: E731
    return FnTest(name, p, h, edge=edge or (lambda: []), rnd=rnd or (lambda r: {}))


def onlinecase(r):
    return dict(online=([r.choice([0, 1, -1, 0x7FFF, 0x8000, 0x12345678, T.rint(r)]) for _ in range(6)],
                        [rnd_path(r) for _ in range(7)]))


def ctor_ppc(e, c, world, window, event):
    ppc_construct_online_config(e)
    return ppc_online_config(e)


def ctor_host(lib, c, st):
    F(lib, "tpw_tpm_reset_online_config", None)()
    return host_online_config(lib)


TESTS = [
    simple_test("OnlineConfig::OnlineConfig", ctor_ppc, ctor_host, edge=lambda: [{}]),
    simple_test("main", main_ppc, main_host, edge=lambda: [{}]),
    simple_test("ThemeParkWorld::~ThemeParkWorld(flag)", world_dtor_ppc, world_dtor_host,
                edge=lambda: [dict(flag=f) for f in (0, 1, -1, 2, 0x7FFF, -0x8000)], rnd=lambda r: dict(flag=r.randint(-3, 3))),
    simple_test("ThemeParkWorld::Initialise", initialise_ppc, initialise_host, edge=init_edges, rnd=init_case),
    simple_test("ThemeParkWorld::Shutdown", shutdown_ppc, shutdown_host, edge=lambda: [{}]),
    simple_test("ThemeParkWorld::Process", process_ppc, process_host, edge=process_edges, rnd=process_case),
    simple_test("ThemeParkWorld::HandleUpdateWindow",
                lambda e, c, w, win, ev: call(e, 0x100008F8, [w, win]) and None,
                lambda lib, c, st: F(lib, "tpw_tpm_HandleUpdateWindow", None, VP, VP)(st["world"], C.addressof(st["window"])),
                edge=lambda: [{}]),
    simple_test("ThemeParkWorld::HandleTyping", typing_ppc, typing_host, edge=typing_edges, rnd=typing_case),
    simple_test("ThemeParkWorld::GetUIMousePos", pos_ppc, pos_host, edge=mouse_edges, rnd=mouse_case),
    simple_test("ThemeParkWorld::HandleMouseMoved", *mouse_fn(0x10000CB0, "tpw_tpm_HandleMouseMoved", None),
                edge=mouse_edges, rnd=mouse_case),
    simple_test("ThemeParkWorld::HandleMouseDown", *mouse_fn(0x10000D38, "tpw_tpm_HandleMouseDown", "button"),
                edge=mouse_edges, rnd=mouse_case, post=strip_button),
    simple_test("ThemeParkWorld::HandleMouseUp", *mouse_fn(0x10000E90, "tpw_tpm_HandleMouseUp", "button"),
                edge=mouse_edges, rnd=mouse_case, post=strip_button),
    simple_test("ThemeParkWorld::HandleMouseWheel", *mouse_fn(0x10000F94, "tpw_tpm_HandleMouseWheel", "delta"),
                edge=mouse_edges, rnd=mouse_case),
    simple_test("ThemeParkWorld::HandleActivateWindow", activate_ppc, activate_host, rnd=activate_case),
    simple_test("ThemeParkWorld::HandleCommand", command_ppc, command_host, rnd=command_case),
    simple_test("ThemeParkWorld::LoadSystemBalanceFiles", balance_ppc, balance_host, edge=init_edges, rnd=init_case),
    simple_test("SetInitialOnlineValues", online_ppc, online_host, rnd=onlinecase),
    simple_test("ThemeParkWorld::LookForCd", lookforcd_ppc, lookforcd_host, edge=cd_edges, rnd=cd_case),
    simple_test("ThemeParkWorld::IsCdPresent", iscd_ppc, iscd_host, edge=cd_edges, rnd=cd_case),
    simple_test("ThemeParkWorld::CdDialog::Filter", filter_ppc, filter_host, edge=cd_edges, rnd=cd_case),
    simple_test("ThemeParkWorld::CdDialog::~CdDialog(flag)", cddtor_ppc, cddtor_host,
                edge=lambda: [dict(flag=f) for f in (0, 1, -1, 2, 0x7FFF, -0x8000)], rnd=lambda r: dict(flag=r.randint(-3, 3))),
    simple_test("__sinit_TPMain_cpp", sinit_ppc, sinit_host, edge=lambda: [{}]),
]
