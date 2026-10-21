# The arrivals and the control manager (unit w176): CArrival::Serialise, CreatePeople, HowManyPeople, CreatePerson and __sinit_Arrival_cpp;
# CControlManager::Serialise, both GetObjectControl, CheckAllControlsInitialised and the ObjectControl file functions with the two word swaps.
# Every callee outside them (the world's arrival vehicle, the park analyser, the map's bus stop, CPerson's constructor, the game type, the file
# and the save log) is a fake in the unit file; gMainBalance, the ride database and the globals are storage defined there.
set(TPW_UNIT_ArrivalControls src/game/people/Arrival.cpp src/game/ControlManager.cpp src/game/ui/ui_visitorStatus.cpp)
set(TPW_UNIT_ArrivalControls_KEEP
  __Z11SwapForFileR13ObjectControl   # SwapForFile(ObjectControl&)
  __Z13LOADVARSTRUCTI13ObjectControlEiPvPT_PKci   # int LOADVARSTRUCT<ObjectControl>(void*, ObjectControl*, char const*, int)
  __Z13SAVEVARSTRUCTI13ObjectControlEiPvPT_PKci   # int SAVEVARSTRUCT<ObjectControl>(void*, ObjectControl*, char const*, int)
  __Z13UnswapForFileR13ObjectControl   # UnswapForFile(ObjectControl&)
  __ZN11ControlSwap18USwapInplace32LongERi   # ControlSwap::USwapInplace32Long(int&)
  __ZN11ControlSwap19USwapInplace32ULongERj   # ControlSwap::USwapInplace32ULong(unsigned int&)
  __ZN15CControlManager16GetObjectControlE12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtEi   # CControlManager::GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>, int)
  __ZN15CControlManager27CheckAllControlsInitialisedEv   # CControlManager::CheckAllControlsInitialised()
  __ZN15CControlManager9SerialiseEPviij   # CControlManager::Serialise(void*, int, int, unsigned int)
  __ZN8CArrival12CreatePeopleEv   # CArrival::CreatePeople()
  __ZN8CArrival12CreatePersonEv   # CArrival::CreatePerson()
  __ZN8CArrival13HowManyPeopleEv   # CArrival::HowManyPeople()
  __ZN8CArrival9SerialiseEPviij   # CArrival::Serialise(void*, int, int, unsigned int)
  __ZNK11CWorldSavvy8GetWorldEv   # CWorldSavvy::GetWorld() const
  __ZNK15CControlManager16GetObjectControlE12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtEi   # CControlManager::GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>, int) const
  _gArrivalFileWord1034C1A4
  _gArrivalFileWord1034C1A8
)
set(TPW_UNIT_ArrivalControls_OWN_CALLLOG ON)
