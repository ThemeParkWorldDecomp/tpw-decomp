# Unit v51047: nine functions (src/game/save/FS_LoadSave.cpp, src/game/ui/InterfaceKnob.cpp, src/game/ui/UIGrapher.cpp, src/game/ui/ui_2dmap.cpp). The callees outside the unit are recording fakes in
# tests/unit_std_basic_istream_basic_istream.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_basic_istream_basic_istream src/game/save/FS_LoadSave.cpp src/game/ui/InterfaceKnob.cpp
    src/game/ui/UIGrapher.cpp src/game/ui/ui_2dmap.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_istream_basic_istream_KEEP
  __Z18CUI2DMapSpriteDtorP14CUI2DMapSprites   # CUI2DMapSpriteDtor(CUI2DMapSprite*, short)
  __Z20CUIGrapher_ConstructP10CUIGrapher   # CUIGrapher_Construct(CUIGrapher*)
  __Z5entryv   # entry()
  __ZN10CUIGrapher5ClearEv   # CUIGrapher::Clear()
  __ZN11CFSLoadSave22GetNextSaveFileDetailsEPK26TFSLoadSaveSaveFileDetails   # CFSLoadSave::GetNextSaveFileDetails(TFSLoadSaveSaveFileDetails const*)
  __ZN13InterfaceKnobmiER15InterfaceWindow   # InterfaceKnob::operator-(InterfaceWindow&)
  __ZN14CUI2DMapSprite13SetVisibilityEi   # CUI2DMapSprite::SetVisibility(int)
  __ZN14CUI2DMapSprite8SetScaleEf   # CUI2DMapSprite::SetScale(float)
  __ZN14CUI2DMapSpriteD1Ev   # CUI2DMapSprite::~CUI2DMapSprite()
  __ZN20ifstream_char_callee11IstreamCtorEP11MacIfstreamij   # ifstream_char_callee::IstreamCtor(MacIfstream*, int, unsigned int)
  _gCUI2DMapSpriteVt
)
