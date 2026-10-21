// The fakes for the engine start-up glue (src/game/TPEngine.cpp, src/game/TPInstance.cpp, src/game/TPMesh.cpp, src/game/park/TPHField.cpp, src/game/rides/TPRide.cpp; libtpw_unit_TPEngine.dylib; see
// calllog.h) and the test entry points. Each fake records the event its PowerPC twin in
// test_TPEngine.py records; they are functional where a value flows back (strings, the
// rectangle SetRect fills, the OpenGL context, the resource handle, heap blocks).
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "CBalanceTable.h"
#include "Clock.h"
#include "ExitErr.h"
#include "FS_System.h"
#include "Game.h"
#include "GameSys.h"
#include "MapWho.h"
#include "OptionsBalance.h"
#include "TPCamera.h"
#include "TPConfig.h"
#include "TPDialog.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "TbDynamicStringTemplate.h"
#include "Wavelet.h"
#include "calllog.h"
#include "platform/indirectx.h"
#include "platform/interfacelib.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "rsse.h"
#include "tb_ddraw.h"
#include "tb_hfield.h"
#include "tb_init.h"
#include "tb_matrix.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_sky.h"
#include "tb_texturecache.h"

using calllog::ev;
using calllog::fbits;
using calllog::lab;
using calllog::newlab;
using calllog::ret;

extern "C" {
char tpw_te_data_path[160];       // what GetDataPath returns
uint8_t tpw_te_resource[4];       // the 'DOPT' bytes GetNamedResource hands out
}

namespace {

char* dup(const char* text) {
    const std::size_t n = std::strlen(text) + 1;
    char* out = static_cast<char*>(std::malloc(n));
    std::memcpy(out, text, n);
    return out;
}

std::string pascal(const unsigned char* text) {
    return text == nullptr ? std::string("(null)") : std::string(reinterpret_cast<const char*>(text + 1), text[0]);
}

const char* text_or_null(const char* text) { return text != nullptr ? text : "(null)"; }

}  // namespace

// The ids of the hook functions, the same on both sides (test_TPEngine.py HOOKS).
extern "C" int32_t tpw_te_hook_id(const void* f) {
    const void* const kHooks[] = {
        nullptr,
        reinterpret_cast<const void*>(&MacOpen),
        reinterpret_cast<const void*>(&MacClose),
        reinterpret_cast<const void*>(&MacRead),
        reinterpret_cast<const void*>(&MacSeek),
        reinterpret_cast<const void*>(&MacSize),
        reinterpret_cast<const void*>(static_cast<char* (*)(const char*, int32_t)>(&FindFilePath)),
        reinterpret_cast<const void*>(&MacIsFirstPerson),
        reinterpret_cast<const void*>(&UseSmallTextures),
        reinterpret_cast<const void*>(&MacReadClock),
        reinterpret_cast<const void*>(&MacUpdateMouse),
        reinterpret_cast<const void*>(&MacMalloc),
        reinterpret_cast<const void*>(&MacFree),
        reinterpret_cast<const void*>(&PAR_InitParticleSystem),
        reinterpret_cast<const void*>(&PAR_CloseParticleSystem),
        reinterpret_cast<const void*>(&PAR_LoadLibrary),
        reinterpret_cast<const void*>(&PAR_BuildDrawingList),
        reinterpret_cast<const void*>(&PAR_FreeDrawingList),
        reinterpret_cast<const void*>(&PAR_GetEmitterParticleCount),
        reinterpret_cast<const void*>(&PAR_GetEmitterBoundingBox),
    };
    if (f == nullptr) return 0;
    for (int32_t i = 1; i < static_cast<int32_t>(sizeof(kHooks) / sizeof(kHooks[0])); ++i)
        if (kHooks[i] == f) return i;
    return -1;
}

// ------------------------------------------------------------------ bullfrog strings (functional)

template<> TbDynamicStringTemplate<char>::TbDynamicStringTemplate()
    : mGrowBy(0) {
    mCapacity = 0;
    mData = dup("");
    mLength = 0;}
