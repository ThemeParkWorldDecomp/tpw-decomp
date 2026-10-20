# The particle system's tick and drawing list and the effect manager (unit w26c). The state block is the real one; the callees in the
# spawning and core units are fakes in the unit file.
set(TPW_UNIT_ParticleTick src/rsse_tick.cpp src/effect_manager.cpp src/rsse_particles_state.cpp)
