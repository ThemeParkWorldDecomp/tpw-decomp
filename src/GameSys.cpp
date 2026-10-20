// GameSys.cpp (main app 0x10233de0..0x10237770): the game system. GAMESYSTEM_Tick is the
// state machine that runs the front end (logo, intro movie, lobby) and the park; the other
// functions start it up, shut it down and set up the systems a park needs.
//
// ThemeParkWorld::Process (TPMain.cpp) calls GAMESYSTEM_Init on its first frame and
// GAMESYSTEM_Tick on every frame, ORing the result into gTPWFlags. GameSys.h lists the
// states.
//
// Not translated: TPGlobals::~TPGlobals (0x10233378, filed here by the inventory but
// TPGlobals.cpp's), GAMESYSTEM_Save (0x1023678c), GAMESYSTEM_Load (0x10236b30) and
// CreateRestartParkSaveGame (0x10237304), which save and restore a park.
#include "platform/msl.h"
#include "GameSys.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "ActionRecorder.h"
#include "Advisor.h"
#include "analyser.h"
#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "Clock.h"
#include "Debug.h"
#include "ExitErr.h"
#include "FS_LoadSave.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "HLSprite.h"
#include "InGame.h"
#include "LbTime.h"
#include "Main.h"
#include "MapWho.h"
#include "Mouse.h"
#include "OptionsBalance.h"
#include "SaveLoad.h"
#include "SoundBalance.h"
#include "TPAdvisor.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPDialog.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPMovie.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "Text.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "World.h"
#include "bfl_FileSystem.h"
#include "intrface.h"
#include "keyboard.h"
#include "lobby.h"
#include "online.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_capture.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_splash.h"
#include "tb_starfield.h"
#include "tb_texturecache.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

// The strings __sinit_GameSys_cpp constructs, in its order (the first and the last come
// from headers that only some files include; the language names, title and "Language"
// are the same set as TPMain.cpp's and TPDialog.cpp's).
TbDynamicStringTemplate<char16_t> gGameSysUpload(u"upload");    // 0x103bc620
TbDynamicStringTemplate<char> gGameSysLanguages[16] = {          // 0x103bc420
    TbDynamicStringTemplate<char>("English"), TbDynamicStringTemplate<char>("French"),
    TbDynamicStringTemplate<char>("German"), TbDynamicStringTemplate<char>("Spanish"),
    TbDynamicStringTemplate<char>("Swedish"), TbDynamicStringTemplate<char>("Italian"),
    TbDynamicStringTemplate<char>("Dutch"), TbDynamicStringTemplate<char>("Polish"),
    TbDynamicStringTemplate<char>("Danish"), TbDynamicStringTemplate<char>("Japanese"),
    TbDynamicStringTemplate<char>("TraditionalChinese"), TbDynamicStringTemplate<char>("SimplifiedChinese"),
    TbDynamicStringTemplate<char>("Thai"), TbDynamicStringTemplate<char>("Korean"),
    TbDynamicStringTemplate<char>("Hebrew"), TbDynamicStringTemplate<char>("American"),
};
TbDynamicStringTemplate<char> gGameSysTitle("Theme Park World");    // 0x103bc400
TbDynamicStringTemplate<char> gGameSysLanguage("Language");         // 0x103bc3e0
TbDynamicStringTemplate<char16_t> gGameSysGmsFile(u"gms.dat");      // 0x103bc3c0
CCommandLine gCommandLine;                                          // 0x103bc1a8

GS_GAME_STATES gCurrentGameStatus;   // 0x103bc164
int32_t gScreenshotNumber;           // 0x103bc168
int32_t gFirstParkFrame;             // 0x103bc16c
int32_t gRestartParkClocks;          // 0x103bc170
uint8_t gWasFirstPerson;             // 0x103bc174
int32_t gGameSys_103bc178;           // 0x103bc178
int32_t gTickCount;                  // 0x103bc17c
int32_t gTickNow;                    // 0x103bc180
int32_t gLastTurnTime;               // 0x103bc184
int32_t gLastFlyTime;                // 0x103bc188
int32_t gTickTime;                   // 0x103bc18c
int32_t gGameSystemState;            // 0x103bc190
int32_t gHudState;                   // 0x103bc194
int32_t gScreenshotMovie;            // 0x103bc198
int32_t gRightButtonFrames;          // 0x103bc19c
int32_t gLeftButtonFrames;           // 0x103bc1a0
int32_t gGameSystemInitialised;      // 0x103bc1a4
uint32_t gTurnsThisFrame;            // 0x103bc3bc
int32_t gLastFrameClock;             // 0x1034c18c
uint32_t gFrameTime;                 // 0x1034c190
uint32_t gWhiteOutClock;             // 0x1034c194
int32_t gWhiteOutTime;               // 0x1034c198

