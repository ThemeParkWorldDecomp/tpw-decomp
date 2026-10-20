"""Differential tests for main GameSys.cpp and Main.cpp (Milestone 1 unit 2), against
libtpw_unit_GameSys.dylib: the unit's own sources plus the call-log fakes of
tests/unit_GameSys.cpp (see calllog.py).

Every translated function is called directly; GAMESYSTEM_Tick has a test per group of
states. A case sets the same globals, object fields, texts and return queues on both
sides, runs the function, and compares the call logs, the result, the globals the unit
writes, the labels of its pointer globals and the fields of the objects they reach.
The unit's functions call each other for real on both sides (the PowerPC side lifts any
stubs other modules left on them).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_GameSys"

A = dict(
    gTPWFlags=0x102B2B4C, gMainWindow=0x102B2BB4, gScreenHeight=0x102B2B50, gGameOptions=0x102B2B48,
    gTPDialogOptions=0x103BC664, gClock=0x1037F048, gText=0x1037FF18, gGame=0x1037F134, gTPFileInfo=0x102B3458,
    gTPGlobals=0x103B74C8, gExitError=0x1037F1C0, gPlayerSettings=0x1038071C, gOptionsBalance=0x102B4DA8,
    gSoundBalance=0x102B4E2C, gUserInfo=0x10380B34, gOnlineSystem=0x10380AB0, gOnlineControl=0x10396D28,
    gMasterSave=0x10380A8C, gMessageCentre=0x1037F6C4, gMouse=0x102DC1F4, gLobby=0x102E4888,
    gIntrface=0x102DC1AC, gUIInterface=0x10396440, gLoadSave=0x10396A7C, gSaveLoad=0x1037FEFC,
    gActionRecorder=0x1037EC44, gGameWorld=0x1037EBFC, gFirstPersonFlags=0x102B5A40, gCameraMatrix=0x102B5B74,
    gCameraTarget=0x102B5BD8, gPointerWorldPosn=0x102C6CF8, gPickupWorldPosn=0x102C6CEC,
    gSmallTexturesOnly=0x102C1DE4, gSoundBanks=0x102F7C94, gCommandLine=0x103BC1A8,
    gCommandLineOptions=0x1037F2B4, gGameType=0x102B3AA0, gGameTypeGuard=0x102B3AA4,
    gCurrentGameStatus=0x103BC164, gScreenshotNumber=0x103BC168, gFirstParkFrame=0x103BC16C,
    gRestartParkClocks=0x103BC170, gWasFirstPerson=0x103BC174, gGameSys_103bc178=0x103BC178,
    gTickCount=0x103BC17C, gTickNow=0x103BC180, gLastTurnTime=0x103BC184, gLastFlyTime=0x103BC188,
    gTickTime=0x103BC18C, gGameSystemState=0x103BC190, gHudState=0x103BC194, gScreenshotMovie=0x103BC198,
    gRightButtonFrames=0x103BC19C, gLeftButtonFrames=0x103BC1A0, gGameSystemInitialised=0x103BC1A4,
    gTurnsThisFrame=0x103BC3BC, gLastFrameClock=0x1034C18C, gFrameTime=0x1034C190, gWhiteOutClock=0x1034C194,
    gWhiteOutTime=0x1034C198)
ENGINE_OBJECTS = ("gei_Splash", "gei_System", "gei_Display", "gei_ProjectionMatrix", "gei_Capture")
ENGINE_POINTERS = ("gei_TCache", "gei_MapWho", "gei_Gamut", "gei_Rain", "gei_SpriteManager")

# Globals compared after every call: (name, size in bytes).
SCALARS = [("gTPWFlags", 4), ("gScreenHeight", 4), ("gGameOptions", 4), ("gTPDialogOptions", 4),
           ("gFirstPersonFlags", 4), ("gSmallTexturesOnly", 1), ("gCurrentGameStatus", 4),
           ("gScreenshotNumber", 4), ("gFirstParkFrame", 4), ("gRestartParkClocks", 4), ("gWasFirstPerson", 1),
           ("gGameSys_103bc178", 4), ("gTickCount", 4), ("gTickNow", 4), ("gLastTurnTime", 4),
           ("gLastFlyTime", 4), ("gTickTime", 4), ("gGameSystemState", 4), ("gHudState", 4),
           ("gScreenshotMovie", 4), ("gRightButtonFrames", 4), ("gLeftButtonFrames", 4),
           ("gGameSystemInitialised", 4), ("gTurnsThisFrame", 4), ("gLastFrameClock", 4), ("gFrameTime", 4),
           ("gWhiteOutClock", 4), ("gWhiteOutTime", 4)]

# Fields set and compared through tpw_gs_set_values / get_values: (base, offset, size).
VALUES = [("gPlayerSettings", 0x08, 4), ("gPlayerSettings", 0x30, 4), ("gPlayerSettings", 0x90, 4),
          ("gOptionsBalance", 0x0C, 4), ("gOptionsBalance", 0x60, 4), ("gSoundBalance", 0x38, 4),
          ("gSoundBalance", 0x3C, 4), ("gSoundBalance", 0x40, 4), ("gSoundBanks", 0x0C, 4), ("gIntrface", 0x24, 2),
          ("gIntrface", 0x28, 4), ("gGame", 0x3C, 4), ("gExitError", 0, 4), ("gCommandLineOptions", 0, 4),
          ("gCommandLineOptions", 0x404, 4), ("gCommandLineOptions", 0x408, 4), ("gCommandLineOptions", 0x40C, 4),
          ("gGameType", 0, 4), ("gei_System", 0, 4), ("gei_Display", 0, 4), ("world", 0x1DA738, 4),
          ("parkinfo", 0x4C, 4)]

# The unit's lazily created objects: pointer global -> (fixed label of a pre-existing
# object, Mac size).
OLD = dict(gLobby=(30, 0x38), gUserInfo=(31, 0xD4), gOnlineSystem=(32, 0x404), gOnlineControl=(33, 0xE8C),
           gMasterSave=(34, 0x4C), gMessageCentre=(35, 0x1D0), gMouse=(36, 4))
OLD_KIND = dict(gLobby="CLBLobby", gUserInfo="CFSUserInfo", gOnlineSystem="CFSOnlineSystem",
                gOnlineControl="COnlineControl", gMasterSave="CFSGlobalMasterSave", gMessageCentre="CMessageCentre",
                gMouse="CMouse")
POINTER_GLOBALS = list(OLD) + ["gGameWorld", "gMainWindow"] + list(ENGINE_POINTERS)
LABELS = dict(gClock=1, gText=2, gGame=3, gTPFileInfo=4, gExitError=5, gIntrface=6, gUIInterface=7, gLoadSave=8,
              gSaveLoad=9, gActionRecorder=10, gCommandLine=11, gGameType=12, gPointerWorldPosn=13,
              gPickupWorldPosn=14, gei_Splash=15, gei_System=16, gei_ProjectionMatrix=17, gei_Capture=18,
              window=20, gei_TCache=21, gei_MapWho=22, gei_Gamut=23, gei_Rain=24, gei_SpriteManager=25, world=26,
              analyser=27, themelist=28)
TEXTS = ["language", "user", "data", "theme", "ftp"]
POSTCULL_TVECTOR = 0x10268A10
SUBSCRIPTION_CTOR, SUBSCRIPTION_DTOR = 0x102657E8, 0x10267288

VP, I32, U32, U8 = C.c_void_p, C.c_int32, C.c_uint32, C.c_uint8

# The unit's own functions: real on both sides.
UNIT = [0x10233DE0, 0x10233E78, 0x102341DC, 0x1023436C, 0x102345C8, 0x102346D0, 0x10236548, 0x10236624,
        0x10236710, 0x10236750, 0x10236E6C, 0x10236EB8, 0x10236EF4, 0x10236F8C, 0x1023706C, 0x102370D4,
        0x1023721C, 0x10237254, 0x102372C8, 0x1015139C, 0x1015149C, 0x10151798, 0x101517E8]


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def w16(e, a, v):
    e.write(a, struct.pack(">H", v & 0xFFFF))


def w8(e, a, v):
    e.write(a, bytes([v & 0xFF]))


def rd(e, a, size):
    return {1: e.read(a, 1)[0], 2: struct.unpack(">H", e.read(a, 2))[0], 4: e.u32(a)}[size]


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f

# ======================================================================= PowerPC side


class GsState:
    """Per-case objects and texts of the PowerPC fakes."""


def build_stubs(emu):
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(UNIT)

    def i(addr, name, spec="", ret=None, then=None):
        S.internal(addr, R(name, spec, ret, then))

    def m(sym, name, spec="", ret=None, then=None):
        S.imp(sym, R(name, spec, ret, then))

    gs = lambda e: e.gs   # noqa: E731

    for sym, fn in CL.string_stubs().items():
        S.imp(sym, fn)

    # ---- C library and platform
    def time_(e):
        e.cl.rec("time")
        v = e.cl.ret("time")
        if e.r(3):
            w32(e, e.r(3), v)
        e.set_r(3, T.u32(v))

    def localtime_(e):
        e.cl.rec("localtime", (T.s32(e.u32(e.r(3))),))
        tm = e.alloc(0x24)
        w32(e, tm + 0xC, e.cl.ret("localtime.mday"))
        e.set_r(3, tm)

    def absolute(e):
        e.cl.rec("GetAbsolute")
        e.write(e.r(3), struct.pack(">Q", e.cl.ret("GetAbsolute") & 0xFFFFFFFFFFFFFFFF))

    def screen_to_client(e):
        p = e.r(4)
        x, y = T.s32(e.u32(p)), T.s32(e.u32(p + 4))
        e.cl.rec("ScreenToClient", (e.cl.lab(e.r(3)), x, y))
        w32(e, p, x + e.cl.ret("ScreenToClient.dx"))
        w32(e, p + 4, y + e.cl.ret("ScreenToClient.dy"))
        e.set_r(3, 1)

    def stricmp(e):
        a, b = e.cstr(e.r(3)), e.cstr(e.r(4))
        e.cl.rec("_stricmp", (), (a[:159], b[:159]))
        la, lb = a.lower(), b.lower()
        e.set_r(3, 0 if la == lb else T.u32(-1 if la < lb else 1))

    S.imp("time", time_)
    S.imp("localtime", localtime_)
    m("srand", "srand", "u")
    S.imp("GetAbsolute__Q213SamsUtilities6UTimerFv", absolute)
    m("Yield__Q213SamsUtilities7UThreadFv", "Yield")
    m("GetAsyncKeyState__10NS_MacDozeFi", "GetAsyncKeyState", "i",
      ret=lambda e, L: L.ret(f"GetAsyncKeyState:{T.s32(e.r(3))}") & 0xFFFF)
    S.imp("ScreenToClient__10NS_MacDozeFP8GrafPortPQ210NS_MacDoze8tagPOINT", screen_to_client)
    S.imp("_stricmp__10NS_MacDozeFPCcPCc", stricmp)
    m("LbTime_GetClock__Fv", "LbTime_GetClock", "", "i")
    i(0x10000E40, "UDebug", "s")

    # ---- runtime: operator delete and the message centre's arrays
    std_delete = emu.stubs["__dl__FPv"]

    def delete(e):
        p = e.r(3)
        std_delete(e)
        if e.cl.lab(p) > 0:
            e.cl.rec("delete", (e.cl.lab(p),))
            e.cl.forget(p)

    def construct_array(e):
        if e.r(4) == SUBSCRIPTION_CTOR:
            e.cl.rec("CMessageCentre", (e.cl.newlab(e.r(3)),))
        else:
            e.cl.rec("construct_array?", (e.r(4),))
        e.set_r(3, e.r(3))

    def destroy_arr(e):
        e.cl.rec("~CMessageCentre" if e.r(4) == SUBSCRIPTION_DTOR else "destroy_arr?")

    S.imp("__dl__FPv", delete)
    S.imp("__construct_array", construct_array)
    S.imp("__destroy_arr", destroy_arr)

    # ---- engine
    m("Init__7CSplashFPcPci", "CSplash::Init", "Lssi", ret=lambda e, L: 1)
    m("Close__7CSplashFv", "CSplash::Close", "L", ret=lambda e, L: 1)
    m("Flush__13CTextureCacheFi", "CTextureCache::Flush", "Li")
    m("ResetAll__7CMapWhoFv", "CMapWho::ResetAll", "L")
    m("Reset__7CMapWhoFi", "CMapWho::Reset", "Li")

    def final_callback(e):
        cb = e.r(4)
        e.cl.rec("CMapWho::SetFinalCallback", (e.cl.lab(e.r(3)), 0 if cb == 0 else 1 if cb == POSTCULL_TVECTOR else -1))

    S.imp("SetFinalCallback__7CMapWhoFPFv_v", final_callback)
    m("Projection__7CMatrixFfff", "CMatrix::Projection", "Lfff")
    m("Init__6CGamutFf", "CGamut::Init", "Lf")
    m("RenderSystem_Flush__Fv", "RenderSystem_Flush", "", ret=lambda e, L: 1)
    m("Load__5CRainFiPc", "CRain::Load", "Lis", ret=lambda e, L: 1)
    m("LoadBlobShadow__14CSpriteManagerFPc", "CSpriteManager::LoadBlobShadow", "Ls", ret=lambda e, L: 1)
    m("SetCapture__8CCaptureFPc", "CCapture::SetCapture", "Ls", ret=lambda e, L: 1)
    m("__ct__Q33bfl10FileSystem12CFileStorageFv", "CFileStorage", "N")
    m("__dt__Q33bfl10FileSystem12CFileStorageFv", "~CFileStorage", "D")
    m("__ct__Q33bfl10FileSystem9CDiscFileFv", "CDiscFile", "N")
    m("__dt__Q33bfl10FileSystem9CDiscFileFv", "~CDiscFile", "D")
    m("Close__Q33bfl10FileSystem5CFileFv", "CFile::Close", "L")

    def disc_open(e):
        e.cl.rec("CDiscFile::Open", (e.cl.lab(e.r(4)), e.r(6)), (e.cstr(e.r(5))[:159],))
        w32(e, e.r(3), e.cl.ret("CDiscFile::Open", -1))

    S.imp("Open__Q33bfl10FileSystem9CDiscFileFPCcUi", disc_open)

    # ---- main app callees
    i(0x101472F4, "CClock::Read", "L", "i")
    i(0x101479C0, "UseFixedTime", "Lu")
    i(0x10147A58, "CancelFixedTime", "L")
    i(0x101CA0B0, "UI_HideCursor")
    i(0x101CA110, "UI_ShowCursor")
    i(0x101CA944, "UI_GetButtonFlags", "", "h")
    i(0x101C8D94, "UI_DevicePointerMove", "hh")
    for addr, name in ((0x101EF5A8, "Process"), (0x101EF5F8, "InitGeneral"), (0x101EF64C, "DeInitGeneral"),
                       (0x101EF6A4, "InitGame"), (0x101EF6FC, "DeInitGame"), (0x101EF750, "InitLobby"),
                       (0x101EF7A4, "DeInitLobby"), (0x101EF7F8, "InitOnline"), (0x101EF84C, "DeInitOnline")):
        i(addr, "UIInterface::" + name, "L")
    i(0x101EF430, "SetThemeTextureDirectories", "i")
    i(0x101EF4E0, "SEC_AuthorizedUser", "", ret=lambda e, L: gs(e).text["user"])
    i(0x10187434, "UIMain_GetFullyOutOfFirstPerson")
    i(0x1015E93C, "CText::GetLanguageName", "L", ret=lambda e, L: gs(e).text["language"])

    i(0x101499D0, "CGame::Unpause", "L")
    i(0x10149B70, "CGame::GetCurrentTheme", "L", "i")
    i(0x10149BA8, "CGame::SetCurrentTheme(long)", "Li")
    i(0x10149C7C, "CGame::SetCurrentTheme(name)", "LT")

    def string_object(text):
        def f(e, L):
            obj = e.alloc(0x14)
            CL.tds_set(e, obj, text(e))
            return obj
        return f

    i(0x1014A058, "CGame::GetCurrentThemeName", "L", ret=string_object(lambda e: gs(e).texts["theme"]))
    i(0x10148C3C, "CGame::InitThemeResources", "LT")
    i(0x10147E34, "CGame::InitWorld", "L", ret=lambda e, L: L.ret("CGame::InitWorld", 1))
    i(0x10147EE4, "CGame::InitTheme", "Ls", ret=lambda e, L: L.ret("CGame::InitTheme", 1))
    i(0x101496EC, "CGame::ExitLevel", "L")
    i(0x10149838, "CGame::Iterate", "L")
    i(0x1014965C, "CGame::DeInit", "L")
    i(0x10147958, "CCommandLine::GetArgc", "L", ret=lambda e, L: gs(e).argc)
    i(0x1014798C, "CCommandLine::GetArgv", "L", ret=lambda e, L: gs(e).argv)
    i(0x10147778, "CCommandLine", "N")
    i(0x1014B6C8, "CExitError::ReportAnyError", "L")
    i(0x1014B62C, "CExitError::SetError", "Lius", then=lambda e, L: w32(e, e.r(3), 1))
    i(0x1015014C, "InitialiseKeytables")
    i(0x1015120C, "DeInitialiseKeyTables")
    i(0x1006FCBC, "CTRACE", "uss")
    i(0x1006FCFC, "TRACE", "s")
    i(0x1000DDE8, "GAME_ASSERTMSG", "is")
    i(0x1006CFF8, "TickRideHighlights")
    i(0x1006D5FC, "TickFlashObjects")

    i(0x10004EC4, "CSYS_TPFileInfo::GetDataPath", "L", ret=string_object(lambda e: gs(e).texts["data"]))
    i(0x10004E48, "CSYS_TPFileInfo::SetDataPath", "LT")
    i(0x10004F00, "CSYS_TPFileInfo::SetLevelPath", "LT")
    i(0x10004BC0, "FS_AddDirectorySeparator", "T",
      then=lambda e, L: CL.tds_set(e, e.r(3), CL.tds_text(e, e.r(3)) + ":"))

    def find_file_path(e, L):
        CL.tds_set(e, e.r(4), "Found:" + CL.tds_text(e, e.r(3)))
        return L.ret("FS_FindFilePath") & 0xFF

    i(0x100045C8, "FS_FindFilePath", "T", ret=find_file_path)
    i(0x100597F8, "FindFilePath", "suL", ret=lambda e, L: CL.new_text(e, "Found:" + e.cstr(e.r(3))))

    for addr, name in ((0x10007E5C, "ADV_Init"), (0x10007FC4, "ADV_Close"), (0x100091D8, "ADV_Tick2"),
                       (0x10009298, "ADV_Tick"), (0x10028CB0, "BMP_Tick"), (0x100BDF84, "PAR_CloseParticleSystem"),
                       (0x100BE564, "PAR_Tick"), (0x100C6964, "TickRideAnims"), (0x100D32E8, "RSSE_Init"),
                       (0x100D33DC, "RSSE_Tick"), (0x100D3674, "RSSE_RenderTick"), (0x100D3708, "RSSE_Close"),
                       (0x100D6E7C, "RSSE_SetReverbOnEax"), (0x100B9C48, "MOVIE_Stop"), (0x100B9D10, "MOVIE_Init"),
                       (0x100B9D58, "MOVIE_Close"), (0x100DCCD4, "SOUND_Close"), (0x100DD41C, "SOUND_StopAllSfx"),
                       (0x100DD4B0, "SOUND_FadeOutAllSfx"), (0x100DD668, "SOUND_Tick"),
                       (0x100DEB40, "SND_StartParkMusic"), (0x100DEBD4, "SND_StopParkMusic"),
                       (0x100DF310, "SND_RegisterGlobalSounds"), (0x100DF67C, "SND_FreeGlobalSounds"),
                       (0x100DF778, "SND_FreeLocalParkSounds"), (0x100D8E4C, "SCR_CloseScriptSystem"),
                       (0x100D907C, "SCR_ScriptTick"), (0x100D9184, "SCR_BuildScriptSpriteList"),
                       (0x100DFD88, "DC_TickDynamicCellMeshes"), (0x100E28B4, "STATIC_WhiteOut"),
                       (0x1005B4D4, "EngineTick1"), (0x1005B540, "EngineTick2"), (0x1005B590, "EngineTick3"),
                       (0x10068D90, "DestroyScratchGeomBuffer"), (0x1003491C, "HandleCamera"),
                       (0x10043028, "CoasterTickPreRender"), (0x10043200, "CoasterTickPostRender"),
                       (0x100554B4, "CoasterUserInputTick"), (0x1007C790, "ESP_RemoveSprites"),
                       (0x100752D4, "FLY_Init"), (0x100755A0, "FLY_SetFlyZone"), (0x1007567C, "FLY_Tick"),
                       (0x100783EC, "FLY_Close"), (0x1014B2BC, "CInGame::Init"),
                       (0x10237304, "CreateRestartParkSaveGame")):
        i(addr, name)
    i(0x10008F00, "ADV_Kill", "i")
    i(0x10031BC0, "LoadAdvisor", "sssu")
    i(0x10031ED0, "TickAdvisor", "u")
    i(0x10031F38, "DestroyAdvisor", "u")
    i(0x100BA3E0, "PAR_LoadLibrary", "s")
    i(0x100BDC24, "PAR_InitParticleSystem", "Lii")
    i(0x100BE03C, "PAR_SetLowSpec", "i")
    i(0x100B9914, "MOVIE_Play", "sLiii")
    i(0x100B9BCC, "MOVIE_Tick", "", "i")
    i(0x100DC940, "SOUND_Init", "sui")
    i(0x100DCF0C, "SOUND_SetSoundQuality", "i")
    i(0x100DD1B4, "SOUND_SetGlobalVolume", "u")
    i(0x100DD1F8, "SOUND_SetMusicVolume", "u")
    i(0x100DD238, "SOUND_SetSfxVolume", "u")
    i(0x100DD278, "SOUND_SetSpeechVolume", "u")
    i(0x100DD9FC, "SOUND_SendEvent", "iiiiii")
    i(0x100DDE7C, "SOUND_SetEarPosition", "fffffffff")
    i(0x100DE944, "SOUND_SetOcclusionProcessing", "i")
    i(0x100DEAC8, "SND_DuckVolume", "i")
    i(0x100DEC30, "SND_SetParkMusicLevel", "i")
    i(0x100DEC9C, "SND_SetChatterLevel", "i")
    i(0x100DEE84, "SND_SetRainLevel", "i")
    i(0x100DF4D4, "SND_RegisterLocalParkSounds", "T")
    i(0x100E5944, "CParkAnalyser::HowBusyIsPark", "L", "i")
    i(0x100E6A6C, "CParkAnalyser::CountNeighboursOfType", "Liwii", "i")
    i(0x1013B278, "CWorld::DoGameTurn", "L")
    i(0x1013B608, "CWorld::DoOnlineGameTurn", "L")
    i(0x1013DE70, "CWorld::UpdateThingsInEngine", "Lf")
    i(0x1013E09C, "CWorld::UpdateBumperCarsInEngine", "Lf")
    i(0x1013EBC8, "CWorld::GetParkAnalyser", "L", ret=lambda e, L: gs(e).analyser)
    i(0x1005B36C, "AnimateRides", "u")
    i(0x100638B0, "LevelInit1", "", ret=lambda e, L: L.ret("LevelInit1", 1))
    i(0x100639B0, "LevelInit2", "", ret=lambda e, L: L.ret("LevelInit2", 1))
    i(0x10063B80, "LevelDeInit", "b")
    i(0x100645B0, "TickLevelLights", "i")
    i(0x10068D14, "CreateScratchGeomBuffer", "i")
    i(0x1005B6C8, "ResetCursorOverSprites", "L")
    i(0x10065F50, "CalculatePointerWorldPosn", "LfLL")
    i(0x10067084, "GetMapCellFromPointerWorldPosn", "L")
    i(0x1007ACE0, "ESP_InitialiseAndLoadSprites", "bb")
    i(0x10077AE4, "FLY_Draw", "f")
    i(0x1007DAE8, "CIntrface::DrawFPSCounter", "Lf")
    i(0x1007DB78, "CIntrface::Process", "L")
    i(0x1007DBB8, "CIntrface::DoPointedThingComputations", "L")
    i(0x1007E2E0, "CMouse::AssignMouseMode", "LL")

    def field(offset, key, size=4):
        def then(e, L):
            cur = rd(e, e.r(3) + offset, size)
            if size == 4:
                cur = T.s32(cur)
            v = L.ret(key, cur)
            (w32 if size == 4 else w8)(e, e.r(3) + offset, v)
        return then

    i(0x100A57E4, "CLBLobby", "N")
    i(0x100A58C8, "~CLBLobby", "D")
    i(0x100A5B40, "CLBLobby::Init", "L")
    i(0x100A5E8C, "CLBLobby::Tick", "L", then=field(0x14, "CLBLobby::Tick.state"))
    i(0x100A5F78, "CLBLobby::Render", "L")
    i(0x100A6004, "CLBLobby::Close", "L", "i")

    i(0x10213870, "COnlineControl", "N", then=lambda e, L: w8(e, e.r(3) + 0x10, 0))
    i(0x10213DD4, "~COnlineControl", "D")
    i(0x102143C4, "COnlineControl::InitClient", "L")
    i(0x10214678, "COnlineControl::InitGameWorld", "L")
    i(0x10214840, "COnlineControl::Shutdown", "L")
    i(0x102148BC, "COnlineControl::WantToShutDown", "L", "b")
    i(0x10214944, "COnlineControl::Update", "L")
    i(0x10214C34, "COnlineControl::ProcessQueue", "L", then=field(0x10, "COnlineControl::ProcessQueue.ready", 1))

    def online_ctor(e, L):
        w32(e, e.r(3) + 0xF8, gs(e).themelist)
        w32(e, e.r(3) + 0x3F0, L.ret("CFSOnlineSystem.pending", 0))

    i(0x1016F7D0, "CFSOnlineSystem", "N", then=online_ctor)
    i(0x1017055C, "~CFSOnlineSystem", "D")
    i(0x10177654, "CFSOnlineSystem::GetFTPDownloadParkInfo", "L", ret=lambda e, L: gs(e).parkinfo)
    i(0x10177608, "CFSOnlineSystem::ReturnFTPDownloadLocalPath", "L", ret=lambda e, L: gs(e).text["ftp"])
    i(0x1017A588, "CFSOnlineSystemThemeList::ReturnOnlineThemeName", "Li",
      ret=lambda e, L: CL.new_text(e, f"Theme{T.s32(e.r(4))}"))
    i(0x1016F5D8, "CGameType", "L")
    i(0x1016F6A0, "CGameType::SetGameType", "Li", then=lambda e, L: w32(e, e.r(3), e.r(4)))

    i(0x1017DE54, "CFSUserInfo", "N", then=lambda e, L: w32(e, e.r(3) + 0x60, L.ret("CFSUserInfo.user", -1)))
    i(0x1017DFEC, "~CFSUserInfo", "D")
    i(0x1017E9D8, "CFSUserInfo::GetGameLoginSlotUserName", "Li",
      ret=lambda e, L: gs(e).slotname if L.ret("CFSUserInfo::GetGameLoginSlotUserName") else 0)
    i(0x1017EE38, "CFSUserInfo::CreateNewGameLoginUserInSlot", "LiWi")
    i(0x1017F330, "CFSUserInfo::GameLogin", "Li")
    i(0x1017F61C, "CFSUserInfo::GameLogout", "L")

    i(0x101694DC, "CFSGlobalMasterSave", "N")
    i(0x1016997C, "~CFSGlobalMasterSave", "D")
    i(0x1016A4FC, "CFSGlobalMasterSave::IsEasyModeUser", "L", "b")
    i(0x1016BA70, "CFSGlobalMasterSave::IsFirstTimePlayer", "L", "b")
    i(0x1016BAB8, "CFSGlobalMasterSave::ClearFirstTimePlayer", "L")

    i(0x101FC47C, "CFSLoadSave::LoadHidden", "LW")
    i(0x101FC4F4, "CFSLoadSave::Save", "LW")
    i(0x101FCDCC, "CFSLoadSave::LoadMostRecent", "L")
    i(0x10157FEC, "CSaveLoad::Load", "LsLi")
    i(0x10144E2C, "CActionRecorder::ReplayRecording", "Li")
    i(0x10151DF8, "CMsgEvent", "Ni")
    i(0x1015256C, "CMessageCentre::PostMessage", "LL", ret=lambda e, L: 1)
    return S


def ppc_begin(e, c):
    """Builds the case's memory on the PowerPC side and starts its call log."""
    s = e.gs = GsState()
    e.heap_in_scratch = True
    fixed = {}
    for name, label in LABELS.items():
        if name in A:
            fixed[A[name]] = label
    for name in ENGINE_OBJECTS:
        if name in LABELS:
            fixed[e.data_import(name)] = LABELS[name]
    # pointer globals and their objects
    ptr = c.get("ptr", {})
    s.objects = {}
    for name, (label, size) in OLD.items():
        if ptr.get(name):
            obj = e.alloc(size)
            s.objects[name] = obj
            fixed[obj] = label
            w32(e, A[name], obj)
        else:
            w32(e, A[name], 0)
    for name in ENGINE_POINTERS:
        slot = e.data_import(name)
        if c.get("engine", {}).get(name, True):
            obj = e.alloc(0x40)
            fixed[obj] = LABELS[name]
            w32(e, slot, obj)
        else:
            w32(e, slot, 0)
    window = e.alloc(0x20)
    fixed[window] = LABELS["window"]
    w32(e, A["gMainWindow"], window)
    s.world_field = e.alloc(0x10)
    world = s.world_field - 0x1DA738        # only the field at +0x1da738 is ever read
    fixed[world] = LABELS["world"]
    w32(e, A["gGameWorld"], world)
    s.analyser = e.alloc(0x10)
    fixed[s.analyser] = LABELS["analyser"]
    s.themelist = e.alloc(0x10)
    fixed[s.themelist] = LABELS["themelist"]
    s.parkinfo = e.alloc(0x50)
    s.slotname = e.alloc(0x14)
    # texts
    s.texts = {k: c.get("texts", {}).get(k, k.upper()) for k in TEXTS}
    s.text = {k: CL.new_text(e, v) for k, v in s.texts.items()}
    args = c.get("args", ["TPW"])
    s.argc = len(args)
    s.argv = e.alloc(4 * 9)
    for n, a in enumerate(args):
        w32(e, s.argv + 4 * n, CL.new_text(e, a))
    # scalars and values
    g = c.get("g", {})
    for name, size in SCALARS:
        (w8 if size == 1 else w32)(e, A[name], g.get(name, 0))
    w8(e, A["gGameTypeGuard"], 1)
    e.write(A["gCommandLineOptions"] + 4, b"\0" * 0x400)
    for (base, off, size), v in zip(VALUES, c.get("v", [0] * len(VALUES))):
        addr = value_addr(e, base) + off
        (w16 if size == 2 else w32)(e, addr, v)
    lobby_state, user, pending, ready = c.get("fields", (1, -1, 0, 0))
    if "gLobby" in s.objects:
        w32(e, s.objects["gLobby"] + 0x14, lobby_state)
    if "gUserInfo" in s.objects:
        w32(e, s.objects["gUserInfo"] + 0x60, user)
    if "gOnlineSystem" in s.objects:
        w32(e, s.objects["gOnlineSystem"] + 0xF8, s.themelist)
        w32(e, s.objects["gOnlineSystem"] + 0x3F0, pending)
    if "gOnlineControl" in s.objects:
        w8(e, s.objects["gOnlineControl"] + 0x10, ready)
    cam = c.get("cam", [0.0] * 19)
    e.write(A["gCameraMatrix"], struct.pack(">16f", *cam[:16]))
    e.write(A["gCameraTarget"], struct.pack(">3f", *cam[16:19]))
    # TPGlobals strings
    for off, text in ((0x409C, "levels"), (0x440C, ".TPWS"), (0x4830, "data")):
        CL.tds_set(e, A["gTPGlobals"] + off, text)
    for off, text in ((0x4510, "autosave"), (0x4524, "restart")):
        CL.wds_set(e, A["gTPGlobals"] + off, text)
    e.cl = CL.PpcLog(fixed, c.get("q", {}))


