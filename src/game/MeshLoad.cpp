// MeshLoad.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "analyser.h"
#include "DynamicMesh.h"
#include "FS_System.h"
#include "Game.h"
#include "GameSys.h"
#include "HFieldTextures.h"
#include "LevelInit.h"
#include "LevelSystems.h"
#include "Light.h"
#include "LLCellSystem.h"
#include "lobby.h"
#include "m3d2.h"
#include "Main.h"
#include "MapWho.h"
#include "platform/macdoze.h"
#include "platform/opengl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "Postcard.h"
#include "ppc_math.h"
#include "Quickload.h"
#include "rsse.h"
#include "tb_mesh.h"
#include "tb_resource_display.h"
#include "TbGraphicFile.h"
#include "TPAdvisor.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPMovie.h"
#include "ui_ErrorDisplay.h"
#include "World.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// Loading meshes (main program of SimTheme Park 2.04b3): LoadMesh, CreateMeshTypeEngineMesh
// and the mesh paths (0x100b7d04..0x100b9140, which the inventory files under rsse.cpp),
// CreateMeshFromM3D2Header and CreateEngineMesh (0x10064cf0, 0x10064f40, TPRide.cpp there).

void* gMeshPathAllocs;
int32_t gFileAccessIndex;
int32_t gMeshPathCount;
char** gMeshPaths;
int32_t gMeshPathIndex = -1;
int32_t gTexturePathIndex = -1;
int32_t gSharedPathIndex = -1;

namespace {
// A table as it is allocated: the link to the table before it, the folders' pointers,
// then their texts. (Four bytes per pointer on the Mac.)
struct SPathAlloc {
    SPathAlloc* mNext;
    char* mPaths[1];
};

// "<root><folder>" with a ':' at the end if the folder has none. (sprintf in the
// original; the buffers are 1 KB.)
template <size_t N>
void JoinPath(char (&out)[N], const char* root, const char* folder) {
    if (folder[std::strlen(folder) - 1] != ':') std::snprintf(out, N, "%s%s:", root, folder);
    else std::snprintf(out, N, "%s%s", root, folder);
}
}  // namespace

tag_sMesh* LoadMesh(char* name, void*, int32_t noEngineMesh, int32_t pathArgument, int32_t oneFrame) {
    tag_sM3D2Header* header = LoadM3D2Header(name, oneFrame != 0 ? 6 : 2);
    if (header == nullptr) return nullptr;
    tag_sMesh* mesh = CreateMeshFromM3D2Header(header);
    OverrideTexturePaths(header, 0, pathArgument);
    if (noEngineMesh == 0) CreateMeshTypeEngineMesh(mesh);
    return mesh;
}

