// The game's CLight (main app, emitted in Advisor.cpp). Native objects, not file images.
// Mac 0x68 bytes, with a virtual CMapLinkBase at +0x60.
#pragma once
#include "MapWho.h"
#include "m3d2.h"

class CLight : public virtual CMapLinkBase {
public:
    CLight();                    // 0x10007a98, also inline in CLBLobbyView
    ~CLight() override;          // 0x100079f4
    int32_t Prepare() override;  // 0x10007704
    int32_t GetLinkType() override; // 0x100076d0: 1
    void* GetData() override;    // 0x10007688: +0x38 for directional lights

    float mColor[4];             // 0x04: alpha, red, green, blue
    s3DPoint mPosition;          // 0x14
    s3DPoint mF20;               // 0x20
    s3DPoint mNormal;            // 0x2c
    uint32_t mData[3];           // 0x38
    float mRadius;              // 0x44
    float mRadiusSquared;       // 0x48
    float mInverseRadiusSquared;// 0x4c
    uint32_t mFlags;            // 0x50
    uint32_t mColorWord;        // 0x54: RGBA bytes
    CLight* mNext;              // 0x58

    // The setters (src/LevelLights.cpp, unit w110). Each starts with gei_System.UpdateLights().
    // 0x1000784c (236 bytes): the four colour floats (alpha, red, green, blue), then the colour word from them:
    // alpha in the low byte, blue in bits 8..15, green in bits 16..23, red in the top byte, each as (int)(255.0f * value).
    void SetColor(float alpha, float red, float green, float blue);
    // 0x10007964 (100 bytes): mNormal = (x, y, z).
    void SetNormal(float x, float y, float z);
    // 0x100641f8 (92 bytes): mRadius, its square and 1.0f over the square (infinity for a radius of 0).
    void SetRadius(float radius);
};

// The two kinds of light CreateGlobalLight (0x10063d24, src/LevelLights.cpp) makes, 0x74 bytes each on the Mac:
// +0 a pointer to the CLight part at +0x10, +4 the class's vtable, +8 the shared CMapLinkBase. They add no members and no
// overrides to the light; the Mac gives each its own destructor (CLightParallel::~CLightParallel 0x10007750,
// CLightPoint::~CLightPoint 0x1006427c, 208 bytes each, untranslated: the host's compiler writes them).
class CLightParallel : public virtual CLight {};   // vtable record 0x10278c58, flags 2 (a direction in mNormal)
class CLightPoint : public virtual CLight {};      // vtable record 0x1029cfac, flags 4 (a position in mPosition)
extern CLight* gev_LightChain;  // engine export, prepared lights

// Unit v5003: the 3DFont.cpp pixel blend IntensifyPix<CTraits<N>> (anonymous namespace on the Mac; N is the byte count of a pixel:
// 4 = 0x100073b8, 3 = 0x100071f4, __vec2x32float__ (2) = 0x10007030, __int128 (1) = 0x10006e6c; 352 bytes each).
struct TbRect;
namespace tpw_intensify {
template <int N> struct CTraits {};
// Blends the pixel at pix toward colour (weight 255*(1-|alpha|)) when (x, y) lies in rect and |alpha| <= 1, then stores N bytes, low byte first.
template <int N>
void IntensifyPix(const CTraits<N>& traits, uint8_t* pix, double alpha, uint32_t colour, uint32_t mask1, uint32_t mask2,
                  uint32_t mask3, uint32_t keepMask, int32_t x, int32_t y, const TbRect& rect);
}  // namespace tpw_intensify

// Unit v5109: the 3DFont.cpp static initialiser (0x1000757c, src/Light_sinit_3DFont_cpp.cpp). It builds one CLightParallel
// (Mac global 0x102b34c8, 0x74 bytes) and its registration node (Mac 0x102b353c); the host's storage is declared here.
extern unsigned char gSinit3DFontLight[0x100];
extern unsigned char gSinit3DFontNode[16];
void __sinit_3DFont_cpp();   // 0x1000757c

// Unit v5272: the light-node helpers (src/LevelSystems_SetLightColourExplicit.cpp). Each starts with gei_System.UpdateLights().
// The parameter block of a light node: mLight points at a slot holding the CLight (Mac +0), mColour at three floats r, g, b (Mac +4).
struct sNodeLightParams {
    CLight** mLight;   // Mac 0x00
    float* mColour;    // Mac 0x04
};
void SetLightColourExplicit(sNodeLightParams* params, float red, float green, float blue);  // 0x1006498c (232)
void EnableLight(sNodeLightParams* params);                                                // 0x10064ab8 (68): clears flag 0x10000000
void SetLightColourRelative(sNodeLightParams* params, float scale);                        // 0x10064854 (248): mColour * scale
void DisableLight(sNodeLightParams* params);                                               // 0x10064b30 (68): sets flag 0x10000000

// Unit v51050: CLightParallel::~CLightParallel 0x10007750 (208), the Mac destructor on the Mac's raw 32-bit layout (the host's own
// destructor is the compiler's). `self` and every pointer it follows are 32-bit addressable words; flag is the hidden delete flag.
// Mac globals it reads (only their address is used, as the vtable record words it stores): 0x10278c60, 0x10278c9c, 0x10278cb4.
extern unsigned char gCLightParallelVt1[0x40];   // 0x10278c60
extern unsigned char gCLightParallelVt2[0x40];   // 0x10278c9c
extern unsigned char gCLightParallelVt3[0x40];   // 0x10278cb4
void CLightParallel_Delete(void* block);   // operator delete (0x10238218), replaceable by tests
CLightParallel* CLightParallel_DtorMac(CLightParallel* self, int16_t flag);

// Unit v51119 (src/lobby_globe_close_CLBScreenComponentOnlineGlobe_Show_2_CLBScreenComponentOnlineGlobe_GlobeRight.cpp): destructors
// with the hidden delete flag on the Mac's raw layout, and the byte swaps (one function per address, names invented).
class Bitmap;
struct MapDelta;
struct BasicFilebufChar;
extern unsigned char gCLightPointVt[0x40];   // 0x1029cfb4 (initial word 0x1029cfac)
Bitmap* Bitmap_DtorMac(Bitmap* self, int16_t flag);                                  // 0x100ca4bc (main copy)
MapDelta* MapDelta_DtorMac(MapDelta* self, int16_t flag);                            // 0x10082c88
BasicFilebufChar* BasicFilebufChar_DtorMac(BasicFilebufChar* self, int16_t flag);    // 0x100700e4
void* BasicIosChar_DtorMac(void* self, int16_t flag);                                // 0x1006ff50
CLightPoint* CLightPoint_DtorMac(CLightPoint* self, int16_t flag);                   // 0x1006427c
void Bitmap_DeleteArray(void* block);                                                // operator delete[] (0x10238638), replaceable by tests
namespace v51119_swap {
void USwapInplace16_short_100de404(int16_t& v);
void USwapInplace32_long_100de448(int32_t& v);
void USwapInplace32_long_10079a64(int32_t& v);
void USwapBlock32_long_100799b0(int32_t* words, int32_t count);
void USwapInplace32_ulong_10079968(uint32_t& v);
void USwapInplace32_ulong_1006b6ac(uint32_t& v);
void USwapInplace32_int_1006b668(int32_t& v);
void USwapInplace16_ushort_1006b620(uint16_t& v);
}
