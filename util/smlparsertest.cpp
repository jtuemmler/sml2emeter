#include <stdio.h>
#include <string.h>
#include "crc16ccitt.h"
#include "smlparser.h"
#include "sml_testpacket.h"

SmlParser smlParser;
uint8_t smlPacket[SML_TEST_PACKET_LENGTH];

void checkResult(uint32_t powerInW, uint32_t powerOutW, uint64_t energyWh, uint32_t ok, uint32_t errors) {
   smlParser.parsePacket(smlPacket, SML_TEST_PACKET_LENGTH);

   printf("%.2fW %.2fW %.2fWh %d %d\n", smlParser.getPowerInW()/100.0, smlParser.getPowerOutW()/100.0, smlParser.getEnergyInWh()/100.0, smlParser.getParsedOk(), smlParser.getParseErrors());

   if ((smlParser.getPowerInW() == powerInW) &&
       (smlParser.getPowerOutW() == powerOutW) &&
       (smlParser.getEnergyInWh() == energyWh) &&
       (smlParser.getParsedOk() == ok) &&
       (smlParser.getParseErrors() == errors)) {
      printf("OK\n");
   }
   else {
      printf("ERROR!\n");
   }
}
int main(int argc, char **argv) {
   SmlParser smlParser;

   memcpy(smlPacket, SML_TEST_PACKET, SML_TEST_PACKET_LENGTH);

   checkResult(18554U,0U,25213320UL,3U,0U);

   smlPacket[0] ^= 0xff;
   checkResult(18554U,0U,25213320UL,3U,1U);

   smlPacket[0] ^= 0xff;
   smlPacket[4] ^= 0xff;
   checkResult(18554U,0U,25213320UL,3U,2U);

   smlPacket[4] ^= 0xff;
   smlPacket[5] ^= 0xff;
   checkResult(18554U,0U,25213320UL,3U,3U);

   smlPacket[5] ^= 0xff;
   smlPacket[30] ^= 0xff;
   checkResult(18554U,0U,25213320UL,3U,4U);

   smlPacket[30] ^= 0xff;
   smlPacket[212] = 0xc8;
   smlPacket[218] = 0xbd;
   smlPacket[219] = 0x70;
   checkResult(0U,14214U,25213320UL,6U,4U);

   return 0;
}
