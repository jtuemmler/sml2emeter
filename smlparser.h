#ifndef SMLPARSER_H
#define SMLPARSER_H

#include <inttypes.h>
#include "crc16ccitt.h"

/**
 * @brief Parser to read power and energy-values from a SML packet
 */
class SmlParser {
public:
   /// Constructor
   SmlParser() : _parsedOk(0U), _parseErrors(0U), _powerInW(0U), _powerOutW(0U), _energyInWh(0UL), _energyOutWh(0UL),
      _pPacket(NULL), _packetLength(0) {}

   /// Number of successfully parsed packets.
   inline uint32_t getParsedOk() const { return _parsedOk; }

   /// Number of parse errors.
   inline uint32_t getParseErrors() const { return _parseErrors; }
   
   /// Imported power in centi W (1cW = 0.01W)
   inline uint32_t getPowerIn() const { return _powerInW; }

   /// Exported power in centi W (1cW = 0.01W)
   inline uint32_t getPowerOut() const { return _powerOutW; }

   /// Imported energy in centi Wh (1cW = 0.01Wh)
   inline uint64_t getEnergyIn() const { return _energyInWh; }

   /// Exported energy in centi Wh (1cW = 0.01Wh)
   inline uint64_t getEnergyOut() const { return _energyOutWh; }

   /**
    * @brief Parse a SML packet
    * @param pPacket       Packet to parse
    * @param packetLength  Length of the packet in bytes
    * @return true, if the packet could be parsed successfully
    */
   bool parsePacket(const uint8_t *pPacket, int packetLength) {
      _pPacket = pPacket;
      _packetLength = packetLength;

      int pos = 0;

      while (pos < packetLength) {
         if (pPacket[pos] == SML_END_OF_MESSAGE) {
            break;
         }
         int messageStart = pos;
         getLength(pos);      // Skip list identifier of message
         int messageBody = getNextElement(pos, 3);
         getNextElement(pos); // Skip to crc
         int messageLength = pos - messageStart;
         uint16_t crc16Expected = (uint16_t)getNextIntValue(pos);

         // Check crc
         _crc16.init();
         _crc16.calc(pPacket + messageStart, messageLength);
         if (crc16Expected == _crc16.getCrc()) {
            if (parseMessageBody(messageBody)) {
               ++_parsedOk;
               return true;
            }
            // Skip 'end of message'
            ++pos;
         }
         else {
            //printf("Parser: Warning %04x != %04x\n", crc16Expected, _crc16.getCrc());
            ++_parseErrors;
            return false;
         }
      }
      // Incomplete or empty packet.
      return false;
   }

protected:
   // ----------------------------------------------------------------------------
   // SML constants
   // For details see:
   // https://www.bsi.bund.de/SharedDocs/Downloads/DE/BSI/Publikationen/TechnischeRichtlinien/TR03109/TR-03109-1_Anlage_Feinspezifikation_Drahtgebundene_LMN-Schnittstelle_Teilb.pdf?__blob=publicationFile
   // ----------------------------------------------------------------------------
   static const uint8_t SML_MORE_FLAG = 0x80;
   static const uint8_t SML_TAG_MASK = 0x70;
   static const uint8_t SML_LENGTH_MASK = 0x0F;
   static const uint8_t SML_OCTED_ID = 0x00;
   static const uint8_t SML_BOOL_ID = 0x40;
   static const uint8_t SML_INT_ID = 0x50;
   static const uint8_t SML_UINT_ID = 0x60;
   static const uint8_t SML_LIST_ID = 0x70;
   static const uint8_t SML_END_OF_MESSAGE = 0x00;

   static const uint16_t SML_GET_LIST_RES = 0x0701;

   static const int SML_MIN_SCALE = -2;
   static const int SML_MAX_SCALE = 5;
   static const int SML_SCALE_VALUES = SML_MAX_SCALE - SML_MIN_SCALE + 1;
   static const int32_t SCALE_FACTORS[SML_SCALE_VALUES];

   // ----------------------------------------------------------------------------
   // OBIS constants
   //
   // For details see:
   // https://www.promotic.eu/en/pmdoc/Subsystems/Comm/PmDrivers/IEC62056_OBIS.htm
   // https://www.bundesnetzagentur.de/DE/Service-Funktionen/Beschlusskammern/BK06/BK6_81_GPKE_GeLi/Mitteilung_Nr_20/Anlagen/Obis-Kennzahlen-System_2.0.pdf?__blob=publicationFile&v=2
   // ----------------------------------------------------------------------------
   static const uint8_t OBIS_TARIFF = 0;
   static const uint8_t OBIS_INSTANTANEOUS_POWER_TYPE = 7;
   static const uint8_t OBIS_ENERGY_TYPE = 8;
   static const uint8_t OBIS_POSITIVE_ACTIVE_POWER = 1;
   static const uint8_t OBIS_NEGATIVE_ACTIVE_POWER = 2;
   static const uint8_t OBIS_SUM_ACTIVE_POWER = 16;

   uint32_t _parsedOk;
   uint32_t _parseErrors;

   uint32_t _powerInW;
   uint32_t _powerOutW;

   uint64_t _energyInWh;
   uint64_t _energyOutWh;

   Crc16Ccitt _crc16;

