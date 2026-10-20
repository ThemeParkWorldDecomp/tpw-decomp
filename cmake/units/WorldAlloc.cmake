# CWorld::Allocate on its own (unit w113), with a fake AllocateThingHeap in the unit file so that the error branch (CExitError type 8)
# can be reached; the heap it builds is tested in the WorldInit library.
set(TPW_UNIT_WorldAlloc src/World_alloc.cpp src/BlockSave.cpp src/ExitErr.cpp)
