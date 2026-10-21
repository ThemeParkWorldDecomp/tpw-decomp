# Unit v5284: bullfrog LbSCBack.cpp helpers (src/bullfrog/LbApp.cpp, src/bullfrog/LbSCBack.cpp). The callees outside the unit are recording fakes in
# tests/unit_LbApp_GetWindowsScreenMode.cpp.
set(TPW_UNIT_LbApp_GetWindowsScreenMode src/bullfrog/LbApp.cpp src/bullfrog/LbSCBack.cpp platform/arena.cpp)
set(TPW_UNIT_LbApp_GetWindowsScreenMode_KEEP
  __Z19LbApp_SetWindowNamePKc   # LbApp_SetWindowName(char const*)
  __Z20__sinit_LbSCBack_cppv   # __sinit_LbSCBack_cpp()
  __Z25LbSEFunctionList_DestructPv   # LbSEFunctionList_Destruct(void*)
  __Z26LbApp_GetWindowsScreenModeR12TbScreenMode   # LbApp_GetWindowsScreenMode(TbScreenMode&)
  __Z26LbSEFunctionList_ConstructPv   # LbSEFunctionList_Construct(void*)
  __Z27LbApp_ProcessWindowMessagesv   # LbApp_ProcessWindowMessages()
  __Z30LbApp_EnableWindowsScreenSaveri   # LbApp_EnableWindowsScreenSaver(int)
  __Z30LbSystemEventCallback_DestructPv   # LbSystemEventCallback_Destruct(void*)
  _gLbSCBack
  _gLbWindowName
)
