# Unit w189: CStaff part 1 (both constructors, Init, FinalDestroy, ReceiveMessage, Serialise, UpdateTurn, SetState, ModelState, UpdateMonthly, the four
# PayGrade / StaffState serialise instances, CMsgStaffRoomEvent's destructor and GetType, __sinit_staff_cpp; src/Staff.cpp). Every callee outside them (the
# CPeep members, the thoughts, the delayed script, the other CStaff members, the world, the bank account, the park analyser, the message centre, the mouse,
# the other serialise instances, the trace and the assertion) is a recording fake in tests/unit_Staff.cpp, which also defines the globals of other files.
set(TPW_UNIT_Staff src/Staff.cpp platform/arena.cpp)