def value_addr(e, base):
    if base == "world":
        return e.gs.world_field - 0x1DA738
    if base == "parkinfo":
        return e.gs.parkinfo
    if base in A:
        return A[base]
    return e.data_import(base)


def ppc_observe(e):
    L, s = e.cl, e.gs
    out = {name: rd(e, A[name], size) for name, size in SCALARS}
    out["values"] = [T.s32(rd(e, value_addr(e, b) + off, size)) if size == 4 else rd(e, value_addr(e, b) + off, size)
                     for b, off, size in VALUES]
    ptrs = {}
    for name in POINTER_GLOBALS:
        a = e.data_import(name) if name.startswith("gei_") else A[name]
        ptrs[name] = L.lab(e.u32(a))
    out["ptrs"] = ptrs

    def fld(name, off, size=4):
        p = e.u32(A[name])
        if p == 0:
            return 0
        v = rd(e, p + off, size)
        return T.s32(v) if size == 4 else v

    out["fields"] = [fld("gLobby", 0x14), fld("gUserInfo", 0x60), fld("gOnlineSystem", 0x3F0),
                     fld("gOnlineControl", 0x10, 1)]
    out["file"] = e.cstr(A["gCommandLineOptions"] + 4)[:200]
    return out

# ======================================================================= host side


