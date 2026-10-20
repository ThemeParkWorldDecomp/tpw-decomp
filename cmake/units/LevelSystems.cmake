# The park's second-stage systems that LevelInit2 and LevelDeInit call (unit w111): the attribute map, the poly search table, the
# heightmap words, the level lights' teardown, the highlight buffers, the coaster types, the cell system's start and end, the
# script system, the dynamic cell meshes and the engine's PT_SetRange. Light.cpp is the lights' constructor and destructor
# (DestroyLevelLights deletes real lights).
set(TPW_UNIT_LevelSystems src/LevelSystems.cpp src/LLCellSystem.cpp src/AnalyserSystems.cpp src/tb_hfield_range.cpp src/Light.cpp)
