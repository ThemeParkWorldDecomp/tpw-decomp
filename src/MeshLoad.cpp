// Loading meshes (main program of SimTheme Park 2.04b3): LoadMesh, CreateMeshTypeEngineMesh
// and the mesh paths (0x100b7d04..0x100b9140, which the inventory files under rsse.cpp),
// CreateMeshFromM3D2Header and CreateEngineMesh (0x10064cf0, 0x10064f40, TPRide.cpp there).
#include <cstdio>
#include <cstring>

#include "Main.h"
#include "Quickload.h"
#include "TPEngine.h"
#include "TPMovie.h"
#include "rsse.h"
#include "tb_mesh.h"
#include "ui_ErrorDisplay.h"

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

tag_sMesh* CreateMeshFromM3D2Header(tag_sM3D2Header* header) {
    if (header == nullptr) return nullptr;
    auto* mesh = static_cast<tag_sMesh*>(MReleaseAllocF(sizeof(tag_sMesh), 1));
    mesh->mF00 = 0;
    mesh->mHeader = header;
    return mesh;
}

void CreateEngineMesh(tag_sMesh* mesh) {
    CMesh* engine = new CMesh;
    engine->Init(mesh->mHeader);
    if (mesh->mF08 == nullptr) ++mesh->mEngineMeshes;
    else ++mesh->mF08->mEngineMeshes;
    mesh->mEngineMesh = engine;
}

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
