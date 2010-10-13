#include <Wire.h>
#define EEPROM_ADDR 0x50 // I2C Bus address of 24LC512 64K EEPROM
void setup(){
  Wire.begin(); // join I2C bus (address optional for master)
  Serial.begin(57600);
}

void loop(){
  char c;
  if(Serial.available()>0){
    digitalWrite(13, HIGH);   // set the LED on
    c=Serial.read();
    if(c=='r'){
      eeread();
    }else if(c=='w'){
      eewrite();
    }else if(c=='s'){
      eegetsize();
    }else if(c=='t'){
      eesetsize();
    }
    digitalWrite(13, LOW);    // set the LED off
  }
}

void eeread(void){
  unsigned int size;
  i2c_eeprom_read_buffer(EEPROM_ADDR, 0, (byte *)&size, 2);

  for (int i=0; i<size; i++){
    Serial.write(i2c_eeprom_read_byte(EEPROM_ADDR, i+2));
  }
}

void eewrite(void){
  unsigned int size,i;
  byte c[28],j,k,v;
  i2c_eeprom_read_buffer(EEPROM_ADDR, 0, (byte *)&size, 2);
  for(i=0;i<size;i+=28){
    k=size>=i+28?28:size-i;
    for(j=0;j<k;j++){
      while(Serial.available()==0);
      c[j]=Serial.read();
      i2c_eeprom_write_byte(EEPROM_ADDR, i+j+2,c[j]);
    }
    //FIXME: This corrupts, so byte-by-byte until fixed.
    //i2c_eeprom_write_page(EEPROM_ADDR, i+2, c, j);
    //delay(500);
    Serial.write(".");
  }
}

void eegetsize(void){
  unsigned int size;
  i2c_eeprom_read_buffer(EEPROM_ADDR, 0, (byte *)&size, 2);
  Serial.write((uint8_t *)&size,2);
}

void eesetsize(void){
  byte size[2];
  while(Serial.available()!=2);
  size[0]=Serial.read();
  size[1]=Serial.read(); 
  i2c_eeprom_write_page(EEPROM_ADDR, 0, size, 2);
}
