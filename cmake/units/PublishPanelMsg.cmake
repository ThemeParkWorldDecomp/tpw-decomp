# Unit v5064: the publish park panel and the online park list panel (PublishPanelMsg, CreatePublishParkPanel, CreateOnlineParkListPanel;
# src/PublishPanelMsg.cpp). The callees outside the unit are recording fakes in tests/unit_PublishPanelMsg.cpp, which also defines the
# globals of other files. List only the sources the library must compile.
set(TPW_UNIT_PublishPanelMsg src/PublishPanelMsg.cpp platform/arena.cpp)
