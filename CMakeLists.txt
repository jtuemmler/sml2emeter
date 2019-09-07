project(sml2emeter)

cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 11)

include_directories(
   .
   util
)

add_executable(smlparser
   util/smlparser.cpp
)

add_executable(sml2emeter
   sml2emeter.ino
   util/main.cpp
   util/sml_testpacket.h
   util/arduino_stubs.h
   util/arduino_stubs.cpp
   util/WiFiUDP.h
   util/WiFiUDP.cpp
   util/ESP8266WiFi.h
   util/IotWebConf.h
)

if(WIN32)
   target_link_libraries(sml2emeter wsock32)
endif(WIN32)