namespace {

// The splash's data folder (the splash keeps the pointer, so it is not copied).
char* const kSplashPath = const_cast<char*>("data:init:");

// The original inlines this test in states 5 to 8: a mouse button, Escape or Space skips
// the logo and the intro movie.
bool SkipPressed() {
    return UI_GetButtonFlags() != 0 || NS_MacDoze::GetAsyncKeyState(0x1b) != 0 ||
           NS_MacDoze::GetAsyncKeyState(0x20) != 0;
}

// The original inlines these lazy accessors at every use: the object is created on first
// use and never checked for NULL afterwards.
CFSOnlineSystem* OnlineSystem() {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem;
    return gOnlineSystem;
}

CFSUserInfo* UserInfo() {
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
    return gUserInfo;
}

COnlineControl* OnlineControl() {
    if (gOnlineControl == nullptr) gOnlineControl = new COnlineControl;
    return gOnlineControl;
}

CFSGlobalMasterSave* MasterSave() {
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
    return gMasterSave;
}

CMessageCentre* MessageCentre() {
    if (gMessageCentre == nullptr) gMessageCentre = new CMessageCentre;
    return gMessageCentre;
}

// Movies play letterboxed: their top edge is 64/480 of the screen height down (unsigned
// arithmetic in the original).
int32_t MovieTop() { return static_cast<int32_t>((static_cast<uint32_t>(gScreenHeight) << 6) / 480); }

}  // namespace

// 0x10233de0: whether to load the small (half size) theme textures.
bool UseSmallTextures() {
    if (gSmallTexturesOnly != 0) return true;
    if (gPlayerSettings.mCustomOptions == 0) {
        const int32_t quality = gOptionsBalance.mValues[1].mInteger;   // TEXTUREQUALITY
        return quality == 0 || quality == 1;
    }
    return (gTPDialogOptions & 0x800) != 0;
}

// 0x10233e78
void GAMESYSTEM_Init() {
    gClock.mTime.UseFixedTime(0x20);
    SetThemeTextureDirectories(UseSmallTextures() ? 1 : 0);   // inlined in the original
    CTRACE(6, "%s\n", SEC_AuthorizedUser());
    char** argv = gCommandLine.GetArgv();   // the original fetches argv first
    if (ControlInit(gCommandLine.GetArgc(), argv) == 0) {
        gExitError.ReportAnyError();
        return;
    }
    InitialiseKeytables();
    if ((gFirstPersonFlags & kFirstPersonAny) != 0)
        gWasFirstPerson = (gFirstPersonFlags & kFirstPersonAny) != 0 ? 1 : 0;

    std::time_t now;
    std::time(&now);
    char banner[256];
    // __DATE__ and __TIME__ of the original build; %dl is the original's format. The
    // original uses sprintf; every text this file formats fits its buffer.
    std::snprintf(banner, sizeof(banner), "*** Compiled %s at %s %dl ***\n", "Jan  8 2001", "18:08:54",
                  static_cast<int32_t>(now));
    TRACE(banner);

    gGameSystemInitialised = 1;
    msl_srand(static_cast<uint32_t>(SamsUtilities::UTimer::GetAbsolute() / 1000));
    const int32_t quality = gPlayerSettings.mCurrent.mSoundQuality;
    SOUND_Init(const_cast<char*>(""), gGameOptions & kGameOptionSound400, quality);
    SOUND_SetSoundQuality(quality);
    MOVIE_Init();
    SOUND_SetGlobalVolume(100);
    SOUND_SetMusicVolume(gSoundBalance.mValues[13].mInteger);    // MUSIC
    SOUND_SetSpeechVolume(gSoundBalance.mValues[14].mInteger);   // SPEECH
    SOUND_SetSfxVolume(gSoundBalance.mValues[12].mInteger);      // SFX
    SND_RegisterGlobalSounds();
    SOUND_SendEvent(0, gSoundBanks[3], 0xd2, 0, 0, 0);
    SOUND_Tick();
    {
        TbDynamicStringTemplate<char> unused;   // constructed around the call, never used
        gei_Splash.Init(gText.GetLanguageName(), kSplashPath, 500);
    }
    gUIInterface.InitGeneral();
    gei_Splash.Close();
    UI_ShowCursor();
    gGameSystemState = (gGameOptions & kGameOptionFrontEnd) != 0 ? 4 : 9;
    gFrameTime = 0;
    gLastFrameClock = gClock.Read();
    ADV_Init();
    gScreenshotMovie = 0;
    gHudState = 1;
}

