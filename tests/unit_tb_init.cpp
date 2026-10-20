#include "u8a_fakes.h"
#include "tb_init.h"
#include "tb_ddraw.h"
#include "tb_init_dependencies.h"
#include "tb_proctexture.h"
#include "tb_printf_opengl.h"
#include "tb_sky.h"
#include "tb_texturecache.h"
#include "tb_error.h"

const int32_t gTextureBytesPerPixel[6] = {2, 2, 2, 4, 2, 4};
CDrawPrimitive* gOpenGLDrawPrimitive = &u8drawGL;
CHField::CHField() {}
CHField::~CHField() {}
CHField u8hfield;
CHField* gei_HField = &u8hfield;
CSky::CSky() {}
CSky::~CSky() {}
CRain::CRain() {}
CSky gei_Sky;
CRain u8rain;
CRain* gei_Rain = &u8rain;
CTextureCache::CTextureCache() {}   // the unit keeps a blank cache of its own
CTextureCache u8cache;
CTextureCache* gei_TCache = &u8cache;
CErrorHandler gei_ErrorHandler;
int32_t gDDResult;
// Record Gamut_Init and return its queued result.
int32_t Gamut_Init() {
    ev("Gamut_Init");
    return ret("Gamut_Init", 1);
}
// Record Gamut_Close and return its queued result.
int32_t Gamut_Close() {
    ev("Gamut_Close");
    return ret("Gamut_Close", 1);
}
// Record Matrix_Init and return its queued result.
int32_t Matrix_Init() {
    ev("Matrix_Init");
    return ret("Matrix_Init", 1);
}
// Record Matrix_Close (it returns nothing).
void Matrix_Close() {
    ev("Matrix_Close");
}
// Record RenderMemory_Init and return its queued result.
int32_t RenderMemory_Init() {
    ev("RenderMemory_Init");
    return ret("RenderMemory_Init", 1);
}
// Record RenderMemory_Close and return its queued result.
int32_t RenderMemory_Close() {
    ev("RenderMemory_Close");
    return ret("RenderMemory_Close", 1);
}
// Record LightManager_Init and return its queued result.
int32_t LightManager_Init() {
    ev("LightManager_Init");
    return ret("LightManager_Init", 1);
}
// Record LightManager_Close and return its queued result.
int32_t LightManager_Close() {
    ev("LightManager_Close");
    return ret("LightManager_Close", 1);
}
// Record RenderList_Init and return its queued result.
int32_t RenderList_Init() {
    ev("RenderList_Init");
    return ret("RenderList_Init", 1);
}
// Record RenderList_Close and return its queued result.
int32_t RenderList_Close() {
    ev("RenderList_Close");
    return ret("RenderList_Close", 1);
}
// Record HField_Init and return its queued result.
int32_t HField_Init() {
    ev("HField_Init");
    return ret("HField_Init", 1);
}
// Record HField_Close and return its queued result.
int32_t HField_Close() {
    ev("HField_Close");
    return ret("HField_Close", 1);
}
// Record MapWho_Init and return its queued result.
int32_t MapWho_Init() {
    ev("MapWho_Init");
    return ret("MapWho_Init", 1);
}
// Record MapWho_Close and return its queued result.
int32_t MapWho_Close() {
    ev("MapWho_Close");
    return ret("MapWho_Close", 1);
}
// Record Switch_Init and return its queued result.
int32_t Switch_Init() {
    ev("Switch_Init");
    return ret("Switch_Init", 1);
}
// Record Switch_Close and return its queued result.
int32_t Switch_Close() {
    ev("Switch_Close");
    return ret("Switch_Close", 1);
}
// Record SpriteManager_Init and return its queued result.
int32_t SpriteManager_Init() {
    ev("SpriteManager_Init");
    return ret("SpriteManager_Init", 1);
}
// Record SpriteManager_Close and return its queued result.
int32_t SpriteManager_Close() {
    ev("SpriteManager_Close");
    return ret("SpriteManager_Close", 1);
}
// Record Particle_Init and return its queued result.
int32_t Particle_Init() {
    ev("Particle_Init");
    return ret("Particle_Init", 1);
}
// Record Particle_Close and return its queued result.
int32_t Particle_Close() {
    ev("Particle_Close");
    return ret("Particle_Close", 1);
}
// Record TCache_Close and return its queued result.
int32_t TCache_Close() {
    ev("TCache_Close");
    return ret("TCache_Close", 1);
}
// Record Rain_Init and return its queued result.
int32_t Rain_Init() {
    ev("Rain_Init");
    return ret("Rain_Init", 1);
}
// Record Rain_Close and return its queued result.
int32_t Rain_Close() {
    ev("Rain_Close");
    return ret("Rain_Close", 1);
}
// Record InitWaveLoad and return its queued result.
int32_t InitWaveLoad() {
    ev("InitWaveLoad");
    return ret("InitWaveLoad", 1);
}
// Record CloseWaveLoad and return its queued result.
int32_t CloseWaveLoad() {
    ev("CloseWaveLoad");
    return ret("CloseWaveLoad", 1);
}
// Record DrawPrimitiveOpenGL_Init and return its queued result.
int32_t DrawPrimitiveOpenGL_Init() {
    ev("DrawPrimitiveOpenGL_Init");
    return ret("DrawPrimitiveOpenGL_Init", 1);
}
// Record DrawPrimitiveOpenGL_Close and return its queued result.
int32_t DrawPrimitiveOpenGL_Close() {
    ev("DrawPrimitiveOpenGL_Close");
    return ret("DrawPrimitiveOpenGL_Close", 1);
}
// Record Clipping_Init and return its queued result.
int32_t Clipping_Init() {
    ev("Clipping_Init");
    return ret("Clipping_Init", 1);
}
// Record Clipping_Close and return its queued result.
int32_t Clipping_Close() {
    ev("Clipping_Close");
    return ret("Clipping_Close", 1);
}
// Record TextureLoadManager_Init and return its queued result.
int32_t TextureLoadManager_Init() {
    ev("TextureLoadManager_Init");
    return ret("TextureLoadManager_Init", 1);
}
// Fake TextureLoadManager_Close for matching external-call events.
bool TextureLoadManager_Close() {
    ev("TextureLoadManager_Close");
    return ret("TextureLoadManager_Close", 1) != 0;
}
// Fake TCache_Init for matching external-call events.
int32_t TCache_Init(int32_t format) {
    ev("TCache_Init", {format});
    return ret("TCache_Init", 1);
}
// Fake CreateSineTable for matching external-call events.
void CreateSineTable() {
    ev("CreateSineTable");
}
// Fake CHField::LoadProcTextures for matching external-call events.
int32_t CHField::LoadProcTextures(char* name, int32_t size) {
    ev("HField.LoadProc", {lab(this), size}, {name});
    return ret("HField.LoadProc", 1);
}
// Fake CSky::Init for matching external-call events.
int32_t CSky::Init() {
    ev("Sky.Init", {lab(this)});
    return ret("Sky.Init", 1);
}
// Fake CRain::SetWindParameters for matching external-call events.
void CRain::SetWindParameters(float direction, float speed) {
    ev("Rain.Wind", {lab(this), fbits(direction), fbits(speed)});
}
// Fake CTextureSettings::CalculateUVFixup for matching external-call events.
void CTextureSettings::CalculateUVFixup() {
    ev("UVFixup", {lab(this)});
}
// Fake CTextureSettings::OutputDebugInfo for matching external-call events.
void CTextureSettings::OutputDebugInfo() {
    ev("Texture.Debug", {lab(this)});
}
// Fake CSystem::OutputDebugInfo for matching external-call events.
void CSystem::OutputDebugInfo() {
    ev("System.Debug", {lab(this)});
}
// Fake CDisplay::OutputDebugInfo for matching external-call events.
void CDisplay::OutputDebugInfo() {
    ev("Display.Debug", {lab(this)});
}

