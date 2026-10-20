# Unit v5085: the shop status panel (ShopStatusPanel::InitWindow, UpdateStatusWindow; src/ShopStatusPanel_InitWindow.cpp). The callees outside the unit are recording fakes in
# tests/unit_ShopStatusPanel_InitWindow.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_ShopStatusPanel_InitWindow src/ShopStatusPanel_InitWindow.cpp platform/arena.cpp)
