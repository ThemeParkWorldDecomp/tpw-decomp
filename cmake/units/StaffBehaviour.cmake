# Unit w191: CStaff part 3 (ProcessWalkingToRestArea, ProcessResting, ProcessWalkingToStrike, ProcessStriking, ProcessBeingStunned, GetTiredPatrolling,
# GetTiredWorking, FindNearestRestingPlace, DoGeneralStaffThing, GoToRestArea, StopResting and the six patrol area members; src/game/people/staff.cpp). Every
# callee outside them (the CPeep members, the staff member's SetState and queries, the five ChooseNextAction, the thoughts, the delayed script, the world's
# getters, the staff headquarters, the park analyser, the map, the script variables, the message centre, the trace and the assertion) is a recording fake in
# tests/unit_StaffBehaviour.cpp, which also defines the globals of other files and the balance table's storage.
set(TPW_UNIT_StaffBehaviour src/game/people/staff.cpp platform/arena.cpp)
set(TPW_UNIT_StaffBehaviour_KEEP
  __ZN6CStaff11StopRestingEv   # CStaff::StopResting()
  __ZN6CStaff12GoToRestAreaEv   # CStaff::GoToRestArea()
  __ZN6CStaff13SetPatrolAreaE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtES2_   # CStaff::SetPatrolArea(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN6CStaff14ProcessRestingEv   # CStaff::ProcessResting()
  __ZN6CStaff15GetTiredWorkingEv   # CStaff::GetTiredWorking()
  __ZN6CStaff15ProcessStrikingEv   # CStaff::ProcessStriking()
  __ZN6CStaff16CancelPatrolAreaEv   # CStaff::CancelPatrolArea()
  __ZN6CStaff18GetTiredPatrollingEv   # CStaff::GetTiredPatrolling()
  __ZN6CStaff19DoGeneralStaffThingEv   # CStaff::DoGeneralStaffThing()
  __ZN6CStaff19ProcessBeingStunnedEv   # CStaff::ProcessBeingStunned()
  __ZN6CStaff20FindDestInPatrolAreaEv   # CStaff::FindDestInPatrolArea()
  __ZN6CStaff22ProcessWalkingToStrikeEv   # CStaff::ProcessWalkingToStrike()
  __ZN6CStaff23FindNearestRestingPlaceEv   # CStaff::FindNearestRestingPlace()
  __ZN6CStaff24ProcessWalkingToRestAreaEv   # CStaff::ProcessWalkingToRestArea()
  __ZNK6CStaff13GetPatrolAreaEP12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtES3_   # CStaff::GetPatrolArea(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>*, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>*) const
  __ZNK6CStaff13HasPatrolAreaEv   # CStaff::HasPatrolArea() const
  __ZNK6CStaff16InsidePatrolAreaE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CStaff::InsidePatrolArea(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>) const
)
