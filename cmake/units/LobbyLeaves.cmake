# The lobby's remaining leaves (unit w40): the version number, the font table entry, text measuring, the mail icon's handler, the game
# type (main app) and the ltms string buffer with the converter it holds (two test modules, one per program: test_LobbyLeaves and
# test_LobbyLeavesLtms).
set(TPW_UNIT_LobbyLeaves src/game/ui/ui_main.cpp src/game/ui/ui_utils.cpp src/game/online/lobby.cpp
    src/game/online/FS_Online.cpp src/ltms/StaticUnicodeBuffer.cpp src/ltms/TbFontRenderParams.cpp
    src/ltms/TbIResourceIndex.cpp src/game/ui/CoolRect.cpp src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp)
set(TPW_UNIT_LobbyLeaves_KEEP
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z14UI_MeasureTextPKDsRK6TbRectaaaiPS1_   # UI_MeasureText(char16_t const*, TbRect const&, signed char, signed char, signed char, int, TbRect*)
  __Z17GetFontTableEntryj   # GetFontTableEntry(unsigned int)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z22UI_CreateVersionNumberb   # UI_CreateVersionNumber(bool)
  __Z27LB_Lobby_MailIconMessagesFnP15InterfaceWindowiii   # LB_Lobby_MailIconMessagesFn(InterfaceWindow*, int, int, int)
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN14TbIMBToUnicode13ConvertStringEPDsPKht   # TbIMBToUnicode::ConvertString(char16_t*, unsigned char const*, unsigned short)
  __ZN14TbIMBToUnicodeD1Ev   # TbIMBToUnicode::~TbIMBToUnicode()
  __ZN20CStaticUnicodeBuffer13AppendUnicodeEDs   # CStaticUnicodeBuffer::AppendUnicode(char16_t)
  __ZN20CStaticUnicodeBuffer13AppendUnicodeEPKDsj   # CStaticUnicodeBuffer::AppendUnicode(char16_t const*, unsigned int)
  __ZN20CStaticUnicodeBuffer5CloseEv   # CStaticUnicodeBuffer::Close()
  __ZN20CStaticUnicodeBuffer8AppendMBEPKhj   # CStaticUnicodeBuffer::AppendMB(unsigned char const*, unsigned int)
  __ZN20CStaticUnicodeBuffer8AppendMBEt   # CStaticUnicodeBuffer::AppendMB(unsigned short)
  __ZN20CStaticUnicodeBufferD1Ev   # CStaticUnicodeBuffer::~CStaticUnicodeBuffer()
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
  __ZN9CGameType11SetGameTypeE8GameType   # CGameType::SetGameType(GameType)
  __ZN9CGameTypeC1Ev   # CGameType::CGameType()
  __ZN9TbIMBInfoD1Ev   # TbIMBInfo::~TbIMBInfo()
  __ZNK9TbIMBInfo8GetIndexERPKh   # TbIMBInfo::GetIndex(unsigned char const*&) const
  _gUIHighPresets
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
)
