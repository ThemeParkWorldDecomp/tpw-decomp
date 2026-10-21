# Unit w199: a ride's wear, breakdowns, maintenance, repair and cleaning (the 18 CObject members of src/game/Object.cpp). src/game/Object.cpp supplies
# IsBrokenDown; every other callee (the ride database, IsRunning, IsCoaster, IsCoasterUseable, IsBOQConnected, GetFrontOfPhysicalQueue, SetState, the script
# variables, the sprite spawn, the hoardings, the world's advisor and bank, the map's regional effectors, the staff queries, the advisor request and its queue,
# the game type's constructor, the trace and the assertion) is a recording fake in tests/unit_ObjectUpkeep.cpp, which also defines the globals of other files.
set(TPW_UNIT_ObjectUpkeep src/game/Object.cpp platform/arena.cpp)
set(TPW_UNIT_ObjectUpkeep_KEEP
  __ZN7CObject10CleanFullyEv   # CObject::CleanFully()
  __ZN7CObject11RepairFullyE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CObject::RepairFully(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN7CObject11SetWornFlagEv   # CObject::SetWornFlag()
  __ZN7CObject11StartFixingEv   # CObject::StartFixing()
  __ZN7CObject14AddWearAndTearEv   # CObject::AddWearAndTear()
  __ZN7CObject16IsPlayingFixAnimEv   # CObject::IsPlayingFixAnim()
  __ZN7CObject16RequestBreakdownEv   # CObject::RequestBreakdown()
  __ZN7CObject17AssignStaffMemberE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CObject::AssignStaffMember(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN7CObject17IsBeingWorkedUponEv   # CObject::IsBeingWorkedUpon()
  __ZN7CObject18RequestMaintenanceEi   # CObject::RequestMaintenance(int)
  __ZN7CObject19UnassignStaffMemberEv   # CObject::UnassignStaffMember()
  __ZN7CObject22GetAssignedStaffMemberEv   # CObject::GetAssignedStaffMember()
  __ZNK7CObject11IsCondemnedEv   # CObject::IsCondemned() const
  __ZNK7CObject12IsBrokenDownEv   # CObject::IsBrokenDown() const
  __ZNK7CObject15RequiresUpgradeEv   # CObject::RequiresUpgrade() const
  __ZNK7CObject23IsFlaggedForMaintenanceEv   # CObject::IsFlaggedForMaintenance() const
  __ZNK7CObject25GetProspectiveReliabilityEfff   # CObject::GetProspectiveReliability(float, float, float) const
  __ZNK7CObject27GetProspectiveDamagePerTurnEfffb   # CObject::GetProspectiveDamagePerTurn(float, float, float, bool) const
  __ZNK7CObject7IsDirtyEv   # CObject::IsDirty() const
  __ZNK7CObject8IsClosedEv   # CObject::IsClosed() const
)
