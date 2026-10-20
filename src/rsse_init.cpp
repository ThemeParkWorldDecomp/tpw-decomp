// rsse.cpp range of the main app (SimTheme Park 2.04b3): RSSE_Init (0x100d32e8, 212 bytes) and SetAnimationDetail (0x100c7c68,
// 12 bytes). The script system starts here: its state words are set, and a checksum of the script language's op table is
// made (RSSE_InitScript compares a script file's header with it).
#include "LevelInit.h"

#include <cstring>

#include "Debug.h"
#include "TPGlobals.h"
#include "rsse.h"

// The script ops, as pairs {name, argument count as a digit} (initialised data at 0x1029ec1c: 212 char pointers, 106 ops, ended by a
// pointer to an empty text). Nothing else in the translated code reads it yet. Its characters are all ASCII, so the signed char
// sum below equals an unsigned one.
static const char* const kRSSEOpTable[] = {
    "NOP", "0",
    "CRIT_LOCK", "0",
    "CRIT_UNLOCK", "0",
    "COPY", "2",
    "SETLV", "1",
    "SUB", "3",
    "ENDSLICE", "0",
    "GETTIME", "1",
    "ADDOBJ", "4",
    "ADDOBJ_EXT", "5",
    "KILLOBJ", "1",
    "FADEOBJ", "1",
    "SETOBJPARAM", "3",
    "EVENT", "3",
    "EVENT_EXT", "4",
    "FLUSHANIM", "0",
    "TRIGANIM", "3",
    "WAITANIM", "2",
    "LOOPANIM", "2",
    "TRIGWAITANIM", "3",
    "GETANIM", "1",
    "TRIGANIMSPEED", "4",
    "FLUSHANIM_CH", "1",
    "TRIGANIM_CH", "4",
    "WAITANIM_CH", "3",
    "LOOPANIM_CH", "3",
    "TRIGWAITANIM_CH", "4",
    "GETANIM_CH", "2",
    "RAND", "2",
    "JSR", "1",
    "RETURN", "0",
    "BRANCH", "1",
    "BRANCH_Z", "1",
    "BRANCH_NZ", "1",
    "BRANCH_NV", "1",
    "BRANCH_PV", "1",
    "DBGMSG", "1",
    "NAME", "1",
    "TEST", "1",
    "CMP", "2",
    "PUSH", "1",
    "POP", "1",
    "HUSH", "1",
    "HOP", "1",
    "WAIT", "1",
    "WAITABS", "1",
    "WAIT4ANIM", "0",
    "ADD", "2",
    "MULT", "3",
    "DIV", "3",
    "MOD", "3",
    "TURBO", "1",
    "END", "0",
    "TOUR", "2",
    "BUMP", "2",
    "COAST", "2",
    "ADDHEAD", "1",
    "DELHEAD", "1",
    "LIMBO", "2",
    "UNLIMBO", "1",
    "FORCEUNLIMBO", "1",
    "INLIMBO", "1",
    "LIMBOSPACE", "1",
    "SPAWNCHILD", "1",
    "SPAWNSOUND", "1",
    "REMOVECHILD", "0",
    "SETVARINCHILD", "2",
    "GETVARINCHILD", "2",
    "SETVARINPARENT", "2",
    "GETVARINPARENT", "2",
    "BOUNCESETNODE", "1",
    "BOUNCESETBASE", "1",
    "BOUNCE", "2",
    "UNBOUNCE", "1",
    "FORCEUNBOUNCE", "1",
    "BOUNCING", "1",
    "WALKON", "7",
    "WALKOFF", "1",
    "WALKGET", "1",
    "WALKST_FLOAT", "3",
    "WALKFLOATSTAT", "1",
    "WALKFLOATSTOP", "0",
    "ENABLELIGHT", "1",
    "DISABLELIGHT", "1",
    "SETLIGHT", "2",
    "COLOURLIGHT", "4",
    "STARTSCREAM", "2",
    "STOPSCREAM", "0",
    "SINGLESCREAM", "2",
    "SCREAMLEVEL", "1",
    "FINDSCRIPTRAND", "2",
    "GETREMOTEVAR", "3",
    "SETREMOTEVAR", "3",
    "REPAIREFFECT", "1",
    "GETCUSTPTCLCODE", "2",
    "SETTIMER", "1",
    "GETTIMER", "1",
    "YEAR", "1",
    "MONTH", "1",
    "DAY", "1",
    "HOUR", "1",
    "MIN", "1",
    "SEC", "1",
    "SETREVERB", "1",
    "DIPMUSIC", "1",
    "SPARK", "4",
    "",
};

uint32_t gRSSEScriptChecksum;   // 0x102f7be8
uint32_t gRSSEF7BE4;            // 0x102f7be4

// The first call starts the system: a flag, two words, the list of live scripts (empty) and the checksum (the sum of every
// character of every text of the op table, as signed chars). A second call (before RSSE_Close) only asserts.
void RSSE_Init() {
    if (gRSSEState.mInitialised == 0) {
        gRSSEState.mInitialised = 1;
        gRSSEState.mF08 = 1;
        gRSSEState.mScripts = nullptr;
        gRSSEState.mScriptCount = 0;
        gRSSEState.mF04 = 0;
        gRSSEScriptChecksum = 0;
        gRSSEF7BE4 = 0;
        for (int32_t entry = 0; kRSSEOpTable[entry][0] != '\0'; ++entry) {
            for (uint32_t i = 0; i < std::strlen(kRSSEOpTable[entry]); ++i) {
                gRSSEScriptChecksum += static_cast<uint32_t>(static_cast<int32_t>(static_cast<signed char>(kRSSEOpTable[entry][i])));
            }
        }
    } else {
        GAME_ASSERTMSG(0, "RSSE: Tried to Initialise twice - doh !");
    }
}

// The animation detail level (LevelInit1 passes 0) goes into the word at gTPGlobals + 0x400c, which the headers call the engine
// flags (RideAnimInterp, RideAnimTick and RideAnimChange test its bits): the original overwrites them whole.
void SetAnimationDetail(uint32_t detail) {
    gTPGlobals.mEngineFlags = detail;
}
