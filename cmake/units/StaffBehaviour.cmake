# Unit w191: CStaff part 3 (ProcessWalkingToRestArea, ProcessResting, ProcessWalkingToStrike, ProcessStriking, ProcessBeingStunned, GetTiredPatrolling,
# GetTiredWorking, FindNearestRestingPlace, DoGeneralStaffThing, GoToRestArea, StopResting and the six patrol area members; src/Staff_behaviour.cpp). Every
# callee outside them (the CPeep members, the staff member's SetState and queries, the five ChooseNextAction, the thoughts, the delayed script, the world's
# getters, the staff headquarters, the park analyser, the map, the script variables, the message centre, the trace and the assertion) is a recording fake in
# tests/unit_StaffBehaviour.cpp, which also defines the globals of other files and the balance table's storage.
set(TPW_UNIT_StaffBehaviour src/Staff_behaviour.cpp platform/arena.cpp)
