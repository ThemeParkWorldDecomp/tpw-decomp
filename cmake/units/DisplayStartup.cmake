set(TPW_UNIT_DisplayStartup src/engine/DirectDraw.cpp src/engine/Clipping.cpp src/engine/DrawPrimitiveOpenGL.cpp
    src/engine/OpenGL.cpp src/engine/OpenGLTexture.cpp src/engine/tb_printf_opengl.cpp)
set(TPW_UNIT_DisplayStartup_KEEP
  __Z10DDraw_InitP8GrafPortiiiii   # DDraw_Init(GrafPort*, int, int, int, int, int)
  __Z11DDraw_Closev   # DDraw_Close()
  __Z11OpenGL_Initi   # OpenGL_Init(int)
  __Z12OpenGL_Closev   # OpenGL_Close()
  __Z13Clipping_Initv   # Clipping_Init()
  __Z14Clipping_Closev   # Clipping_Close()
  __Z24DrawPrimitiveOpenGL_Initv   # DrawPrimitiveOpenGL_Init()
  __Z25DrawPrimitiveOpenGL_Closev   # DrawPrimitiveOpenGL_Close()
  __ZN11CDirectDraw12ReInitializeEv   # CDirectDraw::ReInitialize()
  __ZN11CDirectDraw12TempShutdownEv   # CDirectDraw::TempShutdown()
  __ZN11CDirectDraw14LockBackbufferEv   # CDirectDraw::LockBackbuffer()
  __ZN11CDirectDraw16UnlockBackbufferEv   # CDirectDraw::UnlockBackbuffer()
  __ZN11CDirectDraw18FindDeviceCallbackEPN10NS_MacDoze5_GUIDEPcS3_Pv   # CDirectDraw::FindDeviceCallback(NS_MacDoze::_GUID*, char*, char*, void*)
  __ZN11CDirectDraw4InitEiiiiii   # CDirectDraw::Init(int, int, int, int, int, int)
  __ZN11CDirectDraw5CloseEv   # CDirectDraw::Close()
  __ZN11CDirectDraw9SetupCapsEv   # CDirectDraw::SetupCaps()
  __ZN11CDirectDrawC1Ev   # CDirectDraw::CDirectDraw()
  __ZN11CDirectDrawD1Ev   # CDirectDraw::~CDirectDraw()
  __ZN13COpenGLPrintf9printf_xyEiiPc   # COpenGLPrintf::printf_xy(int, int, char*)
  __ZN14COpenGLTexture10InitialiseEv   # COpenGLTexture::Initialise()
  __ZN14COpenGLTexture7CleanupEv   # COpenGLTexture::Cleanup()
  __ZN20COpenGLDrawPrimitiveC1Ev   # COpenGLDrawPrimitive::COpenGLDrawPrimitive()
  __ZN7COpenGL11MaxTexturesEii   # COpenGL::MaxTextures(int, int)
  __ZN7COpenGL12ReInitializeEv   # COpenGL::ReInitialize()
  __ZN7COpenGL12TempShutdownEv   # COpenGL::TempShutdown()
  __ZN7COpenGL13ClearViewportEv   # COpenGL::ClearViewport()
  __ZN7COpenGL14InitBackgroundEv   # COpenGL::InitBackground()
  __ZN7COpenGL16InitRenderStatesEv   # COpenGL::InitRenderStates()
  __ZN7COpenGL4InitEi   # COpenGL::Init(int)
  __ZN7COpenGL5CloseEv   # COpenGL::Close()
  __ZN7COpenGLC1Ev   # COpenGL::COpenGL()
  _gDDResult
  _gOpenGLBlendModes
  _gOpenGLDrawPrimitive
  _gOpenGLTextureData
  _gOpenGLTextureMemory
  _gei_Clipping
  _gei_DDraw
  _gei_OpenGL
)
