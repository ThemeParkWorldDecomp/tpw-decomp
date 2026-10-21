# Unit w190: CStaff part 2 (SackMe, TrainMe, GetEmploymentDuration, CanBePickedUp, PickMeUp, DropMe, BeingPointedAt, GetStaffType, GetGeneralStaffState,
# GetSkill, IsStriking, IsAllocatedToRide, IsWorkingOnAllocatedObject, IsRestingInStaffRoom, TooTiredToWork, SetName, GetName and both GetSalary;
# src/game/people/staff.cpp). Every callee outside them (the CPeep members, the staff member's SetState, the delayed script, the world's getters, the bank
# account, the park analyser, the two staff headquarters, the mechanic, the mouse and its default mode, the park time, TbTimeStamp::operator-, the trace
# and the assertion) is a recording fake in tests/unit_StaffQueries.cpp, which also defines the globals of other files and the balance table's storage.
set(TPW_UNIT_StaffQueries src/game/people/staff.cpp src/game/ui/ui_visitorStatus.cpp platform/arena.cpp)
set(TPW_UNIT_StaffQueries_KEEP
  __ZN6CStaff10IsStrikingEv   # CStaff::IsStriking()
  __ZN6CStaff12GetStaffTypeEv   # CStaff::GetStaffType()
  __ZN6CStaff13CanBePickedUpEv   # CStaff::CanBePickedUp()
  __ZN6CStaff14BeingPointedAtEv   # CStaff::BeingPointedAt()
  __ZN6CStaff17IsAllocatedToRideE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CStaff::IsAllocatedToRide(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN6CStaff26IsWorkingOnAllocatedObjectE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CStaff::IsWorkingOnAllocatedObject(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN6CStaff6DropMeE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CStaff::DropMe(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN6CStaff6SackMeEv   # CStaff::SackMe()
  __ZN6CStaff7SetNameEPKDs   # CStaff::SetName(char16_t const*)
  __ZN6CStaff7TrainMeEi   # CStaff::TrainMe(int)
  __ZN6CStaff8PickMeUpEv   # CStaff::PickMeUp()
  __ZN6CStaff9GetSalaryE9StaffTypeh   # CStaff::GetSalary(StaffType, unsigned char)
  __ZN6CStaff9GetSalaryEv   # CStaff::GetSalary()
  __ZNK11CWorldSavvy8GetWorldEv   # CWorldSavvy::GetWorld() const
  __ZNK6CStaff14TooTiredToWorkEv   # CStaff::TooTiredToWork() const
  __ZNK6CStaff20GetGeneralStaffStateEv   # CStaff::GetGeneralStaffState() const
  __ZNK6CStaff20IsRestingInStaffRoomE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CStaff::IsRestingInStaffRoom(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>) const
  __ZNK6CStaff21GetEmploymentDurationEv   # CStaff::GetEmploymentDuration() const
  __ZNK6CStaff7GetNameEv   # CStaff::GetName() const
  __ZNK6CStaff8GetSkillEv   # CStaff::GetSkill() const
)
