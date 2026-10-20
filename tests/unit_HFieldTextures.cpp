// Test library for src/HFieldTextures.cpp (see tests/test_HFieldTextures.py): the level
// height field's texture table, the mesh's texture arrays and the flatness marks.
//
// The unit's own code is HFieldTextures.cpp. The text reader of TPAdvisor.cpp (GetString
// and the character tests) is real code in this library, as the original's is on the
// PowerPC side: it has no effect but on its cursor and its buffer. Everything else the
// functions call is a fake that records an event: the file functions, the game's
// allocator, the theme and path functions and the case-blind compare. The C library
// (sprintf, strlen, strcpy, strncpy, memcpy, atoi, sqrt) is the host's own.
//
// The test builds the mesh header (a file image: Ptr32 fields, so every block it lies in
// comes from the arena) through tpw_hft_alloc and writes it from Python, with the same
// code for both sides. The blocks the unit's code asks the allocator for come from the
// arena too and are kept (not freed) until the next reset, so that the test can read what
// the table points to after FreeHFieldTextureTable.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <strings.h>
#include <vector>

#include "FS_System.h"
#include "Game.h"
#include "HFieldTextures.h"
#include "Main.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "calllog.h"
#include "fake_strings.h"
#include "platform/arena.h"
#include "platform/macdoze.h"

using namespace calllog;

// ------------------------------------------------------------------ the world the fakes answer from

namespace {

std::string gFileText;          // what the .tct file holds
std::string gFileTail;          // the bytes in the buffer right after the text that is read (zeros if empty)
std::string gThemeNameText;     // CGame::GetCurrentThemeName
std::string gThemePathText;     // get_theme_path
std::string gFoundText;         // FindFilePath(name, 1, ...) returns this ...
bool gHasFound = false;         // ... unless this is false
std::string gSearchText;        // FindFilePath(name, 0, ...) likewise
bool gHasSearch = false;
TbDynamicStringTemplate<char>* gTheme = nullptr;

// Every block of the arena the test or the unit's code asked for, and where each lies, so
// that the test can say where a pointer points to (a label and an offset).
struct Region {
    uintptr_t start;
    size_t size;
    int32_t label;
};
std::vector<Region> gRegions;
std::vector<void*> gBlocks;
char gHandles[8][16];           // the file handles TPFile_Open gives out
int32_t gHandleCount = 0;

void* NewBlock(size_t size, int32_t label) {
    void* block = tpw::ArenaAllocZeroed(size + 16);
    gBlocks.push_back(block);
    gRegions.push_back({reinterpret_cast<uintptr_t>(block), size == 0 ? 1 : size, label});
    return block;
}

}  // namespace

// ------------------------------------------------------------------ globals of other files

CGame::CGame() {}
CGame::~CGame() {}
CGame gGame;

// ------------------------------------------------------------------ callees

const TbDynamicStringTemplate<char>& CGame::GetCurrentThemeName() {
    ev("GetCurrentThemeName", {lab(this)});
    return *gTheme;
}

char* get_theme_path() {
    ev("get_theme_path");
    return const_cast<char*>(gThemePathText.c_str());
}

// FindFilePath(name, flags, result): the name when the flag for it is set (1), the search
// path otherwise. Recorded with the flags and whether a result pointer was passed.
char* FindFilePath(const char* name, uint32_t flags, int32_t* result) {
    ev("FindFilePath", {flags, result != nullptr}, {name});
    if (flags == 1) return gHasFound ? const_cast<char*>(gFoundText.c_str()) : nullptr;
    return gHasSearch ? const_cast<char*>(gSearchText.c_str()) : nullptr;
}

// The file: opened when the queue "Open" says so (default yes), as long as the text.
bfl::FileSystem::CFile* TPFile_Open(const char* name, const char* mode) {
    ev("TPFile_Open", {}, {name, mode});
    if (ret("Open", 1) == 0) return nullptr;
    auto* handle = reinterpret_cast<bfl::FileSystem::CFile*>(gHandles[gHandleCount++ & 7]);
    newlab(handle);
    return handle;
}

int32_t TPFile_Size(bfl::FileSystem::CFile* file) {
    ev("TPFile_Size", {lab(file)});
    return static_cast<int32_t>(ret("Size", static_cast<int64_t>(gFileText.size())));
}

