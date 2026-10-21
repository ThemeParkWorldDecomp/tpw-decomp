# The engine mesh draw objects' instancing and changes, the gradients' Change and the UI matrix helpers (unit w42). The matrix leaves
# (SetMatrixIdentity, VectorThroughLMatrixNoW) and the rectangles are real; InstanceMesh, the engine mesh's local matrix, the draw object
# base class and the constructors are fakes in the unit file.
set(TPW_UNIT_UIEngineMesh src/game/TPMesh.cpp src/game/ui/InterfaceDrawEngine.cpp src/game/LMatrix.cpp
    src/game/ui/CoolRect.cpp src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp platform/arena.cpp)
set(TPW_UNIT_UIEngineMesh_KEEP
  __Z11GetUIMatrixP7sMatrixssssffffsss   # GetUIMatrix(sMatrix*, short, short, short, short, float, float, float, float, short, short, short)
  __Z11LUBackSubstR7sMatrixPiPf   # LUBackSubst(sMatrix&, int*, float*)
  __Z12InvertMatrixP7sMatrixPKS_   # InvertMatrix(sMatrix*, sMatrix const*)
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z17SetMatrixIdentityP7sMatrix   # SetMatrixIdentity(sMatrix*)
  __Z19ScaleLMatrixNoTransP7sMatrixP8s3DPoint   # ScaleLMatrixNoTrans(sMatrix*, s3DPoint*)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z21UpdateMeshLocalMatrixP9tag_sMeshP7sMatrix   # UpdateMeshLocalMatrix(tag_sMesh*, sMatrix*)
  __Z23VectorThroughLMatrixNoWP7sMatrixP8s3DPoint   # VectorThroughLMatrixNoW(sMatrix*, s3DPoint*)
  __Z25CombineMatricesNoWDest1stP7sMatrixS0_   # CombineMatricesNoWDest1st(sMatrix*, sMatrix*)
  __Z25CombineMatricesNoWDest2ndP7sMatrixS0_   # CombineMatricesNoWDest2nd(sMatrix*, sMatrix*)
  __Z34GetLMatrixRotationAroundZAxisYOnlyP7sMatrix   # GetLMatrixRotationAroundZAxisYOnly(sMatrix*)
  __Z8LUDecompR7sMatrixPiPf   # LUDecomp(sMatrix&, int*, float*)
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN23InterfaceDrawEngineMesh10InstanceMeEv   # InterfaceDrawEngineMesh::InstanceMe()
  __ZN23InterfaceDrawEngineMesh14CalculateFrameEjP8DOChange   # InterfaceDrawEngineMesh::CalculateFrame(unsigned int, DOChange*)
  __ZN23InterfaceDrawEngineMesh6ChangeEjP8DOChange   # InterfaceDrawEngineMesh::Change(unsigned int, DOChange*)
  __ZN28InterfaceDrawEngineHGradient6ChangeEjP8DOChange   # InterfaceDrawEngineHGradient::Change(unsigned int, DOChange*)
  __ZN28InterfaceDrawEngineHGradient9CalcVertsEv   # InterfaceDrawEngineHGradient::CalcVerts()
  __ZN28InterfaceDrawEngineVGradient6ChangeEjP8DOChange   # InterfaceDrawEngineVGradient::Change(unsigned int, DOChange*)
  __ZN8CoolRect14RectIntersectsEPS_   # CoolRect::RectIntersects(CoolRect*)
  __ZN8CoolRect3GetEPsS0_S0_S0_   # CoolRect::Get(short*, short*, short*, short*)
  __ZN8CoolRect3SetEssss   # CoolRect::Set(short, short, short, short)
  __ZN8CoolRect5ShiftEss   # CoolRect::Shift(short, short)
  __ZN8CoolRect6AdjustEssss   # CoolRect::Adjust(short, short, short, short)
  __ZN8CoolRect6MiddleEPsS0_   # CoolRect::Middle(short*, short*)
  __ZN8CoolRect7GetDimsEPsS0_   # CoolRect::GetDims(short*, short*)
  __ZN8CoolRect7PointInEss   # CoolRect::PointIn(short, short)
  __ZN8CoolRectC1Essss   # CoolRect::CoolRect(short, short, short, short)
  __ZN8CoolRectC1Ev   # CoolRect::CoolRect()
  __ZN8CoolRectD1Ev   # CoolRect::~CoolRect()
  _gUIHighPresets
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
)
