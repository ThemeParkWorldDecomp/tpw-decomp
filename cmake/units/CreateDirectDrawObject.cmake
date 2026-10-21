# Unit v5041: WinScrn.cpp part (CreateDirectDrawObject, EnumModesCallback, LbScreen_GetPaletteEntries, LbScreen_SetPaletteEntries,
# LbScreen_SwapClear, __sinit_WinScrn_cpp; src/bullfrog/WinScrn.cpp). The callees outside the unit are recording fakes in
# tests/unit_CreateDirectDrawObject.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateDirectDrawObject src/bullfrog/WinScrn.cpp platform/arena.cpp)
set(TPW_UNIT_CreateDirectDrawObject_KEEP
  __Z17EnumModesCallbackP13DDSURFACEDESCPv   # EnumModesCallback(DDSURFACEDESC*, void*)
  __Z18LbScreen_SwapClear8TbColour   # LbScreen_SwapClear(TbColour)
  __Z19__sinit_WinScrn_cppv   # __sinit_WinScrn_cpp()
  __Z22CreateDirectDrawObjectPN10NS_MacDoze5_GUIDEPP11IDirectDrawPPv   # CreateDirectDrawObject(NS_MacDoze::_GUID*, IDirectDraw**, void**)
  __Z26LbScreen_GetPaletteEntriesP14TbPaletteEntryjj   # LbScreen_GetPaletteEntries(TbPaletteEntry*, unsigned int, unsigned int)
  __Z26LbScreen_SetPaletteEntriesPK14TbPaletteEntryjj   # LbScreen_SetPaletteEntries(TbPaletteEntry const*, unsigned int, unsigned int)
  _gScreenFPUHandler
  _gScreenFPUHandlerNode
  _gWinScrnSurfaceA
  _gWinScrnSurfaceB
  _gWinScrnSurfaceC
  _kScreenFPUHandlerDtorVector
  _kScreenFPUHandlerVtable
)
