#ifndef _PSGROOVE_H_
#define _PSGROOVE_H_
#include "usbdrv.h"
//Hub port statuses
#define PORT_EMPTY 0x0100   /* powered only */
#define PORT_FULL 0x0303    /* connected, enabled, powered, low-speed */
#define C_PORT_CONN  0x0001 /* connection */
#define C_PORT_RESET 0x0010 /* reset */
#define C_PORT_NONE  0x0000 /* no change */
//LEDs
#define RED   1
#define GREEN 2
#define NONE  0
#define BOTH  (RED | GREEN)
#define GREEN_PORT USB_OUTPORT(GREEN_LED_PORT)
#define RED_PORT   USB_OUTPORT(RED_LED_PORT)
#define GREEN_DDR  USB_DDRPORT(GREEN_LED_PORT)
#define RED_DDR    USB_DDRPORT(RED_LED_PORT)
//Stage2 support.
#define STAGE2
//DIP switch to GND on PB2-PB5.
#define DIPSWITCH
//Service mode JIG emulation
#define JIG
#endif //_PSGROOVE_H_
