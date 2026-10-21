# Unit w172: a ride script's ride-specific commands (src/game/rides/rsse.cpp) and the script program reader they call (src/game/rides/RideEffects.cpp, src/game/rides/rsse.cpp: ReadScript,
# ExtractData). The ride callees (FLY_, BMP_, Coaster, LLCC_, GetCoasterInstanceID, FindDataNodeIndexByID) are fakes in tests/unit_RideCommands.cpp, which also
# defines the globals of other files. The arena is the allocator behind the script record and its Ptr32 links.
set(TPW_UNIT_RideCommands src/game/rides/rsse.cpp src/game/rides/RideEffects.cpp platform/arena.cpp)
set(TPW_UNIT_RideCommands_KEEP
  __Z10InitObjectP11RSSE_SCRIPTP11RSSE_OBJECTiii   # InitObject(RSSE_SCRIPT*, RSSE_OBJECT*, int, int, int)
  __Z10KillObjectP11RSSE_SCRIPTP11RSSE_OBJECT   # KillObject(RSSE_SCRIPT*, RSSE_OBJECT*)
  __Z10ReadScriptP11RSSE_SCRIPT   # ReadScript(RSSE_SCRIPT*)
  __Z10StartEventP11RSSE_SCRIPTiiii   # StartEvent(RSSE_SCRIPT*, int, int, int, int)
  __Z11ExtractDataP11RSSE_SCRIPTi   # ExtractData(RSSE_SCRIPT*, int)
  __Z11RSSE_SetvarP11RSSE_SCRIPTii   # RSSE_Setvar(RSSE_SCRIPT*, int, int)
  __Z11SetObjParamP11RSSE_SCRIPTP11RSSE_OBJECTii   # SetObjParam(RSSE_SCRIPT*, RSSE_OBJECT*, int, int)
  __Z12CreateObjectP11RSSE_SCRIPT   # CreateObject(RSSE_SCRIPT*)
  __Z14ProcessObjectsP11RSSE_SCRIPT   # ProcessObjects(RSSE_SCRIPT*)
  __Z15GetRidePositionP16tag_sRideAnimSetRfS1_S1_   # GetRidePosition(tag_sRideAnimSet*, float&, float&, float&)
  __Z16KillLinkedObjectP11RSSE_SCRIPTP11RSSE_OBJECTi   # KillLinkedObject(RSSE_SCRIPT*, RSSE_OBJECT*, int)
  __Z17RSSE_GetRideSpeedP11RSSE_SCRIPT   # RSSE_GetRideSpeed(RSSE_SCRIPT*)
  __Z17RSSE_SetRideSpeedP11RSSE_SCRIPTi   # RSSE_SetRideSpeed(RSSE_SCRIPT*, int)
  __Z18RSSE_GetTourBaseIDP11RSSE_SCRIPT   # RSSE_GetTourBaseID(RSSE_SCRIPT*)
  __Z21GetRideCentrePositionRjRfS0_S0_   # GetRideCentrePosition(unsigned int&, float&, float&, float&)
  __Z26ExecuteTourSpecificCommandP11RSSE_SCRIPT   # ExecuteTourSpecificCommand(RSSE_SCRIPT*)
  __Z28ExecuteBumperSpecificCommandP11RSSE_SCRIPT   # ExecuteBumperSpecificCommand(RSSE_SCRIPT*)
  __Z29ExecuteCoasterSpecificCommandP11RSSE_SCRIPT   # ExecuteCoasterSpecificCommand(RSSE_SCRIPT*)
  __Z3PopP11RSSE_SCRIPT   # Pop(RSSE_SCRIPT*)
  __Z4PushP11RSSE_SCRIPTi   # Push(RSSE_SCRIPT*, int)
  _gRsseWord102F7C00
  _gRsseWord102F7C04
)
