# Unit v5079: the online panel's message function (OnlinePanelMsg; src/OnlinePanelMsg.cpp). The callees outside the unit are recording fakes in
# tests/unit_OnlinePanelMsg.cpp, which also defines the globals of other files. CoolRect is the real one (src/UI_Base.cpp).
set(TPW_UNIT_OnlinePanelMsg src/OnlinePanelMsg.cpp src/UI_Base.cpp platform/arena.cpp)
