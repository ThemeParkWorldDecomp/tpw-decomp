# Unit v5529: three states of the person (ProcessStateWalksToLitterBin, ProcessStateStandingInQueue, ProcessStateWaitingForScriptToAdmitMe;
# src/CPerson_..._CPerson_ProcessStateWalksToLitterBin.cpp). The callees outside the unit are recording fakes in
# tests/unit_CPerson_ProcessStateWalksToLitterBin.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPerson_ProcessStateWalksToLitterBin src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_CPerson_ProcessStateWalksToLitterBin.cpp platform/arena.cpp)
