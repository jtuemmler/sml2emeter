project(sml2emeter)

cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 11)

include_directories(
   .
   util
)

add_executable(smlprinter
   util/smlprinter.cpp
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
   util/crc16ccitt.h
)

add_executable(smlparser
   util/smlparsertest.cpp
   util/crc16ccitt.h
   util/smlparser.h
   util/sml_testpacket.h
)

if(WIN32)
   target_link_libraries(sml2emeter wsock32)
endif(WIN32)
