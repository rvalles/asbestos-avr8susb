#include "usb_utils.h"

uint8_t interruptBufferIdx = 0;
uchar interruptBuffer[8];

void interruptWrite_Byte(uchar byte)
{
   if (interruptBufferIdx < 8) {
      interruptBuffer[interruptBufferIdx++] = byte;
   }
}

void interruptWrite_Word(uint16_t word)
{
   if (interruptBufferIdx < 7) {
      interruptBuffer[interruptBufferIdx++] = word & 0xff;
      interruptBuffer[interruptBufferIdx++] = word >> 8;
   }
}

void sendInterruptBuffer()
{
   usbSetInterrupt(interruptBuffer, interruptBufferIdx);
   interruptBufferIdx = 0;
}

void resetDataToggle()
{
   usbTxStatus1.buffer[0] = USB_INITIAL_DATATOKEN;
}

void pUsbSetInterrupt(const uchar *pData, uchar len)
{
   memcpy_P(interruptBuffer, pData, len);
   interruptBufferIdx = len;
   sendInterruptBuffer();
}


uint8_t outBufferLen = 0;
uint8_t outBuffer[8];

void outBuffer_Write_Byte(uchar byte)
{
   if (outBufferLen < 8) {
      outBuffer[outBufferLen++] = byte;
   }
}

void outBuffer_Write_Word(uint16_t word) 
{
   if (outBufferLen < 7) {
      outBuffer[outBufferLen++] = word & 0xff;
      outBuffer[outBufferLen++] = word >> 8;
   }
}

usbMsgLen_t sendOutBuffer()
{
   usbMsgLen_t len = outBufferLen;
   outBufferLen = 0;
   usbMsgPtr = outBuffer;
   return len;
}
   

