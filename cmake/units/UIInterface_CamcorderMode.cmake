# Unit v51038: UIInterface::CamcorderMode, EnableResearchButton, StaffRoomEvent, KidArrived, ErrorDlgCancel, AddUIError and the block table's
# destructor (src/UI_pump_UIInterface_CamcorderMode.cpp). The callees outside the unit are recording fakes in
# tests/unit_UIInterface_CamcorderMode.cpp, which also defines the globals of other files.
set(TPW_UNIT_UIInterface_CamcorderMode src/UI_pump_UIInterface_CamcorderMode.cpp platform/arena.cpp)
