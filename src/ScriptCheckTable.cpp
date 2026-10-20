// The script system's check table (main app of SimTheme Park 2.04b3, analyser.cpp in the inventory; unit w112c): one byte for each
// word of the compiled scripts' code array, 1 where the word is a command descriptor (a word between the smallest and the largest
// descriptor), else 0. The interpreter (DoScript 0x100d88b4, untranslated) stops at an instruction whose byte is 0.
//
// The code array and the descriptor list are in the Mac's data segment and have no host equivalent yet: the host defaults are empty,
// so the table has no entries and nothing reads it.
#include <cstdint>

#include "analyser.h"

namespace {
const uint32_t kNoWords[1] = {0};
}  // namespace

uint8_t* gScriptCheckTable = nullptr;                  // 0x102f7c10
const uint32_t* gScriptCodeWords = kNoWords;           // 0x1029f20c
const uint32_t* gScriptCodeWordsEnd = kNoWords;        // 0x1029ffd8
const uint32_t* gScriptCommandList = kNoWords;         // 0x1029ffec
uint32_t gScriptCommandFirst = 0;                      // 0x10266958

// 0x100d900c (68 bytes).
void SCR_CloseCheckTable() {
    if (gScriptCheckTable != nullptr) {
        delete[] gScriptCheckTable;
        gScriptCheckTable = nullptr;
    }
}

// 0x100d8f0c (212 bytes). The range starts as the single word gScriptCommandFirst and grows over the list (two independent unsigned
// compares for each word; the ending 0 is not used). The table has (end - begin) / 4 entries, the division rounding toward zero
// (srawi and addze) on the byte difference taken in 32 bits, and is not cleared before it is filled.
void SCR_InitCheckTable() {
    uint32_t lowest = gScriptCommandFirst;
    uint32_t highest = gScriptCommandFirst;
    const uint32_t* const words = gScriptCodeWords;
    const uint32_t* const end = gScriptCodeWordsEnd;
    SCR_CloseCheckTable();
    for (const uint32_t* command = gScriptCommandList; *command != 0; ++command) {
        const uint32_t word = *command;
        if (word < lowest) lowest = word;
        if (word > highest) highest = word;
    }
    const int32_t bytes = static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(end)) -
                                               static_cast<uint32_t>(reinterpret_cast<uintptr_t>(words)));
    const int32_t count = bytes / 4;
    gScriptCheckTable = new uint8_t[count];
    for (int32_t i = 0; i < count; ++i) {
        const uint32_t word = words[i];
        if (word < lowest || word > highest) {
            gScriptCheckTable[i] = 0;
        } else {
            gScriptCheckTable[i] = 1;
        }
    }
}
