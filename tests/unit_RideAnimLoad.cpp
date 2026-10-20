#include "TPEngine.h"
#include "TPGlobals.h"
#include "Quickload.h"
#include "Debug.h"
#include "FS_System.h"
#include "Main.h"
#include "rsse.h"
#include "platform/arena.h"
#include "calllog.h"
#include "fake_strings.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace calllog;
namespace {
constexpr char kinds[] = "CDILSMEUWBRO";
std::vector<void*> blocks;
uint32_t catalog[12];
tag_sMesh mesh;
tag_sM3D2Header* header;
tag_sNode* nodes;
tag_sRideAnimSet* output;
int allocIndex, loadIndex;
uint32_t timeCount;
int loadMesh;
bool nullPath;
uint32_t levelFlags;
char loaded[1040];
void* Alloc(size_t bytes) { void* p = tpw::ArenaAllocZeroed(bytes + 16); blocks.push_back(p); return p; }
bool Exists(const char* name, bool loading) {
    for (int i = 0; i < 12; ++i) {
        char numbered[16], single[16];
        std::snprintf(single, sizeof single, "%c.md2", kinds[i]);
        const size_t n = std::strlen(name), s = std::strlen(single);
        if (n >= s && std::strcmp(name + n - s, single) == 0) return (catalog[i] & 16) != 0;
        for (uint32_t j = 1; j <= (catalog[i] & 15); ++j) {
            std::snprintf(numbered, sizeof numbered, "%c%u.md2", kinds[i], j);
            const size_t z = std::strlen(numbered);
            if (n >= z && std::strcmp(name + n - z, numbered) == 0) return !(loading && (catalog[i] & 32) != 0);
        }
    }
    return false;
}
}
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
int32_t gQuickloadEnabled;
char* GetMeshPath() { static char path[] = "disc:models:"; return nullPath ? nullptr : path; }
tag_sMesh* LoadMesh(char* name, void*, int32_t noEngine, int32_t flags, int32_t one) {
    ev("Mesh", {noEngine, flags, one}, {name});
    return loadMesh && (loadMesh != 2 || std::strstr(name, "TestBase") != nullptr) ? &mesh : nullptr;
}
void UpdateScratchGeomBufferSize(tag_sMesh*) { ev("Scratch"); }
int32_t DoesFileExist(char* name, uint32_t flags) { ev("Exists", {flags}, {name}); return Exists(name, false); }
tag_sM3D2Header* LoadM3D2Header(char* name, uint32_t flags) {
    ev("AnimHeader", {flags}, {name});
    if (!Exists(name, true)) return nullptr;
    auto* h = static_cast<tag_sM3D2Header*>(Alloc(sizeof(tag_sM3D2Header)));
    auto* a = static_cast<tag_sAnimHeader*>(Alloc(sizeof(tag_sAnimHeader)));
    h->mAnim = a;
    tpw_cl_label(100 + loadIndex, h); tpw_cl_label(200 + loadIndex++, a);
    return h;
}
void DestroyMeshHeader(tag_sM3D2Header* h) { ev("DestroyHeader", {lab(h)}); }
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    const uint32_t count = allocIndex < 2 ? size / sizeof(void*) : allocIndex == 2 ? size / sizeof(sRideAnimEntry) : allocIndex == 3 ? timeCount : size;
    ev("Alloc", {allocIndex++, count, flags}); return Alloc(size);
}
namespace NS_MacDoze { int _stricmp(const char* a, const char* b) { return ::strcasecmp(a, b); } }
void ERRORBox(const char* format, ...) {
    char text[300]; va_list args; va_start(args, format); std::vsnprintf(text, sizeof text, format, args); va_end(args);
    ev("ErrorBox", {}, {text});
}
int32_t RAInitType(tag_sMesh*, tag_sAnimHeader* animation) { ev("InitType", {lab(animation)}); return 1; }
void OverrideActiveChannels(tag_sRideAnimSet*) { ev("Channels"); }
void StripRideWrapping(tag_sRideAnimSet*, tag_sNode* n, float amount, void* p, s3DPoint* v) { ev("Strip", {n != nullptr, fbits(amount), p != nullptr, v != nullptr}); }
void CorrectNodeFlags1(tag_sRideAnimSet*) { ev("CorrectFlags"); }
void CreateMeshTypeEngineMesh(tag_sMesh*) { ev("TypeMesh"); }
void CreateEngineMesh(tag_sMesh*) { ev("EngineMesh"); }
int32_t InitRideAnimSet(tag_sRideAnimSet* set, int32_t a, int32_t b, int32_t c) { ev("InitSet", {a, b, c}); set->mFlags |= levelFlags; return 1; }
void TPInitialiseEmbeddedData(tag_sMesh*) { ev("Embedded"); }
CSYS_TPFileInfo gTPFileInfo;
const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetLevelPath() const { return mLevelPath; }
uint32_t gGameOptions;
char* get_theme_path() { static char theme[] = "Data:Fantasy"; return theme; }
void BuildSharedTexturePath(char* out) { std::strcpy(out, "Data:Fantasy:sharetex"); ev("SharedPath"); }
int32_t SetupDirectoryForMeshLoad(char* file, char* folder, char* shared) { ev("Directory", {}, {file, folder, shared}); return 1; }
int32_t BuildHFieldTextureTable(tag_sM3D2Header*) { ev("HFieldTextures"); return 1; }
void SetupHFieldNodeGamuts(tag_sM3D2Header*) { ev("NodeGamuts"); }
void DetermineHFieldFlatness(tag_sM3D2Header*) { ev("Flatness"); }
void LoadProceduralTextureSet(int32_t mode) { ev("Procedural", {mode}); }
void EnableMeshDraw(tag_sMesh*) { ev("Enable"); }
void EnableMeshDraw(tag_sMesh*, HFieldXZ from, HFieldXZ to) { ev("EnableRange", {from.mF00, from.mF02, to.mF00, to.mF02}); }
void* MReleaseFreeF(void* p) { ev("Free", {lab(p)}); return nullptr; }
void DestroyRideAnimSet(tag_sRideAnimSet*, int32_t mode) { ev("DestroySet", {mode}); }
void FreeHFieldTextureTable() { ev("FreeTextures"); }
extern "C" {
void tpw_ral_begin(const uint32_t* seed) {
    for (auto* p : blocks) tpw::ArenaFree(p); blocks.clear();
    header = static_cast<tag_sM3D2Header*>(Alloc(sizeof(tag_sM3D2Header)));
    nodes = static_cast<tag_sNode*>(Alloc(3 * sizeof(tag_sNode)));
    auto* field = static_cast<sHField*>(Alloc(sizeof(sHField)));
    field->mF18[0] = 3; field->mF18[1] = 2;
    field->mHeightCount = 12; field->mQuadCount = 6;
    auto* heights = static_cast<float*>(Alloc(48));
    auto* quads = static_cast<sHFieldQuad*>(Alloc(24));
    for (int i = 0; i < 12; ++i) heights[i] = i * 0.5f;
    for (int i = 0; i < 6; ++i) quads[i] = {static_cast<uint16_t>(i * 17), static_cast<uint16_t>(i * 9)};
    field->mHeights = heights; field->mQuads = quads;
    header->mFlags = seed[0]; header->mNodes = nodes; header->mF78 = nodes; header->mHField = field; header->mNodeCount = 3;
    std::strcpy(header->mName, "test-base");
    const char* names[] = {"Sky", "grass", "SKY"};
    for (int i = 0; i < 3; ++i) {
        char* s = static_cast<char*>(Alloc(8)); std::strcpy(s, names[i]); nodes[i].mName = s; nodes[i].mFlags = 0x40 + i;
    }
    std::memset(&mesh, 0, sizeof mesh); mesh.mHeader = header;
    gTPGlobals.mEngineFlags = seed[1]; gQuickloadEnabled = seed[2];
    loadMesh = seed[3]; nullPath = seed[4]; timeCount = seed[5];
    for (int i = 0; i < 12; ++i) catalog[i] = seed[6 + i];
    output = reinterpret_cast<tag_sRideAnimSet*>(0x12121212);
    allocIndex = loadIndex = 0; std::strcpy(loaded, "unchanged");
    levelFlags = 0;
}
int32_t tpw_ral_run(char* name, uint32_t flags, int32_t wantName) { return LoadRideAnimSet(name, flags, &output, wantName ? loaded : nullptr, timeCount); }
int32_t tpw_ral_state(uint32_t* out) {
    int n = 0;
    out[n++] = header->mFlags;
    for (int i = 0; i < 3; ++i) out[n++] = nodes[i].mFlags;
    for (auto v : header->mFA8) out[n++] = v;
    const bool valid = output != reinterpret_cast<tag_sRideAnimSet*>(0x12121212);
    out[n++] = valid;
    if (!valid) return n;
    out[n++] = output->mSelf == output; out[n++] = output->mFlags; out[n++] = output->mHeads == &mesh;
    out[n++] = output->mF0C; out[n++] = output->mTimeCount;
    out[n++] = output->mTimeControls == reinterpret_cast<uint8_t*>(output) + sizeof(tag_sRideAnimSet);
    out[n++] = output->mF14 != nullptr;
    for (const auto& g : output->mGroups) {
        out[n++] = g.mFlags; out[n++] = g.mCount; out[n++] = g.mEntries != nullptr;
        for (uint32_t i = 0; i < g.mCount; ++i) { out[n++] = g.mEntries[i].mIndex; out[n++] = lab(g.mEntries[i].mAnimation); }
    }
    out[n++] = output->mAnimationCount;
    for (uint32_t i = 0; i < output->mAnimationCount; ++i) { out[n++] = lab(output->mAnimations[i]); out[n++] = lab(output->mAnimationHeaders[i]); }
    return n;
}
const char* tpw_ral_name() { return loaded; }
int32_t tpw_ral_level(int32_t useLevel, int32_t mode, uint32_t options, uint32_t flags) {
    gTPFileInfo.mLevelPath = "Data:Lobby";
    gGameOptions = options; gHeightfieldRideSet = nullptr; gHeightfieldCopy = nullptr;
    levelFlags = flags;
    gHeightfieldHeader = nullptr; gHeightfieldQuads = nullptr;
    const int32_t result = LoadLevelHeightfield(useLevel, mode);
    output = gHeightfieldRideSet ? gHeightfieldRideSet : reinterpret_cast<tag_sRideAnimSet*>(0x12121212);
    return result;
}
int32_t tpw_ral_copy(uint32_t* out) {
    int n = 0; out[n++] = gHeightfieldHeader != nullptr; out[n++] = gHeightfieldQuads != nullptr; out[n++] = gHeightfieldCopy != nullptr;
    if (gHeightfieldCopy) {
        out[n++] = gHeightfieldCopy->mHeightCount; out[n++] = gHeightfieldCopy->mQuadCount;
        out[n++] = gHeightfieldCopy->mHeights.get() == reinterpret_cast<float*>(gHeightfieldCopy + 1);
        out[n++] = gHeightfieldCopy->mQuads.get() == reinterpret_cast<sHFieldQuad*>(reinterpret_cast<uint8_t*>(gHeightfieldCopy + 1) + 48);
        for (int i = 0; i < 12; ++i) out[n++] = fbits(gHeightfieldCopy->mHeights[i]);
        for (int i = 0; i < 6; ++i) { out[n++] = gHeightfieldCopy->mQuads[i].mF00; out[n++] = gHeightfieldCopy->mQuads[i].mF02; }
    }
    return n;
}
void tpw_ral_destroy_level() { DestroyLevelHeightfield(); }
}
