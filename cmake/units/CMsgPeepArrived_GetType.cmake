# Unit v51064: twenty-four small functions (src/game/buildings/MechanicHQ.cpp, src/game/people/Peep.cpp, src/game/people/Person.cpp, src/game/people/PersonQueue.cpp, src/game/people/guard.cpp, src/game/rides/VehicleCallbacks.cpp, src/game/ui/MessageCentre.cpp).
# The callees outside the unit are recording fakes in tests/unit_CMsgPeepArrived_GetType.cpp, which also defines the globals of other files.
set(TPW_UNIT_CMsgPeepArrived_GetType src/game/buildings/MechanicHQ.cpp src/game/people/Peep.cpp
    src/game/people/Person.cpp src/game/people/PersonQueue.cpp src/game/people/guard.cpp
    src/game/rides/VehicleCallbacks.cpp src/game/ui/MessageCentre.cpp platform/arena.cpp)
set(TPW_UNIT_CMsgPeepArrived_GetType_KEEP
  __Z12Arrival_DtorP7Arrivals   # Arrival_Dtor(Arrival*, short)
  __Z14Behaviour_DtorP9Behaviours   # Behaviour_Dtor(Behaviour*, short)
  __Z17CMsgPrankery_DtorP12CMsgPrankerys   # CMsgPrankery_Dtor(CMsgPrankery*, short)
  __Z17__sinit_guard_cppv   # __sinit_guard_cpp()
  __Z18__sinit_Person_cppv   # __sinit_Person_cpp()
  __Z19BehaviourEntry_CtorP14BehaviourEntry   # BehaviourEntry_Ctor(BehaviourEntry*)
  __Z20CMsgPeepArrived_DtorP15CMsgPeepArriveds   # CMsgPeepArrived_Dtor(CMsgPeepArrived*, short)
  __Z22__sinit_MechanicHQ_cppv   # __sinit_MechanicHQ_cpp()
  __Z23__sinit_PersonQueue_cppv   # __sinit_PersonQueue_cpp()
  __Z28__sinit_VehicleCallbacks_cppv   # __sinit_VehicleCallbacks_cpp()
  __Z9Seek_DtorP4Seeks   # Seek_Dtor(Seek*, short)
  __ZN11CMechanicHQ12FinalDestroyEv   # CMechanicHQ::FinalDestroy()
  __ZN15CMsgPeepArrivedC1E12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CMsgPeepArrived::CMsgPeepArrived(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN5CPeep14SetStandScriptEv   # CPeep::SetStandScript()
  __ZN5CPeep15LastCellVisitedEv   # CPeep::LastCellVisited()
  __ZN7CPerson11EndSpotAnimEv   # CPerson::EndSpotAnim()
  __ZN7CPerson19ForceRideAdmittanceEv   # CPerson::ForceRideAdmittance()
  __ZN7CPerson22AddRideToRecentHistoryE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CPerson::AddRideToRecentHistory(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN7CPerson31AddRideToRecentTemporaryHistoryE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CPerson::AddRideToRecentTemporaryHistory(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZNK7CPerson15InQueueingStateEv   # CPerson::InQueueingState() const
  __ZNK7CPerson17ReadyForAdmittingEv   # CPerson::ReadyForAdmitting() const
  __ZNK7CPerson22GetPreferredExcitementEv   # CPerson::GetPreferredExcitement() const
  _gGuardSinitA
  _gGuardSinitB
  _gMechanicHQSinitA
  _gMechanicHQSinitB
  _gPersonQueueSinitA
  _gPersonQueueSinitB
  _gPersonSinitA
  _gPersonSinitB
  _gVehicleCallbacksSinitA
  _gVehicleCallbacksSinitB
)
