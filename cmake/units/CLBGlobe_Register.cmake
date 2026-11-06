# The online globes' list of globes: slots 0x38 and 0x3c, ~CLBGlobe, the Hides and the destructors that delete what the list holds
# (src/game/online/lobby_globe.cpp). The callees outside the unit are recording fakes in tests/unit_CLBGlobe_Register.cpp.
set(TPW_UNIT_CLBGlobe_Register src/game/online/lobby_globe.cpp src/game/online/LBScreenComponentAdvisor.cpp platform/arena.cpp)
set(TPW_UNIT_CLBGlobe_Register_KEEP
  __ZN18CLBScreenComponent9IsVisibleEv   # CLBScreenComponent::IsVisible()
  __ZN29CLBScreenComponentOnlineGlobe10UnRegisterEP8CLBGlobe   # CLBScreenComponentOnlineGlobe::UnRegister(CLBGlobe*)
  __ZN29CLBScreenComponentOnlineGlobe21DeleteCityListEntriesEv   # CLBScreenComponentOnlineGlobe::DeleteCityListEntries()
  __ZN29CLBScreenComponentOnlineGlobe22DeleteGlobeListEntriesEv   # CLBScreenComponentOnlineGlobe::DeleteGlobeListEntries()
  __ZN29CLBScreenComponentOnlineGlobe4HideEv   # CLBScreenComponentOnlineGlobe::Hide()
  __ZN29CLBScreenComponentOnlineGlobe8RegisterEP8CLBGlobe   # CLBScreenComponentOnlineGlobe::Register(CLBGlobe*)
  __ZN29CLBScreenComponentOnlineGlobeC1Ev   # CLBScreenComponentOnlineGlobe::CLBScreenComponentOnlineGlobe()
  __ZN29CLBScreenComponentOnlineGlobeD1Ev   # CLBScreenComponentOnlineGlobe::~CLBScreenComponentOnlineGlobe()
  __ZN30CLBScreenComponentPublishGlobe4HideEv   # CLBScreenComponentPublishGlobe::Hide()
  __ZN30CLBScreenComponentPublishGlobeD1Ev   # CLBScreenComponentPublishGlobe::~CLBScreenComponentPublishGlobe()
  __ZN31CLBScreenComponentDownloadGlobe4HideEv   # CLBScreenComponentDownloadGlobe::Hide()
  __ZN31CLBScreenComponentDownloadGlobeD1Ev   # CLBScreenComponentDownloadGlobe::~CLBScreenComponentDownloadGlobe()
  __ZN8CLBGlobeD1Ev   # CLBGlobe::~CLBGlobe()
  TPW_GLOBE_SHOWHIDE_REAL@game/online/lobby_globe.cpp   # the #define TPW_GLOBE_SHOWHIDE_REAL of src/game/online/lobby_globe.cpp
)