template<> TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text)
    : mGrowBy(0) {
    mCapacity = 0;
    mData = dup(text);
    mLength = static_cast<int32_t>(std::strlen(text));}
template<> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {}
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(
    const TbDynamicStringTemplate<char>& other) {
    mData = dup(other.mData);
    mLength = other.mLength;
    return *this;
}
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>& text) {
    const auto& other = static_cast<const TbDynamicStringTemplate<char>&>(text);
    std::string joined = std::string(mData) + other.mData;
    mData = dup(joined.c_str());
    mLength = static_cast<int32_t>(joined.size());
    return *this;
}
template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate()
    : mGrowBy(0) {
    mCapacity = 0;
    mData = nullptr;
    mLength = 0;}
template<> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {}

// ------------------------------------------------------------------ globals of other files

uint32_t gGameOptions;
uint32_t gTPDialogOptions;
uint32_t gTPDialogTextureMegabytes;
GrafPort* gMainWindow;
int32_t gScreenWidth;
int32_t gScreenHeight;
uint8_t gNoSavedSettings;
uint32_t gFirstPersonFlags;
tag_sRideAnimSet* gFirstPersonRideSet;
tag_sRideAnimSet* gHeightfieldRideSet;
tag_sRideAnimSet* gRideAnimSets[16];
uint32_t gRideCountA;
uint32_t gRideCountB;
uint32_t gMaxMeshFaces4;
uint32_t gMaxMeshF3E;
uint32_t gMaxMeshNodes;
CClock gClock;
// The game object here is a blank one: the unit sets the fields its code reads.
CGame::CGame() {}
CGame::~CGame() {}
CGame gGame;
CSYS_TPFileInfo gTPFileInfo;
// The unit starts from a blank object, not the game's default names.
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
CExitError gExitError;
CPlayerSettings gPlayerSettings;
// TPSettings.cpp's own members this unit does not test (unrecorded).
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}
COptionsBalance gOptionsBalance;
CTextureSettings gei_TextureSettings;
CDisplay gei_Display;
CDisplay::CDisplay() {}   // the unit keeps a blank display of its own
CSystem gei_System;
CSky::CSky() {}
CSky::~CSky() {}
CSky gei_Sky;
CTextureCache* gei_TCache;
CDirectDraw* gei_DDraw;
CMatrix* gei_ViewMatrix;
CMatrix gei_ProjectionMatrix;
CMapWho* gei_MapWho;
CParticle* gei_Particle;
sAlignedBox gs_WorldBoundingArea;

CVariableTime::CVariableTime() {}
CFixedTime::CFixedTime() {}
void CBalanceTable::Init() {}
COptionsBalance::~COptionsBalance() {}
const BalanceDescription* COptionsBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* COptionsBalance::GetBalanceValuesStart() { return mValues; }
CTextureSettings::CTextureSettings() : mFlags(0) {}
CSystem::CSystem() {}
IdxDirectDraw::~IdxDirectDraw() {}

// ------------------------------------------------------------------ callees

