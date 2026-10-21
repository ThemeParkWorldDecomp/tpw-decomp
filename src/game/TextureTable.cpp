// TextureTable.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "FS_System.h"
#include "Game.h"
#include "HFieldTextures.h"
#include "Main.h"
#include "platform/macdoze.h"
#include "ppc_math.h"
#include "TPAdvisor.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// TPAdvisor.cpp (main app of SimTheme Park 2.04b3). PARTIAL: only the text file reader
// (0x10030c08..0x10031014) so far; the quick-load indexes are read with it. The advisor's model is in
// src/TPAdvisor_model.cpp.
//
// The reader knows four classes of character: letters (A-Z, a-z), digits, punctuation
// (every other printable ASCII character but the space) and the two comment characters
// '#' and ';' (which are punctuation too). Everything else is white space. It looks at the
// character at the cursor without asking first whether the cursor is still inside the
// text, so the character just past the end can end up in a string.

namespace {
bool Letter(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
bool Digit(char c) { return c >= '0' && c <= '9'; }
bool Punctuation(char c) {
    return (c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '`') || (c >= '{' && c <= '~');
}
bool Comment(char c) { return c == '#' || c == ';'; }
}  // namespace

int32_t IsComment(sFileParse* parse) { return Comment(*parse->mCursor) ? 1 : 0; }
int32_t IsLetter(sFileParse* parse) { return Letter(*parse->mCursor) ? 1 : 0; }
int32_t IsNumber(sFileParse* parse) { return Digit(*parse->mCursor) ? 1 : 0; }
int32_t IsMiscChar(sFileParse* parse) { return Punctuation(*parse->mCursor) ? 1 : 0; }

int32_t SkipWhitespace(sFileParse* parse) {
    for (;;) {
        if (parse->mSize <= parse->mPos) return 1;
        if (IsComment(parse) != 0 || IsLetter(parse) != 0 || IsNumber(parse) != 0 || IsMiscChar(parse) != 0) return 0;
        if (parse->mSize <= parse->mPos) return 1;
        ++parse->mCursor;
        ++parse->mPos;
    }
}

int32_t IsWhiteSpace(sFileParse* parse) {
    const char c = *parse->mCursor;
    return Letter(c) || Digit(c) || Punctuation(c) || Comment(c) ? 0 : 1;
}

int32_t GetString(sFileParse* parse, char* text, int32_t size, int32_t letters, int32_t digits, int32_t punctuation,
                  int32_t comments) {
    int32_t count = 0;
    *text = '\0';
    if (parse->mSize <= parse->mPos) return 1;
    for (;;) {
        if (size - 1 <= count) return 0;
        const char c = *parse->mCursor;
        const bool wanted = (letters != 0 && Letter(c)) || (digits != 0 && Digit(c)) || (punctuation != 0 && Punctuation(c)) ||
                            (comments != 0 && Comment(c));
        if (!wanted) return 0;
        ++count;
        *text++ = c;
        *text = '\0';
        if (parse->mSize <= parse->mPos) return 1;
        ++parse->mCursor;
        ++parse->mPos;
    }
}

// The level height field's texture table and flatness (main app of SimTheme Park 2.04b3).
// See include/HFieldTextures.h for what the table is and where each function lies in the
// inventory.
//
// Quirks of the original that are kept:
//  - The table's byte count (mBytes) is only ever added to: a second ParseLevelTCT asks for
//    the first one's bytes as well.
//  - InterceptAndInsertM3D2Textures reuses the flag that its search sets when it finds a
//    texture of the mesh in the table, in the loop that makes the mesh's new records: if the
//    LAST texture of the mesh was found, only the first category gets records, and the
//    places that the map gives to the other categories' textures point past the new arrays.
//  - GetString (TPAdvisor.cpp) may read one byte past the end of the text.
//  - FreeHFieldTextureTable leaves the table's other pointers (the categories' records, the
//    new textures) as they were: they point into the blocks it frees.

sExtraTextureEntry gExtraTextureEntries[2] = {
    {"PathTex", 0, nullptr},
    {"QueueTex", 0, nullptr},
};
sExtraTextureTable gExtraTextureTable = {2,       gExtraTextureEntries, 0,       0,       nullptr, 0,
                                         nullptr, nullptr,              nullptr, nullptr, 0,       nullptr};

namespace {

// atoi of the Mac's C library: a number beyond the 32-bit limits gives the limit (the
// host's atoi would wrap, or be undefined). A number here has at most 11 characters.
int32_t Atoi(const char* text) {
    const long long value = std::strtoll(text, nullptr, 10);
    if (value > INT32_MAX) return INT32_MAX;
    if (value < INT32_MIN) return INT32_MIN;
    return static_cast<int32_t>(value);
}

// The inlined skip to the line after the next '\n' (the cursor stays put at the end of the
// text); nonzero when the end of the text was reached.
bool SkipLine(sFileParse* parse) {
    for (;;) {
        if (*parse->mCursor == '\n') {
            if (parse->mPos >= parse->mSize) return true;
            ++parse->mCursor;
            ++parse->mPos;
            return false;
        }
        if (parse->mPos >= parse->mSize) return true;
        ++parse->mCursor;
        ++parse->mPos;
    }
}

}  // namespace

int32_t ParseLevelTCT(char* file, sExtraTextureTable* table) {
    int32_t valueCount = 0;   // counted by the first pass
    for (int32_t pass = 0; pass < 2; ++pass) {
        const bool store = pass != 0;
        char* names = nullptr;   // where the next name goes, in the second pass
        if (store) {
            table->mStored = 0;
            table->mValueCount = valueCount;
            table->mBlock = MReleaseAllocF(static_cast<uint32_t>(table->mBytes), 1);
            // The block holds the records of each category in turn, then the names.
            auto* records = static_cast<sExtraTexture*>(table->mBlock);
            sExtraTextureEntry* entry = table->mEntries;
            for (int32_t n = table->mCount; n != 0; --n) {
                entry->mValues = records;
                records += entry->mCount;
                ++entry;
            }
            names = reinterpret_cast<char*>(records);
        } else {
            valueCount = 0;
        }
        sExtraTextureEntry* entry = table->mEntries;
        for (int32_t n = table->mCount; n != 0; --n) {
            entry->mCount = 0;
            ++entry;
        }
        sExtraTextureEntry* current = nullptr;   // the category the last name line chose
        bfl::FileSystem::CFile* handle = TPFile_Open(file, "r");
        if (handle == nullptr) continue;
        const uint32_t size = static_cast<uint32_t>(TPFile_Size(handle));
        char* text = static_cast<char*>(MReleaseAllocF(size, 1));
        TPFile_Read(text, size, 1, handle);
        TPFile_Close(handle);
        sFileParse parse = {text, 0, size};
        char word[0x100];
        for (;;) {
            if (SkipWhitespace(&parse) != 0) break;
            if (IsComment(&parse) != 0) {
                if (SkipLine(&parse)) break;
            } else if (IsLetter(&parse) != 0) {
                // A category's name: letters and digits.
                const int32_t end = GetString(&parse, word, sizeof(word), 1, 1, 0, 0);
                current = nullptr;
                entry = table->mEntries;
                for (int32_t n = table->mCount; n != 0; --n, ++entry) {
                    if (NS_MacDoze::_stricmp(entry->mName, word) == 0) {
                        current = entry;
                        break;
                    }
                }
                if (end != 0) break;
            } else if (IsNumber(&parse) != 0) {
                // A texture: its number (digits and punctuation, so also a sign), then its file name.
                char number[0xc];
                int32_t end = GetString(&parse, number, sizeof(number), 0, 1, 1, 0);
                const int32_t value = Atoi(number);
                if (end != 0) break;
                if (SkipWhitespace(&parse) != 0) break;
                end = GetString(&parse, word, sizeof(word), 1, 1, 1, 0);
                if (current != nullptr) {
                    if (store) {
                        sExtraTexture* record = &current->mValues[current->mCount];
                        record->mNumber = value;
                        record->mText = names;
                        std::strcpy(record->mText.get(), word);
                        names += std::strlen(word) + 1;
                        ++table->mStored;
                    } else {
                        table->mBytes += sizeof(sExtraTexture);
                        ++valueCount;
                        table->mBytes += static_cast<int32_t>(std::strlen(word)) + 1;
                    }
                    ++current->mCount;
                }
                if (end != 0) break;
            } else {
                // Punctuation where a line should start: the line is skipped.
                if (SkipLine(&parse)) break;
            }
        }
        MReleaseFreeF(text);
    }
    return 1;
}

int32_t BuildHFieldTextureTable(tag_sM3D2Header* header) {
    sExtraTextureTable* table = &gExtraTextureTable;
    char name[0x400];
    const char* theme = gGame.GetCurrentThemeName().mData;
    char* themePath = get_theme_path();
    std::sprintf(name, "%s:Terrain:%s.tct", themePath, theme);
    char* file = FindFilePath(name, kFileReturnName, nullptr);
    if (file != nullptr) ParseLevelTCT(file, table);
    char* searchPath = FindFilePath(name, 0, nullptr);
    if (searchPath != nullptr) {
        const std::size_t length = std::strlen(searchPath);
        // (An empty search path would make the original read the byte before it.)
        const char* format = length != 0 && searchPath[length - 1] == ':' ? "%s%s%s%s" : "%s:%s%s%s";
        theme = gGame.GetCurrentThemeName().mData;
        std::sprintf(name, format, searchPath, "data:levels:", theme, ":Terrain:");
        InterceptAndInsertM3D2Textures(header, table, name);
    }
    return 1;
}

void FreeHFieldTextureTable() {
    sExtraTextureTable* table = &gExtraTextureTable;
    MReleaseFreeF(table->mNames);
    table->mNames = nullptr;
    MReleaseFreeF(table->mInfos);
    table->mInfos = nullptr;
    MReleaseFreeF(table->mBlock);
    table->mBlock = nullptr;
}

int32_t InterceptAndInsertM3D2Textures(tag_sM3D2Header* header, sExtraTextureTable* table, char* folder);

void ReplaceTextures(tag_sM3D2Header* header, int32_t* map, sTexture* textures, int32_t kept, uint32_t total);
