# Unit v5041: WinScrn.cpp part (CreateDirectDrawObject, EnumModesCallback, LbScreen_GetPaletteEntries, LbScreen_SetPaletteEntries,
# LbScreen_SwapClear, __sinit_WinScrn_cpp; src/CreateDirectDrawObject.cpp). The callees outside the unit are recording fakes in
# tests/unit_CreateDirectDrawObject.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateDirectDrawObject src/CreateDirectDrawObject.cpp platform/arena.cpp)
