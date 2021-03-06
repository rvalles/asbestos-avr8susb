/* asbestos-avr8susb
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <string.h>
#include <stdio.h>
#include "config.h"
#include "asbestos-avr8susb.h"
#include "descriptor.h"
#include "usb_utils.h"
#include "oddebug.h"
#include "cfg1_payload1.h"
#ifdef DIPSWITCH
#include "cfg1_payload2.h"
#include "cfg1_payload3.h"
#endif
#ifdef JIG
#define JIG_SETTING 13
#define FINALDEV_SETTING 12
#define CHALLENGE_INDEX 7
#include "key.h"
#include "hmac.h"
#endif
//STAGE2 support.
#ifdef STAGE2
#ifndef EEPROM
#include "stage2a.h"
#include "stage2b.h"
#endif
#endif
#ifdef EEPROM
#include <eeprom.h>
#endif
void setLed(int color) {
	RED_PORT &= ~(1 << RED_BIT);
	GREEN_PORT &= ~(1 << GREEN_BIT);
	if (color & RED) {
		RED_PORT |= 1 << RED_BIT;
	}
	if (color & GREEN) {
		GREEN_PORT |= 1 << GREEN_BIT;
	}
}
uint16_t port_status[6] = { PORT_EMPTY, PORT_EMPTY, PORT_EMPTY, PORT_EMPTY, PORT_EMPTY, PORT_EMPTY };
uint16_t port_change[6] = { C_PORT_NONE, C_PORT_NONE, C_PORT_NONE, C_PORT_NONE, C_PORT_NONE, C_PORT_NONE };
enum { 
	init,
	wait_hub_ready,
	hub_ready,
	p1_wait_reset,
	p1_wait_enumerate,
	p1_ready,
	p2_wait_reset,
	p2_wait_enumerate,
	p2_ready,
	p3_wait_reset,
	p3_wait_enumerate,
	p3_ready,
	p2_wait_disconnect,
	p4_wait_connect,
	p4_wait_reset,
	p4_wait_enumerate,
	p4_ready,
	p5_wait_reset,
	p5_wait_enumerate,
	p5_challenged,
	p5_responded,
	p3_wait_disconnect,
	p3_disconnected,
	p5_wait_disconnect,
	p5_disconnected,
	p4_wait_disconnect,
	p4_disconnected,
	p1_wait_disconnect,
	p1_disconnected,
	p6_wait_reset,
	done
} state = init;
uint8_t hub_int_response = 0x00;
uint8_t hub_int_force_data0 = 0;
int last_port_conn_clear = 0;
int last_port_reset_clear = 0;
//Need a copy in ram to work with usbFunctionDescriptor().
uchar HUB_Hub_Descriptor_ram[sizeof(HUB_Hub_Descriptor)];
uchar port_addr[7] = { 0, 0, 0, 0, 0, 0, 0 };
uchar connected_ports[7] = { 1, 0, 0, 0, 0, 0, 0 };
int8_t port_cur = -1;
extern uchar usbDeviceAddr;
extern uchar usbNewDeviceAddr;
extern uchar usbRxLen;
extern uchar usbTxLen;
//avr8 UART helpers from v-usb.
static void uartPutc(char c) {
	while(!(ODDBG_USR & (1 << ODDBG_UDRE))); //wait for data register empty.
	ODDBG_UDR = c;
}
static void uartPuts(char *msg) {
	uchar len = strlen(msg);
	while(len--)
		uartPutc(*msg++);
}
void usbSetAddr(uchar addr) {
#if DEBUG_LEVEL > 1
	uchar msg[] = {port_cur, addr};
	DBG2(0x03, msg, 2);
#endif
	if(port_addr[port_cur] == 0)
		port_addr[port_cur] = addr;
}
void switch_port(uchar port) {
	if (port_cur == port)
		return;
	port_cur = port;
	usbRxLen = 0;
	usbTxLen = 0;
	usbTxLen1 = USBPID_NAK;
	usbNewDeviceAddr = port_addr[port];
	usbDeviceAddr = port_addr[port] << 1;
}
volatile uint8_t expire = 0;
//counts down every 10 milliseconds.
ISR(TIMER1_OVF_vect) { 
	uint16_t rate = (uint16_t) -(F_CPU / 64 / 100);
	TCNT1H = rate >> 8;
	TCNT1L = rate & 0xff;
	if (expire > 0)
		expire--;
}
void SetupLEDs(void) {
	GREEN_DDR |= (1 << GREEN_BIT);
	RED_DDR |= (1 << RED_BIT);
	setLed(NONE);
}
void SetupHardware(void) {
	usbDeviceConnect();
	odDebugInit();
	//Disable watchdog if enabled by bootloader/fuses
	MCUSR &= ~(1 << WDRF);
	wdt_disable();
	//Disable clock division
	clock_prescale_set(clock_div_1);
	//Setup timer 
	TCCR1B = 0x03; //timer rate clk/64
	TIMSK1 = 0x01;
	//Hardware Initialization
	SetupLEDs();
	usbInit();
	sei(); 
}
void panic(int led1, int led2) {
	DBGMSG1("Panic!");
	for(;;) {
		_delay_ms(1000);
		setLed(led1);
		_delay_ms(1000);
		setLed(led2);
	}		
}
void HUB_Task(void) {
	if (usbInterruptIsReady()){
		if (hub_int_response) {
			if (hub_int_force_data0) {
				resetDataToggle();
				hub_int_force_data0 = 0;
			}
			interruptWrite_Byte(hub_int_response);
			sendInterruptBuffer();
			DBGX2("Hub resp: ", &hub_int_response, 1);
			hub_int_response = 0x00;
		}
	}
}
const uint8_t PROGMEM jig_response[64] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x3d, 0xee, 0x78, 0x80, 0x00, 0x00, 0x00, 0x00, 0x3d, 0xee, 0x88,
	0x80, 0x00, 0x00, 0x00, 0x00, 0x33, 0xe7, 0x20, 0xe8, 0x83, 0xff, 0xf0, 0xe8, 0x63, 0xff, 0xf8,
	0xe8, 0xa3, 0x00, 0x18, 0x38, 0x63, 0x10, 0x00, 0x7c, 0x04, 0x28, 0x00, 0x40, 0x82, 0xff, 0xf4,
	0x38, 0xc3, 0xf0, 0x20, 0x7c, 0xc9, 0x03, 0xa6, 0x4e, 0x80, 0x04, 0x20, 0x04, 0x00, 0x00, 0x00,
};
uchar setting;
void JIG_Task(void) {
	static uchar bytes_in = 0;
	if (usbInterruptIsReady() && state == p5_challenged && expire == 0) {
		if (bytes_in < 64) {
#ifdef JIG
			if(setting==JIG_SETTING){
				usbSetInterrupt(&jig_challenge_res[bytes_in], 8);
				//uartPutc('.');
			} else
#endif
			{
				pUsbSetInterrupt(&jig_response[bytes_in], 8);
			}
			bytes_in += 8;
			if (bytes_in >= 64) {
				state = p5_responded;
				expire = 15;
			}
		}
	}
}
void connect_port(int port) {
	connected_ports[port] = 1;
	last_port_reset_clear = 0;
	hub_int_response |= (1 << port);
	port_status[port - 1] = PORT_FULL;
	port_change[port - 1] = C_PORT_CONN;
}
void disconnect_port(int port) {
	connected_ports[port] = 0;
	last_port_conn_clear = 0;
	hub_int_response |= (1 << port);
	port_status[port - 1] = PORT_EMPTY;
	port_change[port - 1] = C_PORT_CONN;
}
unsigned int stage2_size;
uchar payload_id;
#ifdef EEPROM
uchar eeprom_id;
#endif
int main(void) {
	SetupHardware();
	uartPuts("Welcome to asbestos-avr8susb.\n");
	setLed(RED);
#ifdef DIPSWITCH
#ifdef ARDUINOMEGA
	DDRB&=0xf0; //0xf0 becomes 0 aka read
	PORTB|=0x0f; //Enabling internal pullup.
#else
	DDRB&=0xc3; //0x3c becomes 0 aka read
	PORTB|=0x3c; //Enabling internal pullup.
#endif
	MCUCR&=0xff^(1<<PUD); //Make sure pullup disable is NOT enabled.
	expire=2;
	while(expire); //Wait a little before reading PINB.
#ifndef ARDUINOMEGA
	setting=PINB>>2; //We don't want PB0,1, so we shift past that.
#endif
	setting^=0xff; //We're using pullups so to make connected to GND = 1, we need to flip our values.
	setting&=0x0f; //We only need to read 4 bits so we discard the higher nibble.
#else
#ifdef JIG
	setting=JIG_SETTING;
#else
	setting=1;
#endif
#endif
	DBGX1("Setting: ",&setting,sizeof(setting));
#ifdef JIG
	if(setting==JIG_SETTING){
		uartPuts("Entering JIG emulation mode.\n");
	}else if(setting==FINALDEV_SETTING){
		uartPuts("Entering final device mode directly.\n");
	}else
#endif
	{
		switch(setting&0xC){
			case 0xC:
				panic(RED, GREEN);
				break;
			case 0x8:
				panic(RED, GREEN);
				break;
			case 0x4:
#ifdef EEPROM
				eeprom_id=0xa2;
#else
				panic(RED, GREEN);
#endif
				break;
			case 0x0:
#ifdef EEPROM
				eeprom_id=0xa0;
#endif
				break;
		}
		payload_id=(setting&0x03); //last 2 bits of setting = payload selection. 00 is reserved, so first payload is 01.
		DBGX1("Payload: ",&payload_id,sizeof(payload_id));
		payload_id--; //00 is reserved, so first payload is 01.
	}
	state = init;
	switch_port(0);
	//uartPuts("Waiting for USB to be up.\n");
#ifdef STAGE2
#ifdef EEPROM
	twiinit();
	ee24xx_read_bytes(eeprom_id, 0, 2, (uint8_t *)&stage2_size);
	uartPuts("i2c eeprom ready.\n");
#else
	stage2_size = sizeof(stage2a)+sizeof(stage2b);
#endif
#endif
#ifdef JIG
	if(setting==JIG_SETTING){
		HMACInit(jig_key,20);
		uartPuts("jig_key ready.\n");
	}
#endif
	//Copy the hub descriptor into ram, vusb's usbFunctionSetup() callback can't handle stuff from FLASH.
	memcpy_P(HUB_Hub_Descriptor_ram, HUB_Hub_Descriptor, sizeof(HUB_Hub_Descriptor));
	expire=1;
#ifdef JIG
	if(setting==JIG_SETTING){
		for(;;){
			if (port_cur == 0)
				HUB_Task();
			if (port_cur == 5)
				JIG_Task();
			usbPoll();
			if (state == hub_ready && expire == 0) {
				DBGBB1(0x00, 0xf1);
				setLed(NONE);
				connect_port(5);
				state = p5_wait_reset;
			}
			if (state == p5_wait_reset && last_port_reset_clear == 5) {
				DBGBB1(0x00, 0xf2);
				uartPuts("Hub.\n");
				setLed(RED);
				switch_port(5);
				state = p5_wait_enumerate;
			}
			if (state == p5_responded && expire == 0) {
				DBGBB1(0x00, 0xf3);
				setLed(NONE);
				switch_port(0);
				/* Need wrong data toggle again */
				hub_int_force_data0 = 1;
				disconnect_port(5);
				state = p5_wait_disconnect;
				//state = done;
			}
			if (state == p5_wait_disconnect && last_port_conn_clear == 5) {
				DBGBB1(0x00, 0xf4);
				setLed(RED);
				state = p5_disconnected;
				expire = 20;
			}
			if (state == p5_disconnected && expire == 0){
				setLed(GREEN);
				uartPuts("Done.\n");
				state = done;
			}
		}
	}
