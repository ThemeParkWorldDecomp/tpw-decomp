// Test library for src/lobby_parser.cpp (libtpw_unit_LobbyParser.dylib; see
// tests/test_LobbyParser.py): the lobby description parser over a file the test supplies.
// The file and memory calls are fakes that record; the strings are functional.
#include <cstdlib>
#include <cstring>

#include "FS_System.h"
#include "LbFile.h"
#include "calllog.h"
#include "lobby.h"

#include "fake_strings.h"

using namespace calllog;

namespace {
char gFileObject[8];
const char* gFileText;
uint32_t gFileSize;
}  // namespace

CLBLobbyGlobals::~CLBLobbyGlobals() {}

bfl::FileSystem::CFile* TPFile_Open(const char* name, const char* mode) {
    ev("TPFile_Open", {}, {name, mode});
    return reinterpret_cast<bfl::FileSystem::CFile*>(gFileObject);
}
uint32_t TPFile_Seek(bfl::FileSystem::CFile* file, uint32_t offset, uint32_t whence) {
    ev("TPFile_Seek", {lab(file), offset, whence});
    return 0;
}
int32_t TPFile_Size(bfl::FileSystem::CFile* file) {
    ev("TPFile_Size", {lab(file)});
    return static_cast<int32_t>(gFileSize);
}
uint32_t TPFile_Read(void* dest, uint32_t size, uint32_t count, bfl::FileSystem::CFile* file) {
    ev("TPFile_Read", {lab(dest), size, count, lab(file)});
    std::memcpy(dest, gFileText, gFileSize);
    return count;
}
uint32_t TPFile_Close(bfl::FileSystem::CFile* file) {
    ev("TPFile_Close", {lab(file)});
    return 0;
}
// (One byte more than asked, a zero: the parser walks to the end of the last line without
// looking at the size.)
void* LbMem_Alloc(uint32_t size) {
    void* block = std::calloc(1, static_cast<size_t>(size) + 1);
    ev("LbMem_Alloc", {size, newlab(block)});
    return block;
}
void LbMem_Free(void* block) {
    ev("LbMem_Free", {lab(block)});
    forget(block);
    std::free(block);
}

extern "C" {
void* tpw_lp_file() { return gFileObject; }

// Parses `text` into an object whose memory starts as 0xa5; out: the 26 numbers' bits.
void tpw_lp_parse(const char* text, uint32_t size, uint32_t* out) {
    gFileText = text;
    gFileSize = size;
    alignas(16) static unsigned char store[sizeof(CLBLobbyGlobals)];
    std::memset(store, 0xa5, sizeof store);
    auto* globals = new (store) CLBLobbyGlobals;
    globals->ParseLobbyDescriptor();
    const float* fields[4] = {&globals->mIslandFov, &globals->mSpinSpeed, &globals->mSpinRadius, &globals->mVerticalOffset};
    int n = 0;
    for (const float* field : fields) std::memcpy(&out[n++], field, 4);
    std::memcpy(&out[n++], &globals->mGlobeRadiusOut, 4);
    std::memcpy(&out[n++], &globals->mGlobeRadiusIn, 4);
    for (int i = 0; i < 10; ++i) std::memcpy(&out[n++], &globals->mCameraX[i], 4);
    for (int i = 0; i < 10; ++i) std::memcpy(&out[n++], &globals->mCameraY[i], 4);
}

// LB_ReadTbStringUntilNext on `text` from `start`: the string to `out`, the cursor's new offset.
int32_t tpw_lp_read(char* text, int32_t start, int32_t delimiter, char* out) {
    char* cursor = text + start;
    const TbDynamicStringTemplate<char> result(LB_ReadTbStringUntilNext(&cursor, static_cast<char>(delimiter)));
    std::strcpy(out, result.mData != nullptr ? result.mData : "");
    return static_cast<int32_t>(cursor - text);
}
}
