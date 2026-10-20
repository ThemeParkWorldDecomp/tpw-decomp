# Unit v5737: LbScreen_Terminate, LbApp_ShowMessage, LbScreen_GetDirectDraw, LbScreen_ModeFindFirst/Next
# (src/LbScreen_Initialise_LbScreen_Terminate.cpp). The callees outside the unit are recording fakes in
# tests/unit_LbScreen_Terminate.cpp, which also defines the globals of other files.
set(TPW_UNIT_LbScreen_Terminate src/LbScreen_Initialise_LbScreen_Terminate.cpp src/LbList.cpp platform/arena.cpp)
