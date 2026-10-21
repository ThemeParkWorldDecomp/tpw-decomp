# Unit v5334: LbScreen_Swap, LbScreen_PartSwap, CreateScreenModeList (src/bullfrog/WinScrn.cpp). The callees outside the unit are
# recording fakes in tests/unit_LbScreen_PartSwap.cpp, which also defines the globals of other files.
set(TPW_UNIT_LbScreen_PartSwap src/bullfrog/WinScrn.cpp platform/arena.cpp)
set(TPW_UNIT_LbScreen_PartSwap_KEEP
  __Z13LbScreen_Swapj   # LbScreen_Swap(unsigned int)
  __Z17LbScreen_PartSwapRK7TbPointRK6TbRectj   # LbScreen_PartSwap(TbPoint const&, TbRect const&, unsigned int)
  __Z20CreateScreenModeListiP8GrafPortPN10NS_MacDoze5_GUIDE   # CreateScreenModeList(int, GrafPort*, NS_MacDoze::_GUID*)
  _gWinScrnClientOrigin
  _gWinScrnDoubleBuffer
  _gWinScrnModeGate
  _gWinScrnModeList
)
