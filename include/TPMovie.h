// The QuickTime movie player (MOVIE_*). The original file name is unknown: the
// inventory files this range under rsse.cpp.
#pragma once
#include <cstdint>

#include "platform/interfacelib.h"
#include "platform/quicktime.h"

// 0x100b9914: starts the movie file `path` in `window`, in a box of the screen's width
// and 352/480 of its height with its top left at the global point (x, y); 1 when it
// plays, 0 when anything fails (`flags` is not used).
int32_t MOVIE_Play(const char* path, GrafPort* window, int32_t x, int32_t y, int32_t flags);
int32_t MOVIE_Tick();   // 0x100b9bcc: gives the movie time; nonzero when it has finished (or there is none)
void MOVIE_Stop();      // 0x100b9c48: stops and disposes of the movie
void MOVIE_Init();      // 0x100b9d10: EnterMovies
void MOVIE_Close();     // 0x100b9d58: MOVIE_Stop's work, then ExitMovies
extern Movie gMovie;    // 0x102e49ec (name invented): the movie playing, or NULL
void MOVIE_SetVolume(uint32_t volume);   // 0x100b9cbc: 0 .. 0x3ff
extern uint32_t gMovieVolume;            // 0x1029eb88 (name invented)
