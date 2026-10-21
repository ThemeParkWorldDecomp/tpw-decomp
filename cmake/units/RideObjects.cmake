# Unit w150: the script objects of the ride scripts (emitters and sounds), the script stack and program reader, the small script getters and
# two ride position getters (src/game/rides/RideEffects.cpp, src/game/rides/rsse.cpp). The arena is the allocator behind the objects and the Ptr32 links of the script record. The
# callees outside the unit (the node position, the particle and sound calls, the ride database, the matrix helpers, the trace) are fakes in
# tests/unit_RideObjects.cpp, which also defines the globals of other files.
set(TPW_UNIT_RideObjects src/game/rides/RideEffects.cpp src/game/rides/rsse.cpp platform/arena.cpp)
set(TPW_UNIT_RideObjects_KEEP
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
  __Z3PopP11RSSE_SCRIPT   # Pop(RSSE_SCRIPT*)
  __Z4PushP11RSSE_SCRIPTi   # Push(RSSE_SCRIPT*, int)
)