void CreateMeshTypeEngineMesh(tag_sMesh* mesh) {
    mesh->mTypeEngineMesh = new CMesh;
    mesh->mTypeEngineMesh->Init(mesh->mHeader);
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

// rsse.cpp (main app 0x100b7fa0, unit w114): the height field quads' orientation flags for procedural texturing on or off.

// Walks the height field's quads (gHeightfieldHeader->mHField, mQuadCount of them, 4 bytes each). Every quad whose flag halfword has
// neither bit 0x80 nor bit 0x01 is changed: procedural textures on (`procedural` nonzero) clears the orientation bits 0x78; off (0) clears
// 0xf8 and puts back the same bits of the matching quad of the saved copy (gHeightfieldCopy, made by SaveHFieldCopy). The second halfword
// of a quad and the copy are never written. The copy's quad array is looked up in both modes, so a NULL gHeightfieldCopy faults even for
// mode 1 in the original (a load whose result the host drops); callers run after SaveHFieldCopy.
void CorrectOrientationFlagsForDiffProceduralMode(int32_t procedural) {
    sHField* field = gHeightfieldHeader->mHField;
    sHFieldQuad* quad = field->mQuads;
    const sHFieldQuad* copy = gHeightfieldCopy->mQuads;
    uint32_t count = field->mQuadCount;
    if (procedural != 0) {
        for (; count != 0; --count, ++quad) {
            if ((quad->mF00 & 0x81) == 0) quad->mF00 = static_cast<uint16_t>(quad->mF00 & 0xff87);
        }
        return;
    }
    for (; count != 0; --count, ++quad, ++copy) {
        if ((quad->mF00 & 0x81) == 0) {
            quad->mF00 = static_cast<uint16_t>(quad->mF00 & 0xff07);
            quad->mF00 = static_cast<uint16_t>(quad->mF00 | (copy->mF00 & 0xf8));
        }
    }
}

// Unit v5906 (main app, file rsse.cpp): Postcard::save 0x100c0c20 (the card as a JPEG), SetWorldBoundingArea 0x100b9180 (a header's box
// copied to another and carried by the second's node matrix) and SetHeightfieldTexture 0x100b8080 (sets the texture and flags of one height
// field quad).

// 0x100b8080: the quad (x, y) of the header's height field. With category 0 and texture 8 the quad is made plain (bits 0x101, not 0x80);
// with category 0 and texture 0x37 it takes the saved copy's words (nothing when there is none); otherwise the texture is looked up in
// the table (category and texture wrapped into range) and the rotation `rotation` (0x5a, 0xb4, 0x10e) sets bit 0x20, 0x10 or 8.
// The category wrap divides by the table's count and the texture wrap by the entry's: a count of 0 leaves the texture as it is (the
// emulator's divw); a negative category reads before the table (kept).
void SetHeightfieldTexture(tag_sM3D2Header* header, sExtraTextureTable* table, int32_t x, int32_t y, int32_t category, int32_t texture,
                           int32_t rotation) {
    bool plain = category == 0 && texture == 8;
    bool copy = category == 0 && texture == 0x37;
    if (!plain && !copy) {
        if (category == 0) category = 1;
        int32_t count = table->mCount;
        if (count <= 0) return;
        if (table->mEntries[category - 1].mCount <= 0) return;
        int32_t wrapped = category - 1;
        category = (wrapped - (wrapped / count) * count) + 1;
        int32_t perEntry = table->mEntries[category - 1].mCount;
        texture = texture - (perEntry != 0 ? (texture / perEntry) * perEntry : 0);
    }
    if (x < 0) return;
    sHField* field = header->mHField.get();
    int32_t width = static_cast<int32_t>(field->mF18[0]);
    if (x >= width) return;
    if (y < 0) return;
    if (y >= static_cast<int32_t>(field->mF18[1])) return;
    int32_t index = x + y * width;
    sHFieldQuad* quad = field->mQuads.get() + index;
    if (plain) {
        quad->mF00 = static_cast<uint16_t>(quad->mF00 | 0x101);
        quad = header->mHField->mQuads.get() + index;
        quad->mF00 = static_cast<uint16_t>(quad->mF00 & 0xff7f);
    } else if (copy) {
        if (gHeightfieldCopy == nullptr) return;
        const sHFieldQuad* saved = gHeightfieldCopy->mQuads.get() + index;
        quad->mF02 = saved->mF02;
        quad = header->mHField->mQuads.get() + index;
        quad->mF00 = saved->mF00;
        quad->mF00 = static_cast<uint16_t>(quad->mF00 | 0x100);
        if ((gGameOptions & 0x4000) != 0) quad->mF00 = static_cast<uint16_t>(quad->mF00 & 0xff87);
    } else {
        quad->mF02 = table->mEntries[category - 1].mValues[texture].mTextureIndex;
        uint16_t* word = &(header->mHField->mQuads.get() + index)->mF00;
        *word = static_cast<uint16_t>(*word & 0xff87);
        *word = static_cast<uint16_t>(*word & 0xfffe);
        *word = static_cast<uint16_t>(*word | 0x180);
        if (rotation == 0xb4) *word = static_cast<uint16_t>(*word | 0x10);
        else if (rotation == 0x5a) *word = static_cast<uint16_t>(*word | 0x20);
        else if (rotation == 0x10e) *word = static_cast<uint16_t>(*word | 8);
    }
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

// The dynamic cell meshes (main application; unit w112b). The Mac files are analyser.cpp for the class (CDynamicMesh: constructor
// 0x100e1610, destructor 0x100e191c, SetBoundingBox 0x100e19b0, DC_SetVertex 0x100e1f10, DC_CreateSquareFace 0x100e210c), TPAdvisor.cpp
// for DMCreateMesh 0x100301c0 and the two DMEnableMeshRender 0x1003088c and 0x10030928, rsse.cpp for CorrectDTexturePaths 0x100b8710 and
// LLCellSystem.cpp for LLCC_SystemGetMode 0x100914f4.
//
// A mesh is N squares of four vertices and two faces each, drawn cell by cell by DrawAll (untranslated, the next unit). DMCreateMesh lays the
// whole mesh image out in one block of the engine allocator in the mesh file's 32-bit layout (Ptr32 fields; the block comes from the arena);
// the CDynamicMesh itself, its texture set, entry and mesh set are native records that point into the block.

// ---- rsse.cpp

// 0x100b8710 (324 bytes). Two tries: the texture folder and the first texture's name as they are, then (after FindFilePath finds nothing)
// with the name's last extension replaced by ".wct". FindFilePath answers the search path the file is in, not a file name; with none found
// the last name tried is the root. The Mac's sprintf has no bound; this one stops at the buffer's size.
void CorrectDTexturePaths(sDTextureSet* textures, tag_sDMeshSet* set) {
    char buffer[1032];
    char* root = nullptr;
    for (uint32_t tries = 2; tries != 0; --tries) {
        const char* folder = textures->mFolder;
        const char* name = textures->mEntries[0].mName;
        const size_t length = std::strlen(folder);
        if (folder[static_cast<ptrdiff_t>(length) - 1] == ':') {
            std::snprintf(buffer, sizeof buffer, "%s%s", folder, name);
        } else {
            std::snprintf(buffer, sizeof buffer, "%s:%s", folder, name);
        }
        if (tries == 1) {
            char* dot = std::strrchr(buffer, '.');
            if (dot == nullptr) {
                std::strcat(buffer, ".wct");
            } else {
                std::strcpy(dot, ".wct");
            }
        }
        root = FindFilePath(buffer, 0, nullptr);
        if (root != nullptr) break;
    }
    if (root == nullptr) root = buffer;
    SetNewMeshPaths(root, nullptr, const_cast<char*>(textures->mFolder), const_cast<char*>(textures->mSharedFolder));
    OverrideTexturePaths(set->mMesh->mHeader, 1, 0);
}

tag_sMesh* CreateMeshFromM3D2Header(tag_sM3D2Header* header);

char* GetMeshPath() { return gMeshPathIndex != -1 ? gMeshPaths[gMeshPathIndex] : nullptr; }
int32_t GetFileAccessIndex() { return gFileAccessIndex; }
void SetFileAccessIndex(int32_t index) { gFileAccessIndex = index; }
// sprintf(out, "%s:%s", ...) in the original, into a buffer whose size it does not know.
void BuildSharedTexturePath(char* out) {
    std::strcpy(out, get_theme_path());
    std::strcat(out, ":");
    std::strcat(out, gSharedTextureFolder);
}

// A folder that is the same text as before keeps its old string (in an older table); a
// new table is allocated only when at least one text is new, and then holds all three
// pointers and the new texts. With nothing new the pointers are written into the table
// that is already there.
void SetNewMeshPaths(char* root, char* folder, char* textureFolder, char* sharedTextureFolder) {
    char* oldMesh = gMeshPathIndex == -1 ? nullptr : gMeshPaths[gMeshPathIndex];
    char* oldTexture = gTexturePathIndex == -1 ? nullptr : gMeshPaths[gTexturePathIndex];
    char* oldShared = gSharedPathIndex == -1 ? nullptr : gMeshPaths[gSharedPathIndex];
    char meshPath[1036], texturePath[1024], sharedPath[1024];
    gMeshPathCount = 0;
    int32_t bytes = 0;
    if (folder == nullptr) {
        gMeshPathIndex = -1;
    } else {
        JoinPath(meshPath, root, folder);
        if (oldMesh == nullptr || std::strcmp(meshPath, oldMesh) != 0) {
            oldMesh = nullptr;
            bytes = static_cast<int32_t>(std::strlen(meshPath)) + 1;
        }
        gMeshPathIndex = gMeshPathCount++;
    }
    if (textureFolder == nullptr) {
        gTexturePathIndex = -1;
    } else {
        JoinPath(texturePath, root, textureFolder);
        if (oldTexture == nullptr || std::strcmp(texturePath, oldTexture) != 0) {
            oldTexture = nullptr;
            bytes += static_cast<int32_t>(std::strlen(texturePath)) + 1;
        }
        gTexturePathIndex = gMeshPathCount++;
    }
    if (sharedTextureFolder == nullptr) {
        gSharedPathIndex = -1;
    } else {
        JoinPath(sharedPath, root, sharedTextureFolder);
        if (oldShared == nullptr || std::strcmp(sharedPath, oldShared) != 0) {
            oldShared = nullptr;
            bytes += static_cast<int32_t>(std::strlen(sharedPath)) + 1;
        }
        gSharedPathIndex = gMeshPathCount++;
    }
    char* text = nullptr;
    if (bytes != 0) {
        auto* table = static_cast<SPathAlloc*>(MReleaseAllocF(
            static_cast<uint32_t>(bytes) + static_cast<uint32_t>(gMeshPathCount) * sizeof(char*) + sizeof(SPathAlloc*), 0));
        table->mNext = static_cast<SPathAlloc*>(gMeshPathAllocs);
        gMeshPathAllocs = table;
        gMeshPaths = table->mPaths;
        text = reinterpret_cast<char*>(table->mPaths + gMeshPathCount);
    }
    if (folder != nullptr) {
        if (oldMesh == nullptr) {
            gMeshPaths[gMeshPathIndex] = text;
            std::strcpy(text, meshPath);
            text += std::strlen(meshPath) + 1;
        } else {
            gMeshPaths[gMeshPathIndex] = oldMesh;
        }
    }
    if (textureFolder != nullptr) {
        if (oldTexture == nullptr) {
            gMeshPaths[gTexturePathIndex] = text;
            std::strcpy(text, texturePath);
            text += std::strlen(texturePath) + 1;
        } else {
            gMeshPaths[gTexturePathIndex] = oldTexture;
        }
    }
    if (sharedTextureFolder != nullptr) {
        if (oldShared == nullptr) {
            gMeshPaths[gSharedPathIndex] = text;
            std::strcpy(text, sharedPath);
        } else {
            gMeshPaths[gSharedPathIndex] = oldShared;
        }
    }
}

void DestroyPathAllocs() {
    auto* table = static_cast<SPathAlloc*>(gMeshPathAllocs);
    while (table != nullptr) {
        SPathAlloc* next = table->mNext;
        MReleaseFreeF(table);
        table = next;
    }
    gMeshPathAllocs = nullptr;
    gMeshPaths = nullptr;
    gMeshPathCount = 0;
    gMeshPathIndex = -1;
    gTexturePathIndex = -1;
    gSharedPathIndex = -1;
    gFileAccessIndex = 0;
}

void OverrideTexturePaths(tag_sM3D2Header* header, int32_t checkFiles, int32_t) {
    if (header == nullptr) return;
    header->mPaths = gMeshPaths;
    header->mF34 = static_cast<uint16_t>(gMeshPathCount);
    char* textureFolder = gTexturePathIndex == -1 ? nullptr : gMeshPaths[gTexturePathIndex];
    char* sharedFolder = gSharedPathIndex == -1 ? nullptr : gMeshPaths[gSharedPathIndex];
    sTextureInfo* info = header->mTextureInfos;
    const int32_t access = gFileAccessIndex;
    for (int32_t i = 0; i < header->mTextureCount; ++i, ++info) {
        int32_t found = 0;
        if (textureFolder != nullptr) {
            if (gQuickloadEnabled == 0 || checkFiles != 0) {
                char path[1036];
                std::snprintf(path, sizeof path, "%s%s", textureFolder, info->mName.get());
                char* dot = std::strrchr(path, '.');
                if (dot != nullptr) std::strcpy(dot, ".wct");
                else std::strcat(path, ".wct");
                found = CheckFileExists(path);
                if (found == 0) {
                    dot = std::strrchr(path, '.');
                    if (dot != nullptr) std::strcpy(dot, ".tga");
                    else std::strcat(path, ".tga");
                    found = CheckFileExists(path);
                }
            } else {
                found = CheckIndexedFileExists(info->mName, access, 1);
            }
            if (found != 0) info->mFolder = header->mPaths[gTexturePathIndex];
        }
        if (sharedFolder != nullptr && found == 0) {
            found = 1;
            info->mFolder = header->mPaths[gSharedPathIndex];
        }
        if (found == 0) {
            // The original builds a "Could not load texture" message here and drops it.
            info->mFolder = header->mPaths[0];
        }
    }
}

// 0x100b9180: b's box takes a's; then each of its two corners (3 floats at 0x80 and 0x8c) is carried by the matrix at +0x10 of b's node
// (mF78): row 0 at +0, row 1 at +0x10, row 2 at +0x20, translation at +0x30.
static void CarryCorner(const float* m, float* p) {
    float x = p[0], y = p[1], z = p[2];
    float a = y * m[4];
    float b = y * m[5];
    float c = y * m[6];
    a = ppc_fmadds(x, m[0], a);
    b = ppc_fmadds(x, m[1], b);
    c = ppc_fmadds(x, m[2], c);
    a = ppc_fmadds(z, m[8], a);
    b = ppc_fmadds(z, m[9], b);
    c = ppc_fmadds(z, m[10], c);
    a = m[12] + a;
    b = m[13] + b;
    c = m[14] + c;
    p[0] = a;
    p[1] = b;
    p[2] = c;
}

void SetWorldBoundingArea(tag_sM3D2Header* a, tag_sM3D2Header* b) {
    uint8_t* pa = reinterpret_cast<uint8_t*>(a);
    uint8_t* pb = reinterpret_cast<uint8_t*>(b);
    std::memmove(pb + 0x80, pa + 0x80, 0x18);   // (a and b may be the same header)
    const float* m = reinterpret_cast<const float*>(reinterpret_cast<uint8_t*>(b->mF78.get()) + 0x10);
    CarryCorner(m, reinterpret_cast<float*>(pb + 0x80));
    m = reinterpret_cast<const float*>(reinterpret_cast<uint8_t*>(b->mF78.get()) + 0x10);
    CarryCorner(m, reinterpret_cast<float*>(pb + 0x8c));
}

// The height field's node gamuts (main application, the rsse.cpp range of the inventory):
// for every node of the terrain mesh, the rectangle of height field cells its box covers,
// kept in four signed bytes of the node (mF90: Z high, Z low, X high, X low) so that the
// map can tell which nodes to draw. SetupHFieldNodeGamuts 0x100b971c starts at the mesh's
// root node with the identity matrix; DoCalcNodeHFieldGamut 0x100b9304 does one node and
// its children.

// 0x1029eb78..0x1029eb84 (names invented): the limits the four indices are held to.
int32_t gNodeGamutMinX = -128;   // 0x1029eb78
int32_t gNodeGamutMaxX = 127;    // 0x1029eb7c
int32_t gNodeGamutMinZ = -128;   // 0x1029eb80
int32_t gNodeGamutMaxZ = 127;    // 0x1029eb84

namespace {
constexpr uint32_t kNodeNoBox = 0x200;            // the node has no box of its own (children only)
constexpr uint32_t kNodeSkipped = 0x40000000;     // the node and its children are left out
constexpr uint32_t kNodeGamutSet = 0x2000;
constexpr uint32_t kNodeOutsideField = 0x4000000;

// fctiwz of a single-precision quotient, as the original's (int) cast of a float.
int32_t Cell(float quotient) {
    return ppc_fctiwz(static_cast<double>(quotient));
}

// The first cell: the quotient truncated toward zero, less one when the POSITION (not the
// quotient) is below zero, as the original tests it. With an origin other than zero the
// two tests differ, and that is kept.
int32_t LowCell(float position, float origin, float size) {
    const float quotient = (position - origin) / size;
    const int32_t cell = Cell(quotient);
    return position < 0.0f ? static_cast<int32_t>(static_cast<uint32_t>(cell) - 1u) : cell;
}

// The last cell: 0.999 is added to the quotient before it is truncated.
int32_t HighCell(float position, float origin, float size) {
    const float kAlmostOne = 0.999f;   // 0x10264df8
    const float quotient = (position - origin) / size;
    return Cell(kAlmostOne + quotient);
}
}  // namespace

// The node's matrix goes into `matrix` (the destination of CombineMatricesNoWDest2nd, the
// parameter's own copy), the node's box corners go through it, and each axis is put in
// order. The cells are limited by the four globals and stored as bytes (truncated, so a
// value outside -128..127 wraps) unless the node has flag 0x200. Flag 0x4000000 is set
// before the limits apply, when the unlimited box reaches below cell 0 or past the last
// cell. The children, except those with flag 0x40000000, follow with the combined matrix.
void DoCalcNodeHFieldGamut(tag_sNode* node, sHField* field, sMatrix matrix) {
    CombineMatricesNoWDest2nd(&node->mMatrix, &matrix);
    if ((node->mFlags & kNodeNoBox) == 0) {
        s3DPoint low = {node->mBox.mMin[0], node->mBox.mMin[1], node->mBox.mMin[2]};
        s3DPoint high = {node->mBox.mMax[0], node->mBox.mMax[1], node->mBox.mMax[2]};
        VectorThroughLMatrixNoW(&matrix, &low);
        VectorThroughLMatrixNoW(&matrix, &high);
        // fcmpo and ble: a NaN on either side leaves the pair as it is. The y pair is ordered too,
        // though nothing reads it afterwards.
        if (low.x > high.x) std::swap(low.x, high.x);
        if (low.y > high.y) std::swap(low.y, high.y);
        if (low.z > high.z) std::swap(low.z, high.z);
        // sHField: mF08 is the origin (x, z) then the cell size (x, z); mF18 the cell counts.
        int32_t firstX = LowCell(low.x, field->mF08[0], field->mF08[2]);
        int32_t firstZ = LowCell(low.z, field->mF08[1], field->mF08[3]);
        int32_t lastX = HighCell(high.x, field->mF08[0], field->mF08[2]);
        int32_t lastZ = HighCell(high.z, field->mF08[1], field->mF08[3]);
        if (firstX < 0 || firstZ < 0 || lastX >= static_cast<int32_t>(field->mF18[0]) ||
            lastZ >= static_cast<int32_t>(field->mF18[1]))
            node->mFlags |= kNodeOutsideField;
        firstX = std::max(gNodeGamutMinX, firstX);
        lastX = std::min(lastX, gNodeGamutMaxX);
        firstZ = std::max(gNodeGamutMinZ, firstZ);
        lastZ = std::min(lastZ, gNodeGamutMaxZ);
        node->mFlags |= kNodeGamutSet;
        node->mF90[3] = static_cast<uint8_t>(firstX);
        node->mF90[2] = static_cast<uint8_t>(lastX);
        node->mF90[1] = static_cast<uint8_t>(firstZ);
        node->mF90[0] = static_cast<uint8_t>(lastZ);
    }
    // mF0C is the first child and mF08 the next sibling of the same parent.
    for (tag_sNode* child = node->mF0C; child != nullptr; child = child->mF08)
        if ((child->mFlags & kNodeSkipped) == 0) DoCalcNodeHFieldGamut(child, field, matrix);
}

void SetupHFieldNodeGamuts(tag_sM3D2Header* header) {
    sMatrix identity;
    SetMatrixIdentity(&identity);
    DoCalcNodeHFieldGamut(header->mF78, header->mHField, identity);
}

// The park's second-stage systems that LevelInit2 and LevelDeInit call (main application, unit w111): the attribute map, the poly
// search table, the heightmap system's words, the level lights' teardown, the highlight colour buffers, the coaster types and the
// data error handler. Each function keeps the original's order of calls and stores.

namespace {

// What the Mac's DataErrorHandler titles its box (0x1023ebf6).
const char kDataErrorCaption[] = "M3D2 Exporter - Data Error";

}  // namespace

// 0x100b9834 (72 bytes, rsse.cpp). The Mac's box has OK and Cancel (flags 0x31) and answers false for Cancel; the port logs the
// text and answers true, as a player clicking OK.
bool DataErrorHandler(char* text) {
    SamsUtilities::UDebug("%s: %s", kDataErrorCaption, text);
    return true;
}

// The movie player's range of the main app (SimTheme Park 2.04b3; the inventory files it
// under rsse.cpp): MOVIE_Play 0x100b9914, MOVIE_Tick 0x100b9bcc, MOVIE_Stop 0x100b9c48,
// MOVIE_SetVolume 0x100b9cbc, MOVIE_Init 0x100b9d10, MOVIE_Close 0x100b9d58, and their
// neighbour CheckFileExists (0x100b98a4). One movie at a time, played by QuickTime into the
// game's window.

int32_t CheckFileExists(char* name) {
    bfl::FileSystem::CFile* file = TPFile_Open(name, "rb");
    if (file != nullptr) TPFile_Close(file);
    return file != nullptr;
}
