// LbLog.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "bullfrog_CPU.h"
#include "LbExeKey.h"
#include "LbFile.h"
#include "LbList.h"
#include "LbLog.h"
#include "LbString.h"
#include "LbTime.h"
#include "platform/interfacelib.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

// LbExeKey.cpp (decompiled from the bullfrog library of SimTheme Park 2.04b3, functions
// at 0x10014bf0..0x10014c50; original file name unknown, see LbExeKey.h).

TbExeKey gLbExeKey = {0x890cd511u, 0, 0xe92a80c5u};
bool gLbExeKeyInitialised;
uint32_t gLbExeKeyState;

uint32_t LbExeKey_ReferenceNumber() {
    return gLbExeKey.mReferenceNumber;
}

// The first call clears a word and sets a guard byte: the code CodeWarrior emits for the
// dynamic initialisation of a function-local static (presumably an object whose
// constructor zeroes its one field). Nothing in the library reads the word.
void LbExeKey_Initialise() {
    if (gLbExeKeyInitialised) return;
    gLbExeKeyState = 0;
    gLbExeKeyInitialised = true;
}

// LbList.cpp (decompiled from the bullfrog library of SimTheme Park 2.04b3, functions at
// 0x10014c80..0x10014df8; original file name unknown, see LbList.h).

namespace {
// The sentinels overlap the header's words (see LbList.h).
TbListNode* HeadSentinel(const TbList* list) {
    return reinterpret_cast<TbListNode*>(const_cast<TbList*>(list));
}
TbListNode* TailSentinel(const TbList* list) {
    return reinterpret_cast<TbListNode*>(const_cast<TbListNode**>(&list->mTail));
}
}  // namespace

void LbList_Init(TbList* list) {
    list->mHead = TailSentinel(list);
    list->mTail = nullptr;
    list->mTailPrev = HeadSentinel(list);
}

void LbList_Insert(TbList* list, TbListNode* after, TbListNode* node) {
    node->mNext = after->mNext;
    node->mPrev = after;
    after->mNext->mPrev = node;
    after->mNext = node;
}

// Walks every node up to the tail sentinel (the one whose mNext is null).
int32_t LbList_NodeExists(const TbList* list, const TbListNode* node) {
    for (const TbListNode* n = list->mHead; n->mNext != nullptr; n = n->mNext)
        if (n == node) return 1;
    return 0;
}

TbListNode* LbList_GetFirst(const TbList* list) {
    return list->mHead != TailSentinel(list) ? list->mHead : nullptr;
}

TbListNode* LbList_GetLast(const TbList* list) {
    return list->mTailPrev != HeadSentinel(list) ? list->mTailPrev : nullptr;
}

// LbLog.cpp (decompiled from the bullfrog library of SimTheme Park 2.04b3, functions at
// 0x10014e2c..0x10015504; original file name unknown, see LbLog.h).

TbError LbLog_Setup(TbLog* log, const char* filename, uint32_t flags) {
    if (!LbFile_ToProjectFilename(log->mFilename, sizeof log->mFilename, filename).Succeeded())
        return TbError(-1);
    char directory[0x104];
    std::strcpy(directory, log->mFilename);
    char* end = std::strrchr(directory, '\\');
    if (end == nullptr) end = std::strrchr(directory, '/');
    if (end == nullptr) end = std::strrchr(directory, ':');
    if (end == nullptr) return TbError(-1);
    *end = '\0';
    if (!LbFile_IsDirectory(directory)) return TbError(-1);
    log->mFlags = flags;
    log->mInitialised = 1;
    log->mOpened = 0;
    log->mSuspended = 0;
    log->mPrefix[0] = '\0';
    return TbError(0);
}

