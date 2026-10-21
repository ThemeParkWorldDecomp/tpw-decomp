// The quickload indexes (main app 0x1006736c..0x10068248; src/game/save/TPQuickload.cpp). The
// inventory files this range under TPRide.cpp; the original file name is unknown. For
// each search path, the file names of the current mesh folder and of its theme texture
// folder, read from their qickload.txt files, so lookups need no disk access.
#pragma once
#include <cstdint>

// One index (12 bytes on the Mac).
struct sQuickloadIndex {
    uint32_t mCount;     // 0x00
    char** mNames;       // 0x04  mCount names, pointing into mText
    char* mText;         // 0x08  the names, each NUL-terminated
};

// 0x1006736c: allocates the (empty) indexes, one pair per search path.
void InitialiseQuickloading();
// 0x10067408: reads the names in `data` (`size` bytes of text) into `index`.
void LoadQuickloadIndex(sQuickloadIndex* index, void* data, uint32_t size);
// 0x100675d8: loads the indexes of `folder` and of its theme texture folder in search path
// `path` (when quickloading is initialised); quickloading is on once one loads.
void LoadQuickloadIndexes(const char* folder, int32_t path);
void FreeQuickloadIndexes();     // 0x100677c8: frees the names; quickloading off
void CloseQuickloading();        // 0x100678c4: frees the indexes (not their names)
// 0x1006795c: 1 when `name` is in the index of search path `path` (textures: the texture
// folder's), 0 when not, -1 when quickloading is off.
int32_t DoesQuickloadFilenameExist(const char* name, int32_t textures, int32_t path);
// 0x10067a50: the first search path whose index has `name`; -1 when none has it or
// quickloading is off.
int32_t FindQuickloadFilename(const char* name, int32_t textures);
// 0x10067b50: 1 when the index of search path `path` has `name`, both without their
// extensions; 0 when not or when quickloading is off.
int32_t CheckIndexedFileExists(const char* name, int32_t path, int32_t textures);
// 0x100680fc: makes `folder` the folder meshes load from. Finds "folder:file" (`file` alone
// without a folder) on the search paths, loads the folder's quickload indexes and passes the
// folder, its theme texture folder and `sharedTextureFolder` to SetNewMeshPaths. 1 on
// success; 0 (traced) when the file is not found.
int32_t SetupDirectoryForMeshLoad(char* file, char* folder, char* sharedTextureFolder);

// Globals (original names unknown).
extern sQuickloadIndex* gQuickloadTextureIndexes;   // 0x102c7994, one per search path
extern sQuickloadIndex* gQuickloadIndexes;          // 0x102c7998, one per search path
extern int32_t gQuickloadInitialised;               // 0x102c799c
extern int32_t gQuickloadEnabled;                   // 0x102c79a0, set when an index loads
extern int32_t gQuickloadPathCount;                 // 0x1029cf08 (1)
