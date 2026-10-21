# CalculateGamut, GetHFieldIntersection, the CPolygon members and the clip vectors (w94a).
set(TPW_UNIT_HFieldGamut src/engine/Gamut.cpp)
set(TPW_UNIT_HFieldGamut_KEEP
  __Z19GenerateClipXVector10_D3DVECTORS_f   # GenerateClipXVector(_D3DVECTOR, _D3DVECTOR, float)
  __Z19GenerateClipZVector10_D3DVECTORS_f   # GenerateClipZVector(_D3DVECTOR, _D3DVECTOR, float)
  __ZN6CGamut14CalculateGamutEv   # CGamut::CalculateGamut()
  __ZN6CGamut21GetHFieldIntersectionEP10_D3DVECTOR   # CGamut::GetHFieldIntersection(_D3DVECTOR*)
  __ZN8CPolygon16IsOutsideOfPlaneE10_D3DVECTORi   # CPolygon::IsOutsideOfPlane(_D3DVECTOR, int)
  __ZN8CPolygon25GetNewVertexOutsideStatusE10_D3DVECTOR   # CPolygon::GetNewVertexOutsideStatus(_D3DVECTOR)
  __ZN8CPolygon8DumpDataEP10_D3DVECTOR   # CPolygon::DumpData(_D3DVECTOR*)
)
