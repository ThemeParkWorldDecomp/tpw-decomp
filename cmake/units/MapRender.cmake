set(TPW_UNIT_MapRender src/engine/Gamut.cpp src/engine/MapWho.cpp src/engine/Mesh.cpp src/engine/EngineUtil.cpp
    src/engine/CsTexture.cpp)
set(TPW_UNIT_MapRender_KEEP
  __Z20FPU_RestorePrecisions   # FPU_RestorePrecision(short)
  __Z20FPU_SetFullPrecisions   # FPU_SetFullPrecision(short)
  __Z22FPU_SetSinglePrecisionv   # FPU_SetSinglePrecision()
  __ZN10CMeshDelay5ResetEv   # CMeshDelay::Reset()
  __ZN10CMeshDelay7PrepareEv   # CMeshDelay::Prepare()
  __ZN12CLoadSurface7DisplayEv   # CLoadSurface::Display()
  __ZN6CGamut8GetQuadsEv   # CGamut::GetQuads()
  __ZN7CMapWho12RenderWindowEP7SWindow   # CMapWho::RenderWindow(SWindow*)
  __ZN7CMapWho16GetRenderOptionsEv   # CMapWho::GetRenderOptions()
  __ZN7CMapWho16SetRenderOptionsEi   # CMapWho::SetRenderOptions(int)
  __ZN7CMapWho17PrepareWorldspaceEv   # CMapWho::PrepareWorldspace()
  __ZN7CMapWho19ALTTAB_RestoreStateEv   # CMapWho::ALTTAB_RestoreState()
  __ZN7CMapWho6RenderEv   # CMapWho::Render()
  _gMeshDelay
  _gRenderEntryFlags
  _gRenderFrameMode
  _gRenderLastView
  _gRenderLastViewMade
)
