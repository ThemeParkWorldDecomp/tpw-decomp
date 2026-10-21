# Unit w226: staff records and pool; all external effects are recording fakes.
set(TPW_UNIT_StaffDBPool src/game/people/StaffDB.cpp src/game/people/StaffInfo.cpp src/game/ControlManager.cpp
    src/game/Object.cpp src/game/advisor/Advisor.cpp src/game/park/BankAccount.cpp src/game/park/analyser.cpp
    src/game/rides/FlyingRide.cpp src/game/save/FS_MasterSave.cpp src/game/ui/ui_visitorStatus.cpp)
set(TPW_UNIT_StaffDBPool_KEEP
  __Z13LOADVARSTRUCTI11TbTimeStampEiPvPT_PKci   # int LOADVARSTRUCT<TbTimeStamp>(void*, TbTimeStamp*, char const*, int)
  __Z13LOADVARSTRUCTI12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEEiPvPT_PKci   # int LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>*, char const*, int)
  __Z13LOADVARSTRUCTI12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtEEiPvPT_PKci   # int LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>*, char const*, int)
  __Z13LOADVARSTRUCTI12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtEEiPvPT_PKci   # int LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>*, char const*, int)
  __Z13SAVEVARSTRUCTI11TbTimeStampEiPvPT_PKci   # int SAVEVARSTRUCT<TbTimeStamp>(void*, TbTimeStamp*, char const*, int)
  __Z13SAVEVARSTRUCTI12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEEiPvPT_PKci   # int SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>*, char const*, int)
  __Z13SAVEVARSTRUCTI12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtEEiPvPT_PKci   # int SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>*, char const*, int)
  __Z13SAVEVARSTRUCTI12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtEEiPvPT_PKci   # int SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>*, char const*, int)
  __Z7LOADVARI7MacLongEiPvPT_PKci   # int LOADVAR<MacLong>(void*, MacLong*, char const*, int)
  __Z7LOADVARI8MacULongEiPvPT_PKci   # int LOADVAR<MacULong>(void*, MacULong*, char const*, int)
  __Z7LOADVARIDsEiPvPT_PKci   # int LOADVAR<char16_t>(void*, char16_t*, char const*, int)
  __Z7LOADVARIbEiPvPT_PKci   # int LOADVAR<bool>(void*, bool*, char const*, int)
  __Z7LOADVARIcEiPvPT_PKci   # int LOADVAR<char>(void*, char*, char const*, int)
  __Z7LOADVARIhEiPvPT_PKci   # int LOADVAR<unsigned char>(void*, unsigned char*, char const*, int)
  __Z7LOADVARIiEiPvPT_PKci   # int LOADVAR<int>(void*, int*, char const*, int)
  __Z7LOADVARIjEiPvPT_PKci   # int LOADVAR<unsigned int>(void*, unsigned int*, char const*, int)
  __Z7LOADVARIsEiPvPT_PKci   # int LOADVAR<short>(void*, short*, char const*, int)
  __Z7LOADVARItEiPvPT_PKci   # int LOADVAR<unsigned short>(void*, unsigned short*, char const*, int)
  __Z7SAVEVARI7MacLongEiPvPT_PKci   # int SAVEVAR<MacLong>(void*, MacLong*, char const*, int)
  __Z7SAVEVARI8MacULongEiPvPT_PKci   # int SAVEVAR<MacULong>(void*, MacULong*, char const*, int)
  __Z7SAVEVARIDsEiPvPT_PKci   # int SAVEVAR<char16_t>(void*, char16_t*, char const*, int)
  __Z7SAVEVARIbEiPvPT_PKci   # int SAVEVAR<bool>(void*, bool*, char const*, int)
  __Z7SAVEVARIcEiPvPT_PKci   # int SAVEVAR<char>(void*, char*, char const*, int)
  __Z7SAVEVARIhEiPvPT_PKci   # int SAVEVAR<unsigned char>(void*, unsigned char*, char const*, int)
  __Z7SAVEVARIiEiPvPT_PKci   # int SAVEVAR<int>(void*, int*, char const*, int)
  __Z7SAVEVARIjEiPvPT_PKci   # int SAVEVAR<unsigned int>(void*, unsigned int*, char const*, int)
  __Z7SAVEVARIsEiPvPT_PKci   # int SAVEVAR<short>(void*, short*, char const*, int)
  __Z7SAVEVARItEiPvPT_PKci   # int SAVEVAR<unsigned short>(void*, unsigned short*, char const*, int)
  __ZN10CStaffInfo12GenerateNameEv   # CStaffInfo::GenerateName()
  __ZN10CStaffInfo8GenerateEii   # CStaffInfo::Generate(int, int)
  __ZN10CStaffInfo8SetValidEbi   # CStaffInfo::SetValid(bool, int)
  __ZN10CStaffInfo9SerialiseEPviij   # CStaffInfo::Serialise(void*, int, int, unsigned int)
  __ZN10CStaffInfoD1Ev   # CStaffInfo::~CStaffInfo()
  __ZN12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE8ByteswapEv   # tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>::Byteswap()
  __ZN12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE8ByteswapEv   # tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>::Byteswap()
  __ZN8CStaffDB10EnforceCapEv   # CStaffDB::EnforceCap()
  __ZN8CStaffDB11GetNumStaffEv   # CStaffDB::GetNumStaff()
  __ZN8CStaffDB13GenerateStaffEi   # CStaffDB::GenerateStaff(int)
  __ZN8CStaffDB15CountStaffTypesEv   # CStaffDB::CountStaffTypes()
  __ZN8CStaffDB16GetFirstFreeSlotEv   # CStaffDB::GetFirstFreeSlot()
  __ZN8CStaffDB17GetNumStaffOfTypeEi   # CStaffDB::GetNumStaffOfType(int)
  __ZN8CStaffDB19FateOfPickedUpStaffENS_9StaffFateE   # CStaffDB::FateOfPickedUpStaff(CStaffDB::StaffFate)
  __ZN8CStaffDB24GenerateOpeningStaffPoolEv   # CStaffDB::GenerateOpeningStaffPool()
  __ZN8CStaffDB25EnsureMinimumAmountInPoolEv   # CStaffDB::EnsureMinimumAmountInPool()
  __ZN8CStaffDB9HireStaffEj   # CStaffDB::HireStaff(unsigned int)
  __ZN8CStaffDB9SerialiseEPviij   # CStaffDB::Serialise(void*, int, int, unsigned int)
  __ZNK10CStaffInfo7GetNameEPDsi   # CStaffInfo::GetName(char16_t*, int) const
  __ZNK10CStaffInfo9GetSalaryEv   # CStaffInfo::GetSalary() const
  __ZNK11CWorldSavvy8GetWorldEv   # CWorldSavvy::GetWorld() const
  __ZNK8CStaffDB11IsNameInUseEPDs   # CStaffDB::IsNameInUse(char16_t*) const
  __ZNK8CStaffDB8GetStaffEj   # CStaffDB::GetStaff(unsigned int) const
  _gStaffDBWord6E0
  _gStaffDBWord6E4
  _gStaffGeneratedName
  _gStaffNameScratch
)
set(TPW_UNIT_StaffDBPool_OWN_CALLLOG ON)
