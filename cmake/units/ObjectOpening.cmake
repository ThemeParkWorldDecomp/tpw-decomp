# Unit w200: a ride opens, closes, runs and asks for an upgrade, and its messages (CObject::IsOpenable ... ReceiveMessage in src/game/Object.cpp, the ride
# messages in src/game/Object.cpp, src/game/ui/MessageCentre.cpp, CRideDatabase::GetRideInfo in src/game/rides/RideDB.cpp). The callees outside the unit are recording fakes in tests/unit_ObjectOpening.cpp.
set(TPW_UNIT_ObjectOpening src/game/Object.cpp src/game/ui/MessageCentre.cpp src/game/rides/RideDB.cpp
    platform/arena.cpp)
set(TPW_UNIT_ObjectOpening_KEEP
  __ZN11CMsgNewRideC1E12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CMsgNewRide::CMsgNewRide(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN11CMsgNewRideD1Ev   # CMsgNewRide::~CMsgNewRide()
  __ZN13CRideDatabase11GetRideInfoE12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE   # CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>)
  __ZN15CMsgBlockedExitC1E12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CMsgBlockedExit::CMsgBlockedExit(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN15CMsgBlockedExitD1Ev   # CMsgBlockedExit::~CMsgBlockedExit()
  __ZN15CMsgRideDeletedC1E12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CMsgRideDeleted::CMsgRideDeleted(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN15CMsgRideDeletedD1Ev   # CMsgRideDeleted::~CMsgRideDeleted()
  __ZN17CMsgRideCondemnedC1E12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CMsgRideCondemned::CMsgRideCondemned(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN17CMsgRideCondemnedD1Ev   # CMsgRideCondemned::~CMsgRideCondemned()
  __ZN18CMsgRideBrokenDownC1E12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CMsgRideBrokenDown::CMsgRideBrokenDown(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN18CMsgRideBrokenDownD1Ev   # CMsgRideBrokenDown::~CMsgRideBrokenDown()
  __ZN20CMsgRideWantsUpgradeC1E12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CMsgRideWantsUpgrade::CMsgRideWantsUpgrade(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN20CMsgRideWantsUpgradeD1Ev   # CMsgRideWantsUpgrade::~CMsgRideWantsUpgrade()
  __ZN7CObject10IsOpenableEv   # CObject::IsOpenable()
  __ZN7CObject11CloseObjectEv   # CObject::CloseObject()
  __ZN7CObject11IsCloseableEv   # CObject::IsCloseable()
  __ZN7CObject14ReceiveMessageERK4CMsg   # CObject::ReceiveMessage(CMsg const&)
  __ZN7CObject14RequestUpgradeEi   # CObject::RequestUpgrade(int)
  __ZN7CObject16SetUpgradeParentE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CObject::SetUpgradeParent(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN7CObject17NotifyExitBlockedEv   # CObject::NotifyExitBlocked()
  __ZN7CObject17SetOperatingSpeedEj   # CObject::SetOperatingSpeed(unsigned int)
  __ZN7CObject19IsAvailableToChooseEv   # CObject::IsAvailableToChoose()
  __ZN7CObject20SetOperatingCapacityEh   # CObject::SetOperatingCapacity(unsigned char)
  __ZN7CObject20SetOperatingDurationEh   # CObject::SetOperatingDuration(unsigned char)
  __ZN7CObject6ReopenEv   # CObject::Reopen()
  __ZNK11CMsgNewRide7GetTypeEv   # CMsgNewRide::GetType() const
  __ZNK15CMsgBlockedExit7GetTypeEv   # CMsgBlockedExit::GetType() const
  __ZNK15CMsgRideDeleted7GetTypeEv   # CMsgRideDeleted::GetType() const
  __ZNK17CMsgRideCondemned7GetTypeEv   # CMsgRideCondemned::GetType() const
  __ZNK18CMsgRideBrokenDown7GetTypeEv   # CMsgRideBrokenDown::GetType() const
  __ZNK20CMsgRideWantsUpgrade7GetTypeEv   # CMsgRideWantsUpgrade::GetType() const
  __ZNK7CObject16GetUpgradeParentEv   # CObject::GetUpgradeParent() const
  __ZNK7CObject9GetDBInfoEv   # CObject::GetDBInfo() const
  __ZNK7CObject9IsRunningEv   # CObject::IsRunning() const
)
