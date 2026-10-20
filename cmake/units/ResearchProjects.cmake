# The research lab's projects, efforts and outputs (unit w211): the 21 CResearchLab functions from FinalDestroy to GetResearchOutputFromCategory
# and the 5 CResearchProject ones from SetupProject to GetRemainingPoints (src/ResearchLab_projects.cpp). Every callee outside them (the ride
# database, both object controls, the golden tickets, the world's getters, the park analyser's researcher abilities, the advisor, the two
# messages, the message centre and the thing's FinalDestroy) is a fake in the unit file; gMainBalance is the real storage.
set(TPW_UNIT_ResearchProjects src/ResearchLab_projects.cpp src/MainBalance.cpp)
