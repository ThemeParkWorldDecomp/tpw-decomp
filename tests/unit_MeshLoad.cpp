// Test library for src/MeshLoad.cpp (libtpw_unit_MeshLoad.dylib; see
// tests/test_MeshLoad.py): the game's mesh paths, LoadMesh and the two functions that make
// an engine mesh object, driven by scripts over a mesh image the test lays out.
//
// Fakes that record: the mesh file loader, the engine mesh object (construction and
// Init), the game's allocator (it records the size the Mac asks for: the tables hold
// pointers, four bytes there), CheckFileExists, CheckIndexedFileExists.
#include <cstdlib>
#include <cstring>

#include "Main.h"
#include "Quickload.h"
#include "TPEngine.h"
#include "TPMovie.h"
#include "calllog.h"
#include "platform/arena.h"
#include "rsse.h"
#include "tb_mesh.h"
#include "ui_ErrorDisplay.h"

using namespace calllog;

namespace {
tag_sM3D2Header* gHeader;
tag_sMesh* gMeshes[2];
bool gFoldersValid;   // the textures' folders point into tables that still exist
char gThemePath[32] = "disc:levels:fantasy";
const char* const kFolders[] = {"rides:coaster", "rides:coaster:", "textures", "shared:", "x"};
}  // namespace

char gSharedTextureFolder[16] = "sharedtex";
int32_t gQuickloadEnabled;
char* get_theme_path() { return gThemePath; }

// The game's allocator: blocks from the arena (a table of folders is what a mesh
// header's 32-bit pointer field points at). Recorded as the size the Mac asks for.
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    uint32_t mac = size;
    if (flags == 1 && size == sizeof(tag_sMesh)) mac = 0x40;
    if (flags == 0) mac = size - static_cast<uint32_t>(gMeshPathCount + 1) * (sizeof(char*) - 4);
    ev("Alloc", {mac, flags});
    void* block = tpw::ArenaAlloc(size + 16);
    std::memset(block, (flags & 1) != 0 ? 0 : 0xa5, size + 16);
    newlab(block);
    return block;
}
void* MReleaseFreeF(void* block) {
    ev("Free", {lab(block)});
    forget(block);
    tpw::ArenaFree(block);
    return nullptr;
}
int32_t CheckFileExists(char* name) {
    ev("CheckFileExists", {}, {name});
    return static_cast<int32_t>(ret("Exists", 0));
}
int32_t CheckIndexedFileExists(const char* name, int32_t path, int32_t textures) {
    ev("CheckIndexedFileExists", {path, textures}, {name});
    return static_cast<int32_t>(ret("Indexed", 0));
}
tag_sM3D2Header* LoadM3D2Header(char* name, uint32_t flags) {
    ev("LoadM3D2Header", {flags}, {name});
    return ret("Load", 1) ? gHeader : nullptr;
}

// ---- the engine mesh object: a label, and what it was initialised with
CMapLinkBase::~CMapLinkBase() {}
CMesh::CMesh() {
    newlab(this);
    ev("Mesh.ctor", {lab(this)});
}
CMesh::~CMesh() {}
int32_t CMesh::Init(tag_sM3D2Header* header) {
    ev("Mesh.Init", {lab(this), header == nullptr ? 0 : header == gHeader ? 1 : -1});
    return 1;
}
int32_t CMesh::Prepare() { return 0; }
void* CMesh::GetData() { return nullptr; }
int32_t CMesh::GetLinkType() { return 2; }

