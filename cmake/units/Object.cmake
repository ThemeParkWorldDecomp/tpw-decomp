# CObject::CObject and CObject::SetState (unit w126). External callees are recording fakes.
set(TPW_UNIT_Object src/Object.cpp)
# The unit file includes the allocation counters from calllog.cpp.
set(TPW_UNIT_Object_OWN_CALLLOG ON)