class HostCase:
    pass


def host_addr(lib, name):
    return F(lib, "tpw_gs_addr", VP, C.c_char_p)(name.encode())


def host_begin(lib, c):
    h = HostCase()
    addr = lambda n: host_addr(lib, n)   # noqa: E731
    new = F(lib, "tpw_gs_new", VP, C.c_char_p)
    F(lib, "tpw_gs_reset_headers", None)()
    F(lib, "tpw_gs_set_strings", None)()
    fixed = {}
    for name, label in LABELS.items():
        a = addr(name)
        if a and name not in ENGINE_POINTERS:
            fixed[label] = a
    ptr = c.get("ptr", {})
    for name, (label, size) in OLD.items():
        slot = C.c_void_p.from_address(addr(name))
        if ptr.get(name):
            obj = new(OLD_KIND[name].encode())
            slot.value = obj
            fixed[label] = obj
        else:
            slot.value = None
    for name in ENGINE_POINTERS:
        slot = C.c_void_p.from_address(addr(name))
        if c.get("engine", {}).get(name, True):
            obj = new(b"engine")
            slot.value = obj
            fixed[LABELS[name]] = obj
        else:
            slot.value = None
    window = new(b"engine")
    C.c_void_p.from_address(addr("gMainWindow")).value = window
    fixed[LABELS["window"]] = window
    world = new(b"CWorld")
    C.c_void_p.from_address(addr("gGameWorld")).value = world
    fixed[LABELS["world"]] = world
    fixed[LABELS["analyser"]] = F(lib, "tpw_gs_analyser", VP)()
    fixed[LABELS["themelist"]] = F(lib, "tpw_gs_themelist", VP)()
    texts = {k: c.get("texts", {}).get(k, k.upper()) for k in TEXTS}
    for n, k in enumerate(TEXTS):
        F(lib, "tpw_gs_set_text", None, I32, C.c_char_p)(n, texts[k].encode("latin-1"))
    args = c.get("args", ["TPW"])
    arr = (C.c_char_p * 8)(*[a.encode("latin-1") for a in args[:8]])
    F(lib, "tpw_gs_set_args", None, I32, VP)(len(args), arr)
    g = c.get("g", {})
    for name, size in SCALARS:
        a = addr(name)
        if size == 1:
            C.c_uint8.from_address(a).value = g.get(name, 0) & 0xFF
        else:
            C.c_uint32.from_address(a).value = g.get(name, 0) & 0xFFFFFFFF
    vals = (C.c_int32 * len(VALUES))(*[T.s32(v) for v in c.get("v", [0] * len(VALUES))])
    F(lib, "tpw_gs_set_values", None, VP)(vals)
    F(lib, "tpw_gs_set_object_fields", None, I32, I32, I32, I32)(*c.get("fields", (1, -1, 0, 0)))
    cam = c.get("cam", [0.0] * 19)
    (C.c_float * 16).from_address(addr("gCameraMatrix"))[:] = cam[:16]
    (C.c_float * 3).from_address(addr("gCameraTarget"))[:] = cam[16:19]
    CL.host_begin(lib, fixed, c.get("q", {}))
    return h


