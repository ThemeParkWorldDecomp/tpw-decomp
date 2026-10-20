#include "u8a_fakes.h"
alignas(CTextureLoadManager) uint8_t u8manager[sizeof(CTextureLoadManager)];
alignas(CTextureEntry) uint8_t u8testentries[sizeof(CTextureEntry) * 4096];
extern "C" {
// Expose loader testentry to the differential harness.
void* tpw_loader_testentry(int32_t i) {
    return u8testentries + i * sizeof(CTextureEntry);
}
// Expose loader ctor to the differential harness.
void* tpw_loader_ctor() {
    std::memset(u8manager, 0xa5, sizeof(u8manager));
    return new (u8manager) CTextureLoadManager;
}
// Expose loader store to the differential harness.
void* tpw_loader_store() {
    return u8manager;
}
// Expose loader zero to the differential harness.
int32_t tpw_loader_zero() {
    auto* m = reinterpret_cast<CTextureLoadManager*>(u8manager);
    for (auto& s : m->mLoads)
        if (s.mFolder || s.mName || s.mTexture || s.mReferences)
            return 0;
    return 1;
}
// Expose loader set to the differential harness.
void tpw_loader_set(int32_t count, int32_t def, uint32_t flags) {
    auto* m = reinterpret_cast<CTextureLoadManager*>(u8manager);
    m->mCount = count;
    m->mDefault = def;
    m->mLoadFlags = flags;
}
// Expose loader slot to the differential harness.
void tpw_loader_slot(int32_t i, char* folder, char* name, CTextureEntry* entry, int32_t refs) {
    auto& s = reinterpret_cast<CTextureLoadManager*>(u8manager)->mLoads[i];
    s.mFolder = folder;
    s.mName = name;
    s.mTexture = entry;
    s.mReferences = refs;
}
// Expose loader get to the differential harness.
void tpw_loader_get(int32_t* out) {
    auto* m = reinterpret_cast<CTextureLoadManager*>(u8manager);
    out[0] = m->mCount;
    out[1] = m->mDefault;
    out[2] = m->mLoadFlags;
}
// Expose loader getslot to the differential harness.
void tpw_loader_getslot(int32_t i, int32_t* out) {
    auto& s = reinterpret_cast<CTextureLoadManager*>(u8manager)->mLoads[i];
    out[0] = lab(s.mFolder);
    out[1] = lab(s.mName);
    out[2] = lab(s.mTexture);
    out[3] = s.mReferences;
}
// Expose entry new to the differential harness.
void* tpw_entry_new(int32_t count) {
    return new CTextureEntry[count];
}
// Expose entry set to the differential harness.
void tpw_entry_set(CTextureEntry* entry, int32_t i, uint32_t flags, void* data) {
    entry[i].mFlags = flags;
    entry[i].mData = data;
}
// Expose entry get to the differential harness.
void tpw_entry_get(CTextureEntry* entry, int32_t i, uint32_t* flags, int32_t* pitch, void** data) {
    *flags = entry[i].mFlags;
    *pitch = entry[i].mPitch;
    *data = entry[i].mData;
}
// Expose entry at to the differential harness.
void* tpw_entry_at(CTextureEntry* entry, int32_t i) {
    forget(&entry[i]);
    return &entry[i];
}
// Expose entry delete to the differential harness.
void tpw_entry_delete(CTextureEntry* entry) {
    delete[] entry;
}
// Expose TextureLoadManager Init to the differential harness.
void* tpw_TextureLoadManager_Init(int32_t existed) {
    gei_TLoadManager = existed ? reinterpret_cast<CTextureLoadManager*>(u8manager) : nullptr;
    int32_t result = TextureLoadManager_Init();
    if (!existed && gei_TLoadManager)
        *reinterpret_cast<CTextureLoadManager*>(u8manager) = *gei_TLoadManager;
    return reinterpret_cast<void*>(uintptr_t(result));
}
// Expose TextureLoadManager Close to the differential harness.
int32_t tpw_TextureLoadManager_Close(int32_t existed) {
    gei_TLoadManager = existed ? new CTextureLoadManager : nullptr;
    if (existed)
        gei_TLoadManager->mCount = 0;
    return TextureLoadManager_Close();
}
// Expose loader global to the differential harness.
void tpw_loader_global(int32_t* out) {
    out[0] = gei_TLoadManager != nullptr;
    out[1] = gTextureExtraPlanes;
    out[2] = gTextureAverageColour;
    out[3] = gTextureLoadFlags;
    out[4] = gTextureWaveData != nullptr;
}
// Expose loader globals to the differential harness.
void tpw_loader_globals(int32_t extra, uint32_t avg, uint32_t flags) {
    gTextureExtraPlanes = extra;
    gTextureAverageColour = avg;
    gTextureLoadFlags = flags;
    gTextureWaveData = nullptr;
}
// Expose TGA Load to the differential harness.
void* tpw_TGA_Load(STGAHeader* h, char* name, int32_t extra) {
    return h->Load(name, extra);
}
// Expose TGA Bump to the differential harness.
void* tpw_TGA_Bump(STGAHeader* h, char* name) {
    return h->LoadBumpmap(name);
}
// Expose TGA Emboss to the differential harness.
void tpw_TGA_Emboss(STGAHeader* h, uint8_t* data) {
    h->ConvertToEmbossBumpmap(data);
}
// Expose TGALoad to the differential harness.
void* tpw_TGALoad(char* name, int32_t* w, int32_t* h, int32_t flags, int32_t bytes) {
    return TGALoadAndConvert(name, w, h, flags, nullptr, bytes);
}
// Expose Convert to the differential harness.
uint32_t tpw_Convert(void* d, void* s, int32_t w, int32_t h, int32_t db, int32_t sb, int32_t flip) {
    return UncompressedConvert(d, s, w, h, nullptr, db, sb, flip);
}
// Expose Manager Init to the differential harness.
int32_t tpw_Manager_Init() {
    return reinterpret_cast<CTextureLoadManager*>(u8manager)->Init();
}
// Expose Manager Close to the differential harness.
int32_t tpw_Manager_Close() {
    return reinterpret_cast<CTextureLoadManager*>(u8manager)->Close();
}
// Expose Manager LoadFile to the differential harness.
void* tpw_Manager_LoadFile(char* name) {
    return reinterpret_cast<CTextureLoadManager*>(u8manager)->LoadFile(name);
}
// Expose Manager LoadTexture to the differential harness.
void* tpw_Manager_LoadTexture(char* folder, char* name) {
    return reinterpret_cast<CTextureLoadManager*>(u8manager)->LoadTexture(folder, name);
}
// Expose SLoad ctor to the differential harness.
void tpw_SLoad_ctor(STextureLoad* s) {
    new (s) STextureLoad;
}
// Expose SLoad Close to the differential harness.
int32_t tpw_SLoad_Close(STextureLoad* s) {
    return s->Close();
}
}
