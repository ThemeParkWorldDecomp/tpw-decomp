# Unit v5958: the main menu's quit answer, item callback and fade, and CFSLoadSave::SaveInternal / GetFirstSaveFileDetails
# (src/ui_iterators_MainGameMenuConfirmPublish_OK_MainGameMenuConfirmQuit_OK.cpp). The callees outside the unit are recording fakes in
# tests/unit_MainGameMenuConfirmQuit_OK.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_MainGameMenuConfirmQuit_OK src/ui_iterators_MainGameMenuConfirmPublish_OK_MainGameMenuConfirmQuit_OK.cpp platform/arena.cpp)
