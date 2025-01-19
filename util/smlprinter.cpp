#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "sml_testpacket.h"
#include "sml_demodata.h"
#include "../smlstreamreader.h"

const uint8_t* getOctetString(uint8_t *pBuffer, int *pBufferSize, const uint8_t *pPacket, const int length) {
   ++pPacket;
   *pBufferSize = (length < *pBufferSize) ? length : *pBufferSize;
   for (int i = 0; i < *pBufferSize; ++i) {
      *(pBuffer++) = pPacket[i];
   }
   return pPacket + length;
}

const uint8_t* getInt(int64_t *pValue, const uint8_t *pPacket, const int length) {
   ++pPacket;
   *pValue = 0;
   for (int i = 1; i < length; ++i) {
      *pValue = (*pValue << 8) | *(pPacket++);
   }
   return pPacket;
}

const uint8_t* getUint(uint64_t *pValue, const uint8_t *pPacket, const int length) {
   ++pPacket;
   *pValue = 0;
   for (int i = 1; i < length; ++i) {
      *pValue = (*pValue << 8) | *(pPacket++);
   }
   return pPacket;
}

const uint8_t* getBool(uint8_t *pValue, const uint8_t *pPacket, const int length) {
   ++pPacket;
   *pValue = *(pPacket++);
   return pPacket;
}

const uint8_t* printHex(const uint8_t *pPacket, const int length, const int depth, const char* pMessage) {
   for (int i = 0; i < depth; ++i) {
      printf("   ");
   }
   for (int i = 0; i < length; ++i) {
      printf("%02x ", *(pPacket++));
   }
   int indent = 50 - 3 * (depth + length);
   for (int i = 0; i < indent; ++i) {
      printf(" ");
   }
   printf("%s\n", pMessage);
   return pPacket;
}

const uint8_t* printString(const uint8_t *pPacket, const int length, const int depth) {
   char s[100] = { 0 };
   strncpy(s, "string = ", sizeof(s));
   for (int i = 1; i < length; ++i) {
      char c = pPacket[i];
      s[i + 8] = (c >= ' ' && c <= 'Z') ? c : '.';
   }
   return printHex(pPacket, length, depth, s);
}

#define SML_TAG_MASK 0x70
#define SML_MORE_FLAG 0x80
#define SML_LENGTH_MASK 0x0F
#define SML_OCTED_ID 0x00
#define SML_BOOL_ID 0x40
#define SML_INT_ID 0x50
#define SML_UINT_ID 0x60
#define SML_LIST_ID 0x70

void parseSml(const uint8_t* pPacket) {
   char message[100];
   int listStack[10] = { 0 };
   int depth = 1;
   listStack[1] = 1;
   do {
      // Take current element from list-stack
      listStack[depth]--;
      while ((listStack[depth] == 0) && (depth > 0)) {
         depth--;
         listStack[depth]--;
      }

      // Identify type and length
      uint8_t type = *pPacket & SML_TAG_MASK;
      int16_t length = *pPacket & SML_LENGTH_MASK;
      while (*pPacket & SML_MORE_FLAG) {
         length = (length << 4) | (*(++pPacket) & SML_LENGTH_MASK);
      }

      // Handle the current type
      switch (type) {
      case SML_OCTED_ID: {
         if (length == 0) {
            pPacket = printHex(pPacket, 1, depth, "endOfMessage");
         }
         else if (length == 1) {
            pPacket = printHex(pPacket, 1, depth, "optional, not used");
         }
         else {
            pPacket = printString(pPacket, length, depth);
         }
         break;
      }
      case SML_BOOL_ID: {
         uint8_t value;
         getBool(&value, pPacket, length);
         snprintf(message, sizeof(message), "bool = %d", value);
         pPacket = printHex(pPacket, length, depth, message);
         break;
      }
      case SML_INT_ID: {
         int64_t value;
         getInt(&value, pPacket, length);
         snprintf(message, sizeof(message), "int = %" PRId64, value);
         pPacket = printHex(pPacket, length, depth, message);
         break;
      }
      case SML_UINT_ID: {
         uint64_t value;
         getUint(&value, pPacket, length);
         snprintf(message, sizeof(message), "uint = %" PRIu64, value);
         pPacket = printHex(pPacket, length, depth, message);
         break;
      }
      case SML_LIST_ID: {
         pPacket = printHex(pPacket, 1, depth, "list");
         listStack[++depth] = length + 1;
         break;
      }
      }
   } while (depth > 0);
}

void parseFile(int argc, char** argv) {
   printf("Parsing file: %s\n", argv[1]);

   SmlStreamReader smlReader(1000, false);
   int parsed = 0;
   FILE* pFile = fopen(argv[1], "rb");
   if (pFile) {
      uint8_t buffer[1];
      while (!feof(pFile)) {
         int bytesRead = fread(buffer, 1, sizeof(buffer), pFile);
         if (smlReader.addData(buffer, bytesRead) > 0) {
            parsed++;
            printf("Packet %d, size: %d\n", parsed, smlReader.getLength());
            //printHex(smlReader.getData(), smlReader.getLength(), 0, "");
            parseSml(smlReader.getData());
         }
      }
   }
}

int main(int argc, char** argv) {
   if (argc > 1) {
      if (strcmp(argv[1], "demo") == 0) {
         // Parse demo packet without start-marker and version (1b1b1b1b 01010101)
         parseSml(SML_DATA[1].data + 8);
      }
      else {
         parseFile(argc, argv);
      }
   }
   else {
      printf("Usage: smlprinter demo\n");
      printf("       smlprinter <filename>\n");
   }
   return 0;
}
