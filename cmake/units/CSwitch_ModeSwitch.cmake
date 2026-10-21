# Unit v51066: CSwitch::ModeSwitch and friends (src/engine/DirectDraw.cpp, src/engine/DrawPrimitiveOpenGL.cpp, src/engine/EngineUtil.cpp, src/engine/Switch.cpp, src/engine/tb_hfield.cpp, src/engine/tb_init.cpp). The callees outside the unit are
# recording fakes in tests/unit_CSwitch_ModeSwitch.cpp, which also defines the globals of other files.
set(TPW_UNIT_CSwitch_ModeSwitch src/engine/DirectDraw.cpp src/engine/DrawPrimitiveOpenGL.cpp src/engine/EngineUtil.cpp
    src/engine/Switch.cpp src/engine/tb_hfield.cpp src/engine/tb_init.cpp platform/arena.cpp)
set(TPW_UNIT_CSwitch_ModeSwitch_KEEP
  __Z10PT_Disablev   # PT_Disable()
  __Z16IsAlignedTo64BitRPv   # IsAlignedTo64Bit(void*&)
  __Z16ToggleFullScreenv   # ToggleFullScreen()
  __Z19__sinit_tb_init_cppv   # __sinit_tb_init_cpp()
  __Z21COpenGLPrintf_destroyP13COpenGLPrintfs   # COpenGLPrintf_destroy(COpenGLPrintf*, short)
  __Z21__sinit_tb_hfield_cppv   # __sinit_tb_hfield_cpp()
  __Z9PT_Enablev   # PT_Enable()
  __ZN13SamsUtilities27USwapInplace32UInt_100061d0ERj   # SamsUtilities::USwapInplace32UInt_100061d0(unsigned int&)
  __ZN13SamsUtilities29USwapInplace16UShort_10006218ERt   # SamsUtilities::USwapInplace16UShort_10006218(unsigned short&)
  __ZN13SamsUtilities6UDebugEPKcz   # SamsUtilities::UDebug(char const*, ...)
  __ZN7CSwitch10ModeSwitchEv   # CSwitch::ModeSwitch()
  __ZN7CSwitch9RezSwitchEii   # CSwitch::RezSwitch(int, int)
  _gei_HFieldOne
  _gei_ParameterSettings
)
