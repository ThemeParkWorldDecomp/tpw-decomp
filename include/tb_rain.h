// tb_rain.cpp (engine library). Mac offsets below; 0xa1cc bytes on the Mac.
// Pointer members grow on a native host. The rendering members are in src/tb_rain_frame.cpp
// (Prepare, Render, RenderLightning2) and src/tb_rain_render.cpp (the flash, the lightning, the
// rain and the snow); the bolts' own members AddLightning and SLightning::Init are in
// src/tb_rain_lightning.cpp, along with NewRenderLightning, AddLightningSection and GeneratePoint.
#pragma once
#include <cstdint>
#include <cstddef>
#include "platform/d3d_types.h"

class CTextureEntry;
struct SLightning {
    D3DVECTOR mStart, mEnd, mDirection; // 0x00, 0x0c, 0x18
    int32_t mTime;                    // 0x24
    uint32_t mF28, mSeed;              // 0x28, 0x2c
    // 0x100244a0 (src/tb_rain_lightning.cpp): sets the bolt up. `seeded` non-zero stamps the cycle
    // counter ORed with 1 as the seed (an integer word), zero clears it.
    void Init(D3DVECTOR start, D3DVECTOR end, int32_t time, int32_t seeded);
};
static_assert(sizeof(SLightning) == 0x30, "SLightning layout");
class CRain {
public:
    CRain();                                              // 0x1002059c
    int32_t Init();                                        // 0x100206f8
    int32_t Close();                                       // 0x10020c64
    int32_t Load(int32_t kind, char* path);                 // 0x10020d44
    int32_t GameLoad();                                    // 0x10020e28
    void SetRainType(int32_t type);                        // 0x10020f98
    void SetLength(float length);                          // 0x10020fc8
    void SetRaindrops(int32_t count);                       // 0x10020ff8
    void SetDirection(D3DVECTOR direction);                // 0x1002102c
    void SetDirection(float x, float y, float z);           // 0x10021088
    void SetWindParameters(float direction, float speed);  // 0x100210c4
    void SetVelocity(float speed);                         // 0x100214d0
    void LightningFlash();                                 // 0x1002151c
    // 0x100243cc (src/tb_rain_lightning.cpp): a bolt between two points, dropped when the 16 slots
    // are full; answers 1. The lobby passes the point on the ground first, then the one at height
    // 500 above it, and then 500 and 1.
    int32_t AddLightning(D3DVECTOR start, D3DVECTOR end, int32_t a, int32_t b);
    void SetSnowCubicParameters(float a, float b, float c); // 0x100233ac
    // The frame's members (src/tb_rain_frame.cpp); Render returns 0 on failure.
    int32_t Prepare();                                     // 0x10021554
    int32_t Render();                                      // 0x10023f40
    // One lightning bolt from `from` to `to` (a primitive of its own); 1 when the draw primitive
    // has no room for one.
    int32_t RenderLightning2(D3DVECTOR* from, D3DVECTOR* to);   // 0x1002294c
    // RenderLightning2's subdivision and textured quad builder (src/tb_rain_lightning.cpp).
    int32_t AddLightningSection(D3DVECTOR* from, D3DVECTOR* to);    // 0x1002163c
    int32_t NewRenderLightning(D3DVECTOR* from, D3DVECTOR* to);     // 0x100223d8

    // Render's callees (src/tb_rain_render.cpp). All answer 0 when drawing fails.
    // The white flash of a strike: clears mLightningFlash and draws a sprite over the screen;
    // answers DrawSprite's.
    int32_t __LightningFlash();                                     // 0x10022b20
    // The flash if one is asked for, then the bolts (ageing them, dropping the dead ones, drawing
    // the rest through RenderLightning2). mF3308 is the count of bolts (the original's
    // md_LightningsUsed).
    int32_t __RenderLightning();                                    // 0x10022c10
    // A streak of rain per drop; (x, y, z) is the camera's place wrapped into the drops' window
    // with the wind offset added (Render works it out). 1 when there is no room for a primitive.
    int32_t __RenderRain(float x, float y, float z);                // 0x10022efc
    // A flake of snow per drop, the same arguments.
    int32_t __RenderSnow(float x, float y, float z);                // 0x100233f8

