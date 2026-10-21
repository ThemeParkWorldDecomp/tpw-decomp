# Unit v5761: LbScreen_RestoreMode (src/bullfrog/WinScrn.cpp). The callees outside the unit are
# recording fakes in tests/unit_LbScreen_RestoreMode.cpp, which also defines the globals of other files.
set(TPW_UNIT_LbScreen_RestoreMode src/bullfrog/WinScrn.cpp platform/arena.cpp)
set(TPW_UNIT_LbScreen_RestoreMode_KEEP
  __Z20LbScreen_RestoreModev   # LbScreen_RestoreMode()
)
