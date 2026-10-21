// Movie.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "FS_System.h"
#include "platform/samsutils.h"
#include "rsse.h"
#include "TPMain.h"
#include "TPMovie.h"


// The movie player's range of the main app (SimTheme Park 2.04b3; the inventory files it
// under rsse.cpp): MOVIE_Play 0x100b9914, MOVIE_Tick 0x100b9bcc, MOVIE_Stop 0x100b9c48,
// MOVIE_SetVolume 0x100b9cbc, MOVIE_Init 0x100b9d10, MOVIE_Close 0x100b9d58, and their
// neighbour CheckFileExists (0x100b98a4). One movie at a time, played by QuickTime into the
// game's window.

Movie gMovie;   // 0x102e49ec (name invented)

uint32_t gMovieVolume = 0x100;   // 0x1029eb88 (name invented): 0 .. 0x100, full at start (the image's word)

// Every step that fails reports its error and ends in "bailed out"; MOVIE_Close then
// leaves QuickTime (ExitMovies) although nothing enters it again before the next movie.
int32_t MOVIE_Play(const char* path, GrafPort* window, int32_t x, int32_t y, int32_t) {
    unsigned char name[256];
    FSSpec spec;
    bool targetIsFolder;
    bool wasAliased;
    int16_t file;
    SamsUtilities::UString::c2pcpy(name, path);
    if (gMovie != nullptr) MOVIE_Close();
    OSErr error = FSMakeFSSpec(0, 0, name, &spec);
    if (error == 0) error = ResolveAliasFile(&spec, true, &targetIsFolder, &wasAliased);
    if (error == 0) error = OpenMovieFile(&spec, &file, 1);
    if (error == 0) {
        int16_t resource = 0;
        error = NewMovieFromFile(&gMovie, file, &resource, nullptr, 1, nullptr);
    }
    if (error != 0) {
        SamsUtilities::UDebug("bailed: error %d", error);
        SamsUtilities::UDebug("bailed out");
        MOVIE_Close();
        return 0;
    }
    Rect box;
    GetMovieBox(gMovie, &box);
    SetPort(window);
    ShowWindow(window);
    ValidRect(&box);
    SetMovieGWorld(gMovie, window, GetGWorldDevice(window));
    SamsUtilities::URectangleMac place(x, y, x + gScreenWidth,
                                       y + static_cast<int32_t>(static_cast<uint32_t>(gScreenHeight * 0x160) / 0x1e0));
    place.ToLocal(window);
    SetMovieBox(gMovie, place);
    SetMovieVolume(gMovie, static_cast<int16_t>(gMovieVolume));
    for (int32_t i = 0; i < 10; ++i) MoviesTask(gMovie, 0);
    const TimeValue time = GetMovieTime(gMovie, nullptr);
    const Fixed rate = GetMoviePreferredRate(gMovie);
    PrerollMovie(gMovie, time, rate);
    SetMovieRate(gMovie, rate);
    return 1;
}

int32_t MOVIE_Tick() {
    if (gMovie == nullptr) return 1;
    MoviesTask(gMovie, 0);
    return IsMovieDone(gMovie);
}

void MOVIE_Stop() {
    if (gMovie != nullptr) {
        StopMovie(gMovie);
        DisposeMovie(gMovie);
        gMovie = nullptr;
    }
}

// The game's 0 .. 0x3ff scaled to 0 .. 0x100.
void MOVIE_SetVolume(uint32_t volume) { gMovieVolume = (volume * 0x100) / 0x3ff; }

void MOVIE_Init() { EnterMovies(); }

void MOVIE_Close() {
    if (gMovie != nullptr) {
        StopMovie(gMovie);
        DisposeMovie(gMovie);
        gMovie = nullptr;
    }
    ExitMovies();
}
