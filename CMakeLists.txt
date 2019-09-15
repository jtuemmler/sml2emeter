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
   smlparser.h
   crc16ccitt.h
   util/main.cpp
   util/sml_testpacket.h
   util/arduino_stubs.h
   util/arduino_stubs.cpp
   util/WiFiUDP.h
   util/WiFiUDP.cpp
   util/ESP8266WiFi.h
   util/IotWebConf.h
)

target_compile_options(sml2emeter PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)

add_executable(smlparser
   smlparser.h
   crc16ccitt.h
   util/smlparsertest.cpp
   util/sml_testpacket.h
)

if(WIN32)
   target_link_libraries(sml2emeter wsock32)
endif(WIN32)
