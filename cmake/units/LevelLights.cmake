# The park's lights and its second initialisation and teardown (unit w110): CreateGlobalLight with the CLight setters,
# LevelInit2 and LevelDeInit with the four CWorld height field accessors. Light.cpp is the light's constructor, which
# CreateGlobalLight runs; the arena holds the height field header the test builds.
set(TPW_UNIT_LevelLights src/LevelLights.cpp src/LevelInit2.cpp src/Light.cpp platform/arena.cpp)
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_LevelLights_OWN_CALLLOG ON)
