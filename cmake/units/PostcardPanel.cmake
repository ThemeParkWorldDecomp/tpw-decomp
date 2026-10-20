# Unit w102: postcard panel, preview and keyboard forwarding calls.
# CoolRect and TbRenderArea lifetimes are real; other boundaries record.
# Compile the existing constructor alone: the rest of its source contains unrelated file loaders.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/bfl_graphicfile.cpp" TPW_POSTCARD_RENDERAREA_SOURCE)
string(REGEX MATCH "TbRenderArea::TbRenderArea\\(\\) \\{[^}]*\\}" TPW_POSTCARD_RENDERAREA_CTOR "${TPW_POSTCARD_RENDERAREA_SOURCE}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/PostcardPanel_renderarea.cpp"
     CONTENT "#include \"TbGraphicFile.h\"\n${TPW_POSTCARD_RENDERAREA_CTOR}\n")
set(TPW_UNIT_PostcardPanel src/ui_Postcard_panel.cpp src/ui_Postcard_data.cpp src/UI_Base.cpp "${CMAKE_CURRENT_BINARY_DIR}/PostcardPanel_renderarea.cpp")
set(TPW_UNIT_PostcardPanel_OWN_CALLLOG ON)
