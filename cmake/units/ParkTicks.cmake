# The park loop's small ticks (unit w133): TickLevelLights, TickFlashObjects, TickRideHighlights, SetBaseBlendFlags,
# LLCC_DrawInitialHoarding, CIntrface::DoPointedThingComputations, FLY_SetFlyZone, CActionRecorder::DoTick and
# ResetActionRecording. Light.cpp is the light the sun's tick writes; the arena holds the meshes the test builds.
set(TPW_UNIT_ParkTicks src/LevelLights_tick.cpp src/Debug_overlays.cpp src/Debug_blendflags.cpp src/LLCellSystem_hoarding.cpp
    src/intrface_pointed.cpp src/hlsprite_zone.cpp src/ActionRecorder.cpp src/Light.cpp platform/arena.cpp
    src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp)
