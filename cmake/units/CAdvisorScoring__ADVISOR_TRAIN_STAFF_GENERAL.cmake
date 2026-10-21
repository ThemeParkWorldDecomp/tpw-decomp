# Unit v5282: eight advisor scorers (src/game/advisor/AdvisorScoring.cpp). The callees outside the unit are
# recording fakes in tests/unit_CAdvisorScoring__ADVISOR_TRAIN_STAFF_GENERAL.cpp, which also defines the globals of other files.
set(TPW_UNIT_CAdvisorScoring__ADVISOR_TRAIN_STAFF_GENERAL src/game/advisor/AdvisorScoring.cpp platform/arena.cpp)
set(TPW_UNIT_CAdvisorScoring__ADVISOR_TRAIN_STAFF_GENERAL_KEEP
  __ZN15CAdvisorScoring24_ADVISOR_NEED_STAFF_ROOMEv   # CAdvisorScoring::_ADVISOR_NEED_STAFF_ROOM()
  __ZN15CAdvisorScoring25_ADVISOR_CONGRAT_COSTUMESEv   # CAdvisorScoring::_ADVISOR_CONGRAT_COSTUMES()
  __ZN15CAdvisorScoring27_ADVISOR_HIRE_STAFF_GENERALEv   # CAdvisorScoring::_ADVISOR_HIRE_STAFF_GENERAL()
  __ZN15CAdvisorScoring28_ADVISOR_SET_PATROLS_GENERALEv   # CAdvisorScoring::_ADVISOR_SET_PATROLS_GENERAL()
  __ZN15CAdvisorScoring28_ADVISOR_TRAIN_STAFF_GENERALEv   # CAdvisorScoring::_ADVISOR_TRAIN_STAFF_GENERAL()
  __ZN15CAdvisorScoring32_ADVISOR_PRICING_FOOD_POOR_VALUEEv   # CAdvisorScoring::_ADVISOR_PRICING_FOOD_POOR_VALUE()
  __ZN15CAdvisorScoring33_ADVISOR_PRICING_SHOPS_POOR_VALUEEv   # CAdvisorScoring::_ADVISOR_PRICING_SHOPS_POOR_VALUE()
  __ZN15CAdvisorScoring38_ADVISOR_PRICING_RESTAURANT_POOR_VALUEEv   # CAdvisorScoring::_ADVISOR_PRICING_RESTAURANT_POOR_VALUE()
  _gAdvisorNoneFoodPoorValue
  _gAdvisorNoneRestaurantPoorValue
  _gAdvisorNoneShopsPoorValue
)
