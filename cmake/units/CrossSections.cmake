# Unit w147: the coaster cross-section builders (BuildCrossSectionPoints and BuildCrossSectionEdges; src/game/rides/CoasterType.cpp). The
# allocator is a recording fake in tests/unit_CrossSections.cpp.
set(TPW_UNIT_CrossSections src/game/rides/CoasterType.cpp platform/arena.cpp)
set(TPW_UNIT_CrossSections_KEEP
  __Z22BuildCrossSectionEdgesP16tag_sCoasterTypeP14sCoasterRecordP15CCoasterBalance   # BuildCrossSectionEdges(tag_sCoasterType*, sCoasterRecord*, CCoasterBalance*)
  __Z23BuildCrossSectionPointsP16tag_sCoasterTypeP14sCoasterRecordP15CCoasterBalancePi   # BuildCrossSectionPoints(tag_sCoasterType*, sCoasterRecord*, CCoasterBalance*, int*)
)
