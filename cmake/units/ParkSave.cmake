# Unit w158: the park analyser's value and history save templates (CParkAnalyserVal<int/float>::Serialise, CHistory<T,N>::Serialise for seven
# instances and CHistory<ObjectId,3>::SerialiseStruct; src/game/park/analyser.cpp). LOADVAR, SAVEVAR and the STRUCT pair are recording fakes in
# tests/unit_ParkSave.cpp. List only the sources the library must compile.
set(TPW_UNIT_ParkSave src/game/park/analyser.cpp platform/arena.cpp)
set(TPW_UNIT_ParkSave_KEEP
  __ZN16CParkAnalyserValIfE9SerialiseEPviij   # template int32_t CParkAnalyserVal<float>::Serialise(void*, int32_t, int32_t, uint32_t) (explicit instantiation)
  __ZN16CParkAnalyserValIiE9SerialiseEPviij   # template int32_t CParkAnalyserVal<int32_t>::Serialise(void*, int32_t, int32_t, uint32_t) (explicit instantiation)
  __ZN8CHistoryI12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtELi3EE15SerialiseStructEPviij   # template int32_t CHistory<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>, 3>::SerialiseStruct(void*, int32_t, int32_t, uint32_t) (explicit instantiation)
  __ZN8CHistoryIfLi144EE9SerialiseEPviij   # template int32_t CHistory<float, 144>::Serialise(void*, int32_t, int32_t, uint32_t) (explicit instantiation)
  __ZN8CHistoryIhLi50EE9SerialiseEPviij   # template int32_t CHistory<uint8_t, 50>::Serialise(void*, int32_t, int32_t, uint32_t) (explicit instantiation)
  __ZN8CHistoryIiLi144EE9SerialiseEPviij   # template int32_t CHistory<int32_t, 144>::Serialise(void*, int32_t, int32_t, uint32_t) (explicit instantiation)
  __ZN8CHistoryIiLi30EE9SerialiseEPviij   # template int32_t CHistory<int32_t, 30>::Serialise(void*, int32_t, int32_t, uint32_t) (explicit instantiation)
  __ZN8CHistoryIiLi50EE9SerialiseEPviij   # template int32_t CHistory<int32_t, 50>::Serialise(void*, int32_t, int32_t, uint32_t) (explicit instantiation)
  __ZN8CHistoryIiLi5EE9SerialiseEPviij   # template int32_t CHistory<int32_t, 5>::Serialise(void*, int32_t, int32_t, uint32_t) (explicit instantiation)
  __ZN8CHistoryIjLi30EE9SerialiseEPviij   # template int32_t CHistory<uint32_t, 30>::Serialise(void*, int32_t, int32_t, uint32_t) (explicit instantiation)
  tmpl_03893270e1c3@game/park/analyser.cpp   # Serialise (a template definition src/ParkAnalyser_save.cpp had before the regroup)
  tmpl_1904c8c3cb66@game/park/analyser.cpp   # Serialise (a template definition src/ParkAnalyser_save.cpp had before the regroup)
  tmpl_5d29b28d7f7f@game/park/analyser.cpp   # SerialiseStruct (a template definition src/ParkAnalyser_save.cpp had before the regroup)
)
set(TPW_UNIT_ParkSave_OWN_CALLLOG ON)
