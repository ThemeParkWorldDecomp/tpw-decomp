set(TPW_UNIT_Text src/game/Text.cpp)
set(TPW_UNIT_Text_KEEP
  __ZN5CText13SetLanguageIdEj   # CText::SetLanguageId(unsigned int)
  __ZN5CText15GetLanguageNameEv   # CText::GetLanguageName()
  __ZN5CText17InitThemeSpecificEv   # CText::InitThemeSpecific()
  __ZN5CText19DeInitThemeSpecificEv   # CText::DeInitThemeSpecific()
  __ZN5CText4InitEv   # CText::Init()
  __ZN5CText6SetRezEi   # CText::SetRez(int)
  __ZN5CText6UnInitEv   # CText::UnInit()
  __ZNK5CText13GetLanguageIdEv   # CText::GetLanguageId() const
  __ZNK5CText8IsHebrewEv   # CText::IsHebrew() const
  _gText
  _gTextLanguage
  _gTextLanguages
  _gTextTitle
)
