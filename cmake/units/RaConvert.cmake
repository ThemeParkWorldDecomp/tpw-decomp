# Unit w219: Bullfrog pixel converter. Other converters are recording test fakes.
set(TPW_UNIT_RaConvert src/bullfrog/TbRaConvert.cpp)
set(TPW_UNIT_RaConvert_KEEP
  __ZN10CConverter11ConvertAreaEPhS0_jjjj   # CConverter::ConvertArea(unsigned char*, unsigned char*, unsigned int, unsigned int, unsigned int, unsigned int)
  __ZN10CConverter4InitEPK11TbPixFormatS2_PK9TbPalettej   # CConverter::Init(TbPixFormat const*, TbPixFormat const*, TbPalette const*, unsigned int)
  __ZN10CConverter7ConvBufEPhS0_i   # CConverter::ConvBuf(unsigned char*, unsigned char*, int)
  __ZN10CConverter7ReleaseEv   # CConverter::Release()
  __ZN10CConverterC1Ev   # CConverter::CConverter()
  __ZN11CInverseMap7InvCMapEiiRK9TbPaletteiPjPh   # CInverseMap::InvCMap(int, int, TbPalette const&, int, unsigned int*, unsigned char*)
  __ZN11CInverseMap7maxfillEPji   # CInverseMap::maxfill(unsigned int*, int)
  __ZN11CInverseMap7redloopEv   # CInverseMap::redloop()
  __ZN11CInverseMap8blueloopEi   # CInverseMap::blueloop(int)
  __ZN11CInverseMap9greenloopEi   # CInverseMap::greenloop(int)
  __ZN11CInverseMapC1Ev   # CInverseMap::CInverseMap()
  __ZN11CInverseMapD1Ev   # CInverseMap::~CInverseMap()
  __ZN14CConverterBaseD1Ev   # CConverterBase::~CConverterBase()
  __ZN14CMaskConverter12GetShiftBitsEjjPjS0_   # CMaskConverter::GetShiftBits(unsigned int, unsigned int, unsigned int*, unsigned int*)
  __ZN14CMaskConverter13ConvertLineUpEPhS0_i   # CMaskConverter::ConvertLineUp(unsigned char*, unsigned char*, int)
  __ZN14CMaskConverter15ConvertLineDownEPhS0_i   # CMaskConverter::ConvertLineDown(unsigned char*, unsigned char*, int)
  __ZN14CMaskConverter4InitERK11TbPixFormatS2_   # CMaskConverter::Init(TbPixFormat const&, TbPixFormat const&)
  __ZN14CMaskConverterD1Ev   # CMaskConverter::~CMaskConverter()
  __ZN8TbRAConv4MoveER12TbRenderAreaS1_P9TbPalette   # TbRAConv::Move(TbRenderArea&, TbRenderArea&, TbPalette*)
  __ZN8TbRAConvD1Ev   # TbRAConv::~TbRAConv()
  _gBlueloopBinc
  _gBlueloopHere
  _gBlueloopMax
  _gBlueloopMin
  _gGreenloopGcdist
  _gGreenloopGcdp
  _gGreenloopGcrgbp
  _gGreenloopGinc
  _gGreenloopGxx
  _gGreenloopHere
  _gGreenloopMax
  _gGreenloopMin
  _gRaConverter
  _gRedloopRxx
)
set(TPW_UNIT_RaConvert_OWN_CALLLOG ON)
