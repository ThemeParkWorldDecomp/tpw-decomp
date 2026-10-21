# Unit v5737: LbScreen_Terminate, LbApp_ShowMessage, LbScreen_GetDirectDraw, LbScreen_ModeFindFirst/Next
# (src/bullfrog/WinScrn.cpp). The callees outside the unit are recording fakes in
# tests/unit_LbScreen_Terminate.cpp, which also defines the globals of other files.
set(TPW_UNIT_LbScreen_Terminate src/bullfrog/WinScrn.cpp src/bullfrog/LbLog.cpp platform/arena.cpp)
set(TPW_UNIT_LbScreen_Terminate_KEEP
  __Z11LbList_InitP6TbList   # LbList_Init(TbList*)
  __Z13LbList_InsertP6TbListP10TbListNodeS2_   # LbList_Insert(TbList*, TbListNode*, TbListNode*)
  __Z14LbList_GetLastPK6TbList   # LbList_GetLast(TbList const*)
  __Z15LbList_GetFirstPK6TbList   # LbList_GetFirst(TbList const*)
  __Z17LbApp_ShowMessagePKcz   # LbApp_ShowMessage(char const*, ...)
  __Z17LbList_NodeExistsPK6TbListPK10TbListNode   # LbList_NodeExists(TbList const*, TbListNode const*)
  __Z18LbScreen_Terminatev   # LbScreen_Terminate()
  __Z21LbScreen_ModeFindNextP16TbScreenModeFindP12TbScreenMode   # LbScreen_ModeFindNext(TbScreenModeFind*, TbScreenMode*)
  __Z22LbScreen_GetDirectDrawi   # LbScreen_GetDirectDraw(int)
  __Z22LbScreen_ModeFindFirstP16TbScreenModeFindP12TbScreenMode   # LbScreen_ModeFindFirst(TbScreenModeFind*, TbScreenMode*)
)
