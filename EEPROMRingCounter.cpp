#include "Arduino.h"
#include <EEPROM.h> 
#include "EEPROMRingCounter.h"

EEPROMRingCounter::EEPROMRingCounter(unsigned int FirstAddress, unsigned int NumCells, bool DebugSerialOutput)
{
_FirstAddress = FirstAddress;  
_NumCells = NumCells;
_Debug = DebugSerialOutput;
_LastAddress = (_FirstAddress + ((_NumCells - 1) * _DataSize));
_curAddress = 0;
}

unsigned long EEPROMRingCounter::readValue(unsigned int p_address) 
{    
uint8_t raw[_DataSize];
for ( uint8_t i = 0; i < _DataSize; i++ ) { raw[i] = EEPROM.read(p_address + i); }
unsigned long &ret = (unsigned long&)raw;
if ( ret == _MaxDataValue ) { ret = 0; } // maximum value is returned as minimum value
return ret;
}

void EEPROMRingCounter::writeValue(unsigned int p_address, unsigned long p_value)
{
if ( readValue(p_address) == p_value ) { return; }  
uint8_t raw[_DataSize];
(unsigned long&)raw = p_value;
for ( uint8_t i = 0; i < _DataSize; i++ ) { EEPROM.write(p_address + i, raw[i]); }
EEPROM.commit();
}

unsigned long EEPROMRingCounter::init()
{
unsigned long prevValue = 0;
unsigned long curValue = 0;
for ( unsigned int addr = _FirstAddress; addr <= _LastAddress; addr = addr + _DataSize )
 {
 if ( addr == _FirstAddress ) { prevValue = readValue(_LastAddress); }
 curValue = readValue(addr); 
 if ( prevValue >= curValue )
  {
  _curAddress = addr - _DataSize;
  if ( _curAddress < _FirstAddress ) { _curAddress = _LastAddress; }
  curValue = prevValue; 
  if ( _Debug ) 
   {
   Serial.print(F("INI: "));
   Serial.print((_curAddress - _FirstAddress) / _DataSize);
   Serial.print(F(" "));
   Serial.print(_curAddress);
   Serial.print(F(" "));
   Serial.println(curValue);
   }
  break;
  }
 prevValue = curValue; 
 }
return curValue; 
}

unsigned long EEPROMRingCounter::get()
{
if ( _Debug ) 
 {
 Serial.print(F("GET: "));
 Serial.print((_curAddress - _FirstAddress) / _DataSize);
 Serial.print(F(" "));
 Serial.print(_curAddress);
 Serial.print(F(" "));
 Serial.println(readValue(_curAddress));
 }
return readValue(_curAddress); 
}

boolean EEPROMRingCounter::set(unsigned long p_value)
{
bool ret = true; // returns false if prevValue > p_value
unsigned long prevValue = readValue(_curAddress);
if ( _Debug ) { Serial.print(F("SET: ")); }
if ( prevValue > p_value )
 {
 if ( _Debug ) 
  {
  Serial.print((_curAddress - _FirstAddress) / _DataSize);
  Serial.print(F(" "));
  Serial.print(_curAddress);
  Serial.print(F(" "));
  Serial.print(p_value);
  Serial.println(F(" ignored : prevValue >= p_value"));
  }
 ret = false;
 } 
else if ( prevValue == p_value )
 {
 if ( _Debug )
  {
  Serial.print((_curAddress - _FirstAddress) / _DataSize);
  Serial.print(F(" "));
  Serial.print(_curAddress);
  Serial.print(F(" "));
  Serial.print(p_value);
  Serial.println(F(" not changed : prevValue == p_value"));
  }
 }
else if ( prevValue < p_value )
 { 
 _curAddress += _DataSize;
 if ( _curAddress > _LastAddress ) { _curAddress = _FirstAddress; }
 writeValue(_curAddress, p_value);
 if ( _Debug )
  {
  Serial.print((_curAddress - _FirstAddress) / _DataSize);
  Serial.print(F(" "));
  Serial.print(_curAddress);
  Serial.print(F(" "));
  Serial.print(p_value);
  Serial.println();
  }
 }
return ret;
}

void EEPROMRingCounter::clear()
{
for ( unsigned int addr = _FirstAddress; addr <= _LastAddress + _DataSize; addr++ ) 
 { EEPROM.write(addr, _MaxDataValue); }
EEPROM.commit();
if ( _Debug ) { Serial.println(F("EEPROM area filled with FF")); }
}
