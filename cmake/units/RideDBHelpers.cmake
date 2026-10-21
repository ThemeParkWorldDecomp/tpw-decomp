# Unit w119c: real string helpers, effect getters and coaster data loader.
# Balance Init/UnInit and external creation/allocators are recording fakes.
set(TPW_UNIT_RideDBHelpers src/game/rides/RideDB.cpp src/game/rides/CoasterType.cpp src/game/rides/TPCoaster.cpp)
set(TPW_UNIT_RideDBHelpers_KEEP
  __Z14find_string_idPKDs   # find_string_id(char16_t const*)
  __Z23LoadCoasterTypeDataFilejPcS_R15CCoasterBalance   # LoadCoasterTypeDataFile(unsigned int, char*, char*, CCoasterBalance&)
  __Z4compPKDsS0_   # comp(char16_t const*, char16_t const*)
  __Z9copy_linePPKDsS0_   # copy_line(char16_t const**, char16_t const*)
  __Z9find_linePKDs   # find_line(char16_t const*)
  __ZN15CCoasterBalance21GetBalanceDescriptionEi   # CCoasterBalance::GetBalanceDescription(int)
  __ZN15CCoasterBalance21GetBalanceValuesStartEv   # CCoasterBalance::GetBalanceValuesStart()
  __ZN15CCoasterBalanceD1Ev   # CCoasterBalance::~CCoasterBalance()
  __ZNK8RideInfo18GetCustomSoundBankEv   # RideInfo::GetCustomSoundBank() const
  __ZNK8RideInfo23GetCustomParticleEffectEi   # RideInfo::GetCustomParticleEffect(int) const
  _gCoasterBalanceDescriptions
  _gRideStringTags
  TRAINS_SWAP_FILE_WORDS@game/rides/TPCoaster.cpp   # the #define TRAINS_SWAP_FILE_WORDS of src/game/rides/TPCoaster.cpp (a guard macro the regroup brought into it)
)
