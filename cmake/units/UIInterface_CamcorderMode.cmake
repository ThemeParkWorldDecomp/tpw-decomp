# Unit v51038: UIInterface::CamcorderMode, EnableResearchButton, StaffRoomEvent, KidArrived, ErrorDlgCancel, AddUIError and the block table's
# destructor (src/game/ui/UIInterface.cpp, src/game/ui/ui_ErrorDisplay.cpp). The callees outside the unit are recording fakes in
# tests/unit_UIInterface_CamcorderMode.cpp, which also defines the globals of other files.
set(TPW_UNIT_UIInterface_CamcorderMode src/game/ui/UIInterface.cpp src/game/ui/ui_ErrorDisplay.cpp platform/arena.cpp
    tests/standins/UIInterface_CamcorderMode.cpp)
set(TPW_UNIT_UIInterface_CamcorderMode_KEEP
  __Z10AddUIErroriP8TbITable   # AddUIError(int, TbITable*)
  __Z14ErrorDlgCancelPv   # ErrorDlgCancel(void*)
  __ZN11UIInterface10KidArrivedE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # UIInterface::KidArrived(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN11UIInterface13CamcorderModeEv   # UIInterface::CamcorderMode()
  __ZN11UIInterface14StaffRoomEventE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtEi   # UIInterface::StaffRoomEvent(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, int)
  __ZN11UIInterface20EnableResearchButtonEb   # UIInterface::EnableResearchButton(bool)
  __ZN15errdeque_callee15QueueCdequeDtorEPvs   # errdeque_callee::QueueCdequeDtor(void*, short)
)
