// Unit w208 (main app, SimTheme Park 2.04b3): the park's hoarding written to the theme's hoardings balance file, and the static objects of
// LLCellSystem.cpp.
//   LLCC_WriteHoardingsLine                   0x1009588c  580 bytes
//   LLCC_WriteHoardingsToBalanceFile          0x10095b20  764 bytes
//   LLCC_WriteCompleteHoardingsToBalanceFile  0x10095e54  764 bytes
//   __sinit_LLCellSystem_cpp                  0x10096f04  764 bytes (the definitions just below)
#include "LLCellSystem.h"

#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "FS_System.h"
#include "Main.h"
#include "TPGlobals.h"
#include "TbDynamicStringTemplate.h"
#include "TbError.h"
#include "bfl_FileSystem.h"

// __sinit_LLCellSystem_cpp (0x10096f04) builds these in this order, and registers a destructor for each object that has one with
// __register_global_object: two plain words, the file, the seven strings, then the nine offsets (stored as two words each, the destructor of the
// MapDelta statics registered for each). The host's definitions in the same order stand in for it; the words and the offsets need no construction.
int32_t gLLCC4820 = 0;                                    // 0x102e4820
int32_t gLLCC481C = 0x10000;                              // 0x102e481c
bfl::FileSystem::CDiscFile gLLCCHoardingsFile;            // 0x102e058c  (its destructor 0x103bcb68)
TbDynamicStringTemplate<char> gLLCCHoardingFileHeader("#ThemeParkWorld - Hoardings.sam");   // 0x102e056c
TbDynamicStringTemplate<char> gLLCCHoardingFileNote("#DO NOT EDIT BY HAND");                // 0x102e054c
TbDynamicStringTemplate<char> gLLCCHoardingKeyPrefix("HoardingClicks[");                    // 0x102e052c
TbDynamicStringTemplate<char> gLLCCHoardingKeyX("].X");                                     // 0x102e050c
TbDynamicStringTemplate<char> gLLCCHoardingKeyY("].Y");                                     // 0x102e04ec
TbDynamicStringTemplate<char> gLLCCHoardingEOL("\n");                                       // 0x102e04cc
TbDynamicStringTemplate<char> gLLCCHoardingSep("\t");                                       // 0x102e04ac
const MapDelta gLLCCPointedOffsets[9] = {
    {0, 0, 0},     // 0x102de0e4
    {-1, -1, 0},   // 0x102de0ec
    {0, -1, 0},    // 0x102de0f4
    {1, -1, 0},    // 0x102de0fc
    {-1, 0, 0},    // 0x102de104
    {1, 0, 0},     // 0x102de10c
    {-1, 1, 0},    // 0x102de114
    {0, 1, 0},     // 0x102de11c
    {1, 1, 0},     // 0x102de124
};

