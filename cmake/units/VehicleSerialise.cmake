# Unit w230: Vehicle save/load, four templates, and steering startup. Runtime and scalar callees are recording fakes.
set(TPW_UNIT_VehicleSerialise src/Vehicle_serialise.cpp platform/arena.cpp)
cmake_language(DEFER CALL target_compile_definitions tpw_unit_VehicleSerialise PRIVATE TPW_VEHICLE_UNIT_RUNTIME=1)
