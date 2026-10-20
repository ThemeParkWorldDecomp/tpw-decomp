# TickRideHeads with the matrix functions it calls linked for real (the originals run on the PowerPC side too);
# BuildGeometryTables fills the sine table on the host side, the script setters are recorded.
set(TPW_UNIT_RideHeads src/TPEmbed_heads.cpp src/rsse_matrix.cpp src/RideAnimInterp.cpp src/rsse.cpp platform/arena.cpp)
