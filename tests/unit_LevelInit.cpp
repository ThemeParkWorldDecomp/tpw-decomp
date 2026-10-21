// Test entry points for the park's first initialisation (library unit_LevelInit: Game_theme_resources.cpp,
// TPSettings_itemlist.cpp, rsse_init.cpp, hlsprite_flies_init.cpp, LevelInit.cpp, TPCamera_init.cpp, with the real default
// names of TPGlobals.cpp). Every callee outside those files is a fake that records what it is given: the resource classes
// and manager, the strings' directory separator, the advisor loader, the height field loader, the engine's matrices and
// gamut, the clock, rand, the light maker.
#include <cstdio>
#include <cstring>
#include <new>

#include "Advisor.h"
#include "Clock.h"
#include "Debug.h"
#include "FS_System.h"
#include "Game.h"
#include "HLSprite.h"
#include "LbFile.h"
#include "LevelInit.h"
#include "Main.h"
#include "TPAdvisor.h"
#include "TPCamera.h"
#include "TPDialog.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "bfl_FileSystem.h"
#include "calllog.h"
#include "platform/msl.h"
#include "rsse.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "ui_ErrorDisplay.h"

using namespace calllog;
using String = TbDynamicStringTemplate<char>;

namespace {
// Which fakes record: the resource constructors and storages only in the operation that compares them.
bool gRecordResources = false;
bool gLogStorage = false;
// The classes built on the texture resource record under their own names: they raise this before their base's
// constructor runs.
bool gDerived = false;
}  // namespace

TPGlobals gTPGlobals;   // src/game/TPGlobals.cpp's: the default names

// ---- The strings -------------------------------------------------------------------------------
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// The wide-string library routines the 16-bit strings reference and nothing here reaches.
uint32_t wcslen(const char16_t*) { return 0; }
char16_t* wcscpy(char16_t* dst, const char16_t*) { return dst; }
char16_t* wcsncpy(char16_t* dst, const char16_t*, uint32_t) { return dst; }
char16_t* wcscat(char16_t* dst, const char16_t*) { return dst; }
char16_t* wcsncat(char16_t* dst, const char16_t*, uint32_t) { return dst; }
int32_t wcsncmp(const char16_t*, const char16_t*, uint32_t) { return 0; }
const char16_t* wcsstr(const char16_t* text, const char16_t*) { return text; }

// FS_System.cpp's directory separator (tested there): a separator unless the text ends in one (an empty text gets one).
void FS_AddDirectorySeparator(String& path) {
    const int32_t length = path.Length();
    if (length == 0 || path.mData[length - 1] != ':') path += gTPGlobals.mSeparator;
}

// ---- The game object and the clock ---------------------------------------------------------------
CGame::CGame() {}
CGame::~CGame() {}
CGame gGame;
const String& CGame::GetCurrentThemeName() {
    ev("GetCurrentThemeName", {lab(this)});
    return mCurrentThemeName;
}
CFixedTime::CFixedTime() {}
CVariableTime::CVariableTime() {}
CClock gClock;
int32_t CClock::ReadFixed() {
    const int32_t value = static_cast<int32_t>(ret("ReadFixed", 1234));
    ev("ReadFixed", {lab(this), value});
    return value;
}

// ---- The resources and their manager ---------------------------------------------------------------
// Each constructor records the class, the new object's label and the name and folder it was given; the item list
// resource's own constructor is the unit's, and shows as the texture resource under it.
CTPBaseResource::~CTPBaseResource() {}
int32_t CTPBaseResource::OpenFile(bfl::FileSystem::CFile&, const String&, uint32_t) { return 0; }
int32_t CTPBaseResource::OpenFile(bfl::FileSystem::CFile&, const String&, bfl::FileSystem::CFileStorage&, uint32_t) {
    return 0;
}
void CTPBaseResource::AddLocalStorage(const String& path) { ev("AddLocal", {lab(this)}, {path.mData}); }
CTPIntermediate::~CTPIntermediate() {}
CTPTextureResource::CTPTextureResource(const String& name, const String& path) : CTPBaseResource(name, path) {
    if (gRecordResources && !gDerived) ev("Texture", {newlab(this)}, {name.mData, path.mData});
    gDerived = false;
}
CTPTextureResource::~CTPTextureResource() {}
CTPAdvisorResource::CTPAdvisorResource(const String& name, const String& path) : CTPIntermediate((gDerived = true, name), path) {
    if (gRecordResources) ev("Advisor", {newlab(this)}, {name.mData, path.mData});
}
CTPAdvisorResource::~CTPAdvisorResource() {}
CTPTerrainResource::CTPTerrainResource(const String& name, const String& path)
    : CTPTextureResource((gDerived = true, name), path) {
    if (gRecordResources) ev("Terrain", {newlab(this)}, {name.mData, path.mData});
}
CTPTerrainResource::~CTPTerrainResource() {}
CTPSharedTextureResource::CTPSharedTextureResource(const String& name, const String& path) : CTPBaseResource(name, path) {
    if (gRecordResources) ev("Shared", {newlab(this)}, {name.mData, path.mData});
}
CTPSharedTextureResource::~CTPSharedTextureResource() {}
// The real ~CTPItemListResource and OpenFile overrides are unit v5375's (src/game/TPResource.cpp,
// not linked here): the destructor keeps the base destructors' work only, and nothing here opens a file through the list.
CTPItemListResource::~CTPItemListResource() {}
int32_t CTPItemListResource::OpenFile(bfl::FileSystem::CFile&, const String&, uint32_t) { return 0; }
int32_t CTPItemListResource::OpenFile(bfl::FileSystem::CFile&, const String&, bfl::FileSystem::CFileStorage&, uint32_t) {
    return 0;
}