def host_observe(lib):
    addr = lambda n: host_addr(lib, n)   # noqa: E731
    out = {}
    for name, size in SCALARS:
        a = addr(name)
        out[name] = C.c_uint8.from_address(a).value if size == 1 else C.c_uint32.from_address(a).value
    vals = (C.c_int32 * len(VALUES))()
    F(lib, "tpw_gs_get_values", None, VP)(vals)
    out["values"] = list(vals)
    lab = F(lib, "tpw_cl_lab", I32, VP)
    out["ptrs"] = {name: lab(C.c_void_p.from_address(addr(name)).value) for name in POINTER_GLOBALS}
    fields = (C.c_int32 * 4)()
    F(lib, "tpw_gs_get_object_fields", None, VP)(fields)
    out["fields"] = list(fields)
    out["file"] = F(lib, "tpw_gs_command_file", C.c_char_p)().decode("latin-1")[:200]
    return out

# ======================================================================= running a case


STUBS = None


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    STUBS = build_stubs(e)
    host_addr(runner.lib, "gGameType")    # constructs the game type object before any case


def make_test(name, ppc_call, host_call):
    def ppc(e, c):
        saved = STUBS.activate(e)
        try:
            ppc_begin(e, c)
            result = ppc_call(e, c)
            out = ppc_observe(e)
            out["log"] = e.cl.log
        finally:
            Stubs_deactivate(e, saved)
        out["result"] = result
        return out

    def host(lib, c):
        host_begin(lib, c)
        result = host_call(lib, c)
        log = CL.host_end(lib)
        out = host_observe(lib)
        out["log"] = log
        out["result"] = result
        return out

    return name, ppc, host


