# Unit v5022: the congratulations and bankrupt windows of the main app (UI_Congratulations, UI_Bankrupt, CongratsWindowMsg,
# __sinit_ui_main_cpp; src/ui_main_panel_UI_Congratulations.cpp). The callees outside the unit are recording fakes in
# tests/unit_UI_Congratulations.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_UI_Congratulations src/ui_main_panel_UI_Congratulations.cpp platform/arena.cpp)
