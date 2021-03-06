#ifndef SML_STREAM_READER_H
#define SML_STREAM_READER_H

#include "crc16ccitt.h"
#include "util/sml_demodata.h"
#include "smlparser.h"

class SmlStreamReader {
public:
   SmlStreamReader(int maxPacketSize) : 
      _currentState(&SmlStreamReader::stateReadData),
      _maxPacketSize(maxPacketSize),
      _escLen(0), 
      _escData(0U), 
      _parseErrors(0U),
      _packetPos(0) 
   {
      _data = new uint8_t[_maxPacketSize];
   }

   ~SmlStreamReader() {
      delete[] _data;
   }

   inline const uint8_t *getData() { return _data; }

   inline int getLength() { return _packetLength; }

   inline uint16_t getCrc16() { return _crc16Expected; }

   inline uint32_t getParseErrors() const { return _parseErrors; }

   int addData(const uint8_t *pData, int length) {
      for (int i = 0; i < length; ++i) {
         _crc16.calc(pData[i]);
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

   bool(SmlStreamReader::*_currentState)(uint8_t);
   int _maxPacketSize;
   int _escLen;
   uint32_t _escData;
   uint32_t _parseErrors;
   int _packetPos;
   int _packetLength;
   uint16_t _crc16Expected;
   uint8_t *_data;
   Crc16Ccitt _crc16;

   void startPacket() {
      _packetPos = 0;
      _escLen = 0;
      _crc16.init(0x91dc);
   }

   bool stateReadData(uint8_t currentByte) {
      if (_packetPos >= _maxPacketSize) {
         ++_parseErrors;
         startPacket();
      }
      _data[_packetPos++] = currentByte;
      if (currentByte == 0x1b) {
         if (++_escLen == 4) {
            _packetPos -= 4;
            _currentState = &SmlStreamReader::stateReadEsc;
            _crc16Expected = _crc16.getCrcState();
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
            _crc16.init(_crc16Expected);
            _crc16.calc(0x1a);
            _crc16.calc(spareBytes);
            _crc16Expected = _escData & SML_CRC_MASK;
            if (_crc16Expected != _crc16.getCrc()) {
               //printf("Reader: Warning %04x != %04x\n", _crc16Expected, crc16.getCrc());
               ++_parseErrors;
               return false;
            }
            return true;
         }
      }
      return false;
   }
};

#endif