def Stubs_deactivate(e, saved):
    CL.Stubs.deactivate(e, saved)
    e.cl = None


class GsTest(FnTest):
    """FnTest with a first-difference message for call logs."""

    def run_case(self, runner, case):
        mism = super().run_case(runner, case)
        if mism and any(m.startswith("log") for m in mism):
            p, h = self._last
            diff = CL.first_difference(p["log"], h["log"])
            return [f"log: {diff}"] + [m for m in mism if not m.startswith("log")][:3]
        return mism


def fn_test(name, ppc_call, host_call, edge, rnd):
    n, ppc, host = make_test(name, ppc_call, host_call)
    t = GsTest(n, None, None, edge=edge, rnd=rnd)

    def ppc_keep(e, c):
        t._p = ppc(e, c)
        return t._p

    def host_keep(lib, c):
        h = host(lib, c)
        t._last = (t._p, h)
        return h

    t.ppc, t.host = ppc_keep, host_keep
    return t


def call(e, addr, args=()):
    return T.call_ppc(e, addr, list(args))


def hcall(lib, name, res=None, *argtypes):
    return F(lib, "tpw_gs_" + name, res, *argtypes)

# ======================================================================= case generators


def base_values(r):
    return [r.choice([0, 0, 1, 2, -1]), r.choice([0, 1, 2, 3]), r.choice([0, 0, 1]), r.choice([0, 1, 2, 3, -1]),
            r.randint(0, 300), r.randint(0, 127), r.randint(0, 127), r.randint(0, 127), r.randint(0, 50),
            r.randint(0, 0xFFFF), r.choice([0, 0, 1]), r.choice([0, 1, 7]), 0, 0, 0, 0, 0, r.choice([0, 1, 2]),
            r.getrandbits(32), r.getrandbits(32), r.choice([0, 4, 4, 3, 5]), r.randint(0, 9)]


