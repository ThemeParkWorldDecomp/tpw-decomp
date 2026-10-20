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
#include "HFieldTextures.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "FS_System.h"
#include "Game.h"
#include "Main.h"
#include "TPAdvisor.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "platform/macdoze.h"
#include "ppc_math.h"

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

int32_t InterceptAndInsertM3D2Textures(tag_sM3D2Header* header, sExtraTextureTable* table, char* folder) {
    int32_t kept = 0;   // textures of the mesh that the table does not name
    table->mAdded = 0;
    // The leftover of the caller's register in the original, nonzero (the mesh's address),
    // for a mesh without textures; see the quirks at the top of the file.
    bool found = true;
    // For each texture of the mesh: the place of the table's texture of that name (counting
    // from 1 through all categories), or minus the place among the textures to keep.
    auto* map = static_cast<int32_t*>(MReleaseAllocF(static_cast<uint32_t>(header->mTextureCount) * 4, 1));
    sTextureInfo* info = header->mTextureInfos;
    for (int32_t i = 0; i < header->mTextureCount; ++i, ++info) {
        int32_t place = 0;
        found = false;
        sExtraTextureEntry* entry = table->mEntries;
        for (int32_t n = table->mCount; n != 0; --n, ++entry) {
            sExtraTexture* record = entry->mValues;
            for (int32_t m = entry->mCount; m != 0; --m, ++record) {
                ++place;
                if (NS_MacDoze::_stricmp(record->mText.get(), info->mName.get()) == 0) {
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        if (found) {
            map[i] = place;
        } else {
            ++kept;
            map[i] = -kept;
        }
    }
    // One block for the new infos (0x10 each), the new textures (8 each) and the folder
    // names, one for each category.
    int32_t nameBytes = 0;
    sExtraTextureEntry* entry = table->mEntries;
    for (int32_t n = table->mCount; n != 0; --n, ++entry) {
        const int32_t nameLength = static_cast<int32_t>(std::strlen(entry->mName));
        const int32_t folderLength = static_cast<int32_t>(std::strlen(folder));
        nameBytes += nameLength + 1 + folderLength;
    }
    const uint32_t total = static_cast<uint32_t>(kept + table->mStored);
    auto* block = static_cast<char*>(MReleaseAllocF(total * 0x18 + static_cast<uint32_t>(nameBytes), 1));
    table->mInfos = reinterpret_cast<sTextureInfo*>(block);
    table->mOldInfos = header->mTextureInfos;
    table->mTextures = reinterpret_cast<sTexture*>(block + total * sizeof(sTextureInfo));
    table->mOldTextures = header->mTextures;
    sTextureInfo* newInfo = table->mInfos;
    const sTextureInfo* oldInfo = table->mOldInfos;
    sTexture* newTexture = table->mTextures;
    const sTexture* oldTexture = table->mOldTextures;
    char* folderName = reinterpret_cast<char*>(newTexture + total);
    // First the textures of the mesh to keep, in their order.
    for (int32_t i = 0; i < header->mTextureCount; ++i) {
        if (map[i] < 0) {
            if (newInfo != nullptr && oldInfo != nullptr) std::memcpy(newInfo, oldInfo, sizeof(sTextureInfo));
            if (newTexture != nullptr && oldTexture != nullptr) std::memcpy(newTexture, oldTexture, sizeof(sTexture));
            ++newInfo;
            ++newTexture;
            ++table->mAdded;
        }
        ++oldInfo;
        ++oldTexture;
    }
    // Then the table's, a record for each, with its folder and its name (0x14 bytes, the
    // last of them 0).
    table->mNames = static_cast<char*>(MReleaseAllocF(static_cast<uint32_t>(table->mValueCount) * 0x14, 1));
    char* name = table->mNames;
    uint16_t index = static_cast<uint16_t>(kept);
    entry = table->mEntries;
    for (int32_t n = 0; n < table->mCount; ++n, ++entry) {
        sExtraTexture* record = entry->mValues;
        std::sprintf(folderName, "%s%s", folder, entry->mName);
        for (int32_t m = 0; m < entry->mCount; ++m, ++record) {
            newInfo->mFlags = 0;
            newInfo->mFolder = folderName;
            newInfo->mName = name;
            name += 0x14;
            newInfo->mFrames = 1;
            std::strncpy(newInfo->mName.get(), record->mText.get(), 0x14);
            newInfo->mName.get()[0x13] = 0;
            ++newInfo;
            newTexture->mF00 = 1;
            newTexture->mTexture = nullptr;
            ++newTexture;
            record->mTextureIndex = index++;
        }
        folderName += std::strlen(folderName) + 1;
        if (found) break;
    }
    ReplaceTextures(header, map, table->mTextures, kept, total);
    header->mTextureInfos = table->mInfos;
    header->mTextures = table->mTextures;
    header->mTextureCount = static_cast<uint16_t>(total);
    MReleaseFreeF(map);
    return 1;
}

void ReplaceTextures(tag_sM3D2Header* header, int32_t* map, sTexture* textures, int32_t kept, uint32_t total) {
    // The new place (counting from 0) of the texture the map gives a number for.
    auto place = [&](int32_t number) { return number > 0 ? number + kept - 1 : -number - 1; };
    tag_sNode* node = header->mNodes;
    for (int32_t i = 0; i < header->mNodeCount; ++i, ++node) {
        sFaceSet* faceSet = node->mFaceSets;
        for (int32_t j = 0; j < node->mFaceSetCount; ++j, ++faceSet) {
            if (faceSet->mTexture != nullptr) {
                // The old place in the texture array: the byte difference over 8.
                const uint32_t old = static_cast<uint32_t>(reinterpret_cast<const char*>(faceSet->mTexture.get()) -
                                                           reinterpret_cast<const char*>(header->mTextures.get()));
                faceSet->mTexture = textures + place(map[old >> 3]);
            }
        }
    }
    if (header->mHField == nullptr) return;
    for (int32_t i = 0; i < header->mHField->mQuadCount; ++i) {
        uint16_t* word = &header->mHField->mQuads[i].mF02;
        *word = static_cast<uint16_t>(place(map[*word]));
    }
}

namespace {

// The cross product of the two edges that leave one corner of a triangle of the field: the
// edges (ten, dy1, 0) and (0, dy2, ten), with the cell's size ten (+10 or -10) and the steps
// in height dy1 and dy2, each divided by its length (a double sum, one square root, rounded
// to single). The result is not a unit vector: its length is the sine of the angle between
// the edges. The coordinates come from double products and fused multiply-subtracts, each
// rounded to single, in the order the original computes them.
struct sNormal {
    float x, y, z;
};

sNormal TriangleNormal(float ten, float dy1, float dy2) {
    const float zero = 0.0f;
    // (float)(zero * zero) is 0 and the sums are added in double.
    const double sum1 = static_cast<float>(zero * zero) + ppc_fmadd(ten, ten, static_cast<float>(dy1 * dy1));
    const float length1 = static_cast<float>(std::sqrt(sum1));
    const float edge1Z = zero / length1;
    const float edge1Y = dy1 / length1;
    const float edge1X = ten / length1;
    const double sum2 = static_cast<float>(ten * ten) + ppc_fmadd(zero, zero, static_cast<float>(dy2 * dy2));
    const float length2 = static_cast<float>(std::sqrt(sum2));
    const float edge2Y = dy2 / length2;
    const float edge2X = zero / length2;
    const float edge2Z = ten / length2;
    sNormal normal;
    normal.x = static_cast<float>(ppc_fmsub(edge2Y, edge1Z, static_cast<double>(edge2Z) * edge1Y));
    normal.y = static_cast<float>(ppc_fmsub(edge2Z, edge1X, static_cast<double>(edge2X) * edge1Z));
    normal.z = static_cast<float>(ppc_fmsub(edge2X, edge1Y, static_cast<double>(edge2Y) * edge1X));
    return normal;
}

}  // namespace

void DetermineHFieldFlatness(tag_sM3D2Header* header) {
    sHField* field = header->mHField;
    for (uint32_t y = 0; y < field->mF18[1]; ++y) {
        for (uint32_t x = 0; x < field->mF18[0]; ++x) {
            const float* corner = field->mHeights.get() + (x + y * (field->mF18[0] + 1));
            const float* below = corner + field->mF18[0] + 1;
            const float cell = 10.0f;
            // The triangle at the cell's first corner, then the one at the opposite corner.
            const sNormal first = TriangleNormal(cell, corner[1] - corner[0], below[0] - corner[0]);
            const sNormal second = TriangleNormal(-cell, below[0] - below[1], corner[1] - below[1]);
            float dot = first.y * second.y;
            dot = ppc_fmadds(first.x, second.x, dot);
            dot = ppc_fmadds(first.z, second.z, dot);
            // The two normals of a cell that steps along one axis only are parallel and of length 1,
            // so their dot product is 1; a cell that slopes along both axes has shorter normals.
            // Those cells are marked (0x800 in the quad's first word).
            if (dot < 0.9999f) field->mQuads[x + y * field->mF18[0]].mF00 |= 0x800;
        }
    }
}