// 0x102341dc: the Bullfrog logo. A movie plays full screen at the window's top left or,
// in a window, offset so that it lands at the screen's top left.
void PlayMeTheLogoAnim() {
    TbDynamicStringTemplate<char> path;
    char name[256];
    std::strcpy(name, "Data:Movies:bf.mov");
    const bool found = FS_FindFilePath(TbDynamicStringTemplate<char>(name), path) == 1;
    if (found) {
        RenderSystem_Flush();
        if ((gGameOptions & kGameOptionFullScreen) != 0) {
            MOVIE_Play(path.mData, gMainWindow, 0, MovieTop(), 0);
        } else {
            NS_MacDoze::tagPOINT origin;
            origin.x = 0;
            origin.y = 0;
            NS_MacDoze::ScreenToClient(gMainWindow, &origin);
            MOVIE_Play(path.mData, gMainWindow, -origin.x, -origin.y + MovieTop(), 0);
        }
    }
}

// 0x1023436c: one of eight intro movies, chosen by the day of the month.
void PlayMeARandomFMV() {
    TbDynamicStringTemplate<char> path;
    std::time_t now;
    std::time(&now);
    char name[256];
    switch (std::localtime(&now)->tm_mday % 8) {
    case 0: std::strcpy(name, "Data:Movies:bub.mov"); break;
    case 1: std::strcpy(name, "Data:Movies:buc.mov"); break;
    case 2: std::strcpy(name, "Data:Movies:grav.mov"); break;
    case 3: std::strcpy(name, "Data:Movies:jug.mov"); break;
    case 4: std::strcpy(name, "Data:Movies:mir.mov"); break;
    case 5: std::strcpy(name, "Data:Movies:plan.mov"); break;
    case 6: std::strcpy(name, "Data:Movies:roc.mov"); break;
    case 7: std::strcpy(name, "Data:Movies:roll.mov"); break;
    }
    const bool found = FS_FindFilePath(TbDynamicStringTemplate<char>(name), path) == 1;
    if (found) {
        if ((gGameOptions & kGameOptionFullScreen) != 0) {
            MOVIE_Play(path.mData, gMainWindow, 0, MovieTop(), 0);
        } else {
            NS_MacDoze::tagPOINT origin;
            origin.x = 0;
            origin.y = 0;
            NS_MacDoze::ScreenToClient(gMainWindow, &origin);
            MOVIE_Play(path.mData, gMainWindow, -origin.x, -origin.y + MovieTop(), 0);
        }
    }
}

// 0x102345c8: the park's map-who final callback (state 9 installs it): the per-frame work
// that needs the culled scene.
void PostCullOperations() {
    AnimateRides(3);
    if ((gFirstPersonFlags & kFirstPersonAny) == 0) {
        CalculatePointerWorldPosn(&gPointerWorldPosn, 60.0f, &gPickupWorldPosn, nullptr);
        GetMapCellFromPointerWorldPosn(&gPointerWorldPosn);
        ResetCursorOverSprites(&gPointerWorldPosn);
    }
    ControlIterate();
    DC_TickDynamicCellMeshes();
    TickFlashObjects();
    TickRideHighlights();
    gIntrface.Process();
    EngineTick1();
    gTurnsThisFrame = 0;
    SCR_BuildScriptSpriteList();
    gIntrface.DoPointedThingComputations();
    gFirstParkFrame = 0;
}