def rnd_case(r, **extra):
    c = dict(g={}, v=base_values(r), ptr={}, fields=(1, -1, 0, 0), q={}, texts={}, args=["TPW"],
             cam=[r.uniform(-500, 500) for _ in range(19)])
    c.update(extra)
    return c


def rnd_flags(r):
    return r.choice([0, 8, 0x16, 0x2, 0x10, 0x4, 0x9]) | (r.getrandbits(32) & ~0x1F if r.random() < 0.3 else 0)


# ---- UseSmallTextures
def small_ppc(e, c):
    return call(e, 0x10233DE0)[0] & 0xFF


def small_host(lib, c):
    return hcall(lib, "UseSmallTextures", U8)()


def small_rnd(r):
    c = rnd_case(r)
    c["g"] = dict(gSmallTexturesOnly=r.choice([0, 0, 0, 1, 2, 0xFF]), gTPDialogOptions=r.getrandbits(32))
    c["v"][2] = r.choice([0, 0, 1, -1, 5])
    c["v"][3] = r.choice([0, 1, 2, 3, -1, 0x7FFFFFFF])
    return c


# ---- GAMESYSTEM_Init
def init_ppc(e, c):
    call(e, 0x10233E78)


def init_host(lib, c):
    hcall(lib, "GAMESYSTEM_Init")()


def init_rnd(r):
    c = rnd_case(r)
    c["g"] = dict(gGameOptions=r.getrandbits(32), gFirstPersonFlags=rnd_flags(r),
                  gWasFirstPerson=r.choice([0, 1, 2]), gSmallTexturesOnly=r.choice([0, 1]),
                  gTPDialogOptions=r.getrandbits(32), gGameSystemState=r.randint(0, 16),
                  gFrameTime=r.getrandbits(32), gScreenshotMovie=r.getrandbits(32), gHudState=r.getrandbits(32))
    c["args"] = rnd_args(r)
    c["v"][12] = r.choice([0, 0, 1])   # exit error already set
    c["q"] = {"time": [r.getrandbits(32)], "GetAbsolute": [r.getrandbits(64)], "CClock::Read": [r.getrandbits(32)]}
    c["texts"] = {"language": r.choice(["english", "french", ""]), "user": r.choice(["Bullfrog", "", "x" * 40])}
    return c


# ---- the movie players
def logo_ppc(e, c):
    call(e, 0x102341DC)


def logo_host(lib, c):
    hcall(lib, "PlayMeTheLogoAnim")()


def fmv_ppc(e, c):
    call(e, 0x1023436C)


def fmv_host(lib, c):
    hcall(lib, "PlayMeARandomFMV")()


def movie_rnd(r):
    c = rnd_case(r)
    c["g"] = dict(gGameOptions=r.getrandbits(32), gScreenHeight=r.choice([480, 600, 768, 0, -1, 0x7FFFFFFF,
                                                                         r.getrandbits(32)]))
    c["q"] = {"FS_FindFilePath": [r.choice([1, 1, 0, 2, 0xFF])], "ScreenToClient.dx": [r.randint(-2000, 2000)],
              "ScreenToClient.dy": [r.randint(-2000, 2000)], "time": [r.getrandbits(31)],
              "localtime.mday": [r.randint(0, 31)]}
    return c


# ---- PostCullOperations
def postcull_ppc(e, c):
    call(e, 0x102345C8)


def postcull_host(lib, c):
    hcall(lib, "PostCullOperations")()


def postcull_rnd(r):
    c = rnd_case(r)
    c["g"] = dict(gFirstPersonFlags=rnd_flags(r), gTurnsThisFrame=r.getrandbits(32), gFirstParkFrame=r.getrandbits(32))
    return c


# ---- GAMESYSTEM_Tick
def tick_ppc(e, c):
    return call(e, 0x102346D0)[0]


def tick_host(lib, c):
    return hcall(lib, "GAMESYSTEM_Tick", U32)()


def tick_common(r, state):
    c = rnd_case(r)
    c["g"] = dict(gGameSystemState=state, gGameSystemInitialised=r.choice([1, 1, 1, 1, 1, 0, 2]),
                  gGameOptions=r.getrandbits(32), gTPWFlags=r.getrandbits(32), gLastFrameClock=r.getrandbits(32),
                  gLeftButtonFrames=r.getrandbits(8), gRightButtonFrames=r.getrandbits(8),
                  gFirstPersonFlags=rnd_flags(r), gWasFirstPerson=r.choice([0, 1, 2]),
                  gScreenshotMovie=r.choice([0, 0, 0, 1]), gScreenHeight=r.choice([480, 768, 600]))
    c["q"] = {"CClock::Read": [r.getrandbits(32) if r.random() < 0.2 else r.randint(0, 100000) for _ in range(3)],
              "GetAsyncKeyState:1": [r.choice([0, 0, 1, -32768])], "GetAsyncKeyState:2": [r.choice([0, 0, 1])],
              "GetAsyncKeyState:27": [r.choice([0, 0, 0, 1])], "GetAsyncKeyState:32": [r.choice([0, 0, 0, 1])],
              "UI_GetButtonFlags": [r.choice([0, 0, 0, 1, 0x100, -1])], "MOVIE_Tick": [r.choice([0, 0, 1])],
              "FS_FindFilePath": [r.choice([1, 0])], "time": [r.getrandbits(31)],
              "localtime.mday": [r.randint(1, 31)]}
    c["v"][10] = r.choice([0, 0, 1])
    return c


def tick_front_rnd(r):
    """States 0 and 4 to 8 and out-of-range states."""
    return tick_common(r, r.choice([0, 4, 5, 6, 7, 8, 5, 6, 7, 8, 16, 17, -1, 0x7FFFFFFF]))


def tick_lobby_rnd(r):
    """States 1, 2, 3: lobby set-up, frames and closing."""
    state = r.choice([1, 2, 2, 2, 3, 3])
    c = tick_common(r, state)
    c["ptr"] = {k: r.random() < 0.5 for k in OLD}
    if state in (2, 3):
        c["ptr"]["gLobby"] = True
    c["fields"] = (r.choice([1, 1, 1, 0, 2, -1]), -1, 0, 0)
    now = r.randint(0, 1 << 20)
    c["g"]["gTickTime"] = r.choice([now - r.randint(-100, 800), now, now + 5, r.getrandbits(32),
                                     now - r.choice([499, 500, 501, 502, 30, 31, 32])])
    c["q"]["CClock::Read"] = [r.randint(0, 1 << 20), r.randint(0, 1 << 20), now]
    c["q"]["CLBLobby::Tick.state"] = [r.choice([1, 1, 1, 0, 3])]
    c["q"]["CLBLobby::Close"] = [r.choice([2, 2, 3, 0, 1, 4, -1])]
    c["texts"] = {"data": r.choice(["HD:TPW:Data", "", "x" * 60])}
    return c


def tick_lobby_edges():
    """Lobby frames at the particle catch-up boundaries (499 to 502 ms behind) and at one tick."""
    r = random.Random(11)
    out = []
    for behind in (499, 500, 501, 502, 30, 31, 32, 0, -1):
        c = tick_common(r, 2)
        c["g"]["gGameSystemInitialised"] = 1
        c["ptr"] = {"gLobby": True}
        c["fields"] = (1, -1, 0, 0)
        now = r.randint(1000, 1 << 20)
        c["g"]["gTickTime"] = now - behind
        c["q"]["CClock::Read"] = [r.randint(0, 1 << 20), r.randint(0, 1 << 20), now]
        c["q"]["CLBLobby::Tick.state"] = [1]
        out.append(c)
    return out + edge(tick_lobby_rnd)()


