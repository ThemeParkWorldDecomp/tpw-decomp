# Unit v5368: OnlineConfig::~OnlineConfig (src/game/MslSupport.cpp). operator delete is a recording fake in
# tests/unit_OnlineConfig_Dtor_OnlineConfig.cpp.
set(TPW_UNIT_OnlineConfig_Dtor_OnlineConfig src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_OnlineConfig_Dtor_OnlineConfig_KEEP
  __ZN17online_config_mac7DestroyEP13MslCharStrings   # online_config_mac::Destroy(MslCharString*, short)
)
