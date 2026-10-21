# Unit v51050: CLightParallel::~CLightParallel and InterfaceImage::SetValue (src/game/advisor/Advisor.cpp, src/game/ui/InterfaceImage.cpp). The callees
# are recording fakes in tests/unit_CLightParallel_Dtor_CLightParallel.cpp.
set(TPW_UNIT_CLightParallel_Dtor_CLightParallel src/game/advisor/Advisor.cpp src/game/ui/InterfaceImage.cpp
    platform/arena.cpp)
set(TPW_UNIT_CLightParallel_Dtor_CLightParallel_KEEP
  __Z21CLightParallel_DeletePv   # CLightParallel_Delete(void*)
  __Z22CLightParallel_DtorMacP14CLightParallels   # CLightParallel_DtorMac(CLightParallel*, short)
  __ZN14InterfaceImage8SetValueEi   # InterfaceImage::SetValue(int)
  _gCLightParallelVt1
  _gCLightParallelVt2
  _gCLightParallelVt3
)
