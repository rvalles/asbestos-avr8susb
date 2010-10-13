#include <Wire.h>

void i2c_eeprom_write_byte(int deviceaddress, unsigned int eeaddress, byte data)
{
  int rdata = data;
  Wire.beginTransmission(deviceaddress);
  Wire.send((int)(eeaddress >> 8));    // Address High Byte
  Wire.send((int)(eeaddress & 0xFF));  // Address Low Byte
  Wire.send(rdata);
  Wire.endTransmission();
  delay(2); // needs some delay
}

// Address is a page address, 6-bit (63). More and end will wrap around
// But data can be maximum of 28 bytes, because the Wire library has a buffer of 32 bytes
void i2c_eeprom_write_page(int deviceaddress, unsigned int eeaddresspage, byte* data, byte length)
{
  Wire.beginTransmission(deviceaddress);
  Wire.send((int)(eeaddresspage >> 8)); // Address High Byte
  Wire.send((int)(eeaddresspage & 0xFF)); // Address Low Byte
  byte c;
  for ( c = 0; c < length; c++)
    Wire.send(data[c]);
  Wire.endTransmission();
  delay(10);                           // need some delay
}

byte i2c_eeprom_read_byte(int deviceaddress, unsigned int eeaddress)
{
  byte rdata = 0xFF;
  Wire.beginTransmission(deviceaddress);
  Wire.send((int)(eeaddress >> 8));    // Address High Byte
  Wire.send((int)(eeaddress & 0xFF));  // Address Low Byte
  Wire.endTransmission();
  Wire.requestFrom(deviceaddress,1);
  if (Wire.available()) rdata = Wire.receive();
  return rdata;
}

// should not read more than 28 bytes at a time!
void i2c_eeprom_read_buffer(int deviceaddress, unsigned int eeaddress, byte *buffer, int length)
{
  Wire.beginTransmission(deviceaddress);
  Wire.send((int)(eeaddress >> 8));    // Address High Byte
  Wire.send((int)(eeaddress & 0xFF));  // Address Low Byte
  Wire.endTransmission();
  Wire.requestFrom(deviceaddress,length);
  //int c = 0;
  for ( int c = 0; c < length; c++ )
    if (Wire.available()) buffer[c] = Wire.receive();
}
