# Unit w181: CWeather (FinalDestroy, Serialise, ReceiveMessage, FindLightningCoords, SetQuality, the wind setters and UpdateWindInfoToEngine),
# CWeatherForecast (Serialise, Invalidate, Set) and the four save instances over D3DVECTOR_SWAPABLE and PrecipitationType, with the real basic
# serialise instances (the file is the fake), the real message classes (the test's message is a subclass) and the real balance storage. Every
# other callee (the base thing, the random numbers, the funny time, the time span, the rain, the precipitation type, the file) is a fake in
# tests/unit_WeatherParts.cpp.
set(TPW_UNIT_WeatherParts src/Weather.cpp src/Serialise_basic.cpp src/Serialise_float.cpp src/ThingMessages.cpp src/MainBalance.cpp)
set(TPW_UNIT_WeatherParts_OWN_CALLLOG ON)
