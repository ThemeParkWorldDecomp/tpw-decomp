# Unit w202: the ride's admittance and queue length (src/Object_admit.cpp). The callees outside the unit are fakes in tests/unit_ObjectAdmit.cpp (the assert, the
# trace, the ride script's variables and tour, the ride record lookup, the thing table's guests and the five CPerson members the ride calls, the queue count and
# removal, the coaster test, SetState, the hoardings, the first person camera and the two camera mesh finders); nothing else is linked.
set(TPW_UNIT_ObjectAdmit src/Object_admit.cpp)