namespace {

// One write to the file: the length of the text, then the file's access object (a file that failed to open has none: nothing is written).
// The Mac inlines CFile::Write, which tests the access object each time.
void WriteText(const TbDynamicStringTemplate<char>& text) {
    const uint32_t length = static_cast<uint32_t>(std::strlen(text.mData));
    gLLCCHoardingsFile.Write(text.mData, length);
}

// The next click of the copied stack, which both writers inline (the 8 bytes at 0x102e07b0 are the click it hands out; w204's Backtrack pops
// the same one). The index and the count are compared signed: an index past the count ends the walk at once, otherwise the click at the index is
// copied out, the index moves on and, when it has gone past the count, goes back (the click then handed out is not used). The copy after the last
// click is read too: at the entry 0x400 the Mac reads the index word and the count word that follow the array. Counts above 0x400 are not modelled.
const LLCCClick* NextCopiedClick() {
    const int32_t index = gLLCCClickCopyIndex;
    const int32_t count = gLLCCClickCopyCount;
    if (index > count) return nullptr;
    if (index == 0x400) {
        gLLCCBacktrackClick.mX = index;
        gLLCCBacktrackClick.mY = count;
    } else {
        gLLCCBacktrackClick = gLLCCClickStackCopy[index];
    }
    gLLCCClickCopyIndex = gLLCCClickCopyIndex + 1;
    if (gLLCCClickCopyIndex > count) {
        gLLCCClickCopyIndex = gLLCCClickCopyIndex - 1;
        return nullptr;
    }
    return &gLLCCBacktrackClick;
}

// The two writers are the same code with a different file name (a member of gTPGlobals) and trace text: the Mac has two copies of it.
int32_t WriteBalanceFile(const TbDynamicStringTemplate<char>& name, const char* trace) {
    TbDynamicStringTemplate<char> path;
    int32_t i = 0;
    path = get_theme_path();
    FS_AddDirectorySeparator(path);
    path += name;
    CTRACE(6, trace);
    const TbError opened = gLLCCHoardingsFile.Open(path.mData, 0xC0000010);
    if (static_cast<int32_t>(opened) < 0) {
        return 0;
    }
    // The click stack is built with gLLCC4818 set; the copy of it is LLCC_SystemMakeClickStackCopy's loop, inlined.
    gLLCC4818 = 1;
    LLCC_SystemConstructHoardingClickStack();
    gLLCC4818 = 0;
    for (int32_t k = 0; k < gLLCCClickStackCount; ++k) {
        gLLCCClickStackCopy[k] = gLLCCClickStack[k];
    }
    gLLCCClickCopyCount = gLLCCClickStackCount;
    gLLCCClickCopyIndex = 0;
    for (const LLCCClick* click = NextCopiedClick(); click != nullptr; click = NextCopiedClick()) {
        const int32_t x = static_cast<int32_t>(static_cast<uint32_t>(click->mX) - static_cast<uint32_t>(gHoardingRight));
        if (LLCC_WriteHoardingsLine(gLLCCHoardingKeyX, i, x) == 0) {
            return 0;
        }
        const int32_t y = static_cast<int32_t>(static_cast<uint32_t>(click->mY) - static_cast<uint32_t>(gHoardingBottom));
        if (LLCC_WriteHoardingsLine(gLLCCHoardingKeyY, i, y) == 0) {
            return 0;
        }
        // A blank row between two clicks (the temporary string is destroyed before the answer is tested).
        if (LLCC_WriteHoardingsLine(TbDynamicStringTemplate<char>(""), -1, 0) == 0) {
            return 0;
        }
        ++i;
    }
    if (LLCC_WriteHoardingsLine(gLLCCHoardingKeyX, i, 0x200) == 0) {
        return 0;
    }
    if (LLCC_WriteHoardingsLine(gLLCCHoardingKeyY, i, 0x200) == 0) {
        return 0;
    }
    gLLCCHoardingsFile.Close();
    return 1;
}

}  // namespace

// 0x1009588c (580 bytes). Every piece goes out through WriteText; the digits are made by the bullfrog glue _itoa into a stack buffer.
int32_t LLCC_WriteHoardingsLine(const TbDynamicStringTemplate<char>& key, int32_t index, int32_t value) {
    char buffer[40];
    if (index == -1) {
        WriteText(gLLCCHoardingEOL);
        return 1;
    }
    WriteText(gLLCCHoardingKeyPrefix);
    NS_MacDoze::_itoa(index, buffer, 10);
    gLLCCHoardingsFile.Write(buffer, static_cast<uint32_t>(std::strlen(buffer)));
    WriteText(key);
    WriteText(gLLCCHoardingSep);
    NS_MacDoze::_itoa(value, buffer, 10);
    gLLCCHoardingsFile.Write(buffer, static_cast<uint32_t>(std::strlen(buffer)));
    WriteText(gLLCCHoardingEOL);
    return 1;
}

// 0x10095b20 (764 bytes). Every failed row (never: the row writer always answers 1) returns 0 with the path destroyed and the file left open.
int32_t LLCC_WriteHoardingsToBalanceFile() {
    return WriteBalanceFile(gTPGlobals.mF40EC, "Writing Hoardings Balance file\n");
}

// 0x10095e54 (764 bytes). The same with the complete hoardings' file name.
int32_t LLCC_WriteCompleteHoardingsToBalanceFile() {
    return WriteBalanceFile(gTPGlobals.mF4100, "Writing Complete Hoardings Balance file\n");
}
