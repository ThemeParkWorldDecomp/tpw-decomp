// tb_sky.cpp (engine library). Mac offsets below; 0x2f14 bytes on the Mac.
// Pointer members grow on a native host. The per-frame members (Prepare, RenderAdd, Render)
// are in tb_sky_render.cpp.
#pragma once
#include <cstdint>
#include <cstddef>
#include "platform/d3d_types.h"

class CTextureEntry;
struct SSkyAdd {
    CTextureEntry* mTexture;           // 0x00
    int32_t mF04, mF08;               // 0x04, 0x08
    D3DVECTOR mPosition;              // 0x0c
    float mSize;                      // 0x18
    uint32_t mF1C;                    // 0x1c
};

class CSky {
public:
    CSky();                            // 0x1002996c
    ~CSky();                           // 0x10029e18
    int32_t Load(CTextureEntry** texture, char* path); // 0x10029e94
    int32_t LoadAll(char* sky, char* colors, char* cylinder); // 0x10029f88
    int32_t LoadAdd(void** out, char* path, D3DVECTOR position, float size,
                    int32_t a, int32_t b); // 0x1002a1f8
    void RemoveAdd(int32_t index);      // 0x1002a3a8
    int32_t RemoveAdd(void* texture);   // 0x1002a42c
    int32_t SkyLoad(char* folder);      // 0x1002a5f4
    int32_t Init();
    void InitColors();                 // 0x1002b17c
    int32_t Close();                    // 0x1002b474
    void SetSpherePos(D3DVECTOR pos);   // 0x1002a938
    void SetSphereRadius(float radius);// 0x1002a9b4
    void ResetSpherePos();             // 0x1002aa08
    void SetupScrollDirection(int32_t layer, D3DVECTOR direction); // 0x1002aa6c
    void SetActiveLayers(int32_t layers);   // 0x1002aad8
    CTextureEntry* GetSkyTEntry();      // 0x1002b554
    D3DVECTOR* GetPosition();           // 0x1002b584
    // Per frame (src/tb_sky_render.cpp). Prepare scrolls the layers and has the texture cache
    // place the sky's pictures; RenderAdd draws the added sprites whose layer mask (SSkyAdd::
    // mF08) meets `layers`; Render draws the sky layers, the cylinder and the sprites. Each
    // returns 0 when a draw call failed, else 1.
    int32_t Prepare();                  // 0x1002b5b4
    int32_t RenderAdd(int32_t layers);  // 0x1002b808
    int32_t Render();                   // 0x1002bb04

    D3DVECTOR mCylinder[72];            // 0x0000
    D3DVECTOR mGrid[256];               // 0x0360, column first
    uint32_t mColors[4][256];           // 0x0f60, column first
    int32_t mIndexCount;                // 0x1f60
    D3DVECTOR mSpherePosition;          // 0x1f64
    D3DVECTOR mScrollDirection[4];      // 0x1f70
    D3DVECTOR mPosition[4];             // 0x1fa0
    CTextureEntry* mSkyTexture;         // 0x1fd0
    CTextureEntry* mColorTexture;       // 0x1fd4
    CTextureEntry* mCylinderTexture;    // 0x1fd8
    SSkyAdd mAdds[16];                 // 0x1fdc
    int32_t mAddCount;                  // 0x21dc
    int32_t mWidth, mHeight;            // 0x21e0, 0x21e4
    float mSphereRadius;               // 0x21e8, not used by Init
    float mLayerAngles[4];             // 0x21ec
    uint16_t mIndices[1350];           // 0x21fc
    uint16_t mCylinderIndices[288];     // 0x2c88
    int32_t mActiveLayers;              // 0x2ec8
    float mMatrix[16];                 // 0x2ecc
    int32_t mF2F0C, mF2F10;             // 0x2f0c, 0x2f10
};
static_assert(offsetof(CSky, mSkyTexture) == 0x1fd0, "CSky fields before native pointers");
static_assert(offsetof(CSky, mF2F10) - offsetof(CSky, mAddCount) == 0x2f10 - 0x21dc,
              "CSky fields after native pointers");
extern CSky gei_Sky;   // engine export
