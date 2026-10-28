// log_file.h: the launcher's log file, beside the platform log on stderr (platform.h LogLine). One file per run,
// <data folder>/logs/launcher-YYYYMMDD-HHMMSS.log, opened by the launcher before anything else logs; the launcher and
// the game are one process after Play, so the file holds both. It starts with a header (the launcher's version, the
// date, the system and the hardware, the command line and the file's own path) and every line is written straight to
// the file as it comes (no buffer), so a crash or an abort loses nothing written before it. The ten most recent files
// are kept. A player sends the About box's Export logs file (log_export.h), which holds the newest of them.
//
// Nothing that names the player or the computer reaches the file: every byte goes through log_scrub.h on its way (the
// home folder is ~, the account name <user>, the computer name <computer>), so the file is safe to send as it is.
//
// No game headers here: the platform files that report a failure on their own (msl.cpp's assertion, arena.cpp,
// jpeg_native.cpp) include this file only. The line writers (LogToFile, LogRaw, LogRawF, LogFileF) are defined here,
// inline, with the open file's descriptor: the differential tests' units link one of those platform files without
// log_file.cpp, and there they write to stderr only, as before.
#pragma once
#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <io.h>   // Windows: _write
#else
#include <unistd.h>
#endif

#include "log_scrub.h"

namespace tpw {

// The open log file's descriptor (-1: none). One per program (an inline variable); OpenLogFile sets it.
inline std::atomic<int> gLogFileFd{-1};

// How many launcher-*.log files a folder keeps (the newest, by name; the one just opened among them).
inline constexpr size_t kLogFilesKept = 10;

// Makes `folder`, opens a new launcher-YYYYMMDD-HHMMSS.log in it (a -2, -3, ... before .log when a run in the same
// second has that name), writes the header and removes all but the kLogFilesKept newest launcher-*.log files there.
// `firstLine` is the header's first line (the launcher's name, version and build). False, with `err` saying why, when
// the folder or the file cannot be made; there is then no log file and the lines go to stderr only.
bool OpenLogFile(const std::filesystem::path& folder, const std::string& firstLine, int argc, const char* const* argv,
                 std::string& err);
// The open log file's full path; empty when there is none.
const std::string& LogFilePath();
// The open log file's descriptor (-1: none): for a crash handler, which may only write(2).
inline int LogFileDescriptor() { return gLogFileFd.load(); }
// Closes the log file (the tests'; the launcher leaves it open until the process ends).
void CloseLogFile();

// Removes all but the `keep` newest launcher-*.log files of `folder` (newest by name: the names hold the time), and
// nothing else. The number removed.
size_t RemoveOldLogs(const std::filesystem::path& folder, size_t keep);
// The newest launcher-*.log file of `folder`, the one Show logs selects: the latest modification
// time wins, and files with the same time (or none that can be read) go by name, the later name first. Files that are not
// launcher-*.log files, and folders, are ignored. Its full path, or an empty path when the folder has none or cannot be read.
std::filesystem::path NewestLog(const std::filesystem::path& folder);
// "launcher-20261022-194629.log" for that local time.
std::string LogFileName(std::time_t when);
// The header's lines after the first, each ending in a newline: date, os, hardware, args and log (`path`).
std::string LogHeaderLines(int argc, const char* const* argv, const std::string& path);

// `length` bytes of `text` to the descriptor, scrubbed (log_scrub.h): the way every byte reaches the log file. Safe in a
// signal handler: it takes no lock and allocates nothing.
void WriteScrubbed(int fd, const char* text, size_t length);
// `a`, then `b`, then a newline, to the log file only (nothing when none is open), scrubbed, in one write. Safe in a signal
// handler: it takes no lock and allocates nothing.
void LogToFile(const char* a, const char* b);
// A whole line to both: stderr exactly as given (with a newline after it, whatever Config::logToStderr says, as the
// fprintf it replaces did) and the log file. For the platform's and the launcher's own reports of a failure.
void LogRaw(const char* line);
// The same as LogRaw with printf formatting; one trailing newline of the text is dropped (the line gets its own).
void LogRawF(const char* format, ...) __attribute__((format(printf, 1, 2)));
// A line for the log file only, printf style (stderr stays as it was): what the file adds for a bug report, such as
// the screens, the edition Play started, the game's size and how the run ended.
void LogFileF(const char* format, ...) __attribute__((format(printf, 1, 2)));

// Once SDL's video has started: the log file's "display:" line about the screens (how many, and each one's name, size
// in points, pixel scale and refresh rate, the primary one marked). Said once per run; nothing before video starts.
void LogDisplayOnce();

inline void WriteScrubbed(int fd, const char* text, size_t length) {
    const ScrubIdentity* identity = gScrubIdentity.load(std::memory_order_relaxed);
    char out[4096];
    size_t at = 0;
    while (at < length) {
        size_t written = 0;
        at = ScrubChunk(identity, text, length, at, out, sizeof out, &written);
#ifdef _WIN32
        // Windows: _write takes an int count; the C runtime holds the descriptor's lock for each call, so lines from two
        // threads do not interleave. A short write (a full disk) is left as it is. Windows has no EINTR.
        (void)_write(fd, out, static_cast<unsigned>(written));
#else
        // One write on a descriptor opened for appending: lines from two threads do not interleave. A short write (a full
        // disk) is left as it is.
        while (write(fd, out, written) < 0 && errno == EINTR) {
        }
#endif
    }
}

inline void LogToFile(const char* a, const char* b) {
    const int fd = gLogFileFd.load(std::memory_order_relaxed);
    if (fd < 0) return;
    // The two parts are joined in a buffer on the stack (nothing allocated, as a signal handler needs), scrubbed (log_scrub.h)
    // and written in one call. A line longer than the buffer goes in three, which another thread's line can come between.
    const size_t lengthA = std::strlen(a), lengthB = std::strlen(b);
    char line[2048];
    if (lengthA + lengthB < sizeof line) {
        std::memcpy(line, a, lengthA);
        std::memcpy(line + lengthA, b, lengthB);
        line[lengthA + lengthB] = '\n';
        WriteScrubbed(fd, line, lengthA + lengthB + 1);
    } else {
        WriteScrubbed(fd, a, lengthA);
        WriteScrubbed(fd, b, lengthB);
        WriteScrubbed(fd, "\n", 1);
    }
}

inline void LogRaw(const char* line) {
    LogToFile("", line);
    std::fprintf(stderr, "%s\n", line);
}

inline void LogRawF(const char* format, ...) {
    char text[2048];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof text, format, args);
    va_end(args);
    const size_t length = std::strlen(text);
    if (length > 0 && text[length - 1] == '\n') text[length - 1] = '\0';
    LogRaw(text);
}

inline void LogFileF(const char* format, ...) {
    if (gLogFileFd.load(std::memory_order_relaxed) < 0) return;
    char text[2048];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof text, format, args);
    va_end(args);
    LogToFile("", text);
}

}  // namespace tpw
