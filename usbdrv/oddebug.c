/* Name: oddebug.c
 * Project: AVR library
 * Author: Christian Starkjohann
 * Creation Date: 2005-01-16
 * Tabsize: 4
 * Copyright: (c) 2005 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: GNU GPL v2 (see License.txt), GNU GPL v3 or proprietary (CommercialLicense.txt)
 * This Revision: $Id: oddebug.c 692 2008-11-07 15:07:40Z cs $
 */

#include "oddebug.h"
#include <string.h>

#if DEBUG_LEVEL > 0

static void uartPutc(char c)
{
    while(!(ODDBG_USR & (1 << ODDBG_UDRE)));    /* wait for data register empty */
    ODDBG_UDR = c;
}

static void uartPuts(char *msg)
{
   uchar len = strlen(msg);
   while(len--) 
      uartPutc(*msg++);
}

static uchar    hexAscii(uchar h)
{
    h &= 0xf;
    if(h >= 10)
        h += 'a' - (uchar)10 - '0';
    h += '0';
    return h;
}

static void printHex(uchar c)
{
    uartPutc(hexAscii(c >> 4));
    uartPutc(hexAscii(c));
}

void odDebug(uchar prefix, uchar *data, uchar len)
{
    printHex(prefix);
    uartPutc(':');
    while(len--){
        uartPutc(' ');
        printHex(*data++);
    }
    uartPutc('\r');
    uartPutc('\n');
}

void odDebugByte(uchar prefix, uchar data)
{
    printHex(prefix);
    uartPutc(':');
    printHex(data);
    uartPutc('\r');
    uartPutc('\n');
}

void odDebugPrefixedHex(char *prefix, uchar *data, uchar len)
{
   uartPuts(prefix);
   while(len--) {
      printHex(*data++);
      uartPutc(' ');
   }
   uartPutc('\r');
   uartPutc('\n');
}

void odDebugString(char *msg)
{
   uartPuts(msg);
   uartPutc('\r');
   uartPutc('\n');
}

#endif
