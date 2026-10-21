# Unit v5267: keyboard cheat actions (src/game/keyboard.cpp). The callees are recording fakes in
# tests/unit_WeatherUpLots.cpp, which also defines the globals of other files.
set(TPW_UNIT_WeatherUpLots src/game/keyboard.cpp platform/arena.cpp)
set(TPW_UNIT_WeatherUpLots_KEEP
  __Z11WeatherDownv   # WeatherDown()
  __Z13ChatterTogglev   # ChatterToggle()
  __Z13WeatherUpLotsv   # WeatherUpLots()
  __Z14start_cheatingv   # start_cheating()
  __Z15WeatherDownLotsv   # WeatherDownLots()
  __Z17DeactivateWeatherv   # DeactivateWeather()
  __Z28RealTimeLightingMasterTogglev   # RealTimeLightingMasterToggle()
  __Z9WeatherUpv   # WeatherUp()
  _gChatterToggleGuard
  _gChatterToggleOn
)
