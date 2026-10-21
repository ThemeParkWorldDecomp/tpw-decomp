# Unit v5127: LoadFileIntoMemory, LoadRideHeightMap, GetRideHeightMap (src/game/park/TPHField.cpp). The callees outside the unit are
# recording fakes in tests/unit_LoadFileIntoMemory.cpp, which also defines the globals of other files.
set(TPW_UNIT_LoadFileIntoMemory src/game/park/TPHField.cpp platform/arena.cpp)
set(TPW_UNIT_LoadFileIntoMemory_KEEP
  __Z16GetRideHeightMapP16tag_sRideAnimSetjjPcPvi   # GetRideHeightMap(tag_sRideAnimSet*, unsigned int, unsigned int, char*, void*, int)
  __Z17LoadRideHeightMapP16tag_sRideAnimSetjjPc   # LoadRideHeightMap(tag_sRideAnimSet*, unsigned int, unsigned int, char*)
  __Z18LoadFileIntoMemoryPci   # LoadFileIntoMemory(char*, int)
)
