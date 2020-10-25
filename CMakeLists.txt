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
   smlstreamreader.h
   smlparser.h
   crc16ccitt.h
   emeterpacket.h
   util/main.cpp
   util/sml_testpacket.h
   util/arduino_stubs.h
   util/arduino_stubs.cpp
   util/WiFiUDP.h
   util/WiFiUDP.cpp
   util/ESP8266WiFi.h
   util/IotWebConf.h
   util/PubSubClient.h
)

if(NOT MSVC)
   target_compile_options(sml2emeter PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)
endif(NOT MSVC)

add_executable(testsmlparser
   smlstreamreader.h
   smlparser.h
   crc16ccitt.h
   util/smlparsertest.cpp
   util/sml_testpacket.h
)

add_executable(testsmlreader
	smlstreamreader.h
	smlparser.h
	util/smlreadertest.cpp
	util/sml_demodata.h
)

add_executable(countertest
	util/countertest.cpp
	util/spi_flash.h
	util/spi_flash.cpp
	counter.h
	counter.cpp
)

if(WIN32)
   target_link_libraries(sml2emeter wsock32)
endif(WIN32)
