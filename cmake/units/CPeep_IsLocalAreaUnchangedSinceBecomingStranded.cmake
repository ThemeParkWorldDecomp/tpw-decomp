# Unit v5048: CPeep::IsLocalAreaUnchangedSinceBecomingStranded, SpawnParticleEffect, ReportAllEvents (src/game/people/Peep.cpp). The callees
# outside the unit are recording fakes in tests/unit_CPeep_IsLocalAreaUnchangedSinceBecomingStranded.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPeep_IsLocalAreaUnchangedSinceBecomingStranded src/game/people/Peep.cpp platform/arena.cpp)
set(TPW_UNIT_CPeep_IsLocalAreaUnchangedSinceBecomingStranded_KEEP
  __ZN5CPeep19SpawnParticleEffectEi   # CPeep::SpawnParticleEffect(int)
  __ZN5CPeep41IsLocalAreaUnchangedSinceBecomingStrandedEv   # CPeep::IsLocalAreaUnchangedSinceBecomingStranded()
  __ZNK5CPeep15ReportAllEventsEv   # CPeep::ReportAllEvents() const
)
