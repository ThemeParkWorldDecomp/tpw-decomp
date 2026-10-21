set(TPW_UNIT_DrawColourConvert src/bullfrog/Colour.cpp src/bullfrog/MaskConverter.cpp)
set(TPW_UNIT_DrawColourConvert_KEEP
  __ZN19CMaskToPalConverter12GetShiftBitsEjjRjS0_   # CMaskToPalConverter::GetShiftBits(unsigned int, unsigned int, unsigned int&, unsigned int&)
  __ZN19CMaskToPalConverter13ConvertLineUpEPhS0_i   # CMaskToPalConverter::ConvertLineUp(unsigned char*, unsigned char*, int)
  __ZN19CMaskToPalConverter15BruteFindColourEhhh   # CMaskToPalConverter::BruteFindColour(unsigned char, unsigned char, unsigned char)
  __ZN19CMaskToPalConverter15ConvertLineDownEPhS0_i   # CMaskToPalConverter::ConvertLineDown(unsigned char*, unsigned char*, int)
  __ZN19CMaskToPalConverter4InitERK11TbPixFormatRK9TbPalette   # CMaskToPalConverter::Init(TbPixFormat const&, TbPalette const&)
  __ZN19CMaskToPalConverter7ReleaseEv   # CMaskToPalConverter::Release()
  __ZN19CMaskToPalConverterC1ERK11TbPixFormatRK9TbPalette   # CMaskToPalConverter::CMaskToPalConverter(TbPixFormat const&, TbPalette const&)
  __ZN19CMaskToPalConverterD1Ev   # CMaskToPalConverter::~CMaskToPalConverter()
  __ZN19CPalToMaskConverter12GetShiftBitsEjjRjS0_   # CPalToMaskConverter::GetShiftBits(unsigned int, unsigned int, unsigned int&, unsigned int&)
  __ZN19CPalToMaskConverter13ConvertLineUpEPhS0_i   # CPalToMaskConverter::ConvertLineUp(unsigned char*, unsigned char*, int)
  __ZN19CPalToMaskConverter15ConvertLineDownEPhS0_i   # CPalToMaskConverter::ConvertLineDown(unsigned char*, unsigned char*, int)
  __ZN19CPalToMaskConverter4InitERK9TbPaletteRK11TbPixFormat   # CPalToMaskConverter::Init(TbPalette const&, TbPixFormat const&)
  __ZN19CPalToMaskConverterC1Ev   # CPalToMaskConverter::CPalToMaskConverter()
  __ZN19CPalToMaskConverterD1Ev   # CPalToMaskConverter::~CPalToMaskConverter()
  __ZN3bfl4draw13TbMaskRotates4InitERK11TbPixFormatS4_   # bfl::draw::TbMaskRotates::Init(TbPixFormat const&, TbPixFormat const&)
  __ZNK3bfl4draw13TbMaskRotates15GetRightRotate2Ejj   # bfl::draw::TbMaskRotates::GetRightRotate2(unsigned int, unsigned int) const
  __ZNK3bfl4draw7CColour17GetFormattedValueERK11TbPixFormat   # bfl::draw::CColour::GetFormattedValue(TbPixFormat const&) const
  __ZNK3bfl4draw7CColour17GetFormattedValueERKNS0_13TbMaskRotatesE   # bfl::draw::CColour::GetFormattedValue(bfl::draw::TbMaskRotates const&) const
)