// 0x102346d0
uint32_t GAMESYSTEM_Tick() {
    uint32_t result = 0;

    gFrameTime = gClock.Read() - gLastFrameClock;
    if (gIntrface.mShowFPS != 0) gIntrface.DrawFPSCounter(1000.0f / static_cast<float>(gFrameTime));
    gLastFrameClock = gClock.Read();

    // frames each mouse button has been held (VK_LBUTTON, VK_RBUTTON)
    if (NS_MacDoze::GetAsyncKeyState(1) != 0)
        ++gLeftButtonFrames;
    else
        gLeftButtonFrames = 0;
    if (NS_MacDoze::GetAsyncKeyState(2) != 0)
        ++gRightButtonFrames;
    else
        gRightButtonFrames = 0;

    if (gGameSystemInitialised == 0) return result;

    switch (gGameSystemState) {
    case 4:
        gGameSystemState = 5;
        break;

    case 5:
        UI_HideCursor();
        gUIInterface.Process();
        if (SkipPressed()) {
            gGameSystemState = 7;
        } else {
            PlayMeTheLogoAnim();
            gGameSystemState = 6;
        }
        break;

    case 6:
        gUIInterface.Process();
        if (MOVIE_Tick() != 0 || SkipPressed()) {
            MOVIE_Stop();
            gGameSystemState = 7;
        }
        break;

    case 7:
        gUIInterface.Process();
        if (SkipPressed()) {
            UI_ShowCursor();
            gGameSystemState = 1;
        } else {
            PlayMeARandomFMV();
            gGameSystemState = 8;
        }
        break;

    case 8:
        gUIInterface.Process();
        if (MOVIE_Tick() != 0 || SkipPressed()) {
            MOVIE_Stop();
            UI_ShowCursor();
            gGameSystemState = 1;
        }
        break;

    case 1: {   // set up the lobby
        {
            TbDynamicStringTemplate<char> unused;
            gei_Splash.Init(gText.GetLanguageName(), kSplashPath, 500);
        }
        UI_HideCursor();
        InitialisePersistentSystems();
        CreateScratchGeomBuffer(1);
        gClock.mTime.CancelFixedTime();
        gUIInterface.InitLobby();
        char advisor[1024];
        std::snprintf(advisor, sizeof(advisor), "%s:Global:Advisor", gTPFileInfo.GetDataPath().mData);
        LoadAdvisor(advisor, nullptr, nullptr, 0);
        ADV_Init();
        PAR_InitParticleSystem(nullptr, gOptionsBalance.mValues[22].mInteger, 0x800);   // PARTICLEDENSITY
        if (gPlayerSettings.mCurrent.mDetail == 0)
            PAR_SetLowSpec(1);
        else
            PAR_SetLowSpec(0);
        PAR_LoadLibrary(FindFilePath("Data:Particle:Tp2.plb", 1, nullptr));
        gLobby = new CLBLobby;
        gLobby->Init();
        gGame.Unpause();
        gGame.mF3C = 0;
        ESP_InitialiseAndLoadSprites(false, true);
        gGameSystemState = 2;
        gei_Splash.Close();
        UI_ShowCursor();
        SND_DuckVolume(0);
        gTickNow = gClock.Read();
        gTickTime = gTickNow;
        UI_DevicePointerMove(0, 0);
        break;
    }

    case 2:   // the lobby
        if (gLobby->mState == 1) {
            TickRideAnims();
            SOUND_Tick();
            ADV_Tick();
            TickAdvisor(0);
            ADV_Tick2();
            SOUND_SetEarPosition(0.0f, 0.0f, -50.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f);
            gUIInterface.Process();
            AnimateRides(3);
            EngineTick1();
            EngineTick2();
            gLobby->Tick();
            gLobby->Render();
            EngineTick3();
            // particles tick every 31 ms, catching up at most half a second
            gTickNow = gClock.Read();
            if (gTickNow - gTickTime > 500) gTickTime = gTickNow - 500;
            while (gTickNow > gTickTime) {
                gTickTime += 31;
                PAR_Tick();
            }
        }
        if (gLobby->mState != 1) gGameSystemState = 3;
        break;

    case 3: {   // close the lobby
        const int32_t choice = gLobby->Close();
        delete gLobby;   // the original calls the destructor without a NULL check
        gLobby = nullptr;
        gUIInterface.DeInitLobby();
        ADV_Close();
        DestroyAdvisor(0);
        gei_TCache->Flush(0);
        gei_MapWho->ResetAll();
        SOUND_FadeOutAllSfx();
        SOUND_Tick();
        {
            TbDynamicStringTemplate<char> unused;
            gei_Splash.Init(gText.GetLanguageName(), kSplashPath, 500);
            gei_System.mFlags |= CSystem::kFlag00000001;
        }
        gei_MapWho->Reset(0x70);
        gei_ProjectionMatrix.Projection(0.1f, 1000.0f, 90.0f);
        gei_Gamut->Init(90.0f);
        gTPFileInfo.SetDataPath(gTPGlobals.mDataFolder);
        gTPFileInfo.SetLevelPath(TbDynamicStringTemplate<char>("levels"));
        gei_Display.mFlags &= ~0x20u;
        DestroyScratchGeomBuffer();
        PAR_CloseParticleSystem();
        ESP_RemoveSprites();
        switch (choice) {
        case 2:   // a park was chosen
            gGameSystemState = 9;
            break;
        case 3:
        default:
            gGameSystemState = 12;
            break;
        }
        break;
    }

    case 9: {   // load the park
        if (OnlineSystem()->mFTPParkPending != 0) {
            CFSOnlineSystem* online = OnlineSystem();
            CFSFTPParkInfo* info = OnlineSystem()->GetFTPDownloadParkInfo();
            gGame.SetCurrentTheme(
                TbDynamicStringTemplate<char>(online->mThemeList->ReturnOnlineThemeName(info->mTheme)));
            gGame.mF3C = 0;
        } else {
            gGame.mF3C = 1;
        }
        gGame.InitThemeResources(TbDynamicStringTemplate<char>(get_theme_path()));
        {
            TbDynamicStringTemplate<char> unused;
            gei_Splash.Init(gText.GetLanguageName(), kSplashPath, 500);
        }
        UI_HideCursor();
        RSSE_Init();
        FLY_Init();
        PAR_InitParticleSystem(nullptr, gOptionsBalance.mValues[22].mInteger, 0x800);
        if (gPlayerSettings.mCurrent.mDetail == 0)
            PAR_SetLowSpec(1);
        else
            PAR_SetLowSpec(0);
        PAR_LoadLibrary(FindFilePath("Data:Particle:Tp2.plb", 1, nullptr));
        if ((gGameOptions & kGameOptionFrontEnd) == 0) {
            // no front end: log in the first player slot, as "Debug User" if it is empty
            if (UserInfo()->mGameUser == -1) {
                const int32_t theme = gGame.GetCurrentTheme();
                if (UserInfo()->GetGameLoginSlotUserName(EFSUserInfoGameUser(0)) == nullptr) {
                    TbDynamicStringTemplate<char16_t> name(u"Debug User");
                    UserInfo()->CreateNewGameLoginUserInSlot(EFSUserInfoGameUser(0), name, 0);
                }
                UserInfo()->GameLogin(EFSUserInfoGameUser(0));
                gGame.SetCurrentTheme(theme);
            }
            InitialisePersistentSystems();
        }
        SND_RegisterLocalParkSounds(gGame.GetCurrentThemeName());
        SOUND_Tick();
        SND_StartParkMusic();
        if (LevelInit1() == 0) {
            result = 2;
        } else {
            gei_MapWho->SetFinalCallback(PostCullOperations);
            if (gGame.InitWorld() == 0) {
                result = 2;
            } else {
                if (TheGameType().mType == kGameTypeOnline)
                    gUIInterface.InitOnline();
                else
                    gUIInterface.InitGame();
                ESP_InitialiseAndLoadSprites(false, false);
                if (gGame.InitTheme(get_theme_path()) == 0) {
                    result = 2;
                } else if (LevelInit2() == 0) {
                    result = 2;
                } else {
                    gFirstParkFrame = 1;
                    TickLevelLights(1);
                    gGameSystemState = 15;
                    GS_SetCurrentGameStatus(GS_PLAYING);
                }
            }
        }
        init_time();   // inlined in the original
        if (TheGameType().mType == kGameTypeOnline) {
            SamsUtilities::UDebug("ONLINE CHAT: init starting\n");
            const uint32_t start = LbTime_GetClock();
            OnlineControl()->InitClient();
            for (;;) {
                SamsUtilities::UThread::Yield();
                OnlineControl()->ProcessQueue();
                if (OnlineControl()->mClientReady == 1) {
                    OnlineControl()->InitGameWorld();
                    SamsUtilities::UDebug("ONLINE CHAT: init done\n");
                    break;
                }
                if (OnlineControl()->WantToShutDown()) {
                    gGameSystemState = 11;
                    break;
                }
                const uint32_t current = LbTime_GetClock();
                GAME_ASSERTMSG(current >= start,
                               "currentTime is less than starting time in GAMESYSTEM_Tick() online init!\n");
                if (current - start > 300000) {
                    OnlineControl()->Shutdown();
                    gGameSystemState = 11;
                    SamsUtilities::UDebug("ONLINE CHAT: client init timed out. Packing up and going home.\n");
                    break;
                }
            }
        }
        if (gCommandLineOptions.mF404 == 0) {
            if (TheGameType().mType == kGameTypeOnline) {
                if (!OnlineControl()->WantToShutDown()) {
                    CSaveLoadHeader header;
                    gSaveLoad.Load(OnlineSystem()->ReturnFTPDownloadLocalPath(), &header, CSaveLoad::LoadType(1));
                    gActionRecorder.ReplayRecording(ReplayType(0));
                }
            } else {
                gLoadSave.LoadMostRecent();
            }
        }
        if (TheGameType().mType != kGameTypeOnline && MasterSave()->IsFirstTimePlayer()) {
            MasterSave()->ClearFirstTimePlayer();
            if (TheGameType().mType == kGameTypeEasy) {
                {
                    CMsgEvent message(EventType(10));
                    MessageCentre()->PostMessage(message);
                }
                {
                    CMsgEvent message(EventType(0));
                    MessageCentre()->PostMessage(message);
                }
            }
        }
        gei_Splash.Close();
        UI_ShowCursor();
        OnlineSystem()->mFTPParkPending = 0;
        SND_DuckVolume(0);
        break;
    }

    case 15:   // park loaded
        if (TheGameType().mType != kGameTypeOnline) CreateRestartParkSaveGame();
        gGameSystemState = 10;
        break;

    case 10: {   // the park
        if (gRestartParkClocks != 0) {
            init_time();
            gRestartParkClocks = 0;
        }
        if (gScreenshotMovie != 0)
            gClock.mTime.UseFixedTime(0x20);
        else
            gClock.mTime.CancelFixedTime();
        TickRideAnims();
        // the simulation ticks every 31 ms, catching up at most two seconds
        gTickNow = gClock.Read();
        if (gTickNow - gTickTime > 2000) gTickTime = gTickNow - 2000;
        while (gTickNow > gTickTime) {
            gTickTime += 31;
            ++gTickCount;
            if ((gTPWFlags & 8) == 0 && (gGameOptions & kGameOptionFullScreen) != 0) continue;
            PAR_Tick();
            BMP_Tick();
            RSSE_Tick();
            if ((gTickCount & 1) == 0) {
                gLastFlyTime = gTickTime;
                FLY_Tick();
                SCR_ScriptTick();
            }
            if ((gTickCount & 7) == 0) {
                gLastTurnTime = gTickTime;
                if (gTurnsThisFrame < 3) {
                    ++gTurnsThisFrame;
                    if (TheGameType().mType == kGameTypeNormal || TheGameType().mType == kGameTypeEasy) {
                        gGameWorld->DoGameTurn();
                    } else if (TheGameType().mType == kGameTypeOnline) {
                        gGameWorld->DoOnlineGameTurn();
                        OnlineControl()->Update();
                    }
                }
                RSSE_SetReverbOnEax();
            }
            if ((gTickCount & 0x1f) == 0) {
                int32_t busy = gGameWorldRef->GetParkAnalyser()->HowBusyIsPark();
                if (busy > 0x59) busy = 0x59;
                if (gGameWorld->mF1DA738 == 4) busy = 0;
                SND_SetParkMusicLevel(busy);
                tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id;
                id.mIndex = gIntrface.mF24;
                int32_t chatter =
                    gGameWorld->GetParkAnalyser()->CountNeighboursOfType(CThing::ThingModel(1), id, 4, 0);
                if (chatter > 100) chatter = 100;
                if (gGameWorld->mF1DA738 == 4) chatter = 0;
                SND_SetChatterLevel(chatter);
                FLY_SetFlyZone();
            }
        }
        if ((gTPWFlags & 8) != 0 || (gGameOptions & kGameOptionFullScreen) == 0) {
            // hear from the camera in first person, else from halfway to what it looks at
            const float x = gCameraMatrix.m[3][0];
            const float y = gCameraMatrix.m[3][1];
            const float z = gCameraMatrix.m[3][2];
            if ((gFirstPersonFlags & kFirstPersonAny) != 0) {
                SOUND_SetEarPosition(x, y, z, gCameraMatrix.m[2][0], gCameraMatrix.m[2][1], gCameraMatrix.m[2][2],
                                     gCameraMatrix.m[1][0], gCameraMatrix.m[1][1], gCameraMatrix.m[1][2]);
            } else {
                const float dx = x - gCameraTarget[0];
                const float dy = y - gCameraTarget[1];
                const float dz = z - gCameraTarget[2];
                SOUND_SetEarPosition(ppc_fnmsubs(dx, 0.5f, x), ppc_fnmsubs(dy, 0.5f, y), ppc_fnmsubs(dz, 0.5f, z),
                                     gCameraMatrix.m[2][0], gCameraMatrix.m[2][1], gCameraMatrix.m[2][2],
                                     gCameraMatrix.m[1][0], gCameraMatrix.m[1][1], gCameraMatrix.m[1][2]);
            }
            SOUND_Tick();
            ADV_Tick();
            gUIInterface.Process();
            RSSE_RenderTick();
            gGameWorld->UpdateBumperCarsInEngine(static_cast<float>(gTickNow - gTickTime) / 31.0f);
            FLY_Draw(static_cast<float>(gTickNow - gLastFlyTime) / 62.0f);
            gGameWorld->UpdateThingsInEngine(static_cast<float>(gTickNow - gLastTurnTime) / 248.0f);
            CoasterUserInputTick();
            CoasterTickPreRender();
            AnimateRides(2);
            HandleCamera();
            TickAdvisor(0);
            TickAdvisor(1);
            ADV_Tick2();
            // entering or leaving first person flips a display flag and whites the screen
            // out for 300 ms
            const bool firstPerson = (gFirstPersonFlags & kFirstPersonAny) != 0;
            if ((firstPerson && gWasFirstPerson == 0) || (!firstPerson && gWasFirstPerson == 1)) {
                gei_Display.mFlags ^= 0x2000u;
                SOUND_SetOcclusionProcessing(gPlayerSettings.mCurrent.mSoundQuality);
                gWhiteOutTime = 300;
                gWhiteOutClock = static_cast<uint32_t>(SamsUtilities::UTimer::GetAbsolute() / 1000);
            }
            if ((gFirstPersonFlags & kFirstPersonAny) != 0)
                gWasFirstPerson = 1;
            else
                gWasFirstPerson = 0;
            if (gWhiteOutTime > 0) {
                STATIC_WhiteOut();
                const uint32_t now = static_cast<uint32_t>(SamsUtilities::UTimer::GetAbsolute() / 1000);
                gWhiteOutTime = gWhiteOutTime - static_cast<int32_t>(now - gWhiteOutClock);
                gWhiteOutClock = static_cast<uint32_t>(SamsUtilities::UTimer::GetAbsolute() / 1000);
            } else {
                EngineTick2();
            }
            CoasterTickPostRender();
        }
        if (gScreenshotMovie != 0) GAMESYSTEM_Screenshot();
        gTurnsThisFrame = 0;
        EngineTick3();
        switch (GS_GetCurrentGameStatus()) {
        case GS_RESTART_LEVEL:
            gGameSystemState = 13;
            break;
        case GS_LEAVE_PARK:
        case GS_QUIT_GAME:
            gGameSystemState = 11;
            break;
        default:
            break;
        }
        if (TheGameType().mType == kGameTypeOnline && OnlineControl()->WantToShutDown()) gGameSystemState = 11;
        break;
    }

    case 11:   // leave the park
        if (TheGameType().mType != kGameTypeOnline) gLoadSave.Save(gTPGlobals.mAutosaveName);
        UIMain_GetFullyOutOfFirstPerson();
        gei_MapWho->SetFinalCallback(nullptr);
        gei_TCache->Flush(0);
        gei_MapWho->ResetAll();
        if (TheGameType().mType == kGameTypeOnline)
            gUIInterface.DeInitOnline();
        else
            gUIInterface.DeInitGame();
        gGame.ExitLevel();
        LevelDeInit(false);
        SCR_CloseScriptSystem();
        if (GS_GetCurrentGameStatus() != GS_QUIT_GAME && (gGameOptions & kGameOptionFrontEnd) != 0)
            gGameSystemState = OnlineSystem()->mFTPParkPending != 0 ? 9 : 1;
        else
            gGameSystemState = 12;
        SOUND_FadeOutAllSfx();
        SND_SetChatterLevel(0);
        SND_StopParkMusic();
        SND_SetRainLevel(0);
        SND_FreeLocalParkSounds();
        PAR_CloseParticleSystem();
        RSSE_Close();
        FLY_Close();
        if (TheGameType().mType == kGameTypeOnline) {
            delete gOnlineControl;
            gOnlineControl = nullptr;
            if (OnlineSystem()->mFTPParkPending == 0) {
                if (MasterSave()->IsEasyModeUser())
                    TheGameType().SetGameType(kGameTypeEasy);
                else
                    TheGameType().SetGameType(kGameTypeNormal);
            }
        }
        break;

    case 12:   // quit
        if (UserInfo()->mGameUser != -1) {
            UserInfo()->GameLogout();
            delete gOnlineControl;
            gOnlineControl = nullptr;
        }
        SND_FreeGlobalSounds();
        gTPWFlags |= 2;
        delete gMessageCentre;
        gMessageCentre = nullptr;
        delete gMasterSave;
        gMasterSave = nullptr;
        delete gOnlineSystem;
        gOnlineSystem = nullptr;
        delete gUserInfo;
        gUserInfo = nullptr;
        delete gMouse;
        gMouse = nullptr;
        delete gOnlineControl;
        gOnlineControl = nullptr;
        break;

    case 13:   // restart the level
        GameSys_RestartLevelDeInit();
        gGameSystemState = 14;
        break;

    case 14:
        GameSys_RestartLevelReInit();
        gGameSystemState = 10;
        GS_SetCurrentGameStatus(GS_PLAYING);
        break;

    default:
        break;
    }
    return result;
}

