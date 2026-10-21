# Unit v5953: ConvertSquareFrom32, PT_Load, CSwitch::Do, COpenGLPrintf::Close (src/engine/DirectDraw.cpp, src/engine/Switch.cpp, src/engine/tb_hfield.cpp, src/engine/tb_printf_opengl.cpp). The callees outside the unit
# are recording fakes in tests/unit_ConvertSquareFrom32.cpp, which also defines the globals of other files.
set(TPW_UNIT_ConvertSquareFrom32 src/engine/DirectDraw.cpp src/engine/Switch.cpp src/engine/tb_hfield.cpp
    src/engine/tb_printf_opengl.cpp platform/arena.cpp)
set(TPW_UNIT_ConvertSquareFrom32_KEEP
  __Z19ConvertSquareFrom32PvS_i   # ConvertSquareFrom32(void*, void*, int)
  __Z7PT_LoadPci   # PT_Load(char*, int)
  __ZN13COpenGLPrintf5CloseEv   # COpenGLPrintf::Close()
  __ZN7CSwitch2DoEi   # CSwitch::Do(int)
)
