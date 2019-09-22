#include <stdio.h>
#include <string.h>
#include <initializer_list>
#include "crc16ccitt.h"
#include "smlparser.h"
#include "sml_testpacket.h"

SmlParser smlParser;
uint8_t smlPacket[SML_TEST_PACKET_LENGTH];

class BaseDecodeTests : public SmlParser {
public:
   BaseDecodeTests() : SmlParser() {}

   int run() {
      int failed = 0;

      failed += testIntegerDecoding(init({ 0x51, 0xff }), 0);
      failed += testIntegerDecoding(init({ 0x52, 0x00 }), 0);
      failed += testIntegerDecoding(init({ 0x52, 0x10 }), 16);
      failed += testIntegerDecoding(init({ 0x52, 0x80 }), -128);
      failed += testIntegerDecoding(init({ 0x52, 0xff }), -1);
      failed += testIntegerDecoding(init({ 0x52, 0xfe }), -2);
      failed += testIntegerDecoding(init({ 0x53, 0xc8, 0x7a }), -14214);
      failed += testIntegerDecoding(init({ 0x54, 0x00, 0x86, 0x08 }), 34312);
      failed += testIntegerDecoding(init({ 0x54, 0x00, 0x86, 0x08 }), 34312);
      failed += testIntegerDecoding(init({ 0x54, 0xff, 0x79, 0xf8 }), -34312);
      failed += testIntegerDecoding(init({ 0x59, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }), 9223372036854775807);
      failed += testIntegerDecoding(init({ 0x59, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }), -1);

      failed += testIntegerDecoding(init({ 0x61, 0xff }), 0);
      failed += testIntegerDecoding(init({ 0x62, 0x00 }), 0);
      failed += testIntegerDecoding(init({ 0x62, 0x10 }), 16);
      failed += testIntegerDecoding(init({ 0x62, 0x80 }), 128);
      failed += testIntegerDecoding(init({ 0x64, 0x00, 0x86, 0x08 }), 34312);
      failed += testIntegerDecoding(init({ 0x64, 0x00, 0x86, 0x08 }), 34312);
      failed += testIntegerDecoding(init({ 0x64, 0xff, 0x79, 0xf8 }), 16742904);
      failed += testIntegerDecoding(init({ 0x69, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }), 9223372036854775807);
      failed += testIntegerDecoding(init({ 0x69, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }), -1);

      failed += testLengthDecoding(init({ 0x10 }), 0x0, 1, true);
      failed += testLengthDecoding(init({ 0x15 }), 0x5, 1, true);
      failed += testLengthDecoding(init({ 0x15 }), 0x5, 1, true);
      failed += testLengthDecoding(init({ 0x81, 0x82, 0x83, 0x04 }), 0x1234, 4, true);
      failed += testLengthDecoding(init({ 0x81, 0x82, 0x83, 0x04 }), 0x1234, 0, false);

      return failed;
   }

   int testIntegerDecoding(const uint8_t *pBuffer, int64_t expected) {
      SmlParser::_pPacket = pBuffer;
      SmlParser::_packetLength = 100;
      int pos = 0;

      int64_t value = getNextValue(pos);

      printf("%s: Value expected %ld, got %ld\n",
             expected == value ? "OK" : "ERROR",
             expected, value);

      return (value == expected) ? 0 : 1;
   }

   int testLengthDecoding(const uint8_t *pBuffer, uint16_t expectedLength, int expectedPos, bool updatePos) {
      SmlParser::_pPacket = pBuffer;
      SmlParser::_packetLength = 100;
      int pos = 0;

      uint16_t length = getLength(pos, updatePos);

      bool testOk = (length == expectedLength) && (pos == expectedPos);
      printf("%s: Length expected %u, got %u, position expected %d, got %d)\n",
             testOk ? "OK" : "ERROR",
             expectedLength, length,
             expectedPos, pos);

      return testOk ? 0 : 1;
   }

private:
   uint8_t _buffer[100];

   const uint8_t* init(std::initializer_list<uint8_t> list) {
      int count = 0;
      for (auto v : list) {
         _buffer[count++] = v;
      }
      return _buffer;
   }
};

int checkResult(uint32_t powerInW, uint32_t powerOutW, uint64_t energyWh, uint32_t ok, uint32_t errors) {
   smlParser.parsePacket(smlPacket, SML_TEST_PACKET_LENGTH);

   bool testOk = (smlParser.getPowerInW() == powerInW) &&
         (smlParser.getPowerOutW() == powerOutW) &&
         (smlParser.getEnergyInWh() == energyWh) &&
         (smlParser.getParsedOk() == ok) &&
         (smlParser.getParseErrors() == errors);

   printf("%s: %.2fW %.2fW %.2fWh %d %d\n", testOk ? "OK" : "ERROR",
          smlParser.getPowerInW()/100.0,
          smlParser.getPowerOutW()/100.0,
          smlParser.getEnergyInWh()/100.0,
          smlParser.getParsedOk(),
          smlParser.getParseErrors());

   return testOk ? 0 : 1;
}

int main(int argc, char **argv) {
   int failed = 0;

   BaseDecodeTests baseDecodeTests;
   failed += baseDecodeTests.run();

   SmlParser smlParser;

   memcpy(smlPacket, SML_TEST_PACKET, SML_TEST_PACKET_LENGTH);

   failed += checkResult(18554U,0U,25213320UL,1U,0U);

   smlPacket[0] ^= 0xff;
   failed += checkResult(18554U,0U,25213320UL,1U,1U);
   smlPacket[0] ^= 0xff;

   smlPacket[4] ^= 0xff;
   failed += checkResult(18554U,0U,25213320UL,1U,2U);
   smlPacket[4] ^= 0xff;

   smlPacket[5] ^= 0xff;
   failed += checkResult(18554U,0U,25213320UL,1U,3U);
   smlPacket[5] ^= 0xff;

   smlPacket[30] ^= 0xff;
   failed += checkResult(18554U,0U,25213320UL,1U,4U);
   smlPacket[30] ^= 0xff;

   smlPacket[212] = 0xc8; // Value
   smlPacket[213] = 0x7a; // Value
   smlPacket[218] = 0xbd; // Checksum 1
   smlPacket[219] = 0x70; // Checksum 2
   failed += checkResult(0U,14214U,25213320UL,2U,4U);

   if (failed == 0) {
      printf("ALL TESTS PASSED.\n");
   }
   else {
      printf("%d TEST(S) FAILED.\n",failed);
   }

   return 0;
}