// 0x10236548
void GAMESYSTEM_Close() {
    if (gGameSystemInitialised == 0) return;
    if (gGameSystemState == 10) {
        gGameSystemState = 11;
        GAMESYSTEM_Tick();
    }
    ControlDeInit();
    gUIInterface.DeInitGeneral();
    DeInitialiseKeyTables();
    MOVIE_Close();
    SOUND_StopAllSfx();
    SND_FreeGlobalSounds();
    SND_FreeLocalParkSounds();
    SOUND_Close();
    PAR_CloseParticleSystem();
    RSSE_Close();
    FLY_Close();
    ADV_Close();
    DeInitialisePersistentSystems();
}

// 0x10236624: captures the next frame to the first free ScrNNNNN.tga.
void GAMESYSTEM_Screenshot() {
    bfl::FileSystem::CDiscFile file;
    char name[20];
    do {
        file.Close();
        ++gScreenshotNumber;
        std::snprintf(name, sizeof(name), "Scr%05ld.tga", static_cast<long>(gScreenshotNumber));
    } while (file.Open(name, 0x80000001).Succeeded());
    file.Close();
    gei_Capture.SetCapture(name);
}

// 0x10236710: nonzero captures every park frame (and runs the clock at a fixed step).
void GAMESYSTEM_ScreenshotMovie(int32_t on) { gScreenshotMovie = on; }

