#include <stdio.h>
#include <stdint.h>
#include "util/sml_demodata.h"
#include "smlstreamreader.h"
#include "smlparser.h"

void printInfo(SmlStreamReader &reader, int result) {
   if (result >= 0) {
      printf("Length = %d, CRC16 = %04x\n", reader.getLength(), reader.getCrc16());
      for (int i = 0; i < reader.getLength(); ++i) {
         printf("%02x ", reader.getData()[i]);
      }
      printf("\n");
   }
   else {
      printf("No valid packet!\n");
   }
}

int testSmlTestPacket(SmlStreamReader &reader) {
   int result = -1;
   for (int i = 0; i < SML_DATA[0].length; ++i) {
      result = reader.addData(SML_DATA[0].data + i, 1);
   }
   return result;
}

int testEscPacket(SmlStreamReader &reader) {
   uint8_t data[] = { 0x1b, 0x1b, 0x1b, 0x1b, 0x01, 0x01, 0x01, 0x01, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1a, 0x00, 0x94, 0xfc };
   int result = reader.addData(data, 24);
   return result;
}

int testDataPacket(SmlStreamReader &reader, int length) {
   uint8_t data[] = { 0x1b, 0x1b, 0x1b, 0x1b, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x03, 0x04, 0x1b, 0x1b, 0x1b, 0x1b, 0x1a, 0x00, 0x00, 0x00 };
   data[17] = 4 - length;
   Crc16Ccitt crc16;
   crc16.init();
   crc16.calc(data, 18);
   data[18] = crc16.getCrc() >> 8;
   data[19] = crc16.getCrc() & 0xff;
   int result = reader.addData(data, 20);
   return result;
}

int main(int argc, char ** argv) {
   SmlStreamReader reader;
   SmlParser parser;

   printInfo(reader, testSmlTestPacket(reader));
   printInfo(reader, testEscPacket(reader));
   printInfo(reader, testDataPacket(reader, 1));
   printInfo(reader, testDataPacket(reader, 2));
   printInfo(reader, testDataPacket(reader, 3));

   for (int i = 0; i < SML_DATA_LENGTH; ++i) {
      int offset = 0;
      do {
         offset = reader.addData(SML_DATA[i].data + offset, SML_DATA[i].length - offset);
         if (offset >= 0) {
            if (parser.parsePacket(reader.getData(), reader.getLength())) {
               printf("%d. %s: Parsed OK: %d %ld %d %ld\n", i, SML_DATA[i].name, 
                  parser.getPowerInW(), parser.getEnergyInWh(),
                  parser.getPowerOutW(), parser.getEnergyOutWh());
            }
            else {
               printf("%d. %s: Error\n", i, SML_DATA[i].name);
            }
         }
      } while (offset >= 0);
   } 

   return 0;
}
