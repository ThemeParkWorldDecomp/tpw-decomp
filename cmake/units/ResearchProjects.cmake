# The research lab's projects, efforts and outputs (unit w211): the 21 CResearchLab functions from FinalDestroy to GetResearchOutputFromCategory
# and the 5 CResearchProject ones from SetupProject to GetRemainingPoints (src/game/buildings/ResearchLab.cpp). Every callee outside them (the ride
# database, both object controls, the golden tickets, the world's getters, the park analyser's researcher abilities, the advisor, the two
# messages, the message centre and the thing's FinalDestroy) is a fake in the unit file; gMainBalance is the real storage.
set(TPW_UNIT_ResearchProjects src/game/buildings/ResearchLab.cpp src/game/balance/MainBalance.cpp)
set(TPW_UNIT_ResearchProjects_KEEP
  __ZN12CMainBalance21GetBalanceDescriptionEi   # CMainBalance::GetBalanceDescription(int)
  __ZN12CMainBalance21GetBalanceValuesStartEv   # CMainBalance::GetBalanceValuesStart()
  __ZN12CMainBalanceD1Ev   # CMainBalance::~CMainBalance()
  __ZN12CResearchLab11NextProjectE11ResearchCat   # CResearchLab::NextProject(ResearchCat)
  __ZN12CResearchLab11SetWorkLoadEj   # CResearchLab::SetWorkLoad(unsigned int)
  __ZN12CResearchLab12FinalDestroyEv   # CResearchLab::FinalDestroy()
  __ZN12CResearchLab15PreviousProjectE11ResearchCat   # CResearchLab::PreviousProject(ResearchCat)
  __ZN12CResearchLab16CalculateOutputsEv   # CResearchLab::CalculateOutputs()
  __ZN12CResearchLab16CheatCompleteAllEv   # CResearchLab::CheatCompleteAll()
  __ZN12CResearchLab18ContributeResearchEj   # CResearchLab::ContributeResearch(unsigned int)
  __ZN12CResearchLab18UpdateActiveStatusEv   # CResearchLab::UpdateActiveStatus()
  __ZN12CResearchLab18UpdateHighestGroupEi   # CResearchLab::UpdateHighestGroup(int)
  __ZN12CResearchLab24CheatCompleteOnlyPaidForEv   # CResearchLab::CheatCompleteOnlyPaidFor()
  __ZN12CResearchLab9SetEffortEij   # CResearchLab::SetEffort(int, unsigned int)
  __ZN16CResearchProject12SetupProjectEPK8RideInfo   # CResearchProject::SetupProject(RideInfo const*)
  __ZN16CResearchProject16SetupStatUpgradeEPK8RideInfo   # CResearchProject::SetupStatUpgrade(RideInfo const*)
  __ZN16CResearchProject18ContributeResearchEf   # CResearchProject::ContributeResearch(float)
  __ZNK12CResearchLab16GetResearchSpeedE11ResearchCat   # CResearchLab::GetResearchSpeed(ResearchCat) const
  __ZNK12CResearchLab16IsCategoryActiveE11ResearchCat   # CResearchLab::IsCategoryActive(ResearchCat) const
  __ZNK12CResearchLab17GetCurrentProjectE11ResearchCat   # CResearchLab::GetCurrentProject(ResearchCat) const
  __ZNK12CResearchLab17IsAllResearchDoneEv   # CResearchLab::IsAllResearchDone() const
  __ZNK12CResearchLab18GetRemainingPointsE11ResearchCat   # CResearchLab::GetRemainingPoints(ResearchCat) const
  __ZNK12CResearchLab23GetNumberLeftToResearchE11ResearchCat   # CResearchLab::GetNumberLeftToResearch(ResearchCat) const
  __ZNK12CResearchLab29GetResearchOutputFromCategoryE11ResearchCat   # CResearchLab::GetResearchOutputFromCategory(ResearchCat) const
  __ZNK12CResearchLab33GetEstimatedCostForCurrentProjectE11ResearchCat   # CResearchLab::GetEstimatedCostForCurrentProject(ResearchCat) const
  __ZNK12CResearchLab34GetTotalEffortFromActiveCategoriesEv   # CResearchLab::GetTotalEffortFromActiveCategories() const
  __ZNK12CResearchLab9GetEffortEj   # CResearchLab::GetEffort(unsigned int) const
  __ZNK16CResearchProject18GetRemainingPointsEf   # CResearchProject::GetRemainingPoints(float) const
  __ZNK16CResearchProject18IsProjectCompletedEv   # CResearchProject::IsProjectCompleted() const
  _gMainBalance
  _gMainBalanceDescriptions
)