def tick_park_load_rnd(r):
    """State 9: loading a park, offline and online."""
    c = tick_common(r, 9)
    c["ptr"] = {k: r.random() < 0.6 for k in OLD}
    c["fields"] = (1, r.choice([-1, -1, 0, 2]), r.choice([0, 0, 1]), r.choice([0, 1]))
    c["v"][13:17] = [0, r.choice([0, 0, 0, 1]), 0, 0]
    c["v"][17] = r.choice([0, 1, 1, 2])
    c["q"].update({"CFSOnlineSystem.pending": [r.choice([0, 1])], "CFSUserInfo.user": [r.choice([-1, 0])],
                   "CFSUserInfo::GetGameLoginSlotUserName": [r.choice([0, 1])],
                   "CGame::GetCurrentTheme": [r.randint(0, 20)],
                   "LevelInit1": [r.choice([1, 1, 1, 0])], "CGame::InitWorld": [r.choice([1, 1, 1, 0])],
                   "CGame::InitTheme": [r.choice([1, 1, 1, 0])], "LevelInit2": [r.choice([1, 1, 1, 0])],
                   "CFSGlobalMasterSave::IsFirstTimePlayer": [r.choice([0, 1])]})
    # the online client loop: ProcessQueue's readiness and WantToShutDown per pass, the clock
    passes = r.randint(1, 4)
    ready = [0] * (passes - 1) + [r.choice([1, 1, 0, 2])]
    shut = [r.choice([0, 0, 0, 1]) for _ in range(passes)]
    start = r.choice([0, 1000, r.getrandbits(32)])
    clock = [start] + [(start + r.choice([0, 10, 299999, 300000, 300001, -5])) & 0xFFFFFFFF for _ in range(passes)]
    clock.append((start + 300001) & 0xFFFFFFFF)     # the loop always ends
    c["q"].update({"COnlineControl::ProcessQueue.ready": ready, "COnlineControl::WantToShutDown": shut,
                   "LbTime_GetClock": clock})
    c["texts"] = {"theme": r.choice(["Lost Kingdom", "Halloween", ""]), "data": "HD:TPW:Data",
                  "ftp": r.choice(["HD:Downloads:park.TPWS", ""])}
    return c


def tick_park_rnd(r):
    """State 10: a park frame, and 13, 14, 15."""
    state = r.choice([10, 10, 10, 10, 13, 14, 15])
    c = tick_common(r, state)
    c["ptr"] = {k: r.random() < 0.5 for k in OLD}
    now = r.randint(0, 1 << 24)
    behind = r.choice([0, 1, 30, 31, 32, 62, 93, 248, 500, 1999, 2000, 2001, 5000, -10])
    c["g"].update(gTickTime=now - behind, gTickCount=r.getrandbits(32) if r.random() < 0.3 else r.randint(0, 64),
                  gTurnsThisFrame=r.choice([0, 0, 1, 2, 3, 4, 0xFFFFFFFF]), gRestartParkClocks=r.choice([0, 0, 1]),
                  gLastFlyTime=now - r.randint(-100, 100), gLastTurnTime=now - r.randint(-300, 300),
                  gWhiteOutTime=r.choice([0, 0, -5, 1, 300, 150]), gWhiteOutClock=r.getrandbits(32),
                  gCurrentGameStatus=r.choice([0, 0, 0, 1, 2, 3, 4, -1]))
    clock = [r.randint(0, 1 << 24), r.randint(0, 1 << 24)]
    if c["g"]["gRestartParkClocks"]:
        clock += [now - behind, r.randint(0, 1 << 24), r.randint(0, 1 << 24)]
    c["q"]["CClock::Read"] = clock + [now]
    c["q"].update({"CParkAnalyser::HowBusyIsPark": [r.choice([0, 50, 0x59, 0x5A, 200, -3])],
                   "CParkAnalyser::CountNeighboursOfType": [r.choice([0, 99, 100, 101, 1000, -1])],
                   "GetAbsolute": [r.getrandbits(40) for _ in range(3)],
                   "COnlineControl::WantToShutDown": [r.choice([0, 0, 1])], "CDiscFile::Open": [-1]})
    c["v"][17] = r.choice([0, 1, 2, 3])
    c["cam"] = r.choice([c["cam"], [0.0] * 19, [r.choice([0.0, -0.0, 1.0, 2.0]) for _ in range(19)]])
    return c


def tick_leave_rnd(r):
    """States 11 (leave the park) and 12 (quit)."""
    state = r.choice([11, 11, 12, 12])
    c = tick_common(r, state)
    c["ptr"] = {k: r.random() < 0.6 for k in OLD}
    c["fields"] = (1, r.choice([-1, 0, 3]), r.choice([0, 1]), 0)
    c["g"]["gCurrentGameStatus"] = r.choice([0, 1, 2, 3, 3])
    c["v"][17] = r.choice([0, 1, 1, 2])
    c["q"].update({"CFSOnlineSystem.pending": [r.choice([0, 1])], "CFSUserInfo.user": [r.choice([-1, 0])],
                   "CFSGlobalMasterSave::IsEasyModeUser": [r.choice([0, 1])]})
    return c


# ---- GAMESYSTEM_Close
def close_ppc(e, c):
    call(e, 0x10236548)


def close_host(lib, c):
    hcall(lib, "GAMESYSTEM_Close")()


def close_rnd(r):
    c = tick_leave_rnd(r)
    c["g"]["gGameSystemState"] = r.choice([10, 10, 2, 11, 0])
    c["g"]["gGameSystemInitialised"] = r.choice([0, 1, 1, 1])
    return c


# ---- the screenshot
def shot_ppc(e, c):
    call(e, 0x10236624)


def shot_host(lib, c):
    hcall(lib, "GAMESYSTEM_Screenshot")()


def shot_rnd(r):
    c = rnd_case(r)
    c["g"] = dict(gScreenshotNumber=r.choice([0, 5, 99998, 99999, 123456, -1, -2147483647, 0x7FFFFFFE]))
    n = r.randint(0, 4)
    c["q"] = {"CDiscFile::Open": [0] * n + [r.choice([-1, -2, -0x7FFFFFFF])]}
    return c


# ---- small functions
def small_fn(addr, name, argf=None, res=None, conv=None):
    def ppc(e, c):
        r3 = call(e, addr, [T.u32(argf(c))] if argf else [])[0]
        return conv(r3) if conv else None

    def host(lib, c):
        f = hcall(lib, name, res, *([I32] if argf else []))
        v = f(argf(c)) if argf else f()
        return v if conv else None

    return ppc, host


def scalar_rnd(r):
    c = rnd_case(r)
    c["arg"] = T.s32(r.choice([0, 1, 2, 3, 4, -1, r.getrandbits(32)]))
    c["g"] = dict(gScreenshotMovie=r.getrandbits(32), gHudState=r.getrandbits(32),
                  gCurrentGameStatus=r.getrandbits(32))
    return c


def clocks_rnd(r):
    c = rnd_case(r)
    c["g"] = dict(gTickTime=r.getrandbits(32), gLastFlyTime=r.getrandbits(32), gLastTurnTime=r.getrandbits(32),
                  gTickCount=r.getrandbits(32), gGameSys_103bc178=r.getrandbits(32))
    c["q"] = {"CClock::Read": [r.getrandbits(32) for _ in range(3)]}
    return c


def weather_rnd(r):
    c = rnd_case(r)
    c["engine"] = {"gei_Rain": r.random() < 0.7, "gei_SpriteManager": r.random() < 0.7}
    return c


# ---- Main.cpp
def theme_path_ppc(e, c):
    return e.cstr(call(e, 0x1015139C)[0])


def theme_path_host(lib, c):
    return hcall(lib, "get_theme_path", C.c_char_p)().decode("latin-1")


def theme_path_rnd(r):
    c = rnd_case(r)
    c["texts"] = {"data": r.choice(["HD:TPW:Data", "", "Data:", "x" * 80]),
                  "theme": r.choice(["Lost Kingdom", "Wonder Land", "", "Halloween:"])}
    return c


SWITCHES = ["v", "version", "quickload", "nodebug", "noload", "SAVEDEBUG", "flmouse", "bwcursor"]


