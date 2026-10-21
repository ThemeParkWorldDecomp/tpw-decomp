# Unit v5641: PlaceEndPylons and CoasterUVAnimTick (src/game/rides/TPCoaster.cpp). The callees outside the unit are recording fakes in
# tests/unit_PlaceEndPylons.cpp, which also defines gRideAnimSets.
set(TPW_UNIT_PlaceEndPylons src/game/rides/TPCoaster.cpp platform/arena.cpp)
set(TPW_UNIT_PlaceEndPylons_KEEP
  __Z14PlaceEndPylonsP20tag_sCoasterInstancef   # PlaceEndPylons(tag_sCoasterInstance*, float)
  __Z17CoasterUVAnimTickP20tag_sCoasterInstance   # CoasterUVAnimTick(tag_sCoasterInstance*)
  TRAINS_SWAP_FILE_WORDS@game/rides/TPCoaster.cpp   # the #define TRAINS_SWAP_FILE_WORDS of src/game/rides/TPCoaster.cpp (a guard macro the regroup brought into it)
)
