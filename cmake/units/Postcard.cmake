set(TPW_UNIT_Postcard src/Postcard.cpp)
# The unit file includes calllog.cpp itself (it watches the heap: every array allocation and release is an event).
set(TPW_UNIT_Postcard_OWN_CALLLOG ON)
