// TPConfig.cpp (main app 0x100591a4..0x1005999c, static initialiser 0x100599c8;
// src/TPConfig.cpp): the game's start-up stages, the search path and the data file lookup.
// The inventory's other TPConfig.cpp rows are coaster and layout code filed there by
// address; they are untranslated.
#pragma once
#include <cstdint>

#include "TbDynamicStringTemplate.h"

void InitialiseGameStage1();   // 0x100591a4: the default game options, or the player's custom ones
void InitialiseGameStage2();   // 0x10059254: the sine table
void InitialiseGameStage3();   // 0x100592a8: the search path, the screen size, quickloading
void ClosedownGame();          // 0x10059670
void TPSaveRegistry();         // 0x1005999c (empty in this build)

// The flags of DoesFileExist and FindFilePath.
constexpr uint32_t kFileReturnName = 0x1;       // FindFilePath returns the name, not its search path
constexpr uint32_t kFileQuickload = 0x2;        // look the name's last component up in the quickload index
constexpr uint32_t kFileFallBackToDisk = 0x4;   // ... and on disk when it is not indexed
constexpr uint32_t kFileTextures = 0x8;         // the theme texture folder's index

// 0x10059714: nonzero when `name` exists: in the quickload index of the current mesh search
// path (kFileQuickload; -1 when no index is loaded), else (or also when not indexed, with
// kFileFallBackToDisk) on disk.
int32_t DoesFileExist(char* name, uint32_t flags);
// 0x100597f8: finds `name` in the quickload indexes or on disk. Returns the name copied into
// gFilePathBuffer (kFileReturnName) or the search path it is in, and stores that search
// path's index in *result (-1 when not found); NULL when not found.
char* FindFilePath(const char* name, uint32_t flags, int32_t* result);

// Globals of TPConfig.cpp (original names unknown).
extern char gFilePathBuffer[0x400];   // 0x102c1798, FindFilePath's copy of the name
extern char* gPathString;             // 0x102c1b98, the search path's text
extern char** gPaths;                 // 0x102c1b9c, the search paths (one)
extern int32_t gPathLength;           // 0x102c1ba0, the search path's size with its terminator
// The strings __sinit_TPConfig_cpp constructs (the same set as TPMain.cpp's).
extern TbDynamicStringTemplate<char> gTPConfigLanguages[16];   // 0x102c1be4
extern TbDynamicStringTemplate<char> gTPConfigTitle;           // 0x102c1bc4
extern TbDynamicStringTemplate<char> gTPConfigLanguage;        // 0x102c1ba4
