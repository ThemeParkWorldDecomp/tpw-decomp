# Unit w190: CStaff part 2 (SackMe, TrainMe, GetEmploymentDuration, CanBePickedUp, PickMeUp, DropMe, BeingPointedAt, GetStaffType, GetGeneralStaffState,
# GetSkill, IsStriking, IsAllocatedToRide, IsWorkingOnAllocatedObject, IsRestingInStaffRoom, TooTiredToWork, SetName, GetName and both GetSalary;
# src/Staff_queries.cpp). Every callee outside them (the CPeep members, the staff member's SetState, the delayed script, the world's getters, the bank
# account, the park analyser, the two staff headquarters, the mechanic, the mouse and its default mode, the park time, TbTimeStamp::operator-, the trace
# and the assertion) is a recording fake in tests/unit_StaffQueries.cpp, which also defines the globals of other files and the balance table's storage.
set(TPW_UNIT_StaffQueries src/Staff_queries.cpp src/WorldSavvy.cpp platform/arena.cpp)
