# Unit v5697: the progress bar (CreateUIProgressBar x2, UpdateUIProgressBar; src/game/ui/ui_progressBar.cpp). The callees
# outside the unit are recording fakes in tests/unit_CreateUIProgressBar.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateUIProgressBar src/game/ui/ui_progressBar.cpp platform/arena.cpp)
set(TPW_UNIT_CreateUIProgressBar_KEEP
  __Z19CreateUIProgressBarPDs   # CreateUIProgressBar(char16_t*)
  __Z19CreateUIProgressBari   # CreateUIProgressBar(int)
  __Z19UpdateUIProgressBari   # UpdateUIProgressBar(int)
  _gUIProgressBarText
)
