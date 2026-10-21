# Unit v5292: OnlineChatData constructor and destructor (src/COnlineControl_..._SaveIgnore_OnlineChatData_OnlineChatData.cpp). The callees are
# recording fakes in tests/unit_OnlineChatData_OnlineChatData.cpp.
set(TPW_UNIT_OnlineChatData_OnlineChatData src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_OnlineChatData_OnlineChatData_KEEP
  __ZN14OnlineChatData8DestructEPS_s   # OnlineChatData::Destruct(OnlineChatData*, short)
  __ZN14OnlineChatData9ConstructEPS_   # OnlineChatData::Construct(OnlineChatData*)
)
