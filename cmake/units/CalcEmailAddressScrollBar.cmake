# Unit v5017: the online outbox panel's helpers (FillOutboxList, SetCurrentEmailInfo, CalcEmailAddressScrollBar, UpdatePreview;
# src/UI_IslandLobby_CalcEmailAddressScrollBar.cpp). The callees outside the unit are recording fakes in
# tests/unit_CalcEmailAddressScrollBar.cpp, which also defines the globals of other files. CoolRect is the program's (UI_Base.cpp).
set(TPW_UNIT_CalcEmailAddressScrollBar src/UI_IslandLobby_CalcEmailAddressScrollBar.cpp src/UI_Base.cpp platform/arena.cpp)