    int32_t mRainType;                 // 0x0000
    int32_t mLightningFlash;           // 0x0004
    D3DVECTOR mDrops[1024];            // 0x0008
    SLightning mLightning[16];        // 0x3008
    int32_t mF3308, mF330C;            // 0x3308, 0x330c
    float mF3310, mLength;             // 0x3310, 0x3314
    int32_t mRaindrops;                // 0x3318
    D3DVECTOR mDirection;              // 0x331c
    D3DVECTOR mF3328;                  // 0x3328
    int32_t mF3334;                    // 0x3334
    float mRainVelocity, mSnowVelocity;// 0x3338, 0x333c
    float mF3340;                     // 0x3340
    uint32_t mClock;                   // 0x3344
    CTextureEntry* mSnowTexture;       // 0x3348
    CTextureEntry* mLightningTexture;  // 0x334c
    CTextureEntry* mRainTexture;       // 0x3350
    float mF3354;                     // 0x3354
    float mEnvelope[128];             // 0x3358
    float mSnowTimes[1024];            // 0x3558
    float mSnowRates[1024];            // 0x4558
    D3DVECTOR mSnowOffsets[1024];      // 0x5558
    D3DVECTOR mSnowCubics[128][4];     // 0x8558
    float mMatrix[16];                // 0x9d58
    float mDeltaTime;                 // 0x9d98, seconds
    int32_t mDeltaMilliseconds;       // 0x9d9c
    int32_t mCurrentLightning;        // 0x9da0
    int32_t mLightningCounts[16];     // 0x9da4
    float mLightningPoints[16][4];    // 0x9de4, projected xyzw
    float mLightningEdges[16][8];     // 0x9ee4, projected pairs of xyzw
    float mLightningNormals[16][2];   // 0xa0e4
    float mFA164, mFA168;             // 0xa164, 0xa168
    float mFA16C, mFA170, mFA174, mFA178, mFA17C; // 0xa16c..0xa17c
    float mFA180, mFA184;              // 0xa180, 0xa184
    int32_t mFA188;                    // 0xa188
    D3DVECTOR mSnowParameters;         // 0xa18c
    int32_t mFA198;                   // 0xa198
    uint32_t mLightningColor;         // 0xa19c
    int32_t mLightningDepth;          // 0xa1a0
    int32_t mFA1A4, mFA1A8;            // 0xa1a4, 0xa1a8
    D3DVECTOR mFA1AC;                  // 0xa1ac
    float mFA1B8;                     // 0xa1b8
    D3DVECTOR mFA1BC;                  // 0xa1bc
    float mFA1C8;                     // 0xa1c8
};
static_assert(offsetof(CRain, mSnowTexture) == 0x3348, "CRain fields before native pointers");
static_assert(offsetof(CRain, mFA1C8) - offsetof(CRain, mF3354) == 0xa1c8 - 0x3354,
              "CRain fields after native pointers");
extern CRain* gei_Rain;               // 0x100b1628

// The low 32 bits of the platform timer's cycle counter (the Mac's time base), which
// __RenderLightning stamps a bolt without a seed with (src/tb_rain_render.cpp).
uint32_t GetCycleCount();             // 0x10038b14
// The engine imports Random from the C library; 0x1004c5e8 is its glue stub. The Mac's
// generator is not in the dump; the launcher interim returns 15-bit values like rand().
int32_t Random();
// 0x10021f48: randomly displaced midpoint (src/tb_rain_lightning.cpp).
void GeneratePoint(D3DVECTOR* out, D3DVECTOR* a, D3DVECTOR* b, float scale);
int32_t Rain_Init();                  // 0x10020458
int32_t Rain_Close();                 // 0x1002051c