// Fake Tileset::Tileset for matching external-call events.
Tileset::Tileset() {
}
// Fake Tileset::~Tileset for matching external-call events.
Tileset::~Tileset() {
}
// Fake Landscape::Landscape for matching external-call events.
Landscape::Landscape() {
    if (on())
        ev("Landscape.ctor", {newlab(this)});
}
// Fake Landscape::~Landscape for matching external-call events.
Landscape::~Landscape() {
    ev("Landscape.dtor", {lab(this)});
    forget(this);
}
// Fake Landscape::set_tilesize for matching external-call events.
void Landscape::set_tilesize(int32_t size) {
    ev("Landscape.Size", {lab(this), size});
}
// Fake Landscape::load_landscape for matching external-call events.
int32_t Landscape::load_landscape(char* name) {
    ev("Landscape.Load", {lab(this)}, {name});
    return ret("Landscape.Load", 1);
}
// Fake Landscape::generate_tile for matching external-call events.
void Landscape::generate_tile(uint8_t* out, uint32_t x, uint32_t y, sTileOverlay* overlay) {
    // Fake ev for matching external-call events.
    ev("Landscape.Tile", {lab(this), lab(out), x, y, lab(overlay->mData), overlay->mWidth,
                          overlay->mHeight, overlay->mFormat, overlay->mFlags});
    int32_t size = ret("tile_size", 3);
    for (int32_t i = 0; i < size * size * 4; ++i)
        out[i] = uint8_t(i * 37 + x * 5 + y * 7);
}
// Fake ConvertSquareFrom32 for matching external-call events.
void ConvertSquareFrom32(void* dest, void* source, int32_t size) {
    ev("ConvertSquare", {lab(dest), lab(source), size});
    auto* d = static_cast<uint8_t*>(dest);
    auto* s = static_cast<uint8_t*>(source);
    for (int32_t i = 0; i < size * size * 2; ++i)
        d[i] = s[i] ^ 0x5a;
}
// Fake CTextureCache::Remove for matching external-call events.
int32_t CTextureCache::Remove(CTextureEntry* entry) {
    ev("Cache.Remove", {lab(this), lab(entry)});
    return 1;
}
// Fake CTextureCache::Prepare for matching external-call events.
void CTextureCache::Prepare(CTextureEntry* entry, float distance) {
    ev("Cache.Prepare", {lab(this), lab(entry), fbits(distance)});
}
// Fake CErrorHandler::Push for matching external-call events.
void CErrorHandler::Push(int32_t level, int32_t code) {
    ev("Error", {level, code});
}

