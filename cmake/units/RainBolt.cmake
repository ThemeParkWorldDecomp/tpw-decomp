# w67: the lightning midpoint, subdivision and textured quad builder.
set(TPW_UNIT_RainBolt src/tb_rain_lightning.cpp)
# A realistic bolt needs a 2,000-word Random queue.
set(TPW_UNIT_RainBolt_OWN_CALLLOG ON)
# Only the test library counts entries to the real recursive function.
cmake_language(DEFER CALL target_compile_options tpw_unit_RainBolt PRIVATE -finstrument-functions)
