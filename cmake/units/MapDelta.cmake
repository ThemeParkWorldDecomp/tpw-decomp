# Unit w145: the map half of MapDelta.cpp, CMap's validity test, region effectors, position getters, time stamps, the engine refresh walks and the
# walking distance, the direction helpers and CMapCell's litter and thing list (src/MapDelta_map.cpp); src/MapDelta.cpp (w139) supplies the three
# compass offsets GetNeighbouringCell(uint8_t) reads. The callees outside the unit are recording fakes in tests/unit_MapDelta.cpp, which also
# includes calllog.cpp itself (the engine refresh walks log up to 32,768 events).
set(TPW_UNIT_MapDelta src/MapDelta_map.cpp src/MapDelta.cpp platform/arena.cpp)
set(TPW_UNIT_MapDelta_OWN_CALLLOG ON)
