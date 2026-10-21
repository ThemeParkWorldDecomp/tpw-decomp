# CObject::CObject and CObject::SetState (unit w126). External callees are recording fakes.
set(TPW_UNIT_Object src/game/Object.cpp)
set(TPW_UNIT_Object_KEEP
  __ZN7CObject8SetStateENS_11ObjectStateE   # CObject::SetState(CObject::ObjectState)
  __ZN7CObjectC1E12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtES0_I25IDXTYPEUniqueNameObjectIdtEii   # CObject::CObject(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>, int, int)
)
# The unit file includes the allocation counters from calllog.cpp.
set(TPW_UNIT_Object_OWN_CALLLOG ON)
