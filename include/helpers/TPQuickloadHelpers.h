// TPQuickloadHelpers.h: file-local helpers that src/game/save/TPQuickload.cpp shares with TPRide.cpp. Before the
// regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now lives
// here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "FS_System.h"
#include "Quickload.h"
#include "TPEngine.h"

static inline sQuickloadIndex* IndexOf(int32_t path, bool textures) {
    return textures ? &gQuickloadTextureIndexes[path] : &gQuickloadIndexes[path];
}

// A whole file in a new block, its size in *size; NULL when it does not open (*size
// untouched) or the block cannot be allocated. Inlined in LoadQuickloadIndexes.
static inline void* LoadWholeFile(const char* name, uint32_t* size) {
    bfl::FileSystem::CFile* file = TPFile_Open(name, "r");
    if (file == nullptr) return nullptr;
    const uint32_t length = static_cast<uint32_t>(TPFile_Size(file));
    void* data = MReleaseAllocF(length, 1);
    if (data != nullptr) TPFile_Read(data, length, 1, file);
    TPFile_Close(file);
    *size = length;
    return data;
}
