// log_file.h: the launcher's log file, beside the platform log on stderr (platform.h LogLine). ONE file,
// <data folder>/logs/launcher.log on every system, that every run adds to: the launcher's, and the game's own process
// (launcher/game_app.h, which writes there too while the launcher that started it may still be running). Each run starts
// with a run header line ("==== run 12 | the date | the launcher's version and build | the system | ... ====") and the
// lines below it (the date, the system and the hardware, the command line and the file's path); every line is written
// straight to the file as it comes (no buffer), so a crash or an abort loses nothing written before it, and the crash
// report goes in under its run. On Windows what the processes print (SDL's messages, a failed assertion) goes into the
// same file (host_win.h CaptureOutputToLog). A player sends the About box's Export logs file (log_export.h), made from it.
//
// The file keeps the newest kLogKeepBytes: it is cut at a launch only, never while a run writes, before the new run's
// header, at a run header (whole runs go), or at a line when the newest run alone is longer. A launch folds the files of
// the launchers before this one in too (logs/launcher-YYYYMMDD-HHMMSS.log, one per run, and Windows' launcher.log and
// game.log beside the logs folder), oldest first, and removes them.
//
// Several processes write it at once (the launcher and the game's own process, the launcher a game reopens): each line goes
// in one write that the system appends at the file's end (O_APPEND; on Windows a handle that may only append), so lines
// never overwrite or split one another. Each writer holds a shared lock on the file for its life, and the cut and the
// folding are done only under the exclusive lock, which a launch takes only when no other process has the file open: when
// it cannot, it leaves the file as it is this time (the next launch cuts it). The game's own process never cuts it.
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
#include <vector>

#ifdef _WIN32
#include <io.h>   // Windows: _write
#else
#include <unistd.h>
#endif

#include "log_scrub.h"

namespace tpw {

// The open log file's descriptor (-1: none). One per program (an inline variable); OpenLogFile sets it.
inline std::atomic<int> gLogFileFd{-1};
// Whether stderr itself goes into the log file (Windows, started from Explorer: host_win.h CaptureOutputToLog). The lines
// that the log file takes anyway are then not printed to stderr too, which would put them in twice.
inline std::atomic<bool> gStderrIsLogFile{false};

// The log file's name, in the data folder's logs/ on every system.
inline constexpr const char* kLogFileName = "launcher.log";
// How much the file keeps at a launch: its newest 5 MB (in bytes of 1,000).
inline constexpr size_t kLogKeepBytes = 5000000;
// How every run header line starts; the run's number follows.
inline constexpr const char* kRunHeaderStart = "==== run ";

// Makes `folder` and opens its launcher.log for appending (made when missing). A launcher (not the game's own process,
// whose command line has --from-launcher) that gets the file's exclusive lock first folds the old log files in
// (OldLogFiles), oldest first, cuts the file to its newest kLogKeepBytes (TrimLog) and removes the old files. Then the run's
// header (RunHeaderLine, LogHeaderLines, and a line for what the launch did to the file). `firstLine` is the launcher's name,
// version and build. False, with `err` saying why, when the folder or the file cannot be made; there is then no log file
// and the lines go to stderr only.
bool OpenLogFile(const std::filesystem::path& folder, const std::string& firstLine, int argc, const char* const* argv,
                 std::string& err);
// The open log file's full path; empty when there is none.
const std::string& LogFilePath();
// The open log file's descriptor (-1: none): for a crash handler, which may only write(2).
inline int LogFileDescriptor() { return gLogFileFd.load(); }
// This run's number in the file (0: no file open).
int LogRunNumber();
// " [run 12]", what this run's exit lines end with, so that each says whose it is in a file that two processes write
// ("" when no file is open). Safe in a signal handler: a buffer made when the file opened.
const char* LogRunTag();
// Closes the log file and lets go of its lock (the tests'; the launcher leaves it open until the process ends).
void CloseLogFile();

// The run header line, with no newline: "==== run 12 | 2026-10-28 14:03:22 -03:00 | <firstLine> | <system> |
// <edition, when known> | <program> | process 4711 ====". `edition` and `program` may be empty (left out).
std::string RunHeaderLine(int run, std::time_t when, const std::string& firstLine, const std::string& system,
                          const std::string& edition, const std::string& program, long process);
// The run number of a run header line (a line starting with kRunHeaderStart and a number); 0 when it is not one.
int RunNumberOf(const std::string& line);
// The highest run number of the run header lines in `text` (0: none).
int HighestRunNumber(const std::string& text);
// What `text` keeps of a log file at most `keep` bytes long: all of it when it fits; else from the first run header at
// or after its last `keep` bytes; and when there is none there (the newest run alone is longer), from the start of a line,
// with that run's header line and a line saying how much of the run was cut put back before it, all within `keep`.
// `cut` (when not null) is told how many bytes of `text` went.
std::string TrimLog(const std::string& text, size_t keep, size_t* cut = nullptr);
// The log files of the launchers before this one in `logsFolder`, oldest first (by modification time, then by name):
// every launcher-YYYYMMDD-HHMMSS.log and launcher-YYYYMMDD-HHMMSS-N.log there, and the launcher.log and game.log that
// Windows wrote beside it (in the folder above `logsFolder`). Nothing else: the names are matched exactly, and only
// files.
std::vector<std::filesystem::path> OldLogFiles(const std::filesystem::path& logsFolder);
// Whether `name` is an old run's file name: launcher-YYYYMMDD-HHMMSS.log, or with a -N (1 to 99) before .log.
bool IsOldRunLogName(const std::string& name);
// The text an old log file becomes in the one file: a run header ("==== run N | its date | its first line | from the old
// file NAME ====") and its lines (scrubbed with `identity`, null: the generic rules; at most its last `keep` bytes, from a
// line's start), ending in a newline.
std::string FoldedOldLog(const std::filesystem::path& file, int run, size_t keep, const ScrubIdentity* identity);
// The header's lines after the run header line, each ending in a newline: date, os, hardware, args and log (`path`).
std::string LogHeaderLines(int argc, const char* const* argv, const std::string& path);

// `length` bytes of `text` to the descriptor, scrubbed (log_scrub.h): the way every byte reaches the log file. Safe in a
// signal handler: it takes no lock and allocates nothing.
void WriteScrubbed(int fd, const char* text, size_t length);
// `a`, then `b`, then a newline, to the log file only (nothing when none is open), scrubbed, in one write. Safe in a signal
// handler: it takes no lock and allocates nothing.
void LogToFile(const char* a, const char* b);
// A whole line to both: stderr exactly as given (with a newline after it, whatever Config::logToStderr says, as the
// fprintf it replaces did) and the log file. For the platform's and the launcher's own reports of a failure. stderr is
// left out when it goes into the log file itself (gStderrIsLogFile).
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
        // threads do not interleave, and the handle may only append (log_file.cpp), so the system puts each write at the
        // file's end, whichever process wrote last. A short write (a full disk) is left as it is. Windows has no EINTR.
        (void)_write(fd, out, static_cast<unsigned>(written));
#else
        // One write on a descriptor opened for appending: lines from two threads, or two processes, do not interleave. A
        // short write (a full disk) is left as it is.
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
    if (gStderrIsLogFile.load(std::memory_order_relaxed) && gLogFileFd.load(std::memory_order_relaxed) >= 0) return;
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
