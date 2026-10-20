// tb_loadmanager.h: CsTexture, the texture object of a mesh's texture record, and the
// delayed-load list (engine library, 0x1001658c..0x1001722c; "tb_loadmanager.cpp" is the
// name in CsTexture's assertions).
//
// A CsTexture holds the texture entries of one texture record: one per frame, taken from
// the texture load manager by folder and name, or a single entry it makes itself (a blank
// image, for a record with flag 0x80). It is shared by count: Load on a loaded object
// only adds a reference, Close gives one back and frees at the last.
#pragma once
#include <cstddef>
#include <cstdint>

class CTextureEntry;
struct sTexture;
struct sTextureInfo;

class CsTexture {
public:
    // CMesh::Init stores the object's address in the texture record of the mesh file, a
    // 32-bit pointer field (include/Ptr32.h), so the object comes from the arena. The
    // operator answers nullptr when there is no memory, as the Mac's did (the original
    // tests the result of `new`).
    static void* operator new(size_t size) noexcept;
    static void operator delete(void* block);
    // Arrays are also stored in the mesh file's Ptr32 texture fields.
    static void* operator new[](size_t size) noexcept;
    static void operator delete[](void* block);
    static void* operator new(size_t, void* where) noexcept { return where; }
    static void operator delete(void*, void*) noexcept {}

    CsTexture() { Reset(); }                             // 0x100167b0
    ~CsTexture() { Close(); }                            // 0x1001680c
    void Reset();                                        // 0x10016890
    int32_t Load(CTextureEntry* entry);                  // 0x100168d4
    int32_t Load(char* name);                            // 0x100169b4
    int32_t Load(sTexture* texture, sTextureInfo* info); // 0x10016b30
    int32_t LoadTextureFromDisk();                       // 0x10016da0
    int32_t ReLoad(CsTexture* other);                    // 0x10016f60
    int32_t Close();                                     // 0x10016fc0
    void SetTextureEntries(int32_t count) { mEntryCount = static_cast<int16_t>(count); }   // 0x100170ec
    uint16_t GetFrameCount();                            // 0x1001712c
    sTexture* GetsTexture() { return mTexture; }         // 0x10017168
    void SetTextureEntry(CTextureEntry* entry, int32_t index) { mEntries[index] = entry; }  // 0x1001719c
    CTextureEntry* GetTextureEntry(int32_t index) { return mEntries[index]; }               // 0x100171ec
    CTextureEntry** GetTextureEntryAddr(int32_t index) { return mEntries + index; }         // 0x1001722c

    sTexture* mTexture;        // 0x00
    sTextureInfo* mInfo;       // 0x04
    CTextureEntry** mEntries;  // 0x08  from the engine allocator
    int16_t mEntryCount;       // 0x0c  frames; -0x8000: one entry the object owns
    int16_t mReferences;       // 0x0e
};

// Textures whose loading was put off (texture setting 0x40000000, set while a mesh is
// initialised): CsTexture::Load(texture, info) lists them, this loads them all.
void DelayedLoad_LoadTextures();                         // 0x1001670c
extern int32_t gDelayedLoadCount;                        // engine data 0x100af1d0
extern CsTexture* gDelayedLoads[256];                    // 0x100af1d4
