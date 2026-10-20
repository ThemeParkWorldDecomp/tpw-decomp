# The world's turn and its thing lists (unit w121): CWorld::DoGameTurn, DoOnlineGameTurn, UpdateThingsInEngine, UpdateBumperCarsInEngine,
# AddThingToThingList, _CThingArrayEntry::FreeMe, DeleteAllThings, DeletePendingThings, GetNextUsedThing, GetUsedThingHead, IsParkClosed and
# ParkGates_GetStatus. Every callee outside them (the thing virtuals, the people, the macro AI, the action recorder, the script variables, the
# pending list's RemoveHead, the game type constructor, the asserts and traces) is a fake in the unit file.
set(TPW_UNIT_WorldTurn src/World_turn.cpp)
# The unit file includes calllog.cpp itself (it keeps the blocks the code frees until the next case).
set(TPW_UNIT_WorldTurn_OWN_CALLLOG ON)
