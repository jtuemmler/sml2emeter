#ifndef SML_STREAM_READER_H
#define SML_STREAM_READER_H

#include "crc16ccitt.h"
#include "util/sml_demodata.h"
#include "smlparser.h"

class SmlStreamReader {
public:
   SmlStreamReader() : _currentState(&SmlStreamReader::stateReadData), _escLen(0), _escData(0U), _packetPos(0) {}

   inline const uint8_t *getData() {
      return _data;
   }

   inline int getLength() {
      return _packetLength;
   }

   inline uint16_t getCrc16() {
      return _crc16;
   }

   int addData(const uint8_t *pData, int length) {
      for (int i = 0; i < length; ++i) {
         crc16.calc(pData[i]);
         if ((this->*_currentState)(pData[i])) {
            return i + 1;
         }
      }
      return -1;
   }

private:
   static const uint8_t STATE_READ_DATA = 0;
   static const uint8_t STATE_READ_ESC = 1;
   static const uint32_t SML_ESC = 0x1b1b1b1b;
   static const uint32_t SML_BEGIN_VERSION1 = 0x01010101;
   static const uint32_t SML_END = 0x1a000000;
   static const uint32_t SML_END_MASK = 0xff000000;
   static const uint32_t SML_SPARE_MASK = 0x00ff0000;
   static const uint32_t SML_CRC_MASK = 0x0000ffff;
   static const int MAX_PACKET_SIZE = 500;

   void startPacket() {
      _packetPos = 0;
      _escLen = 0;
      crc16.init(0x91dc);
   }

   bool stateReadData(uint8_t currentByte) {
      if (_packetPos >= MAX_PACKET_SIZE) {
         startPacket();
      }
      _data[_packetPos++] = currentByte;
      if (currentByte == 0x1b) {
         if (++_escLen == 4) {
            _packetPos -= 4;
            _currentState = &SmlStreamReader::stateReadEsc;
            _crc16 = crc16.getCrcState();
         }
      }
      else {
         _escLen = 0;
      }
      return false;
   }

   bool stateReadEsc(uint8_t currentByte) {
      _escData = (_escData << 8) | currentByte;
      if (--_escLen <= 0) {
         _currentState = &SmlStreamReader::stateReadData;
         if (_escData == SML_BEGIN_VERSION1) {
            startPacket();
         }
         if (_escData == SML_ESC) {
            _packetPos += 4;
         }
         if ((_escData & SML_END_MASK) == SML_END) {
            int spareBytes = ((_escData & SML_SPARE_MASK) >> 16);
            _packetLength = _packetPos - spareBytes;
            crc16.init(_crc16);
            crc16.calc(0x1a);
            crc16.calc(spareBytes);
            _crc16 = _escData & SML_CRC_MASK;
            if (_crc16 != crc16.getCrc()) {
               printf("Warning %04x != %04x\n", _crc16, crc16.getCrc());
               return false;
            }
            return true;
         }
      }
      return false;
   }

   bool(SmlStreamReader::*_currentState)(uint8_t);
   int _escLen;
   uint32_t _escData;
   int _packetPos;
   int _packetLength;
   uint16_t _crc16;
   uint8_t _data[MAX_PACKET_SIZE];
   Crc16Ccitt crc16;
};


#endif
