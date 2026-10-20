// The engine start-up glue of the main app (0x10059c70..0x10068d90 in part; the inventory
// files the range under TPRide.cpp, and the original file name is unknown): the options
// table and the options dialog turned into engine flags, the window, the OpenGL context and
// the engine's start, the engine's Mac* hooks, the per-frame engine calls, the memory hooks,
// the procedural texturing switches and the scratch geometry buffer.
//
// The rest of the range (heightfield, ride height maps, hoardings, instancing, meshes,
// quickload) is not translated yet.
#include "TPEngine.h"

#include <cfloat>
#include <cstring>

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
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "TbDynamicStringTemplate.h"
#include "Wavelet.h"
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

uint8_t gSmallTexturesOnly;
SamsUtilities::UOpenGL* gOpenGL;
IdxDirectDraw* gIdxDirectDraw;
int32_t gCursorOverSprite;
s3DPoint gCursorSpritePosition;
uint32_t g3DRefreshFlags;
int32_t gMemorySystemInitialised;
uint32_t gScratchGeomVertices;
void* gScratchGeomBuffer;

namespace {

// Pascal strings of the original.
const unsigned char kDebugOptionsName[] = "\x0d" "debug options";
const unsigned char kWindowTitle[] = "\x10" "Theme Park World";

// A byte of the "debug options" resource: 0xff leaves the option alone, 1 sets it, anything
// else clears it.
void ApplyDebugOption(uint8_t value, uint32_t bit) {
    if (value == 0xff) return;
    if (value == 1)
        gGameOptions |= bit;
    else
        gGameOptions &= ~bit;
}

}  // namespace

// 0x10059c70: a "debug options" resource file next to the application may override four
// option bits (its 'DOPT' resource of the same name, one byte per bit).
void MacLoadDebugSettings() {
    FSSpec spec;
    if (FSMakeFSSpec(0, 0, kDebugOptionsName, &spec) != 0) return;
    const int16_t file = FSpOpenResFile(&spec, 1);
    if (file == -1) return;
    const int16_t previous = CurResFile();
    UseResFile(file);
    Handle options = GetNamedResource('DOPT', kDebugOptionsName);
    if (options != nullptr) {
        ApplyDebugOption(static_cast<uint8_t>((*options)[0]), 0x4);
        ApplyDebugOption(static_cast<uint8_t>((*options)[1]), kGameOptionFullScreen);
        ApplyDebugOption(static_cast<uint8_t>((*options)[2]), kGameOptionSound400);
        ApplyDebugOption(static_cast<uint8_t>((*options)[3]), kGameOptionFrontEnd);
    }
    CloseResFile(file);
    UseResFile(previous);
}

// 0x10059e40
int32_t MacIsFirstPerson() { return static_cast<int32_t>(gFirstPersonFlags & kFirstPersonAny); }

// 0x10059e78
uint32_t MacReadClock() { return static_cast<uint32_t>(gClock.Read()); }

// 0x10059ec4
void MacUpdateMouse() {}

// 0x10059ef0..0x1005a010: the engine's file hooks, the game's TPFile functions.
void* MacOpen(const char* name, const char* mode) { return TPFile_Open(name, mode); }

uint32_t MacRead(void* dest, uint32_t size, uint32_t count, void* file) {
    return TPFile_Read(dest, size, count, static_cast<bfl::FileSystem::CFile*>(file));
}

uint32_t MacClose(void* file) { return TPFile_Close(static_cast<bfl::FileSystem::CFile*>(file)); }

uint32_t MacSeek(void* file, uint32_t offset, uint32_t whence) {
    return TPFile_Seek(static_cast<bfl::FileSystem::CFile*>(file), offset, whence);
}

int32_t MacSize(void* file) { return TPFile_Size(static_cast<bfl::FileSystem::CFile*>(file)); }

// 0x1005a054
void* MacMalloc(uint32_t size) { return MReleaseAllocF(size, 1); }

// 0x1005a0a0
void MacFree(void*& block) {
    MReleaseFreeF(block);
    block = nullptr;
}

