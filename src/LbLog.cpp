// LbLog.cpp (decompiled from the bullfrog library of SimTheme Park 2.04b3, functions at
// 0x10014e2c..0x10015504; original file name unknown, see LbLog.h).
#include "LbLog.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "LbFile.h"
#include "LbString.h"
#include "LbTime.h"

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
