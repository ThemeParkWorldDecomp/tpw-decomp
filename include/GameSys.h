// GameSys.cpp (main app 0x10233de0..0x10237770): the game system. See src/game/GameSys.cpp.
//
// GAMESYSTEM_Tick's states (gGameSystemState; the numbers are the original's, the
// descriptions are ours):
//   1  set up the lobby (splash, advisor, particles, CLBLobby), then 2
//   2  one lobby frame, until the lobby's state leaves 1, then 3
//   3  close the lobby: 9 when it chose a park (CLBLobby::Close returned 2), else 12
//   4  front end start, then 5 (GAMESYSTEM_Init picks 4 when the front end is on, else 9)
//   5  start the Bullfrog logo movie (6), or 7 when a skip input is down
//   6  the logo movie, until it ends or is skipped, then 7
//   7  start a random intro movie (8), or 1 when skipped
//   8  the intro movie, until it ends or is skipped, then 1
//   9  load the park, then 15 (on failure the state stays and the tick returns 2, which
//      ThemeParkWorld::Process takes as a quit request)
//  10  one park frame
//  11  leave the park: 1 (the lobby), 9 (a downloaded online park) or 12
//  12  quit: frees the game-wide objects and sets gTPWFlags bit 2
//  13  restart the level, then 14; 14  reload it, then 10
//  15  park loaded: saves the restart point (not online), then 10
//   0 and anything above 15: nothing
#pragma once
#include <cstdint>

#include "Game.h"
#include "TbDynamicStringTemplate.h"

// The park's status, which the park code sets to leave the park (names invented).
enum GS_GAME_STATES : int32_t {
    GS_PLAYING = 0,
    GS_RESTART_LEVEL = 1,   // GAMESYSTEM_Tick goes to 13
    GS_LEAVE_PARK = 2,      // to 11
    GS_QUIT_GAME = 3,       // to 11, then 12
};

bool UseSmallTextures();                 // 0x10233de0
void GAMESYSTEM_Init();                  // 0x10233e78
void PlayMeTheLogoAnim();                // 0x102341dc
void PlayMeARandomFMV();                 // 0x1023436c
void PostCullOperations();               // 0x102345c8
// 0x102346d0: one frame of the state machine. 2 when loading a park failed, else 0; the
// caller ORs it into gTPWFlags (bit 2 asks ThemeParkWorld::Process to quit).
uint32_t GAMESYSTEM_Tick();
void GAMESYSTEM_Close();                 // 0x10236548
void GAMESYSTEM_Screenshot();            // 0x10236624
void GAMESYSTEM_ScreenshotMovie(int32_t on);   // 0x10236710
void GAMESYSTEM_SetHudState(int32_t state);    // 0x10236750
void GS_SetCurrentGameStatus(GS_GAME_STATES status);   // 0x10236e6c
GS_GAME_STATES GS_GetCurrentGameStatus();              // 0x10236eb8
void init_time();                        // 0x10236ef4
void InitialiseWeather();                // 0x10236f8c
void InitialiseBlobShadow();             // 0x1023706c
void InitialisePersistentSystems();      // 0x102370d4
void DeInitialisePersistentSystems();    // 0x1023721c
// The original calls these three without a TOC-restore slot, as it does for functions it
// knows are local; they may have been static.
int32_t GameSys_RestartLevelReInit();    // 0x10237254, returns 1
int32_t GameSys_RestartLevelDeInit();    // 0x102372c8, returns 1
void CreateRestartParkSaveGame();        // 0x10237304, untranslated

// GameSys.cpp's globals (original names unknown).
extern GS_GAME_STATES gCurrentGameStatus;   // 0x103bc164
extern int32_t gScreenshotNumber;           // 0x103bc168, the last ScrNNNNN.tga tried
extern int32_t gFirstParkFrame;             // 0x103bc16c, 1 from loading a park to its first PostCullOperations
extern int32_t gRestartParkClocks;          // 0x103bc170, set by CSaveLoad::PostLoadOperations
extern uint8_t gWasFirstPerson;             // 0x103bc174, the first-person state of the last park frame
extern int32_t gGameSys_103bc178;           // 0x103bc178, zeroed with the clocks, saved with the park
extern int32_t gTickCount;                  // 0x103bc17c, park ticks (31 ms) since the clocks started
extern int32_t gTickNow;                    // 0x103bc180, the clock when the ticks were last run
extern int32_t gLastTurnTime;               // 0x103bc184, tick time of the last game turn (every 8th)
extern int32_t gLastFlyTime;                // 0x103bc188, tick time of the last fly and script tick (every 2nd)
extern int32_t gTickTime;                   // 0x103bc18c, how far the ticks have got
extern int32_t gGameSystemState;            // 0x103bc190, GAMESYSTEM_Tick's state
extern int32_t gHudState;                   // 0x103bc194
extern int32_t gScreenshotMovie;            // 0x103bc198
extern int32_t gRightButtonFrames;          // 0x103bc19c
extern int32_t gLeftButtonFrames;           // 0x103bc1a0
extern int32_t gGameSystemInitialised;      // 0x103bc1a4
extern CCommandLine gCommandLine;           // 0x103bc1a8
extern uint32_t gTurnsThisFrame;            // 0x103bc3bc, game turns run this frame (at most 3)
extern int32_t gLastFrameClock;             // 0x1034c18c
extern uint32_t gFrameTime;                 // 0x1034c190, clock time of the last frame (unsigned:
                                            // the FPS counter converts it as unsigned)
extern uint32_t gWhiteOutClock;             // 0x1034c194, milliseconds (UTimer) of the white-out's last step
extern int32_t gWhiteOutTime;               // 0x1034c198, milliseconds of white-out left
// The strings __sinit_GameSys_cpp constructs.
extern TbDynamicStringTemplate<char16_t> gGameSysUpload;     // 0x103bc620
extern TbDynamicStringTemplate<char> gGameSysLanguages[16];  // 0x103bc420
extern TbDynamicStringTemplate<char> gGameSysTitle;          // 0x103bc400
extern TbDynamicStringTemplate<char> gGameSysLanguage;       // 0x103bc3e0
extern TbDynamicStringTemplate<char16_t> gGameSysGmsFile;    // 0x103bc3c0

// Unit v5151 (src/game/GameSys.cpp): the clocks in a park file, nine big-endian words each.
int32_t GAMESYSTEM_Save(void* file);     // 0x1023678c, 1 when every write transferred 4 bytes, else 0
int32_t GAMESYSTEM_Load(void* file);     // 0x10236b30, 1 when every read transferred 4 bytes, else 0
