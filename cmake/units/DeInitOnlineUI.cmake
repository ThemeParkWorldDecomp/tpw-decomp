# Unit v5769: DeInitOnlineUI (src/game/ui/ui_main.cpp). The callees outside the unit are recording fakes in
# tests/unit_DeInitOnlineUI.cpp, which also defines the globals of other files.
set(TPW_UNIT_DeInitOnlineUI src/game/ui/ui_main.cpp src/game/Light.cpp platform/arena.cpp)
set(TPW_UNIT_DeInitOnlineUI_KEEP
  __Z14DeInitOnlineUIv   # DeInitOnlineUI()
  __ZN6CLight11GetLinkTypeEv   # CLight::GetLinkType()
  __ZN6CLight7GetDataEv   # CLight::GetData()
  __ZN6CLight7PrepareEv   # CLight::Prepare()
  __ZN6CLightC1Ev   # CLight::CLight()
  __ZN6CLightD1Ev   # CLight::~CLight()
)