// 0x1005a0fc: the options table (gOptionsBalance, value slot = row) to the engine's flags.
// Values outside each option's range leave its flags alone.
void EngineStartup_SetOptions() {
    BalanceValue* option = gOptionsBalance.mValues;
    CTextureSettings& textures = gei_TextureSettings;
    CDisplay& display = gei_Display;

    switch (option[2].mInteger) {   // TEXTUREFILTERING
    case 0:
    case 1: textures.mFlags &= ~0x18u; break;
    case 2:
    case 3: textures.mFlags |= 0x18; break;
    }
    switch (option[3].mInteger) {   // PROCEDURALTEXTURING
    case 0:
        gGameOptions &= ~0x4000u;
        gGameOptions &= ~0x2000u;
        break;
    case 1:
        gGameOptions |= 0x4000;
        gGameOptions |= 0x2000;
        gTPDialogOptions |= 4;
        break;
    case 2:
        gGameOptions |= 0x4000;
        gGameOptions |= 0x2000;
        gTPDialogOptions |= 2;
        break;
    case 3:
        gGameOptions |= 0x4000;
        gGameOptions |= 0x2000;
        gTPDialogOptions |= 1;
        break;
    }
    switch (option[4].mInteger) {   // PROCEDURALTEXTURECACHE
    case 0: textures.mProcTextureParam = 1500; break;
    case 1: textures.mProcTextureParam = 2000; break;
    case 2: textures.mProcTextureParam = 2500; break;
    }
    switch (option[5].mInteger) {   // ANIMATINGTEXTURES
    case 0: textures.mFlags &= ~0x40u; break;
    case 1: textures.mFlags |= 0x40; break;
    }
    switch (option[6].mInteger) {   // SKYQUALITY (3 sets no layers)
    case 0: gei_Sky.SetActiveLayers(0); break;
    case 1: gei_Sky.SetActiveLayers(1); break;
    case 2: gei_Sky.SetActiveLayers(2); break;
    case 4: gei_Sky.SetActiveLayers(4); break;
    }
    switch (option[7].mInteger) {   // SKYDETAIL
    case 0: display.mFlags &= ~0x400000u; break;
    case 1: display.mFlags |= 0x400000; break;
    }
    switch (option[8].mInteger) {   // SKYSHADOW
    case 0: display.mFlags &= ~0x10000u; break;
    case 1: display.mFlags |= 0x10000; break;
    }
    switch (option[9].mInteger) {   // MESHSHADOW
    case 0: gTPDialogOptions &= ~0x200u; break;
    case 1: gTPDialogOptions |= 0x200; break;
    }
    if (option[10].mInteger != 0) option[10].mInteger = 3;   // SPRITESHADOW: only off or 3
    switch (option[10].mInteger) {
    case 0:
        display.mFlags |= 0x20000;
        display.mFlags &= ~0x140000u;
        break;
    case 1:
        display.mFlags |= 0x40000;
        display.mFlags &= ~0x120000u;
        break;
    case 2:
    case 3: display.mFlags &= ~0x160000u; break;
    }
    switch (option[11].mInteger) {   // TRIPLEBUFFER
    case 0: display.mFlags &= ~1u; break;
    case 1: display.mFlags |= 1; break;
    }
    switch (option[12].mInteger) {   // FOG
    case 0:
        display.mFlags &= ~0x2000u;
        display.mFlags &= ~0x4000u;
        break;
    case 1:
        display.mFlags &= ~0x2000u;
        display.mFlags |= 0x4000;
        break;
    }
    switch (option[13].mInteger) {   // MIPMAP
    case 0: textures.mFlags &= ~2u; break;
    case 1: textures.mFlags |= 2; break;
    }
    switch (option[14].mInteger) {   // RENDER32
    case 0: gGameOptions &= ~0x200000u; break;
    case 1: gGameOptions |= 0x200000; break;
    }
    switch (option[15].mInteger) {   // TEXTURE32
    case 0: gGameOptions &= ~0x400000u; break;
    case 1: gGameOptions |= 0x400000; break;
    }
    switch (option[16].mInteger) {   // BUMPMAPPING
    case 0: textures.mFlags &= ~0x20u; break;
    case 1: textures.mFlags |= 0x20; break;
    }
    switch (option[17].mInteger) {   // FIRSTPERSONVIEWDISTANCE: the view distance and the fog
    case 0:
        display.mFirstPersonViewDistance = 200.0f;
        display.SetFogSpace(50.0f, 60.0f);
        break;
    case 1:
        display.mFirstPersonViewDistance = 325.0f;
        display.SetFogSpace(100.0f, 110.0f);
        break;
    case 2:
        display.mFirstPersonViewDistance = 450.0f;
        display.SetFogSpace(135.0f, 150.0f);
        break;
    case 3:
        display.mFirstPersonViewDistance = 700.0f;
        display.SetFogSpace(230.0f, 250.0f);
        break;
    }
    switch (option[18].mInteger) {   // LIGHTING
    case 0:
        display.mFlags |= 0x800;
        display.mFlags |= 0x1000;
        break;
    case 1:
        display.mFlags &= ~0x800u;
        display.mFlags &= ~0x1000u;
        break;
    }
}

