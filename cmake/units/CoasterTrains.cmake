# Unit w192: the coaster trains (src/game/rides/BumperRide.cpp, src/game/rides/TPCoaster.cpp). The callees outside the unit are recording fakes in tests/unit_CoasterTrains.cpp.
set(TPW_UNIT_CoasterTrains src/game/rides/BumperRide.cpp src/game/rides/TPCoaster.cpp platform/arena.cpp)
set(TPW_UNIT_CoasterTrains_KEEP
  __Z17LoadTrainsAndCarsPvP20tag_sCoasterInstance   # LoadTrainsAndCars(void*, tag_sCoasterInstance*)
  __Z17SaveTrainsAndCarsPvP20tag_sCoasterInstance   # SaveTrainsAndCars(void*, tag_sCoasterInstance*)
  __Z20DestroyCoasterTrainsP20tag_sCoasterInstance   # DestroyCoasterTrains(tag_sCoasterInstance*)
  __Z21InstanceCoasterTrainsP16tag_sCoasterTypeP20tag_sCoasterInstancei   # InstanceCoasterTrains(tag_sCoasterType*, tag_sCoasterInstance*, int)
  __Z22GetCoaster1stPersonCarj   # GetCoaster1stPersonCar(unsigned int)
  __ZN13TPCoasterSwap17USwapInplace32IntERi   # TPCoasterSwap::USwapInplace32Int(int&)
  __ZN13TPCoasterSwap17USwapInplaceFloatERf   # TPCoasterSwap::USwapInplaceFloat(float&)
  __ZN13TPCoasterSwap19USwapInplace32ULongERj   # TPCoasterSwap::USwapInplace32ULong(unsigned int&)
  __ZN13TPCoasterSwap20USwapInplace16UShortERt   # TPCoasterSwap::USwapInplace16UShort(unsigned short&)
  TRAINS_SWAP_FILE_WORDS@game/rides/TPCoaster.cpp   # the #define TRAINS_SWAP_FILE_WORDS of src/game/rides/TPCoaster.cpp (a guard macro the regroup brought into it)
)
