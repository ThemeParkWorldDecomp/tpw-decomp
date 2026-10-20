# Unit v5091: NewPylon and UpdatePylonStackMeshes (src/TPCoaster_loaders_NewPylon.cpp). The callees outside the unit are recording fakes in
# tests/unit_NewPylon.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_NewPylon src/TPCoaster_loaders_NewPylon.cpp platform/arena.cpp)