alignas(CProcTextureHandler) uint8_t u8proc[sizeof(CProcTextureHandler)];
uint8_t u8scratch[64 * 64 * 4];
SamsUtilities::ULogManager u8logger;
extern "C" {
// Expose init setup to the differential harness.
void tpw_init_setup(int32_t renderer, uint32_t display, uint32_t system, uint32_t memory,
                    int32_t texmemory, int32_t logger) {
    gei_System.mRenderer = renderer;
    gei_System.mFlags = system;
    gei_System.mTotalPhys = memory;
    gei_Display.mFlags = display;
    gei_TextureSettings.mTextureMemory = texmemory;
    tpw_cl_label(6, &u8hfield);
    tpw_cl_label(7, &gei_Sky);
    tpw_cl_label(8, &u8rain);
    tpw_cl_label(9, &u8cache);
    SamsUtilities::gLogger = logger ? &u8logger : nullptr;
    tpw_cl_label(10, &u8logger);
    gProcTextureBuffer = u8scratch;
    tpw_cl_label(11, u8scratch);
}
// Expose init state to the differential harness.
void tpw_init_state(uint32_t* out) {
    out[0] = gei_Display.mFlags;
    out[1] = gei_TextureSettings.mFlags;
    out[2] = lab(gei_DrawPrimitive);
    out[3] = SamsUtilities::gLogger != nullptr;
}
// Expose EngineInit to the differential harness.
int32_t tpw_EngineInit(int32_t format, int32_t version, char* name) {
    return EngineAdd_Init(format, version, name);
}
// Expose EngineClose to the differential harness.
int32_t tpw_EngineClose() {
    return EngineAdd_Close();
}
// Expose EngineReinit to the differential harness.
int32_t tpw_EngineReinit() {
    return EngineAdd_Reinit();
}
// Expose proc ctor to the differential harness.
void* tpw_proc_ctor() {
    std::memset(u8proc, 0xa5, sizeof(u8proc));
    return new (u8proc) CProcTextureHandler;
}
// Expose proc store to the differential harness.
void* tpw_proc_store() {
    return u8proc;
}
// Expose proc get to the differential harness.
void tpw_proc_get(int32_t* out) {
    auto* p = reinterpret_cast<CProcTextureHandler*>(u8proc);
    out[0] = lab(p->mLandscape);
    out[1] = p->mSize;
    out[2] = p->mCyclic != nullptr;
    out[3] = p->mCount;
    out[4] = p->mCount ? p->mCurrent - p->mCyclic : -1;
}
// Expose proc ctorcheck to the differential harness.
void tpw_proc_ctorcheck(int32_t* out) {
    auto* p = reinterpret_cast<CProcTextureHandler*>(u8proc);
    out[0] = 1;
    for (auto* n : p->mTiles)
        if (n)
            out[0] = 0;
    out[1] = reinterpret_cast<uintptr_t>(p->mCurrent) == uintptr_t(0xa5a5a5a5a5a5a5a5);
}
// Expose proc loadsetup to the differential harness.
void tpw_proc_loadsetup(int32_t landscape, int32_t size) {
    auto* p = reinterpret_cast<CProcTextureHandler*>(u8proc);
    int32_t recording = tpw_cl_on;
    tpw_cl_on = 0;
    static Landscape land;
    tpw_cl_on = recording;
    tpw_cl_label(12, &land);
    p->mLandscape = landscape ? &land : nullptr;
    p->mSize = size;
}
// Expose proc init to the differential harness.
int32_t tpw_proc_init(int32_t count, int32_t size) {
    return reinterpret_cast<CProcTextureHandler*>(u8proc)->Init(count, size);
}
// Expose proc close to the differential harness.
int32_t tpw_proc_close() {
    return reinterpret_cast<CProcTextureHandler*>(u8proc)->Close();
}
// Expose proc dtor to the differential harness.
void tpw_proc_dtor(int32_t flag) {
    auto* p = reinterpret_cast<CProcTextureHandler*>(u8proc);
    if (flag > 0) {
        auto* copy = new CProcTextureHandler;
        *copy = *p;
        delete copy;
    } else
        p->~CProcTextureHandler();
}
// Expose proc preinitialized to the differential harness.
void tpw_proc_preinitialized(int32_t kind) {
    auto* p = reinterpret_cast<CProcTextureHandler*>(u8proc);
    if (kind == 1)
        p->mCyclic = reinterpret_cast<SProcTexture*>(uintptr_t(1));
    if (kind == 2)
        p->mLandscape = reinterpret_cast<Landscape*>(uintptr_t(1));
}
// Expose proc load to the differential harness.
int32_t tpw_proc_load(char* name) {
    return reinterpret_cast<CProcTextureHandler*>(u8proc)->Load(name);
}
// Expose proc links to the differential harness.
void tpw_proc_links(int32_t i, int32_t* out) {
    auto* p = reinterpret_cast<CProcTextureHandler*>(u8proc);
    auto& n = p->mCyclic[i];
    out[0] = n.mPrevious - p->mCyclic;
    out[1] = n.mNext - p->mCyclic;
    out[2] = n.mSlot ? n.mSlot - p->mTiles : -1;
    out[3] = lab(n.mTexture);
}
// Expose proc testsetup to the differential harness.
void tpw_proc_testsetup(int32_t count, int32_t size) {
    auto* p = reinterpret_cast<CProcTextureHandler*>(u8proc);
    p->mSize = size;
    p->mLandscape = new Landscape;
    p->mCyclic = new SProcTexture[count]{};
    p->mCount = count;
    p->mCurrent = p->mCyclic + count / 2;
    tpw_cl_label(12, p->mLandscape);
    for (int32_t i = 0; i < count; ++i) {
        auto& n = p->mCyclic[i];
        n.mPrevious = &p->mCyclic[(i + count - 1) % count];
        n.mNext = &p->mCyclic[(i + 1) % count];
        n.mSlot = nullptr;
        n.mTexture = new CTextureEntry[1];
        n.mTexture->mData = std::calloc(size * size * 4 + 64, 1);
        tpw_cl_label(20 + i, n.mTexture);
        tpw_cl_label(40 + i, n.mTexture->mData);
    }
}
// Expose proc cached to the differential harness.
void tpw_proc_cached(uint32_t tile, int32_t index) {
    auto* p = reinterpret_cast<CProcTextureHandler*>(u8proc);
    auto& slot = p->mTiles[(tile >> 8) * 128 + (tile & 255)];
    slot = &p->mCyclic[index];
    p->mCyclic[index].mSlot = &slot;
}
// Expose proc labels to the differential harness.
void tpw_proc_labels(int32_t count) {
    auto* p = reinterpret_cast<CProcTextureHandler*>(u8proc);
    tpw_cl_label(12, p->mLandscape);
    for (int32_t i = 0; i < count; ++i) {
        tpw_cl_label(20 + i, p->mCyclic[i].mTexture);
        tpw_cl_label(40 + i, p->mCyclic[i].mTexture->mData);
    }
}
// Expose proc prepare to the differential harness.
int32_t tpw_proc_prepare(PrepareEntry* entries, sTileOverlay* overlays, int32_t count) {
    return reinterpret_cast<CProcTextureHandler*>(u8proc)->PrepareList(entries, overlays, count);
}
// Expose proc entry to the differential harness.
void* tpw_proc_entry(int32_t i) {
    return reinterpret_cast<CProcTextureHandler*>(u8proc)->mCyclic[i].mTexture;
}
// Expose proc data to the differential harness.
void* tpw_proc_data(int32_t i) {
    return reinterpret_cast<CProcTextureHandler*>(u8proc)->mCyclic[i].mTexture->mData;
}
// Expose proc create to the differential harness.
int32_t tpw_proc_create(int32_t i, int32_t x, int32_t y, sTileOverlay* overlay) {
    return reinterpret_cast<CProcTextureHandler*>(u8proc)->CreateProcTexture(
        reinterpret_cast<CProcTextureHandler*>(u8proc)->mCyclic[i].mTexture, x, y, overlay);
}
}
