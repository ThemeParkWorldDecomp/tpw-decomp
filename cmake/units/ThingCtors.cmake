# The constructors of the things a new level makes (unit w122a): CThing::CThing and the constructors of CStaffHQ, CMechanicHQ, CAdvisor,
# CTagSystem, CResearchLab, CWeather and CUIMsgReceiver (the leaf part constructors are inline in the headers). Every callee outside them
# (the world's table, the map cell list, the message centre, the Init functions, the weather quality, the rain, the trace and operator
# new) is a fake in the unit file.
set(TPW_UNIT_ThingCtors src/ThingCtors.cpp)
