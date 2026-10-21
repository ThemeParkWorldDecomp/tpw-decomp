# Unit v5670: CFSOnlineTheme's constructor and destructor (src/game/online/FS_OnlineList.cpp). The string callees are recording fakes in
# tests/unit_CFSOnlineTheme_CFSOnlineTheme.cpp; the list members run for real.
set(TPW_UNIT_CFSOnlineTheme_CFSOnlineTheme src/game/online/FS_OnlineList.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineTheme_CFSOnlineTheme_KEEP
  __ZN14CFSOnlineThemeC1EPK11ccThemeInfoP24CFSOnlineSystemThemeList   # CFSOnlineTheme::CFSOnlineTheme(ccThemeInfo const*, CFSOnlineSystemThemeList*)
  __ZN14CFSOnlineThemeD1Ev   # CFSOnlineTheme::~CFSOnlineTheme()
)