   const uint8_t *_pPacket;
   int _packetLength;

   /**
    * @brief Parse the message body and store parsed information
    * @param pos  Position in the packet
    */
   bool parseMessageBody(int pos) {
      getLength(pos);            // Skip list identifier of message
      uint16_t message = (uint16_t)getNextIntValue(pos);
      if (message != SML_GET_LIST_RES) {
         return false;
      }
      getLength(pos);            // Skip list identifier of message body
      getNextElement(pos, 4);    // Skip first 4 entries of GET_LIST_RES message
      int listElements = getLength(pos);
      for (int i = 0; i < listElements; ++i) {
         getLength(pos);         // Skip the list identifier of the SML_LIST_ENTRY
         pos += 3;               // Skip the beginning of the value-identifier
         uint8_t index = _pPacket[pos++];
         uint8_t type = _pPacket[pos++];
         uint8_t tariff = _pPacket[pos++];
         ++pos;                  // Skip next value
         getNextElement(pos, 2);  // Skip status and timestamp
         getNextIntValue(pos);      // Skip unit
         int8_t scale = (int8_t)getNextIntValue(pos);
         int64_t value = (int64_t)getNextIntValue(pos);
         getNextElement(pos);    // Skip signature

         // Store the value in fields
         if ((tariff == OBIS_TARIFF) && (scale >= SML_MIN_SCALE) && (scale <= SML_MAX_SCALE)) {
            value *= SCALE_FACTORS[scale - SML_MIN_SCALE];
            switch (type) {
            case OBIS_INSTANTANEOUS_POWER_TYPE:
               switch (index) {
               case OBIS_POSITIVE_ACTIVE_POWER:
                  _powerInW = (uint32_t)value;
                  break;
               case OBIS_NEGATIVE_ACTIVE_POWER:
                  _powerOutW = (uint32_t)value;;
                  break;
               case OBIS_SUM_ACTIVE_POWER:
                  _powerInW = (uint32_t)(value >= 0 ? value : 0U);
                  _powerOutW = (uint32_t)(value <= 0 ? -value : 0U);
                  break;
               }
               break;

            case OBIS_ENERGY_TYPE:
               switch (index) {
               case OBIS_POSITIVE_ACTIVE_POWER:
                  _energyInWh = (uint64_t)value;
                  break;
               case OBIS_NEGATIVE_ACTIVE_POWER:
                  _energyOutWh = (uint64_t)value;
                  break;
               }
               break;
            }
         }
      }
      return true;
   }

   /**
    * @brief Get the type of the current element
    * @param pos  Position in the packet
    * @return The type of the current element
    */
   inline uint8_t getType(int pos) const { return _pPacket[pos] & SML_TAG_MASK; }

   /**
    * @brief Get the length of the current element
    * @param pos  Position in the packet
    * @param updatePosition Flag that indicates wether the position should be updated
    * @return The length of the current element in bytes
    */
   uint16_t getLength(int &pos, bool updatePosition = true) const {
      int localPos = pos;

      uint16_t result = _pPacket[localPos] & SML_LENGTH_MASK;
      while ((_pPacket[localPos] & SML_MORE_FLAG) && (localPos < _packetLength)) {
         result = (result << 4) | (_pPacket[++localPos] & SML_LENGTH_MASK);
      }
      ++localPos;

      if (updatePosition) {
         pos = localPos;
      }
      return result;
   }

   /**
    * @brief Get the position of the next element
    * @param pos  Position in the packet (will be updated!)
    * @param elementsToRead Elements to skip
    * @return The position of the next element in the packet
    */
   int getNextElement(int &pos, int elementsToRead = 1) {
      while ((elementsToRead > 0) && (pos < _packetLength)) {
         --elementsToRead;
         uint8_t elementType = getType(pos);
         uint16_t elementLength = getLength(pos, false);

         if (elementType == SML_LIST_ID) {
            ++pos;
            elementsToRead += elementLength;
         }
         else {
            pos += elementLength;
         }
      }
      return pos;
   }

   /**
    * @brief Get the next integer value (supports both, uint and int types)
    * @param pos  Position in the packet (will be updated!)
    * @return The value as uint64
    * @note This function returns 0 if the element-type is invalid
    */
   int64_t getNextIntValue(int &pos) {
      uint8_t elementType = getType(pos);
      uint16_t elementLength = getLength(pos);
      int64_t value = 0UL;
      if (((elementType == SML_INT_ID) || (elementType == SML_UINT_ID)) && elementLength > 1) {
         value = (elementType == (int64_t)(SML_INT_ID) ? (int8_t)_pPacket[pos++] : _pPacket[pos++]);
         for (int i = 2; i < elementLength; ++i) {
            value = (value << 8) | _pPacket[pos++];
         }
      }
      else {
         pos += elementLength - 1;
      }
      return value;
   }
};

// Scale factors
// Note: Exponent of the scale factors is shifted by 2 (10^0 becomes 10^2, etc.)
//       This is done to keep integer calculations as long as possible.
//       As a result, all values are returned as Centi-W or Centi-Wh.
//                                                                     -2  -1    0     1      2       3        4         5
const int32_t SmlParser::SCALE_FACTORS[SmlParser::SML_SCALE_VALUES] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000 };

#endif // SMLPARSER_H
