set(TPW_UNIT_tb_init src/engine/tb_init.cpp)
set(TPW_UNIT_tb_init_KEEP
  __Z14EngineAdd_InitiiPc   # EngineAdd_Init(int, int, char*)
  __Z15EngineAdd_Closev   # EngineAdd_Close()
  __Z16EngineAdd_Reinitv   # EngineAdd_Reinit()
  __ZN19CProcTextureHandler11PrepareListEP12PrepareEntryP12sTileOverlayi   # CProcTextureHandler::PrepareList(PrepareEntry*, sTileOverlay*, int)
  __ZN19CProcTextureHandler17CreateProcTextureEP13CTextureEntryiiP12sTileOverlay   # CProcTextureHandler::CreateProcTexture(CTextureEntry*, int, int, sTileOverlay*)
  __ZN19CProcTextureHandler4InitEii   # CProcTextureHandler::Init(int, int)
  __ZN19CProcTextureHandler4LoadEPc   # CProcTextureHandler::Load(char*)
  __ZN19CProcTextureHandler5CloseEv   # CProcTextureHandler::Close()
  __ZN19CProcTextureHandlerC1Ev   # CProcTextureHandler::CProcTextureHandler()
  __ZN19CProcTextureHandlerD1Ev   # CProcTextureHandler::~CProcTextureHandler()
  _gProcTextureBuffer
)
