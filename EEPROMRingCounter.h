//
// For increasing unsigned long values ONLY !
//
#pragma once
#include <inttypes.h>
class EEPROMRingCounter
 {
 public:
  EEPROMRingCounter(unsigned int FirstAddress, unsigned int NumCells, bool DebugSerialOutput = false);
  unsigned long init();               // Finds and returns the last (maximum) stored counter value 
  unsigned long get();                // Returns the last stored counter value
  boolean set(unsigned long p_value); // Returns false if the last saved counter value is greater than the new one.
                                      // If the new counter value is less than or equal to the last saved one,
                                      // it is ignored
  void clear();                       // Fills the EEPROM area from _FirstAddress to _LastAddress+_DataSize
                                      // with FF values
 private:
  const unsigned int _DataSize = 4;
  const unsigned long _MaxDataValue = 4294967295;
  unsigned int _FirstAddress;
  unsigned int _NumCells;
  unsigned int _curAddress;
  unsigned int _LastAddress;
  unsigned long readValue(unsigned int p_address);
  void writeValue(unsigned int p_address, unsigned long p_value);
  bool _Debug;
 };
