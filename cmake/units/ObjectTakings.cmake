# Unit w198: a ride's takings, costs, prizes and satisfaction (src/game/Object.cpp; 29 CObject members). The history sums and averages
# (src/game/park/analyser.cpp) and the world accessor (src/game/ui/ui_visitorStatus.cpp) are real; every other callee (the ride database, the world's accessors, the bank
# account, the challenge counter, the map's effectors, the script calls, the random number, the game type constructor, the funny time and its difference,
# the string table, the assertion and the trace) is a recording fake in tests/unit_ObjectTakings.cpp, which also defines the globals of other files.
set(TPW_UNIT_ObjectTakings src/game/Object.cpp src/game/park/analyser.cpp src/game/ui/ui_visitorStatus.cpp
    platform/arena.cpp)
set(TPW_UNIT_ObjectTakings_KEEP
  __ZN16CParkAnalyserValIiE3SetEi   # CParkAnalyserVal<int>::Set(int)
  __ZN7CObject10HadAWinnerEv   # CObject::HadAWinner()
  __ZN7CObject10WalkedAwayEv   # CObject::WalkedAway()
  __ZN7CObject11HadCustomerEv   # CObject::HadCustomer()
  __ZN7CObject11RecordACostEj   # CObject::RecordACost(unsigned int)
  __ZN7CObject13SetPrizeValueEi   # CObject::SetPrizeValue(int)
  __ZN7CObject14SetPricePerUseEi   # CObject::SetPricePerUse(int)
  __ZN7CObject16PersonUsedToiletEj   # CObject::PersonUsedToilet(unsigned int)
  __ZN7CObject16RecordNewTakingsEj   # CObject::RecordNewTakings(unsigned int)
  __ZN7CObject17SetProductQualityEh   # CObject::SetProductQuality(unsigned char)
  __ZN7CObject18SetChanceOfWinningEh   # CObject::SetChanceOfWinning(unsigned char)
  __ZN7CObject19ExpressSatisfactionEi   # CObject::ExpressSatisfaction(int)
  __ZN7CObject28SetAmountOfSpecialIngredientEi   # CObject::SetAmountOfSpecialIngredient(int)
  __ZNK11CWorldSavvy8GetWorldEv   # CWorldSavvy::GetWorld() const
  __ZNK16CParkAnalyserValIiE3GetEv   # CParkAnalyserVal<int>::Get() const
  __ZNK7CObject12GetObjectAgeEv   # CObject::GetObjectAge() const
  __ZNK7CObject13GetPrizeValueEv   # CObject::GetPrizeValue() const
  __ZNK7CObject14GetCostOfGoodsEv   # CObject::GetCostOfGoods() const
  __ZNK7CObject14GetPricePerUseEv   # CObject::GetPricePerUse() const
  __ZNK7CObject15GetTotalRevenueEv   # CObject::GetTotalRevenue() const
  __ZNK7CObject16GetProfitabilityEv   # CObject::GetProfitability() const
  __ZNK7CObject16GetRemainingLifeEv   # CObject::GetRemainingLife() const
  __ZNK7CObject17GetProductQualityEv   # CObject::GetProductQuality() const
  __ZNK7CObject18GetChanceOfWinningEv   # CObject::GetChanceOfWinning() const
  __ZNK7CObject22GetPercentageOfWinnersEv   # CObject::GetPercentageOfWinners() const
  __ZNK7CObject23GetCurrentResidualValueEv   # CObject::GetCurrentResidualValue() const
  __ZNK7CObject23GetCustomerSatisfactionEv   # CObject::GetCustomerSatisfaction() const
  __ZNK7CObject24GetSpecialIngredientNameEv   # CObject::GetSpecialIngredientName() const
  __ZNK7CObject25GetProspectiveCostOfGoodsEff   # CObject::GetProspectiveCostOfGoods(float, float) const
  __ZNK7CObject26GetResidualValuePercentageEv   # CObject::GetResidualValuePercentage() const
  __ZNK7CObject28GetAmountOfSpecialIngredientEv   # CObject::GetAmountOfSpecialIngredient() const
  __ZNK7CObject8WillIWinEP7CPerson   # CObject::WillIWin(CPerson*) const
  __ZN8CHistoryI12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtELi3EE8SetAtEndES2_   # template void HistoryObjectId::SetAtEnd(ObjectIdTag) (explicit instantiation)
  __ZN8CHistoryIfLi144EE15GetHighestValueEfi   # template int32_t CHistory<float, 144>::GetHighestValue(float, int32_t) (explicit instantiation)
  __ZN8CHistoryIfLi144EE8SetAtEndEf   # template void CHistory<float, 144>::SetAtEnd(float) (explicit instantiation)
  __ZN8CHistoryIhLi50EE8SetAtEndEh   # template void CHistory<unsigned char, 50>::SetAtEnd(unsigned char) (explicit instantiation)
  __ZN8CHistoryIiLi144EE11SetFromTempEv   # template void CHistory<int32_t, 144>::SetFromTemp() (explicit instantiation)
  __ZN8CHistoryIiLi144EE14GetLowestValueEii   # template int32_t CHistory<int32_t, 144>::GetLowestValue(int32_t, int32_t) (explicit instantiation)
  __ZN8CHistoryIiLi144EE15GetHighestValueEii   # template int32_t CHistory<int32_t, 144>::GetHighestValue(int32_t, int32_t) (explicit instantiation)
  __ZN8CHistoryIiLi144EE8SetAtEndEi   # template void CHistory<int32_t, 144>::SetAtEnd(int32_t) (explicit instantiation)
  __ZN8CHistoryIiLi30EE11SetFromTempEv   # template void CHistory<int32_t, 30>::SetFromTemp() (explicit instantiation)
  __ZN8CHistoryIiLi30EE8SetAtEndEi   # template void CHistory<int32_t, 30>::SetAtEnd(int32_t) (explicit instantiation)
  __ZN8CHistoryIiLi50EE8SetAtEndEi   # template void CHistory<int32_t, 50>::SetAtEnd(int32_t) (explicit instantiation)
  __ZN8CHistoryIiLi5EE8SetAtEndEi   # template void CHistory<int32_t, 5>::SetAtEnd(int32_t) (explicit instantiation)
  __ZN8CHistoryIjLi30EE11SetFromTempEv   # template void CHistory<uint32_t, 30>::SetFromTemp() (explicit instantiation)
  __ZN8CHistoryIjLi30EE8SetAtEndEj   # template void CHistory<uint32_t, 30>::SetAtEnd(uint32_t) (explicit instantiation)
  __ZNK8CHistoryI12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtELi3EE8GetValueERS2_i   # template bool HistoryObjectId::GetValue(ObjectIdTag&, int32_t) const (explicit instantiation)
  __ZNK8CHistoryIfLi144EE10HowFarBackEv   # template int32_t CHistory<float, 144>::HowFarBack() const (explicit instantiation)
  __ZNK8CHistoryIfLi144EE8GetValueERfi   # template bool CHistory<float, 144>::GetValue(float&, int32_t) const (explicit instantiation)
  __ZNK8CHistoryIhLi50EE10HowFarBackEv   # template int32_t CHistory<unsigned char, 50>::HowFarBack() const (explicit instantiation)
  __ZNK8CHistoryIhLi50EE12GetSumOfLastERdi   # template int32_t CHistory<unsigned char, 50>::GetSumOfLast(double&, int32_t) const (explicit instantiation)
  __ZNK8CHistoryIhLi50EE16GetAverageOfLastERhi   # template void CHistory<unsigned char, 50>::GetAverageOfLast(unsigned char&, int32_t) const (explicit instantiation)
  __ZNK8CHistoryIhLi50EE8GetValueERhi   # template bool CHistory<unsigned char, 50>::GetValue(unsigned char&, int32_t) const (explicit instantiation)
  __ZNK8CHistoryIiLi144EE10HowFarBackEv   # template int32_t CHistory<int32_t, 144>::HowFarBack() const (explicit instantiation)
  __ZNK8CHistoryIiLi144EE12GetSumOfLastERdi   # template int32_t CHistory<int32_t, 144>::GetSumOfLast(double&, int32_t) const (explicit instantiation)
  __ZNK8CHistoryIiLi144EE16GetAverageOfLastERii   # template void CHistory<int32_t, 144>::GetAverageOfLast(int32_t&, int32_t) const (explicit instantiation)
  __ZNK8CHistoryIiLi144EE8GetValueERii   # template bool CHistory<int32_t, 144>::GetValue(int32_t&, int32_t) const (explicit instantiation)
  __ZNK8CHistoryIiLi30EE10HowFarBackEv   # template int32_t CHistory<int32_t, 30>::HowFarBack() const (explicit instantiation)
  __ZNK8CHistoryIiLi30EE12GetSumOfLastERdi   # template int32_t CHistory<int32_t, 30>::GetSumOfLast(double&, int32_t) const (explicit instantiation)
  __ZNK8CHistoryIiLi30EE16GetAverageOfLastERii   # template void CHistory<int32_t, 30>::GetAverageOfLast(int32_t&, int32_t) const (explicit instantiation)
  __ZNK8CHistoryIiLi30EE8GetValueERii   # template bool CHistory<int32_t, 30>::GetValue(int32_t&, int32_t) const (explicit instantiation)
  __ZNK8CHistoryIiLi50EE10HowFarBackEv   # template int32_t CHistory<int32_t, 50>::HowFarBack() const (explicit instantiation)
  __ZNK8CHistoryIiLi50EE12GetSumOfLastERdi   # template int32_t CHistory<int32_t, 50>::GetSumOfLast(double&, int32_t) const (explicit instantiation)
  __ZNK8CHistoryIiLi50EE16GetAverageOfLastERii   # template void CHistory<int32_t, 50>::GetAverageOfLast(int32_t&, int32_t) const (explicit instantiation)
  __ZNK8CHistoryIiLi50EE8GetValueERii   # template bool CHistory<int32_t, 50>::GetValue(int32_t&, int32_t) const (explicit instantiation)
  __ZNK8CHistoryIiLi5EE10HowFarBackEv   # template int32_t CHistory<int32_t, 5>::HowFarBack() const (explicit instantiation)
  __ZNK8CHistoryIiLi5EE8GetValueERii   # template bool CHistory<int32_t, 5>::GetValue(int32_t&, int32_t) const (explicit instantiation)
  __ZNK8CHistoryIjLi30EE10HowFarBackEv   # template int32_t CHistory<uint32_t, 30>::HowFarBack() const (explicit instantiation)
  __ZNK8CHistoryIjLi30EE12GetSumOfLastERdi   # template int32_t CHistory<uint32_t, 30>::GetSumOfLast(double&, int32_t) const (explicit instantiation)
  __ZNK8CHistoryIjLi30EE8GetValueERji   # template bool CHistory<uint32_t, 30>::GetValue(uint32_t&, int32_t) const (explicit instantiation)
  tmpl_0b9ef2b5e4df@game/park/analyser.cpp   # GetValue (a template definition src/ParkAnalyser_history.cpp had before the regroup)
  tmpl_5556a3143f5d@game/park/analyser.cpp   # GetHighestValue (a template definition src/ParkAnalyser_history.cpp had before the regroup)
  tmpl_6717df0d84a9@game/park/analyser.cpp   # SetFromTemp (a template definition src/ParkAnalyser_history.cpp had before the regroup)
  tmpl_9a5c925ed5b6@game/park/analyser.cpp   # GetSumOfLast (a template definition src/ParkAnalyser_history.cpp had before the regroup)
  tmpl_a602598bf002@game/park/analyser.cpp   # GetLowestValue (a template definition src/ParkAnalyser_history.cpp had before the regroup)
  tmpl_a84534163e7b@game/park/analyser.cpp   # GetAverageOfLast (a template definition src/ParkAnalyser_history.cpp had before the regroup)
  tmpl_dc8fcc06f6b3@game/park/analyser.cpp   # HowFarBack (a template definition src/ParkAnalyser_history.cpp had before the regroup)
  tmpl_fd888502d289@game/park/analyser.cpp   # SetAtEnd (a template definition src/ParkAnalyser_history.cpp had before the regroup)
)
