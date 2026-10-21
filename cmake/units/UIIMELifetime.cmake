# Unit w92: the construction and destruction of the IME classes (CIME constructor and virtual destructor, the two MSL vector
# tear_downs, list_cand_data_t and CUIIME destructors). The three CIME blr stubs of TPEmbed.cpp share a source with them.
set(TPW_UNIT_UIIMELifetime src/game/ui/ui_graph.cpp src/game/ui/ui_iterators.cpp src/game/ui/IME.cpp)
set(TPW_UNIT_UIIMELifetime_KEEP
  __ZN3IME16list_cand_data_tD1Ev   # IME::list_cand_data_t::~list_cand_data_t()
  __ZN3IME4CIME10UnActivateEv   # IME::CIME::UnActivate()
  __ZN3IME4CIME23SetMaxCompositionLengthEi   # IME::CIME::SetMaxCompositionLength(int)
  __ZN3IME4CIME8ActivateEP8GrafPort   # IME::CIME::Activate(GrafPort*)
  __ZN3IME4CIMEC1Ev   # IME::CIME::CIME()
  __ZN3IME4CIMED1Ev   # IME::CIME::~CIME()
  __ZN6CUIIMED1Ev   # CUIIME::~CUIIME()
  __ZN9MslVectorIN3IME13MslWideStringEE9tear_downEv   # MslVector<IME::MslWideString>::tear_down()
  __ZN9MslVectorIN3IME16list_cand_data_tEE9tear_downEv   # MslVector<IME::list_cand_data_t>::tear_down()
)
