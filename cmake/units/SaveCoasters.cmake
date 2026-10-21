# Unit v5968: SaveCoasters (src/game/rides/TPCoaster.cpp). The callees outside the unit are recording fakes in
# tests/unit_SaveCoasters.cpp, which also defines the globals of other files.
set(TPW_UNIT_SaveCoasters src/game/rides/TPCoaster.cpp platform/arena.cpp)
set(TPW_UNIT_SaveCoasters_KEEP
  __Z12SaveCoastersPv   # SaveCoasters(void*)
  TRAINS_SWAP_FILE_WORDS@game/rides/TPCoaster.cpp   # the #define TRAINS_SWAP_FILE_WORDS of src/game/rides/TPCoaster.cpp (a guard macro the regroup brought into it)
)
