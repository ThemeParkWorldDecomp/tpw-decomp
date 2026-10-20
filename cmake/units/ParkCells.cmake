# Unit w156: the park analyser's cell and map queries (src/ParkAnalyser_cells.cpp). The callees outside the unit are fakes in tests/unit_ParkCells.cpp
# (the assert, the world accessors, the used-thing walk, the analyser value's Get and Set, CountNeighboursOfType, the eleven CMapCell predicates,
# CStaff::HasPatrolArea and GetPatrolArea, CMap::GetRegionalEffectCell), which also defines gThingArray, gWorld and gWorldMap.
set(TPW_UNIT_ParkCells src/ParkAnalyser_cells.cpp platform/arena.cpp)
