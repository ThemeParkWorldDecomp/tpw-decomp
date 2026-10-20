# The world's construction (unit w113): CGame::InitWorld, the CWorld constructor, Allocate, the thing heap, the staff database and
# park time and arrival constructors, the cell constructors and grid registrations, ObjectControl's empty constructor and the save
# block table. CExitError::SetError is real (Allocate's error branch); StaffDB_dtor.cpp is linked only because the world's constructor
# names ~CStaffDB for the case that a member's constructor throws (the unit file fakes ~CStaffInfo).
set(TPW_UNIT_WorldInit src/World_init.cpp src/World_alloc.cpp src/Game_world.cpp src/StaffDB_ctor.cpp src/ParkTime.cpp src/Arrival.cpp
    src/CellInit.cpp src/BlockSave.cpp src/ExitErr.cpp src/StaffDB_dtor.cpp)
