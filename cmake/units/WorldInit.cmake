# The world's construction (unit w113): CGame::InitWorld, the CWorld constructor, Allocate, the thing heap, the staff database and
# park time and arrival constructors, the cell constructors and grid registrations, ObjectControl's empty constructor and the save
# block table. CExitError::SetError is real (Allocate's error branch); StaffDB_dtor.cpp is linked only because the world's constructor
# names ~CStaffDB for the case that a member's constructor throws (the unit file fakes ~CStaffInfo).
set(TPW_UNIT_WorldInit src/game/World.cpp src/game/Game.cpp src/game/people/StaffDB.cpp src/game/people/StaffInfo.cpp
    src/game/people/ParkTime.cpp src/game/people/Arrival.cpp src/game/buildings/TrainingCentre.cpp
    src/game/park/CellBase.cpp src/game/park/MapDelta.cpp src/game/people/Researcher.cpp src/game/save/BlockSave.cpp
    src/game/ExitErr.cpp)
set(TPW_UNIT_WorldInit_KEEP
  __ZN10CBlockSave17RegisterSaveBlockENS_9SaveBlockEPviPKc   # CBlockSave::RegisterSaveBlock(CBlockSave::SaveBlock, void*, int, char const*)
  __ZN10CExitError8SetErrorENS_9ErrorTypeEjPKc   # CExitError::SetError(CExitError::ErrorType, unsigned int, char const*)
  __ZN10CStaffInfoC1Ev   # CStaffInfo::CStaffInfo()
  __ZN10CTrackCellC1Ev   # CTrackCell::CTrackCell()
  __ZN13ObjectControlC1Ev   # ObjectControl::ObjectControl()
  __ZN17CRegionEffectCellC1Ev   # CRegionEffectCell::CRegionEffectCell()
  __ZN5CGame9InitWorldEv   # CGame::InitWorld()
  __ZN6CWorld13InitThingHeapEv   # CWorld::InitThingHeap()
  __ZN6CWorld17AllocateThingHeapEv   # CWorld::AllocateThingHeap()
  __ZN6CWorld8AllocateEv   # CWorld::Allocate()
  __ZN6CWorldC1Ev   # CWorld::CWorld()
  __ZN8CArrivalC1Ev   # CArrival::CArrival()
  __ZN8CMapCellC1Ev   # CMapCell::CMapCell()
  __ZN8CStaffDBC1Ev   # CStaffDB::CStaffDB()
  __ZN8CStaffDBD1Ev   # CStaffDB::~CStaffDB()
  __ZN9CCellBase15RegisterMapGridEPK8CMapCelli   # CCellBase::RegisterMapGrid(CMapCell const*, int)
  __ZN9CCellBase17RegisterTrackGridEPK10CTrackCelli   # CCellBase::RegisterTrackGrid(CTrackCell const*, int)
  __ZN9CParkTime5ResetEv   # CParkTime::Reset()
  __ZN9CParkTimeC1Ev   # CParkTime::CParkTime()
  _gExitError
)