TbError LbLog(TbLog* log, const char* format, ...) {
    if (!log->mInitialised) return TbError(-1);
    if (!log->mSuspended) {
        int32_t header = 0;   // 1 "CREATED", 2 "APPENDED"
        bool blankLine = false;
        if (log->mOpened == 0 && (log->mFlags & kLogHeader)) {
            if (log->mFlags & kLogOverwrite) {
                header = 1;
            } else {
                blankLine = true;
                header = 2;
            }
        }
        bool overwrite = log->mOpened == 0 && (log->mFlags & kLogOverwrite);
        FILE* file = std::fopen(log->mFilename, overwrite ? "w" : "a");
        if (file == nullptr) return TbError(-1);
        log->mOpened = 1;
        if (header != 0) {
            if (blankLine) std::fprintf(file, "\n");
            std::fprintf(file, "LOG %s", header == 1 ? "CREATED" : "APPENDED");
            bool timed = false;
            if (log->mFlags & kLogHeaderTime) {
                TbTime time;
                LbTime_GetTime(&time);
                std::fprintf(file, "  @ %02d:%02d:%02d", time.mHour, time.mMinute, time.mSecond);
                timed = true;
            }
            if (log->mFlags & kLogHeaderDate) {
                TbDate date;
                LbTime_GetDate(&date);
                std::fprintf(file, " %s%d-%02d-%d", timed ? " " : "  @ ", date.mDay, date.mMonth,
                             date.mYear);
            }
            std::fprintf(file, "\n\n");
        }
        if (log->mFlags & kLogDate) {
            TbDate date;
            LbTime_GetDate(&date);
            std::fprintf(file, "%02d-%02d-%d ", date.mDay, date.mMonth, date.mYear);
        }
        if (log->mFlags & kLogTime) {
            TbTime time;
            LbTime_GetTime(&time);
            std::fprintf(file, "%02d:%02d:%02d ", time.mHour, time.mMinute, time.mSecond);
        }
        // The prefix is the format string, as in the original (a '%' in it is a directive).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
        if (log->mPrefix[0] != '\0') std::fprintf(file, log->mPrefix);
#pragma GCC diagnostic pop
        va_list args;
        va_start(args, format);
        std::vfprintf(file, format, args);
        va_end(args);
        std::fclose(file);
    }
    return TbError(0);
}

TbError LbLog_Close(TbLog* log) {
    if (!log->mInitialised) return TbError(-1);
    log->mInitialised = 0;
    return TbError(0);
}

// A prefix that does not fit is stored truncated and the call fails.
TbError LbLog_SetPrefix(TbLog* log, const char* prefix) {
    if (!log->mInitialised) return TbError(-1);
    if (prefix == nullptr) {
        log->mPrefix[0] = '\0';
    } else if (!LbString_strnzcpy(log->mPrefix, prefix, sizeof log->mPrefix)) {
        return TbError(-1);
    }
    return TbError(0);
}

// Kept as compiled: mOpened is cleared when the delete FAILS, so after a successful
// delete the next LbLog appends to a new file without the header.
TbError LbLog_Delete(TbLog* log) {
    if (!log->mInitialised) return TbError(-1);
    if (LbFile_Delete(log->mFilename).Succeeded()) return TbError(0);
    log->mOpened = 0;
    return TbError(-1);
}

TbError LbLog_Suspend(TbLog* log) {
    if (!log->mInitialised) return TbError(-1);
    log->mSuspended = 1;
    return TbError(0);
}

TbError LbLog_Restart(TbLog* log) {
    if (!log->mInitialised) return TbError(-1);
    log->mSuspended = 0;
    return TbError(0);
}

int32_t LbLog_IsInitialised(TbLog* log) {
    return log->mInitialised;
}

int32_t LbLog_IsSuspended(TbLog* log) {
    return log->mSuspended;
}

// bullfrog_CPU.cpp (decompiled from the bullfrog library of SimTheme Park 2.04b3,
// functions at 0x100156b0..0x10015788; original file name unknown, see bullfrog_CPU.h).

namespace bullfrog {

int32_t gCPUClockMHz;

void CPU::detect() {
    if (gCPUClockMHz == 0) {
        int32_t hz;
        if (Gestalt(0x70636c6b /* 'pclk' */, &hz) == 0) gCPUClockMHz = hz / 1000000;
    }
    BlockMoveData("FunkyPowerPC", mName, 13);
    mType = 6;
    mHasRDTSC = 1;
    mFeatures = 0;
}

int32_t CPU::clocks_per_millisec(int32_t unused) {
    return gCPUClockMHz * 1000;
}

}  // namespace bullfrog