// 0x1005a6a8
int32_t EngineStartup_OptionsFile(uint32_t flags) {
    TbDynamicStringTemplate<char> path;
    path = gTPFileInfo.GetDataPath();
    FS_AddDirectorySeparator(path);
    if ((flags & 0x800) != 0)
        path += gTPGlobals.mLowOptionsFile;
    else if ((flags & 0x1000) != 0)
        path += gTPGlobals.mMediumOptionsFile;
    else if ((flags & 0x2000) != 0)
        path += gTPGlobals.mHighOptionsFile;
    if (gOptionsBalance.Load(path.mData, CBalanceTable::LoadMode(0)) == 0) return 0;
    EngineStartup_SetOptions();
    return 1;
}

// 0x1005a7e0: the options dialog's own flags (gTPDialogOptions and gGameOptions, each
// read once) to the engine's.
void EngineStartup_Dialog() {
    const uint32_t dialog = gTPDialogOptions;
    if ((dialog & 0x100) != 0)
        gei_Display.mFlags |= 0x10000;
    else
        gei_Display.mFlags &= ~0x10000u;
    const uint32_t options = gGameOptions;
    if ((options & 0x8000) != 0)
        gei_Display.mFlags |= 1;
    else
        gei_Display.mFlags &= ~1u;
    if ((options & 0x80000) != 0)
        gei_TextureSettings.mFlags |= 2;
    else
        gei_TextureSettings.mFlags &= ~2u;
    if (gTPDialogTextureMegabytes != 0)
        gei_TextureSettings.mTextureMemoryLimit = static_cast<int32_t>(gTPDialogTextureMegabytes << 20);
    if ((options & 2) != 0)
        gei_System.mFlags &= ~0x40u;
    else
        gei_System.mFlags |= 0x40;
    if ((dialog & 0x40) != 0) {   // software rendering: everything costly off
        gPlayerSettings.SetRendererType(CPlayerSettings::eRENDERER(0));
        gei_System.mRenderer = 4;
        gTPDialogOptions &= ~0x100u;
        gTPDialogOptions &= ~0x200u;
        gTPDialogOptions &= ~0x400u;
        gGameOptions &= ~0x1000u;
        gGameOptions &= ~0x8000u;
        gGameOptions &= ~0x80000u;
        gGameOptions &= ~0x4000u;
        gGameOptions &= ~0x2000u;
        gGameOptions &= ~0x100000u;
        gGameOptions &= ~0x20000u;
    } else {
        gPlayerSettings.SetRendererType(CPlayerSettings::eRENDERER(1));
        gei_System.mRenderer = CSystem::kRendererOpenGL;
    }
}

// 0x1005a9b4: the preset options file for the player's detail level.
int32_t EngineStartup_SettingsFile() {
    gTPDialogOptions &= ~0x400u;
    gei_TextureSettings.mTextureMemoryLimit = 0;
    gGameOptions |= 0x40604;
    switch (gPlayerSettings.mCurrent.mDetail) {
    case 0:
        if (EngineStartup_OptionsFile(0x800) == 0) return 0;
        break;
    case 1:
        if (EngineStartup_OptionsFile(0x1000) == 0) return 0;
        break;
    case 2:
        if (gPlayerSettings.mCurrent.mRenderer == 0) {
            gPlayerSettings.SetGraphicsDetail(CPlayerSettings::eDETAIL(1));
            if (EngineStartup_OptionsFile(0x1000) == 0) return 0;
        } else {
            if (EngineStartup_OptionsFile(0x2000) == 0) return 0;
        }
        break;
    }
    if (gPlayerSettings.mCurrent.mVideoCard == 1)
        gGameOptions |= 0x1000;
    else
        gGameOptions &= ~0x1000u;
    gei_System.mRenderer = CSystem::kRendererOpenGL;
    return 1;
}

