# Unit v5056: WinScrn.cpp's LbScreen_Initialise, ReleaseMode, WindowProc (src/bullfrog/WinScrn.cpp). The callees outside the unit are
# recording fakes in tests/unit_LbScreen_Initialise.cpp, which also defines the globals of other files.
set(TPW_UNIT_LbScreen_Initialise src/bullfrog/WinScrn.cpp platform/arena.cpp)
set(TPW_UNIT_LbScreen_Initialise_KEEP
  __Z10WindowProcP8GrafPortjjPi   # WindowProc(GrafPort*, unsigned int, unsigned int, int*)
  __Z11ReleaseModev   # ReleaseMode()
  __Z19LbScreen_InitialisePN10NS_MacDoze5_GUIDEiPvS2_   # LbScreen_Initialise(NS_MacDoze::_GUID*, int, void*, void*)
  _gLbSystemEventCallback
  _gWinScrnActive
  _gWinScrnClassName
  _gWinScrnClipper
  _gWinScrnCoopWindow
  _gWinScrnDirectDraw
  _gWinScrnF120
  _gWinScrnFocus
  _gWinScrnGuid
  _gWinScrnInitialised
  _gWinScrnModeFlag
)
