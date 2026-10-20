// Main.cpp (main app 0x1015139c..0x1015184c): the command line and the control entry
// points GAMESYSTEM_Init, PostCullOperations and GAMESYSTEM_Close call.
#pragma once
#include <cstdint>

// What the command line asked for (name invented; 0x410 bytes at 0x1037f2b4).
struct CommandLineOptions {
    CommandLineOptions() : mLoadFile(0), mF404(0), mFlMouse(0) {}   // __sinit_Main_Cpp, 0x10151834

    int32_t mLoadFile;     // 0x000  set when mFile names a park to load (CGame::InitTheme loads it)
    char mFile[0x400];     // 0x004
    int32_t mF404;         // 0x404  nonzero skips GAMESYSTEM_Tick's save loading; nothing sets it
    int32_t mFlMouse;      // 0x408  "flmouse"
    int32_t mBwCursor;     // 0x40c  "bwcursor" (not set by the constructor)
};
extern CommandLineOptions gCommandLineOptions;   // 0x1037f2b4

// <data path>::levels:<current theme>, in a function-local static string (0x1015139c).
char* get_theme_path();
// 0x1015149c: parses the command line; 1 unless an option was bad (gExitError holds the
// error) or asked for the version.
int32_t ControlInit(int32_t argc, char** argv);
void ControlIterate();   // 0x10151798: CGame::Iterate
void ControlDeInit();    // 0x101517e8: CGame::DeInit