// 0x10236750
void GAMESYSTEM_SetHudState(int32_t state) { gHudState = state; }

// 0x10236e6c
void GS_SetCurrentGameStatus(GS_GAME_STATES status) { gCurrentGameStatus = status; }

// 0x10236eb8
GS_GAME_STATES GS_GetCurrentGameStatus() { return gCurrentGameStatus; }

// 0x10236ef4: restarts the park's tick clocks.
void init_time() {
    gTickTime = gClock.Read();
    gLastFlyTime = gClock.Read();
    gLastTurnTime = gClock.Read();
    gTickCount = 0;
    gGameSys_103bc178 = 0;
}

// 0x10236f8c
void InitialiseWeather() {
    if (gei_Rain != nullptr) {
        // the original reads gei_Rain after each FindFilePath
        char* path = FindFilePath("Data:Generic:Weather:snow.tga", 1, nullptr);
        gei_Rain->Load(2, path);
        path = FindFilePath("Data:Generic:Weather:raindrop.tga", 1, nullptr);
        gei_Rain->Load(1, path);
        path = FindFilePath("Data:Generic:Weather:lightning.tga", 1, nullptr);
        gei_Rain->Load(3, path);
    }
}

// 0x1023706c
void InitialiseBlobShadow() {
    if (gei_SpriteManager != nullptr)
        gei_SpriteManager->LoadBlobShadow(const_cast<char*>("data:generic:shadow:alphkid.tga"));
}

// 0x102370d4: the systems that outlive a park (InitialiseWeather and InitialiseBlobShadow
// are inlined in the original), loaded with a disc file storage in place.
void InitialisePersistentSystems() {
    bfl::FileSystem::CDiscFileStorage storage;
    InitialiseWeather();
    InitialiseBlobShadow();
    CInGame::Init();
}

// 0x1023721c
void DeInitialisePersistentSystems() {}

// 0x10237254: the second half of a level restart: reloads the "restart" save.
int32_t GameSys_RestartLevelReInit() {
    ADV_Kill(1);
    gLoadSave.LoadHidden(gTPGlobals.mRestartName);
    return 1;
}

// 0x102372c8
int32_t GameSys_RestartLevelDeInit() { return 1; }