int32_t CClock::Read() {
    ev("CClock::Read", {lab(this)});
    return static_cast<int32_t>(ret("CClock::Read"));
}
void CGame::SetCurrentTheme(int32_t theme) { ev("CGame::SetCurrentTheme(long)", {lab(this), theme}); }
const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetDataPath() const {
    static TbDynamicStringTemplate<char> sPath;
    ev("CSYS_TPFileInfo::GetDataPath", {lab(this)});
    sPath = TbDynamicStringTemplate<char>(tpw_te_data_path);
    return sPath;
}
void FS_AddDirectorySeparator(TbDynamicStringTemplate<char>& path) {
    ev("FS_AddDirectorySeparator", {}, {path.mData});
    path += TbDynamicStringTemplate<char>(":");
}
int32_t CBalanceTable::Load(const char* path, LoadMode mode) {
    ev("CBalanceTable::Load", {lab(this), mode}, {path});
    return static_cast<int32_t>(ret("CBalanceTable::Load", 1));
}
void CExitError::SetError(ErrorType type, uint32_t value, const char* text) {
    ev("CExitError::SetError", {lab(this), type, value}, {text_or_null(text)});
    mError = 1;
}
void CPlayerSettings::SetRendererType(eRENDERER type) { ev("CPlayerSettings::SetRendererType", {lab(this), type}); }
void CPlayerSettings::SetVideoCard(eVIDEOCARD card) { ev("CPlayerSettings::SetVideoCard", {lab(this), card}); }
void CPlayerSettings::SetGraphicsDetail(eDETAIL detail) {
    ev("CPlayerSettings::SetGraphicsDetail", {lab(this), detail});
}
void CSky::SetActiveLayers(int32_t layers) { ev("CSky::SetActiveLayers", {lab(this), layers}); }
void CDisplay::SetFogSpace(float nearDistance, float farDistance) {
    ev("CDisplay::SetFogSpace", {lab(this), fbits(nearDistance), fbits(farDistance)});
}
void CMatrix::Projection(float nearPlane, float farPlane, float fov) {
    ev("CMatrix::Projection", {lab(this), fbits(nearPlane), fbits(farPlane), fbits(fov)});
}
void CMapWho::Render() { ev("CMapWho::Render", {lab(this)}); }
int32_t RenderSystem_Init(int32_t mode) {
    ev("RenderSystem_Init", {mode});
    return static_cast<int32_t>(ret("RenderSystem_Init", 1));
}
int32_t RenderSystem_Close() {
    ev("RenderSystem_Close");
    return 1;
}
int32_t RenderSystem_FlipScreen() {
    ev("RenderSystem_FlipScreen");
    return 1;
}
int32_t DDraw_Init(GrafPort* window, int32_t width, int32_t height, int32_t depth, int32_t a, int32_t b) {
    ev("DDraw_Init", {lab(window), width, height, depth, a, b});
    return static_cast<int32_t>(ret("DDraw_Init", 1));
}
int32_t DDraw_Close() { ev("DDraw_Close"); return 1; }
int32_t EngineAdd_Init(int32_t textureFormat, int32_t version, char* name) {
    ev("EngineAdd_Init", {textureFormat, version, lab(name)});
    return static_cast<int32_t>(ret("EngineAdd_Init", 1));
}
int32_t EngineAdd_Close() { ev("EngineAdd_Close"); return 1; }
void SetFileCallbacks(TbFileOpenFn open, TbFileCloseFn close, TbFileReadFn read, TbFileSeekFn seek, TbFileTellFn tell,
                      TbFileNameFn name) {
    ev("SetFileCallbacks",
       {tpw_te_hook_id(reinterpret_cast<const void*>(open)), tpw_te_hook_id(reinterpret_cast<const void*>(close)),
        tpw_te_hook_id(reinterpret_cast<const void*>(read)), tpw_te_hook_id(reinterpret_cast<const void*>(seek)),
        tpw_te_hook_id(reinterpret_cast<const void*>(tell)), tpw_te_hook_id(reinterpret_cast<const void*>(name))});
}
void SetEngineCallbacks(TbIsFirstPersonFn isFirstPerson, TbUseSmallTexturesFn useSmallTextures,
                        TbReadClockFn readClock, TbUpdateMouseFn updateMouse, TbMallocFn malloc, TbFreeFn free) {
    ev("SetEngineCallbacks", {tpw_te_hook_id(reinterpret_cast<const void*>(isFirstPerson)),
                              tpw_te_hook_id(reinterpret_cast<const void*>(useSmallTextures)),
                              tpw_te_hook_id(reinterpret_cast<const void*>(readClock)),
                              tpw_te_hook_id(reinterpret_cast<const void*>(updateMouse)),
                              tpw_te_hook_id(reinterpret_cast<const void*>(malloc)),
                              tpw_te_hook_id(reinterpret_cast<const void*>(free))});
}
bool UseSmallTextures() {
    ev("UseSmallTextures");
    return false;
}
PTCL* PAR_BuildDrawingList(int32_t list) { return nullptr; }
int32_t PAR_GetEmitterParticleCount(uint32_t emitter) { return 0; }
void PAR_GetEmitterBoundingBox(uint32_t emitter, PARPOINT3D* min, PARPOINT3D* max) {}
void PAR_FreeDrawingList(PTCL* list) {}
void PAR_LoadLibrary(char* path) {}
void PAR_InitParticleSystem(void* owner, int32_t density, int32_t maxParticles) {}
void PAR_CloseParticleSystem() {}

