// Main.cpp: the binary spells it Main.Cpp (__sinit_Main_Cpp); the name is proven by that symbol.

#include "ExitErr.h"
#include "FS_System.h"
#include "Game.h"
#include "Main.h"
#include "platform/macdoze.h"
#include "TbDynamicStringTemplate.h"
#include "TPGlobals.h"
#include <cstring>

// Main.cpp (main app 0x1015139c..0x1015184c): the command line and the game's control
// entry points.

CommandLineOptions gCommandLineOptions;

namespace {

// The original repeats this test inline for every switch: '/' or '-', then the name in
// any case.
bool IsSwitch(const char* argument, const char* name) {
    return (argument[0] == '/' || argument[0] == '-') && NS_MacDoze::_stricmp(argument + 1, name) == 0;
}

}  // namespace

// 0x1015139c
char* get_theme_path() {
    static TbDynamicStringTemplate<char> sPath;   // 0x1037f290, guard byte 0x1037f2a4
    sPath = gTPFileInfo.GetDataPath();
    FS_AddDirectorySeparator(sPath);
    FS_AddDirectorySeparator(sPath);
    sPath += gTPGlobals.mLevelsFolder;
    FS_AddDirectorySeparator(sPath);
    sPath += gGame.GetCurrentThemeName();
    return sPath.mData;
}

// 0x1015149c: "v" and "version" stop the game; "quickload", "nodebug", "noload" and
// "SAVEDEBUG" are accepted and ignored; "flmouse" and "bwcursor" set options; the first
// argument that ends in the save extension (".TPWS") is the park to load. Anything else is
// an error.
int32_t ControlInit(int32_t argc, char** argv) {
    for (int32_t i = 1; i < argc; ++i) {
        if (IsSwitch(argv[i], "v")) return 0;
        if (IsSwitch(argv[i], "version")) return 0;
        if (IsSwitch(argv[i], "quickload")) continue;
        if (IsSwitch(argv[i], "nodebug")) continue;
        if (IsSwitch(argv[i], "noload")) continue;
        if (IsSwitch(argv[i], "SAVEDEBUG")) continue;
        if (IsSwitch(argv[i], "flmouse")) {
            gCommandLineOptions.mFlMouse = 1;
            continue;
        }
        if (IsSwitch(argv[i], "bwcursor")) {
            gCommandLineOptions.mBwCursor = 1;
            continue;
        }
        if (gCommandLineOptions.mLoadFile == 0) {
            const char* argument = argv[i];
            const char* extension = gTPGlobals.mSaveExtension.mData;
            const int32_t length = static_cast<int32_t>(std::strlen(argument));
            const int32_t extensionLength = static_cast<int32_t>(std::strlen(extension));
            const bool isSave = length >= extensionLength &&
                                NS_MacDoze::_stricmp(argument + length - extensionLength, extension) == 0;
            if (isSave) {
                gCommandLineOptions.mLoadFile = 1;
                std::strcpy(gCommandLineOptions.mFile, argv[i]);
                continue;
            }
        }
        gExitError.SetError(CExitError::ErrorType(2), 0, argv[i]);
        break;
    }
    return gExitError.mError == 0 ? 1 : 0;
}

// 0x10151798
void ControlIterate() { gGame.Iterate(); }

// 0x101517e8
void ControlDeInit() { gGame.DeInit(); }
