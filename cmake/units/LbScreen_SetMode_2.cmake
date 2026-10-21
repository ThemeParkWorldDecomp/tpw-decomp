# Unit v51000: LbScreen_SetMode (src/bullfrog/WinScrn.cpp). The callees outside the unit are recording fakes in
# tests/unit_LbScreen_SetMode_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_LbScreen_SetMode_2 src/bullfrog/WinScrn.cpp platform/arena.cpp)
set(TPW_UNIT_LbScreen_SetMode_2_KEEP
  __Z16LbScreen_SetModejjjjPK9TbPalette   # LbScreen_SetMode(unsigned int, unsigned int, unsigned int, unsigned int, TbPalette const*)
)
