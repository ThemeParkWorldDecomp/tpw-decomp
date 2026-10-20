# Unit w155: the park analyser's staff queries (src/ParkAnalyser_staff.cpp; 13 functions). Every callee outside the unit (the world accessor and its
# used-thing walk, the assertion, CStaff::TooTiredToWork and HasPatrolArea, CStaffDB::GetNumStaffOfType, CParkTime::GetFunnyTime and
# TbTimeStamp::operator-) is a recording fake in tests/unit_ParkStaff.cpp; gMainBalance is the real storage.
set(TPW_UNIT_ParkStaff src/ParkAnalyser_staff.cpp src/MainBalance.cpp platform/arena.cpp)
