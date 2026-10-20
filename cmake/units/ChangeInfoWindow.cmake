# Unit v5102: ChangeInfoWindow and UpdateHelpText (src/ChangeInfoWindow.cpp). The callees outside the unit are recording fakes in
# tests/unit_ChangeInfoWindow.cpp, which also defines the globals of other files. CoolRect runs for real (src/UI_Base.cpp).
set(TPW_UNIT_ChangeInfoWindow src/ChangeInfoWindow.cpp src/UI_Base.cpp platform/arena.cpp)
