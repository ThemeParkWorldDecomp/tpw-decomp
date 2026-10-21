# Unit v5103: COnlineControl Shutdown, WantToShutDown, Update, CallbackFn and the first-person moves. The callees outside the unit are
# recording fakes in tests/unit_COnlineControl_WantToShutDown.cpp.
set(TPW_UNIT_COnlineControl_WantToShutDown src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_COnlineControl_WantToShutDown_KEEP
  __ZN14COnlineControl10CallbackFnEiPv   # COnlineControl::CallbackFn(int, void*)
  __ZN14COnlineControl12UnHideAvatarE6MapPosS0_   # COnlineControl::UnHideAvatar(MapPos, MapPos)
  __ZN14COnlineControl14WantToShutDownEv   # COnlineControl::WantToShutDown()
  __ZN14COnlineControl18LeavingFirstPersonE6MapPosS0_   # COnlineControl::LeavingFirstPerson(MapPos, MapPos)
  __ZN14COnlineControl19EnteringFirstPersonE6MapPosS0_   # COnlineControl::EnteringFirstPerson(MapPos, MapPos)
  __ZN14COnlineControl20UpdateFirstPersonPosE6MapPosS0_   # COnlineControl::UpdateFirstPersonPos(MapPos, MapPos)
  __ZN14COnlineControl6UpdateEv   # COnlineControl::Update()
  __ZN14COnlineControl8ShutdownEv   # COnlineControl::Shutdown()
)