#endif
#ifdef JIG
	if(setting==FINALDEV_SETTING){
		for(;;){
			if (port_cur == 0)
				HUB_Task();
			usbPoll();
			if (state == hub_ready && expire == 0) {
				DBGBB1(0x00, 0xa1);
				setLed(NONE);
				connect_port(6);
				state = p6_wait_reset;
			}
			if (state == p6_wait_reset && last_port_reset_clear == 6) {
				DBGBB1(0x00, 0xa2);
				setLed(RED);
				switch_port(6);
				uartPuts("Final device ready.\n");
				state = done;
			}
			if (state == done) {
				setLed(GREEN);
			}
		}
	}
#endif
	for(;;){
		if (port_cur == 0)
			HUB_Task();
		if (port_cur == 5)
			JIG_Task();
		usbPoll();
		//connect 1
		if (state == hub_ready && expire == 0) {
			DBGBB1(0x00, 0x01);
			setLed(NONE);
			connect_port(1);
			state = p1_wait_reset;
		}
		if (state == p1_wait_reset && last_port_reset_clear == 1) {
			DBGBB1(0x00, 0x02);
			uartPuts("Hub.\n");
			setLed(RED);
			switch_port(1);
			state = p1_wait_enumerate;
		}
		//connect 2
		if (state == p1_ready && expire == 0) {
			DBGBB1(0x00, 0x03);
			setLed(NONE);
			switch_port(0);
			connect_port(2);
			state = p2_wait_reset;
		}
		if (state == p2_wait_reset && last_port_reset_clear == 2) {
			DBGBB1(0x00, 0x04);
			setLed(RED);
			switch_port(2);
			state = p2_wait_enumerate;
		}
		//connect 3
		if (state == p2_ready && expire == 0) {
			DBGBB1(0x00, 0x05);
			setLed(NONE);
			switch_port(0);
			connect_port(3);
			state = p3_wait_reset;
		}
		if (state == p3_wait_reset && last_port_reset_clear == 3) {
			DBGBB1(0x00, 0x06);
			setLed(RED);
			switch_port(3);
			state = p3_wait_enumerate;
		}
		//disconnect 2
		if (state == p3_ready && expire == 0) {
			DBGBB1(0x00, 0x07);
			setLed(NONE);
			switch_port(0);
			disconnect_port(2);
			state = p2_wait_disconnect;
		}
		if (state == p2_wait_disconnect && last_port_conn_clear == 2) {
			DBGBB1(0x00, 0x08);
			setLed(RED);
			state = p4_wait_connect;
			expire = 15;
		}
		//connect 4
		if (state == p4_wait_connect && expire == 0) 
		{
			DBGBB1(0x00, 0x09);
			setLed(NONE);
			connect_port(4);
			state = p4_wait_reset;
		}
		if (state == p4_wait_reset && last_port_reset_clear == 4) {
			DBGBB1(0x00, 0x10);
			setLed(RED);
			switch_port(4);
			state = p4_wait_enumerate;
		}
		//connect 5
		if (state == p4_ready && expire == 0) {
			DBGBB1(0x00, 0x11);
			setLed(NONE);
			switch_port(0);
			//When first connecting port 5, we need to have the wrong data toggle for the PS3 to respond.
			hub_int_force_data0 = 1;
			connect_port(5);
			state = p5_wait_reset;
		}
		if (state == p5_wait_reset && last_port_reset_clear == 5) {
			DBGBB1(0x00, 0x12);
			uartPuts("xpl.\n");
			setLed(RED);
			switch_port(5);
			state = p5_wait_enumerate;
		}
		//disconnect 3
		if (state == p5_responded && expire == 0) {
			DBGBB1(0x00, 0x13);
			setLed(NONE);
			switch_port(0);
			/* Need wrong data toggle again */
			hub_int_force_data0 = 1;
			disconnect_port(3);
			state = p3_wait_disconnect;
			//state = done;
		}
		if (state == p3_wait_disconnect && last_port_conn_clear == 3) {
			DBGBB1(0x00, 0x14);
			setLed(RED);
			state = p3_disconnected;
			expire = 45;
		}
		//disconnect 5
		if (state == p3_disconnected && expire == 0) {
			DBGBB1(0x00, 0x15);
			setLed(NONE);
			switch_port(0);
			disconnect_port(5);
			state = p5_wait_disconnect;
		}
		if (state == p5_wait_disconnect && last_port_conn_clear == 5) {
			DBGBB1(0x00, 0x16);
			setLed(RED);
			state = p5_disconnected;
			expire = 20;
		}
		//disconnect 4
		if (state == p5_disconnected && expire == 0) {
			DBGBB1(0x00, 0x17);
			setLed(NONE);
			switch_port(0);
			disconnect_port(4);
			state = p4_wait_disconnect;
		}
		if (state == p4_wait_disconnect && last_port_conn_clear == 4) {
			DBGBB1(0x00, 0x18);
			setLed(RED);
			state = p4_disconnected;
			expire = 20;
		}
		//disconnect 1
		if (state == p4_disconnected && expire == 0) {
			DBGBB1(0x00, 0x19);
			setLed(NONE);
			switch_port(0);
			disconnect_port(1);
			state = p1_wait_disconnect;
		}
		if (state == p1_wait_disconnect && last_port_conn_clear == 1) {
			DBGBB1(0x00, 0x20);
			setLed(RED);
			state = p1_disconnected;
			expire = 20;
		}
		//connect 6
		if (state == p1_disconnected && expire == 0) {
			DBGBB1(0x00, 0x21);
			setLed(NONE);
			switch_port(0);
			connect_port(6);
			state = p6_wait_reset;
		}
		if (state == p6_wait_reset && last_port_reset_clear == 6) {
			DBGBB1(0x00, 0x22);
			setLed(RED);
			switch_port(6);
			uartPuts("Final device ready.\n");
			state = done;
		}
		//done
		if (state == done) {
			setLed(GREEN);
		}
	}
}
static int currentPosition, bytesRemaining;
usbMsgLen_t usbFunctionDescriptor(struct usbRequest *rq) {
	const uint8_t  DescriptorType   = rq->wValue.bytes[1];
	const uint8_t  DescriptorNumber = rq->wValue.bytes[0];
	const uint16_t  wLength = rq->wLength.word;
	usbMsgLen_t Size = 0;
	switch (DescriptorType) {
		case USBDESCR_DEVICE:
			switch (port_cur) {
				case 0:
					usbMsgPtr = (void *) HUB_Device_Descriptor;
					Size = sizeof(HUB_Device_Descriptor);
					break;
				case 1:
					usbMsgPtr = (void *) port1_device_descriptor;
					Size = sizeof(port1_device_descriptor);
					break;
				case 2:
					usbMsgPtr = (void *) port2_device_descriptor;
					Size = sizeof(port2_device_descriptor);
					break;
				case 3:
					usbMsgPtr = (void *) port3_device_descriptor;
					Size = sizeof(port3_device_descriptor);
					break;
				case 4:
					usbMsgPtr = (void *) port4_device_descriptor;
					Size = sizeof(port4_device_descriptor);
					break;
				case 5:
					usbMsgPtr = (void *) port5_device_descriptor;
					Size = sizeof(port5_device_descriptor);
					break;
				case 6:
					//DBGMSG1("Requested port6_device_descriptor");
					usbMsgPtr = (void *) port6_device_descriptor;
					Size = sizeof(port6_device_descriptor);
					break;
			}
			break;
		case USBDESCR_CONFIG: 
			switch (port_cur) {
				case 0:
					usbMsgPtr = (void *) HUB_Config_Descriptor;
					Size = sizeof(HUB_Config_Descriptor);
					break;
				case 1:
					// 4 configurations are the same.
					// For the initial 8-byte request, we give a different
					// length response than in the full request.
					if (DescriptorNumber < 4) {
						if (wLength == 8) {
							usbMsgPtr = (void *) port1_short_config_descriptor;
							Size = sizeof(port1_short_config_descriptor);
							//DBGMSG1("!");
						} else {
#ifdef DIPSWITCH
							switch (payload_id){
								case 0:
									usbMsgPtr = (void *) port1_config_descriptor_payload1;
									Size = sizeof(port1_config_descriptor_payload1);
									break;
								case 1:
									usbMsgPtr = (void *) port1_config_descriptor_payload2;
									Size = sizeof(port1_config_descriptor_payload2);
									break;
								case 2:
									usbMsgPtr = (void *) port1_config_descriptor_payload3;
									Size = sizeof(port1_config_descriptor_payload3);
									break;
							}
#else
							usbMsgPtr = (void *) port1_config_descriptor_payload1;
							Size = sizeof(port1_config_descriptor_payload1);
							//no idea why the following isn't working, stashing for now
							//bytesRemaining=sizeof(port1_config_descriptor);
							//currentPosition=0;
							//return USB_NO_MSG;
#endif
						}
						if (DescriptorNumber == 3 && wLength > 8) {
							state = p1_ready;
							expire = 40;
						}
					}
					break;
				case 2:
					// only 1 config
					usbMsgPtr = (void *) port2_config_descriptor;
					Size = sizeof(port2_config_descriptor);
					state = p2_ready;
					expire = 15;
					break;
				case 3:
					// 2 configurations are the same
					usbMsgPtr = (void *) port3_config_descriptor;
					Size = sizeof(port3_config_descriptor);
					if (DescriptorNumber == 1 && wLength > 8) {
						state = p3_ready;
						expire = 20;
					}
					break;
				case 4:
					// 3 configurations
					if (DescriptorNumber == 0) {
						usbMsgPtr = (void *) port4_config_descriptor_1;
						Size = sizeof(port4_config_descriptor_1);
					} else if (DescriptorNumber == 1) {
						if (wLength == 8) {
							usbMsgPtr = (void *) port4_short_config_descriptor_2;
							Size = sizeof(port4_short_config_descriptor_2);
						} else {
							usbMsgPtr = (void *) port4_config_descriptor_2;
							Size = sizeof(port4_config_descriptor_2);
						}
					} else if (DescriptorNumber == 2) {
						usbMsgPtr = (void *) port4_config_descriptor_3;
						Size    = sizeof(port4_config_descriptor_3);
						if (wLength > 8) {
							state = p4_ready;
							expire = 20;  // longer seems to help this one?
						}
					}
					break;
				case 5:
					// 1 config
					usbMsgPtr = (void *) port5_config_descriptor;
					Size = sizeof(port5_config_descriptor);
					break;
				case 6:
					// 1 config
					//DBGMSG1("Requested port6_config_descriptor");
					usbMsgPtr = (void *) port6_config_descriptor;
					Size = sizeof(port6_config_descriptor);
					break;
			}
			break;
	}
	DBGMSG2("getDescriptorType and number");
	DBG2(0x01, &DescriptorType, 1);
	DBG2(0x01, &DescriptorNumber, 1);
	return Size;
}
enum {
	WRITE_PRINT=0,
	WRITE_EEPROM
};
uchar writeop;
uchar usbFunctionWrite(uchar *data, uchar len) {
	uchar i;
	uchar tdata[9];
	if(state==p5_wait_enumerate) {
		static int bytes_out = 0;
#ifdef JIG
		if(setting==JIG_SETTING){
			for(i=0;i<len;i++) {
				jig_challenge_res[bytes_out+i]=*(data+i);
			}
		}
#endif
		bytes_out += len;
		if (bytes_out >= 64) {
#ifdef JIG
			if(setting==JIG_SETTING){
				DBGX1("Request : ",jig_challenge_res,64);
				//prepare the response
				jig_challenge_res[1]--;
				jig_challenge_res[3]++;
				jig_challenge_res[6]++;
				HMACBlock(&jig_challenge_res[CHALLENGE_INDEX],20);
				//usbPoll(); //just in case
				HMACDone();
				jig_challenge_res[7] = jig_id[0];
				jig_challenge_res[8] = jig_id[1];
				for(i=0;i<20;i++)
					jig_challenge_res[9+i] = hmacdigest[i];
				for(i=29;i<64;i++)
					jig_challenge_res[i] = 0xEC;
				DBGX1("Response: ",jig_challenge_res,64);
			}
#endif
			expire = 50;
			state = p5_challenged;
			return 0;
		} else {
			return 0xff; //STALL/Error.
		}
	}
	if(state==done) {
		switch(writeop){
			case WRITE_PRINT:
				for(i=0;i<len;i++) {
					tdata[i]=*(data+i);
				}
				tdata[i]=0;
				uartPuts((char*)tdata);
				return 0;
				break;
#ifdef EEPROM
			case WRITE_EEPROM:
				ee24xx_write_bytes(eeprom_id, 2+currentPosition, len, (uint8_t *)data);
				return 0;
				break;
#endif
		}
	}
	return 0xff; //there is no logic to handle when the state isn't p5 or done.
}
uchar usbFunctionRead(uchar *data, uchar len) {
	if (state==p1_wait_enumerate){
		//uartPutc('.');
		if(!bytesRemaining){
			uartPutc('F');
			return 0;
		}
		memcpy_P(data,port1_config_descriptor_payload1+currentPosition,len);
		bytesRemaining -= len;
		currentPosition += len;
		return len;
	}
	if (state==done){
#ifdef STAGE2
		setLed(BOTH);
		uartPutc('.');
		if(len > bytesRemaining)                // len is max chunk size
			len = bytesRemaining;               // send an incomplete chunk
#ifdef EEPROM
		ee24xx_read_bytes(eeprom_id, 2+currentPosition, len, data);
#else
		if(currentPosition<32767) {
			if(currentPosition+len>32767)
				len=32767-currentPosition;
			memcpy_P(data,stage2a+currentPosition,len);
		} else {
			memcpy_P(data,stage2b+currentPosition,len);
		}
#endif
		bytesRemaining -= len;
		currentPosition += len;
		setLed(GREEN);
		return len;                             // return real chunk size
#endif
	}
	panic(RED, GREEN);
	return 0;
}
#define TYPE_HOST2DEV 0x40
#define TYPE_DEV2HOST 0xc0
enum { 
        REQ_PRINT = 1,
        REQ_GET_STAGE2_SIZE,
        REQ_READ_STAGE2_BLOCK,
	REQ_GET_EEPROM_SIZE,
	REQ_SET_EEPROM_SIZE,
	REQ_READ_EEPROM_BYTE,
	REQ_WRITE_EEPROM_BYTE,
	REQ_READ_EEPROM_EIGHT
};
usbMsgLen_t usbFunctionSetup(uchar data[8]) {
	usbRequest_t *rq = (usbRequest_t *) data;
	// uchar msg[] = {usbDeviceAddr, usbNewDeviceAddr};
#ifdef STAGE2
	//asbestos REQ_PRINT
	if (port_cur == 6 && rq->bmRequestType == TYPE_HOST2DEV && rq->bRequest == REQ_PRINT) {
		DBGMSG2("debug print requested");
		writeop=WRITE_PRINT;
		return 0;
	}
	//asbestos REQ_GET_STAGE2_SIZE
	if (port_cur == 6 && rq->bmRequestType == TYPE_DEV2HOST && rq->bRequest == REQ_GET_STAGE2_SIZE) {
		unsigned int size;
		DBGMSG2("stage2 size requested.");
		DBGX1("size:", (uchar*)&stage2_size, sizeof(stage2_size));
		outBuffer_Write_Word(0x0000);
		size = (stage2_size>>8)|(stage2_size<<8);
		outBuffer_Write_Word(size);
		return sendOutBuffer();
	}
	//asbestos REQ_READ_STAGE2_BLOCK
	if (port_cur == 6 && rq->bmRequestType == TYPE_DEV2HOST && rq->bRequest == REQ_READ_STAGE2_BLOCK) {
		DBGMSG2("stage2 block requested.");
		bytesRemaining = rq->wLength.word;
		currentPosition = rq->wIndex.word<<12;
		//DBGX1("ofst:",&currentPosition,2);
		//DBGX1("lgth:",&bytesRemaining,2);
		if(bytesRemaining > stage2_size - currentPosition)
		{
			uartPuts("FAIL: Request goes beyond stage2.");
			return 0;
		}
		return USB_NO_MSG;
	}
#endif
#ifdef EEPROM
	//eeprom programming REQ_GET_EEPROM_SIZE
	if (port_cur == 6 && rq->bmRequestType == TYPE_DEV2HOST && rq->bRequest == REQ_GET_EEPROM_SIZE) {
		unsigned int size;
		unsigned char eeid = rq->wValue.word;
		DBGMSG2("eeprom size requested.");
		DBGX2("eeid:", (uchar*)&eeid, sizeof(eeid));
		ee24xx_read_bytes(eeid, 0, 2, (uint8_t *)&size);
		DBGX2("size:", (uchar*)&size, sizeof(size));
		outBuffer_Write_Word(size);
		return sendOutBuffer();
	}
	//eeprom programming REQ_SET_EEPROM_SIZE
	if (port_cur == 6 && rq->bmRequestType == TYPE_HOST2DEV && rq->bRequest == REQ_SET_EEPROM_SIZE) {
		unsigned char eeid = rq->wValue.word;
		DBGMSG2("setting eeprom size requested.");
		DBGX2("eeid:", (uchar*)&eeid, sizeof(eeid));
		DBGX2("size:", (uchar*)&rq->wIndex.word, sizeof(rq->wIndex.word));
		ee24xx_write_bytes(eeid, 0, 2, (uint8_t *)&rq->wIndex.word);
		DBGBB1(0xFF,0x77);
		return 0;
	}
	//eeprom programming REQ_READ_EEPROM_BYTE
	if (port_cur == 6 && rq->bmRequestType == TYPE_DEV2HOST && rq->bRequest == REQ_READ_EEPROM_BYTE) {
		unsigned char eeid = rq->wValue.word;
		unsigned char byte;
		DBGMSG2("reading eeprom requested.");
		ee24xx_read_bytes(eeid, 2+rq->wIndex.word, 1, &byte);
		outBuffer_Write_Byte(byte);
		return sendOutBuffer();
	}
	//eeprom programming REQ_READ_EEPROM_EIGHT
	if (port_cur == 6 && rq->bmRequestType == TYPE_DEV2HOST && rq->bRequest == REQ_READ_EEPROM_EIGHT) {
		unsigned char eeid = rq->wValue.word;
		unsigned char eight[8];
		unsigned char i;
		DBGMSG2("reading eeprom requested.");
		ee24xx_read_bytes(eeid, 2+rq->wIndex.word, 8, (uint8_t *)&eight);
		for(i=0;i<8;i++){
			outBuffer_Write_Byte(eight[i]);
		}
		return sendOutBuffer();
	}
	//eeprom programming REQ_WRITE_EEPROM_BYTE
	if (port_cur == 6 && rq->bmRequestType == TYPE_HOST2DEV && rq->bRequest == REQ_WRITE_EEPROM_BYTE) {
		DBGMSG2("writing eeprom requested.");
		eeprom_id = rq->wValue.word;
		writeop=WRITE_EEPROM;
		currentPosition=rq->wIndex.word;
		DBGX2("eeid:", (uchar *)&eeprom_id, sizeof(eeprom_id));
		DBGX2("ofst:", (uchar *)&currentPosition, sizeof(currentPosition));
		return 0;
	}
#endif
	//done
	if (port_cur == 6 && rq->bRequest == 0xAA) {
		/* holy crap, it worked! */
		state = done;
		return 0;
	}
	//USBRQ_SET_INTERFACE
	if (port_cur == 5 && rq->bRequest == USBRQ_SET_INTERFACE) {
		/* can ignore this */
		return 0;
	}
	//HUB_Hub_Descriptor
	if (port_cur == 0 && rq->bmRequestType == 0xA0 && rq->bRequest == 0x06 && rq->wValue.bytes[1] == 0x29) {
		usbMsgPtr = (void *) HUB_Hub_Descriptor_ram;
		return sizeof(HUB_Hub_Descriptor_ram);
	}
	//GET HUB STATUS
	if (port_cur == 0 && rq->bmRequestType == 0xA0 && rq->bRequest == 0x00 && rq->wValue.word == 0x00 && rq->wIndex.word == 0x00 && rq->wLength.word == 0x04) {
		DBGMSG2("Get hub status.");
		outBuffer_Write_Word(0x0000);
		outBuffer_Write_Word(0x0000);
		return sendOutBuffer();
	}
	//GET PORT STATUS
	if (port_cur == 0 && rq->bmRequestType == 0xA3 && rq->bRequest == 0x00 && rq->wValue.word == 0x00 && rq->wLength.word == 0x04) {
		uint8_t p = rq->wIndex.word;
		if (p < 1 || p > 6)
			return 0;
		DBGX2("Get port status: ", &p, 1);
		outBuffer_Write_Word(port_status[p - 1]);
		outBuffer_Write_Word(port_change[p - 1]);
		return sendOutBuffer();
	}
	//SET_FEATURE
	if (port_cur == 0 && rq->bmRequestType == 0x23 && rq->bRequest == 0x03 && rq->wLength.word == 0x00) {
		uint8_t p = rq->wIndex.word;
		if (p < 1 || p > 6)
			return 0;
		switch(rq->wValue.word) {
			case 0x0008: //PORT_POWER
				DBGX2("Set port power: ", &p, 1);
				if (p == 6 && state == init) {
					//after the 6th port is powered, wait a bit and continue.
					state = hub_ready;
					expire = 15;
				}
			break;
			case 0x0004: //PORT_RESET
				DBGX2("Set port reset: ", &p, 1);
				hub_int_response |= (1 << p);
				port_change[p - 1] |= C_PORT_RESET;
				break;
		}
		return 0;
	}
	//CLEAR_FEATURE
	if (port_cur == 0 && rq->bmRequestType == 0x23 && rq->bRequest == 0x01 && rq->wLength.word == 0x00) {
		uint8_t p = rq->wIndex.word;
		if (p < 1 || p > 6)
			return 0;
		switch(rq->wValue.word) {
			case 0x0010: //C_PORT_CONNECTION
				DBGX2("Clear C_PORT_CONN: ", &p, 1);
				port_change[p - 1] &= ~C_PORT_CONN;
				last_port_conn_clear = p;
				break;
			case 0x0014: //C_PORT_RESET
				DBGX2("Clear C_PORT_RESET: ", &p, 1);
				port_change[p - 1] &= ~C_PORT_RESET;
				last_port_reset_clear = p;
				break;
		}
		return 0;
	}
	//panic(RED, GREEN);
	DBGX1("unk type:", &rq->bmRequestType, 1);
	DBGX1("request : ", &rq->bRequest, 1);
	return 0;
}
