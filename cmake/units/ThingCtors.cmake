# The constructors of the things a new level makes (unit w122a): CThing::CThing and the constructors of CStaffHQ, CMechanicHQ, CAdvisor,
# CTagSystem, CResearchLab, CWeather and CUIMsgReceiver (the leaf part constructors are inline in the headers). Every callee outside them
# (the world's table, the map cell list, the message centre, the Init functions, the weather quality, the rain, the trace and operator
# new) is a fake in the unit file.
set(TPW_UNIT_ThingCtors src/game/Thing.cpp src/game/advisor/Advisor.cpp src/game/buildings/MechanicHQ.cpp
    src/game/buildings/ResearchLab.cpp src/game/buildings/StaffHQ.cpp src/game/park/Weather.cpp
    src/game/ui/vector2d.cpp)
set(TPW_UNIT_ThingCtors_KEEP
  __ZN10CTagSystemC1Ev   # CTagSystem::CTagSystem()
  __ZN11CMechanicHQC1Ev   # CMechanicHQ::CMechanicHQ()
  __ZN12CResearchLabC1Ev   # CResearchLab::CResearchLab()
  __ZN14CUIMsgReceiverC1Ev   # CUIMsgReceiver::CUIMsgReceiver()
  __ZN6CThingC1E12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtENS_10ThingModelE   # CThing::CThing(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, CThing::ThingModel)
  __ZN8CAdvisorC1Ev   # CAdvisor::CAdvisor()
  __ZN8CStaffHQC1Ev   # CStaffHQ::CStaffHQ()
  __ZN8CWeatherC1Ev   # CWeather::CWeather()
)
