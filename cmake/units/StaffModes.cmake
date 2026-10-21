# Unit w143: the park click and the staff mouse modes (src/Mouse_staff.cpp, src/ui_main_click.cpp, src/TPRide_pickup.cpp). The landed
# src/Mouse.cpp (AssignMouseMode, CMMDefault) runs for real; every other callee is a recording fake in tests/unit_StaffModes.cpp, which also
# defines the globals of other files.
set(TPW_UNIT_StaffModes src/Mouse_staff.cpp src/ui_main_click.cpp src/TPRide_pickup.cpp src/Mouse.cpp)
