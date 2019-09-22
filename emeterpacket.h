#ifndef EMETERPACKET_H
#define EMETERPACKET_H

#include <stdint.h>
#include <string.h>

/**
* @brief Class to create SMA energy meter packets.
*
* For details see:
* https://www.sma.de/fileadmin/content/global/Partner/Documents/SMA_Labs/EMETER-Protokoll-TI-de-10.pdf
*/
class EmeterPacket {
public:
   // IDs to identify values in the energy meter packets
   static const uint32_t SMA_POSITIVE_ACTIVE_POWER = 0x00010400;
   static const uint32_t SMA_POSITIVE_REACTIVE_POWER = 0x00030400;
   static const uint32_t SMA_NEGATIVE_ACTIVE_POWER = 0x00020400;
   static const uint32_t SMA_NEGATIVE_REACTIVE_POWER = 0x00040400;
   static const uint32_t SMA_POSITIVE_ENERGY = 0x00010800;
   static const uint32_t SMA_NEGATIVE_ENERGY = 0x00020800;
   static const uint32_t SMA_VERSION = 0x90000000;

   /**
   * @brief Constructor
   */
   EmeterPacket(uint32_t serNo = 0U) {
      initEmeterPacket(serNo);
      begin(0U);
      end();
   }

   /**
   * @brief Initialize the packet with the given serial number
   */
   void init(uint32_t serNo) {
      initEmeterPacket(serNo);
   }

   /**
   * @brief Begin the update sequence
   */
   void begin(unsigned long timeStampMs) {
      _pPacketPos = meterPacket + _headerLength;
      storeU32BE(_pMeterTime, timeStampMs);
      // Initial length of packet (ID + SN + TS)
      _length = INITIAL_PAYLOAD_LENGTH;
   }

   /**
   * @brief Add a measurement value (32 bit)
   */
   void addMeasurementValue(uint32_t id, uint32_t value) {
      _pPacketPos = storeU32BE(_pPacketPos, id);
      _pPacketPos = storeU32BE(_pPacketPos, value);
      _length += 8;
   }

   /**
   * @brief Add a counter value (64 bit)
   */
   void addCounterValue(uint32_t id, uint64_t value) {
      _pPacketPos = storeU32BE(_pPacketPos, id);
      _pPacketPos = storeU64BE(_pPacketPos, value);
      _length += 12;
   }

   /**
   * @brief End the update sequence
   */
   uint16_t end() {
      // Store version
      _pPacketPos = storeU32BE(_pPacketPos, SMA_VERSION);
      _pPacketPos = storeU32BE(_pPacketPos, 0x01020452);
      _length += 8;

      // Update length
      storeU16BE(_pDataSize, _length);

      // Add end-tag
      storeU32BE(_pPacketPos, 0);
      _length += 4;

      // Calculate final length
      _length = _headerLength + _length - INITIAL_PAYLOAD_LENGTH;
      return _length;
   }

   /**
   * @brief Get the data of the current packet
   */
   const uint8_t *getData() const {
      return meterPacket;
   }

   /**
   * @brief Get the length of the current packet
   */
   uint16_t getLength() const {
      return _length;
   }

private:
   // Initial length of the payload (Protocol-ID + SRC + Time)
   static const int INITIAL_PAYLOAD_LENGTH = 12;

   // Buffer to store the energy-meter packet
   static const int METER_PACKET_SIZE = 1000;
   uint8_t meterPacket[METER_PACKET_SIZE];

   // Length of the energy meter packet header
   uint16_t _headerLength;

   // Pointer to the data-length field in the energy meter packet
   uint8_t *_pDataSize;

   // Pointer to the time field in the energy meter packet
   uint8_t *_pMeterTime;

   // Current position in the packet
   uint8_t *_pPacketPos;

   // Current length of the packet
   uint16_t _length;

   /**
   * @brief Store an U16 in big endian byte order
   */
   uint8_t *storeU16BE(uint8_t *pPos, uint16_t value) {
      *(pPos++) = value >> 8;
      *(pPos++) = value & 0xff;
      return pPos;
   }

   /**
   * @brief Store an U32 in big endian byte order
   */
   uint8_t *storeU32BE(uint8_t *pPos, uint32_t value) {
      pPos = storeU16BE(pPos, value >> 16);
      return storeU16BE(pPos, value & 0xffff);
   }

   /**
   * @brief Store an U64 in big endian byte order
   */
   uint8_t *storeU64BE(uint8_t *pPos, uint64_t value) {
      pPos = storeU32BE(pPos, value >> 32);
      return storeU32BE(pPos, value & 0xffffffff);
   }

   /**
   * @brief Find the offset of the given identifier
   */
   uint8_t* offsetOf(uint8_t *pData, uint8_t identifier, int size) {
      for (int i = 0; i < size; ++i) {
         if (pData[i] == identifier) {
            return pData + i;
         }
      }
      return 0;
   }

   /**
   * @brief Initialize energy meter packet
   */
   void initEmeterPacket(uint32_t serNo) {
      // IDs to identify offsets in the SMA meter header
      const uint8_t WLEN = 0xfa;
      const uint8_t DSRC = 0xfb;
      const uint8_t DTIM = 0xfc;

      // Protocol-header for SMA energy meter packet
      uint8_t SMA_METER_HEADER[] = { 'S', 'M', 'A', 0,                               // Identifier
                                   0x00, 0x04, 0x02, 0xa0, 0x00, 0x00, 0x00, 0x01,   // Group 1
                                   WLEN, WLEN, 0x00, 0x10, 0x60, 0x69,               // Start of protocol 0x6069
                                   0x01, 0x0e, DSRC, DSRC, DSRC, DSRC,               // Source address
                                   DTIM, DTIM, DTIM, DTIM };                         // Timestamp

      _headerLength = sizeof(SMA_METER_HEADER);
      memcpy(meterPacket, SMA_METER_HEADER, _headerLength);

      _pDataSize = offsetOf(meterPacket, WLEN, _headerLength);
      _pMeterTime = offsetOf(meterPacket, DTIM, _headerLength);

      uint8_t *pSerNo = offsetOf(meterPacket, DSRC, _headerLength);
      storeU32BE(pSerNo, serNo);
   }
};

#endif // EMETERPACKET_H
