# Unit w231: surface wrappers with the real pixel-mask helper.
set(TPW_UNIT_LbSurface src/bullfrog/LbSurface.cpp src/bullfrog/TbPixFmt.cpp platform/arena.cpp)
set(TPW_UNIT_LbSurface_KEEP
  _TbPixFormat16
  _TbPixFormat24
  _TbPixFormat32
  _TbPixFormat8
  __Z13LbSurface_BltPK9TbSurfacejjS1_PK6TbRectj   # LbSurface_Blt(TbSurface const*, unsigned int, unsigned int, TbSurface const*, TbRect const*, unsigned int)
  __Z14LbSurface_LockP9TbSurfacePPh   # LbSurface_Lock(TbSurface*, unsigned char**)
  __Z16LbSurface_CreatejjjP9TbSurface   # LbSurface_Create(unsigned int, unsigned int, unsigned int, TbSurface*)
  __Z16LbSurface_DeleteP9TbSurface   # LbSurface_Delete(TbSurface*)
  __Z16LbSurface_UnlockP9TbSurface   # LbSurface_Unlock(TbSurface*)
  __Z18_InitialiseSurfaceP9TbSurfacejjjj   # _InitialiseSurface(TbSurface*, unsigned int, unsigned int, unsigned int, unsigned int)
  __Z19LbSurface_ClearRectPK9TbSurfacePK6TbRect8TbColourj   # LbSurface_ClearRect(TbSurface const*, TbRect const*, TbColour, unsigned int)
  __Z20LbSurface_GetCurrentv   # LbSurface_GetCurrent()
  __Z20LbSurface_SetCurrentP9TbSurface   # LbSurface_SetCurrent(TbSurface*)
  __Z20LbSurface_SetSysLocki   # LbSurface_SetSysLock(int)
  __Z20LbSurface_StretchBltPK9TbSurfacePK6TbRectS1_S4_j   # LbSurface_StretchBlt(TbSurface const*, TbRect const*, TbSurface const*, TbRect const*, unsigned int)
  __Z21LbSurface_GetFadeStepPK9TbSurface   # LbSurface_GetFadeStep(TbSurface const*)
  __Z21LbSurface_SetFadeStepP9TbSurfacej   # LbSurface_SetFadeStep(TbSurface*, unsigned int)
  __Z22LbSurface_GetDDSurfaceP9TbSurfacei   # LbSurface_GetDDSurface(TbSurface*, int)
  __Z22LbSurface_GetDrawFlagsPK9TbSurface   # LbSurface_GetDrawFlags(TbSurface const*)
  __Z22LbSurface_GetFadeTablePK9TbSurface   # LbSurface_GetFadeTable(TbSurface const*)
  __Z22LbSurface_GetOriginPtrPK9TbSurface   # LbSurface_GetOriginPtr(TbSurface const*)
  __Z22LbSurface_SetDrawFlagsP9TbSurfacej   # LbSurface_SetDrawFlags(TbSurface*, unsigned int)
  __Z22LbSurface_SetFadeTableP9TbSurfacePh   # LbSurface_SetFadeTable(TbSurface*, unsigned char*)
  __Z23LbSurface_GetFadeColourP9TbSurfacej   # LbSurface_GetFadeColour(TbSurface*, unsigned int)
  __Z23LbSurface_GetGhostTablePK9TbSurface   # LbSurface_GetGhostTable(TbSurface const*)
  __Z23LbSurface_SetGhostTableP9TbSurfacePh   # LbSurface_SetGhostTable(TbSurface*, unsigned char*)
  __Z23_CheckAndRestoreSurfacePK9TbSurface   # _CheckAndRestoreSurface(TbSurface const*)
  __Z24LbSurface_GetPixelFormatPK9TbSurfaceR11TbPixFormat   # LbSurface_GetPixelFormat(TbSurface const*, TbPixFormat&)
  __Z24LbSurface_SetDrawFlagsOnP9TbSurfacej   # LbSurface_SetDrawFlagsOn(TbSurface*, unsigned int)
  __Z25LbSurface_MapToRenderAreaR12TbRenderAreaP9TbSurface   # LbSurface_MapToRenderArea(TbRenderArea&, TbSurface*)
  __Z25LbSurface_SetDrawFlagsOffP9TbSurfacej   # LbSurface_SetDrawFlagsOff(TbSurface*, unsigned int)
  __Z28LbSurface_RegisterDirectDrawP11IDirectDraw   # LbSurface_RegisterDirectDraw(IDirectDraw*)
  __Z35LbSurface_RegisterDirectDrawSurfaceP18IDirectDrawSurfaceP9TbSurface   # LbSurface_RegisterDirectDrawSurface(IDirectDrawSurface*, TbSurface*)
  __ZN10TbMaskInfo3SetEjj   # TbMaskInfo::Set(unsigned int, unsigned int)
  __ZN11TbPixelCaps3SetERK11TbPixFormat   # TbPixelCaps::Set(TbPixFormat const&)
  __ZNK9TbSurface13GetRenderAreaEv   # TbSurface::GetRenderArea() const
  __ZNK9TbSurface20CalculateSurfaceAreaER13DDSURFACEDESC   # TbSurface::CalculateSurfaceArea(DDSURFACEDESC&) const
  __lbCurrentDrawFlags
  __lbCurrentGhostTable
  __lbpCurrentFadeRow
  __lbpCurrentFadeTable
  _gSurfaceDirectDraw
  _gSurfaceSysLockDisabled
)
