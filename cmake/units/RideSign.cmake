# main and engine are isolated so each program's external callees stay fakes.
set(TPW_UNIT_RideSign src/RideSign.cpp platform/arena.cpp)
set(TPW_UNIT_RideSignMesh src/tb_mesh.cpp platform/arena.cpp)
set(TPW_UNIT_RideSignMesh_OWN_CALLLOG ON)