// 0x1005ab18
int32_t EngineStartup_Final(int32_t depth, int32_t a, int32_t fullScreen, int32_t textureFormat) {
    SetFileCallbacks(MacOpen, MacClose, MacRead, MacSeek, MacSize, FindFilePath);
    SetEngineCallbacks(MacIsFirstPerson, UseSmallTextures, MacReadClock, MacUpdateMouse, MacMalloc, MacFree);
    {
        SamsUtilities::UString unused("DDraw_Init");   // built and dropped (a disabled log line)
    }
    if (DDraw_Init(gMainWindow, gScreenWidth, gScreenHeight, depth, a, fullScreen) == 0) return 0;
    if (gNoSavedSettings != 0 && gOpenGL->mVideoMemory <= 0x800000) {   // a first run on a small card
        gPlayerSettings.SetGraphicsDetail(CPlayerSettings::eDETAIL(0));
        gSmallTexturesOnly = 1;
    }
    {
        SamsUtilities::UString unused("RenderSystem_Init");
    }
    if (RenderSystem_Init(1) == 0) {
        DDraw_Close();
        return 0;
    }
    {
        SamsUtilities::UString unused("EngineAdd_Init");
    }
    if (EngineAdd_Init(textureFormat, 0x92f2266, nullptr) == 0) {
        RenderSystem_Close();
        DDraw_Close();
        return 0;
    }
    gei_Particle->mInitParticleSystem = PAR_InitParticleSystem;
    gei_Particle->mCloseParticleSystem = PAR_CloseParticleSystem;
    gei_Particle->mLoadLibrary = PAR_LoadLibrary;
    gei_Particle->mBuildDrawingList = PAR_BuildDrawingList;
    gei_Particle->mFreeDrawingList = PAR_FreeDrawingList;
    gei_Particle->mGetEmitterParticleCount = PAR_GetEmitterParticleCount;
    gei_Particle->mGetEmitterBoundingBox = PAR_GetEmitterBoundingBox;
    return 1;
}

// 0x1005ad58: the engine's name hook.
char* FindFilePath(const char* name, int32_t flags) { return FindFilePath(name, flags != 0 ? 1 : 0, nullptr); }

// 0x1005adbc
int32_t EngineStartup() {
    if (gPlayerSettings.mCustomOptions == 1) {
        if ((gTPDialogOptions & 0x4000) == 0 && EngineStartup_OptionsFile(gTPDialogOptions) == 0) return 0;
        EngineStartup_Dialog();
    } else {
        EngineStartup_SettingsFile();   // its result is not checked
    }
    MacLoadDebugSettings();
    if ((gGameOptions & kGameOptionFrontEnd) == 0) gGame.SetCurrentTheme(0);

    Rect bounds;
    if ((gGameOptions & kGameOptionFullScreen) != 0) {
        SetRect(&bounds, 0, 0, static_cast<int16_t>(gScreenWidth), static_cast<int16_t>(gScreenHeight));
        gMainWindow = NewCWindow(nullptr, &bounds, kWindowTitle, false, 2, reinterpret_cast<GrafPort*>(-1), false, 0);
    } else {
        SetRect(&bounds, 0x40, 0x40, static_cast<int16_t>(gScreenWidth + 0x40),
                static_cast<int16_t>(gScreenHeight + 0x40));
        gMainWindow = NewCWindow(nullptr, &bounds, kWindowTitle, false, 0, reinterpret_cast<GrafPort*>(-1), false, 0);
    }
    if (gMainWindow == nullptr) {
        gExitError.SetError(CExitError::ErrorType(4), 0, nullptr);
        return 0;
    }

    gIdxDirectDraw = IdxMakeOpenGLImplementation(SamsUtilities::DeviceKind(1000), gMainWindow, nullptr);
    if (gIdxDirectDraw == nullptr) return 0;   // no error is set
    gOpenGL = gIdxDirectDraw->DisplayGetContext();
    gOpenGL->AddAttribute(8, 4);    // 4 bits each of red, blue, green, alpha
    gOpenGL->AddAttribute(10, 4);
    gOpenGL->AddAttribute(9, 4);
    gOpenGL->AddAttribute(11, 4);
    gOpenGL->mF4C = 16;
    gOpenGL->mF06 = 1;
    gOpenGL->mF04 = 0;

    const uint32_t options = gGameOptions;
    const int32_t textureFormat = (options & 0x400000) != 0 ? 5 : 4;   // TEXTURE32
    const int32_t fullScreen = (options & kGameOptionFullScreen) != 0 ? 1 : 0;
    const int32_t a = (options & 0x1000) != 0 ? 1 : 0;
    const int32_t depth = (options & 0x200000) != 0 ? 32 : 16;          // RENDER32
    if (EngineStartup_Final(depth, a, fullScreen, textureFormat) == 0) {
        gExitError.SetError(CExitError::ErrorType(12), 0, nullptr);
        return 0;
    }

    gei_ViewMatrix->SetIdentity();   // inline in the original: memset and the diagonal
    gei_ProjectionMatrix.Projection(0.1f, 1000.0f, 90.0f);
    int32_t quality = -1;
    switch (gOptionsBalance.mValues[1].mInteger) {   // TEXTUREQUALITY
    case 0: quality = 3; break;
    case 1: quality = 2; break;
    case 2: quality = 1; break;
    case 3: quality = 0; break;
    }
    if (quality >= 0) {
        gei_TCache->mF1908 = quality;
        gei_TCache->mF18DC = quality;
    }

    switch (gei_System.mRenderer) {
    case 1:
    case 3:   // hardware
        gPlayerSettings.SetRendererType(CPlayerSettings::eRENDERER(1));
        gTPDialogOptions |= 0x80;
        gTPDialogOptions &= ~0x40u;
        break;
    default:   // software
        gPlayerSettings.SetRendererType(CPlayerSettings::eRENDERER(0));
        gTPDialogOptions &= ~0x80u;
        gTPDialogOptions |= 0x40;
        gGameOptions &= ~0x4000u;
        gGameOptions &= ~0x2000u;
        break;
    }
    if (gPlayerSettings.mCurrent.mVideoCard == 1 && gei_DDraw->mDeviceCount <= 1) gPlayerSettings.SetVideoCard(CPlayerSettings::eVIDEOCARD(0));
    return 1;
}