namespace {
// The kind of resource the manager is given: 0 texture, 1 item list, 2 advisor, 3 terrain, 4 shared texture.
int32_t Kind(CTPBaseResource* resource) {
    if (dynamic_cast<CTPItemListResource*>(resource) != nullptr) return 1;
    if (dynamic_cast<CTPAdvisorResource*>(resource) != nullptr) return 2;
    if (dynamic_cast<CTPTerrainResource*>(resource) != nullptr) return 3;
    if (dynamic_cast<CTPSharedTextureResource*>(resource) != nullptr) return 4;
    return 0;
}
}  // namespace
int32_t CTPResourceManager::RegisterResource(CTPBaseResource* resource) {
    ev("Register", {lab(this), lab(resource), Kind(resource)});
    return 1;
}
int32_t CTPResourceManager::RegisterResource(CTPSharedTextureResource* resource) {
    ev("RegisterShared", {lab(this), lab(resource), Kind(resource)});
    return 1;
}

// The file storage inside the texture resources: constructed (and recorded for the item list's constructor) only.
bfl::FileSystem::CFileStorage::CFileStorage() : mAccess(nullptr) {
    if (gLogStorage) ev("CFileStorage", {lab(this)});
}
bfl::FileSystem::CFileStorage::~CFileStorage() {}
void bfl::FileSystem::CFileStorage::Close() {}
TbError bfl::FileSystem::CFileStorage::FindFirst(const char*, CFileInfo&) { return TbError(-1); }
TbError bfl::FileSystem::CFileStorage::FindNext(CFileInfo&) { return TbError(-1); }
TbError bfl::FileSystem::CFileStorage::OpenFile(CFile&, const char*, uint32_t, void*) { return TbError(-1); }
TbError bfl::FileSystem::CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::FullPath(String&, const String&) { return TbError(0); }
const char* bfl::FileSystem::CFileStorage::GetRootPath() { return ""; }
TbError bfl::FileSystem::CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::CreateDir(const char*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::Delete(const char*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::Move(const char*, const char*) { return TbError(0); }

// ---- The script system, the flies ------------------------------------------------------------------
RSSEState gRSSEState = {};   // src/game/rides/rsse.cpp's
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
namespace {
// The block LbMem_Alloc answers: the table, then a guard of 16 bytes the table must not reach.
alignas(16) unsigned char gFlyBlock[0x4000 + 16];
}  // namespace
// FLY_Close (src/game/rides/FlyingRide.cpp, linked for the flies' globals) is not run by any case.
void LbMem_Free(void*) {}
void FLY_RemoveBase(int32_t) {}
void* LbMem_Alloc(uint32_t size) {
    ev("LbMem_Alloc", {size});
    std::memset(gFlyBlock, 0xa5, sizeof gFlyBlock);
    return gFlyBlock;
}

// ---- The level ----------------------------------------------------------------------------------
namespace {
char gThemePath[160] = "";
}  // namespace
char* get_theme_path() {
    ev("get_theme_path");
    return gThemePath;
}
CSYS_TPFileInfo gTPFileInfo;
const String& CSYS_TPFileInfo::GetDataPath() const {
    ev("GetDataPath", {lab(this)});
    return mDataPath;
}
char gThemeTextureFolder[32] = "";
uint32_t gTPDialogOptions;
void LoadAdvisor(char* path, char* folder, char* variant, uint32_t which) {
    ev("LoadAdvisor", {which}, {path, folder, variant});
}
void ADV_Init() { ev("ADV_Init"); }
int32_t LoadLevelHeightfield(int32_t a, int32_t b) {
    ev("LoadLevelHeightfield", {a, b});
    return 0;
}
void CreateScratchGeomBuffer(int32_t large) { ev("CreateScratchGeomBuffer", {large}); }
int32_t msl_rand() {
    const int32_t value = static_cast<int32_t>(ret("Rand", 4242));
    ev("rand", {value});
    return value;
}

// The light CreateGlobalLight answers: a record holding a directional light (a real CLightParallel, built by src/game/Light.cpp)
// whose flags the case sets.
CLight* gev_LightChain;            // engine data: Light.cpp's Prepare chains the lights here (not run)
CMapLinkBase::~CMapLinkBase() {}   // engine 0x1001980c
namespace {
CLightParallel gLightObject;
sGlobalLight gRecord = {2, &gLightObject, nullptr, nullptr, 0};
}  // namespace
sGlobalLight* CreateGlobalLight(int32_t type, uint32_t colour, s3DPoint* position, s3DPoint* direction) {
    uint32_t bits[3];
    std::memcpy(bits, direction, sizeof bits);
    ev("CreateGlobalLight", {type, colour, lab(position), lab(direction), bits[0], bits[1], bits[2]});
    return &gRecord;
}

// ---- The camera: the engine's matrices and gamut -------------------------------------------------------
namespace {
CMatrix gViewObject;
CGamut gGamutObject;
}  // namespace
CGamut::CGamut() {}
CMatrix* gei_ViewMatrix = &gViewObject;
CMatrix gei_ProjectionMatrix;
CGamut* gei_Gamut = &gGamutObject;
void CMatrix::Load(sMatrix& source) {
    uint32_t words[16];
    std::memcpy(words, &source, sizeof words);
    ev("CMatrix::Load", {lab(this), lab(&source), words[0], words[1], words[2], words[3], words[4], words[5]});
    ev("CMatrix::Load.", {words[6], words[7], words[8], words[9], words[10], words[11], words[12], words[13]});
    ev("CMatrix::Load..", {words[14], words[15]});
}
void CMatrix::Projection(float nearPlane, float farPlane, float fov) {
    ev("CMatrix::Projection", {lab(this), fbits(nearPlane), fbits(farPlane), fbits(fov)});
}
void CGamut::Init(float fov) { ev("CGamut::Init", {lab(this), fbits(fov)}); }
// The matrix routine of rsse_matrix.cpp (tested there): zero, then ones on the diagonal.
void SetMatrixIdentity(sMatrix* matrix) {
    ev("SetMatrixIdentity", {lab(matrix)});
    std::memset(matrix, 0, sizeof(sMatrix));
    matrix->m[0][0] = matrix->m[1][1] = matrix->m[2][2] = matrix->m[3][3] = 1.0f;
}
uint32_t gFirstPersonFlags;

// ---- Entry points ----------------------------------------------------------------------------------
namespace {
struct Entry {
    const char* name;
    void* address;
};
const Entry kState[] = {
    {"engineFlags", &gTPGlobals.mEngineFlags}, {"sinTableLast", &gTPGlobals.mSinTable[0xfff]},
    {"mF4010", &gTPGlobals.mF4010},
    {"rsse0", &gRSSEState.mInitialised}, {"rsse4", &gRSSEState.mF04}, {"rsse8", &gRSSEState.mF08},
    {"rssec", &gRSSEState.mScriptCount}, {"rsse10", &gRSSEState.mScripts},
    {"checksum", &gRSSEScriptChecksum}, {"f7be4", &gRSSEF7BE4},
    {"flyActive", &gFlyActive}, {"flyInit", &gFlyInitialised}, {"flyTable", &gFlySinTable}, {"flyBases", gFlyBases},
    {"cameraF68", &gCameraF68}, {"cameraStart", gCameraStartPosition}, {"cameraPosition", gCameraPosition},
    {"cameraFov", &gCameraFov}, {"cameraStamp", &gCameraClockStamp}, {"modeA", &gCameraModeA}, {"modeB", &gCameraModeB},
    {"cameraF5AAC", &gCameraF5AAC}, {"matrixA", &gCameraMatrixA}, {"matrixB", &gCameraMatrixB},
    {"cameraMatrix", &gCameraMatrix}, {"cameraF5BB4", gCameraF5BB4}, {"cameraF5BF4", &gCameraF5BF4},
    {"firstPerson", &gFirstPersonFlags},
    {"lightHandle", &gGlobalLight}, {"lightDirection", &gGlobalLightDirection}, {"lightColour", &gGlobalLightColour},
    {"levelRandom", &gLevelRandom}, {"dialogOptions", &gTPDialogOptions},
    {"textureFolder", gThemeTextureFolder}, {"lightFlags", nullptr},
};
}  // namespace

extern "C" {

// The address of a named global (the test reads and writes it directly).
void* tpw_li_addr(const char* name) {
    if (std::strcmp(name, "lightFlags") == 0) return &static_cast<CLight&>(gLightObject).mFlags;
    for (const Entry& entry : kState)
        if (std::strcmp(entry.name, name) == 0) return entry.address;
    return nullptr;
}

// Labels for the objects the log names, the text the fakes answer, and the fake light's handle.
void tpw_li_prepare(const char* theme, const char* themePath, const char* dataPath, const char* textureFolder) {
    gGame.mCurrentThemeName = theme;
    std::strncpy(gThemePath, themePath, sizeof gThemePath - 1);
    gThemePath[sizeof gThemePath - 1] = '\0';
    gTPFileInfo.mDataPath = dataPath;
    std::strncpy(gThemeTextureFolder, textureFolder, sizeof gThemeTextureFolder - 1);
    gThemeTextureFolder[sizeof gThemeTextureFolder - 1] = '\0';
    tpw_cl_label(1, &gGame);
    tpw_cl_label(4, &gTPFileInfo);
    tpw_cl_label(5, &gViewObject);
    tpw_cl_label(6, &gei_ProjectionMatrix);
    tpw_cl_label(7, &gGamutObject);
    tpw_cl_label(8, &gCameraMatrix);
    tpw_cl_label(9, &gCameraMatrixB);
    tpw_cl_label(10, &gCameraMatrixA);
    tpw_cl_label(11, &gRecord);
    tpw_cl_label(12, &gGlobalLightDirection);
    tpw_cl_label(14, &gClock);
    std::memset(gFlyBlock, 0xa5, sizeof gFlyBlock);
}

// op 0: RSSE_Init; 1: SetAnimationDetail(arg); 2: FLY_Init; 3: LevelInit1; 4: LoadLevelAdvisors; 5: SetupLevelLights;
// 6: InitCamera; 7: Load1stPersonMesh. Answers LevelInit1's result.
int32_t tpw_li_call(int32_t op, uint32_t arg) {
    int32_t result = 0;
    switch (op) {
    case 0: RSSE_Init(); break;
    case 1: SetAnimationDetail(arg); break;
    case 2: FLY_Init(); break;
    case 3: result = LevelInit1(); break;
    case 4: LoadLevelAdvisors(); break;
    case 5: SetupLevelLights(); break;
    case 6: InitCamera(); break;
    case 7: Load1stPersonMesh(); break;
    }
    return result;
}

// The block the fly table was made in (the table and 16 guard bytes after it).
const unsigned char* tpw_li_fly_block() { return gFlyBlock; }

// InitThemeResources on a game object that is not gGame (its manager is label 3, the object label 2), with the theme of
// gGame as set by tpw_li_prepare; the resource constructors record.
void tpw_li_itr(const char* path) {
    static char manager;
    alignas(CGame) static unsigned char block[sizeof(CGame)];
    std::memset(block, 0xa5, sizeof block);
    CGame* game = new (block) CGame;
    game->mResourceManager = reinterpret_cast<CTPResourceManager*>(&manager);
    tpw_cl_label(3, &manager);
    tpw_cl_label(2, game);
    gRecordResources = true;
    game->InitThemeResources(String(path));
    gRecordResources = false;
}

// The item list resource's constructor on a block of 0xa5, with the file storage recorded; fields back in `out`:
// the two strings (name, folder) in `text` (separated by a newline), the list present, the four words after it, and the
// two words the class adds.
void tpw_li_itemlist(const char* name, const char* path, char* text, int64_t* out) {
    alignas(CTPItemListResource) static unsigned char block[sizeof(CTPItemListResource)];
    std::memset(block, 0xa5, sizeof block);
    const String nameString(name);
    const String pathString(path);
    tpw_cl_label(13, &reinterpret_cast<CTPItemListResource*>(block)->mF40);
    gLogStorage = true;
    auto* resource = new (block) CTPItemListResource(nameString, pathString);
    gLogStorage = false;
    std::snprintf(text, 400, "%s\n%s", resource->mName.mData, resource->mPath.mData);
    out[0] = resource->mStorages != nullptr;
    out[1] = resource->mManager != nullptr;
    out[2] = resource->mCurrentSearchStorage != nullptr;
    out[3] = resource->mLastFoundFileStorage != nullptr;
    out[4] = resource->mSearchPattern != nullptr;
    out[5] = resource->mF48 != nullptr;
    out[6] = resource->mItems != nullptr;
    resource->~CTPItemListResource();
}

}  // extern "C"
