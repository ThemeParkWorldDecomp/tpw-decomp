# The park's lights and its second initialisation and teardown (unit w110): CreateGlobalLight with the CLight setters,
# LevelInit2 and LevelDeInit with the four CWorld height field accessors. Light.cpp is the light's constructor, which
# CreateGlobalLight runs; the arena holds the height field header the test builds.
set(TPW_UNIT_LevelLights src/game/Light.cpp src/game/TPLevel.cpp src/game/World.cpp platform/arena.cpp)
set(TPW_UNIT_LevelLights_KEEP
  __Z10LevelInit2v   # LevelInit2()
  __Z11LevelDeInitb   # LevelDeInit(bool)
  __Z17CreateGlobalLightijP8s3DPointS0_   # CreateGlobalLight(int, unsigned int, s3DPoint*, s3DPoint*)
  __ZN6CLight11GetLinkTypeEv   # CLight::GetLinkType()
  __ZN6CLight7GetDataEv   # CLight::GetData()
  __ZN6CLight7PrepareEv   # CLight::Prepare()
  __ZN6CLight8SetColorEffff   # CLight::SetColor(float, float, float, float)
  __ZN6CLight9SetNormalEfff   # CLight::SetNormal(float, float, float)
  __ZN6CLight9SetRadiusEf   # CLight::SetRadius(float)
  __ZN6CLightC1Ev   # CLight::CLight()
  __ZN6CLightD1Ev   # CLight::~CLight()
  __ZNK6CWorld19GetHeightfieldWidthEv   # CWorld::GetHeightfieldWidth() const
  __ZNK6CWorld20GetHeightfieldHeightEv   # CWorld::GetHeightfieldHeight() const
  __ZNK6CWorld20GetHeightfieldXStartEv   # CWorld::GetHeightfieldXStart() const
  __ZNK6CWorld20GetHeightfieldYStartEv   # CWorld::GetHeightfieldYStart() const
  _gHeightfieldHeight
  _gHeightfieldRangeExtraX
  _gHeightfieldRangeExtraY
  _gHeightfieldWidth
  _gHeightfieldXOrigin
  _gHeightfieldXStart
  _gHeightfieldYOrigin
  _gHeightfieldYStart
)
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_LevelLights_OWN_CALLLOG ON)