bfl::FileSystem::CFile* TPFile_Open(const char* name, const char* mode) {
    ev("TPFile_Open", {}, {name, mode});
    return reinterpret_cast<bfl::FileSystem::CFile*>(static_cast<uintptr_t>(ret("TPFile_Open")));
}
uint32_t TPFile_Read(void* dest, uint32_t size, uint32_t count, bfl::FileSystem::CFile* file) {
    ev("TPFile_Read", {lab(dest), size, count, lab(file)});
    return static_cast<uint32_t>(ret("TPFile_Read"));
}
uint32_t TPFile_Close(bfl::FileSystem::CFile* file) {
    ev("TPFile_Close", {lab(file)});
    return static_cast<uint32_t>(ret("TPFile_Close"));
}
uint32_t TPFile_Seek(bfl::FileSystem::CFile* file, uint32_t offset, uint32_t whence) {
    ev("TPFile_Seek", {lab(file), offset, whence});
    return static_cast<uint32_t>(ret("TPFile_Seek"));
}
int32_t TPFile_Size(bfl::FileSystem::CFile* file) {
    ev("TPFile_Size", {lab(file)});
    return static_cast<int32_t>(ret("TPFile_Size"));
}
char* FindFilePath(const char* name, uint32_t flags, int32_t* result) {
    static char sPath[200];
    ev("FindFilePath", {flags, lab(result)}, {name});
    std::snprintf(sPath, sizeof(sPath), "Found:%s", name);
    return sPath;
}
void AnimateRideSet(tag_sRideAnimSet* set, int32_t a, uint32_t flags) { ev("AnimateRideSet", {lab(set), a, flags}); }
void TickRideHeads(tag_sMesh*& mesh) { ev("TickRideHeads", {lab(&mesh), lab(mesh)}); }
void HandleActiveHoardings(tag_sRideAnimSet* set, float step) { ev("HandleActiveHoardings", {lab(set), fbits(step)}); }
void PT_Enable() { ev("PT_Enable"); }
void PT_Disable() { ev("PT_Disable"); }
void CorrectOrientationFlagsForDiffProceduralMode(int32_t procedural) {
    ev("CorrectOrientationFlagsForDiffProceduralMode", {procedural});
}

NS_MacDoze::InternalHandle* NS_MacDoze::GlobalAlloc(uint32_t flags, uint32_t size) {
    ev("GlobalAlloc", {flags, size});
    void* block = std::calloc(1, size != 0 ? size : 1);
    newlab(block);
    return static_cast<NS_MacDoze::InternalHandle*>(block);
}
NS_MacDoze::InternalHandle* NS_MacDoze::GlobalFree(NS_MacDoze::InternalHandle* block) {
    ev("GlobalFree", {lab(block)});
    return reinterpret_cast<NS_MacDoze::InternalHandle*>(static_cast<uintptr_t>(ret("GlobalFree")));
}

