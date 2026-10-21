# Unit v5556: CPeep::CPeep (src/game/people/Peep.cpp). The callees are recording fakes in
# tests/unit_CPeep_CPeep.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPeep_CPeep src/game/people/Peep.cpp platform/arena.cpp)
set(TPW_UNIT_CPeep_CPeep_KEEP
  __ZN5CPeepC1E12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEN6CThing10ThingModelE   # CPeep::CPeep(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, CThing::ThingModel)
  ___construct_array
)
