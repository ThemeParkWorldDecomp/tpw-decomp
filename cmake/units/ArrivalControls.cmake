# The arrivals and the control manager (unit w176): CArrival::Serialise, CreatePeople, HowManyPeople, CreatePerson and __sinit_Arrival_cpp;
# CControlManager::Serialise, both GetObjectControl, CheckAllControlsInitialised and the ObjectControl file functions with the two word swaps.
# Every callee outside them (the world's arrival vehicle, the park analyser, the map's bus stop, CPerson's constructor, the game type, the file
# and the save log) is a fake in the unit file; gMainBalance, the ride database and the globals are storage defined there.
set(TPW_UNIT_ArrivalControls src/Arrival_people.cpp src/ControlManager.cpp src/WorldSavvy.cpp)
set(TPW_UNIT_ArrivalControls_OWN_CALLLOG ON)