void SetRect(Rect* rect, int16_t left, int16_t top, int16_t right, int16_t bottom) {
    ev("SetRect", {left, top, right, bottom});
    rect->left = left;
    rect->top = top;
    rect->right = right;
    rect->bottom = bottom;
}
namespace {
GrafPort gFakeWindow;
}
extern "C" GrafPort* tpw_te_window() { return &gFakeWindow; }
GrafPort* NewCWindow(void* storage, const Rect* bounds, const unsigned char* title, bool visible, int16_t procID,
                     GrafPort* behind, bool goAwayFlag, int32_t refCon) {
    ev("NewCWindow", {lab(storage), bounds->top, bounds->left, bounds->bottom, bounds->right, visible ? 1 : 0, procID,
                      static_cast<int32_t>(reinterpret_cast<intptr_t>(behind)), goAwayFlag ? 1 : 0, refCon},
       {pascal(title).c_str()});
    return ret("NewCWindow", 1) != 0 ? &gFakeWindow : nullptr;
}
OSErr FSMakeFSSpec(int16_t vRefNum, int32_t dirID, const unsigned char* fileName, FSSpec* spec) {
    ev("FSMakeFSSpec", {vRefNum, dirID}, {pascal(fileName).c_str()});
    return static_cast<OSErr>(ret("FSMakeFSSpec"));
}
int16_t FSpOpenResFile(const FSSpec* spec, int8_t permission) {
    ev("FSpOpenResFile", {permission});
    return static_cast<int16_t>(ret("FSpOpenResFile", 7));
}
int16_t CurResFile() {
    ev("CurResFile");
    return static_cast<int16_t>(ret("CurResFile", 3));
}
void UseResFile(int16_t refNum) { ev("UseResFile", {refNum}); }
Handle GetNamedResource(uint32_t type, const unsigned char* name) {
    static char* sPointer;
    ev("GetNamedResource", {type}, {pascal(name).c_str()});
    sPointer = reinterpret_cast<char*>(tpw_te_resource);
    return ret("GetNamedResource", 1) != 0 ? &sPointer : nullptr;
}
void CloseResFile(int16_t refNum) { ev("CloseResFile", {refNum}); }

SamsUtilities::UString::UString(const char* text) : mLength(static_cast<uint32_t>(std::strlen(text))) {
    mText = dup(text);
    ev("UString", {}, {text});
}
SamsUtilities::UString::~UString() { ev("~UString"); }
void SamsUtilities::UOpenGL::AddAttribute(int32_t attribute, int32_t value) {
    ev("UOpenGL::AddAttribute", {lab(this), attribute, value});
}

namespace {
class FakeIdxDirectDraw : public IdxDirectDraw {
public:
    SamsUtilities::UOpenGL* DisplayGetContext() override {
        ev("IdxDirectDraw::DisplayGetContext", {lab(this)});
        return mContext;
    }
    SamsUtilities::UOpenGL* mContext = nullptr;
};
FakeIdxDirectDraw gFakeIdx;
SamsUtilities::UOpenGL gFakeContext;
}
extern "C" void* tpw_te_idx() { return &gFakeIdx; }
extern "C" void* tpw_te_context() { return &gFakeContext; }
IdxDirectDraw* IdxMakeOpenGLImplementation(SamsUtilities::DeviceKind device, GrafPort* window, void* devices) {
    ev("IdxMakeOpenGLImplementation", {device, lab(window), lab(devices)});
    gFakeIdx.mContext = &gFakeContext;
    return ret("IdxMakeOpenGLImplementation", 1) != 0 ? &gFakeIdx : nullptr;
}

// ------------------------------------------------------------------ test entry points