// 0x1005b36c
void AnimateRides(uint32_t mode) {
    if (mode == 2) {   // only the ride being ridden in first person
        if ((gFirstPersonFlags & 4) == 0) return;
        AnimateRideSet(gFirstPersonRideSet, 1, 0);
        if (gFirstPersonRideSet->mHeads->mF28 != nullptr) TickRideHeads(gFirstPersonRideSet->mHeads);
        if ((gFirstPersonRideSet->mFlags & 0x20) != 0) HandleActiveHoardings(gFirstPersonRideSet, gTPGlobals.mF4014);
        return;
    }
    for (uint32_t i = 0; i < gRideCountB + gRideCountA; ++i) {
        tag_sRideAnimSet* set = gRideAnimSets[i + 1];
        if (set == nullptr) continue;
        const uint32_t flags = set->mFlags;
        if ((flags & 0x200000) != 0) {   // skip this frame
            set->mFlags = flags & ~0x200000u;
            continue;
        }
        if (set->mF14 != nullptr && (flags & 0x3000) == 0) AnimateRideSet(set, 0, 8);
        if ((set->mFlags & 0x20) != 0) HandleActiveHoardings(set, gTPGlobals.mF4014);
        set->mFlags |= 0x800000;
    }
}

// 0x1005b4d4
void EngineTick1() {
    g3DRefreshFlags = 0;
    if (gHeightfieldRideSet != nullptr) AnimateRideSet(gHeightfieldRideSet, 1, 4);
}

// 0x1005b540
void EngineTick2() { gei_MapWho->Render(); }

// 0x1005b590
void EngineTick3() { RenderSystem_FlipScreen(); }

// 0x1005b5d8
void EngineCloseDown() {
    EngineAdd_Close();
    RenderSystem_Close();
    DDraw_Close();
}

// 0x1005b634: an empty box (minimum at FLT_MAX, maximum at -FLT_MAX).
void ResetWorldBoundingArea() {
    gs_WorldBoundingArea.mMin[0] = FLT_MAX;
    gs_WorldBoundingArea.mMin[1] = FLT_MAX;
    gs_WorldBoundingArea.mMin[2] = FLT_MAX;
    gs_WorldBoundingArea.mMax[0] = -FLT_MAX;
    gs_WorldBoundingArea.mMax[1] = -FLT_MAX;
    gs_WorldBoundingArea.mMax[2] = -FLT_MAX;
}

