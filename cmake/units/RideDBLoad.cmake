# Unit w119b: ride database loading, shape and text, custom effects and coaster setup.
# Every external call is a recording fake; the eleven functions call one another for real.
set(TPW_UNIT_RideDBLoad src/game/rides/RideDB.cpp src/game/rides/CoasterType.cpp src/game/rides/TPCoaster.cpp
    platform/msl.cpp)
set(TPW_UNIT_RideDBLoad_KEEP
  __Z15LoadCoasterTypejPcR15CCoasterBalance   # LoadCoasterType(unsigned int, char*, CCoasterBalance&)
  __ZN12CRideBalance21GetBalanceDescriptionEi   # CRideBalance::GetBalanceDescription(int)
  __ZN12CRideBalance21GetBalanceValuesStartEv   # CRideBalance::GetBalanceValuesStart()
  __ZN13CRideDatabase12ProcessShapeEPP8RideInfo   # CRideDatabase::ProcessShape(RideInfo**)
  __ZN13CRideDatabase13ScanDirectoryER19CTPItemListResource   # CRideDatabase::ScanDirectory(CTPItemListResource&)
  __ZN13CRideDatabase8LoadItemERK19CTPItemListResourceRN3bfl10FileSystem12CFileStorageER15CTPItemResource   # CRideDatabase::LoadItem(CTPItemListResource const&, bfl::FileSystem::CFileStorage&, CTPItemResource&)
  __ZN13CRideDatabase9LoadThemeEPKc   # CRideDatabase::LoadTheme(char const*)
  __ZN15CCoasterBalance21GetBalanceDescriptionEi   # CCoasterBalance::GetBalanceDescription(int)
  __ZN15CCoasterBalance21GetBalanceValuesStartEv   # CCoasterBalance::GetBalanceValuesStart()
  __ZN15CCoasterBalanceD1Ev   # CCoasterBalance::~CCoasterBalance()
  __ZN8RideInfo10LoadSoundsERN3bfl10FileSystem12CFileStorageE   # RideInfo::LoadSounds(bfl::FileSystem::CFileStorage&)
  __ZN8RideInfo11LoadStringsERN3bfl10FileSystem5CFileE   # RideInfo::LoadStrings(bfl::FileSystem::CFile&)
  __ZN8RideInfo13LoadParticlesERN3bfl10FileSystem12CFileStorageE   # RideInfo::LoadParticles(bfl::FileSystem::CFileStorage&)
  __ZN8RideInfo14DeallocStringsEv   # RideInfo::DeallocStrings()
  __ZN8RideInfo15SetIsCustomRideEv   # RideInfo::SetIsCustomRide()
  __ZN8RideInfoD1Ev   # RideInfo::~RideInfo()
  _gCoasterBalanceDescriptions
  _gRideBalanceDescriptions
  TRAINS_SWAP_FILE_WORDS@game/rides/TPCoaster.cpp   # the #define TRAINS_SWAP_FILE_WORDS of src/game/rides/TPCoaster.cpp (a guard macro the regroup brought into it)
)
set(TPW_UNIT_RideDBLoad_OWN_CALLLOG ON)
