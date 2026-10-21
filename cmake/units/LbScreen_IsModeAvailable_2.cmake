# Unit v5945: screen mode lookup, palette fade, OS_dd_error (src/bullfrog/LbSurface.cpp, src/bullfrog/WinScrn.cpp).
set(TPW_UNIT_LbScreen_IsModeAvailable_2 src/bullfrog/LbSurface.cpp src/bullfrog/WinScrn.cpp platform/arena.cpp)
set(TPW_UNIT_LbScreen_IsModeAvailable_2_KEEP
  __Z11OS_dd_errori   # OS_dd_error(int)
  __Z20LbScreen_FadePaletteP9TbPalettej17TbPaletteFadeType   # LbScreen_FadePalette(TbPalette*, unsigned int, TbPaletteFadeType)
  __Z24LbScreen_IsModeAvailablejjj   # LbScreen_IsModeAvailable(unsigned int, unsigned int, unsigned int)
  ___destroy_arr
  _gLbFadeActive
  _gLbFadeSaved
  _gLbFadeStep
  _gLbFadeZero
  _gLbScreenModeFlagA
  _gLbScreenModeFlagB
)
