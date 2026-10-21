# The world's turn and its thing lists (unit w121): CWorld::DoGameTurn, DoOnlineGameTurn, UpdateThingsInEngine, UpdateBumperCarsInEngine,
# AddThingToThingList, _CThingArrayEntry::FreeMe, DeleteAllThings, DeletePendingThings, GetNextUsedThing, GetUsedThingHead, IsParkClosed and
# ParkGates_GetStatus. Every callee outside them (the thing virtuals, the people, the macro AI, the action recorder, the script variables, the
# pending list's RemoveHead, the game type constructor, the asserts and traces) is a fake in the unit file.
set(TPW_UNIT_WorldTurn src/game/World.cpp)
set(TPW_UNIT_WorldTurn_KEEP
  __ZN16CThingArrayEntry6FreeMeEv   # CThingArrayEntry::FreeMe()
  __ZN6CWorld10DoGameTurnEv   # CWorld::DoGameTurn()
  __ZN6CWorld15DeleteAllThingsEv   # CWorld::DeleteAllThings()
  __ZN6CWorld16DoOnlineGameTurnEv   # CWorld::DoOnlineGameTurn()
  __ZN6CWorld19AddThingToThingListEP6CThing   # CWorld::AddThingToThingList(CThing*)
  __ZN6CWorld19DeletePendingThingsEv   # CWorld::DeletePendingThings()
  __ZN6CWorld20UpdateThingsInEngineEf   # CWorld::UpdateThingsInEngine(float)
  __ZN6CWorld24UpdateBumperCarsInEngineEf   # CWorld::UpdateBumperCarsInEngine(float)
  __ZNK6CWorld12IsParkClosedEv   # CWorld::IsParkClosed() const
  __ZNK6CWorld16GetNextUsedThingEPK6CThing   # CWorld::GetNextUsedThing(CThing const*) const
  __ZNK6CWorld16GetUsedThingHeadEv   # CWorld::GetUsedThingHead() const
  __ZNK6CWorld19ParkGates_GetStatusEv   # CWorld::ParkGates_GetStatus() const
)
# The unit file includes calllog.cpp itself (it keeps the blocks the code frees until the next case).
set(TPW_UNIT_WorldTurn_OWN_CALLLOG ON)
