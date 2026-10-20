# Unit v5188: GotoLastUsedPanel and DoCashWin (src/ui_mainpanel2_msg_GotoLastUsedPanel.cpp). The callees are recording fakes in
# tests/unit_GotoLastUsedPanel.cpp, which also defines the globals of other files. UI_Base.cpp gives the real CoolRect.
set(TPW_UNIT_GotoLastUsedPanel src/ui_mainpanel2_msg_GotoLastUsedPanel.cpp src/UI_Base.cpp platform/arena.cpp)