extern "C" {

void* tpw_te_addr(const char* name) {
    struct Entry {
        const char* name;
        void* address;
    };
    static const Entry kEntries[] = {
        {"gGameOptions", &gGameOptions}, {"gTPDialogOptions", &gTPDialogOptions},
        {"gTPDialogTextureMegabytes", &gTPDialogTextureMegabytes}, {"gMainWindow", &gMainWindow},
        {"gScreenWidth", &gScreenWidth}, {"gScreenHeight", &gScreenHeight}, {"gNoSavedSettings", &gNoSavedSettings},
        {"gFirstPersonFlags", &gFirstPersonFlags}, {"gFirstPersonRideSet", &gFirstPersonRideSet},
        {"gHeightfieldRideSet", &gHeightfieldRideSet}, {"gRideAnimSets", &gRideAnimSets},
        {"gRideCountA", &gRideCountA}, {"gRideCountB", &gRideCountB}, {"gMaxMeshFaces4", &gMaxMeshFaces4},
        {"gMaxMeshF3E", &gMaxMeshF3E}, {"gMaxMeshNodes", &gMaxMeshNodes}, {"gClock", &gClock}, {"gGame", &gGame},
        {"gTPFileInfo", &gTPFileInfo}, {"gExitError", &gExitError}, {"gPlayerSettings", &gPlayerSettings},
        {"gOptionsBalance", &gOptionsBalance}, {"gei_TextureSettings", &gei_TextureSettings},
        {"gei_Display", &gei_Display}, {"gei_System", &gei_System}, {"gei_Sky", &gei_Sky}, {"gei_TCache", &gei_TCache},
        {"gei_DDraw", &gei_DDraw}, {"gei_ViewMatrix", &gei_ViewMatrix}, {"gei_ProjectionMatrix", &gei_ProjectionMatrix},
        {"gei_MapWho", &gei_MapWho}, {"gei_Particle", &gei_Particle}, {"gs_WorldBoundingArea", &gs_WorldBoundingArea},
        {"gSmallTexturesOnly", &gSmallTexturesOnly}, {"gOpenGL", &gOpenGL}, {"gIdxDirectDraw", &gIdxDirectDraw},
        {"gCursorOverSprite", &gCursorOverSprite}, {"gCursorSpritePosition", &gCursorSpritePosition},
        {"g3DRefreshFlags", &g3DRefreshFlags}, {"gMemorySystemInitialised", &gMemorySystemInitialised},
        {"gScratchGeomVertices", &gScratchGeomVertices}, {"gScratchGeomBuffer", &gScratchGeomBuffer},
    };
    for (const Entry& entry : kEntries)
        if (std::strcmp(entry.name, name) == 0) return entry.address;
    return nullptr;
}

void* tpw_te_new(int32_t size) { return std::calloc(1, static_cast<std::size_t>(size)); }

void tpw_te_set_data_path(const char* text) {
    std::strncpy(tpw_te_data_path, text, 159);
    tpw_te_data_path[159] = 0;
}

void tpw_te_set_strings(float hoardingStep) {
    gTPGlobals.mF4014 = hoardingStep;
    gTPGlobals.mLowOptionsFile = TbDynamicStringTemplate<char>("low.sam");
    gTPGlobals.mMediumOptionsFile = TbDynamicStringTemplate<char>("med.sam");
    gTPGlobals.mHighOptionsFile = TbDynamicStringTemplate<char>("high.sam");
}

// The fields the unit reads and writes, in test_TPEngine.py FIELDS order.
#define TPW_TE_FIELDS(X)                                                                              \
    X(gPlayerSettings.mCurrent.mRenderer) X(gPlayerSettings.mCurrent.mDetail) X(gPlayerSettings.mCurrent.mVideoCard)                  \
    X(gPlayerSettings.mCustomOptions) X(gei_TextureSettings.mFlags) X(gei_TextureSettings.mTextureMemoryLimit) \
    X(gei_TextureSettings.mProcTextureParam) X(gei_Display.mFlags) X(gei_System.mFlags) X(gei_System.mRenderer) \
    X(gExitError.mError) X(gFakeContext.mF04) X(gFakeContext.mF06) X(gFakeContext.mF4C) X(gFakeContext.mVideoMemory)
void tpw_te_set_fields(const int32_t* v) {
    int32_t i = 0;
#define TPW_TE_SET(field) field = static_cast<decltype(field)>(v[i++]);
    TPW_TE_FIELDS(TPW_TE_SET)
#undef TPW_TE_SET
}
void tpw_te_get_fields(int32_t* v) {
    int32_t i = 0;
#define TPW_TE_GET(field) v[i++] = static_cast<int32_t>(field);
    TPW_TE_FIELDS(TPW_TE_GET)
#undef TPW_TE_GET
}
float* tpw_te_view_distance() { return &gei_Display.mFirstPersonViewDistance; }
int32_t* tpw_te_tcache_fields(CTextureCache* cache, int32_t which) { return which == 0 ? &cache->mF18DC : &cache->mF1908; }
int32_t* tpw_te_ddraw_devices(CDirectDraw* ddraw) { return &ddraw->mDeviceCount; }
void tpw_te_particle_hooks(const CParticle* hooks, int32_t* out) {
    out[0] = tpw_te_hook_id(reinterpret_cast<const void*>(hooks->mGetEmitterBoundingBox));
    out[1] = tpw_te_hook_id(reinterpret_cast<const void*>(hooks->mInitParticleSystem));
    out[2] = tpw_te_hook_id(reinterpret_cast<const void*>(hooks->mCloseParticleSystem));
    out[3] = tpw_te_hook_id(reinterpret_cast<const void*>(hooks->mLoadLibrary));
    out[4] = tpw_te_hook_id(reinterpret_cast<const void*>(hooks->mBuildDrawingList));
    out[5] = tpw_te_hook_id(reinterpret_cast<const void*>(hooks->mFreeDrawingList));
    out[6] = tpw_te_hook_id(reinterpret_cast<const void*>(hooks->mGetEmitterParticleCount));
}
// The test's stand-in for the particle object is a block of memory: only its hooks are used.
void tpw_te_clear_particle_hooks(CParticle* hooks) { std::memset(static_cast<void*>(hooks), 0, sizeof(*hooks)); }

// Ride sets and meshes in host layout, from the values a case gives (Mac field order).
void tpw_te_set_ride(tag_sRideAnimSet* set, uint32_t flags, tag_sMesh* heads, void* f14) {
    set->mFlags = flags;
    set->mHeads = heads;
    set->mF14 = f14;
}
uint32_t tpw_te_ride_flags(const tag_sRideAnimSet* set) { return set->mFlags; }
void tpw_te_set_mesh(tag_sMesh* mesh, tag_sM3D2Header* header, void* f28) {
    mesh->mHeader = header;
    mesh->mF28 = f28;
}
// The mesh header and its nodes keep the file's layout with 32-bit pointer fields
// (include/Ptr32.h), so they come from the arena.
void* tpw_te_arena(uint32_t size) { return tpw::ArenaAllocZeroed(size); }
void tpw_te_set_header(tag_sM3D2Header* header, const uint16_t* counts, tag_sNode* nodes) {
    header->mUVSetCount = counts[0];
    header->mFaceCount = counts[1];
    header->mAllNodeCount = counts[2];
    header->mNodeCount = counts[3];
    header->mNodes = nodes;
}
void tpw_te_set_node(tag_sNode* node, uint16_t f58, uint16_t f5c, uint16_t f5e) {
    node->mVertexCount = f58;
    node->mF5C = f5c;
    node->mF5E = f5e;
}

int32_t tpw_te_call(const char* name, const int64_t* a) {
    const std::string n(name);
    if (n == "MacLoadDebugSettings") MacLoadDebugSettings();
    else if (n == "MacIsFirstPerson") return MacIsFirstPerson();
    else if (n == "MacReadClock") return static_cast<int32_t>(MacReadClock());
    else if (n == "MacUpdateMouse") MacUpdateMouse();
    else if (n == "MacOpen") return static_cast<int32_t>(reinterpret_cast<uintptr_t>(
        MacOpen(reinterpret_cast<const char*>(a[0]), reinterpret_cast<const char*>(a[1]))));
    else if (n == "MacRead") return static_cast<int32_t>(MacRead(reinterpret_cast<void*>(a[0]), static_cast<uint32_t>(a[1]),
                                                                 static_cast<uint32_t>(a[2]), reinterpret_cast<void*>(a[3])));
    else if (n == "MacClose") return static_cast<int32_t>(MacClose(reinterpret_cast<void*>(a[0])));
    else if (n == "MacSeek") return static_cast<int32_t>(MacSeek(reinterpret_cast<void*>(a[0]), static_cast<uint32_t>(a[1]),
                                                                 static_cast<uint32_t>(a[2])));
    else if (n == "MacSize") return MacSize(reinterpret_cast<void*>(a[0]));
    else if (n == "MacMalloc") return lab(MacMalloc(static_cast<uint32_t>(a[0])));
    else if (n == "MacFree") {
        void** slot = reinterpret_cast<void**>(a[0]);
        MacFree(*slot);
        return *slot == nullptr ? 0 : lab(*slot);
    }
    else if (n == "EngineStartup_SetOptions") EngineStartup_SetOptions();
    else if (n == "EngineStartup_OptionsFile") return EngineStartup_OptionsFile(static_cast<uint32_t>(a[0]));
    else if (n == "EngineStartup_Dialog") EngineStartup_Dialog();
    else if (n == "EngineStartup_SettingsFile") return EngineStartup_SettingsFile();
    else if (n == "EngineStartup_Final") return EngineStartup_Final(static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]),
                                                                    static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]));
    else if (n == "FindFilePath") return lab(FindFilePath(reinterpret_cast<const char*>(a[0]), static_cast<int32_t>(a[1])));
    else if (n == "EngineStartup") return EngineStartup();
    else if (n == "AnimateRides") AnimateRides(static_cast<uint32_t>(a[0]));
    else if (n == "EngineTick1") EngineTick1();
    else if (n == "EngineTick2") EngineTick2();
    else if (n == "EngineTick3") EngineTick3();
    else if (n == "EngineCloseDown") EngineCloseDown();
    else if (n == "ResetWorldBoundingArea") ResetWorldBoundingArea();
    else if (n == "TPForce3DRefresh") TPForce3DRefresh(static_cast<uint32_t>(a[0]));
    else if (n == "ResetCursorOverSprites") ResetCursorOverSprites(reinterpret_cast<s3DPoint*>(a[0]));
    else if (n == "HNewHeap") return HNewHeap(static_cast<uint32_t>(a[0]), static_cast<uint32_t>(a[1]),
                                               static_cast<uint32_t>(a[2]), reinterpret_cast<sHeapInfo**>(a[3]));
    else if (n == "HFreeHeap") return HFreeHeap(reinterpret_cast<sHeapInfo*>(a[0]));
    else if (n == "InitialiseInstancing") InitialiseInstancing(static_cast<uint32_t>(a[0]));
    else if (n == "EnableProceduralTexturing") EnableProceduralTexturing();
    else if (n == "DisableProceduralTexturing") DisableProceduralTexturing();
    else if (n == "IsProceduralTexturingActive") return IsProceduralTexturingActive();
    else if (n == "MReleaseAllocF") return lab(MReleaseAllocF(static_cast<uint32_t>(a[0]), static_cast<uint32_t>(a[1])));
    else if (n == "MReleaseFreeF") return static_cast<int32_t>(reinterpret_cast<uintptr_t>(
        MReleaseFreeF(reinterpret_cast<void*>(a[0]))));
    else if (n == "InitialiseMemorySystem") InitialiseMemorySystem();
    else if (n == "UpdateScratchGeomBufferSize") UpdateScratchGeomBufferSize(reinterpret_cast<tag_sMesh*>(a[0]));
    else if (n == "CreateScratchGeomBuffer") CreateScratchGeomBuffer(static_cast<int32_t>(a[0]));
    else if (n == "DestroyScratchGeomBuffer") DestroyScratchGeomBuffer();
    else return -12345;
    return 0;
}

}  // extern "C"

extern "C" {

void tpw_te_set_options(const int32_t* v, int32_t n) {
    for (int32_t k = 0; k < n; ++k) gOptionsBalance.mValues[k].mInteger = v[k];
}
void tpw_te_get_options(int32_t* v, int32_t n) {
    for (int32_t k = 0; k < n; ++k) v[k] = gOptionsBalance.mValues[k].mInteger;
}

}  // extern "C"

#include "fake_strings.h"
