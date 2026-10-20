// QuickTime's Movie Toolbox (QuickTimeLib), the sixteen routines the game's movie player
// calls directly. The platform layer implements them (platform/quicktime.cpp).
#pragma once
#include <cstdint>

#include "platform/interfacelib.h"

struct MovieRecord;
using Movie = MovieRecord**;      // a handle
using TimeValue = int32_t;
using Fixed = int32_t;            // 16.16; a rate of 0x10000 is normal speed

OSErr EnterMovies();
void ExitMovies();
// OpenMovieFile(spec, refNum, permission): opens the movie file (permission 1: read).
OSErr OpenMovieFile(const FSSpec* spec, int16_t* refNum, int8_t permission);
// NewMovieFromFile(movie, refNum, resId, resName, flags, dataRefWasChanged): the movie of an
// open file (flag 1: active).
OSErr NewMovieFromFile(Movie* movie, int16_t refNum, int16_t* resId, unsigned char* resName, int16_t flags,
                       bool* dataRefWasChanged);
void GetMovieBox(Movie movie, Rect* box);
void SetMovieBox(Movie movie, const Rect* box);
void SetMovieGWorld(Movie movie, GrafPort* port, GDevice** device);
void SetMovieVolume(Movie movie, int16_t volume);          // 8.8: 0x100 is full volume
void MoviesTask(Movie movie, int32_t maxMilliseconds);     // gives the movie time to draw
TimeValue GetMovieTime(Movie movie, void* currentTime);
Fixed GetMoviePreferredRate(Movie movie);
OSErr PrerollMovie(Movie movie, TimeValue time, Fixed rate);
void SetMovieRate(Movie movie, Fixed rate);                // starts the movie
bool IsMovieDone(Movie movie);
void StopMovie(Movie movie);
void DisposeMovie(Movie movie);