def rnd_args(r):
    args = ["TPW"]
    for _ in range(r.randint(0, 5)):
        k = r.random()
        if k < 0.55:
            sw = r.choice(SWITCHES)
            if r.random() < 0.3:
                sw = sw.upper() if r.random() < 0.5 else sw.capitalize()
            args.append(r.choice(["/", "-", "/", "-", "+", ""]) + sw + r.choice(["", "", "", "x"]))
        elif k < 0.85:
            args.append(r.choice(["park", "My Park", "a", ""]) + r.choice([".TPWS", ".tpws", ".TPW", ".TPWSx", "TPWS"]))
        else:
            args.append(r.choice([".TPWS", "TPWS", "x", "-", "/", "--v", "Park.TpWs"]))
    return args


def controlinit_ppc(e, c):
    return T.s32(call(e, 0x1015149C, [e.gs.argc, e.gs.argv])[0])


def controlinit_host(lib, c):
    return hcall(lib, "ControlInit", I32)()


def controlinit_rnd(r):
    c = rnd_case(r)
    c["args"] = rnd_args(r)
    c["v"][12] = r.choice([0, 0, 0, 1])
    c["v"][13] = r.choice([0, 0, 1])
    c["v"][15] = r.choice([0, 1])
    c["v"][16] = r.choice([0, 1])
    return c


# ---- the static initialisers
SINIT_STRINGS = ([(0x103BC620, True)] + [(0x103BC420 + 0x14 * k, False) for k in range(16)] +
                 [(0x103BC400, False), (0x103BC3E0, False), (0x103BC3C0, True)])


def sinit_ppc(e, c):
    """Runs __sinit_GameSys_cpp; returns the texts it built, in construction order,
    and the objects it registered for destruction."""
    saved = STUBS.activate(e)
    try:
        ppc_begin(e, c)
        regs = []
        e.stubs["__register_global_object"] = lambda em: regs.append(em.r(3))
        for a, wide in SINIT_STRINGS:
            e.write(a, b"\0" * 0x14)
        call(e, 0x102374A0)
        texts = [CL.wds_text(e, a) if wide else CL.tds_text(e, a) for a, wide in SINIT_STRINGS]
        log = e.cl.log
    finally:
        Stubs_deactivate(e, saved)
    order = [a for a, _ in SINIT_STRINGS] + [A["gCommandLine"]]
    return dict(texts="|".join(texts) + "|", registered=[order.index(a) if a in order else -1 for a in regs],
                log=log)


def sinit_host(lib, c):
    buf = C.create_string_buffer(1024)
    F(lib, "tpw_gs_sinit_texts", None, VP)(buf)
    # the host's statics are built by the C++ runtime in definition order, which is the
    # original's construction order (checked on the PowerPC side by the registrations)
    return dict(texts=buf.value.decode("latin-1"), registered=list(range(len(SINIT_STRINGS) + 1)),
                log=[("CCommandLine", (CL.FIRST_NEW,), ())])


def options_ppc(e, c):
    base = A["gCommandLineOptions"]
    fill = bytes(c["fill"])
    e.write(base, fill)
    saved = STUBS.activate(e)
    try:
        call(e, 0x10151834)
    finally:
        CL.Stubs.deactivate(e, saved)
    return e.read(base, 0x410)


def options_host(lib, c):
    buf = C.create_string_buffer(bytes(c["fill"]), 0x410)
    F(lib, "tpw_gs_construct_options", None, VP)(buf)
    return bytes(buf.raw[:0x410])     # the fields it sets are zero, so byte order does not matter


def options_rnd(r):
    return {"fill": [r.getrandbits(8) for _ in range(0x410)]}


def edge(rnd, n=40, seed=7):
    def f():
        r = random.Random(seed)
        return [rnd(r) for _ in range(n)]
    return f


TESTS = [
    fn_test("UseSmallTextures", small_ppc, small_host, edge(small_rnd), small_rnd),
    fn_test("GAMESYSTEM_Init", init_ppc, init_host, edge(init_rnd), init_rnd),
    fn_test("PlayMeTheLogoAnim", logo_ppc, logo_host, edge(movie_rnd), movie_rnd),
    fn_test("PlayMeARandomFMV", fmv_ppc, fmv_host, edge(movie_rnd), movie_rnd),
    fn_test("PostCullOperations", postcull_ppc, postcull_host, edge(postcull_rnd), postcull_rnd),
    fn_test("GAMESYSTEM_Tick (front end)", tick_ppc, tick_host, edge(tick_front_rnd), tick_front_rnd),
    fn_test("GAMESYSTEM_Tick (lobby)", tick_ppc, tick_host, tick_lobby_edges, tick_lobby_rnd),
    fn_test("GAMESYSTEM_Tick (load park)", tick_ppc, tick_host, edge(tick_park_load_rnd), tick_park_load_rnd),
    fn_test("GAMESYSTEM_Tick (park)", tick_ppc, tick_host, edge(tick_park_rnd), tick_park_rnd),
    fn_test("GAMESYSTEM_Tick (leave, quit)", tick_ppc, tick_host, edge(tick_leave_rnd), tick_leave_rnd),
    fn_test("GAMESYSTEM_Close", close_ppc, close_host, edge(close_rnd), close_rnd),
    fn_test("GAMESYSTEM_Screenshot", shot_ppc, shot_host, edge(shot_rnd), shot_rnd),
    fn_test("GAMESYSTEM_ScreenshotMovie", *small_fn(0x10236710, "GAMESYSTEM_ScreenshotMovie", lambda c: c["arg"]),
            edge(scalar_rnd), scalar_rnd),
    fn_test("GAMESYSTEM_SetHudState", *small_fn(0x10236750, "GAMESYSTEM_SetHudState", lambda c: c["arg"]),
            edge(scalar_rnd), scalar_rnd),
    fn_test("GS_SetCurrentGameStatus", *small_fn(0x10236E6C, "GS_SetCurrentGameStatus", lambda c: c["arg"]),
            edge(scalar_rnd), scalar_rnd),
    fn_test("GS_GetCurrentGameStatus", *small_fn(0x10236EB8, "GS_GetCurrentGameStatus", None, I32, T.s32),
            edge(scalar_rnd), scalar_rnd),
    fn_test("init_time", *small_fn(0x10236EF4, "init_time"), edge(clocks_rnd), clocks_rnd),
    fn_test("InitialiseWeather", *small_fn(0x10236F8C, "InitialiseWeather"), edge(weather_rnd), weather_rnd),
    fn_test("InitialiseBlobShadow", *small_fn(0x1023706C, "InitialiseBlobShadow"), edge(weather_rnd), weather_rnd),
    fn_test("InitialisePersistentSystems", *small_fn(0x102370D4, "InitialisePersistentSystems"), edge(weather_rnd),
            weather_rnd),
    fn_test("DeInitialisePersistentSystems", *small_fn(0x1023721C, "DeInitialisePersistentSystems"),
            edge(weather_rnd), weather_rnd),
    fn_test("GameSys_RestartLevelReInit", *small_fn(0x10237254, "GameSys_RestartLevelReInit", None, I32, T.s32),
            edge(weather_rnd), weather_rnd),
    fn_test("GameSys_RestartLevelDeInit", *small_fn(0x102372C8, "GameSys_RestartLevelDeInit", None, I32, T.s32),
            edge(weather_rnd), weather_rnd),
    fn_test("get_theme_path", theme_path_ppc, theme_path_host, edge(theme_path_rnd), theme_path_rnd),
    fn_test("ControlInit", controlinit_ppc, controlinit_host, edge(controlinit_rnd, 80), controlinit_rnd),
    fn_test("ControlIterate", *small_fn(0x10151798, "ControlIterate"), edge(weather_rnd), weather_rnd),
    fn_test("ControlDeInit", *small_fn(0x101517E8, "ControlDeInit"), edge(weather_rnd), weather_rnd),
    FnTest("__sinit_GameSys_cpp", sinit_ppc, sinit_host, edge=lambda: [rnd_case(random.Random(1))],
           rnd=lambda r: rnd_case(r)),
    FnTest("__sinit_Main_Cpp", options_ppc, options_host, edge=lambda: [{"fill": [0xFF] * 0x410},
                                                                       {"fill": [0] * 0x410}], rnd=options_rnd),
]