// Reads what there is of the text and zeros after it; the bytes right after what was asked
// for are the tail (the slack of the block: the text reader looks one byte past the end).
uint32_t TPFile_Read(void* destination, uint32_t size, uint32_t count, bfl::FileSystem::CFile* file) {
    ev("TPFile_Read", {size, count, lab(destination), lab(file)});
    const size_t bytes = static_cast<size_t>(size) * count;
    if (bytes != 0) {
        std::memset(destination, 0, bytes);
        std::memcpy(destination, gFileText.data(), bytes < gFileText.size() ? bytes : gFileText.size());
        std::memcpy(static_cast<char*>(destination) + bytes, gFileTail.data(), gFileTail.size());
    }
    return count;
}

uint32_t TPFile_Close(bfl::FileSystem::CFile* file) {
    ev("TPFile_Close", {lab(file)});
    return 0;
}

// The game's allocator: blocks from the arena, zeroed, with a few bytes of room after
// them (the text reader may look one byte past the end of the text it was given). Nothing
// is freed before the next reset. A block of no size is NULL.
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {size, flags});
    if (size == 0) return nullptr;
    void* block = NewBlock(size, 0);
    gRegions.back().label = newlab(block);
    return block;
}

void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {lab(block)});
    return nullptr;
}

int32_t NS_MacDoze::_stricmp(const char* a, const char* b) {
    ev("_stricmp", {}, {a, b});
    return ::strcasecmp(a, b);
}

// ------------------------------------------------------------------ test entry points

extern "C" {

// Clears the world and gives the fakes their answers. found and search may be NULL
// (FindFilePath then finds nothing).
void tpw_hft_reset(const char* theme, const char* themePath, const char* found, const char* search, const char* text,
                   int32_t length, const char* tail, int32_t tailLength) {
    for (void* block : gBlocks) tpw::ArenaFree(block);
    gBlocks.clear();
    gRegions.clear();
    gHandleCount = 0;
    gThemeNameText = theme;
    gThemePathText = themePath;
    gHasFound = found != nullptr;
    gFoundText = gHasFound ? found : "";
    gHasSearch = search != nullptr;
    gSearchText = gHasSearch ? search : "";
    gFileText.assign(text, static_cast<size_t>(length));
    gFileTail.assign(tail, static_cast<size_t>(tailLength));
    delete gTheme;
    gTheme = new TbDynamicStringTemplate<char>(gThemeNameText.c_str());
}

uint8_t* tpw_hft_arena_base() { return tpw::ArenaBase(); }
void* tpw_hft_game() { return &gGame; }

// A zeroed block of the arena for the test's header and arrays, with a label of its own.
void* tpw_hft_alloc(uint32_t size, int32_t label) { return NewBlock(size, label); }

// The label of the region an address lies in, with its offset there; 0 when it is in none.
int32_t tpw_hft_locate(const void* address, int64_t* offset) {
    const uintptr_t p = reinterpret_cast<uintptr_t>(address);
    for (const Region& region : gRegions) {
        if (p >= region.start && p < region.start + region.size) {
            *offset = static_cast<int64_t>(p - region.start);
            return region.label;
        }
    }
    return 0;
}

// The size asked for of the region with this label.
int64_t tpw_hft_size(int32_t label) {
    for (const Region& region : gRegions)
        if (region.label == label) return static_cast<int64_t>(region.size);
    return -1;
}

void* tpw_hft_table() { return &gExtraTextureTable; }
int32_t tpw_hft_table_size() { return static_cast<int32_t>(sizeof(gExtraTextureTable)); }

// The table as the program starts: `count` categories, `bytes` already counted.
void tpw_hft_table_reset(int32_t count, int32_t bytes) {
    gExtraTextureEntries[0] = {"PathTex", 0, nullptr};
    gExtraTextureEntries[1] = {"QueueTex", 0, nullptr};
    gExtraTextureTable = {count, gExtraTextureEntries, 0, 0, nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, nullptr};
    gExtraTextureTable.mBytes = bytes;
}

int32_t tpw_hft_parse(char* file) { return ParseLevelTCT(file, &gExtraTextureTable); }
int32_t tpw_hft_intercept(tag_sM3D2Header* header, char* folder) {
    return InterceptAndInsertM3D2Textures(header, &gExtraTextureTable, folder);
}
void tpw_hft_replace(tag_sM3D2Header* header, int32_t* map, sTexture* textures, int32_t kept, uint32_t total) {
    ReplaceTextures(header, map, textures, kept, total);
}
int32_t tpw_hft_build(tag_sM3D2Header* header) { return BuildHFieldTextureTable(header); }
void tpw_hft_free() { FreeHFieldTextureTable(); }
void tpw_hft_flatness(tag_sM3D2Header* header) { DetermineHFieldFlatness(header); }

}  // extern "C"
