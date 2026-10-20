// tb_textureentry.h: a texture as the engine holds it (engine library; the original file
// is tb_textureentry.cpp by its assertions, 0x10040d30..0x10041848, with five inline
// getters emitted at 0x10025d60..0x10025e40; the inventory files the range under
// tb_printf_opengl.cpp).
//
// An entry is the image (or the compressed image, to be unpacked on demand), its size,
// and where the texture cache has put it. Mac offsets in the comments; 0x5c bytes there.
// In the original the last 0x18 bytes are a virtual base (the compiler's pointer to it is
// the first word); here they are a member with the pointer kept, since the cache reaches
// them through that pointer and nothing else derives from the class.
#pragma once
#include <cstdint>

class CCompBase;
class CTextureBase;
class CTextureLink;

// Where an entry sits on a cache page (0x18 bytes; only cleared so far, name invented).
struct STexturePlace {
    uint8_t mF00[0x10];
    int32_t mF10;                // 0x10
    int32_t mF14;                // 0x14
};

class CTextureEntry {
public:
    enum : uint32_t {
        kSizeMask = 0x00000007,       // BestSize of the larger side
        kOnPage = 0x00001000,         // the cache has placed it (Remove takes it off)
        kLocked = 0x00004000,
        kWrapAny = 0x00030000,
        kFlipped = 0x00040000,        // stored upside down
        kNextIsPlane = 0x00800000,    // the entry after this one is a further plane of the same image
        kOwnsData = 0x01000000,
        kCompressed = 0x10000000,     // mData is a CCompBase to unpack from, not pixels
    };

    CTextureEntry();                                 // 0x10040d30
    ~CTextureEntry();                                // 0x10040dfc: Close()
    // A blank square image of `size` pixels in the current texture format; 0 without memory.
    int32_t Create(int32_t size, int32_t flags);     // 0x10040e84
    int32_t Close();                                 // 0x10040f7c
    // The draw primitive's blending bits for this entry's kind.
    uint32_t GetBlendingMode();                      // 0x100410a4
    // The cache's size class of a side: 0 above 128 pixels down to 5 for 8 and under.
    int32_t BestSize(int32_t size);                  // 0x1004117c
    int32_t GetBestSize();                           // 0x10041204
    // The pixels: unpacked into the engine's scratch buffer first when compressed.
    void* GetData();                                 // 0x10041274
    // The same, turned the right way up in place when the image is stored flipped.
    void* GetCorrectedData();                        // 0x10041334
    void SetSize(int32_t width, int32_t height);     // 0x10041560
    void FlushEntryFlags();                          // 0x1004168c
    void Lock();                                     // 0x10041738
    void Unlock();                                   // 0x100417c0
    int32_t StoreAsWrapping();                       // 0x10041848
    float GetVSize() { return mVSize; }              // 0x10025d60
    float GetUSize() { return mUSize; }              // 0x10025d98
    float GetVOffset() { return mVOffset; }          // 0x10025dd0
    float GetUOffset() { return mUOffset; }          // 0x10025e08
    CTextureBase* GetTexture() { return (mFlags & kOnPage) != 0 ? mTexture : nullptr; }   // 0x10025e40

    STexturePlace* mPlace;       // 0x00  &mOwnPlace
    CTextureEntry* mNext;        // 0x04  the next entry on the same cache page
    uint32_t mFlags;             // 0x08
    int16_t mWidth;              // 0x0c
    int16_t mHeight;             // 0x0e
    int16_t mCacheWidth;         // 0x10  the size as the cache holds it
    int16_t mCacheHeight;        // 0x12
    CTextureLink* mPage;         // 0x14  the cache page it is on
    CTextureBase* mTexture;      // 0x18  that page's texture
    float mUSize;                // 0x1c  0.0 until placed
    float mVSize;                // 0x20
    float mUOffset;              // 0x24
    float mVOffset;              // 0x28
    float mPriority;             // 0x2c  0.0
    int16_t mPitch;              // 0x30  pixels per row of the image
    int32_t mSortKey;            // 0x34  built from priority, size and flags
    void* mF38;                  // 0x38  an engine allocation, freed by Close
    void* mData;                 // 0x3c  the pixels, or with kCompressed the CCompBase
    int32_t mF40;                // 0x40  not set by the constructor; copied into the mF34 of the
                                 //       primitives it is drawn with (CHField::Execute,
                                 //       SRenderListEntry::SetupPrimitive)
    STexturePlace mOwnPlace;     // 0x44
};
