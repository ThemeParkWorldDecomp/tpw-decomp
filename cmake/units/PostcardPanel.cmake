# Unit w102: postcard panel, preview and keyboard forwarding calls.
# CoolRect and TbRenderArea lifetimes are real; other boundaries record.
# Compile the existing constructor alone: the rest of its source has the other constructors and members it does not need.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/bullfrog/TbRenderArea.cpp" TPW_POSTCARD_RENDERAREA_SOURCE)
string(REGEX MATCH "TbRenderArea::TbRenderArea\\(\\) \\{[^}]*\\}" TPW_POSTCARD_RENDERAREA_CTOR "${TPW_POSTCARD_RENDERAREA_SOURCE}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/PostcardPanel_renderarea.cpp"
     CONTENT "#include \"TbGraphicFile.h\"\n${TPW_POSTCARD_RENDERAREA_CTOR}\n")
set(TPW_UNIT_PostcardPanel src/game/keyboard.cpp src/game/ui/UIInterface.cpp src/game/ui/ui_Postcard.cpp
    src/game/ui/CoolRect.cpp src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/PostcardPanel_renderarea.cpp")
set(TPW_UNIT_PostcardPanel_KEEP
  __Z12TakePostCardv   # TakePostCard()
  __Z13UI_DoPostcardv   # UI_DoPostcard()
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z16PostcardPanelMsgP15InterfaceWindowiii   # PostcardPanelMsg(InterfaceWindow*, int, int, int)
  __Z18PostcardPreviewMsgP15InterfaceWindowiii   # PostcardPreviewMsg(InterfaceWindow*, int, int, int)
  __Z19CreatePostcardPanelv   # CreatePostcardPanel()
  __Z19RemovePostcardPanelv   # RemovePostcardPanel()
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z20RefreshPreviewWindowP15InterfaceWindow   # RefreshPreviewWindow(InterfaceWindow*)
  __Z21UI_ActuallyDoPostcardv   # UI_ActuallyDoPostcard()
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN11UIInterface10DoPostcardEv   # UIInterface::DoPostcard()
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
  _gAddressEdited
  _gMessageEdited
  _gPostcard
  _gPostcardAddress
  _gPostcardMessage
  _gPostcardPanel
  _gPostcardPanelDescription
  _gPostcardSent
  _gPostcardText5da5
  _gPreviewDragging
  _gPreviewLastX
  _gPreviewLastY
  _gPreviewOffsetX
  _gPreviewOffsetY
  _gTempPath
  _gTempPathFetched
  _gUIHighPresets
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
)
set(TPW_UNIT_PostcardPanel_OWN_CALLLOG ON)