extern "C" {
void* tpw_ml_arena_base() { return tpw::ArenaBase(); }
void* tpw_ml_alloc(uint32_t size) { return tpw::ArenaAllocZeroed(size); }
void tpw_ml_reset(void* header) {
    gHeader = static_cast<tag_sM3D2Header*>(header);
    gMeshes[0] = gMeshes[1] = nullptr;
    gMeshPathAllocs = nullptr;
    gFileAccessIndex = 0;
    gMeshPathCount = 0;
    gMeshPaths = nullptr;
    gMeshPathIndex = gTexturePathIndex = gSharedPathIndex = -1;
    gQuickloadEnabled = 0;
    gFoldersValid = false;
}

static char* Folder(int32_t which) {
    static char texts[4][32];
    static int32_t next;
    if (which < 0) return nullptr;
    char* text = texts[next++ & 3];
    std::strcpy(text, kFolders[which]);
    return text;
}

int64_t tpw_ml_op(int32_t op, const int32_t* a, char* text) {
    switch (op) {
    case 1: {
        char root[16];
        std::strcpy(root, a[0] ? "disc:" : "hd:data:");
        SetNewMeshPaths(root, Folder(a[1]), Folder(a[2]), Folder(a[3]));
        return 0;
    }
    case 2:
        DestroyPathAllocs();
        gFoldersValid = false;
        return 0;
    case 3: {
        const char* path = GetMeshPath();
        std::strcpy(text, path != nullptr ? path : "(null)");
        return 0;
    }
    case 4: SetFileAccessIndex(a[0]); return GetFileAccessIndex();
    case 5: BuildSharedTexturePath(text); return 0;
    case 6:   // the result: whether the header now has the current table
        OverrideTexturePaths(a[0] ? gHeader : nullptr, a[1], a[2]);
        if (a[0]) gFoldersValid = true;
        return a[0] != 0 && gHeader->mPaths.get() == gMeshPaths;
    case 7: {
        char name[32];
        std::strcpy(name, "rides:coaster:car.md2");
        tag_sMesh* mesh = LoadMesh(name, nullptr, a[1], a[2], a[3]);
        if (mesh != nullptr) {
            gMeshes[a[0] & 1] = mesh;
            gFoldersValid = true;
        }
        return lab(mesh);
    }
    case 8: {
        tag_sMesh* mesh = CreateMeshFromM3D2Header(a[1] ? gHeader : nullptr);
        if (mesh != nullptr) gMeshes[a[0] & 1] = mesh;
        return lab(mesh);
    }
    case 9: CreateEngineMesh(gMeshes[a[0] & 1]); return 0;
    case 10: CreateMeshTypeEngineMesh(gMeshes[a[0] & 1]); return 0;
    case 11: gQuickloadEnabled = a[0]; return 0;
    case 12: gMeshes[a[0] & 1]->mF08 = a[1] ? gMeshes[~a[0] & 1] : nullptr; return 0;
    }
    return -99;
}

// The state in the order of PpcSide.state in the Python module; texts go to `texts`, one
// per line.
int32_t tpw_ml_state(int64_t* out, char* texts) {
    int32_t n = 0;
    texts[0] = 0;
    const auto text = [&](const char* value) {
        std::strcat(texts, value != nullptr ? value : "(null)");
        std::strcat(texts, "\n");
    };
    out[n++] = gMeshPathCount;
    out[n++] = gMeshPathIndex;
    out[n++] = gTexturePathIndex;
    out[n++] = gSharedPathIndex;
    out[n++] = gFileAccessIndex;
    int32_t tables = 0;
    for (void* table = gMeshPathAllocs; table != nullptr; table = *static_cast<void**>(table)) ++tables;
    out[n++] = tables;
    out[n++] = lab(gMeshPathAllocs);
    for (int32_t index : {gMeshPathIndex, gTexturePathIndex, gSharedPathIndex}) {
        text(index != -1 && gMeshPaths != nullptr ? gMeshPaths[index] : nullptr);
    }
    const tag_sM3D2Header* h = gHeader;
    out[n++] = !h->mPaths ? 0 : 1;   // (the table it points at may have been freed since)
    out[n++] = h->mF34;
    for (int32_t i = 0; i < h->mTextureCount; ++i) text(gFoldersValid ? h->mTextureInfos[i].mFolder.get() : "(stale)");
    for (const tag_sMesh* mesh : gMeshes) {
        out[n++] = lab(mesh);
        if (mesh == nullptr) continue;
        out[n++] = mesh->mF00;
        out[n++] = mesh->mHeader == nullptr ? 0 : mesh->mHeader == gHeader ? 1 : -1;
        out[n++] = lab(mesh->mEngineMesh);
        out[n++] = lab(mesh->mTypeEngineMesh);
        out[n++] = mesh->mEngineMeshes;
        out[n++] = lab(mesh->mF08);
    }
    return n;
}
}
