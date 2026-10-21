# Unit v5092: SMachineParameters::Output and ::Suggest (src/engine/tb_init.cpp). The callees outside the unit are
# recording fakes in tests/unit_SMachineParameters_Output.cpp, which also defines the globals of other files.
set(TPW_UNIT_SMachineParameters_Output src/engine/tb_init.cpp platform/arena.cpp)
set(TPW_UNIT_SMachineParameters_Output_KEEP
  __ZN18SMachineParameters6OutputEv   # SMachineParameters::Output()
  __ZN18SMachineParameters7SuggestEv   # SMachineParameters::Suggest()
  _gSMachineCfgData
  _gSMachineOutputCount
)