// 0x1005b68c
void TPForce3DRefresh(uint32_t flags) { g3DRefreshFlags |= flags; }

// 0x1005b6c8
void ResetCursorOverSprites(s3DPoint* position) {
    if (position != nullptr) gCursorSpritePosition = *position;
    gCursorOverSprite = 0;
}

// 0x1005b734: no heap is made; any non-NULL value does.
int32_t HNewHeap(uint32_t a, uint32_t b, uint32_t c, sHeapInfo** heap) {
    *heap = reinterpret_cast<sHeapInfo*>(1);
    return 1;
}

// 0x1005b774
int32_t HFreeHeap(sHeapInfo* heap) { return 1; }

// 0x10061aec
void InitialiseInstancing(uint32_t flags) {}

// 0x100635cc: turns procedural textures on when they are available and off, if the
// player's setting allows (1); a setting of 0 makes them unavailable.
void EnableProceduralTexturing() {
    const uint32_t options = gGameOptions;
    if ((options & 0x2000) == 0 || (options & 0x4000) != 0) return;
    if (gPlayerSettings.mCurrent.mRenderer == 1) {
        gGameOptions = options | 0x4000;
        PT_Enable();
        CorrectOrientationFlagsForDiffProceduralMode(1);
    } else if (gPlayerSettings.mCurrent.mRenderer == 0) {
        gGameOptions = options & ~0x4000u;
        gGameOptions &= ~0x2000u;
    }
}

// 0x1006367c
void DisableProceduralTexturing() {
    const uint32_t options = gGameOptions;
    if ((options & 0x2000) == 0 || (options & 0x4000) == 0) return;
    gGameOptions = options & ~0x4000u;
    PT_Disable();
    CorrectOrientationFlagsForDiffProceduralMode(0);
}

// 0x10063700
int32_t IsProceduralTexturingActive() {
    const uint32_t options = gGameOptions;
    return (options & 0x2000) != 0 && (options & 0x4000) != 0 ? 1 : 0;
}

// 0x10064bd4
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    if (size == 0) return nullptr;
    if ((flags & 1) != 0) return NS_MacDoze::GlobalAlloc(0x40, size);   // GMEM_ZEROINIT
    return NS_MacDoze::GlobalAlloc(0, size);
}

// 0x10064c54
void* MReleaseFreeF(void* block) {
    if (block == nullptr) return nullptr;
    return NS_MacDoze::GlobalFree(static_cast<NS_MacDoze::InternalHandle*>(block));
}

// 0x10064cb0
void InitialiseMemorySystem() { gMemorySystemInitialised = 1; }

// 0x100682fc: keeps the largest counts any loaded mesh needs.
void UpdateScratchGeomBufferSize(tag_sMesh* mesh) {
    const tag_sNode* node = mesh->mHeader->mNodes;
    for (uint16_t n = mesh->mHeader->mNodeCount; n != 0; --n, ++node) {
        if (node->mF5C > gScratchGeomVertices) gScratchGeomVertices = node->mF5C;
        if (node->mVertexCount > gScratchGeomVertices) gScratchGeomVertices = node->mVertexCount;
        if (node->mF5E > gScratchGeomVertices) gScratchGeomVertices = node->mF5E;
    }
    if (mesh->mHeader->mAllNodeCount > gScratchGeomVertices) gScratchGeomVertices = mesh->mHeader->mAllNodeCount;
    if (static_cast<uint32_t>(mesh->mHeader->mUVSetCount) << 2 > gMaxMeshFaces4)
        gMaxMeshFaces4 = static_cast<uint32_t>(mesh->mHeader->mUVSetCount) << 2;
    if (mesh->mHeader->mFaceCount > gMaxMeshF3E) gMaxMeshF3E = mesh->mHeader->mFaceCount;
    if (mesh->mHeader->mNodeCount > gMaxMeshNodes) gMaxMeshNodes = mesh->mHeader->mNodeCount;
}

// 0x10068d14
void CreateScratchGeomBuffer(int32_t large) {
    const uint32_t vertices = large != 0 ? 0x400 : gScratchGeomVertices;
    gScratchGeomBuffer = MReleaseAllocF(vertices * 12, 1);
}

// 0x10068d90: the pointer is left as it was.
void DestroyScratchGeomBuffer() { MReleaseFreeF(gScratchGeomBuffer); }
