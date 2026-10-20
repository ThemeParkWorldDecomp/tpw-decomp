// The command line (main app of SimTheme Park 2.04b3; Game.cpp range, 0x10147778..
// 0x1014798c), in a file of its own so a test library can link it without the game object.
#include "Game.h"

#include <cstdlib>

#include "Debug.h"
#include "platform/macdoze.h"

// The Mac build's command line is a constant, and it is empty: the parser below never
// finds an argument there. (The Windows build took its text from the system.) An
// argument ends at a space that is followed by a slash; at most 128 are kept.
CCommandLine::CCommandLine() {
    mText = NS_MacDoze::_strdup("");
    mArgc = 0;
    char* p = mText;
    while (mArgc <= 0x7f && *p != '\0') {
        mArgv[mArgc] = p;
        for (; *p != '\0'; ++p) {
            if (*p == ' ' && p[1] == '/') break;
        }
        if (*p != '\0' && *p == ' ' && p[1] == '/') {
            *p = '\0';
            ++p;
        }
        CTRACE(1, "Arg %d: %s", mArgc, mArgv[mArgc]);
        ++mArgc;
    }
}

CCommandLine::~CCommandLine() { std::free(mText); }

int32_t CCommandLine::GetArgc() { return mArgc; }

char** CCommandLine::GetArgv() { return mArgv; }
