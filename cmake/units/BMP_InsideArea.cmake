# Unit v5050: BMP_InsideArea and BMP_OutsideArea (src/game/rides/BumperRide.cpp). PointInColObject is a recording fake in
# tests/unit_BMP_InsideArea.cpp, which also defines the sine table and the collision list.
set(TPW_UNIT_BMP_InsideArea src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_BMP_InsideArea_KEEP
  __Z14BMP_InsideAreaP7tagBUMP   # BMP_InsideArea(tagBUMP*)
  __Z15BMP_OutsideAreaP7tagBUMP   # BMP_OutsideArea(tagBUMP*)
)
