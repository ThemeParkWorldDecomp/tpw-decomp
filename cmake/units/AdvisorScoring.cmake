# Unit w95: the advisor's message scoring (CAdvisorScoring's constructor, destructor and ten queries) and CAdvisorBalance (the advisor's
# balance table: two virtuals and the generated description rows). The message table is generated data. The balance reader (Init, Load,
# UnInit), the trace output and the game type constructor are fakes in the unit file; the three scorers the table's pointers use are fakes too.
set(TPW_UNIT_AdvisorScoring src/AdvisorScoring.cpp ${TPW_GENERATED_DIR}/AdvisorScoring_table.cpp src/AdvisorBalance.cpp ${TPW_GENERATED_DIR}/AdvisorBalance_table.cpp)
