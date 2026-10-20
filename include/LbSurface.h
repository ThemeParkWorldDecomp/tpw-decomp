#pragma once
#include "TbGraphicFile.h"
#include "TbColour.h"
#include "platform/indirectx.h"

// Unit w231 (src/LbSurface.cpp), bullfrog WinScrn.cpp.
TbRenderArea& LbSurface_MapToRenderArea(TbRenderArea&, TbSurface*); // 0x100666ac, 136 B
TbPixFormat& LbSurface_GetPixelFormat(const TbSurface*, TbPixFormat&); // 0x10066780, 100 B
void LbSurface_SetCurrent(TbSurface*); // 0x1006682c, 36 B
TbSurface* LbSurface_GetCurrent(); // 0x10066888, 8 B
uint8_t* LbSurface_GetOriginPtr(const TbSurface*); // 0x100668bc, 8 B
uint32_t LbSurface_GetDrawFlags(const TbSurface*); // 0x100668fc, 8 B
void LbSurface_SetDrawFlags(TbSurface*, uint32_t); // 0x1006693c, 76 B
void LbSurface_SetDrawFlagsOn(TbSurface*, uint32_t); // 0x100669c4, 92 B
void LbSurface_SetDrawFlagsOff(TbSurface*, uint32_t); // 0x10066a5c, 92 B
uint8_t* LbSurface_GetFadeTable(const TbSurface*); // 0x10066af4, 8 B
void LbSurface_SetFadeTable(TbSurface*, uint8_t*); // 0x10066b34, 100 B
int32_t LbSurface_GetFadeStep(const TbSurface*); // 0x10066bd4, 20 B
void LbSurface_SetFadeStep(TbSurface*, uint32_t); // 0x10066c20, 96 B
uint8_t LbSurface_GetFadeColour(TbSurface*, uint32_t); // 0x10066cb8, 12 B
uint8_t* LbSurface_GetGhostTable(const TbSurface*); // 0x10066d00, 8 B
void LbSurface_SetGhostTable(TbSurface*, uint8_t*); // 0x10066d44, 76 B
void LbSurface_SetSysLock(int32_t); // 0x10066f80, 20 B
void LbSurface_RegisterDirectDraw(IDirectDraw*); // 0x10066fc0, 12 B
TbResult LbSurface_RegisterDirectDrawSurface(IDirectDrawSurface*, TbSurface*); // 0x1006700c, 164 B
void _InitialiseSurface(TbSurface*, uint32_t, uint32_t, uint32_t, uint32_t); // 0x1006710c, 48 B
int32_t _CheckAndRestoreSurface(const TbSurface*); // 0x10067178, 96 B
TbResult LbSurface_ClearRect(const TbSurface*, const TbRect*, TbColour, uint32_t); // 0x10067214, 640 B
TbResult LbSurface_Create(uint32_t, uint32_t, uint32_t, TbSurface*); // 0x100674e0, 240 B
TbResult LbSurface_Delete(TbSurface*); // 0x10067608, 76 B
TbResult LbSurface_Blt(const TbSurface*, uint32_t, uint32_t, const TbSurface*, const TbRect*, uint32_t); // 0x10067688, 556 B
TbResult LbSurface_StretchBlt(const TbSurface*, const TbRect*, const TbSurface*, const TbRect*, uint32_t); // 0x10067900, 740 B
TbResult LbSurface_Lock(TbSurface*, uint8_t**); // 0x10067c3c, 288 B
void LbSurface_Unlock(TbSurface*); // 0x10067d90, 44 B
IDirectDrawSurface* LbSurface_GetDDSurface(TbSurface*, int32_t); // 0x10067df0, 72 B

extern uint32_t _lbCurrentDrawFlags;      // 0x10074834
extern uint8_t* _lbCurrentGhostTable;     // 0x10074830
extern uint8_t* _lbpCurrentFadeTable;     // 0x1007482c
extern uint8_t* _lbpCurrentFadeRow;        // 0x10074828
extern uint8_t gSurfaceSysLockDisabled;  // 0x100748e0 (name invented)
extern IDirectDraw* gSurfaceDirectDraw;  // 0x100748e4 (name invented)

// Outside this unit, recording boundaries in its tests.
void LbDraw_SetCurrent(TbSurface*); // bullfrog 0x1002b690, 280 B
TbRect* LbRect_Copy(TbRect*, const TbRect*); // bullfrog 0x100298d0, 36 B
void OS_dd_error(int32_t); // bullfrog 0x10067e70, 1028 B

// Unit v5945 (src/LbSurface_LbScreen_IsModeAvailable_2.cpp), bullfrog WinScrn.cpp.
struct TbPalette;
struct TbScreenModeFind;
enum TbPaletteFadeType : int32_t { kPaletteFadeStep = 0, kPaletteFadeAll = 1 };
uint32_t LbScreen_IsModeAvailable(uint32_t, uint32_t, uint32_t); // 0x10068298, 288 B
uint32_t LbScreen_FadePalette(TbPalette*, uint32_t, TbPaletteFadeType); // 0x100683ec, 728 B
void LbScreen_ModeFindEnd(TbScreenModeFind*); // 0x100690bc, 4 B
void LbScreen_WaitVbi(); // 0x1006a400, 52 B
extern int32_t gLbScreenModeFlagA;   // 0x10073a40 (name invented)
extern int32_t gLbScreenModeFlagB;   // 0x10073a44 (name invented)
extern int32_t gLbFadeActive;        // 0x100748e8 (name invented)
extern uint32_t gLbFadeStep;         // 0x100748ec (name invented)
extern uint8_t gLbFadeSaved[0x400];  // 0x100748f0 (name invented)
extern uint8_t gLbFadeZero[0x400];   // 0x10074cf0 (name invented)
